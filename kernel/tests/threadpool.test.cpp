#include <cstdio>
#define DOM_TRACE
#include "../main/kernel.h"
#include "../mt/threadpool.h"
#include <csignal>
#include <chrono>

using namespace std::chrono_literals;

static sig_atomic_t raise_signal = 0;

class CTask : public Dom::Client::Embedded<CTask, IThreadJob> {
public:
	CTask() { ; }
	virtual ~CTask() { ; }
	inline virtual void Exec(const std::atomic_bool&  DoWork) {
		std::this_thread::sleep_for(30s);
	}
	CLSID(CTask)
};

int main(int argc, char* argv[]) {

	try {

		auto shandler = [](int sig) {
			extern sig_atomic_t raise_signal;
			raise_signal = sig;
		};

		::signal(SIGINT, shandler);
		::signal(SIGQUIT, shandler);
		::signal(SIGTERM, shandler);
		::signal(SIGSEGV, shandler);
		::signal(SIGUSR1, shandler);
		::signal(SIGUSR2, shandler);


		Core::ThreadPool pool("10", "50", "2,3-8", "1,3,7");


		size_t iter = 1, PoolWorkers = 0, PoolTasks, PoolBusyAnts = 0;
		do {
			std::this_thread::sleep_for(1s);
			if (raise_signal == SIGUSR1) {
				printf("SIG: %ld = %ld\n", raise_signal, iter);
				raise_signal = 0;
				pool.Enqueue([](size_t num) {
					printf("EXEC: %ld = %ld\n", std::this_thread::get_id(), num);
					std::this_thread::sleep_for(30s);
				}, iter++);
			}
			if (raise_signal == SIGUSR2) {
				printf("SIG: %ld = %ld\n", raise_signal, iter);
				raise_signal = 0;
				pool.Enqueue(*(new CTask));
			}
			pool.GetState(PoolWorkers, PoolTasks, PoolBusyAnts);
			printf("THREADPOOL: Workers: %ld/%ld, Tasks: %ld\n", PoolWorkers, PoolBusyAnts, PoolTasks);

		} while (raise_signal == 0);

		pool.Join();
	}
	catch (std::exception& ex) {
		printf("%s\n", ex.what());
	}

	return 0;
}