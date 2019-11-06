#include "../pch.h"
#include "cchannel-mq.h"
#include "../capp.h"
#include "../../core/cexcept.h"
#include "../clog.h"


using namespace fiber;

static inline auto& msg_ctime(cchannel_mq::message& msg) { return std::get<0>(msg); }
static inline auto& msg_mtime(cchannel_mq::message& msg) { return std::get<1>(msg); }
static inline auto& msg_confirm(cchannel_mq::message& msg) { return std::get<2>(msg); }
static inline auto& msg_request(cchannel_mq::message& msg) { return std::get<3>(msg); }

cchannel_mq::cchannel_mq(const std::string& name, const core::coptions& options) :
	queueName(name),
	queueLimitCapacity(options.at("channel-limit-capacity", "0").number()),
	queueLimitSibscribers(options.at("channel-mq-subscribers-limit", "0").number()),
	queueMqConfirmAttempts(options.at("channel-mq-confirm-attempts", "0").number()),
	queueMqConfirmTimeout(options.at("channel-mq-confirm-timeout", "10s").seconds()),
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

std::weak_ptr<cchannel_mq::channel_queue> cchannel_mq::subchannel(const std::string& channel_uri) {
	std::unique_lock<std::mutex> lock(msgSync);
	if (auto&& ch_it = queueChannels.find(channel_uri); ch_it != queueChannels.end()) {
		return ch_it->second;
	}
	return queueChannels.emplace(channel_uri, std::make_shared<channel_queue>()).first->second;
}

void cchannel_mq::OnCONNECT(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	auto&& headers = request->request_headers();

	if (auto&& cb_it = headers.find(_h("x-fiber-channel-callback")); cb_it != headers.end() && !cb_it->second.trim().empty()) {
		
		if (auto&& channel_ptr = subchannel(path);  auto channel = channel_ptr.lock()) {
			std::unique_lock<std::mutex> lock(channel->guardian);
			if (!queueLimitSibscribers || queueLimitSibscribers < channel->subscribers.size()) {
				channel->subscribers.emplace(cb_it->second.trim().str());
				request->response({}, 202, "Enqueued", { DEFAULT_HEADERS_MSG(msg_id,channel->subscribers.size()) });
			}
			else {
				request->response({}, 507, "Subscriber connection limit exceed", { { "X-FIBER-CHANNEL-LIMIT",std::to_string(queueLimitSibscribers) } ,DEFAULT_HEADERS(channel->subscribers.size()) });
			}
		}
		else {
			request->response({}, 500, "Channel is down", { DEFAULT_HEADERS(0) });
		}
	}
	else {
		request->response({}, 449, "Header `x-fiber-channel-callback` are required", { { "X-FIBER-CHANNEL-LIMIT",std::to_string(queueLimitSibscribers) } ,DEFAULT_HEADERS(0) });
	}
	request->disconnect();

	PostQueueMsg(path);
}

