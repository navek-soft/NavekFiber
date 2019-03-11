#pragma once
#include <dom.h>

struct IConfig : virtual public Dom::IUnknown {
	virtual const char* GetConfigValue(const char* propname, const char* propdefault = "") const = 0;
	virtual const char* GetProgramName() const = 0;
	virtual const char* GetProgramDir() const = 0;
	virtual const char* GetProgramCurrentDir() const = 0;
};