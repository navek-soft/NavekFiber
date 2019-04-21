#pragma once
#include <dom.h>
#include "IRequest.h"

struct ISAPI : virtual public Dom::IUnknown {
	struct Info { const char *Name; const char* Version; };

	virtual bool Initialize(Dom::IUnknown* /* kernelObject */, Dom::IUnknown* /* optionsList */) = 0;
	virtual const Info&	GetInfo() const = 0;
	virtual void Finalize(Dom::IUnknown*) = 0;
	virtual bool Execute(Dom::IUnknown*) = 0;

	IID(ISAPI);
};