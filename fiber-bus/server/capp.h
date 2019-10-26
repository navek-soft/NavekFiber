#pragma once
#include "cbroker.h"
#include "../core/cthreadpool.h"
#include "crequest.h"
#include <memory>

namespace fiber {
	class capp {
	public:
		static int run(int argc, char* argv[]);
		/* dispatch incomming request */
		static void dispatch(std::shared_ptr<fiber::crequest>&& msg);

		/* execute sapi request */
		static void execute(cmsgid& msg_id, const std::string& sapi, const std::string& execscript, size_t task_limit, size_t task_timeout, std::shared_ptr<fiber::crequest>&& msg);

		static std::weak_ptr<core::cthreadpool> threadpool();
	};
}