#include "../pch.h"
#include "cchannel-queue.h"
#include "../capp.h"
using namespace fiber;


static inline auto& msg_status(cchannel_queue::message& msg) { return std::get<0>(msg); }
static inline auto& msg_ctime(cchannel_queue::message& msg) { return std::get<1>(msg); }
static inline auto& msg_mtime(cchannel_queue::message& msg) { return std::get<2>(msg); }
static inline auto& msg_request(cchannel_queue::message& msg) { return std::get<3>(msg); }
static inline auto& msg_response(cchannel_queue::message& msg) { return std::get<4>(msg); }

cchannel_queue::cchannel_queue(const core::coptions& options) :
	queueLimitCapacity(options.at("queue-capacity","0").number()),
	queueLimitSapiSameTime(options.at("sapi-task-limit", "0").number()),
	queueLimitSapiPeriodTime(options.at("sapi-time-period", "0").number())
{
	//std::string sapiHandler(options.at("sapi-"));
}

cchannel_queue::~cchannel_queue() {

}

void cchannel_queue::processing(const cmsgid& msg_id,const std::string& uri, const std::shared_ptr<fiber::crequest>& msg)
{
	//dispatch(std::move(msg_id), std::move(uri), msg);
	capp::threadpool().enqueue([this](cmsgid msg_id, std::string uri, std::shared_ptr<fiber::crequest> request) {
		dispatch(std::move(msg_id), std::move(uri), request);
	}, msg_id, uri, msg);
}

void cchannel_queue::OnGET(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	auto&& headers = request->request_headers();
	auto&& req_msg = msgPool.end();
	{
		std::shared_lock<std::shared_mutex> lock(msgSync);
		if (auto&& it_id = headers.find({ "x-fiber-msg-id",14 }); it_id != headers.end()) {
			cmsgid req_msg_id(it_id->second.trim());
			if (req_msg = msgPool.find(req_msg_id); req_msg == msgPool.end()) {
				request->response({}, 404, "Message not found", { { "X-FIBER-MSG-QUEUE","ASYNC-QUEUE" } });
				request->disconnect();
				return;
			}
			if (msg_status(req_msg->second) & crequest::status::complete) {
				auto&& response = msg_response(req_msg->second);
				request->response(response->request_paload(), response->request_paload_length(), 200, {},
					make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), "ASYNC-QUEUE", {}, response->request_headers()));
				msgSync.unlock_shared();
				msgSync.lock();
				msgPool.erase(req_msg);
				msgSync.unlock();
				msgSync.lock_shared();
				request->disconnect();
				return;
			}
		}
		else if (msgQueue.empty()){
			request->response({}, 204, "Queue is empty", { { "X-FIBER-MSG-QUEUE","ASYNC-QUEUE" } });
			request->disconnect();
			return;
		}
		else {
			msg_status(req_msg->second) |= crequest::status::pushed;
			msgSync.unlock_shared();
			msgSync.lock();
			req_msg = msgPool.find(msgQueue.front());
			msgQueue.pop();
			msgSync.unlock();
			msgSync.lock_shared();
		}

		if (req_msg != msgPool.end()) {
			msg_mtime(req_msg->second) = std::time(nullptr);

			request->response(msg_request(req_msg->second)->request_paload(), msg_request(req_msg->second)->request_paload_length(), 200, {}, 
				make_headers(req_msg->first, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), "ASYNC-QUEUE", {}, headers));
			request->disconnect();
		}
		else {
			request->response({}, 404, "Message not found", { { "X-FIBER-MSG-QUEUE","ASYNC-QUEUE" } });
			request->disconnect();
		}
	}
}

