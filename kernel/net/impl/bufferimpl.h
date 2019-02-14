#pragma once
#include <list>
#include <cinttypes>
#include <sys/socket.h>
#include <cerrno>
#include <memory>
#include <memory.h>
#include <string>
#include <sys/ioctl.h>

namespace Fiber {

	class BufferImpl {
	private:
		ssize_t									dataLength;
		std::list<std::pair<uint8_t*, size_t>>	dataParts;
	public:
		BufferImpl() : dataLength(0) { ; }
		~BufferImpl() { 
			for (auto&& part : dataParts) {
				delete[] part.first;
			}
		}
		inline int Read(int fd, ssize_t MaxLength, ssize_t PartLength = 8192) noexcept {
			ssize_t nread = 0;
			int ncount = 0;
			do {
				dataParts.emplace_back(new uint8_t[PartLength], PartLength);
				dataParts.back().second = nread = ::recv(fd, dataParts.back().first, PartLength, 0);
			} while (nread > 0 && (dataLength += nread) && (dataLength < MaxLength) && (ioctl(fd, FIONREAD, &ncount) == 0 && ncount > 0));

			return nread < 0 ? errno : 0;
		}
		inline bool empty() { return dataLength > 0; }
		inline ssize_t length() { return dataLength; }
		inline std::pair<const uint8_t*, const uint8_t*> Head() { auto&& head = dataParts.front(); return { head.first,head.first + head.second }; }
		inline std::string Content(size_t HeadOffset = 0) {
			std::string result;
			result.resize(dataLength - HeadOffset);
			size_t offset = 0;
			for (auto&& it = dataParts.begin(); it != dataParts.end(); offset += it->second, it++, HeadOffset = 0) {
				memcpy(result.data() + offset,it->first,it->second);
			}
			return result;
		}
	};
}