#pragma once
#include "impl/cfgimpl.h"
#include "impl/routerimpl.h"
#include <trace.h>

static inline size_t initRoutes(Fiber::RouterImpl& router, Dom::Client::Manager<IKernel, ISerialize>& kernel,
	std::unordered_map<std::string, std::tuple<std::string, std::string, std::string, std::shared_ptr<Fiber::ConfigImpl>>>& Channels,
	std::unordered_map<std::string, std::tuple<std::string, std::shared_ptr<Fiber::ConfigImpl>>>& ClassesMq) {
	auto numChannels = 0;
	for (auto&& ch : Channels) {
		auto&& mq = ClassesMq.at(std::get<0>(ch.second));
		if (router.AddRoute(std::get<2>(ch.second), std::get<0>(mq), ch.first, std::get<3>(ch.second).get(), Dom::Interface<IKernel>(&kernel))) {
			numChannels++;
			log_option("route.endpoint", "%s", std::get<2>(ch.second).c_str());
		}
		else {
			log_option("route.endpoint", "<Runtime.Error> Invalid `%s` initialization. Skipped.", std::get<2>(ch.second).c_str());
		}
	}
	return numChannels;
}