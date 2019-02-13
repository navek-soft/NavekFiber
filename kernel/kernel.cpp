#include <cstdio>
#define DOM_TRACE
#include "main/kernel.h"
#include <csignal>
#include <chrono>

using namespace std::chrono_literals;

static sig_atomic_t raise_signal = 0;

int main(int argc,char* argv[]) { 

	try {

		auto shandler = [](int sig) {
			extern sig_atomic_t raise_signal;
			raise_signal = sig;
		};

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

	Fiber::Kernel core;

	core.Run(argc, argv);

	Dom::Interface<IConfig> cfg(core);
	//core.QueryInterface(IConfig::guid(), (void**)cfg);
	
	printf("Test: %s\n", cfg->GetProgramDir());
	return 0; 
}