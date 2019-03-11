#pragma once
#include "impl/cfgimpl.h"
#include "sys/path.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <trace.h>

static inline size_t initSAPI(const Fiber::ConfigImpl& options, Dom::Client::Manager<IKernel>& fiberRegistry, std::unordered_map<std::string, std::unique_ptr<const Fiber::ConfigImpl>>& ClassesSAPI) {
	std::string sapiPath(options.GetConfigValue("modules-dir", options.GetProgramDir()));
	auto numSAPI = 0;
	for (auto&& sapi : options.GetSections()) {
		auto&& sapi_class = std::string(options[sapi.c_str()].GetConfigValue("class"));
		auto&& sapi_module = std::string(options[sapi.c_str()].GetConfigValue("module"));

		try {
			if (!sapi_class.empty() && !sapi_module.empty() && fiberRegistry.EmplaceServer(sapiPath + sapi_module, "SAPI")) {
				numSAPI++;
				ClassesSAPI.emplace(sapi_class, std::unique_ptr<const Fiber::ConfigImpl>(new Fiber::ConfigImpl(options[sapi.c_str()])));
				log_option(std::string("sapi." + sapi).c_str(), "Class `%s` (%s) loaded", sapi_class.c_str(), std::string(sapiPath + sapi_module).c_str());
				continue;
			}
			else
				throw;
		}
		catch (...) {
			log_option(std::string("sapi." + sapi).c_str(), "<Runtime.Error> Class `%s` (%s) not loaded",
				sapi_class.empty() ? "`INVALID CLASS NAME`" : sapi_class.c_str(),
				sapi_module.empty() ? "`IVALID MODULE PATHNAME`" : std::string(sapiPath + sapi_module).c_str());
		}

	}
	return numSAPI;
}