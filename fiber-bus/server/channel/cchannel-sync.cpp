#include "../pch.h"
#include "cchannel-sync.h"
#include "../capp.h"
using namespace fiber;

static inline auto& msg_status(cchannel_sync::message& msg) { return std::get<0>(msg); }
static inline auto& msg_ctime(cchannel_sync::message& msg) { return std::get<1>(msg); }
static inline auto& msg_mtime(cchannel_sync::message& msg) { return std::get<2>(msg); }
static inline auto& msg_request(cchannel_sync::message& msg) { return std::get<3>(msg); }

cchannel_sync::cchannel_sync(const core::coptions& options) :
	queueLimitCapacity(options.at("queue-capacity", "0").number()),
	queueLimitSapiSameTime(options.at("sapi-task-limit", "0").number()),
	queueLimitSapiPeriodTime(options.at("sapi-time-period", "0").number())
{
	//std::string sapiHandler(options.at("sapi-"));
}

cchannel_sync::~cchannel_sync() {

}

void cchannel_sync::processing(const cmsgid& msg_id, const std::string& uri, const std::shared_ptr<fiber::crequest>& msg)
{
	dispatch(std::move(msg_id), std::move(uri), msg);
	/*
	capp::broker().threadpool().enqueue([this](cmsgid msg_id, std::string uri, std::shared_ptr<fiber::crequest> request) {
		HandleRequest(std::move(msg_id), std::move(uri), request);
	}, msg_id, uri, msg);
	*/
}

void cchannel_sync::OnGET(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	//auto&& headers = request->request_headers();
	fprintf(stdout, "%s\n", __PRETTY_FUNCTION__);
	request->response({}, 200, {}, { { "X-FIBER-MSG-QUEUE","ASYNC-QUEUE" } });
	request->disconnect();
}

void cchannel_sync::OnHEAD(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	fprintf(stdout, "%s\n", __PRETTY_FUNCTION__);
	request->response({}, 200, {}, { { "X-FIBER-MSG-QUEUE","ASYNC-QUEUE" } });
	request->disconnect();
}

void cchannel_sync::OnPOST(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	fprintf(stdout, "%s\n\t%s\n", __PRETTY_FUNCTION__, request->request_paload().at(0).str().c_str());
	fflush(stdout);
	request->response({}, 200, {}, { { "X-FIBER-MSG-QUEUE","SYNC-QUEUE" } });
	request->disconnect();
}
