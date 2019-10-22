#include "pch.h"
#include "capp.h"
#include "chttpserver.h"
#include "../core/cexcept.h"
#include "../core/coption.h"
#include "channel/cchannel-queue.h"
#include "channel/cchannel-sync.h"

using namespace fiber;

fiber::cbroker*	gesbBroker = nullptr;
core::cthreadpool* gesbThreadPool = nullptr;

fiber::cbroker& capp::broker() { return *gesbBroker; }
core::cthreadpool& capp::threadpool() { return *gesbThreadPool; }

int capp::run(int argc, char* argv[])
{
	core::coptions options;
	core::cthreadpool esbThreadPool(options.at("threads-count", "1").number(), options.at("threads-max", "1").number());
	gesbThreadPool = &esbThreadPool;
	{
		fiber::cbroker esbBroker({});
		
		gesbBroker = &esbBroker;

		esbBroker.emplace("/btk/users/", std::shared_ptr<fiber::cchannel>(new fiber::cchannel_queue({})));
		esbBroker.emplace("/btk/services/", std::shared_ptr<fiber::cchannel>(new fiber::cchannel_sync({})));
		{
			core::cserver esbServer;
			try {
				core::coptions::list optlist;
				optlist.emplace("port", "8080");
				esbServer.emplace(new fiber::chttpserver(core::coptions(optlist)));
			}
			catch (core::system_error & er) {

			}

			while (esbServer.listen(-1) >= 0) {
				;
			}
			esbServer.shutdown();
		}
		gesbBroker = nullptr;
	}
	gesbThreadPool = nullptr;
	esbThreadPool.join();
	return 0;
}