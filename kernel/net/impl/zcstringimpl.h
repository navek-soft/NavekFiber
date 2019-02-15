#pragma once
#include <cinttypes>
#include <string>
#include <deque>
#include <utils/ascii.h>
#include <utils/findmatch.h>

namespace Fiber {
	class zcstring : public std::pair<const uint8_t*, const uint8_t*> {
	public:
		enum {
			both, left, right
		};
		zcstring(const uint8_t* _from = nullptr, const uint8_t* _to = nullptr)
			: pair<const uint8_t*, const uint8_t*>(_from, _to) { ; }
		zcstring(std::string& s)
			: pair<const uint8_t*, const uint8_t*>(nullptr, nullptr) { first = (const uint8_t*)s.c_str(); second = (const uint8_t*)s.c_str() + s.length(); }

		zcstring(const zcstring& v) { first = v.first; second = v.second; }
		inline zcstring& operator = (const zcstring& v) { first = v.first; second = v.second; return *this; }

		inline std::string str() const { return std::string(first, second); }

		inline void assign(const uint8_t* _from, const uint8_t* _to) { first = _from; second = _to; }

		inline zcstring trim(size_t at = both) const {
			uint8_t* itb = (uint8_t*)first, *ite = (uint8_t*)second - 1;
			if (at != right) {
				for (; itb < second && isspace(*itb); ++itb) { ; }
			}
			if (at != left) {
				for (; isspace(*ite) && ite >= first; --ite) { ; }
			}
			return { itb ,ite + 1 };
		}
		inline zcstring trim(const std::array<uint8_t,256>& characters, size_t at = both) const {
			uint8_t* itb = (uint8_t*)first, *ite = (uint8_t*)second - 1;
			if (at != right) {
				for (; itb < second && characters[*itb]; ++itb) { ; }
			}
			if (at != left) {
				for (; characters[*itb] && ite >= first; --ite) { ; }
			}
			return { itb ,ite + 1 };
		}

		inline bool empty() const { return first == second; }

		inline std::deque<zcstring> explode(std::string delimiter, ssize_t limit = -1) const {
			const uint8_t*	end = second;
			const uint8_t*	start = first;

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

		inline const uint8_t* begin() const { return first; }
		inline const uint8_t* end() const { return second; }
	};
}