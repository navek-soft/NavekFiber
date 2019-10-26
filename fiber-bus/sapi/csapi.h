#pragma once
#include "../server/crequest.h"
#include "../ci/cstring.h"
#include "../core/coption.h"
#include <sys/socket.h>
#include <memory>
#include <deque>
#include <string>
#include <vector>

namespace fiber {
	class csapi {
	protected:
		friend class request;
		friend class response;
#pragma pack(push,1)
		struct header {
			uint64_t	tlen	: 22;
			uint64_t	poffset	: 18;
			uint64_t	code	: 10;
			uint64_t	type	: 8;
			uint64_t	argc	: 6;
			union {
				char argv[0];
				uint8_t buffer[0];
			};
		};
#pragma pack(pop)
	public:
		class request : public fiber::crequest {
		private:
			int					 msgSocket;
			std::vector<uint8_t> msgData;
			crequest::headers	 msgHeaders;
			ci::cstringview		msgUri;
			crequest::payload	msgPayload;
			inline header* get() { return (header*)(!msgData.empty() ? msgData.data() : nullptr); }
		public:
			request(int sock);
			virtual ~request();

			virtual crequest::type request_type();
			virtual const ci::cstringview& request_uri();
			virtual const crequest::headers& request_headers();
			virtual const crequest::payload& request_paload();
			virtual ssize_t request_paload_length();

			virtual ssize_t response(const payload& data, size_t data_length, size_t msg_code, const std::string& msg_text = {}, const response_headers& headers_list = {}) { return -1; }
			virtual ssize_t response(const ci::cstringformat& data, size_t msg_code, const std::string& msg_text = {}, const response_headers& headers_list = {}) { return -1; }

			virtual ssize_t response(const payload& data, const std::string& uri, size_t msg_code, crequest::type msg_type, const crequest::response_headers& headers_list = {});
			virtual ssize_t response(const ci::cstringformat& data, const std::string& uri, size_t msg_code, crequest::type msg_type, const response_headers& headers_list = {});

			virtual void disconnect() { ; }
		};
		class response {
		private:
			std::deque<std::string> msgPayload;
		public:
			response() { ; }
			virtual ~response() { ; }
			inline void emplace(const ci::cstringformat& data) { msgPayload.emplace_back(data.str()); }
			inline void emplace(const std::string& data) { msgPayload.emplace_back(data); }
			ssize_t reply(int sock,const std::string& uri, size_t msg_code, crequest::type msg_type, const crequest::response_headers& headers_list = {});
			ssize_t reply(int sock,const crequest::payload& payload, size_t payload_length, const std::string& uri, size_t msg_code, crequest::type msg_type, const crequest::response_headers& headers_list = {});
		};
	public:
		csapi(const core::coption& connection_string);
		virtual ~csapi();
		virtual void onshutdown() = 0;
		virtual void onconnect(int sock, const sockaddr_storage&) = 0;
		virtual void ondisconnect(int sock) = 0;
		virtual void onsend(int sock) = 0;
		virtual void ondata(int sock) = 0;
		int run();
	private:
		int sapiSocket{ -1 };
		core::coption::dsn_params sapiConnection;
	};
}
