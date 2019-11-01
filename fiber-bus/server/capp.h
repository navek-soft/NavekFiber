#pragma once
#include "cbroker.h"
#include "../core/cthreadpool.h"
#include "../core/coption.h"
#include "crequest.h"
#include <memory>

namespace fiber {
	class capp {
	private:
		static inline std::string prog_name, prog_pwd, prog_cwd;
		static inline std::size_t opt_verbose;
	public:
		static inline std::size_t verbose() { return opt_verbose; }
		static int run(int argc, char* argv[]);
		/* dispatch incomming request */
		static void dispatch(const std::shared_ptr<fiber::crequest>& msg);

		/* execute sapi request */
		static ssize_t execute(const cmsgid& msg_id, const std::string& sapi, const std::string& execscript, std::size_t task_limit, std::size_t task_timeout,const std::shared_ptr<fiber::crequest>&& msg);

		static std::weak_ptr<core::cthreadpool> threadpool();
	};
}