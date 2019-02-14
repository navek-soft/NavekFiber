#pragma once
#include "../server.h"

namespace Fiber {
	namespace Proto {
		class WebSocketImpl : public Server::Handler {
		public:
			WebSocketImpl(int hSocket, sockaddr_storage sa) { printf("%s\n", __PRETTY_FUNCTION__); }
			~WebSocketImpl() { printf("%s\n", __PRETTY_FUNCTION__); }
		};
	}
}