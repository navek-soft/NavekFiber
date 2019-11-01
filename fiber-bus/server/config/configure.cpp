#include "../../core/cini.h"
#include "../../core/coption.h"
#include "../../core/cexcept.h"
#include "../clog.h"

void ini_server(core::cini& ini, core::coptions& options) {
	if (auto&& sec = ini.xpath("server", "restful"); sec) {
		auto&& hostport = sec->option("listen", "8080").host();
		options.emplace("listen", sec->option("listen", "8080"));
		options.emplace("port", hostport.port);
		options.emplace("ip", hostport.host);
		options.emplace("iface", "");
		options.emplace("query-limit", sec->option("query-limit", "1M"));
		options.emplace("header-limit", sec->option("header-limit", "4K"));
	}
	else {
		throw core::system_error(-1, "invalid config file. server section not configured properly", __PRETTY_FUNCTION__, __LINE__);
	}
}

void ini_sapi(core::cini& ini, core::coptions& options) {
	if (auto&& sec = ini.xpath("server", "sapi"); sec) {

		auto&& sapi_point = sec->option("sapi-pipe").split("://", true);
		if (sapi_point.front() != "unix") {
			core::system_error(-1, "invalid sapi type. support only unix://", __PRETTY_FUNCTION__, __LINE__);
		}
		options.emplace("sapi-pipe", sec->option("sapi-pipe", ""));
		options.emplace("sapi-engines", sec->option("sapi-engines", ""));
		if (sapi_point.size() == 2) {
			options.emplace("sapi-pipe-type", sapi_point.front());
			options.emplace("sapi-pipe-path", sapi_point.back());
		}
	}
	else {
		options.emplace("sapi-pipe", "");
	}
}

void ini_threadpool(core::cini& ini, core::coptions& options) {
	if (auto&& sec = ini.xpath("server", "threadpool"); sec) {
		options.emplace("thread-workers", sec->option("thread-workers", "10"));
		options.emplace("core-bind", sec->option("core-bind", ""));
		options.emplace("core-exclude", sec->option("core-exclude", ""));
	}
	else {
		options.emplace("thread-workers", "10");
		options.emplace("core-bind", "");
		options.emplace("core-exclude", "");
	}
}

void ini_fpm(core::cini& ini, std::unordered_map<std::string, core::coptions>& options) {
	if (auto&& sec = ini.xpath("fpm"); sec) {
		for (auto&& s : sec->sections()) {
			if (s.second->option("fpm-sapi-pipe").isset()) {
				auto&& opts = options.emplace(s.first, core::coptions());
				opts.first->second.emplace("fpm-sapi-name", s.first);
				opts.first->second.emplace("fpm-sapi-pipe", s.second->option("fpm-sapi-pipe"));
				opts.first->second.emplace("fpm-num-workers", s.second->option("fpm-num-workers", "10"));
				opts.first->second.emplace("fpm-enable-resurrect", s.second->option("fpm-enable-resurrect", "0"));
				opts.first->second.emplace("fpm-params-transfer", s.second->option("fpm-params-transfer", "stdin"));
				opts.first->second.emplace("fpm-ini-config", s.second->option("fpm-ini-config", ""));
			}
			else {
				fiber::clog::msg(10, "( init:config ) fpm:%s:warning: fpm-sapi-pipe is empty. skip.\n", s.first.c_str());
			}
		}
	}
}

void ini_channels(core::cini& ini, std::unordered_map<std::string, core::coptions>& options) {
	if (auto&& sec = ini.xpath("channels"); sec) {
		for (auto&& s : sec->sections()) {
			if (s.second->option("channel-endpoint").isset()) {
				auto&& channel = s.second->option("channel-endpoint").split("://", true);

				if (channel.size() != 2) {
					fiber::clog::msg(10, "( init:config ) channel:%s:warning: invalid channel-endpoint format. skip channel.\n", s.first.c_str());
					continue;
				}
				auto&& opts = options.emplace(s.first, core::coptions());
				opts.first->second.emplace("channel-name", s.first);
				opts.first->second.emplace("channel-endpoint", s.second->option("channel-endpoint"));
				opts.first->second.emplace("channel-endpoint-type", channel.front());
				opts.first->second.emplace("channel-endpoint-url", std::string("/").append(channel.back()));
				opts.first->second.emplace("channel-durability", s.second->option("channel-durability","none"));
				opts.first->second.emplace("channel-sapi", s.second->option("channel-sapi"));
				opts.first->second.emplace("channel-sapi-task-limit", s.second->option("channel-sapi-task-limit","0"));
				opts.first->second.emplace("channel-sapi-request-limit", s.second->option("channel-sapi-request-limit", "0"));
				opts.first->second.emplace("channel-limit-capacity", s.second->option("channel-limit-capacity", "0"));
			}
			else {
				fiber::clog::msg(10, "( init:config ) channel:%s:warning: invalid channel-endpoint. skip channel.\n",s.first.c_str());
			}
		}
	}
}

