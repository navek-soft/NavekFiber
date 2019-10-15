#pragma once
#include <string>
#include <unordered_map>
#include <deque>
#include "../ci/cstring.h"

namespace ibus {
	class crequest {
		static const std::unordered_map<size_t, const char*> codeResponse;
	public:
		enum type { invalid, unsupported, get, post, put, head, options, del, trace, connect, patch };
		using code = std::pair<size_t, const char*>;
		using headers = std::unordered_map<cstringview, cstringview>;
		using payload = std::deque<cstringview>;

		inline code message(size_t msg_code) {  auto&& msg = codeResponse.find(msg_code); return msg != codeResponse.end() ? code(msg_code, msg->second) : code(msg_code,"unknown response code"); }
		inline code message(size_t msg_code,const char* msg) { return code(msg_code, msg); }

		virtual ~crequest() { ; }
		virtual type request_type() = 0;
		virtual const headers& request_headers() = 0;
		virtual const payload& request_paload() = 0;
		virtual ssize_t request_paload_length() = 0;

		virtual size_t response(const uint8_t* response_data, ssize_t response_length) = 0;
		virtual size_t response(const cstringformat& data, size_t msg_code, const std::string& msg_text = {}, const std::unordered_map<std::string, std::string>& headers_list = {}) = 0;

		virtual void disconnect() = 0;
	};
}