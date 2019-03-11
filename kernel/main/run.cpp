#include "kernel.h"
#include "trace.h"
#include <sys/types.h>
#include "impl/cfgimpl.h"
#include <csignal>
#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>
#include <coreexcept.h>
#include <utils/option.h>
#include <string>

#include "init-cmd.hpp"
#include "init-server.hpp"
#include "init-threadpool.hpp"
#include "init-mq.hpp"
#include "init-sapi.hpp"

using namespace std;
using namespace Fiber;

sig_atomic_t Kernel::procSignal = 0;

template<typename ... SIGN>
static inline void handleSignals(__sighandler_t&& handler, SIGN...signals) {
	int signals_list[sizeof...(signals) + 1] = { signals ...,0 };
	for (auto&& n = 0; signals_list[n]; n++) {
		::signal(n, handler);
	}
}

Kernel::Kernel() { ; }

Kernel::~Kernel() { ; }

void Kernel::Run(int argc, char* argv[]) {
	
	using cconfig = unordered_map<string, pair<list<string>, unordered_map<string, string>>>;

	unique_ptr<Server>		fiberServer;
	unique_ptr<ThreadPool>	fiberWorkers;
	unique_ptr<ConfigImpl>	fiberConfig;

	
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

				fiberConfig = std::unique_ptr<ConfigImpl>(new ConfigImpl(move(config_data), move(programDir), move(programWorkDir), move(programName)));
				
				auto&& numMQ = initMQ((*fiberConfig)["fiber"]["mq"],*this, fiberMQModules);

				auto&& numSAPI = initSAPI((*fiberConfig)["fiber"]["sapi"], *this, fiberSAPIModules);

				auto&& numListeners = initServer((*fiberConfig)["fiber"]["server"], fiberServer);

				if (!numMQ || !numListeners) {
					log_print("<System.Startup.Error> Invalid Fiber.Bus configuration.");
					exit(-3);
				}
				initThreadPool((*fiberConfig)["fiber"]["server"], fiberWorkers);

			}
		}
		catch (exception& ex) {
			log_print("[ %s:%s ] %s", "EXCEPTION", "STARTUP", ex.what());
			exit(-2);
		}
	}
	/* Main Loop */
	{
		handleSignals([](int sig_no) {
			procSignal = sig_no;
		},SIGINT, SIGQUIT, SIGTERM, SIGUSR1, SIGUSR2);

		handleSignals([](int sig_no) {
			void *array[10];
			size_t size;
			size = backtrace(array, 10);
			log_print("[ SIGSEGV ]");
			backtrace_symbols_fd(array, (int)size, STDOUT_FILENO);
			exit(-1);
		}, SIGSEGV);

		auto&& requestHandler = [&fiberWorkers](shared_ptr<Server::CHandler> handler) {
			fiberWorkers->Enqueue([](shared_ptr<Server::CHandler> handler) {
				try {
					handler->Process();
					handler->Reply(200, nullptr, (const uint8_t*)"test\n", 5, { {"XSample-HEADER","909"} });
				}
				catch (RuntimeException& ex) {
					log_print("<Server.Runtime.Exception> %s", ex.what());
				}
				catch (std::exception& ex) {
					log_print("<Server.Std.Exception> %s", ex.what());
				}
			}, handler);
		};

		while (fiberServer->Listen(requestHandler, 3000)) {
			if (procSignal == 0) {
				continue;
			}
			else if (procSignal == SIGUSR1 || procSignal == SIGUSR2) {
				log_print("[ %s ] Signal recvd.", signalNames.at(procSignal).c_str());
				procSignal = 0;
				continue;
			}
			else if (procSignal == SIGINT || procSignal == SIGQUIT || procSignal == SIGTERM) {
				log_print("[ %s ] Signal recvd. Exit now.", signalNames.at(procSignal).c_str());
				break;
			}
		}
		{
			fiberServer.release();
		}
		{
			fiberWorkers.release();
		}
	}
}