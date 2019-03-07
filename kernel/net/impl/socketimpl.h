#pragma once
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <cerrno>
#include <string>
#include <memory.h>
#include <trace.h>
#include "bufferimpl.h"

namespace Fiber {
	namespace Socket {
		template <typename T>
		static inline ssize_t SetOpt(int socket, int level, int optname, const T& optval) noexcept {
			return setsockopt(socket, level, optname, (const void*)&optval, (socklen_t)sizeof(T));
		}
		template <typename T>
		static inline ssize_t SetOpt(int socket, int level, int optname, T* optval, socklen_t optlen) noexcept {
			return setsockopt(socket, level, optname, (const void*)optval, optlen);
		}

		static inline ssize_t ReadData(int socket,ReadBufferImpl& data,size_t MaxLength = 1048576, size_t PartLength = 8192) noexcept {
			return data.Read(socket, MaxLength, PartLength);
		}

		static inline ssize_t WriteData(int& socket,const void* data,size_t data_length) noexcept {
			return data_length ? send(socket, data, data_length, 0) : 0;
		}

		static inline ssize_t Close(int& socket) noexcept {
			return (socket && close(socket) == 0) ? (socket = 0) : errno;
		}

		static inline ssize_t Open(int& socket,const std::string& device,const std::string& port,const std::string& maxcon="400") noexcept {
			socket = 0;
			if ((socket = ::socket(AF_INET, SOCK_STREAM /*| SOCK_NONBLOCK*/, IPPROTO_TCP)) <= 0) {
				return errno;
			}
			if (SetOpt(socket, SOL_TCP, TCP_NODELAY, 1) != 0) {
				trace("Invalid TCP_NODELAY %s(%ld)", strerror(errno), errno);
			}
			if (SetOpt(socket, SOL_TCP, TCP_FASTOPEN, 5) != 0) {
				trace("Invalid TCP_FASTOPEN %s(%ld)",strerror(errno), errno);
			}

			if (SetOpt(socket, SOL_SOCKET, SO_REUSEADDR, 1)) {
				trace("Invalid SO_REUSEADDR %s(%ld)", strerror(errno), errno);
			}
			
			if (!device.empty() && SetOpt(socket, SOL_SOCKET, SO_BINDTODEVICE, device.c_str(), (socklen_t)device.length()) != 0) {
				trace("Invalid SO_BINDTODEVICE %s(%ld)", strerror(errno), errno);
			}

			struct sockaddr_in sin;
			
			memset((char *)&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
			sin.sin_port = htons((uint16_t)std::stoi(port));
			sin.sin_addr.s_addr = htonl(INADDR_ANY);

			if (::bind(socket, (sockaddr*)&sin, sizeof(sockaddr_in)) != 0) {
				auto err = errno;
				::close(socket);
				return err;
			}

			if (::listen(socket, std::stoi(maxcon)) != 0) {
				auto err = errno;
				::close(socket);
				return err;
			}
			return 0;
		}
	}
}





