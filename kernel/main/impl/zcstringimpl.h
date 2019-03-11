#pragma once
#include "interface/IString.h"
#include <dom.h>

namespace Fiber {
	class ZCString : Dom::Client::Embedded<ZCString> {
	private:
		const uint8_t* cfrom, *cto;
	public:
		ZCString(const uint8_t* from = nullptr, const uint8_t* to = nullptr) : cfrom(from), cto(to) { ; }
		virtual inline const char* GetData() const { return (const char*)cfrom; }
		virtual inline size_t GetLength() const { return cto - cfrom; }
		virtual bool Empty() const { return cfrom == cto; }
		virtual inline ssize_t Compare(const char* WithValue) const { return 0; }
	};
}