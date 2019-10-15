#include "cserver.h"
#include "cexcept.h"
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/timerfd.h>

#pragma GCC diagnostic ignored "-Wterminate"

using namespace core;

void cserver::emplace_tcp(std::shared_ptr<cserver::base>&& srv) throw() {
	auto&& server = srv.get()->get<tcp>();
	if (int sock = socket(AF_INET, (int)server.soType | SOCK_NONBLOCK, (int)server.soProtocol); sock > 0) {
		try {
			/* options adjust */
			if (server.flagReuseAddress && setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &server.flagReuseAddress, sizeof(server.flagReuseAddress)) < 0)
				throw core::system_error(errno, "TCP-SERVER(SO_REUSEADDR)", __PRETTY_FUNCTION__, __LINE__);
			//fprintf(stderr, "\x1b[1;31mCannot set SO_REUSEADDR option on listen socket (%s)\x1b[0m\n", strerror(errno));
			if (server.flagReusePort && setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &server.flagReusePort, sizeof(server.flagReusePort)) < 0)
				throw core::system_error(errno, "TCP-SERVER(SO_REUSEPORT)", __PRETTY_FUNCTION__, __LINE__);
			if (server.flagNoDelay && setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &server.flagNoDelay, sizeof(server.flagNoDelay)) < 0)
				throw core::system_error(errno, "TCP-SERVER(TCP_NODELAY)", __PRETTY_FUNCTION__, __LINE__);
			if (server.flagKeepAlive)
			{
				if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &server.flagKeepAlive, sizeof(server.flagKeepAlive)) == 0) {
					if (server.optKeepAliveCountAttempts && setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &server.optKeepAliveCountAttempts, sizeof(server.optKeepAliveCountAttempts)) < 0)
						throw core::system_error(errno, "TCP-SERVER(TCP_KEEPCNT)", __PRETTY_FUNCTION__, __LINE__);
					else if (server.optKeepAliveIdle && setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &server.optKeepAliveIdle, sizeof(server.optKeepAliveIdle)) < 0)
						throw core::system_error(errno, "TCP-SERVER(TCP_KEEPIDLE)", __PRETTY_FUNCTION__, __LINE__);
					else if (server.optKeepAliveInterval && setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &server.optKeepAliveInterval, sizeof(server.optKeepAliveInterval)) < 0)
						throw core::system_error(errno, "TCP-SERVER(TCP_KEEPINTVL)", __PRETTY_FUNCTION__, __LINE__);
				}
				else {
					throw core::system_error(errno, "TCP-SERVER(SO_KEEPALIVE)", __PRETTY_FUNCTION__, __LINE__);
				}
			}
			if (server.flagFastOpen && setsockopt(sock, IPPROTO_TCP, TCP_FASTOPEN, &server.flagFastOpen, sizeof(server.flagFastOpen)) < 0)
				throw core::system_error(errno, "TCP-SERVER(TCP_FASTOPEN)", __PRETTY_FUNCTION__, __LINE__);

			/* bind to interface */
			if (!server.sIFace.empty()) {
				struct ifreq ifr;
				bzero(&ifr, sizeof(ifr));
				snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), server.sIFace.c_str());
				if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, (void*)&ifr, sizeof(ifr)) < 0) 
					throw core::system_error(errno, "TCP-SERVER(SO_BINDTODEVICE)", __PRETTY_FUNCTION__, __LINE__);
			}

			/* binding */
			{
				struct sockaddr_in addr;
				bzero(&addr, sizeof(addr));
				addr.sin_family = AF_INET;
				addr.sin_port = htons((uint16_t)server.nPort);
				addr.sin_addr.s_addr = server.sIp.empty() ? htonl(INADDR_ANY) : inet_addr(server.sIp.c_str());

				if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) 
					throw core::system_error(errno, "TCP-SERVER(bind)", __PRETTY_FUNCTION__, __LINE__);
				
				//accept4
			}

			if (::listen(sock, server.optMaxConnections > 0 ? server.optMaxConnections : SOMAXCONN) < 0)
				throw core::system_error(errno, "TCP-SERVER(listen)", __PRETTY_FUNCTION__, __LINE__);

			if (!socketPool.emplace(sock, std::pair<uint8_t, std::shared_ptr<cserver::base>>(1, srv)).second || eventPoll.emplace(sock, cepoll::in | cepoll::hup | cepoll::offline) < 0) {
				//clientPool.emplace(sock, std::unordered_set<int>());
				throw core::system_error(errno);
			}

			if (server.optReceiveTimeout > 0) {
				int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
				if ( timer_fd == -1) {
					throw core::system_error(errno, "TCP-SERVER(timerfd_create)", __PRETTY_FUNCTION__, __LINE__);
				}
				{
					struct itimerspec ts;
					/*
					ts.it_value.tv_sec = (server.optReceiveTimeout + 500) / 1000;
					ts.it_value.tv_nsec = (server.optReceiveTimeout % 1000) * 1000000;
					ts.it_interval.tv_sec = 0;
					ts.it_interval.tv_nsec = 0;
					*/
					ts.it_interval.tv_sec = (server.optReceiveTimeout + 500) / 1000;
					ts.it_interval.tv_nsec = (server.optReceiveTimeout % 1000) * 1000000;
					ts.it_value.tv_sec = 0;
					ts.it_value.tv_nsec = 0;
					if (timerfd_settime(timer_fd,TFD_TIMER_ABSTIME, &ts, NULL) < 0) {
						close(timer_fd);
						throw core::system_error(errno);
					}
					if (!socketPool.emplace(timer_fd, std::pair<uint8_t, std::shared_ptr<cserver::base>>(3, srv)).second || eventPoll.emplace(timer_fd, cepoll::in | cepoll::edge) < 0) {
						//clientPool.emplace(sock, std::unordered_set<int>());
						throw core::system_error(errno);
					}
				}
				
			}
		}
		catch (core::system_error & er) {
			socketPool.erase(sock);
			::close(sock);
			throw;
		}
	}
	else {
		throw core::system_error(errno, "TCP-SERVER(socket)", __PRETTY_FUNCTION__,__LINE__);
	}
}

