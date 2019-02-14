#pragma once
#include <functional>
#include <csignal>
#include <memory>

namespace sys {
	class signals {
	public:
		template<typename ... SGNLS>
		static inline void handle(__sighandler_t&& fn, SGNLS ... sgnls) {
			for (auto sig : std::vector<int>({ sgnls... })) {
				if (sig >= 0 && sig < _NSIG) {
					::signal(sig, fn);
				}
			}
		}
		static inline void ign(int sig) { ::signal(sig,SIG_IGN); }
		static inline void dfl(int sig) { ::signal(sig, SIG_DFL); }
	};
}