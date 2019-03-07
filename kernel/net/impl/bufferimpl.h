#pragma once
#include <forward_list>
#include <cinttypes>
#include <sys/socket.h>
#include <cerrno>
#include <memory>
#include <memory.h>
#include <string>
#include <sys/ioctl.h>

namespace Fiber {

	class ReadBufferImpl {
	private:
		ssize_t									dataLength;
		std::forward_list<std::pair<uint8_t*, size_t>>	dataParts;
	public:
		ReadBufferImpl() : dataLength(0) { ; }
		~ReadBufferImpl() {
			for (auto&& part : dataParts) {
				delete[] part.first;
			}
		}
		inline int Read(int fd, ssize_t MaxLength, ssize_t PartLength = 8192) noexcept {
			ssize_t nread = 0;
			int ncount = 0;
			do {
				auto&& it = dataParts.emplace_after(dataParts.before_begin(),new uint8_t[PartLength], PartLength);
				it->second = nread = ::recv(fd, it->first, PartLength, 0);
			} while (nread > 0 && (dataLength += nread) && (dataLength < MaxLength) && (ioctl(fd, FIONREAD, &ncount) == 0 && ncount > 0));

			return nread < 0 ? errno : 0;
		}
		inline bool empty() const { return dataLength == 0; }
		inline ssize_t length() const { return dataLength; }
		inline std::pair<const uint8_t*, const uint8_t*> Head() const { auto&& head = dataParts.front(); return { head.first,head.first + head.second }; }
		inline std::string Content(size_t HeadOffset = 0) const {
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