void cserver::emplace_udp(std::shared_ptr<cserver::base>&& srv) throw() {
	auto&& server = srv.get()->get<udp>();

	if (int sock = socket(AF_INET, (int)server.soType | SOCK_NONBLOCK, (int)server.soProtocol); sock > 0) {
		try {
			/* options adjust */
			if (server.flagReuseAddress && setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &server.flagReuseAddress, sizeof(server.flagReuseAddress)) < 0)
				throw core::system_error(errno, "UDP-SERVER(SO_REUSEADDR)", __PRETTY_FUNCTION__, __LINE__);
			//fprintf(stderr, "\x1b[1;31mCannot set SO_REUSEADDR option on listen socket (%s)\x1b[0m\n", strerror(errno));
			if (server.flagReusePort && setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &server.flagReusePort, sizeof(server.flagReusePort)) < 0)
				throw core::system_error(errno, "UDP-SERVER(SO_REUSEPORT)", __PRETTY_FUNCTION__, __LINE__);
			
			/* binding */
			{
				struct sockaddr_in addr;
				bzero(&addr, sizeof(addr));
				addr.sin_family = AF_INET;
				addr.sin_port = htons((uint16_t)server.nPort);
				addr.sin_addr.s_addr = server.sIp.empty() ? htonl(INADDR_ANY) : inet_addr(server.sIp.c_str());

				if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0)
					throw core::system_error(errno, "UDP-SERVER(bind)", __PRETTY_FUNCTION__, __LINE__);

				//accept4
			}

			if (!socketPool.emplace(sock, std::pair<uint8_t, std::shared_ptr<cserver::base>>(1, srv)).second || eventPoll.emplace(sock, cepoll::in | cepoll::hup | cepoll::offline) < 0) {
				throw core::system_error(errno); 
				//clientPool.emplace(sock, std::unordered_set<int>());
			}
		}
		catch (core::system_error & er) {
			socketPool.erase(sock);
			::close(sock);
			throw;
		}
	}
	else {
		throw core::system_error(errno, "UDP-SERVER(socket)", __PRETTY_FUNCTION__, __LINE__);
	}

}

uint64_t cserver::gettime() {
	timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	return now.tv_sec * 1000;
}

