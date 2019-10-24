#include "cfpm.h"

using namespace fiber;

int cfpm::run(const std::unique_ptr<csapi>& sapi, const core::coptions& options) {

	size_t numWorkers = options.at("fpm-num-workers","10").number();
	size_t enableResurrect = options.at("fpm-enable-resurrect", "0").number();

	return sapi->run();

	do {

		for (size_t nWorker = 0; (nWorker % numWorkers) < numWorkers; nWorker++) {
			switch (pid_t pid = fork(); pid) {
			case 0: /* child */
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
				printf("fpm-child-down: code(%ld),result(%d), signo(%d), pid(%d) \n", WEXITSTATUS(status), WIFEXITED(status), WIFSIGNALED(status) ? WTERMSIG(status) : 0, cpid);
				if (!enableResurrect) continue;
			}
		}
	} while (enableResurrect);
	return 0;
}