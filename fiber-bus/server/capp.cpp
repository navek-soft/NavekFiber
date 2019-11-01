#include "pch.h"
#include "capp.h"
#include "chttpserver.h"
#include "../core/cexcept.h"
#include "../core/coption.h"
#include "../core/ccmdline.h"
#include "../core/cini.h"
#include "channel/cchannel-queue.h"
#include "channel/cchannel-sync.h"
#include "csapiserver.h"
#include "clog.h"
#include "../core/cexcept.h"


using namespace fiber;

std::weak_ptr<fiber::cbroker> gEsbBroker;
std::weak_ptr<core::cserver::base> gEsbSapi;
std::weak_ptr<core::cthreadpool> gEsbThreadPool;

extern void ini_server(core::cini& ini, core::coptions& options);
extern void ini_sapi(core::cini& ini, core::coptions& options);
extern void ini_threadpool(core::cini& ini, core::coptions& options);
extern void ini_fpm(core::cini& ini, std::unordered_map<std::string, core::coptions>& options);
extern void ini_channels(core::cini& ini, std::unordered_map<std::string, core::coptions>& options);
void print_startup_config(core::coption& cfg, std::string& prog, core::coptions& server, core::coptions& sapi, core::coptions& threadpool, std::unordered_map<std::string, core::coptions>& fpm, std::unordered_map<std::string, core::coptions>& channels);

int capp::run(int argc, char* argv[])
{
	core::coption cmd_config("fiber.config");
	core::coption cmd_verbose("0");
	
	core::coptions options_server, options_sapi, options_threadpool;
	std::unordered_map<std::string, core::coptions> options_fpm, options_channels;

	try {
		core::ccmdline::options(argc, argv, {
			{ cmd_config,"config",'c',1 },
			{ cmd_verbose,"verbose",'V',0 },
		}, prog_pwd, prog_cwd, prog_name);

		core::cini ini(cmd_config.get());

		ini_server(ini, options_server);
		ini_sapi(ini, options_sapi);
		ini_threadpool(ini, options_threadpool);
		ini_fpm(ini, options_fpm);
		ini_channels(ini, options_channels);
	}
	catch (std::string& err) {
		clog::msg(10, "( init:config ) exception: %s\n", err.c_str());
		return (1);
	}
	catch (std::system_error& err) {
		clog::msg(10, "( init:config ) exception: %s\n", err.what());
		return (1);
	}
	catch (core::system_error & err) {
		clog::msg(10, "( init:config ) exception: %s\n", err.what());
		return (1);
	}

	print_startup_config(cmd_config, prog_name, options_server, options_sapi, options_threadpool, options_fpm, options_channels);

	std::shared_ptr<core::cthreadpool> esbThreadPool(new core::cthreadpool(options_threadpool.at("thread-workers").number(), options_threadpool.at("threads-max", "50").number()));
	gEsbThreadPool = esbThreadPool;

	{
		std::shared_ptr<fiber::cbroker> esbBroker(new fiber::cbroker(options_server));
		std::shared_ptr<core::cserver::base> sapiServer;
		core::cserver esbServer;

		/* initialize server */
		try {
			/* initialize channels */
			for (auto&& ch : options_channels) {
				auto&& channel_type = ch.second.at("channel-endpoint-type").get();
				if (channel_type == "async") {
					esbBroker->emplace(ch.second.at("channel-endpoint-url").get(), std::shared_ptr<fiber::cchannel>(new fiber::cchannel_queue(ch.first, ch.second)));
				}
			}
			/* initialize sapi server */
			if (!options_sapi.at("sapi-pipe").get().empty()) {
				sapiServer = std::shared_ptr<core::cserver::base>(new fiber::csapiserver(core::coptions(options_sapi)));
				esbServer.emplace(sapiServer);
			}

			esbServer.emplace(std::shared_ptr<core::cserver::base>(new fiber::chttpserver(options_server)));

		} catch (core::system_error & err) {
			clog::err(10, "( server:initialize) exception: %s\n", err.what());
			return (2);
		}

		gEsbBroker = esbBroker;
		gEsbSapi = sapiServer;

		try {
			while (esbServer.listen(-1) >= 0) {
				;
			}

			esbServer.shutdown();
			esbBroker.reset();
			sapiServer.reset();

			esbThreadPool->join();

			gEsbThreadPool.reset();
			gEsbBroker.reset();
			gEsbSapi.reset();

			return 0;
		}
		catch (...) {
			clog::err(10, "( server:run) unhandled exception.\n");
			return (2);
		}

		/*options.emplace("channel-sapi", "python://home/irokez/projects/NavekFiber/fiber-sapi-py/sample-python-test.py");
		options.emplace("channel-sapi-task-limit", "2");
		options.emplace("channel-sapi-request-limit", "0");

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
				*/
	}
	return 2;
}

