#include "csapiserver.h"
#include <list>
#include <sys/ioctl.h>
#include <unistd.h>
#include <endian.h>
#include <cstdarg>
#include "capp.h"
#include <netinet/tcp.h>
#include "clog.h"

using namespace fiber;

csapiserver::csapiserver(const core::coptions& options)
	: core::cserver::pipe(options.at("sapi-pipe-path","/tmp/navekfiber-fpm").get())
{
	sapiList = options.at("sapi-engines").sequence();
	execManager = std::thread([](csapiserver* owner,std::condition_variable& notifer, std::mutex& sync,std::atomic_bool& yet) {
		clog::log(0, "( sapi:server ) thread startup\n");
		do {
			std::unique_lock<std::mutex> lock(sync);
			notifer.wait(lock);
			if (yet) {
				owner->whirligig();
			}
		} while (yet);
		clog::log(0, "( sapi:server ) thread goin down\n");
	},this,std::ref(execNotify), std::ref(syncQueue), std::ref(execDoIt));
}

csapiserver::~csapiserver() {
	if (execManager.joinable()) {
		execDoIt = false;
		execNotify.notify_one();
		execManager.join();
	}
}

void csapiserver::onshutdown() const {
	clog::log(0, "( sapi:server ) shutdown\n");
}

void csapiserver::onclose(int soc) const {
	std::unique_lock<std::mutex> lock(syncPool);
	if (auto&& task = taskExecute.find(soc); task != taskExecute.end()) {
		clog::log(0, "( sapi:server ) close connection #%ld\n", soc);
		task->second.second->response(ci::cstringformat(), 499, "SAPI execution error");
	}
	else {
		clog::log(0, "( sapi:server ) close connection #%ld\n", soc);
	}
	taskExecute.erase(soc);
}

void csapiserver::onconnect(int soc, const sockaddr_storage&, uint64_t time) const {

	clog::log(0, "( sapi:server ) accept connection #%ld\n", soc);
}

void csapiserver::ondisconnect(int soc) const {
	std::unique_lock<std::mutex> lock(syncPool);
	if (auto&& task = taskExecute.find(soc); task != taskExecute.end()) {
		clog::log(0, "( sapi:server ) sapi process is down #%ld\n", soc);
		task->second.second->response(ci::cstringformat(), 499, "SAPI execution error");
		task->second.second->disconnect();
	}
	else {
		clog::log(0, "( sapi:server ) disconnect client #%ld\n", soc);
	}
	taskExecute.erase(soc);
}

void csapiserver::onidle(uint64_t time) const {
	execNotify.notify_one();
}

