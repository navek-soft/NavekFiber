#include "httpimpl.h"
#include "socketimpl.h"
#include <coreexcept.h>

using namespace Fiber::Proto;

HttpImpl::HttpImpl(int hSocket, sockaddr_storage& sa, socklen_t len) : sHandle(hSocket) {
	
	int result = Socket::ReadData(sHandle, sRequest);
	
	if (result != 0 && result != ETIMEDOUT) {
		throw SystemException(result,"Request",__FILE__,__LINE__);
	}

	/* copy if done */
	memcpy(&sAddress, &sa, len);
}

HttpImpl::~HttpImpl() { ; }