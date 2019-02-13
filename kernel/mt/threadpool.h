#pragma once
#include <unordered_map>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <future>
#include <stdexcept>
#include <interface/IThreadPool.h>

namespace Core {
	using namespace std;
	class ThreadPool {
	private:
		using threads = unordered_map< std::thread::id, thread >;
		enum WorkerType { sPersistent, sExtended, sAsync};
		atomic_bool					PoolYet;
		size_t						PoolInitWorkers;
		size_t						PoolMaxWorkers;
		size_t						PoolIndexWorkers;
		atomic_long					PoolBusyWorkers;
		threads						PoolWorkers;
		queue< function<void()> >	PoolTasks;
		mutex						PoolQueueSync;
		condition_variable			PoolCondition;
		vector<size_t>				PoolWorkersBinding;
		inline void CheckResize(size_t NumWorker, size_t NumTasks);
		inline void Hire(size_t NumWorker, WorkerType Type);
	public:
		ThreadPool(const string& InitialWorkers, const string& MaxWorkers,const string& CoreBinding = "1-2", const string& ExcludeBinding = "");
		~ThreadPool();

		void Join();

		void GetState(size_t& NumWorkers, size_t& NumAwaitingTasks, size_t& NumBusy);

		void Enqueue(Dom::IUnknown* pThreadJob);

		void Async(Dom::IUnknown* pThreadJob);

		template<class FN, class... Args>
		auto Enqueue(FN&& f, Args&&... args) -> std::future<typename std::result_of<FN(Args...)>::type> {
			
			using return_type = typename std::result_of<FN(Args...)>::type;

			auto task = make_shared< packaged_task<return_type()> >(
				bind(forward<FN>(f), std::forward<Args>(args)...)
			);

			size_t numWorkers = 0, numTasks = 0;
			std::future<return_type> res = task->get_future();
			{
				unique_lock<mutex> lock(PoolQueueSync);
				if (!PoolYet)
					throw std::runtime_error("enqueue on stopped ThreadPool");

				PoolTasks.emplace([task]() { (*task)(); });

				numWorkers = PoolWorkers.size();
				numTasks = PoolTasks.size();
			}
			CheckResize(numWorkers, numTasks);
			PoolCondition.notify_one();
			return res;
		}
	};
}