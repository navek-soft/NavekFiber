#pragma once
#include "cchannel.h"
#include <unordered_map>
#include <queue>
#include <memory>
#include <tuple>
#include <ctime>
#include <shared_mutex>

namespace fiber {
	class cchannel_mq : virtual public cchannel {
		static const inline std::string banner{ "MQ-CHANNEL" };
		enum mode { mRoundRobin = 1, mBroadcast };

		void PostQueueMsg(const std::string& subChannelQueue);

	protected:
		virtual void OnCONNECT(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request);
		virtual void OnPOST(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request);
		virtual void OnPUT(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request);
	public:
		using message = std::tuple<uint16_t, std::time_t, std::time_t, std::shared_ptr<fiber::crequest>, std::shared_ptr<fiber::crequest>>;

		cchannel_mq(const std::string& name, const core::coptions& options);
		virtual ~cchannel_mq();

		virtual void processing(const cmsgid& msg_id, const std::string& uri, const std::shared_ptr<fiber::crequest>& msg);
	private:
		using element = cmsgid;
		/*
		using element = std::pair<std::time_t, cmsgid>;

		class element_less {
		public:
			inline bool operator ()(const element& l, const element& r) const {
				return l.first < r.first;
			}
		};
		*/
		std::string					queueName{};
		std::size_t					queueLimitCapacity{ 0 }, queueLimitSibscribers{ 0 };
		mode						queueMode{ mRoundRobin };
		std::string					queueDurability{};
		std::unordered_map<std::string, std::queue<std::string>>	msgSubscribers;
		std::unordered_map<cmsgid, message>	msgPool;
		//std::unordered_map<std::string, std::priority_queue<element,std::vector<element>, element_less>>		msgQueue;
		std::unordered_map<std::string, std::queue<element>>	msgQueue;
		std::shared_mutex		msgSync;
	};
}