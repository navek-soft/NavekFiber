#pragma once
#include "../server.h"
#include "bufferimpl.h"

namespace Fiber {
	namespace Proto {
		class HttpImpl : public Server::Handler {
			int					sHandle;
			sockaddr_storage	sAddress;
			BufferImpl			sRequest;
		public:
			HttpImpl(int hSocket, sockaddr_storage& sa, socklen_t len);
			~HttpImpl();
		};
	}
}