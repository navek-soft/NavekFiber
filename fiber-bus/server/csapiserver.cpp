#include "csapiserver.h"
#include <list>
#include <sys/ioctl.h>
#include <unistd.h>
#include <endian.h>
#include <cstdarg>
#include "capp.h"
#include <netinet/tcp.h>

using namespace fiber;

csapiserver::csapiserver(const core::coptions& options)
	: core::cserver::pipe(options.at("fpm-sapi-pipe","/tmp/navekfiber-fpm").get())
{
	printf("sapi-fpm: start at `%s` endpoint\n", options.at("fpm-sapi-pipe", "/tmp/navekfiber-fpm").get().c_str());

	sapiList.emplace("python");
	sapiList.emplace("php");

	execManager = std::thread([](csapiserver* owner,std::condition_variable& notifer, std::mutex& sync,std::atomic_bool& yet) {
		printf("sapi (manager) start\n");
		do {
			std::unique_lock<std::mutex> lock(sync);
			notifer.wait(lock);
			if (yet) {
				owner->whirligig();
			}
		} while (yet);
		printf("sapi (manager) goin down\n");

	},this,std::ref(execNotify), std::ref(syncQueue), std::ref(execDoIt));

	//optReceiveTimeout = (time_t)(options.at("receive-timeout", "3000").number());
	//optMaxRequestSize = (ssize_t)options.at("max-request-size", "5048576").bytes();
}

csapiserver::~csapiserver() {
	if (execManager.joinable()) {
		execDoIt = false;
		execNotify.notify_one();
		execManager.join();
	}
}

void csapiserver::onshutdown() const {
	printf("csapiserver::onshutdown\n");
}

void csapiserver::onclose(int soc) const {
	//if (auto&& cli = requestsClient.find(soc); cli != requestsClient.end()) {
	//	requestsClient.erase(cli);
	//}
	printf("csapiserver::onclose\n");
}

void csapiserver::onconnect(int soc, const sockaddr_storage&, uint64_t time) const {

	printf("csapiserver::onconnect\n");

	//auto&& req = requestsClient.emplace(soc, std::shared_ptr<request>(new request(soc, optReceiveTimeout, time)));
	//if (!req.second) {
	//	req.first->second.get()->reset();
	//	req.first->second->update_time();
	//}
	//else {
	//	int opt = 1;	setsockopt(soc, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
	//	opt = 30;		setsockopt(soc, IPPROTO_TCP, TCP_KEEPINTVL, &opt, sizeof(opt));
	//}
	//printf("onconnect (%ld)\n", soc);
	//struct timeval tv = { optReceiveTimeout / 1000, optReceiveTimeout % 1000 };
	//setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO, (struct timeval*) & tv, sizeof(struct timeval));
}

void csapiserver::ondisconnect(int soc) const {
	printf("csapiserver::ondisconnect\n");
	//printf("ondisconnect (%ld)\n", soc);
	//if (auto&& cli = requestsClient.find(soc); cli != requestsClient.end()) {
	//	requestsClient.erase(cli);
	//}
}

void csapiserver::onidle(uint64_t time) const {
	printf("csapiserver::onidle\n");
}

void csapiserver::whirligig() {

	std::unique_lock<std::mutex> lock(syncPool);

	for (auto&& pool : workerPool) {
		if (pool.second.workers.empty() || pool.second.tasks.empty()) {
			continue; // no worker found or no tasks, skip pool
		}
		cmsgid first_id(pool.second.tasks.front());

		do {
			/* deque task from front */

			auto&& msg_id = pool.second.tasks.front();
			pool.second.tasks.pop();

			if (auto&& task = taskPool.find(msg_id); task != taskPool.end()) {

				/* limited of number or period execution task, check number currently running */
				if (task->second.task_limit || task->second.task_timeout) {
					if (auto&& ds_it = taskDelayExec.find(task->second.execscript); ds_it != taskDelayExec.end()) {
						if (task->second.task_limit) {
							if (ds_it->second.task_executing >= ds_it->second.task_limit) {
								/* can not execute task now, return back to queue */
								pool.second.tasks.emplace(msg_id);
								continue; // next task
							}
						}
						if (task->second.task_timeout) {
							if (std::time_t(ds_it->second.task_timestamp + task->second.task_timeout) >= std::time(nullptr)) {
								/* can not execute task now, return back to queue */
								pool.second.tasks.emplace(msg_id);
								continue; // next task
							}

							ds_it->second.task_timestamp = std::time(nullptr);
						}

						printf("sapi (%s) execute delayed `%s` (%s) count of executing (%ld) \n", pool.first.c_str(), task->second.execscript.c_str(), msg_id.str().c_str(), ds_it->second.task_executing);
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

					/* task success send to execute, remove it from pool */
					printf("sapi (%s) execute %s(%s)\n", pool.first.c_str(), task->second.execscript.c_str(),msg_id.str().c_str());
					taskPool.erase(task);
				}
				else {
					/* enque message back */
					pool.second.tasks.emplace(msg_id);
					printf("sapi (%s) all workers are busy\n",pool.first.c_str());

					break; /* no workers found, go to next pool */
				}
			}
			else {
				printf("sapi (%s) zombie message %s\n", pool.first.c_str(), msg_id.str().c_str());
			}

		} while (first_id != pool.second.tasks.front());
	}
}

ssize_t csapiserver::execute(const std::string& sapi, const std::string& execscript, std::size_t task_limit, std::size_t task_timeout, cmsgid& msg_id, const std::shared_ptr<crequest>& request) {
	
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
		}
		printf("sapi (execute) emplace %s << %s\n", sapi.c_str(), msg_id.str().c_str());
		return 202;
	}
	printf("sapi (execute) bad sapi `%s`\n", sapi.c_str());
	return 415;
}

void csapiserver::ondata(int soc) const {

	std::shared_ptr<crequest> msg(new csapi::request(soc));

	switch (msg->request_type())
	{
	case crequest::connect:
	{
		auto&& sapi = msg->request_headers().at({ "x-fiber-sapi",12 }).str();

		if (sapiList.find(sapi) != sapiList.end()) {
			if (msg->response("SHELLO", "/", 200, crequest::connect, {}) == 200) {
				std::size_t num_workers = 0;
				{
					std::unique_lock<std::mutex> lock(syncPool);
					/* may be ondisconnect not call, for socket number? it is bad :(. if you want, handle this situation now */
					auto&& it = workerPool.emplace(sapi, sapi_pool());
					it.first->second.workers.emplace(soc);
					num_workers = it.first->second.workers.size();
				}
				msg->disconnect(); /* for unix socket is dummy */
				printf("sapi (connect): %s, worker-count: %ld\n", sapi.c_str(), num_workers);
				return;
			}
			else {
				printf("sapi (connect) bad channel `%s`\n", sapi.c_str());
			}
		}
		else {
			printf("sapi (connect) sapi `%s` not declarted\n", sapi.c_str());
		}
	}
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
