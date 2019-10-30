#pragma once
#include "pch.h"
#include "../core/coption.h"
#include "../ci/cstring.h"
#include "channel/cchannel.h"
#include "crequest.h"
#include <condition_variable>
#include <mutex>
#include <vector>
#include <thread>
#include <unordered_map>


namespace fiber {
	class cbroker {
		class path_hash {
			std::hash<std::string> hasher;
		public:
			inline std::size_t operator()(const std::string& key) const {
				auto pos = key.find_first_of("/");
				return hasher({ key.begin(),key.begin() + pos });
			}
		};
		class path_equal {
		public:
			inline bool operator()(const std::string& lkey, const std::string& rkey) const {
				return std::strncmp(lkey.data(), rkey.data(), rkey.length()) == 0;
			}
		};
		std::unordered_multimap<std::string, std::shared_ptr<cchannel>, path_hash, path_equal> listEndpoints;
	public:
		cbroker(const core::coptions& options);
		~cbroker();
		bool emplace(const std::string& endpoint, std::shared_ptr<cchannel>&& queue);
		bool enqueue(std::shared_ptr<fiber::crequest>&& msg);

		bool serialize() { return true; }
		bool unserialize() { return true; }
	};
}