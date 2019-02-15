#pragma once
#include "../server.h"
#include "bufferimpl.h"
#include "protoimpl.h"
#include "zcstringimpl.h"

namespace Fiber {
	namespace Proto {
		class HttpImpl : public Server::Handler {
		private:
			class Request {
			protected:
				friend class HttpImpl;
				ReadBufferImpl		sRaw;
				RequestMethod		sMethod;
				zcstring			sUri;
				zcstring			sParams;
				deque<std::pair<zcstring, zcstring>> sHeaders;
				size_t				sPayloadOffset;
			};
			class Response {
			protected:
				friend class HttpImpl;
				WriteBufferImpl					sRaw;
				size_t							sCode;
				const char*						sMessage;
				unordered_map<string, string>	sHeaders;
				list<string>					sPayload;
			public:
				Response(size_t Code, const char* Message = nullptr, unordered_map<string, string>&& Headers = {}, list<string>&& Payload = {});
			};
		private:
			int					sHandle;
			sockaddr_storage	sAddress;
			Request				sRequest;
		public:
			HttpImpl(msgid mid, int hSocket, sockaddr_storage& sa, socklen_t len);
			~HttpImpl();
		};
	}
}