void cchannel_queue::OnHEAD(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	auto&& headers = request->request_headers();
	if (auto&& it_id = headers.find({ "x-fiber-msg-id",14 }); it_id != headers.end()) {
		cmsgid req_msg_id(it_id->second.trim());
		
		std::shared_lock<std::shared_mutex> lock(msgSync);

		if (auto&& req_msg = msgPool.find(req_msg_id); req_msg != msgPool.end()) {
			tm ctm, mtm;
			localtime_r(&msg_ctime(req_msg->second), &ctm);
			localtime_r(&msg_mtime(req_msg->second), &mtm);
			request->response({}, 200, {}, 
				make_headers(req_msg->first, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), "ASYNC-QUEUE"));
			request->disconnect();
		}
		else {
			request->response({}, 404, "Message not found", { { "X-FIBER-MSG-QUEUE","ASYNC-QUEUE" } });
			request->disconnect();
		}
	}
	else {
		std::shared_lock<std::shared_mutex> lock(msgSync);
		if (!msgQueue.empty()) {
			if (auto&& req_msg = msgPool.find(msgQueue.front()); req_msg != msgPool.end()) {
				if (msg_status(req_msg->second) != crequest::complete) {
					request->response({}, 200, {}, 
						make_headers(req_msg->first, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), "ASYNC-QUEUE"));
					request->disconnect();
					return;
				}
			}
		}
		request->response({}, 204, "Queue is empty", { { "X-FIBER-MSG-QUEUE","ASYNC-QUEUE" } });
		request->disconnect();
	}
}

void cchannel_queue::OnPUT(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {

	{
		std::unique_lock<std::shared_mutex> lock(msgSync);
		if (!queueLimitCapacity || (queueLimitCapacity < msgPool.size())) {
			if (msgPool.emplace(msg_id, message(crequest::status::accepted | crequest::status::enqueued, std::time(nullptr), std::time(nullptr), request, {})).second) {
				msgQueue.emplace(msg_id);
				request->response({}, 201, "Enqueued", { { "X-FIBER-MSG-ID",msg_id.str() },{ "X-FIBER-MSG-QUEUE","ASYNC-QUEUE" } });
			}
		}
		else {
			request->response({}, 507, "Queue limit capacity exceed", { { "X-FIBER-QUEUE-LIMIT",std::to_string(queueLimitCapacity) },{ "X-FIBER-MSG-QUEUE","ASYNC-QUEUE" } });
		}
	}
	request->disconnect();
}


void cchannel_queue::OnPOST(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	auto&& headers = request->request_headers();
	if (auto&& it_id = headers.find({ "x-fiber-msg-id",14 }); it_id != headers.end()) {
		cmsgid req_msg_id(it_id->second.trim());

		std::shared_lock<std::shared_mutex> lock(msgSync);

		if (auto&& req_msg = msgPool.find(req_msg_id); req_msg != msgPool.end()) {
			msg_mtime(req_msg->second) = std::time(nullptr);

			if (auto&& it_callback = msg_request(req_msg->second)->request_headers().find({ "x-fiber-callback",16 }); it_callback != headers.end()) {
				if (external_post(it_callback->second.trim().str(), request->request_paload(), request->request_paload_length(), 
					make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), "ASYNC-QUEUE", {}, headers)) == 200) {
					request->response({}, 200, {},
						make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), "ASYNC-QUEUE"));
					msgSync.unlock_shared();
					msgSync.lock();
					msgPool.erase(req_msg);
					msgSync.unlock();
					msgSync.lock_shared();
				}
				else {
					msg_response(req_msg->second) = request;
					msg_status(req_msg->second) |= crequest::status::pushed;
					request->response({}, 202, {}, make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), "ASYNC-QUEUE"));
				}
			}
			else {
				msg_response(req_msg->second) = request;
				msg_status(req_msg->second) |= crequest::status::complete;
				request->response({}, 202, {}, make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), "ASYNC-QUEUE"));
			}
		}
		else {
			request->response({}, 404, "Message not found", { { "X-FIBER-MSG-QUEUE","ASYNC-QUEUE" } });
		}
	}
	else {
		request->response({}, 449, "Header `X-FIBER-MSG-ID` are required", { { "X-FIBER-MSG-QUEUE","ASYNC-QUEUE" } });
	}
	request->disconnect();
}