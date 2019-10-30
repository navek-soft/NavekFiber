#include "cpoll.h"
#include "cexcept.h"

#include <unistd.h>

using namespace core;

cepoll::cepoll() {
	if (fdPoll = epoll_create1(0); fdPoll <= 0) { throw core::system_error(errno); }
}

cepoll::~cepoll() {

	shutdown();
}

void cepoll::shutdown() {

	if (fdPoll != -1) { close(fdPoll); fdPoll = -1; }
}


ssize_t cepoll::wait(cepoll::events& list, ssize_t timeout) {
	//https://github.com/CppComet/comet-server/blob/master/src/tcpServer.cpp

	if (auto nevents = epoll_wait(fdPoll, (struct epoll_event*)list.data(), (int)list.size(), (int)timeout); nevents >= 0) {
		return nevents;
	}
	if (errno == EINTR || errno == EAGAIN) { return 0; }
	printf("epoll_wait(%d) %s\n", errno, strerror(errno));
	return -errno;
}


ssize_t cepoll::emplace(int fd, const uint32_t& events) {
	struct epoll_event event;
	event.events = events;
	event.data.fd = fd;
	return !epoll_ctl(fdPoll, EPOLL_CTL_ADD, fd, &event) ? 0 : -errno;
}

ssize_t cepoll::update(int fd, const uint32_t& events) {
	struct epoll_event event;
	event.events = events;
	event.data.fd = fd;
	return !epoll_ctl(fdPoll, EPOLL_CTL_MOD, fd, &event) ? 0 : -errno;
}

ssize_t cepoll::remove(int fd) {
	struct epoll_event event;
	event.data.fd = fd;
	event.events = 0;
	return !epoll_ctl(fdPoll, EPOLL_CTL_DEL, fd, &event) ? 0 : -errno;
}