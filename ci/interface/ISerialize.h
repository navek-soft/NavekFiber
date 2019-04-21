#pragma once
#include <dom.h>

struct ISerialize : virtual public Dom::IUnknown {

	virtual bool Write(const char* Key,const char* Id,const char* Value) = 0;
	virtual bool Read(const char* Key, char** Id, char** Value) = 0;

	IID(Serialize)
};
