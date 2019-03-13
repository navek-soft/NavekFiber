#pragma once
#include "../server.h"

namespace Fiber {
	namespace Proto {
		class WebSocketImpl : public Server::CHandler {
		protected:
			Server::CTelemetry Telemetry;
		public:
			WebSocketImpl(msgid mid, int hSocket, sockaddr_storage& sa, size_t max_request_length = 1048576, size_t max_header_length = 8192) noexcept { printf("%s\n", __PRETTY_FUNCTION__); }
			~WebSocketImpl() { printf("%s\n", __PRETTY_FUNCTION__); }

			virtual void Process() {}
			virtual void Reply(size_t Code, const char* Message = nullptr, const uint8_t* Content = nullptr, size_t ContentLength = 0, const unordered_map<const char*, const char*>& HttpHeaders = {}, bool ConnectionClose = true) { ; }
			virtual inline const msgid& GetMsgId() const { return 0; }
			virtual inline const Server::CAddress& GetAddress() const { return {}; }
			virtual inline const Proto::RequestMethod& GetMethod() const { return Proto::UNSUPPORTED; }
			virtual inline const zcstring& GetUri() const { return {}; }
			virtual inline const zcstring& GetParams() const { return {}; }
			virtual inline const deque<std::pair<zcstring, zcstring>>& GetHeaders() const { return {}; }
			virtual inline std::string GetContent() const { return {}; }
			virtual inline Server::CTelemetry& GetTelemetry() { return Telemetry; }
		};
	}
}