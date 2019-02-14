#pragma once
#include <dom.h>

struct IConfig : virtual public Dom::IUnknown {
	virtual const char* GetConfigValue(const char* propname, const char* propdefault = "") = 0;
	virtual const char* GetProgramName() = 0;
	virtual const char* GetProgramDir() = 0;
	virtual const char* GetProgramCurrentDir() = 0;
};