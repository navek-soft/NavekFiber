#pragma once
#include <string>
#include <vector>
#include <set>
#include <algorithm> 
#include <cstdarg>
#include <memory>
#include <stdexcept>

namespace utils {
	static inline std::string& trim(std::string &s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
			return !std::isspace(ch);
		}));
		s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
			return !std::isspace(ch);
		}).base(), s.end());
		return s; 
	}
	static inline std::vector<std::string>& trim(std::vector<std::string>&& list) {
		for (auto& str : list) { trim(str); }
		return list;
	}
	static inline std::string& lowercase(std::string& s) {
		std::transform(s.begin(), s.end(), s.begin(), ::tolower);
		return s;
	}

	static inline std::string& uppercase(std::string& s) {
		std::transform(s.begin(), s.end(), s.begin(), ::toupper);
		return s;
	}

	static std::string format(const std::string&& format, ...) {
		va_list args;
		va_start(args, format);
		size_t size = std::vsnprintf(nullptr, 0, format.c_str(), args) + 1;
		va_end(args);
		std::unique_ptr<char> buf(new char[size]);
		va_start(args, format);
		std::vsnprintf(buf.get(), size, format.c_str(), args);
		va_end(args);
		return std::string(buf.get(), buf.get() + size - 1);
	}

	static std::vector<std::string> explode(const std::string&& delimiter, std::string& string, ssize_t limit = -1) {
		size_t start = 0;
		size_t end = string.find(delimiter);
		std::vector<std::string> list;
		if (!string.empty()) {
			while (--limit != 0 && end != std::string::npos)
			{
				list.emplace_back(string.begin() + start, string.begin() + end);
				start = end + delimiter.length();
				end = string.find(delimiter, start);
			}
			list.emplace_back(string.begin() + start, string.end());
		}

		return list;
	}

	static std::string implode(const std::string&& glue, const std::vector<std::string>& pieces) {
		std::string result;
		auto it = pieces.begin();
		for (size_t i = pieces.size(); --i; it++) {
			result += *it + glue;
		}
		result += *it;
		return result;
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

	template<typename TP = std::string,class OCS = ExpandCastString<std::string>,class OINE=ExpandIsNotEqual,class OPP=ExpandIncrease>
	static std::set<TP> expand(std::string range,const std::string&& sep_chain=",",const std::string&& sep_range="-") {
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
					for (TP f(Cast(r[0])); IsNotEqual(f,t); Opp(f)) { result.emplace(f); }
					result.emplace(t);
				}
			}
		}
		return result;
	}
}