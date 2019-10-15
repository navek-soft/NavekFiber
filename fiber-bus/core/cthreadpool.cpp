#include "cthreadpool.h"
#include <unordered_set>

using namespace core;

cthreadpool::cthreadpool(size_t InitialWorkers, size_t MaxWorkers, const std::vector<size_t>& CoreBinding, const std::vector<size_t>& CoreExclude) :
	PoolYet(true), PoolInitWorkers(InitialWorkers), PoolMaxWorkers(MaxWorkers), PoolIndexWorkers(0), PoolBusyWorkers(0)
{

	/* Initialize CPUs bindings  */
	{
		size_t max_threads = std::thread::hardware_concurrency();
		std::unordered_set<size_t> uniqCores, excludeCores(CoreExclude.begin(), CoreExclude.end());

		if (CoreBinding.empty()) {
			for (; max_threads--;) {
				if (excludeCores.find(max_threads) == excludeCores.end()) {
					uniqCores.emplace(max_threads);
				}
			}
		}
		else {
			for (auto&& co : CoreBinding) {
				if (excludeCores.find(co % max_threads) == excludeCores.end()) {
					uniqCores.emplace(co % max_threads);
				}
			}
		}
		for (auto&& co : uniqCores) {
			PoolWorkersBinding.emplace_back(co);
		}
	}
	if (PoolWorkersBinding.empty() || !PoolInitWorkers || PoolMaxWorkers < PoolInitWorkers) {
		throw std::runtime_error("Invalid thread pool initialization.");
	}

	hire(PoolInitWorkers, sPersistent);
}
cthreadpool::~cthreadpool() {
	join();
}

void cthreadpool::join() {
	if (PoolYet) {
		{
			std::unique_lock<std::mutex> lock(PoolQueueSync);
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

void cthreadpool::stats(size_t& NumWorkers, size_t& NumAwaitingTasks, size_t& NumBusy) {
	{
		std::unique_lock<std::mutex> lock(PoolQueueSync);
		NumWorkers = PoolWorkers.size();
		NumAwaitingTasks = PoolTasks.size();
	}
	NumBusy = (long)PoolBusyWorkers;
}

void cthreadpool::check_resize(size_t NumWorker, size_t NumTasks) {
	auto AvailableWorkers = NumWorker - (long)PoolBusyWorkers;
	size_t NewWorkerCount = 0;

	if (NumTasks > AvailableWorkers) {
		NewWorkerCount = size_t(((NumTasks - AvailableWorkers) + (PoolInitWorkers - 1)) / PoolInitWorkers) * PoolInitWorkers;
	}

	if ((NumWorker + NewWorkerCount) > PoolMaxWorkers) {
		NewWorkerCount = PoolMaxWorkers - NumWorker;
	}

	if (NewWorkerCount > 0) {
		hire(NewWorkerCount, sSecondary);
	}
}

void cthreadpool::hire(size_t NumWorkers, WorkerType Type) {
	std::unique_lock<std::mutex> lock(PoolQueueSync);
	while (NumWorkers--)
	{
		std::thread ant([](
			ssize_t core, size_t num, std::atomic_bool& Yet, std::mutex& Sync, std::condition_variable& Condition,
			std::queue< std::function<void()> >& Tasks, threads& Workers, std::atomic_long& Busy, cthreadpool::WorkerType Type)
			{
				auto timeout = std::chrono::milliseconds(10000);

				if (core != -1) set_thread_affinity(core);

				while (1) {
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(Sync);
						Condition.wait_for(lock, timeout, [&Yet, &Tasks]
							{
								return !Yet || !Tasks.empty();
							});


						/* false negative */
						if (Tasks.empty()) {
							if (!Yet) { break; }
							if (Type == sPersistent) continue;
							auto&& th = Workers.find(std::this_thread::get_id());
							if (th != Workers.end()) {
								th->second.detach();
								//dbg_trace("Leave thread: `%s` (%ld)", thread_name.c_str(), Workers.erase(this_thread::get_id()));
							}
							else {
								//dbg_trace("Gone thread: `%s` (%ld)", thread_name.c_str(), this_thread::get_id());
							}
							break;
						}

						task = std::move(Tasks.front());
						Tasks.pop();
					}
					Busy += 1;
					task();
					Busy -= 1;
				}
			}, PoolWorkersBinding[PoolIndexWorkers % PoolWorkersBinding.size()], PoolIndexWorkers, ref(PoolYet), ref(PoolQueueSync), ref(PoolCondition), ref(PoolTasks), ref(PoolWorkers), ref(PoolBusyWorkers), Type);
		PoolIndexWorkers++;
		PoolWorkers.emplace(ant.get_id(), move(ant));
	}
}