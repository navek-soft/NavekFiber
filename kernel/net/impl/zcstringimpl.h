#pragma once
#include <cinttypes>
#include <string>
#include <utils/ascii.h>

namespace Fiber {
	class zcstring : public std::pair<const uint8_t*, const uint8_t*> {
	public:
		class equal {
		public:
			inline bool operator()(const zcstring& _str1, const zcstring& _str2) const {
				size_t len1 = _str1.second - _str1.first, len2 = _str2.second - _str2.first;
				bool result = true;
				for (uint8_t* it1 = (uint8_t*)_str1.first, *it2 = (uint8_t*)_str2.first;
					len1 && len2 && (result = utils::icase_ascii_table[*it1] == utils::icase_ascii_table[*it2]);
					++it1, ++it2, --len1, --len2) {
					;
				}
				return result && (len1 == len2);
			}
		};
		class hash {
		public:
			inline size_t operator()(const zcstring& _str) const {
				return algo::murmurhash(_str.first, _str.second - _str.first);
			}
		};
	public:
		enum {
			both, left, right
		};
		zcstring(const uint8_t* _from = nullptr, const uint8_t* _to = nullptr)
			: pair<const uint8_t*, const uint8_t*>(_from, _to) { ; }
		zcstring(string& s)
			: pair<const uint8_t*, const uint8_t*>(nullptr, nullptr) { first = (const uint8_t*)s.c_str(); second = (const uint8_t*)s.c_str() + s.length(); }

		zcstring(const zcstring& v) { first = v.first; second = v.second; }
		inline zcstring& operator = (const zcstring& v) { first = v.first; second = v.second; return *this; }

		inline string str() const { return string(first, second); }

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
		inline zcstring trim(const char* character_mask, size_t at = both) const {
			uint8_t alphabet[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, };
			for (uint8_t* ch = (uint8_t*)character_mask; *ch; ++ch) { alphabet[(*ch) >> 3] |= uint8_t(0x80 >> (*ch & 7)); }
			uint8_t* itb = (uint8_t*)first, *ite = (uint8_t*)second - 1;
			if (at != right) {
				for (; itb < second && (alphabet[(*itb) >> 3] & uint8_t(0x80 >> (*itb & 7))); ++itb) { ; }
			}
			if (at != left) {
				for (; (alphabet[(*ite) >> 3] & uint8_t(0x80 >> (*ite & 7))) && ite >= first; --ite) { ; }
			}
			return { itb ,ite + 1 };
		}

		inline bool empty() const { return first == second; }

		inline vector<zcstring> explode(std::string delimiter, ssize_t limit = -1) const {
			const uint8_t*	end = second;
			const uint8_t*	start = first;

			vector<zcstring> parts;
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