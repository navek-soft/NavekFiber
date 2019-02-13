#include "threadpool.h"
#include <dom.h>
#include <utils/transform.h>
#include <unordered_set>
#include <coreexcept.h>
#include <csignal>
#include <trace.h>
using namespace Core;

struct CvtStringToInt {
	inline size_t operator ()(const std::string& from) const { return std::stoul(from); }
};

static inline void set_thread_name(const std::string&& name)
{
	pthread_setname_np(pthread_self(), name.c_str());
}

static inline void set_thread_affinity(size_t cpu_id) {
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(cpu_id, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

ThreadPool::ThreadPool(const string& InitialWorkers, const string& MaxWorkers, const string& CoreBinding, const string& ExcludeBinding)
	: PoolYet (true), PoolInitWorkers(std::stoul(InitialWorkers)),PoolMaxWorkers(std::stoul(MaxWorkers)), PoolIndexWorkers(0), PoolBusyWorkers(0)
{

	/* Initialize CPUs bindings  */
	{
		auto list_worker = utils::expand<size_t, CvtStringToInt>(CoreBinding);
		auto list_exclude = utils::expand<size_t, CvtStringToInt>(ExcludeBinding);

		size_t max_threads = std::thread::hardware_concurrency();

		if (list_worker.empty()) {
			for (; max_threads--;) {
				if (list_exclude.empty() || list_exclude.find(max_threads) == list_exclude.end()) {
					PoolWorkersBinding.emplace_back(max_threads);
				}
			}
		}
		else {
			for (auto&& co : list_worker) {
				if (list_exclude.empty() || list_exclude.find(co) == list_exclude.end()) {
					PoolWorkersBinding.emplace_back(co % max_threads);
				}
			}
		}
	}
	if (PoolWorkersBinding.empty() || !PoolInitWorkers || PoolMaxWorkers < PoolInitWorkers) {
		throw RuntimeException(__FILE__, __LINE__, "Invalid thread pool initialization. (Workers: %ld, Max: %ld, Bindigns: %s)", PoolInitWorkers, PoolMaxWorkers,CoreBinding.c_str());
	}

	Hire(PoolInitWorkers,sPersistent);
}

ThreadPool::~ThreadPool() {
	Join();
}

void ThreadPool::Join() {
	if (PoolYet) {
		{
			unique_lock<mutex> lock(PoolQueueSync);
			PoolYet = false;
		}
		PoolCondition.notify_all();
		for (auto& worker : PoolWorkers) {
			if (worker.second.joinable()) {
				worker.second.join();
			}
		}
	}
}

void ThreadPool::GetState(size_t& NumWorkers, size_t& NumAwaitingTasks, size_t& NumBusy) {
	{
		unique_lock<mutex> lock(PoolQueueSync);
		NumWorkers = PoolWorkers.size();
		NumAwaitingTasks = PoolTasks.size();
	}
	NumBusy = (long)PoolBusyWorkers;
}

void ThreadPool::Enqueue(Dom::IUnknown* pThreadJob) {
	pThreadJob->AddRef();
	Enqueue([](Dom::IUnknown* pThreadJob,const atomic_bool& Yet) {
		IThreadJob* job = nullptr;
		if (pThreadJob->QueryInterface(IThreadJob::guid(), (void**)&job)) {
			job->Exec(Yet);
			job->Release();
		}
		else {
			trace("Interface `IThreadJob` not implemnted");
		}
		job->Release();
	}, pThreadJob, ref(PoolYet));
}

void ThreadPool::CheckResize(size_t NumWorker, size_t NumTasks) {
	auto AvailableWorkers = NumWorker - (long)PoolBusyWorkers;
	size_t NewWorkerCount = 0;

	if (NumTasks > AvailableWorkers) {
		NewWorkerCount = size_t(((NumTasks - AvailableWorkers) + (PoolInitWorkers - 1)) / PoolInitWorkers) * PoolInitWorkers;
	}

	if ((NumWorker + NewWorkerCount) > PoolMaxWorkers) {
		NewWorkerCount = PoolMaxWorkers - NumWorker;
	}

	if (NewWorkerCount > 0) {
		Hire(NewWorkerCount, sExtended);
	}
}

void ThreadPool::Hire(size_t NumWorkers,ThreadPool::WorkerType Type) {
	unique_lock<mutex> lock(PoolQueueSync);
	while (NumWorkers --)
	{
		std::thread ant([](
			size_t core, size_t num, atomic_bool& Yet, mutex& Sync, condition_variable& Condition,
			queue< std::function<void()> >& Tasks, threads& Workers, atomic_long& Busy, ThreadPool::WorkerType Type)
		{
			string thread_name(string(Type == ThreadPool::sPersistent ? "Peristent" : (Type == ThreadPool::sExtended ? "Extended" : (Type == ThreadPool::sAsync ? "Async" : "Unknown"))) + "#" + to_string(num));
			auto timeout = std::chrono::milliseconds(10000);

			set_thread_name(move(thread_name));
			set_thread_affinity(core);

			while (1) {
				function<void()> task;
				{
					unique_lock<mutex> lock(Sync);
					Condition.wait_for(lock, timeout, [&Yet, &Tasks]
					{
						return !Yet || !Tasks.empty();
					});
					if (!Yet) {
						trace("Terminate thread: `%s`", thread_name.c_str());
						return;
					}

					/* false negative */
					if (Tasks.empty()) {
						if (Type == ThreadPool::sPersistent) continue;
						auto&& th = Workers.find(this_thread::get_id());
						if (th != Workers.end()) {
							th->second.detach();
							trace("Leave thread: `%s` (%ld)", thread_name.c_str(), Workers.erase(this_thread::get_id()));
						}
						else {
							trace("Gone thread: `%s` (%ld)", thread_name.c_str(), this_thread::get_id());
						}
						return;
					}

					task = std::move(Tasks.front());
					Tasks.pop();
				}
				Busy += 1;
				task();
				Busy-=1;
			}
		}, PoolWorkersBinding[PoolIndexWorkers%PoolWorkersBinding.size()], PoolIndexWorkers++, ref(PoolYet), ref(PoolQueueSync), ref(PoolCondition), ref(PoolTasks), ref(PoolWorkers),ref(PoolBusyWorkers), Type);

		PoolWorkers.emplace(ant.get_id(), move(ant));
	}
}