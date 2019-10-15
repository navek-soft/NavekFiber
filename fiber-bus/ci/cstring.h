#pragma once
#include <cinttypes>
#include <string>
#include <deque>
#include <list>
#include <cstdarg>
#include <cstring>

class cstringview
{
	using tp = uint8_t;
	tp* _begin{ nullptr }, * _end{ nullptr };
public:

	enum trim {
		both, left, right
	};

	cstringview() { ; }
	template<typename T>
	cstringview(const T* from, const T* to) : _begin((tp*)(from)), _end((tp*)(to)) { ; }
	template<typename T>
	cstringview(const T* str,std::size_t len) : _begin((tp*)(str)), _end((tp*)(str) +len) { ; }
	cstringview(const std::string& str) : _begin((tp*)(str.c_str())), _end((tp*)(str.c_str())+str.length()) { ; }
	cstringview(const cstringview& c) : _begin(c._begin), _end(c._end) { ; }
	~cstringview() { _begin = nullptr; _end = nullptr; }
	cstringview& operator = (const cstringview& c) { _begin = c._begin; _end = c._end; return *this; }

	inline size_t length() const { return _end - _begin; }
	inline bool empty() const { return _end == _begin; }

	inline tp* begin() const { return _begin; }
	inline tp* end() const { return _end; }
	inline tp* data() const { return _begin; }
	template<typename T>
	inline const T& cast() const { return *(T*)_begin; }
	template<typename T>
	inline const T& cast(ssize_t offset) const { return *(T*)(_begin + offset); }
	inline std::string str() const { return std::string(_begin, _end); }

	template<typename T>
	inline void assign(const T* _from, const T* _to) { _begin = ((tp*)(_from)); _end = ((tp*)(_to)); }

