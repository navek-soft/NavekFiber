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
std::weak_ptr<fiber::csapiserver> gEsbSapi;
std::weak_ptr<core::cthreadpool> gEsbThreadPool;

void capp::dispatch(std::shared_ptr<fiber::crequest>&& msg) {
	std::weak_ptr<fiber::cbroker> lock(gEsbBroker);
	if (auto&& ptr = gEsbBroker.lock(); ptr) { ptr->enqueue(std::move(msg)); }
}

void capp::execute(cmsgid& msg_id, const std::string& sapi, const std::string& execscript, size_t task_limit, size_t task_timeout,  std::shared_ptr<fiber::crequest>&& msg) {
	std::weak_ptr<fiber::csapiserver> lock(gEsbSapi);
	if (auto&& ptr = gEsbSapi.lock(); ptr) { ptr->execute(sapi, execscript, task_limit, task_timeout,msg); }
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
		fiber::cbroker esbBroker({});

		gEsbBroker = esbBroker;
		
		esbBroker->emplace("/btk/users/", std::shared_ptr<fiber::cchannel>(new fiber::cchannel_queue({})));
		esbBroker->emplace("/btk/services/", std::shared_ptr<fiber::cchannel>(new fiber::cchannel_sync({})));

		{
			core::cserver esbServer;
			try {
				core::coptions::list optlist;
				optlist.emplace("port", "8080");
				optlist.emplace("fpm-sapi-pipe", "/tmp/navekfiber-fpm");


				
				esbServer.emplace(std::shared_ptr<core::cserver::base>(new fiber::chttpserver(core::coptions(optlist))));
				esbServer.emplace(std::shared_ptr<core::cserver::base>(new fiber::csapiserver(core::coptions(optlist))));
			

				while (esbServer.listen(-1) >= 0) {
					;
				}
			}
			catch (core::system_error & er) {
				printf("bus: catch exception `%s`\n", er.what());
			}
			esbServer.shutdown();
		}
		gesbBroker = nullptr;
	}
	gesbThreadPool = nullptr;
	esbThreadPool.join();
	return 0;
}