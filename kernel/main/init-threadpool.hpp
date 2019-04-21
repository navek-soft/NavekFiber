#pragma once
#include "impl/cfgimpl.h"
#include "../mt/threadpool.h"

static inline void initThreadPool(const Fiber::ConfigImpl& options, unique_ptr<Fiber::ThreadPool>& fiberWorkers) {
	fiberWorkers = std::unique_ptr<Fiber::ThreadPool>(new Fiber::ThreadPool(
		options.GetConfigValue("thread-workers", "10"),
		options.GetConfigValue("thread-limit", "100"),
		options.GetConfigValue("core-bind", "1-8"),
		options.GetConfigValue("core-exclude", "")));
	{
		vector<string> coreList;
		std::for_each(fiberWorkers->BindingCores().begin(), fiberWorkers->BindingCores().end(), [&coreList](size_t s) {coreList.emplace_back(to_string(s)); });
		log_option("thread-pool.workers", "%s", options.GetConfigValue("pool-workers", "10"));
		log_option("thread-pool.limit", "%s", options.GetConfigValue("pool-max", "100"));
		log_option("thread-pool.cores", "%s (hardware threads: %ld)", utils::implode(", ", coreList).c_str(), std::thread::hardware_concurrency());
	}
}