	inline cstringview trim(cstringview::trim at = trim::both) const {
		tp* itb = _begin, *ite = _end - 1;
		if (at != right) {
			for (; itb < _end && isspace(*itb); ++itb) { ; }
		}
		if (at != left) {
			for (; isspace(*ite) && ite >= _begin; --ite) { ; }
		}
		return { itb ,ite + 1 };
	}
	static inline cstringview find_next(const char symbol, const cstringview& str) {
		tp* start = str._begin;
		for (; start < str._end; ++start) { 
			if (((*start) == symbol)) {
				return { str._begin, start };
			}
		}
		return { str._end,str._end };
	}
	static inline cstringview find_next_space(const cstringview& str) {
		tp* start = str._begin;
		for (; start < str._end; ++start) {
			if (isspace(*start)) {
				return { str._begin, start };
			}
		}
		return { str._end,str._end };
	}
	inline std::deque<cstringview> explode(const char delimiter, ssize_t limit = -1) const {
		tp *start = _begin;
		std::deque<cstringview> parts;
		for (tp* it = _begin; it < _end; it++) {
			if (*it != delimiter) continue;
			parts.emplace_back(start, it + 1);
			start = it + 1;
			if (--limit == 0) break;
		}
		if (start < _end) {
			parts.emplace_back(start, _end);
		}
		return parts;
	}
public:
	constexpr static inline uint8_t icase_ascii_table[256] = {
		0x00 /* */,0x01 /* */,0x02 /* */,0x03 /* */,0x04 /* */,0x05 /* */,0x06 /* */,0x07 /* */,0x08 /* */,0x09 /* */,0x0a /* */,0x0b /* */,0x0c /* */,0x0d /* */,0x0e /* */,0x0f /* */,
		0x10 /* */,0x11 /* */,0x12 /* */,0x13 /* */,0x14 /* */,0x15 /* */,0x16 /* */,0x17 /* */,0x18 /* */,0x19 /* */,0x1a /* */,0x1b /* */,0x1c /* */,0x1d /* */,0x1e /* */,0x1f /* */,
		0x20 /* */,0x21 /* */,0x22 /* */,0x23 /* */,0x24 /* */,0x25 /* */,0x26 /* */,0x27 /* */,0x28 /* */,0x29 /* */,0x2a /* */,0x2b /* */,0x2c /* */,0x2d /* */,0x2e /* */,0x2f /* */,
		0x30 /*0*/,0x31 /*1*/,0x32 /*2*/,0x33 /*3*/,0x34 /*4*/,0x35 /*5*/,0x36 /*6*/,0x37 /*7*/,0x38 /*8*/,0x39 /*9*/,0x3a /* */,0x3b /* */,0x3c /* */,0x3d /* */,0x3e /* */,0x3f /* */,
		0x40 /* */,0x41 /*A*/,0x42 /*B*/,0x43 /*C*/,0x44 /*D*/,0x45 /*E*/,0x46 /*F*/,0x47 /*G*/,0x48 /*H*/,0x49 /*I*/,0x4a /*J*/,0x4b /*K*/,0x4c /*L*/,0x4d /*M*/,0x4e /*N*/,0x4f /*O*/,
		0x50 /*P*/,0x51 /*Q*/,0x52 /*R*/,0x53 /*S*/,0x54 /*T*/,0x55 /*U*/,0x56 /*V*/,0x57 /*W*/,0x58 /*X*/,0x59 /*Y*/,0x5a /*Z*/,0x5b /* */,0x5c /* */,0x5d /* */,0x5e /* */,0x5f /* */,
		0x60 /* */,0x41 /*A*/,0x42 /*B*/,0x43 /*C*/,0x44 /*D*/,0x45 /*E*/,0x46 /*F*/,0x47 /*G*/,0x48 /*H*/,0x49 /*I*/,0x4a /*J*/,0x4b /*K*/,0x4c /*L*/,0x4d /*M*/,0x4e /*N*/,0x4f /*O*/,
		0x50 /*P*/,0x51 /*Q*/,0x52 /*R*/,0x53 /*S*/,0x54 /*T*/,0x55 /*U*/,0x56 /*V*/,0x57 /*W*/,0x58 /*X*/,0x59 /*Y*/,0x5a /*Z*/,0x7b /* */,0x7c /* */,0x7d /* */,0x7e /* */,0x7f /* */,
		0x80 /* */,0x81 /* */,0x82 /* */,0x83 /* */,0x84 /* */,0x85 /* */,0x86 /* */,0x87 /* */,0x88 /* */,0x89 /* */,0x8a /* */,0x8b /* */,0x8c /* */,0x8d /* */,0x8e /* */,0x8f /* */,
		0x90 /* */,0x91 /* */,0x92 /* */,0x93 /* */,0x94 /* */,0x95 /* */,0x96 /* */,0x97 /* */,0x98 /* */,0x99 /* */,0x9a /* */,0x9b /* */,0x9c /* */,0x9d /* */,0x9e /* */,0x9f /* */,
		0xa0 /* */,0xa1 /* */,0xa2 /* */,0xa3 /* */,0xa4 /* */,0xa5 /* */,0xa6 /* */,0xa7 /* */,0xa8 /* */,0xa9 /* */,0xaa /* */,0xab /* */,0xac /* */,0xad /* */,0xae /* */,0xaf /* */,
		0xb0 /* */,0xb1 /* */,0xb2 /* */,0xb3 /* */,0xb4 /* */,0xb5 /* */,0xb6 /* */,0xb7 /* */,0xb8 /* */,0xb9 /* */,0xba /* */,0xbb /* */,0xbc /* */,0xbd /* */,0xbe /* */,0xbf /* */,
		0xc0 /* */,0xc1 /* */,0xc2 /* */,0xc3 /* */,0xc4 /* */,0xc5 /* */,0xc6 /* */,0xc7 /* */,0xc8 /* */,0xc9 /* */,0xca /* */,0xcb /* */,0xcc /* */,0xcd /* */,0xce /* */,0xcf /* */,
		0xd0 /* */,0xd1 /* */,0xd2 /* */,0xd3 /* */,0xd4 /* */,0xd5 /* */,0xd6 /* */,0xd7 /* */,0xd8 /* */,0xd9 /* */,0xda /* */,0xdb /* */,0xdc /* */,0xdd /* */,0xde /* */,0xdf /* */,
		0xe0 /* */,0xe1 /* */,0xe2 /* */,0xe3 /* */,0xe4 /* */,0xe5 /* */,0xe6 /* */,0xe7 /* */,0xe8 /* */,0xe9 /* */,0xea /* */,0xeb /* */,0xec /* */,0xed /* */,0xee /* */,0xef /* */,
		0xf0 /* */,0xf1 /* */,0xf2 /* */,0xf3 /* */,0xf4 /* */,0xf5 /* */,0xf6 /* */,0xf7 /* */,0xf8 /* */,0xf9 /* */,0xfa /* */,0xfb /* */,0xfc /* */,0xfd /* */,0xfe /* */,0xff /* */,
	};
};

