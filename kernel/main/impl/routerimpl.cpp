#include "routerimpl.h"
#include "requestimpl.h"
#include <coreexcept.h>
#include <trace.h>

using namespace Fiber;

bool RouterImpl::AddRoute(const std::string& chPath, const std::string& mqClass, Fiber::ConfigImpl* chConfig, Dom::Interface<IKernel>&& kernel) {
	Dom::Interface<IMQ> mq;
	if (!chPath.empty() && kernel->CreateMQ(mqClass, mq) && mq->Initialize(kernel, chConfig)) {
		mq->AddRef();
		Routes.emplace(chPath, std::move(mq));
		return true;
	}
	return false;
}

void RouterImpl::Process(Server::CHandler* handler) {

	auto requimpl = new RequestImpl(handler);
	Dom::Interface<IRequest> request(requimpl);
	try {
		handler->Process();

		auto&& uri = request->GetUri();

		auto&& route = Routes.find({ (const char*)uri.begin(),(const char*)uri.end() });

		if (route != Routes.end()) {
			requimpl->Process(route->second);
		}
		else {
			log_print("<Router.Runtime.Warning> Route `%s` not found. [MSG: %lu, %s:%ld]", uri.str().c_str(), handler->GetMsgId(), handler->GetAddress().toString(), handler->GetAddress().Port);
			handler->Reply(404, "Endpoint not found", nullptr);
		}
	}
	catch (RuntimeException& ex) {
		log_print("<Server.Runtime.Exception> %s. [MSG: %lu, %s:%ld]", ex.what(), handler->GetMsgId(), handler->GetAddress().toString(), handler->GetAddress().Port);
		auto msg = ex.what();
		handler->Reply(503, "Server.Runtime.Exception", (uint8_t*)msg, std::strlen(msg));
	}
	catch (std::exception& ex) {
		log_print("<Server.Std.Exception> %s. [MSG: %lu, %s:%ld]", ex.what(), handler->GetMsgId(), handler->GetAddress().toString(), handler->GetAddress().Port);
		auto msg = ex.what();
		handler->Reply(503, "Server.Std.Exception", (uint8_t*)msg, std::strlen(msg));
	}
}