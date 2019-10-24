#pragma once
#include "../core/coption.h"
#include "../sapi/csapi.h"

namespace fiber {
	class csapi_python : public csapi {
	public:
		csapi_python(const core::coptions& options);
		virtual ~csapi_python();

		virtual void onshutdown();
		virtual void onconnect(int sock, const sockaddr_storage&);
		virtual void ondisconnect(int sock);
		virtual void onsend(int sock);
		virtual void ondata(int sock);
	};
}