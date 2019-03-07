#pragma once
#include "../server.h"
#include "bufferimpl.h"
#include <list>

namespace Fiber {
	namespace Proto {
		class HttpImpl : public Server::CHandler {
		private:
			class CRequest {
			public:
				ReadBufferImpl		Raw;
				RequestMethod		Method;
				zcstring			Uri;
				zcstring			Params;
				deque<std::pair<zcstring, zcstring>> Headers;
				size_t				PayloadOffset;
				inline string		Content() const { return Raw.Content(PayloadOffset); }
			};

		public:
			int							Handle;
			msgid						MsgId;
			size_t						MaxRequestLength;
			size_t						MaxHeaderLength;
			Server::CAddress			Address;
			CRequest					Request;
			Server::CTelemetry			Telemetry;
		public:
			HttpImpl(msgid mid, int hSocket, sockaddr_storage& sa,size_t max_request_length= 1048576,size_t max_header_length = 8192) noexcept;
			~HttpImpl();

			virtual void Process();
			virtual void Reply(size_t Code, const char* Message = nullptr, const uint8_t* Content = nullptr,size_t ContentLength = 0, const unordered_map<const char*, const char*>& HttpHeaders = {}, bool ConnectionClose=true);
			virtual inline const msgid& GetMsgIds() const { return MsgId; }
			virtual inline const Server::CAddress& GetAddress() const { return Address; }
			virtual inline const Proto::RequestMethod& GetMethod() const { return Request.Method; }
			virtual inline const zcstring& GetUri() const { return Request.Uri; }
			virtual inline const zcstring& GetParams() const { return Request.Params; }
			virtual inline const deque<std::pair<zcstring, zcstring>>& GetHeaders() const { return Request.Headers; }
			virtual inline std::string GetContent() const { return Request.Content(); }
			virtual inline Server::CTelemetry& GetTelemetry() { return Telemetry; }
		};
	}
}