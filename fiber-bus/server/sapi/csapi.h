#pragma once
#include "../crequest.h"
#include "../../core/coption.h"
#include <memory>

namespace fiber {
	class csapi {
	public:
		virtual ~csapi() { ; }
		virtual void Execute(const cmsgid&& msg_id, const std::string&& path, const std::shared_ptr<fiber::crequest>& request) { ; }
	};
}
