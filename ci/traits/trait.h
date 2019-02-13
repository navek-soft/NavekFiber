#pragma once
#include <tuple>

template<typename ... ARGS>
class trait : std::tuple<ARGS...> {
public:
	trait(ARGS&& ... args) : std::tuple(std::forward<ARGS&&>(args)...) { ; }
};