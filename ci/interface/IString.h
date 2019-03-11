#pragma once
#include <cinttypes>
#include "../../lib/dom/src/dom/IUnknown.h"

struct IZCString : virtual public Dom::IUnknown {

	virtual const char* GetData() const = 0;
	virtual size_t GetLength() const = 0;
	virtual bool Empty() const = 0;
	virtual ssize_t Compare(const char* WithValue) const = 0;

	IID(ZCString);
};