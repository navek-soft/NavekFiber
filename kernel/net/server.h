#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <atomic>
#include <sys/socket.h>
#include <poll.h>
#include <utils/timestamp.h>
#include "impl/zcstringimpl.h"
#include "impl/protoimpl.h"

namespace Fiber {
	using namespace std;
	
	using msgid = uint_fast64_t;

	class Server {
	public:
		class CAddress {
		public:
			uint16_t	Port;
			uint32_t	Address[4];
			enum { Unsupported = 0, Ip4 = 4, Ip6 = 6 } Version;
			CAddress() : Port(0) { Address[0] = Address[1] = Address[2] = Address[3] = 0; Version = Unsupported; }
			CAddress(const sockaddr_storage& sa);
			const char* toString();
		};
		struct CTelemetry {
			utils::microseconds		tmCreatedAt;
			utils::microseconds		tmReadingAt;
			utils::microseconds		tmFinishedAt;
			CTelemetry() : tmCreatedAt(0), tmReadingAt(0), tmFinishedAt(0) { ; }
		} Telemetry;
		class CHandler {
		protected:
			CHandler() { ; }
			virtual ~CHandler() { ; }
		public:
			virtual void Process() = 0;
			/* Why uint8_t*,char* ? Because ABI support need */
			virtual void Reply(size_t Code, const char* Message = nullptr, const uint8_t* Content = nullptr, size_t ContentLength = 0, const unordered_map<const char*, const char*>& HttpHeaders = {}, bool ConnectionClose = true) = 0;
			virtual inline const msgid& GetMsgIds() const = 0;
			virtual inline const CAddress& GetAddress() const = 0;
			virtual inline const Proto::RequestMethod& GetMethod() const = 0;
			virtual inline const zcstring& GetUri() const = 0;
			virtual inline const zcstring& GetParams() const = 0;
			virtual inline const deque<std::pair<zcstring, zcstring>>& GetHeaders() const = 0;
			virtual inline string GetContent() const = 0;
			virtual inline CTelemetry& GetTelemetry() = 0;
		};
	private:
		using callback = function<shared_ptr<CHandler>(msgid, int, sockaddr_storage&,socklen_t)>;
		uint_fast64_t					msgIndex;
		unordered_map<int, callback>	conHandlers;
		vector<struct pollfd>			conFd;
	public:
		Server();
		~Server();
		int AddListener(const string& proto, const string& listen, const string& query_limit="1048576", const string& header_limit = "8192");

		bool Listen(std::function<void(shared_ptr<CHandler>)>&& callback,size_t timeout_msec = 5000);
	};

	static inline const char* toString(const size_t& val) {
		constexpr size_t size = 4 * sizeof(size_t) + 1;
		static thread_local char buffer[size];
		std::snprintf(buffer, size, "%" PRId64, val);
		return buffer;
	}
}