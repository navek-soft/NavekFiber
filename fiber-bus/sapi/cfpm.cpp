#include "cfpm.h"

using namespace fiber;

int cfpm::run(const std::unique_ptr<csapi>& sapi, const core::coptions& options) {

	std::size_t numWorkers = options.at("fpm-num-workers","10").number();
	std::size_t enableResurrect = options.at("fpm-enable-resurrect", "0").number();

	printf("fpm-python: %d workers, resurrect (%s)\n", numWorkers, enableResurrect ? "on" : "off");
	std::size_t nWorker = 0;
	do {

		for (; nWorker < numWorkers; nWorker++) {
			switch (pid_t pid = fork(); pid) {
			case 0: /* child */
				printf("fpm-child-python: %d (%ld)\n", nWorker, getpid());
				return sapi->run();
			case -1:
				return 2;
			default:
				break;
			}
		}
		{
			int status;
			pid_t cpid = -1;

			while ((cpid = waitpid(-1, &status, 0)) >= 0) {
				printf("fpm-child-python: exit, code(%ld),result(%d), signo(%d), pid(%d) \n", WEXITSTATUS(status), WIFEXITED(status), WIFSIGNALED(status) ? WTERMSIG(status) : 0, cpid);
				nWorker--;
				if (!enableResurrect) continue;
				std::this_thread::sleep_for(std::chrono::seconds(1));
				break;
			}
		}
	} while (enableResurrect);
	return 0;
}