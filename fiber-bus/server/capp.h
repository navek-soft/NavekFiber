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
		static ssize_t execute(const cmsgid& msg_id, const std::string& sapi, const std::string& execscript, std::size_t task_limit, std::size_t task_timeout,const std::shared_ptr<fiber::crequest>&& msg);

		static std::weak_ptr<core::cthreadpool> threadpool();

	};
}