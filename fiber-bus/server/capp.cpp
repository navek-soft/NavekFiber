#include "pch.h"
#include "capp.h"
#include "chttpserver.h"
#include "../core/cexcept.h"
#include "../core/coption.h"
#include "channel/cchannel-queue.h"
#include "channel/cchannel-sync.h"
#include "csapiserver.h"

using namespace fiber;

std::weak_ptr<fiber::cbroker> gEsbBroker;
std::weak_ptr<core::cserver::base> gEsbSapi;
std::weak_ptr<core::cthreadpool> gEsbThreadPool;

void capp::dispatch(std::shared_ptr<fiber::crequest>&& msg) {
	std::weak_ptr<fiber::cbroker> lock(gEsbBroker);
	if (auto&& ptr = gEsbBroker.lock(); ptr) { ptr->enqueue(std::move(msg)); }
}

ssize_t capp::execute(const cmsgid& msg_id, const std::string& sapi, const std::string& execscript, std::size_t task_limit, std::size_t task_timeout, const std::shared_ptr<fiber::crequest>&& msg) {
	std::weak_ptr<core::cserver::base> lock(gEsbSapi);
	if (auto ptr = lock.lock(); ptr) {
		return ((fiber::csapiserver*)ptr.get())->execute(sapi, execscript, task_limit, task_timeout, msg_id,msg);
	}
	return 0;
}

std::weak_ptr<core::cthreadpool> capp::threadpool() { 
	return gEsbThreadPool;
}



int capp::run(int argc, char* argv[])
{
	core::coptions options;

	std::shared_ptr<core::cthreadpool> esbThreadPool(new core::cthreadpool(options.at("threads-count", "1").number(), options.at("threads-max", "1").number()));
	gEsbThreadPool = esbThreadPool;

	{
		std::shared_ptr<fiber::cbroker> esbBroker(new fiber::cbroker(options));

		gEsbBroker = esbBroker;
		
		options.emplace("channel-sapi", "python://sample-python-test.py");

		esbBroker->emplace("/btk/users/", std::shared_ptr<fiber::cchannel>(new fiber::cchannel_queue("btk-users",options)));
		esbBroker->emplace("/btk/services/", std::shared_ptr<fiber::cchannel>(new fiber::cchannel_sync({})));

		{
			core::cserver esbServer;
			try {
				core::coptions::list optlist;
				optlist.emplace("port", "8080");
				optlist.emplace("fpm-sapi-pipe", "/tmp/navekfiber-fpm");

				auto&& sapiServer = std::shared_ptr<core::cserver::base>(new fiber::csapiserver(core::coptions(optlist)));

				gEsbSapi = sapiServer;

				esbServer.emplace(std::shared_ptr<core::cserver::base>(new fiber::chttpserver(core::coptions(optlist))));
				esbServer.emplace(std::move(sapiServer));

				while (esbServer.listen(-1) >= 0) {
					;
				}

				printf("exit from main loop\n");
			}
			catch (core::system_error & er) {
				printf("bus: catch exception `%s`\n", er.what());
			}
			esbServer.shutdown();
		}
		esbBroker.reset();
	}
	gEsbThreadPool.reset();
	esbThreadPool->join();
	return 0;
}