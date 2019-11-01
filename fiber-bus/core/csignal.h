#pragma once
#include <csignal>
#include <vector>
#include <cstring>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include "cexcept.h"
#include <deque>
#include <functional>

namespace core {
	class csignal {
	public:
		using handler = void (*) (int, siginfo_t*, void*);
		template<typename ... SIG_LIST>
		static inline void action(handler fn, SIG_LIST...signs) {
			int		signals_list[]{ signs... };
			size_t	sigals_count = sizeof...(signs);
			struct sigaction act;
			sigset_t   set;

			sigemptyset(&set);

			memset(&act, 0, sizeof(act));

			for (size_t i = 0; i < sigals_count; i++) {
				sigaddset(&set, signals_list[i]);
			}
			act.sa_sigaction = fn;
			act.sa_mask = set;
			act.sa_flags |= SA_SIGINFO;

			for (size_t i = 0; i < sigals_count; i++) {
				sigaction(signals_list[i], &act, 0);
			}
		}
		template<typename ... SIG_LIST>
		static inline void reset(sighandler_t handler,SIG_LIST...signs) {
			int		signals_list[]{ signs... };
			size_t	sigals_count = sizeof...(signs);
			for (size_t i = 0; i < sigals_count; i++) {
				std::signal(signals_list[i], handler);
			}
		}
	private:
		int			epollfd{-1};
		int			sigfd{-1};
		std::vector<struct epoll_event> epolllist{ 20 };
		sigset_t	signals_set;
	public:

		using siginfo = struct signalfd_siginfo;

		template<typename ... SIG_LIST>
		csignal(SIG_LIST...signs) {
			int		signals_list[]{ signs... };
			size_t	sigals_count = sizeof...(signs);

			if (epollfd = epoll_create1(EPOLL_CLOEXEC); epollfd <= 0) { throw core::system_error(errno); }

			sigemptyset(&signals_set);
			for (size_t i = 0; i < sigals_count; i++) {
				sigaddset(&signals_set, signals_list[i]);
			}

			/* Block signals so that they aren't handled
			   according to their default dispositions */

			if (sigprocmask(SIG_BLOCK, &signals_set, NULL) == -1) {
				core::system_error(errno, __PRETTY_FUNCTION__, __LINE__);
			}
				

			struct epoll_event event;
			event.events = EPOLLIN;
			event.data.fd = sigfd = signalfd(-1, &signals_set, SFD_CLOEXEC | SFD_NONBLOCK);
			
			if (event.data.fd == -1 || epoll_ctl(epollfd, EPOLL_CTL_ADD, event.data.fd, &event) == -1) {
				core::system_error(errno, __PRETTY_FUNCTION__, __LINE__);
			}

		}

		inline ssize_t wait(std::function<void(csignal::siginfo & fdsi)> fn, size_t timeout = 0) {
			int nevents = -1;
			if (nevents = epoll_wait(epollfd, (struct epoll_event*)epolllist.data(), (int)epolllist.size(), (int)timeout); nevents >= 0) {
				for (int n = 0; n < nevents; n++) {
					csignal::siginfo fdsi;
					if (read(epolllist[n].data.fd, &fdsi, sizeof(csignal::siginfo)) == sizeof(csignal::siginfo)) {
						fn(fdsi);
					}
				}
			}

			return nevents;
		}
	};
}