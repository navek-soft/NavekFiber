#pragma once
#include "cbroker.h"
#include "../core/cthreadpool.h"

namespace fiber {
	class capp {
	public:
		static int run(int argc, char* argv[]);
		static cbroker& broker();
		static core::cthreadpool& threadpool();
	};
}