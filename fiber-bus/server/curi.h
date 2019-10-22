#pragma once
#include "../ci/cstring.h"
#include <deque>
#include <unordered_map>

namespace fiber {
	class curi {
		std::string					spath;
		std::deque<ci::cstringview>	lpath;
		std::unordered_map<ci::cstringview, ci::cstringview>	largs;
	public:
		curi(const std::string& uri) : spath(uri) {
			ci::cstringview split(&spath.front(), &spath.back());
			
			auto&& path_args = split.trim('/').explode('?', 1);
		}
	};

}