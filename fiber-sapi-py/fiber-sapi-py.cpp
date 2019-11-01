#include <cstdio>
#include "core/coption.h"
#include "core/cexcept.h"
#include "sapi/cfpm.h"
#include "py/csapi-python.h"
#include "core/csignal.h"
#include "core/ccmdline.h"
#include "core/cini.h"
#include "server/clog.h"
#include <system_error>

volatile pid_t parent_pid{ 0 };
volatile int signal_no{ 0 };

void ini_fpm(core::cini& ini, core::coptions& options);
void print_startup_config(core::coption& cfg, std::string& prog, core::coptions& fpm);

using namespace fiber;
using namespace std;

int main(int argc, char* argv[])
{
	core::coption cmd_config("fiber.config");
	core::coption cmd_verbose("0");
	std::string prog_name, prog_pwd, prog_cwd;

	core::coptions options_fpm;

	try {
		core::ccmdline::options(argc, argv, {
			{ cmd_config,"config",'c',1 },
			{ cmd_verbose,"verbose",'V',0 },
		}, prog_pwd, prog_cwd, prog_name);

		core::cini ini(cmd_config.get());

		ini_fpm(ini, options_fpm);
	}
	catch (std::string & err) {
		clog::msg(10, "( init:config ) exception: %s\n", err.c_str());
		return (1);
	}
	catch (std::system_error & err) {
		clog::msg(10, "( init:config ) exception: %s\n", err.what());
		return (1);
	}
	catch (core::system_error & err) {
		clog::msg(10, "( init:config ) exception: %s\n", err.what());
		return (1);
	}

	print_startup_config(cmd_config, prog_name, options_fpm);

	core::csignal::action([](int signo, siginfo_t* info, void*) {
		signal_no = info->si_signo;
		printf("fpm-python(%ld): #%d signal recvd\n", getpid(), signal_no);
		sigignore(signal_no);
	}, SIGQUIT,SIGABRT,SIGINT);

	try {
		//auto sapi = std::unique_ptr<fiber::csapi>(new fiber::csapi_python(options));
		//sapi->run();
		fiber::cfpm::run(std::unique_ptr<fiber::csapi>(new fiber::csapi_python(options_fpm)), options_fpm);
	}
	catch (core::system_error & er) {
		return 3;
	}

	//core::cthreadpool esbThreadPool(options.at("threads-count", "1").number(), options.at("threads-max", "1").number());

    return 0;
}

void ini_fpm(core::cini& ini, core::coptions& options) {
	if (auto&& sec = ini.xpath("fpm", "python"); sec) {
		if (sec->option("fpm-sapi-pipe").isset()) {
			auto&& sapi_point = sec->option("fpm-sapi-pipe").split("://", true);
			if (sapi_point.front() != "unix") {
				core::system_error(-1, "invalid sapi type. support only unix://", __PRETTY_FUNCTION__, __LINE__);
			}
			options.emplace("name", "python");
			options.emplace("endpoint", sec->option("fpm-sapi-pipe"));
			options.emplace("proto", sapi_point.front());
			options.emplace("pipe", sapi_point.back());
			options.emplace("num-workers", sec->option("fpm-num-workers", "10"));
			options.emplace("enable-resurrect", sec->option("fpm-enable-resurrect", "0"));
			options.emplace("params-transfer", sec->option("fpm-params-transfers", "stdin"));
			options.emplace("ini-config", sec->option("fpm-ini-config", ""));
		}
		else {
			throw core::system_error(-1, "invalid config file. sapi-pipe notfound", __PRETTY_FUNCTION__, __LINE__);
		}
	}
	else {
		throw core::system_error(-1, "invalid config file. server section not configured properly", __PRETTY_FUNCTION__, __LINE__);
	}
}


void print_startup_config(core::coption& cfg, std::string& prog, core::coptions& fpm)
{
	printf("%30s : %s\n", "program.name", prog.c_str());
	printf("%30s : %s\n", "program.config", cfg.get().c_str());
	
}

