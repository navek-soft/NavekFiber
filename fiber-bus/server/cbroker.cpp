#include "cbroker.h"
#include "clog.h"

using namespace fiber;

cbroker::cbroker(const core::coptions& options) 
{
}

cbroker::~cbroker() {
//	workerPool.join();
	/*
	workerDoIt = false;
	for (auto&& worker : listEndpoints) {
		if (worker.second.second.joinable()) {
			worker.second.second.join();
		}
	}
	*/
}

bool cbroker::emplace(const std::string& endpoint, std::shared_ptr<cchannel>&& channel) {
	return listEndpoints.emplace(endpoint.back() == '/' ? endpoint : (endpoint + "/"), channel) != listEndpoints.end();
}

bool cbroker::enqueue(const std::shared_ptr<fiber::crequest>& msg) {
	auto uri = msg->request_uri().str();
	if (auto&& que = listEndpoints.find(uri); que != listEndpoints.end()) {
		que->second->processing(cmsgid::gen(),std::string(uri.begin() + que->first.length(), uri.end()),msg);
		return true;
	}
	clog::err(1, "invalid endpoint `%s`\n", uri.c_str());
	msg->response({}, 404, "Endpoint not found");
	return false;
}