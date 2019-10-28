#pragma once
#include <deque>
#include <mutex>
#include <list>
#include "../core/cserver.h"
#include "../core/coption.h"
#include "cmemory.h"
#include "crequest.h"
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <queue>

namespace fiber {
	class csapiserver : public core::cserver::pipe {
	private:
		
		struct delay_exec_state {
			size_t	task_executing{ 0 }, task_limit{ 0 }, task_timeout{0};
			time_t task_timestamp{ 0 };

			delay_exec_state() = default;
			~delay_exec_state() = default;
			delay_exec_state(const delay_exec_state& st) : task_executing(st.task_executing), task_limit(st.task_limit), task_timeout(st.task_timeout), task_timestamp(st.task_timestamp) { ; }
		};

		struct sapi_pool {
			std::unordered_set<int> workers;
			std::queue<cmsgid>		tasks;

			sapi_pool() = default;
			~sapi_pool() = default;
			sapi_pool(const sapi_pool& p) : workers(p.workers), tasks(p.tasks) { ; }
		};

		struct exec_task {
			size_t task_limit{ 0 }, task_timeout{ 0 };
			std::string sapi, execscript;
			std::shared_ptr<crequest> request;

			exec_task() = default;
			~exec_task() = default;

			exec_task(const std::string& sapiname, const std::string& script, size_t tasklimit, size_t tasktimeout, const std::shared_ptr<crequest>& _request) :
				task_limit(tasklimit), task_timeout(tasktimeout), sapi(sapiname), execscript(script), request(_request) {
				;
			}
			exec_task(const exec_task& t) :
				task_limit(t.task_limit), task_timeout(t.task_timeout), sapi(t.sapi), execscript(t.execscript), request(t.request) {
				;
			}
		};

		mutable std::mutex	syncPool, syncQueue;
		mutable std::unordered_map<std::string, delay_exec_state> taskDelayExec;
		mutable std::unordered_map<std::string, sapi_pool> workerPool;
		mutable std::unordered_map<cmsgid, exec_task> taskPool;
		std::unordered_set<std::string> sapiList;

		std::condition_variable  execNotify;
		std::thread				 execManager;
		std::atomic_bool		 execDoIt{true};

		void whirligig();
	public:
		static inline const std::string server_banner{ "csapiserver" };
		virtual const std::string& getname() const { return server_banner; }

		csapiserver(const core::coptions& options);
		virtual ~csapiserver();
	public:
		ssize_t execute(const std::string& sapi, const std::string& execscript,std::size_t task_limit, std::size_t task_timeout, cmsgid& msg_id, const std::shared_ptr<crequest>& request);
		virtual void onshutdown() const;
		virtual void onclose(int soc) const;
		virtual void onconnect(int soc, const sockaddr_storage&, uint64_t) const;
		virtual void ondisconnect(int soc) const;
		virtual void ondata(int soc) const;
		virtual void onwrite(int soc) const;
		virtual void onidle(uint64_t time) const;
	};
}