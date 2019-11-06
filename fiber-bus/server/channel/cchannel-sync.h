#pragma once
#include "cchannel.h"
#include <unordered_map>
#include <queue>
#include <memory>
#include <tuple>
#include <ctime>
#include <mutex>

namespace fiber {
	class cchannel_sync : virtual public cchannel {
		static const inline std::string banner{"SYNC-CHANNEL"};
	protected:
		virtual void OnPOST(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request);
		virtual void OnPUT(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request);
		virtual void OnHEAD(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request);
	public:
		using message = std::tuple<uint16_t, std::time_t, std::time_t, std::shared_ptr<fiber::crequest>>;

		cchannel_sync(const std::string& name, const core::coptions& options);
		virtual ~cchannel_sync();

		virtual void processing(const cmsgid& msg_id, const std::string& uri, const std::shared_ptr<fiber::crequest>& msg);
	private:
		std::string					queueName{};
		std::size_t					queueLimitCapacity{ 0 };
		std::string					queueDurability{};
		std::size_t					sapiExecLimit{ 0 };
		std::size_t					sapiExecTimeout{ 0 };
		core::coption::dsn_params	sapiExecScript{};
		std::unordered_map<cmsgid, message>	msgPool;
		std::mutex		msgSync;
		std::unique_ptr<csapi>	queueSapi;
	};
}