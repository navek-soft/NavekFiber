#include "server.h"
#include <ctime>
#include "impl/httpimpl.h"
#include "impl/wsimpl.h"
#include "impl/socketimpl.h"
#include <utils/findmatch.h>
#include <utils/transform.h>
#include <trace.h>
#include <csignal>

using namespace Fiber;

inline msgid getMsgId(std::atomic_uint_fast32_t&& Counter) {
	return (((uint_fast64_t)time(nullptr)) << 32) | ((uint_fast64_t)(Counter++));
}

inline struct pollfd pollin(int socket) {
	return { socket,POLLIN,0 };
}


Server::Server(const unordered_map<string, string>& options) : msgIndex(0) {
	signal(SIGPIPE, SIG_IGN);
}

Server::~Server() {
	;
}

bool Server::AddListener(const string& proto, const string& listen, const unordered_map<string, string>& options) {
	auto&& r = utils::match(proto, "http"/*, "ws"*/);
	int so_server;

	if (r.result()) {
		auto&& bind_params = utils::explode(":", listen, 2);
		auto err = Socket::Open(so_server, bind_params[0], bind_params[1]);
		if (err != 0) {
			trace("Protocol handler `%s` open listen failed. %s(%ld). Listener `%s` will be skipped", proto.c_str(), strerror(err), err, listen.c_str());
			return false;
		}

		if (r.result() == 1 /* http */) {
			conHandlers.emplace(so_server, [options](int socket,sockaddr_storage& sa, socklen_t len) -> shared_ptr<Handler> {
				return shared_ptr<Handler>(new Proto::HttpImpl(socket, sa, len));
			});
		}
		else {
			trace("Protocol handler `%s` not implemented. Listener `%s` will be skipped", proto.c_str(), listen.c_str());
			return false;
		}

		conFd.emplace_back(pollin(so_server));

		return true;
	}
	trace("Protocol handler `%s` not implemented. Listener `%s` will be skipped", proto.c_str(), listen.c_str());
	return false;
}

bool Server::Listen(size_t timeout_msec) {
	auto result = poll(conFd.data(), conFd.size(), (int)timeout_msec);
	switch (result) {
	case -1: return false;
	case 0:	return true;
	default:
		for (size_t s = 0; result && s < conFd.size(); s++)
		{
			if (conFd[s].revents & POLLIN) {
				result--;
				conFd[s].revents = 0;
				while (1)
				{
					sockaddr_storage sa;
					socklen_t len = sizeof(sa);
					int so_client = accept(conFd[s].fd, (sockaddr*)&sa, &len);
					if (so_client > 0) {
						conHandlers[conFd[s].fd](so_client, sa,len);
						continue;
					}
					else if (so_client < 0) {
						trace("Invalid accept connection %s(%ld)", strerror(errno), errno);
					}
					break;
				}
			}
		}
	}
	return true;
}