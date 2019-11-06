#include "../pch.h"
#include "cchannel-simple.h"
#include "../capp.h"
#include "../../core/cexcept.h"
#include "../clog.h"


using namespace fiber;

static inline auto& msg_status(cchannel_simple::message& msg) { return std::get<0>(msg); }
static inline auto& msg_ctime(cchannel_simple::message& msg) { return std::get<1>(msg); }
static inline auto& msg_mtime(cchannel_simple::message& msg) { return std::get<2>(msg); }
static inline auto& msg_request(cchannel_simple::message& msg) { return std::get<3>(msg); }
static inline auto& msg_response(cchannel_simple::message& msg) { return std::get<4>(msg); }

cchannel_simple::cchannel_simple(const std::string& name, const core::coptions& options) :
	queueName(name),
	queueLimitCapacity(options.at("channel-limit-capacity", "0").number()),
	queueDurability(options.at("channel-durability", "none").get())
{
}

cchannel_simple::~cchannel_simple() {

}

void cchannel_simple::processing(const cmsgid& msg_id, const std::string& uri, const std::shared_ptr<fiber::crequest>& msg)
{
	if (auto&& pool = capp::threadpool().lock(); pool) {
		pool->enqueue([this](cmsgid msg_id, std::string uri, std::shared_ptr<fiber::crequest> request) {
			dispatch(std::move(msg_id), std::move(uri), request);
		}, msg_id, uri, msg);
	}
}

void cchannel_simple::OnGET(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	auto&& headers = request->request_headers();
	auto&& req_msg = msgPool.end();
	{
		std::unique_lock<std::mutex> lock(msgSync);
		if (auto&& it_id = headers.find(_h("x-fiber-msg-id")); it_id != headers.end()) {
			cmsgid req_msg_id(it_id->second.trim());
			if (req_msg = msgPool.find(req_msg_id); req_msg == msgPool.end()) {
				request->response({}, 404, "Message not found", { DEFAULT_HEADERS_MSG(req_msg_id,msgPool.size()) });
			}
			else if (msg_status(req_msg->second) & crequest::status::complete) {
				/* response is ready */
				auto&& response = msg_response(req_msg->second);
				ssize_t result = 0;
				if ((result = request->response(response->request_paload(), response->request_paload_length(), 200, {},
					make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner, { DEFAULT_HEADERS(msgPool.size()) }, response->request_headers()))) == 200) {
					msgPool.erase(req_msg);
				}
				else {
					clog::err(0, "%s(%s)::get: invalid response sending (%ld)\n", banner.c_str(), queueName.c_str(), result);
				}
			}
			else {
				request->response(ci::cstringformat(), 204, "Request enqued",
					make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner, { DEFAULT_HEADERS(msgPool.size()) }));
			}
			request->disconnect();
			return;
		}
		else if (msgQueue.empty()) {
			request->response({}, 204, "Queue is empty", { { "X-FIBER-MSG-QUEUE","ASYNC-QUEUE" } });
		}
		else {
			msg_status(req_msg->second) |= crequest::status::pushed;
			req_msg = msgPool.find(msgQueue.front());
			msgQueue.pop();
		}

		if (req_msg != msgPool.end()) {
			msg_mtime(req_msg->second) = std::time(nullptr);

			request->response(msg_request(req_msg->second)->request_paload(), msg_request(req_msg->second)->request_paload_length(), 200, {},
				make_headers(req_msg->first, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner, { DEFAULT_HEADERS(msgPool.size()) }, msg_request(req_msg->second)->request_headers()));
		}
		else {
			request->response({}, 404, "Message not found", { DEFAULT_HEADERS(msgPool.size()) });
		}
		request->disconnect();
	}
}

void cchannel_simple::OnHEAD(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	auto&& headers = request->request_headers();
	if (auto&& it_id = headers.find({ "x-fiber-msg-id",14 }); it_id != headers.end()) {
		cmsgid req_msg_id(it_id->second.trim());

		std::unique_lock<std::mutex> lock(msgSync);

		if (auto&& req_msg = msgPool.find(req_msg_id); req_msg != msgPool.end()) {
			tm ctm, mtm;
			localtime_r(&msg_ctime(req_msg->second), &ctm);
			localtime_r(&msg_mtime(req_msg->second), &mtm);
			request->response({}, 200, {},
				make_headers(req_msg->first, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner, { DEFAULT_HEADERS(msgPool.size()) }));
		}
		else {
			request->response({}, 404, "Message not found", { DEFAULT_HEADERS(msgPool.size()) });
		}
	}
	else {
		std::unique_lock<std::mutex> lock(msgSync);
		if (!msgQueue.empty()) {
			if (auto&& req_msg = msgPool.find(msgQueue.front()); req_msg != msgPool.end()) {
				request->response({}, 200, {},
					make_headers(req_msg->first, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner, { DEFAULT_HEADERS(msgPool.size()) }));
			}
		}
		else {
			request->response({}, 204, "Queue is empty", { DEFAULT_HEADERS(msgPool.size()) });
		}
	}
	request->disconnect();
}

void cchannel_simple::OnPUT(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	std::unique_lock<std::mutex> lock(msgSync);
	if (!queueLimitCapacity || (queueLimitCapacity < msgPool.size())) {
		if (msgPool.emplace(msg_id, message(crequest::status::accepted | crequest::status::enqueued, std::time(nullptr), std::time(nullptr), request, {})).second) {
			msgQueue.emplace(msg_id);
			request->response({}, 202, "Enqueued", { DEFAULT_HEADERS_MSG(msg_id,msgPool.size()) });
			request->disconnect();
		}
	}
	else {
		request->response({}, 507, "Queue limit capacity exceed", { { "X-FIBER-QUEUE-LIMIT",std::to_string(queueLimitCapacity) },{ "X-FIBER-MSG-QUEUE","ASYNC-QUEUE" } });
		request->disconnect();
	}
}


void cchannel_simple::OnPOST(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	auto&& headers = request->request_headers();
	if (auto&& it_id = headers.find(_h("x-fiber-msg-id")); it_id != headers.end()) {
		cmsgid req_msg_id(it_id->second.trim());

		std::unique_lock<std::mutex> lock(msgSync);

		if (auto&& req_msg = msgPool.find(req_msg_id); req_msg != msgPool.end()) {
			msg_mtime(req_msg->second) = std::time(nullptr);
			msg_status(req_msg->second) |= crequest::status::complete;

			if (auto&& it_callback = msg_request(req_msg->second)->request_headers().find(_h("x-fiber-channel-callback")); it_callback != headers.end() && !it_callback->second.trim().empty()) {
				if (external_post(it_callback->second.trim().str(), request->request_paload(), request->request_paload_length(),
					make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner,
						{ DEFAULT_HEADERS(msgPool.size()) }, msg_request(req_msg->second)->request_headers())) == 200) {
					request->response({}, 200, {},
						make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner));
					msgPool.erase(req_msg);
				}
				else {
					msg_response(req_msg->second) = request;
					msg_status(req_msg->second) |= crequest::status::answering;
					request->response({}, 202, {}, make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner));
				}
			}
			else {
				msg_response(req_msg->second) = request;
				msg_status(req_msg->second) |= crequest::status::complete | crequest::status::answering;
				request->response({}, 202, {}, make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner));
			}
		}
		else {
			request->response({}, 404, "Message not found", { DEFAULT_HEADERS_MSG(req_msg_id,msgPool.size()) });
		}
	}
	else {
		request->response({}, 449, "Header `X-FIBER-MSG-ID` are required", { DEFAULT_HEADERS(msgPool.size()) });
	}
	request->disconnect();
}