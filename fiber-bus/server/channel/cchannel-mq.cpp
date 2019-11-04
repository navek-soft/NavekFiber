#include "../pch.h"
#include "cchannel-mq.h"
#include "../capp.h"
#include "../../core/cexcept.h"
#include "../clog.h"


using namespace fiber;

static inline auto& msg_status(cchannel_mq::message& msg) { return std::get<0>(msg); }
static inline auto& msg_ctime(cchannel_mq::message& msg) { return std::get<1>(msg); }
static inline auto& msg_mtime(cchannel_mq::message& msg) { return std::get<2>(msg); }
static inline auto& msg_request(cchannel_mq::message& msg) { return std::get<3>(msg); }
static inline auto& msg_response(cchannel_mq::message& msg) { return std::get<4>(msg); }

cchannel_mq::cchannel_mq(const std::string& name, const core::coptions& options) :
	queueName(name),
	queueLimitCapacity(options.at("channel-limit-capacity", "0").number()),
	queueLimitSibscribers(options.at("channel-mq-subscribers-limit", "0").number()),
	queueMode(mRoundRobin),
	queueDurability(options.at("channel-durability", "none").get())
{
	if (options.at("channel-mq-mode", "round-robin").get() == "broadcast") {
		queueMode = mBroadcast;
	}
}

cchannel_mq::~cchannel_mq() {

}

void cchannel_mq::processing(const cmsgid& msg_id, const std::string& uri, const std::shared_ptr<fiber::crequest>& msg)
{
	if (auto&& pool = capp::threadpool().lock(); pool) {
		pool->enqueue([this](cmsgid msg_id, std::string uri, std::shared_ptr<fiber::crequest> request) {
			dispatch(std::move(msg_id), std::move(uri), request);
			}, msg_id, uri, msg);
	}
}

void cchannel_mq::OnCONNECT(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	auto&& headers = request->request_headers();

	std::unique_lock<std::shared_mutex> lock(msgSync);

	auto&& subchannel = msgSubscribers.emplace(path);

	if (!queueLimitSibscribers || queueLimitSibscribers < subchannel.first->second.size()) {

		auto&& headers = request->request_headers();

		if (auto&& cb_it = headers.find(_h("x-fiber-channel-callback")); cb_it != headers.end() && !cb_it->second.trim().empty()) {

			subchannel.first->second.emplace(cb_it->second.trim().str());

			request->response({}, 202, "Enqueued", { DEFAULT_HEADERS_MSG(msg_id,msgPool.size()) });
		}
		else {
			request->response({}, 449, "Header `x-fiber-channel-callback` are required", { { "X-FIBER-CHANNEL-LIMIT",std::to_string(queueLimitSibscribers) } ,DEFAULT_HEADERS(msgPool.size()) });
		}
	}
	else {
		request->response({}, 507, "Subscriber connection limit exceed", { { "X-FIBER-CHANNEL-LIMIT",std::to_string(queueLimitSibscribers) } ,DEFAULT_HEADERS(msgPool.size()) });
	}
	request->disconnect();
}