ssize_t cserver::listen(ssize_t timeout_milisecond) {
	
	auto nresult = eventPoll.wait(eventList, timeout_milisecond);
	if (nresult >= 0) {
		for (ssize_t i = 0; i < nresult; i++) {
			if (auto&& so_it = socketPool.find(eventList[i].data.fd); so_it != socketPool.end()) {
				if (so_it->second.first == 1) {
					if (so_it->second.second->type_id() == ttcp) {
						auto&& srv = so_it->second.second->get<cserver::tcp>();
						/* accept new connection */
						if (eventList[i].events == cepoll::in) {
							sockaddr_storage sa;
							socklen_t sa_len = sizeof(sockaddr_storage);
							if (int soc = accept4(eventList[i].data.fd, (sockaddr*)&sa, &sa_len, SOCK_CLOEXEC | SOCK_NONBLOCK); soc > 0) {
								if (auto&& cl_it = socketPool.find(soc); cl_it != socketPool.end()) {
									eventPoll.remove(soc);
									if (cl_it->second.second->type_id() == ttcp) {
										auto&& cl_srv = cl_it->second.second->get<cserver::tcp>();
										cl_srv.ondisconnect(soc);
									}
									cl_it->second = std::pair<uint8_t, std::shared_ptr<cserver::base>>(2, so_it->second.second);
									eventPoll.emplace(soc, cepoll::in | cepoll::hup | cepoll::offline | cepoll::edge);
									srv.onconnect(soc, sa,gettime());
									//srv.ondata(soc);
								}
								else if (auto&& res = eventPoll.emplace(soc, cepoll::in | cepoll::hup | cepoll::offline | cepoll::edge); res == 0) {
									//clientPool[eventList[i].data.fd].emplace(soc);
									socketPool.emplace(soc, std::pair<uint8_t, std::shared_ptr<cserver::base>>(2, so_it->second.second));
									srv.onconnect(soc, sa,gettime());
								}
								else {
									fprintf(stderr, "server(%s) epoll emplace fail (%s)\n", srv.getname().c_str(), strerror((int)res));
									::close(soc);
								}
							}
							else {
								fprintf(stderr, "server(%s) accept connection fail (%s)\n", srv.getname().c_str(), strerror(errno));
							}
						}
						else {
							throw std::runtime_error("server down");
							/* server is down * /
							srv.onshutdown();
							/ * remove clients and close connection * /
							eventPoll.remove(eventList[i].data.fd);
							::close(eventList[i].data.fd);
							for (auto&& cl_it = clientPool[eventList[i].data.fd].begin(); cl_it != clientPool[eventList[i].data.fd].end(); ++cl_it) {
								socketPool.erase(*cl_it);
								eventPoll.remove(*cl_it);
								srv.onclose(*cl_it);
								::close(*cl_it);
							}
							clientPool.erase(eventList[i].data.fd);
							socketPool.erase(eventList[i].data.fd);
							*/
						}
					}
					else if (so_it->second.second->type_id() == tudp) {
						auto&& srv = so_it->second.second->get<cserver::udp>();
						/* accept new connection */
						if (eventList[i].events == cepoll::in) {
							srv.ondata(eventList[i].data.fd);
						}
						else {
							/* server is down * /
							srv.onshutdown();
							/ * remove clients and close connection * /
							eventPoll.remove(eventList[i].data.fd);
							::close(eventList[i].data.fd);
							socketPool.erase(eventList[i].data.fd);
							*/
							throw std::runtime_error("server down");
						}
					}
					continue;
				}
				else if (so_it->second.first == 2 /* tcp client */) {
					auto&& srv = so_it->second.second->get<cserver::tcp>();
					if (eventList[i].events == cepoll::in) {
						srv.ondata(eventList[i].data.fd);
					}
					else if (eventList[i].events == cepoll::out) {
						srv.onwrite(eventList[i].data.fd);
					}
					else {
						/* client is down */
						eventPoll.remove(eventList[i].data.fd);
						socketPool.erase(eventList[i].data.fd);
						::close(eventList[i].data.fd);
						srv.ondisconnect(eventList[i].data.fd);
						//clientPool[so_it->first].erase(eventList[i].data.fd);
					}
					continue;
				}
				else if (so_it->second.first == 3 /* timer */) {
					auto&& srv = so_it->second.second->get<cserver::tcp>();
					uint64_t time;
					if (::read(so_it->first, &time, sizeof(time)) > 0) {
						srv.onidle(gettime());
					}
					continue;
				}
			}

			fprintf(stderr, "socket(%ld) not in event poll. remove it\n", eventList[i].data.fd);
			eventPoll.remove(eventList[i].data.fd);
			socketPool.erase(eventList[i].data.fd);
			::close(eventList[i].data.fd);
		}
	}
	return nresult;
}


void cserver::shutdown() {
	for (auto&& so_it : socketPool) {
		if (so_it.second.second->type_id() == ttcp) {
			auto&& srv = so_it.second.second->get<cserver::tcp>();
			eventPoll.remove(so_it.first);
			::close(so_it.first);
			if (so_it.second.first == 2) {
				srv.onclose(so_it.first);
			}
			else if (so_it.second.first == 1) {
				srv.onshutdown();
			}
			/*
			for (auto&& cl_it : clientPool[so_it.first]) {
				eventPoll.remove(cl_it);
				::close(cl_it);
				srv.onclose(cl_it);
			}
			*/
		}
		else if (so_it.second.second->type_id() == tudp) {
			auto&& srv = so_it.second.second->get<cserver::udp>();
			eventPoll.remove(so_it.first);
			::close(so_it.first);
			srv.onshutdown();
		}
	}
	eventPoll.shutdown();
}
