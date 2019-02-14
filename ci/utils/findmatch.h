#pragma once
#include <cinttypes>
#include <tuple>
#include <cctype>
#include "ascii.h"

namespace utils {
	using namespace std;
	

	struct match_result : tuple<size_t, const char*, const char*> {
	public:
		match_result(size_t res = 0, const char* b = nullptr, const char* e = nullptr) { get<0>(*this) = res; get<1>(*this) = b;	get<2>(*this) = e; }
		inline size_t result() { return get<0>(*this); }
		inline const char* begin() { return get<1>(*this); }
		inline const char* end() { return get<2>(*this); }

		operator bool() { return get<0>(*this) != 0; }
	};


	template<typename ... needle>
	inline match_result match(const uint8_t* haystack_from, const uint8_t* haystack_to, needle&& ... values) {
		static const uint8_t* needle_values[] = { (const uint8_t*)values ... };
		const size_t needle_size = sizeof(needle_values) / sizeof(const uint8_t*);
		const uint8_t*  needle_it = (const uint8_t*)needle_values[0];
		const uint8_t*	 haystack_it = haystack_from;

		bool match = false;
		const uint8_t** needle_list = (const uint8_t**)&needle_values[0];
		size_t needle_index = needle_size;

		if (haystack_from == nullptr || haystack_to == nullptr) return {};

		for (; needle_index && !(match = *needle_it == '\0') && haystack_it < haystack_to; ) {
			if (icase_ascii_table[*(haystack_it)] != icase_ascii_table[*(needle_it)]) {
				needle_index--;
				needle_it = *(++needle_list);
				haystack_it = haystack_from;
				continue;
			}
			needle_it++;
			haystack_it++;
		}
		if (match) {
			return { 1 + needle_size - needle_index,  (const char*)haystack_from,(const char*)haystack_it };
		}
		return { };
	}

	template<typename ... needle>
	inline match_result match(const string& haystack, needle&& ... values) {
		return match((const uint8_t*)(&haystack.front()), (const uint8_t*)(&haystack.back() + 1), std::forward<needle>(values)...);
	}

	template<typename ... needle>
	inline match_result find(const uint8_t* haystack_from, const uint8_t* haystack_to, needle&& ... values) {
		static const uint8_t* needle_values[] = { (const uint8_t*)values ... };
		const size_t needle_size = sizeof(needle_values) / sizeof(const uint8_t*);
		const uint8_t* needle_it = needle_values[0];
		const uint8_t*	 haystack_it = haystack_from;

		const uint8_t* n_it = nullptr, *h_it = nullptr;
		bool match = false;

		if (haystack_from == nullptr || haystack_to == nullptr) return {};

		for (size_t needle_index = 0; haystack_it < haystack_to; needle_it = needle_values[needle_index % needle_size], !(needle_index % needle_size) && haystack_it++) {
			for (n_it = needle_it, h_it = haystack_it;
				h_it < haystack_to && icase_ascii_table[*(h_it)] == icase_ascii_table[*(n_it)] && !(match = (*(n_it + 1) == (uint8_t)'\0')); ++n_it, ++h_it) {
			}
			if (match) {
				return { 1 + (needle_index % needle_size),  (const char*)haystack_it , (const char*)h_it + 1 };
			}
			needle_index++;
		}
		return {};
	}

	inline match_result find_char(const uint8_t* haystack_from, const uint8_t* haystack_to, const char* charlist) {
		const uint8_t* it = haystack_from;
		for (const uint8_t* ch = (const uint8_t*)charlist;
			it < haystack_to;
			ch++, it += (*ch == '\0'), ch = *ch != '\0' ? ch : (const uint8_t*)charlist) {
			if (*it == *ch) {
				return { (size_t)(((const uint8_t*)charlist - ch)) + 1,(const char*)haystack_from,(const char*)it + 1 };
			}
		}
		return { 0,(const char*)haystack_from,(const char*)it };
	}
}