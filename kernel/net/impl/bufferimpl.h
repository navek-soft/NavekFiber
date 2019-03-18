#pragma once
#include <list>
#include <cinttypes>
#include <sys/socket.h>
#include <cerrno>
#include <memory>
#include <memory.h>
#include <string>
#include <sys/ioctl.h>
#include <utils/str.h>

namespace Fiber {

	class ReadBufferImpl {
	private:
		ssize_t		dataLength;
		ssize_t		dataPartLength;
		uint8_t*	dataBuffer;
	public:
		ReadBufferImpl() : dataLength(0), dataBuffer(nullptr){ ; }
		~ReadBufferImpl() {
			if (dataBuffer) { delete[] dataBuffer; }
		}
		inline int Read(int fd, ssize_t MaxLength, ssize_t PartLength = 8192) noexcept {
			ssize_t nread = 0,noffset=0;
			int ncount = 0;
			dataPartLength = PartLength;
			dataBuffer = new uint8_t[MaxLength];
			do {
				nread = ::recv(fd, dataBuffer + noffset, MaxLength - noffset, 0);
			} while (nread > 0 && (dataLength += nread) && (ioctl(fd, FIONREAD, &ncount) == 0 && ncount > 0) && (noffset += nread) && ((noffset + ncount) < MaxLength));
			printf("|||| %ld (%ld)\n", nread, dataLength);
			return nread < 0 ? errno : 0;
		}
		inline bool empty() const { return dataLength == 0; }
		inline ssize_t length() const { return dataLength; }
		inline const zcstring Head() const { 
			return dataBuffer ? zcstring(dataBuffer, dataBuffer + dataPartLength) :	zcstring(nullptr,nullptr); 
		}
		inline const zcstring Content(ssize_t HeadOffset = 0) const {
			return dataBuffer ? zcstring(dataBuffer + (HeadOffset < dataLength ? HeadOffset : dataLength), dataBuffer + dataLength) : zcstring(nullptr, nullptr);
		}
	};
}