void csapiserver::whirligig() {

	std::unique_lock<std::mutex> lock(syncPool);

	for (auto&& pool : workerPool) {
		if (pool.second.tasks.empty()) {
			continue;
		}
		if (pool.second.workers.empty()) {
			clog::log(0, "( sapi:server:%s ) all workers are busy <pool.size=%ld>\n", pool.first.c_str(), pool.second.tasks.size());
			continue; // no worker found or no tasks, skip pool
		}
		std::queue<fiber::cmsgid> unprocessed;

		do {
			/* deque task from front */

			auto&& msg_id = pool.second.tasks.front();
			pool.second.tasks.pop();

			std::unordered_map<std::string, delay_exec_state>::iterator itDelayed = taskDelayExec.end();

			if (auto&& task = taskPool.find(msg_id); task != taskPool.end()) {

				/* limited of number or period execution task, check number currently running */
				if (task->second.task_limit || task->second.task_timeout) {
					if (itDelayed = taskDelayExec.find(task->second.execscript); itDelayed != taskDelayExec.end()) {
						if (task->second.task_limit) {
							if (itDelayed->second.task_executing >= task->second.task_limit) {
								/* can not execute task now, return back to queue */
								unprocessed.emplace(msg_id);
								clog::log(0, "( sapi:server:%s ) task limits `%s` (%s) count of executing %ld (%ld)\n", pool.first.c_str(), task->second.execscript.c_str(), msg_id.str().c_str(),
									itDelayed->second.task_executing, taskPool.size());
								continue; // next task
							}
						}
						if (task->second.task_timeout) {
							if (std::time_t(itDelayed->second.task_timestamp + task->second.task_timeout) >= std::time(nullptr)) {
								/* can not execute task now, return back to queue */
								unprocessed.emplace(msg_id);
								clog::log(0, "( sapi:server:%s ) wait timeout `%s` (%s) count of executing (%ld), %ld (%ld) \n", pool.first.c_str(), task->second.execscript.c_str(), msg_id.str().c_str(), itDelayed->second.task_executing,
									std::time(nullptr) - itDelayed->second.task_timestamp, task->second.task_timeout);
								continue; // next task
							}
						}
						clog::log(0, "( sapi:server:%s ) execute delayed `%s` (%s) count of executing %ld (%ld) \n", pool.first.c_str(), task->second.execscript.c_str(), msg_id.str().c_str(), itDelayed->second.task_executing, task->second.task_limit);
					}
				}

				/* immediately execute, find first idle worker */
repeat_with_next_socket:
				if (auto&& it_so = pool.second.workers.begin(); it_so != pool.second.workers.end()) {
					int so = *it_so;
					pool.second.workers.erase(so);

					/* create request */

					csapi::response msg;

					if (msg.reply(so,task->second.request->request_paload(), task->second.request->request_paload_length(),
						task->second.request->request_uri().str(), 200, crequest::type::put, { 
							{"X-FIBER-FPM",task->second.sapi},
							{"X-FIBER-FPM-EXEC",task->second.execscript},
							{"X-FIBER-MSG-ID",msg_id.str()},
						}) != 200) {
						/* invalid worker, close connection now */
						::close(so);

						goto repeat_with_next_socket;
					}
					if (itDelayed != taskDelayExec.end()) {
						itDelayed->second.task_timestamp = std::time(nullptr);
						itDelayed->second.task_executing++;
					}
					/* task success send to execute, remove it from pool */
					clog::log(0, "( sapi:server:%s ) execute %s(%s)\n", pool.first.c_str(), task->second.execscript.c_str(),msg_id.str().c_str());

					taskExecute.emplace(so, std::make_pair(msg_id, task->second.request));

					taskPool.erase(task);
				}
				else {
					/* enque message back */
					unprocessed.emplace(msg_id);
					clog::log(0, "( sapi:server:%s ) all workers are busy\n",pool.first.c_str());

					break; /* no workers found, go to next pool */
				}
			}
			else {
				clog::log(0, "( sapi:server:%s ) zombie message %s\n", pool.first.c_str(), msg_id.str().c_str());
			}

		} while (!pool.second.tasks.empty());
		pool.second.tasks.swap(unprocessed);

		clog::log(0, "( sapi:server:%s ) unprocessed %ld\n", pool.first.c_str(), pool.second.tasks.size());

	}
}

ssize_t csapiserver::execute(const std::string& sapi, const std::string& execscript, std::size_t task_limit, std::size_t task_timeout, const cmsgid& msg_id, const std::shared_ptr<crequest>& request) {
	
	if (auto&& pool = workerPool.find(sapi); pool != workerPool.end()) {
		{
			/* emplace task in pool */
			std::unique_lock<std::mutex> lock(syncQueue);
			taskPool.emplace(msg_id, exec_task(sapi, execscript, task_limit, task_timeout, request));

			if (task_limit || task_timeout) {
				taskDelayExec.emplace(execscript, delay_exec_state());
			}
		}
		{
			/* emplace task in queue */
			std::unique_lock<std::mutex> lock(syncPool);
			pool->second.tasks.emplace(msg_id);

			execNotify.notify_one();
		}
		clog::log(0, "( sapi:server:%s ) %s (limit: %ld, timeout: %ld)\n", sapi.c_str(), msg_id.str().c_str(), task_limit, task_timeout);
		return 202;
	}
	printf("sapi (execute) sapi interface `%s` not declarated\n", sapi.c_str());
	return 415;
}

bool csapiserver::OnCONNECT(int soc,const std::shared_ptr<crequest>& msg) const {

	auto&& sapi = msg->request_headers().at({ "x-fiber-sapi",12 }).trim().str();

	if (sapiList.find(sapi) != sapiList.end()) {
		if (msg->response("SHELLO", "/", 202, crequest::connect, {}) == 200) {
			std::size_t num_workers = 0;
			{
				std::unique_lock<std::mutex> lock(syncPool);
				/* may be ondisconnect not call, for socket number? it is bad :(. if you want, handle this situation now */
				auto&& it = workerPool.emplace(sapi, sapi_pool());
				it.first->second.workers.emplace(soc);
				num_workers = it.first->second.workers.size();
			}
			printf("sapi (%s): connect, worker-count: %ld\n", sapi.c_str(), num_workers);
			return true;
		}
		else {
			printf("sapi (connect) bad channel `%s`\n", sapi.c_str());
		}
	}
	else {
		printf("sapi (connect) sapi `%s` not declarted\n", sapi.c_str());
	}
	return false;
}

