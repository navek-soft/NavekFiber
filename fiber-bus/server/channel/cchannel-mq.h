#pragma once
#include "cchannel.h"
#include <unordered_map>
#include <queue>
#include <memory>
#include <tuple>
#include <ctime>
#include <mutex>

namespace fiber {
	class cchannel_mq : virtual public cchannel {
		static const inline std::string banner{ "MQ-CHANNEL" };
		enum DeliveryMode { mRoundRobin = 1, mBroadcast };

		void PostQueueMsg(const std::string& subChannelQueue);

	protected:
		virtual void OnCONNECT(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request);
		//virtual void OnPOST(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request);
		virtual void OnPUT(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request);
	public:
		using message = std::tuple<std::time_t, std::time_t, ssize_t, std::shared_ptr<fiber::crequest>>;

		cchannel_mq(const std::string& name, const core::coptions& options);
		virtual ~cchannel_mq();

		virtual void processing(const cmsgid& msg_id, const std::string& uri, const std::shared_ptr<fiber::crequest>& msg);
	private:
		struct channel_queue {
			std::queue<std::string>		subscribers;
			std::list<cmsgid>			queue;
			std::mutex					guardian;
			std::unordered_map<cmsgid, message>	messages;
			channel_queue() = default;
			~channel_queue() = default;
		};

		std::weak_ptr<channel_queue> subchannel(const std::string& channel_uri);

		std::string					queueName{};
		std::size_t					queueLimitCapacity{ 0 }, queueLimitSibscribers{ 0 }, 
									queueMqConfirmAttempts{ 0 }, queueMqConfirmTimeout{0};
		DeliveryMode				queueMode{ mRoundRobin };
		std::string					queueDurability{};
		std::unordered_map<std::string, std::shared_ptr<channel_queue>>	queueChannels;
		std::mutex		msgSync;
	};
}