#pragma once
#include "../core/coption.h"
#include "../sapi/csapi.h"
#include <ctime>

namespace fiber {
	class csapi_php : public csapi {
	private:
		static inline constexpr const char* banner{"php"};

		void OnCONNECT(csapi::request& msg);
		void OnPUT(csapi::request& msg);

		crequest::response_headers make_headers(std::time_t ctime, std::time_t mtime, const crequest::headers& reqeaders = {}, const crequest::response_headers& additional = {});

	private:
		std::string	sapiPipe;

		ssize_t execute(ci::cstringformat& result,const std::string& msgid, const std::string& method, const std::string& url, const std::string& script, const crequest::payload&);
	public:
		csapi_php(const core::coptions& options);
		virtual ~csapi_php();

		virtual void onshutdown();
		virtual void onconnect(int sock, const sockaddr_storage&);
		virtual void ondisconnect(int sock);
		virtual void onsend(int sock);
		virtual void ondata(int sock);
	};
}
