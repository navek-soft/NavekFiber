#include <cstdio>
#define DOM_TRACE
#include "main/kernel.h"
#include <csignal>
#include <chrono>

#include "main/kernel.h"

using namespace std::chrono_literals;


int main(int argc,char* argv[]) { 

	Fiber::Kernel().Run(argc,argv);
	return 0;
	/*
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
	*/
}
