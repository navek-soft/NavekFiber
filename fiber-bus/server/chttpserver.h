#pragma once
#include <deque>
#include <mutex>
#include <list>
#include "../core/cserver.h"
#include "../core/coption.h"
#include "cmemory.h"
#include "crequest.h"

namespace fiber {
	class chttpserver : public core::cserver::tcp {
	private:
		class request : public fiber::crequest {
		private:
			mutable ssize_t reqLength{ 0 };
			mutable ssize_t reqContentLength{ 0 }, reqPaloadLength{ 0 }, reqTime{0}, reqReceiveTime{ 0 }, reqReceiveTimeOut{ 0 };
			crequest::type	reqType{ crequest::invalid };
			mutable int		reqSocket{ -1 };
			mutable std::list<std::unique_ptr<cmemory::frame>> reqBuffer;
			crequest::payload reqPayload;
			
			size_t				offset_payload{ 0 };
			ci::cstringview		uri;
			crequest::headers	headers;
			
			inline crequest::type check_type_parse();
			inline bool request_parse(ci::cstringview& val, const ci::cstringview& request);
			inline bool headers_parse(crequest::headers& val, size_t& payload_offset, const ci::cstringview& request);
		protected:
			friend class chttpserver;
			inline ssize_t get_chunk(ssize_t max_request_size);
			inline void reset();
			inline bool is_timeout_ellapsed(uint64_t now) { return reqReceiveTimeOut && (now > (size_t)reqReceiveTime); }
			inline void update_time() { reqReceiveTime += reqReceiveTimeOut; }
		public:
			request(int sock,uint64_t rcv_timeout,uint64_t time) : reqTime(time), reqReceiveTime(time + rcv_timeout), reqReceiveTimeOut(rcv_timeout), reqSocket(sock) { ; }
			virtual ~request();
			virtual type request_type() { 
				return reqType; 
			}
			virtual const ci::cstringview& request_uri() { return uri; }
			virtual ssize_t request_length() { return reqLength; }
			virtual const crequest::headers& request_headers() { return headers; }
			virtual const crequest::payload& request_paload();
			virtual ssize_t request_paload_length() { return reqPaloadLength; }
			virtual size_t response(const uint8_t* response_data, ssize_t response_length);
			virtual size_t response(const payload& data, size_t data_length, size_t msg_code, const std::string& msg_text = {}, const response_headers& headers_list = {});
			virtual size_t response(const ci::cstringformat& data, size_t msg_code, const std::string& msg_text = {}, const std::unordered_map<std::string, std::string>& headers_list = {});
			virtual void disconnect();
		};
		time_t optReceiveTimeout{ 0 };
		ssize_t optMaxRequestSize{ 0 };
		mutable std::unordered_map<int, std::shared_ptr<request>> requestsClient;
	public:
		std::string server_banner{ "chttpserver" };
		chttpserver(const core::coptions& options);
		virtual ~chttpserver() { ; }
	public:
		virtual const std::string& getname() const { return server_banner; }
		virtual void onshutdown() const;
		virtual void onclose(int soc) const;
		virtual void onconnect(int soc, const sockaddr_storage&, uint64_t) const;
		virtual void ondisconnect(int soc) const;
		virtual void ondata(int soc) const;
		virtual void onwrite(int soc) const;
		virtual void onidle(uint64_t time) const;
	};
}