void cchannel_mq::PostQueueMsg(const std::string& subChannelQueue) {

	if (auto&& q_it = msgQueue.find(subChannelQueue); q_it != msgQueue.end() && !q_it->second.empty()) {
		if (auto&& sbs_it = msgSubscribers.find(subChannelQueue); sbs_it != msgSubscribers.end()) {
			
			auto&& msg_id = q_it->second.front();

			if (auto&& msg = msgPool.find(msg_id); msg != msgPool.end()) {

				mode deliveryMode{ queueMode };
				auto&& request = msg_request(msg->second);
				auto&& headers = request->request_headers();

				if (auto&& h_it = headers.find(_h("x-fiber-mq-msg-type")); h_it != headers.end()) {
					auto&& h_mode = h_it->second.str();
					if (h_mode == "broadcast") { deliveryMode = mBroadcast; }
				}

				if (deliveryMode == mRoundRobin) {
					do {
						auto subscriber = sbs_it->second.front();
						sbs_it->second.pop();

						if (external_post(subscriber, request->request_paload(), request->request_paload_length(),
							make_headers(msg_id, msg_ctime(msg->second), msg_mtime(msg->second), msg_status(msg->second), banner,
								{ DEFAULT_HEADERS(msgPool.size()) }, request->request_headers())) == 200) {

							/* message is success processed */

							q_it->second.pop();

							/*
							push subscriber back
							*/
							
							sbs_it->second.emplace(subscriber);
							break;
						}
						else {
							/*
							subscriber are not back of the queue
							*/
							clog::log(1, "%s(%s)::processing(`%s`) subscriber is down (%s).\n", banner.c_str(), queueName.c_str(), subChannelQueue.c_str(), s_it.c_str());
						}

					} while (!sbs_it->second.empty());
				}
				else if (deliveryMode == mBroadcast) {
					std::queue<std::string> subscribers_list;

					do {
						auto subscriber = sbs_it->second.front();
						sbs_it->second.pop();

						if (external_post(subscriber, request->request_paload(), request->request_paload_length(),
							make_headers(msg_id, msg_ctime(msg->second), msg_mtime(msg->second), msg_status(msg->second), banner,
								{ DEFAULT_HEADERS(msgPool.size()) }, request->request_headers())) == 200) {
							subscribers_list.emplace(subscriber);
						}
						else {
							/*
							subscriber are not back of the queue
							*/
							clog::log(1, "%s(%s)::processing(`%s`) subscriber is down (%s).\n", banner.c_str(), queueName.c_str(), subChannelQueue.c_str(), s_it.c_str());
						}

					} while (!sbs_it->second.empty());

					if (!subscribers_list.empty()) {
						/* message is success processed and delivery to all live subscribers */
						q_it->second.pop();
					}
					/* new subscribers list, without leaving */
					sbs_it->second.swap(subscribers_list);
				}
			}
			else {
				clog::log(1, "%s(%s)::processing(`%s`) invalid message(%s).\n", banner.c_str(), queueName.c_str(), subChannelQueue.c_str(), msg->first.str().c_str());
				q_it->second.pop();
			}
		}
		else {
			clog::log(1, "%s(%s)::processing(`%s`) no subscribers found.\n", banner.c_str(), queueName.c_str(), subChannelQueue.c_str());
		}
	}
}

void cchannel_mq::OnPUT(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	{
		std::unique_lock<std::shared_mutex> lock(msgSync);
		if (auto&& path_qit = msgQueue.emplace(path); !queueLimitCapacity || (queueLimitCapacity < path_qit.first->second.size())) {
			if (msgPool.emplace(msg_id, message(crequest::status::accepted | crequest::status::enqueued, std::time(nullptr), std::time(nullptr), request, {})).second) {
				auto&& path_qit = msgQueue.emplace(path);
				//path_qit.first->second.emplace(element(std::time(nullptr), msg_id));
				path_qit.first->second.emplace(msg_id);
				request->response({}, 202, "Enqueued", { { "X-FIBER-CHANNEL-ENQUEUED",std::to_string(path_qit.first->second.size()) }, DEFAULT_HEADERS_MSG(msg_id,msgPool.size()) });
			}
		}
		else {
			request->response({}, 507, "Queue limit capacity exceed", { { "X-FIBER-QUEUE-LIMIT",std::to_string(queueLimitCapacity) }, DEFAULT_HEADERS_MSG(msg_id,msgPool.size()) });
		}
		request->disconnect();
	}
	PostQueueMsg(path);
}


void cchannel_mq::OnPOST(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	auto&& headers = request->request_headers();
	if (auto&& it_id = headers.find(_h("x-fiber-msg-id")); it_id != headers.end()) {
		cmsgid req_msg_id(it_id->second.trim());

		std::shared_lock<std::shared_mutex> lock(msgSync);

		if (auto&& req_msg = msgPool.find(req_msg_id); req_msg != msgPool.end()) {
			msg_mtime(req_msg->second) = std::time(nullptr);
			msg_status(req_msg->second) |= crequest::status::complete;

			if (auto&& it_callback = msg_request(req_msg->second)->request_headers().find(_h("x-fiber-channel-callback")); it_callback != headers.end()) {
				if (external_post(it_callback->second.trim().str(), request->request_paload(), request->request_paload_length(),
					make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner,
						{ DEFAULT_HEADERS(msgPool.size()) }, msg_request(req_msg->second)->request_headers())) == 200) {
					request->response({}, 200, {},
						make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner));
					msgSync.unlock_shared();
					msgSync.lock();
					msgPool.erase(req_msg);
					msgSync.unlock();
					msgSync.lock_shared();
				}
				else {
					request->response({}, 499, {},
						make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner));
				}
			}
			else {
				request->response({}, 200, {},
					make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner));
				msgSync.unlock_shared();
				msgSync.lock();
				msgPool.erase(req_msg);
				msgSync.unlock();
				msgSync.lock_shared();
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