class cstringformat {
	ssize_t _length{ 0 };
	mutable std::list<cstringview> _pieces;
public:
	using iterator = typename std::list<cstringview>::const_iterator;

	cstringformat() { ; }
	~cstringformat() { clear(); }
	inline void clear() { _length = 0; for (auto&& p : _pieces) { delete[] p.begin(); } }
	inline size_t append(const char* format, ...) {
		va_list args;
		va_start(args, format);
		size_t size = std::vsnprintf(nullptr, 0, format, args);
		va_end(args);
		if (size) {
			auto buffer = new uint8_t[size + 1];
			va_start(args, format);
			std::vsnprintf((char*)buffer, size + 1, format, args);
			va_end(args);
			_pieces.emplace_back(cstringview( buffer,size ));
		}
		_length += size;
		return size;
	}
	inline size_t append(const std::string& v) {
		if (!v.empty()) {
			auto buffer = new uint8_t[v.length()];
			memcpy(buffer, v.data(), v.length());
			_pieces.emplace_back(cstringview(buffer, v.length()));
		}
		_length += v.length();
		return v.length();
	}
	inline size_t append(const uint8_t* data,size_t size) {
		if (size) {
			auto buffer = new uint8_t[size];
			memcpy(buffer, data, size);
			_pieces.emplace_back(cstringview(buffer, size));
			_length += size;
		}
		return size;
	}
	inline void append(const cstringformat& fmt) {
		while (!fmt._pieces.empty()) {
			_length += fmt._pieces.front().length();
			_pieces.emplace_back(fmt._pieces.front());
			fmt._pieces.pop_front();
		}
	}
	inline iterator begin() const { return _pieces.cbegin(); }
	inline iterator end() const { return _pieces.cend(); }

	inline bool empty() const { return _pieces.empty(); }
	inline ssize_t length() const { return _length; }
};

namespace std {
	template<>
	class hash<cstringview> {
	public:
		inline size_t operator()(const cstringview& s) const
		{
			uint64_t hash = 0x5bd1e9955bd1e995ull;
			size_t len = s.length();
			for (uint8_t* phash = (uint8_t*)&hash,*pstr=s.data(); len--; pstr++) {
				phash[len % sizeof(uint64_t)] ^= cstringview::icase_ascii_table[*pstr];
			}
			return hash;
		}
	};
	template<>
	class equal_to<cstringview> {
	public:
		inline bool operator()(const cstringview& a, const cstringview& b) const 
		{
			if (a.length() != b.length()) return false;
			if (size_t len = a.length(); len) {
				for (uint8_t* p_a = a.data(), *p_b = b.data(); len--; ++p_a, ++p_b) {
					if (cstringview::icase_ascii_table[*p_a] != cstringview::icase_ascii_table[*p_b]) { return false; }
				}
			}
			return true;
		}
	};
}