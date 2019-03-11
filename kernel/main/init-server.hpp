#pragma once
#include "impl/cfgimpl.h"
#include "../net/server.h"
#include <trace.h>

static inline size_t initServer(const Fiber::ConfigImpl& options, std::unique_ptr<Fiber::Server>& fiberServer) {
	fiberServer = std::unique_ptr<Fiber::Server>(new Fiber::Server());
	auto numListeners = 0;
	for (auto&& proto : options.GetSections()) {
		auto&& proto_options = options[proto.c_str()];
		auto listen = utils::explode(":", proto_options.GetConfigValue("listen"), 2);
		int err = 0;
		if ((err = fiberServer->AddListener(proto, proto_options.GetConfigValue("listen"), proto_options.GetConfigValue("query-limit"), proto_options.GetConfigValue("header-limit"))) == 0) {
			numListeners++;
			log_option(std::string("server." + proto).c_str(), "Port: %s (Iface: %s) success initialize", 
				listen[1].c_str(), listen[0].empty() ? "any" : listen[0].c_str());
		}
		else {
			log_option(std::string("server." + proto).c_str(), "<Runtime.Error> Port: %s (Iface: %s). # %s (%ld).", 
				listen[1].empty() ? "INVALID PORT" : listen[1].c_str(), listen[0].empty() ? "any" : listen[0].c_str(), strerror((int)err), err);
		}
	}
	return numListeners;
}