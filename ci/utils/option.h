#pragma once
#include <string>
#include <unordered_map>
#include <set>
#include <regex>
#include "transform.h"

namespace utils {
	namespace option {
		static inline const std::string get_value(const std::unordered_map<std::string, std::string>& options, const std::string& option, const std::string& def_value = "") {
			auto&& it = options.find(option);
			return it != options.end() ? it->second : def_value;
		}

		static inline const size_t get_bytes(const std::string& value, const size_t& def_value) {
			static const std::regex re(R"(\s*(\d+)\s*(G|M|K|B)?)");
			std::smatch match;
			if (!value.empty() && std::regex_search(value, match, re) && match.size() > 1) {
				auto&& units = match.str(2);
				return std::stoul(match.str(1)) << (units == "G" ? 30 : (units == "M" ? 20 : (units == "K" ? 10 : 0)));
			}
			return def_value;
		}

		static inline const size_t get_bytes(const std::unordered_map<std::string, std::string>& options, const std::string& option, const size_t& def_value) {
			return get_bytes(get_value(options, option, ""), def_value);
		}

		class ExpandIsNotEqual {
		public:
			template<typename T> bool operator ()(const T& a, const T& b) const { return a != b; }
		};

		class ExpandIncrease {
		public:
			template<typename T> void operator ()(T& a) const { ++a; }
		};

		template<class T>
		class ExpandCastString {
		public:
			typename std::enable_if<std::is_integral<T>::value, T>::type operator ()(const std::string& from) const { return from; }
		};

		template<typename TP = std::string, class OCS = ExpandCastString<std::string>, class OINE = ExpandIsNotEqual, class OPP = ExpandIncrease>
		static std::set<TP> expand(std::string range, const std::string&& sep_chain = ",", const std::string&& sep_range = "-") {
			std::set <TP> result;
			OINE IsNotEqual;
			OPP Opp;
			OCS Cast;

			if (!range.empty()) {
				auto chain = trim(move(explode(move(sep_chain), range)));
				for (auto&& p : chain) {
					auto r = trim(move(explode(move(sep_range), p)));
					if (r.size() == 1) {
						result.emplace(Cast(r[0]));
					}
					else if (r.size() == 2) {
						TP t(Cast(r[1].empty() ? r[0] : r[1]));
						for (TP f(Cast(r[0])); IsNotEqual(f, t); Opp(f)) { result.emplace(f); }
						result.emplace(t);
					}
				}
			}
			return result;
		}

		static inline size_t time_period(const std::string& value) {
			static const std::regex re(R"((?:\s*(\d+)\s*h)?(?:\s*(\d+)\s*m)?(?:\s*(\d+)\s*s)?)");
			std::smatch match;
			if (!value.empty() && std::regex_search(value, match, re) && match.size() > 1) {
				auto&& h = match.str(1);
				auto&& m = match.str(2);
				auto&& s = match.str(3);
				return (h.empty() ? 0 : std::stol(h) * 3600) + (m.empty() ? 0 : std::stol(m) * 60) + (s.empty() ? 0 : std::stol(s));
			}
			return 0;
		}
	}
}