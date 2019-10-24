#pragma once
#include <deque>
#include <mutex>
#include <list>
#include "../core/cserver.h"
#include "../core/coption.h"
#include "cmemory.h"
#include "crequest.h"

namespace fiber {
	class csapiserver : public core::cserver::pipe {
	private:
		class request : public fiber::crequest {};
		time_t optReceiveTimeout{ 0 };
		ssize_t optMaxRequestSize{ 0 };
	public:
		std::string server_banner{ "csapiserver" };
		virtual const std::string& getname() const { return server_banner; }

		csapiserver(const core::coptions& options);
		virtual ~csapiserver() { ; }
	public:
		virtual void onshutdown() const;
		virtual void onclose(int soc) const;
		virtual void onconnect(int soc, const sockaddr_storage&, uint64_t) const;
		virtual void ondisconnect(int soc) const;
		virtual void ondata(int soc) const;
		virtual void onwrite(int soc) const;
		virtual void onidle(uint64_t time) const;
	};
}