#pragma once
#include "../core/coption.h"
#include <cstring>
#include <unistd.h>
#include <thread>
#include <sys/types.h>
#include <sys/wait.h>
#include <functional>

#include "csapi.h"

namespace fiber {
	class cfpm {
	public:
		static int run(const std::unique_ptr<csapi>& sapi, const core::coptions& options);
	};
}