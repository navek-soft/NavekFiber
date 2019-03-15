#pragma once
#include <string>
#include <deque>
#include "findmatch.h"
#include "ascii.h"

class zcstring {
private:
	const uint8_t* _begin;
	const uint8_t* _end;
public:
	enum {
		both, left, right
	};
	zcstring(const uint8_t* _from = nullptr, const uint8_t* _to = nullptr) : _begin(_from), _end(_to){ ; }
	zcstring(std::string& s)
		: _begin(nullptr),_end(nullptr) { _begin = (const uint8_t*)s.c_str(); _end = (const uint8_t*)s.c_str() + s.length(); }

	zcstring(const zcstring& v) { _begin = v._begin; _end = v._end; }
	inline zcstring& operator = (const zcstring& v) { _begin = v._begin; _end = v._end; return *this; }

	inline std::string str() const { return std::string(_begin, _end); }

	inline void assign(const uint8_t* _from, const uint8_t* _to) { _begin = _from; _end = _to; }

	inline zcstring trim(size_t at = both) const {
		uint8_t* itb = (uint8_t*)_begin, *ite = (uint8_t*)_end - 1;
		if (at != right) {
			for (; itb < _end && isspace(*itb); ++itb) { ; }
		}
		if (at != left) {
			for (; isspace(*ite) && ite >= _begin; --ite) { ; }
		}
		return { itb ,ite + 1 };
	}
	inline zcstring trim(const std::array<uint8_t, 256>& characters, size_t at = both) const {
		uint8_t* itb = (uint8_t*)_begin, *ite = (uint8_t*)_end - 1;
		if (at != right) {
			for (; itb < _end && characters[*itb]; ++itb) { ; }
		}
		if (at != left) {
			for (; characters[*itb] && ite >= _begin; --ite) { ; }
		}
		return { itb ,ite + 1 };
	}

	inline size_t length() const { return _end - _begin; }

	inline bool empty() const { return _begin == _end; }

	inline const uint8_t* data() const { return _begin; }
	inline const char* c_str() const { return (const char*)_begin; }

	inline std::deque<zcstring> explode(std::string delimiter, ssize_t limit = -1) const {
		const uint8_t*	end = _end;
		const uint8_t*	start = _begin;

		std::deque<zcstring> parts;
		for (auto&& part = utils::find(start, end, delimiter.c_str());
			(--limit != 0) && part.result();
			start = (const uint8_t*)part.begin() + delimiter.length(), part = utils::find(start, end, delimiter.c_str()))
		{
			parts.emplace_back(start, (const uint8_t*)part.begin());
		}
		if (start < end) {
			parts.emplace_back(start, end);
		}
		return parts;
	}

	inline const uint8_t* begin() const { return _begin; }
	inline const uint8_t* end() const { return _end; }
};

namespace utils {
	class zcstring_ifasthash {
	public:
		inline size_t operator()(const zcstring& v) const {
			size_t hash = 0;
			auto it = v.begin();
			for (size_t n = sizeof(size_t) > v.length() ? v.length() : sizeof(size_t); n--; it++) {
				hash = (hash | utils::icase_ascii_table[*it]) << 8;
			}
			return hash;
		}
	};
	class zcstring_iequal {
	public:
		inline bool operator()(const zcstring& a, const zcstring& b) const {
			if (a.length() == b.length()) {
				if (a.length()) {
					auto it_a = a.begin();
					auto it_b = b.begin();
					bool is_equal = false;
					for (size_t n = a.length(); n-- && (is_equal = utils::icase_ascii_table[*it_a] == utils::icase_ascii_table[*it_b]); it_a++, it_b++) { ; }
					return is_equal;
				}
				return true;
			}
			return false;
		}
	};
}