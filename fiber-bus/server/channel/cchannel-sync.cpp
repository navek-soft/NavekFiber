#include "../pch.h"
#include "cchannel-sync.h"
#include "../capp.h"
#include "../../core/cexcept.h"
#include "../clog.h"

using namespace fiber;

static inline auto& msg_status(cchannel_sync::message& msg) { return std::get<0>(msg); }
static inline auto& msg_ctime(cchannel_sync::message& msg) { return std::get<1>(msg); }
static inline auto& msg_mtime(cchannel_sync::message& msg) { return std::get<2>(msg); }
static inline auto& msg_request(cchannel_sync::message& msg) { return std::get<3>(msg); }

cchannel_sync::cchannel_sync(const std::string& name, const core::coptions& options) :
	queueName(name),
	queueLimitCapacity(options.at("channel-limit-capacity", "0").number()),
	queueDurability(options.at("channel-durability", "none").get()),
	sapiExecLimit(options.at("channel-sapi-task-limit", "0").number()),
	sapiExecTimeout(options.at("channel-sapi-request-limit", "0").number()),
	sapiExecScript(options.at("channel-sapi", "").dsn())
{
	if (sapiExecScript.proto.empty()) {
		throw core::system_error(-1, std::string("Sync channel `").append(name).append("` must be declared with sapi handler options"),__PRETTY_FUNCTION__,__LINE__);
	}
}

cchannel_sync::~cchannel_sync() {

}

void cchannel_sync::processing(const cmsgid& msg_id, const std::string& uri, const std::shared_ptr<fiber::crequest>& msg)
{
	if (auto&& pool = capp::threadpool().lock(); pool) {
		pool->enqueue([this](cmsgid msg_id, std::string uri, std::shared_ptr<fiber::crequest> request) {
			dispatch(std::move(msg_id), std::move(uri), request);
		}, msg_id, uri, msg);
	}
}

void cchannel_sync::OnHEAD(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	std::size_t size = 0;
	{
		std::shared_lock<std::shared_mutex> lock(msgSync);
		if (!msgPool.empty()) {
			size = msgPool.size();
		}
	}
	if (size) {
		request->response({}, 200, "OK", { DEFAULT_HEADERS(size) });
	}
	else {
		request->response({}, 204, "Queue is empty", { DEFAULT_HEADERS(0) });
	}
	request->disconnect();
}

void cchannel_sync::OnPUT(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {

	/* emplace new msg request */
	std::unique_lock<std::shared_mutex> lock(msgSync);
	if (!queueLimitCapacity || (queueLimitCapacity < msgPool.size())) {
		if (msgPool.emplace(msg_id, message(crequest::status::accepted | crequest::status::enqueued | crequest::status::pushed, std::time(nullptr), std::time(nullptr), request, {})).second) {

			auto result = fiber::capp::execute(msg_id, sapiExecScript.proto, "/" + sapiExecScript.fullpathname, sapiExecLimit, sapiExecTimeout, std::move(request));

			if (result != 202) {

				/* invalid sapi execution */

				msgPool.erase(msg_id);
				request->response({}, result, "", { DEFAULT_HEADERS_MSG(msg_id,msgPool.size()) });
				request->disconnect();
			}
		}
	}
	else {
		request->response({}, 507, "Queue limit capacity exceed", { DEFAULT_HEADERS(msgPool.size()) });
		request->disconnect();
	}
}


void cchannel_sync::OnPOST(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {

	/* sapi response for queued msg */

	auto&& headers = request->request_headers();

	if (auto&& it_id = headers.find(_h("x-fiber-msg-id")); it_id != headers.end()) {
		cmsgid req_msg_id(it_id->second.trim());

		std::shared_lock<std::shared_mutex> lock(msgSync);

		if (auto&& req_msg = msgPool.find(req_msg_id); req_msg != msgPool.end()) {
			
			msg_mtime(req_msg->second) = std::time(nullptr);

			msg_status(req_msg->second) |= crequest::status::complete;


			auto result = msg_request(req_msg->second)->response(request->request_paload(), request->request_paload_length(), 200, "", 
				make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second),banner, {
					DEFAULT_HEADERS(msgPool.size() - 1)
				}));

			msg_request(req_msg->second)->disconnect();

			if (result != 200) {
				clog::err(0, "%s(%s)::post: invalid response sending (%ld)\n", banner.c_str(), queueName.c_str(), result);
			}

			/* skip response error code */

			msgPool.erase(req_msg);

		}
		else {
			request->response({}, 404, "Message not found", { DEFAULT_HEADERS(msgPool.size()) });
		}
	}
	else {
		request->response({}, 449, "Header `X-FIBER-MSG-ID` are required", { DEFAULT_HEADERS(msgPool.size()) });
	}
	request->disconnect();
}