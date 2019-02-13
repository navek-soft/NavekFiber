#pragma once

namespace trait {
	template<typename T>
	class iterable {
		using iterator = typename T::const_iterator;
		iterator from, to;
	public:
		iterable(const T&& o) : from(o.begin()), to(o.end()) { ; }
		iterable(const iterator&& b, const iterator&& e) : from(move(b)), to(move(e)) { ; }
		iterable(const iterable&& rit) : from(rit.from), to(rit.to) { ; }
		inline iterator begin() { return from; }
		inline iterator end() { return to; }
	};
}