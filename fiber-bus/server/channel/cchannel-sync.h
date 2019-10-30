#pragma once
#include "cchannel.h"
#include <memory>
#include <tuple>
#include <ctime>
#include <queue>
#include <shared_mutex>

namespace fiber {
	class cchannel_sync : virtual public cchannel {
	protected:
		virtual void OnGET(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request);
		virtual void OnPOST(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request);
		virtual void OnHEAD(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request);
	public:
		using message = std::tuple<uint16_t, std::time_t, std::time_t, std::shared_ptr<fiber::crequest>>;

		cchannel_sync(const core::coptions& options);
		virtual ~cchannel_sync();

		virtual void processing(const cmsgid& msg_id, const std::string& uri, const std::shared_ptr<fiber::crequest>& msg);
	private:
		std::size_t					queueLimitCapacity{ 0 };
		std::size_t					queueLimitSapiSameTime{ 0 };
		std::size_t					queueLimitSapiPeriodTime{ 0 };
		std::queue<std::pair<cmsgid, message>>		msgQueue;
		std::shared_mutex		msgSync;
		std::unique_ptr<csapi>	queueSapi;
	};
}