void csapiserver::OnPATCH(int soc, const std::shared_ptr<crequest>& msg) const {

	auto headers = msg->request_headers();

	auto&& sapi = headers.at({ "x-fiber-sapi",12 }).trim().str();
	auto&& msg_id = headers.at({ "x-fiber-msg-id",14 }).trim().str();

	switch (((csapi::request*)msg.get())->http_code()) {
	case 102: // FPM is accept message and executing now
		printf("sapi(%s) #%ld is busy execute (%s)\n", sapi.c_str(), soc, headers.at({ "x-fiber-msg-id",14 }).trim().str().c_str());
		break;
	case 100:
		{
			std::unique_lock<std::mutex> lock(syncPool);
			taskPool.erase(msg_id);
			taskExecute.erase(soc);
			//taskExecute.emplace(so, std::make_pair(msg_id, task->second.request));
		}
		{
			// if task delayed, release task executing counter
			if (auto&& script = headers.find({ "x-fiber-fpm-exec",16 }); script != headers.end()) {
				std::unique_lock<std::mutex> lock(syncPool);
				if (auto&& it = taskDelayExec.find(script->second.trim().str()); it != taskDelayExec.end()) {
					it->second.task_timestamp = std::time(nullptr);
					it->second.task_executing--;
				}
			}
		}
		{
			// FPM return back to pool after execute message
			if (sapiList.find(sapi) != sapiList.end()) {
				std::size_t num_workers = 0;
				{
					std::unique_lock<std::mutex> lock(syncPool);
					/* may be ondisconnect not call, for socket number? it is bad :(. if you want, handle this situation now */
					auto&& it = workerPool.emplace(sapi, sapi_pool());
					it.first->second.workers.emplace(soc);
					num_workers = it.first->second.workers.size();
				}
				printf("sapi (%s): #%ld came back (%s), worker-count: %ld\n", sapi.c_str(), soc, msg_id.c_str(), num_workers);
				execNotify.notify_one();
			}
			else {
				printf("sapi (connect) sapi `%s` not declarted\n", sapi.c_str());
				::close(soc);
			}
		}
		break;
	}
}


void csapiserver::OnPOST(int soc, const std::shared_ptr<crequest>& msg) const {
	capp::dispatch(msg);
	OnPATCH(soc, msg);
}


void csapiserver::ondata(int soc) const {

	std::shared_ptr<crequest> msg(new csapi::request(soc));

	switch (msg->request_type())
	{
	case crequest::connect: 
		if (OnCONNECT(soc, msg)) { break; }
	case crequest::patch:
		/* Response from SAPI FPM */
		/*
		httpcode: 202 accepted
		else: i dont known :) 
		*/
		OnPATCH(soc, msg);
		break;
	case crequest::post:
		OnPOST(soc, msg);
		break;
	default:
		msg->response("SBYE", "/", 400, crequest::connect, {});
		::close(soc);
		break;
	}
	/*
	if (auto&& cli = requestsClient.find(soc); cli != requestsClient.end()) {
		auto&& result = cli->second->get_chunk(optMaxRequestSize);
		cli->second->update_time();
		if (result == 200) {
			capp::broker().enqueue(std::shared_ptr<fiber::crequest>(cli->second));
			requestsClient.erase(cli);
		}
		else if (result == 202) {
			printf("(%ld) more data\n", soc);
		}
		else if (result > 299) {
			::shutdown(soc, SHUT_RD);
			cli->second->response({}, result, {}, { { "X-Server","NavekFiber ESB" } });
			cli->second->disconnect();
			requestsClient.erase(cli);
		}
	}
	else {
		fprintf(stderr, "socket is'tn client.\n");
		::close(soc);
	}
	*/
}

void csapiserver::onwrite(int soc) const {
	printf("csapiserver::ondata\n");
	//printf("onwrite (%ld)\n", soc);
}
