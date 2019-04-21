#pragma once
#include <chrono>
#include <cinttypes>

namespace utils {

	using microseconds = int64_t;

	static inline microseconds timestamp() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
}