void cchannel_mq::PostQueueMsg(const std::string& subChannelQueue) {

	if (auto&& channel_ptr = subchannel(subChannelQueue);  auto channel = channel_ptr.lock()) {
		std::unordered_map<cmsgid, message>::iterator msg_it;
		do /* process all queue messages */
		{
			{
				/* leave if nothing */
				std::unique_lock<std::mutex> lock(channel->guardian);
				if (channel->queue.empty() || channel->subscribers.empty()) { break; }
				
				msg_it = channel->messages.find(channel->queue.front());
				channel->queue.pop_front();
			}

			if (msg_it != channel->messages.end()) {

				DeliveryMode msgDeliveryMode{ queueMode };
				std::string msgConfirmCallback;
				size_t msgDeliveryCounter = 0;

				auto&& request = msg_request(msg_it->second);
				auto&& headers = request->request_headers();

				if (auto&& h_it = headers.find(_h("x-fiber-mq-msg-delivery")); h_it != headers.end() && (h_it->second.str() == "broadcast")) {
					msgDeliveryMode = mBroadcast;
				}
				if (auto&& h_it = headers.find(_h("x-fiber-mq-msg-confirm")); h_it != headers.end() && (h_it->second.str() == "required")) {
					if (h_it = headers.find(_h("x-fiber-mq-msg-confirm-callback")); h_it != headers.end() && !h_it->second.trim().empty()) {
						msgConfirmCallback = h_it->second.trim().str();
					}
				}
				
				if (msg_confirm(msg_it->second) == -1) {
					std::list<std::string> subscribers_list;
					bool bContinue = true;
					do {
						std::string subscriber;
						{
							std::unique_lock<std::mutex> lock(channel->guardian);
							subscriber = channel->subscribers.front();
							channel->subscribers.pop();
							bContinue = !channel->subscribers.empty();
						}
						ssize_t result = 0;
						if (result = external_post(subscriber, request->request_paload(), request->request_paload_length(),
							make_headers(msg_it->first, msg_ctime(msg_it->second), std::time(nullptr), 0, banner,
								{ DEFAULT_HEADERS(0) }, request->request_headers())); result == 200) {

							++msgDeliveryCounter;

							subscribers_list.emplace_front(subscriber); // reverse insert 

							if (msgDeliveryMode == mRoundRobin) {
								bContinue = false;
							}
						}
						else if (result > 0) {
							subscribers_list.emplace_front(subscriber); // reverse insert 
							clog::log(1, "%s(%s)::processing(`%s`) subscriber `%s` http (%ld).\n", banner.c_str(), queueName.c_str(), subChannelQueue.c_str(), subscriber.c_str(), result);
						}
						else {
							/*
							subscriber are not back of the queue
							*/
							clog::log(1, "%s(%s)::processing(`%s`) subscriber `%s` curl (%ld).\n", banner.c_str(), queueName.c_str(), subChannelQueue.c_str(), subscriber.c_str(), result);
						}
					} while (bContinue);

					{
						std::unique_lock<std::mutex> lock(channel->guardian);
						for (auto&& subs : subscribers_list) {
							channel->subscribers.emplace(subs);
						}
					}

					if (msgDeliveryCounter) {
						if (!msgConfirmCallback.empty()) {
							/* Message are delivered succefuly, need confirm  to producer */
							if (external_post(msgConfirmCallback, request->request_paload(), request->request_paload_length(),
								make_headers(msg_it->first, msg_ctime(msg_it->second), std::time(nullptr), 0, banner,
									{ DEFAULT_HEADERS(msgDeliveryCounter) }, request->request_headers())) == 200) {

								clog::log(3, "%s(%s)::processing(`%s`) confirmed (%s).\n", banner.c_str(), queueName.c_str(), subChannelQueue.c_str(), msg_it->first.str().c_str());

								std::unique_lock<std::mutex> lock(channel->guardian);
								channel->messages.erase(msg_it);
							}
							else if(queueMqConfirmAttempts) { /* init producer confirm loop */
								msg_confirm(msg_it->second) = queueMqConfirmAttempts;
								msg_mtime(msg_it->second) = std::time(nullptr) + queueMqConfirmTimeout;
								clog::log(3, "%s(%s)::processing(`%s`) not confirmed %s (%ld, %ld).\n", banner.c_str(), queueName.c_str(), subChannelQueue.c_str(), msg_it->first.str().c_str(),
									msg_confirm(msg_it->second), msg_mtime(msg_it->second) - msg_ctime(msg_it->second));

								/* emplace message back to queue for next confirm attempt */
								std::unique_lock<std::mutex> lock(channel->guardian);
								channel->queue.emplace_back(msg_it->first);
							}
							else {
								std::unique_lock<std::mutex> lock(channel->guardian);
								channel->messages.erase(msg_it);
							}
						}
						else {
							std::unique_lock<std::mutex> lock(channel->guardian);
							channel->messages.erase(msg_it);
						}
					}
					else {
						/* may be no subscribers found, return message to front */
						clog::log(1, "%s(%s)::processing(`%s`) message cannot be delivered now (%s).\n", banner.c_str(), queueName.c_str(), subChannelQueue.c_str(), msg_it->first.str().c_str());
						std::unique_lock<std::mutex> lock(channel->guardian);
						channel->queue.emplace_front(msg_it->first);
						/* leave channel processing queue */
						break;
					}
				}
				else { /* message are delivered, producer not confirmed */
					if (msg_confirm(msg_it->second)) {
						if (std::time(nullptr) >= msg_mtime(msg_it->second)) {
							--msg_confirm(msg_it->second);
							msg_mtime(msg_it->second) = std::time(nullptr) + queueMqConfirmTimeout;

							if (external_post(msgConfirmCallback, request->request_paload(), request->request_paload_length(),
								make_headers(msg_it->first, msg_ctime(msg_it->second), std::time(nullptr), 0, banner,
									{ DEFAULT_HEADERS(msgDeliveryCounter) }, request->request_headers())) == 200) {

								clog::log(3, "%s(%s)::processing(`%s`) confirmed (%s).\n", banner.c_str(), queueName.c_str(), subChannelQueue.c_str(), msg_it->first.str().c_str());

								std::unique_lock<std::mutex> lock(channel->guardian);
								channel->messages.erase(msg_it);
							}
							else {
								clog::log(3, "%s(%s)::processing(`%s`) not confirmed %s (%ld, %ld).\n", banner.c_str(), queueName.c_str(), subChannelQueue.c_str(), msg_it->first.str().c_str(),
									msg_confirm(msg_it->second), msg_mtime(msg_it->second) - msg_ctime(msg_it->second));

								/* emplace message back to queue for next confirm attempt */
								std::unique_lock<std::mutex> lock(channel->guardian);
								channel->queue.emplace_back(msg_it->first);
							}
						}
						else {
							/* timeout not elapsed */
							std::unique_lock<std::mutex> lock(channel->guardian);
							channel->queue.emplace_back(msg_it->first);
						}
					}
					else {
						clog::log(1, "%s(%s)::processing(`%s`) no more attempts, drop (%s).\n", banner.c_str(), queueName.c_str(), subChannelQueue.c_str(), msg_it->first.str().c_str());
						std::unique_lock<std::mutex> lock(channel->guardian);
						channel->messages.erase(msg_it);
					}
				}
			}
			else {
				clog::log(1, "%s(%s)::processing(`%s`) invalid message (%s).\n", banner.c_str(), queueName.c_str(), subChannelQueue.c_str(), msg_it->first.str().c_str());
			}
		} while (1);
	}
}

