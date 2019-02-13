#pragma once
#include <unistd.h>
#include <limits.h>
#include <string>

namespace sys {

	static inline std::string cwd() {
		static char cwd[PATH_MAX + 1] = { '\0' };
		if (!cwd[0]) { auto&& len = std::strlen(getcwd(cwd, sizeof(cwd))); cwd[len] = '/'; cwd[len + 1] = '\0'; }
		return cwd;
	}
}