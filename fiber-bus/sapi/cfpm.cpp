#include "cfpm.h"
#include "../core/csignal.h"
#include <unordered_set>

using namespace fiber;

extern volatile int signal_no;

std::unordered_set<pid_t> childs_list;

int cfpm::run(const std::unique_ptr<csapi>& sapi, const core::coptions& options) {

	std::size_t numWorkers = options.at("num-workers","10").number();
	std::size_t enableResurrect = options.at("enable-resurrect", "0").number();
	size_t nWorker = 0;

	printf("fpm-python: %d workers, resurrect (%s)\n", numWorkers, enableResurrect ? "on" : "off");

	do {
		for (; childs_list.size() < numWorkers; nWorker++) {
			switch (pid_t pid = fork(); pid) {
			case 0: /* child */
				core::csignal::reset(SIG_DFL,SIGQUIT, SIGABRT, SIGINT);
				printf("fpm-child-python(%d) child: %ld\n", nWorker, getpid());
				exit(sapi->run());
				break;
			case -1:
				return 2;
			default:
				childs_list.emplace(pid);
				break;
			}
		}
		{
			int status;
			pid_t cpid = -1;

			while (((cpid = waitpid(-1, &status, 0)) > 0) || 1) {
				if (cpid > 0) {
					printf("fpm-child-python: exit, code(%ld),result(%d), signo(%d), pid(%d) \n", WEXITSTATUS(status), WIFEXITED(status), WIFSIGNALED(status) ? WTERMSIG(status) : 0, cpid);
					if (!enableResurrect) continue;
					std::this_thread::sleep_for(std::chrono::seconds(1));
					break;
				}
				else if(cpid == -1 && errno == EINTR && signal_no) {
					printf("shutdown fpm-python\n");
					for (auto pid : childs_list) {
						kill(pid, 3);
						auto result = waitpid(pid, &status, 0);
						printf("fpm-child-python: shutdown(%ld), code(%ld),result(%d), signo(%d), pid(%d) \n", result, WEXITSTATUS(status), WIFEXITED(status), WIFSIGNALED(status) ? WTERMSIG(status) : 0, cpid);
					}
				}
				break;
			}
		}
	} while (!signal_no && enableResurrect);


	return 0;
}