void capp::dispatch(const std::shared_ptr<fiber::crequest>& msg) {
	std::weak_ptr<fiber::cbroker> lock(gEsbBroker);
	if (auto&& ptr = gEsbBroker.lock(); ptr) { ptr->enqueue(msg); }
}

ssize_t capp::execute(const cmsgid& msg_id, const std::string& sapi, const std::string& execscript, std::size_t task_limit, std::size_t task_timeout, const std::shared_ptr<fiber::crequest>&& msg) {
	std::weak_ptr<core::cserver::base> lock(gEsbSapi);
	if (auto ptr = lock.lock(); ptr) {
		return ((fiber::csapiserver*)ptr.get())->execute(sapi, execscript, task_limit, task_timeout, msg_id, msg);
	}
	return 0;
}

std::weak_ptr<core::cthreadpool> capp::threadpool() {
	return gEsbThreadPool;
}


void print_startup_config(core::coption& cfg, std::string& prog, core::coptions& server, core::coptions& sapi, core::coptions& threadpool, std::unordered_map<std::string, core::coptions>& fpm, std::unordered_map<std::string, core::coptions>& channels)
{
	printf("%30s : %s\n", "program.name", prog.c_str());
	printf("%30s : %s\n", "program.config", cfg.get().c_str());
	{
		auto&& hostport = server.at("listen").host();
		printf("%30s : %s, port: %s\n", "server.listen", hostport.host.empty() ? "*" : hostport.host.c_str(), hostport.port.c_str());
		printf("%30s : %ld bytes\n", "server.query.limit", server.at("query-limit").bytes());
		printf("%30s : %ld bytes\n", "server.query.header.limit", server.at("header-limit").bytes());
	}
	printf("%30s : %ld\n", "server.threads.count", threadpool.at("thread-workers").number());
	printf("%30s : %s\n", "server.sapi", sapi.at("sapi-pipe").get().empty() ? "disabled" : sapi.at("sapi-pipe").get().c_str());

	for (auto&& f : fpm) {
		printf("%30s : %s\n", std::string("fpm.").append(f.first).c_str(), "enabled");
	}

	for (auto&& ch : channels) {
		printf("%30s : %s\n", std::string("channel.").append(ch.first).c_str(), "enabled");
		printf("%30s   %s: %s\n", " ", "endpoint", ch.second.at("channel-endpoint").get().c_str());
		printf("%30s   %s: %s\n", " ", "durability", ch.second.at("channel-durability").get().c_str());
		printf("%30s   %s: %s\n", " ", "capacity", ch.second.at("channel-limit-capacity").number() ? ch.second.at("channel-limit-capacity").get().c_str() : "unlimit");
		{
			auto handler = ch.second.at("channel-sapi").get();
			if (!handler.empty()) {
				auto&& limit = ch.second.at("channel-sapi-task-limit");
				auto&& timeout = ch.second.at("channel-sapi-request-limit");
				printf("%30s   %s: sapi(%s), request.limit(%s), request.timeout(%s)\n", " ", "handler", handler.c_str(), limit.number() ? limit.get().c_str() : "no", timeout.number() ? timeout.get().c_str() : "no");
			}
			else {
				printf("%30s   %s: %s\n", " ", "handler", "queue");
			}
		}
	}
}