void cchannel_mq::OnPUT(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	
	auto&& headers = request->request_headers();
	
	if (auto&& channel_ptr = subchannel(path);  auto channel = channel_ptr.lock()) {
		std::unique_lock<std::mutex> lock(channel->guardian);

		if (!queueLimitCapacity || (queueLimitCapacity < channel->messages.size())) {
			channel->messages.emplace(msg_id, std::make_tuple(
				std::time(nullptr),
				std::time(nullptr),
				-1, /* emplace with no confirm attempts, check it after when message processing */
				request));
			channel->queue.emplace_back(msg_id);
			request->response({}, 202, "Enqueued", { { "X-FIBER-CHANNEL-ENQUEUED",std::to_string(channel->queue.size()) }, DEFAULT_HEADERS_MSG(msg_id,channel->messages.size()) });
		}
		else {
			request->response({}, 507, "Queue limit capacity exceed", { { "X-FIBER-QUEUE-LIMIT",std::to_string(queueLimitCapacity) }, DEFAULT_HEADERS_MSG(msg_id,channel->messages.size()) });
		}
	}
	else {
		request->response({}, 500, "Channel is down", { DEFAULT_HEADERS(0) });
	}

	request->disconnect();
	
	PostQueueMsg(path);
}

/*
void cchannel_mq::OnPOST(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) {
	auto&& headers = request->request_headers();
	if (auto&& it_id = headers.find(_h("x-fiber-msg-id")); it_id != headers.end()) {
		cmsgid req_msg_id(it_id->second.trim());

		std::unique_lock<std::mutex> lock(msgSync);

		if (auto&& req_msg = msgPool.find(req_msg_id); req_msg != msgPool.end()) {
			msg_mtime(req_msg->second) = std::time(nullptr);
			msg_status(req_msg->second) |= crequest::status::complete;

			if (auto&& it_callback = msg_request(req_msg->second)->request_headers().find(_h("x-fiber-channel-callback")); it_callback != headers.end()) {
				if (external_post(it_callback->second.trim().str(), request->request_paload(), request->request_paload_length(),
					make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner,
						{ DEFAULT_HEADERS(msgPool.size()) }, msg_request(req_msg->second)->request_headers())) == 200) {
					request->response({}, 200, {},
						make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner));
					msgPool.erase(req_msg);
				}
				else {
					request->response({}, 499, {},
						make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner));
				}
			}
			else {
				request->response({}, 200, {},
					make_headers(req_msg_id, msg_ctime(req_msg->second), msg_mtime(req_msg->second), msg_status(req_msg->second), banner));
				msgPool.erase(req_msg);
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
*/