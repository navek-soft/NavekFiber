#pragma once
#include <memory>
#include <dom.h>
#include <interface/IRequest.h>
#include <interface/IMQ.h>
#include "../../net/server.h"
#include "utils/str.h"
#include <unordered_map>

namespace Fiber {
	class RequestImpl : public Dom::Client::Embedded<RequestImpl, IRequest> {
	private:
		std::unique_ptr<Server::CHandler>		Request;
		std::string								Content;
		std::unordered_map<zcstring, zcstring, utils::zcstring_ifasthash, utils::zcstring_iequal>	Headers;
	public:
		RequestImpl(Server::CHandler*);
		virtual ~RequestImpl();
		virtual uint64_t GetMsgId();
		virtual Method GetMethod();
		virtual const zcstring GetHeaderOption(const char* option, const char* defvalue = nullptr);
		virtual const zcstring GetContent();
		virtual const zcstring GetUri();
		virtual bool Reply(size_t Code, const char* Message = nullptr, const char* Content = nullptr, size_t ContentLength = 0, const std::unordered_map<const char*, const char*>& HttpHeaders = {}, bool ConnectionClose = true);

		void Process(Dom::IUnknown*);
	};
}