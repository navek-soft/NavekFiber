#include "kernel.h"
#include "trace.h"
#include <sys/cmdline.h>
#include <sys/path.h>
#include <sys/types.h>
#include <unistd.h>
#include "impl/cfgimpl.h"
#include <csignal>
#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>
#include "../net/server.h"
#include "../mt/threadpool.h"
#include <coreexcept.h>
#include <utils/option.h>

using namespace std;
using namespace Fiber;

sig_atomic_t Kernel::procSignal = 0;

static inline sys::cmdline initCmdLine(int argc, char* argv[], string& programDir,string& programWorkDir,string& programName) {

	using coption = sys::cmdline::option;
	sys::cmdline cmd;

	string procPathName(argv[0]);
	programDir.assign(argv[0], procPathName.rfind('/') + 1);
	programName = procPathName.substr(procPathName.rfind('/') + 1);
	programWorkDir = sys::cwd();

	cmd.options(argc, argv,
		coption("config", 'c', 1, nullptr)
	);

	log_option("Program name","%s (PID: %ld)", programName.c_str(), getpid());
	log_option("Program directory", "%s", programDir.c_str(), getpid());
	log_option("Program work directory", "%s", programWorkDir.c_str());

	return cmd;
}

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
	
	unique_ptr<Server>		netServer;
	unique_ptr<ThreadPool>	poolWorkers;
	procSignal = 0;


	/* Startup Kernel */
	{
		try {
			using cconfig = unordered_map<string, pair<list<string>, unordered_map<string, string>>>;
			cconfig	config_data;
			/* Initialize ESB */
			{
				sys::cmdline&& cmd = initCmdLine(argc, argv, programDir, programWorkDir, programName);

				LoadConfig(cmd("config", "fiber.config"), move(config_data));

				ConfigImpl config(move(config_data), move(programDir), move(programWorkDir), move(programName));
				config.AddRef();

				auto&& mq = config["fiber"]["mq"];
				auto&& sapi = config["fiber"]["sapi"];
				/* Initialize server */
				{
					auto&& options = config["fiber"]["server"];
					Server server({ { "pool-workers","10" }, { "pool-cores","2-12" }, { "pool-exclude","3,4" } });
				}
				/* Intialize thread pool */
				{
					auto&& options = config["fiber"]["server"];
					poolWorkers = std::unique_ptr<ThreadPool>(new ThreadPool(
						options.GetConfigValue("pool-workers", "10"), 
						options.GetConfigValue("pool-max", "100"),
						options.GetConfigValue("pool-cores", "1-8"),
						options.GetConfigValue("pool-exclude", "")));
					{
						vector<string> coreList;
						std::for_each(poolWorkers->BindingCores().begin(), poolWorkers->BindingCores().end(), [&coreList](size_t s) {coreList.emplace_back(to_string(s)); });
						log_option("Server.WorkersCount", "%s", options.GetConfigValue("pool-workers", "10"));
						log_option("Server.WorkersLimit", "%s", options.GetConfigValue("pool-max", "100"));
						log_option("Server.WorkersCores", "%s (hardware threads: %ld)", utils::implode(", ", coreList).c_str(), std::thread::hardware_concurrency());
					}
				}

				log_option("SAPI modules directory", "%s", sapi.GetConfigValue("modules_dir", "./"));
				for (auto it : sapi.GetSections()) {
					log_option(string("sapi." + it + ".class").c_str(),"%s",sapi.GetConfigValue(string(it+".class").c_str(),"*INVALID"));
				}
			}
		}
		catch (exception& ex) {
			log_print("[ %9s:%s ] %s","EXCEPTION","STARTUP",ex.what());
			throw;
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
			backtrace_symbols_fd(array, size, STDOUT_FILENO);
			exit(-1);
		}, SIGSEGV);

		netServer->Listen([&poolWorkers](shared_ptr<Server::CHandler> handler) {
			poolWorkers->Enqueue([](shared_ptr<Server::CHandler> handler) {
				try {
					handler->Process();
					handler->Reply(200, nullptr, (const uint8_t*)"test\n", 5, { {"XSample-HEADER","909"} });
				}
				catch (RuntimeException& ex) {
					log_print("[ Server::Listen::RuntimeException ] %s", ex.what());
				}
				catch (std::exception& ex) {
					log_print("[ Server::Listen::StdException ] %s", ex.what());
				}
			}, handler);
		}, 5000);
	}
}