#include <cstdio>
#define DOM_TRACE
#include "main/kernel.h"
#include <csignal>
#include <chrono>

#include "net/server.h"

using namespace std::chrono_literals;

static sig_atomic_t raise_signal = 0;

int main(int argc,char* argv[]) { 

	try {

		auto shandler = [](int sig) {
			extern sig_atomic_t raise_signal;
			raise_signal = sig;
		};
		struct sigaction s_handler = {[](int sig) {
			extern sig_atomic_t raise_signal;
			raise_signal = sig;
		},0,0 };
		::sigaction(SIGINT, &s_handler, nullptr);
		::signal(SIGINT, shandler);
		::signal(SIGQUIT, shandler);
		::signal(SIGTERM, shandler);
		::signal(SIGSEGV, shandler);
		::signal(SIGUSR1, shandler);
		::signal(SIGUSR2, shandler);
	}
	catch (std::exception& ex) {
		printf("%s\n",ex.what());
	}

	Fiber::Server server({ { "pool-workers","10" }, { "pool-cores","2-12" }, { "pool-exclude","3,4" } });
	
	server.AddListener("http", ":8080", {});
	server.AddListener("ws", "eth1:8081", {});

	while (server.Listen() && !raise_signal) {
	}

	return 0;

	Fiber::Kernel core;

	core.Run(argc, argv);

	Dom::Interface<IConfig> cfg(core);
	//core.QueryInterface(IConfig::guid(), (void**)cfg);
	
	printf("Test: %s\n", cfg->GetProgramDir());
	return 0; 
}
