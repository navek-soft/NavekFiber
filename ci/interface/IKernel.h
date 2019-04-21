#pragma once
#include <dom.h>

struct IKernel : virtual public Dom::IUnknown {
	
	virtual bool CreateMQ(const Dom::clsuid& cid, void ** ppv) = 0;
	virtual bool CreateSAPI(const Dom::clsuid& cid, void ** ppv) = 0;
	IID(Kernel)
};
