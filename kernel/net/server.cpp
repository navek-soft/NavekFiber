#include "server.h"
#include <ctime>
#include "impl/httpimpl.h"
#include "impl/wsimpl.h"
#include "impl/socketimpl.h"
#include <utils/findmatch.h>
#include <utils/transform.h>
#include <utils/option.h>
#include <trace.h>
#include <csignal>
#include <coreexcept.h>
#include <netinet/in.h>

using namespace Fiber;

inline msgid getMsgId(uint_fast64_t&& Counter) {
	return (((uint_fast64_t)time(nullptr)) << 32) | ((uint_fast64_t)(Counter++));
}

inline struct pollfd pollin(int socket) {
	return { socket,POLLIN,0 };
}


Server::Server() : msgIndex(0) {
	signal(SIGPIPE, SIG_IGN);
}

Server::~Server() {
}

ssize_t Server::AddListener(const string& proto, const string& listen, const string& query_limit, const string& header_limit) {
	auto&& r = utils::match(proto, "http"/*, "ws"*/);
	int so_server;
	int err = EPROTONOSUPPORT;
	if (r.result()) {
		auto&& bind_params = utils::explode(":", listen, 2);
		
		if (r.result() == 1 /* http */) {
			if ((err = Socket::Open(so_server, bind_params[0], bind_params[1])) == 0) {
				auto optQueryLimit = utils::option::get_bytes(query_limit, 1048576);
				auto optHeaderLimit = utils::option::get_bytes(header_limit, 8192);
				conHandlers.emplace(so_server, [optQueryLimit, optHeaderLimit](msgid mid, int socket, sockaddr_storage& sa, socklen_t len) -> CHandler* {
					return (CHandler*)(new Proto::HttpImpl(mid, socket, sa, optQueryLimit, optHeaderLimit));
				});
				conFd.emplace_back(pollin(so_server));
				return 0;
			}
		}
	}
	return err;
}

bool Server::Listen(std::function<void(CHandler*)>&& callback, size_t timeout_msec) {
	auto result = poll(conFd.data(), conFd.size(), (int)timeout_msec);
	switch (result) {
	case -1: 
		return errno == EINTR;
	case 0:	return true;
	default:
		for (size_t s = 0; result && s < conFd.size(); s++)
		{
			if (conFd[s].revents & POLLIN) {
				conFd[s].revents = 0;
				while (result--)
				{
					sockaddr_storage sa;
					socklen_t len = sizeof(sa);
					int so_client = accept(conFd[s].fd, (sockaddr*)&sa, &len);
					if (so_client > 0) {
						callback(conHandlers[conFd[s].fd](getMsgId(move(msgIndex)), so_client, sa, len));
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

Server::CAddress::CAddress(const sockaddr_storage& sa) {
	if (sa.ss_family == AF_INET) {
		Version = Ip4;
		Address[0] = htonl(((const sockaddr_in&)(sa)).sin_addr.s_addr);
		Port = htons(((const sockaddr_in&)(sa)).sin_port);
	}
	else if (sa.ss_family == AF_INET6) {
		Version = Ip4;
		uint32_t* addr = (uint32_t*)&(((const sockaddr_in6&)(sa)).sin6_addr.__in6_u.__u6_addr32);
		Address[0] = htonl(addr[0]); Address[1] = htonl(addr[1]);
		Address[2] = htonl(addr[2]); Address[3] = htonl(addr[3]);
		Port = htons(((const sockaddr_in&)(sa)).sin_port);
	}
	else {
		Version = Unsupported;
	}
}

const char* Server::CAddress::toString() const {
	static thread_local char buffer[48];
	if (Version == Ip4) {
		auto addr = (uint8_t*)(&Address[0]);
		snprintf(buffer, 48, "%u.%u.%u.%u", addr[3], addr[2], addr[1], addr[0]);
	}
	else if (Version == Ip6) {
		uint8_t* addr = (uint8_t*)&Address[0];
		snprintf(buffer, 48, "%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X",
			addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7],
			addr[8], addr[9], addr[10], addr[11], addr[12], addr[13], addr[14], addr[15]);
	}
	else {
		buffer[0] = '\0';
	}
	return buffer;
}