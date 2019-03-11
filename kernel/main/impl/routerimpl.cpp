#include "routerimpl.h"

using namespace Fiber;

bool RouterImpl::AddRoute(const std::string& path, const std::string& classname, Dom::Interface<IKernel>&& kernel) {
	Dom::Interface<IMQ> mq;
	if (!path.empty() && kernel->CreateMQ(classname, mq)) {
	}
}

bool RouterImpl::Process(const Server::CHandler& request) {

}