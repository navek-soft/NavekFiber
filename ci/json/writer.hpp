#pragma once
#include <string>
#include <deque>

class json {
	static inline std::string toString(const std::deque<std::string>& val) {
		std::string result("[");
		if (!val.empty()) {
			for (ssize_t n = 0; n < (ssize_t)val.size() - 1; n++) {
				result.append(val[n]).append(",");
			}
			result.append(val.back());
		}

		result.append("]");
		return result;
	}
	static inline std::string toString(const std::deque<std::pair<std::string, std::string>>& val) {
		std::string result("{");
		if (!val.empty()) {
			for (ssize_t n = 0; n < (ssize_t)val.size() - 1; n++) {
				result.append("\"").append(val[n].first).append("\":").append(val[n].second).append(",");
			}
			result.append("\"").append(val.back().first).append("\":").append(val.back().second);
		}
		result.append("}");
		return result;
	}
	static inline std::string toString(const std::string& val) { std::string result("'"); return result.append(val).append("'"); }
	static inline std::string toString(const char* val) { std::string result("'"); return result.append(val).append("'"); }
	static inline std::string toString(const size_t& val) { return std::to_string(val); }
	static inline std::string toString(const int& val) { return std::to_string(val); }

	class cstring {
		std::string el_value;
	public:
		template<typename T>
		cstring(T&& t) : el_value(toString(t)) { ; }
		inline std::string str() const { return el_value; }
	};

public:
	static inline std::deque<std::pair<std::string, std::string>> object(const std::deque<std::pair<std::string, cstring>>&& vals) {
		std::deque<std::pair<std::string, std::string>> result;
		for (auto&& r : vals) {
			result.emplace_back(r.first, r.second.str());
		}
		return result;
	}
	template<typename ... ELS>
	static inline std::deque<std::string> array(ELS&& ... els) {
		return std::deque<std::string>({ toString(els)... });
	}

	template<typename ... ELS>
	static inline std::string string(ELS&& ... els) {
		std::deque<std::string> el_list({ toString(els)... });
		std::string result;
		for (ssize_t n = 0; n < (ssize_t)el_list.size() - 1; n++) {
			result.append(el_list[n]).append(",");
		}
		result.append(el_list.back());
		return result;
	}
};