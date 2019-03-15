#include "requestimpl.h"
#include "trace.h"

using namespace Fiber;

RequestImpl::RequestImpl(Server::CHandler* request) : Request(request) {
	dbg_trace("");
}

RequestImpl::~RequestImpl() {
	dbg_trace("");
}

uint64_t RequestImpl::GetMsgId() {
	return Request->GetMsgId();
}

IRequest::Method RequestImpl::GetMethod() {
	return (IRequest::Method)Request->GetMethod();
}

const zcstring RequestImpl::GetHeaderOption(const char* option, const char* defvalue) {
	if (Headers.empty()) {
		for (auto&& opt : Request->GetHeaders()) {
			Headers.emplace(opt.first, opt.second);
		}
	}
	auto&& it = Headers.find({ (const uint8_t*)option,(const uint8_t*)option + strlen(option) });
	if (it != Headers.end()) {
		return it->second;
	}
	return zcstring((const uint8_t*)defvalue, (const uint8_t*)defvalue + (defvalue ? strlen(defvalue) : 0));
}

const zcstring RequestImpl::GetContent() {
	if (Content.empty()) {
		Content = Request->GetContent();
	}
	return { Content };
}

const zcstring RequestImpl::GetUri() {
	return Request->GetUri();
}

bool RequestImpl::Reply(size_t Code, const char* Message, const char* Content, size_t ContentLength, const std::unordered_map<const char*, const char*>& HttpHeaders, bool ConnectionClose) {
	Request->Reply(Code, Message, (const uint8_t*)Content, ContentLength, HttpHeaders, ConnectionClose);
	return true;
}

void RequestImpl::Process(Dom::IUnknown* mq) {
	Dom::Interface<IMQ> queue(mq);
	if (queue) {
		enum Method { Get = 1, Post, Put, Head, Options, Delete, Trace, Connect, Patch };
		switch (Request->GetMethod())
		{
		case Get: queue->Get(this); break;
		case Post: queue->Post(this); break;
		case Put: queue->Put(this); break;
		case Head: queue->Head(this); break;
		case Options: queue->Options(this); break;
		case Delete: queue->Delete(this); break;
		case Trace: queue->Trace(this); break;
		case Connect: queue->Connect(this); break;
		case Patch: queue->Patch(this); break;
		default:
			Request->Reply(200, "OK", nullptr);
		}
		return;
	}
	throw std::runtime_error("IMQ interface not implemented");
}