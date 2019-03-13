#pragma once
#include "impl/cfgimpl.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <trace.h>
#include <regex>

static inline bool parseUri(const std::string& uri, std::string& mq, std::string& route) {
	std::regex re(R"(([\w-]+):/(.+))");
	std::smatch match;
	if (std::regex_search(uri, match, re) && match.size() > 1) {
		mq = match.str(1);
		route = match.str(2);
		return true;
	}
	return false;
}

static inline size_t initChannels(const Fiber::ConfigImpl& options, 
	std::unordered_map<std::string, std::tuple<std::string,std::string, std::string,std::shared_ptr<Fiber::ConfigImpl>>>& Channels,
	const std::unordered_map<std::string, std::tuple<std::string, std::shared_ptr<Fiber::ConfigImpl>>>& ClassesMq,
	const std::unordered_map<std::string, std::tuple<std::string, std::shared_ptr<Fiber::ConfigImpl>>>& ClassesSAPI) {
	auto numChannels = 0;
	for (auto&& ch : options.GetSections()) {
		
		auto&& ch_route = std::string(options[ch.c_str()].GetConfigValue("channel-endpoint"));
		std::string ch_proto, ch_uri, sapi_proto, sapi_uri;
		if (!ch_route.empty() && parseUri(ch_route, ch_proto, ch_uri)) {
			if (ClassesMq.find(ch_proto) != ClassesMq.end()) {
				auto&& ch_sapi = std::string(options[ch.c_str()].GetConfigValue("channel-sapi"));
				if (!ch_sapi.empty()) {
					if (!parseUri(ch_sapi, sapi_proto, sapi_uri) || ClassesSAPI.find(sapi_proto) == ClassesSAPI.end()) {
						log_option(std::string("ch." + ch).c_str(), "<Runtime.Error> Invalid SAPI `%s` module. Skipped.", sapi_proto.c_str());
						continue;
					}
				}
				numChannels++;

				if (ch_uri.back() != '/') ch_uri.append("/");

				Channels.emplace(ch_proto, std::make_tuple(ch_proto,sapi_proto, ch_uri, std::shared_ptr<Fiber::ConfigImpl>(new Fiber::ConfigImpl(options[ch.c_str()]))));

				if (sapi_proto.empty()) {
					log_option(std::string("channel." + ch).c_str(), "%s:/%s", ch_proto.c_str(), ch_uri.c_str());
				}
				else {
					log_option(std::string("channel." + ch).c_str(), "%s:/%s => %s:/%s", ch_proto.c_str(), ch_uri.c_str(), sapi_proto.c_str(), sapi_uri.c_str());
				}
			}
			else {
				log_option(std::string("channel." + ch).c_str(), "<Runtime.Error> MQ `%s` not found. Skipped.", ch_proto.c_str());
			}
		}
		else {
			log_option(std::string("channel." + ch).c_str(), "<Runtime.Error> Invalid channel endpoint. Skipped.");
		}
	}
	return numChannels;
}