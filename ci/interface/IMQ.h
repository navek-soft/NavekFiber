#pragma once
#include "../../lib/dom/src/dom/IUnknown.h"
#include "IString.h"

struct IMQ : virtual public Dom::IUnknown {
	struct Info { const char* Name; const char* Version;};

	virtual bool Initialize(IUnknown* /* kernelObject */, IUnknown* /* optionsList */) = 0;
	virtual const Info&	GetInfo() const = 0;
	virtual void Finalize(IUnknown*) = 0;

	virtual void Get(IUnknown* /* Request */) = 0;
	virtual void Post(IUnknown* /* Request */) = 0;
	virtual void Put(IUnknown* /* Request */) = 0;
	virtual void Head(IUnknown* /* Request */) = 0;
	virtual void Options(IUnknown* /* Request */) = 0;
	virtual void Delete(IUnknown* /* Request */) = 0;
	virtual void Trace(IUnknown* /* Request */) = 0;
	virtual void Connect(IUnknown* /* Request */) = 0;
	virtual void Patch(IUnknown* /* Request */) = 0;

	IID(IMQ);
};