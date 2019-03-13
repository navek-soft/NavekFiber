#include "kernel.h"
#include "trace.h"
#include <sys/types.h>
#include <csignal>
#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>
#include <coreexcept.h>
#include <utils/option.h>
#include <string>

#include "impl/cfgimpl.h"
#include "impl/routerimpl.h"

#include "init-cmd.hpp"
#include "init-server.hpp"
#include "init-threadpool.hpp"
#include "init-mq.hpp"
#include "init-sapi.hpp"
#include "init-channels.hpp"
#include "init-routes.hpp"

using namespace std;
using namespace Fiber;

sig_atomic_t Kernel::procSignal = 0;

template<typename ... SIGN>
static inline void handleSignals(__sighandler_t&& handler, SIGN...signals) {
	int signals_list[sizeof...(signals) + 1] = { signals ...,0 };
	for (auto&& n = 0; signals_list[n]; n++) {
		struct sigaction act;
		memset(&act, 0, sizeof(act));
		act.sa_handler = handler;
		sigaction(n, &act, 0);
	}
}

Kernel::Kernel() { ; }

Kernel::~Kernel() { ; }

void Kernel::Run(int argc, char* argv[]) {
	
	using cconfig = unordered_map<string, pair<list<string>, unordered_map<string, string>>>;

	unique_ptr<ConfigImpl>	fiberConfig;
	unique_ptr<RouterImpl>	fiberRouter;
	unique_ptr<Server>		fiberServer;
	unique_ptr<ThreadPool>	fiberWorkers;

	std::unordered_map<std::string, std::tuple<std::string, std::string, std::string, std::shared_ptr<Fiber::ConfigImpl>>> fiberChannels;
	
	const static unordered_map<int, string> signalNames({ {SIGUSR1,"SIGUSR1"},{SIGUSR2,"SIGUSR2"},{SIGINT,"SIGINT"},{SIGQUIT,"SIGQUIT"},{SIGTERM,"SIGTERM"} });

	procSignal = 0;

	/* Startup Kernel */
	{
		try {
			
			cconfig	config_data;
			/* Initialize ESB */
			{
				sys::cmdline&& cmd = initCmdLine(argc, argv, programDir, programWorkDir, programName);

				initConfig(cmd("config", "fiber.config"), move(config_data));

				log_option("Configuration from", "%s", cmd("config", "fiber.config").c_str());

				fiberConfig = std::unique_ptr<ConfigImpl>(new ConfigImpl(move(config_data), move(programDir), move(programWorkDir), move(programName)));
				
				if (!initMQ((*fiberConfig)["fiber"]["mq"], *this, fiberMQModules))
					throw std::runtime_error("No valid MQ modules found");

				initSAPI((*fiberConfig)["fiber"]["sapi"], *this, fiberSAPIModules);

				if (!initChannels((*fiberConfig)["channel"], fiberChannels, fiberMQModules, fiberSAPIModules))
					throw std::runtime_error("No valid Channels found");

				fiberRouter = std::unique_ptr<RouterImpl>(new RouterImpl);

				if (!initRoutes(*fiberRouter, *this,fiberChannels, fiberMQModules))
					throw std::runtime_error("No valid Routes found");

				if (!initServer((*fiberConfig)["fiber"]["server"], fiberServer)) 
					throw std::runtime_error("No valid Listeners found");

				initThreadPool((*fiberConfig)["fiber"]["pool"], fiberWorkers);

			}
		}
		catch (exception& ex) {
			log_print("<System.Startup.Error> %s", ex.what());
			exit(-2);
		}
	}
	/* Main Loop */
	{
		handleSignals([](int sig_no) {
			procSignal = sig_no;
		},SIGINT, SIGQUIT, SIGTERM, SIGKILL, SIGSTOP);

		handleSignals([](int sig_no) {
			void *array[10];
			size_t size;
			size = backtrace(array, 10);
			log_print("[ SIGSEGV ]");
			backtrace_symbols_fd(array, (int)size, STDOUT_FILENO);
			exit(-1);
		}, SIGSEGV);

		auto&& requestHandler = [&fiberWorkers,&fiberRouter](shared_ptr<Server::CHandler>&& handler) {
			fiberWorkers->Enqueue([&fiberRouter](shared_ptr<Server::CHandler>& request) {
				fiberRouter->Process(request);
			}, std::ref(handler));
		};

		while (fiberServer->Listen(requestHandler, 3000)) {
			if (procSignal == 0) {
				continue;
			}
			else if (procSignal == SIGINT || procSignal == SIGQUIT || procSignal == SIGTERM) {
				log_print("[ %s ] Signal recvd. Exit now.", signalNames.at(procSignal).c_str());
				fflush(stdout);
				break;
			}
		}

		fiberServer.reset();
		fiberWorkers.reset();
		fiberRouter.reset();
	}
}