#pragma once
#include "IString.h"
#include <unordered_map>
#include "../../lib/dom/src/dom/IUnknown.h"

struct IRequest : virtual public Dom::IUnknown {
	enum Method { Get = 1, Post, Put, Head, Options, Delete, Trace, Connect, Patch };

	virtual uint64_t GetMsgId() = 0;
	virtual Method GetMethod() = 0;
	virtual IZCString* GetHeaderOption(const char* option, const char* defvalue) = 0;
	virtual IZCString* GetContent() = 0;
	virtual IZCString* GetUri() = 0;
	virtual bool Reply(size_t Code, const char* Message = nullptr, const char* Content = nullptr, size_t ContentLength = 0, const std::unordered_map<const char*, const char*>& HttpHeaders = {}, bool ConnectionClose = true) = 0;
	IID(Request)
};
