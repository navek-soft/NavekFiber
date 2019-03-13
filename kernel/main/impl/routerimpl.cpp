#include "routerimpl.h"
#include <coreexcept.h>
#include <trace.h>

using namespace Fiber;

bool RouterImpl::AddRoute(const std::string& chPath, const std::string& mqClass, Fiber::ConfigImpl* chConfig, Dom::Interface<IKernel>&& kernel) {
	Dom::Interface<IMQ> mq;
	if (!chPath.empty() && kernel->CreateMQ(mqClass, mq) && mq->Initialize(kernel, chConfig)) {
		Routes.emplace(chPath, std::move(mq));
		return true;
	}
	return false;
}

void RouterImpl::Process(const shared_ptr<Server::CHandler>& request) {

	try {
		request->Process();
		// TODO: route requst
		//auto&& request->GetUri();
	}
	catch (RuntimeException& ex) {
		log_print("<Server.Runtime.Exception> %s. [MSG: %ul, %s:%ld]", ex.what(), request->GetMsgId(), request->GetAddress().toString(), request->GetAddress().Port);
		auto msg = ex.what();
		request->Reply(503, "Server.Runtime.Exception", (uint8_t*)msg, std::strlen(msg));
	}
	catch (std::exception& ex) {
		log_print("<Server.Std.Exception> %s. [MSG: %ul, %s:%ld]", ex.what(), request->GetMsgId(), request->GetAddress().toString(), request->GetAddress().Port);
		auto msg = ex.what();
		request->Reply(503, "Server.Std.Exception", (uint8_t*)msg, std::strlen(msg));
	}
}