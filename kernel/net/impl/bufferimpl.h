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

	class ReadBufferImpl {
	private:
		ssize_t									dataLength;
		std::list<std::pair<uint8_t*, size_t>>	dataParts;
	public:
		ReadBufferImpl() : dataLength(0) { ; }
		~ReadBufferImpl() {
			for (auto&& part : dataParts) {
				printf("%s(free:%lx)\n", __FUNCTION__, part.first);
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
		inline bool empty() const { return dataLength > 0; }
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

	class WriteBufferImpl {
	private:
		ssize_t										dataLength;
		std::list<std::pair<const char*,size_t>>	dataParts;
	public:
		WriteBufferImpl() : dataLength(0) { ; }
		~WriteBufferImpl() { ; }
		inline ssize_t Write(int fd) noexcept {
			ssize_t writen = 0;
			for (auto&& part : dataParts) {
				writen += send(fd, part.first, part.second, 0);
			}
			return writen;
		}

		inline WriteBufferImpl& Emplace(std::string&& data) {
			dataLength += data.size();
			dataParts.emplace_back(data.c_str(), data.size());
			return *this;
		}

		inline WriteBufferImpl& Emplace(const std::string& data) {
			dataLength += data.size();
			dataParts.emplace_back(data.c_str(), data.size());
			return *this;
		}
	};
}