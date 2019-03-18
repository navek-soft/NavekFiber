#pragma once
#include "impl/cfgimpl.h"
#include "sys/path.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <trace.h>

static inline size_t initMQ(const Fiber::ConfigImpl& options, 
	Dom::Client::Manager<IKernel,ISerialize>& fiberRegistry,
	std::unordered_map<std::string, std::tuple<std::string, std::shared_ptr<Fiber::ConfigImpl>>>& ClassesMq) {
	std::string mqPath(options.GetConfigValue("modules-dir", options.GetProgramDir()));
	auto numMQ = 0;
	for (auto&& mq : options.GetSections()) {
		auto&& mq_class = std::string(options[mq.c_str()].GetConfigValue("class"));
		auto&& mq_module = std::string(options[mq.c_str()].GetConfigValue("module"));
		try {
			if (!mq_class.empty() && !mq_module.empty() && fiberRegistry.EmplaceServer(mqPath + mq_module, "MQ")) {
				numMQ++;
				ClassesMq.emplace(mq, std::make_tuple(mq_class,std::shared_ptr<Fiber::ConfigImpl>(new Fiber::ConfigImpl(options[mq.c_str()]))));
				log_option(std::string("mq." + mq).c_str(), "Class `%s` (%s) loaded", mq_class.c_str(), std::string(mqPath + mq_module).c_str());

				continue;
			}
			else 
				throw;
		}
		catch (...) {
			log_option(std::string("mq." + mq).c_str(), "<Runtime.Error> Class `%s` (%s) not loaded",
				mq_class.empty() ? "`INVALID CLASS NAME`" : mq_class.c_str(),
				mq_module.empty() ? "`IVALID MODULE PATHNAME`" : std::string(mqPath + mq_module).c_str());
		}
	}
	return numMQ;
}