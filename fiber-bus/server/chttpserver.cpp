#include "chttpserver.h"
#include "../core/memcpy.h"
#include <list>
#include <sys/ioctl.h>
#include <unistd.h>
#include <endian.h>
#include <cstdarg>
#include "capp.h"
#include <netinet/tcp.h>

using namespace fiber;

chttpserver::chttpserver(const core::coptions& options)
	: core::cserver::tcp((uint16_t)options.at("port").number(), options.at("ip","").get(), options.at("iface", "").get())
{
	optReceiveTimeout = (time_t)(options.at("receive-timeout", "3000").number());
	optMaxRequestSize = (ssize_t)options.at("max-request-size", "5048576").bytes();
	set_nodelay();
	set_reuseaddress();
	set_reuseport();
	if (optReceiveTimeout) {
		/* set check every 500 msec */
		set_receivetimeout(5000);
	}
}

void chttpserver::onshutdown() const {
	printf("onshutdown\n");
}

void chttpserver::onclose(int soc) const {
	if (auto&& cli = requestsClient.find(soc); cli != requestsClient.end()) {
		requestsClient.erase(cli);
	}
}

void chttpserver::onconnect(int soc, const sockaddr_storage&, uint64_t time) const {
	auto&& req = requestsClient.emplace(soc, std::shared_ptr<request>(new request(soc, optReceiveTimeout, time)));
	if (!req.second) {
		req.first->second.get()->reset();
		req.first->second->update_time();
	}
	else {
		int opt = 1;	setsockopt(soc, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
		opt = 30;		setsockopt(soc, IPPROTO_TCP, TCP_KEEPINTVL, &opt, sizeof(opt));
	}
	//printf("onconnect (%ld)\n", soc);
	//struct timeval tv = { optReceiveTimeout / 1000, optReceiveTimeout % 1000 };
	//setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO, (struct timeval*) & tv, sizeof(struct timeval));
}

void chttpserver::ondisconnect(int soc) const {
	//printf("ondisconnect (%ld)\n", soc);
	if (auto&& cli = requestsClient.find(soc); cli != requestsClient.end()) {
		requestsClient.erase(cli);
	}
}

void chttpserver::onidle(uint64_t time) const { 
	for (auto&& cli : requestsClient) {
		if (cli.second->is_timeout_ellapsed(time)) {
			cli.second->response({}, 408, {}, { { "X-Server","NavekFiber ESB" } });
			cli.second->disconnect();
			requestsClient.erase(cli.first);
			break;
		}
	}
}

void chttpserver::ondata(int soc) const {

	if (auto&& cli = requestsClient.find(soc); cli != requestsClient.end()) {
		auto&& result = cli->second->get_chunk(optMaxRequestSize);
		cli->second->update_time();
		if (result == 200) {
			capp::dispatch(std::shared_ptr<fiber::crequest>(cli->second));
			requestsClient.erase(cli);
		}
		else if (result == 202) {
			printf("(%ld) more data\n", soc);
		}
		else if (result > 299) {
			::shutdown(soc, SHUT_RD);
			cli->second->response({}, result, {}, { { "X-Server","NavekFiber ESB" } });
			cli->second->disconnect();
			requestsClient.erase(cli);
		}
	}
	else {
		fprintf(stderr, "socket is'tn client.\n");
		::close(soc);
	}
}

void chttpserver::onwrite(int soc) const {
	//printf("onwrite (%ld)\n", soc);
}

inline void chttpserver::request::reset() {
	reqPayload.clear();
	reqBuffer.clear();
	reqLength = 0;
	reqType = crequest::invalid;
	reqPaloadLength = 0;
	reqContentLength = 0;
}

inline ssize_t chttpserver::request::get_chunk(ssize_t max_request_size) {
	reqBuffer.emplace_back(std::unique_ptr<cmemory::frame>(new cmemory::frame));
	int ncount = 0;
	do {
		if (reqBuffer.back()->length > 0) {
			reqBuffer.emplace_back(std::unique_ptr<cmemory::frame>(new cmemory::frame));
		}
		reqBuffer.back()->length = ::recv(reqSocket, reqBuffer.back()->data, sizeof(reqBuffer.back()->data), 0);
		
		if (reqBuffer.back()->length > 0) {
			if (reqBuffer.size() == 1) {
				switch (reqType = check_type_parse(); reqType) {
				case crequest::post: case crequest::put: case patch:
				{
					auto&& it = headers.find(ci::cstringview( "content-length",14 ));
					if (it != headers.end()) {
						reqContentLength = std::stoull(it->second.str());
						if (reqContentLength > max_request_size) {
							return 413;
						}
						reqPaloadLength = reqBuffer.back()->length - offset_payload;
					}
					else {
						return 411;
					}
					break;
				}
				case crequest::invalid:
					return 400;
				case crequest::unsupported:
					return 405;
				default:
					return 200;
				}
			}
			else {
				reqPaloadLength += reqBuffer.back()->length;
			}
			reqLength += reqBuffer.back()->length;
			if (reqLength > max_request_size) {
				return 413;
			}
			if (reqContentLength && reqContentLength == reqPaloadLength) {
				return 200;
			}
		}
		else {
			ssize_t nresult = reqBuffer.back()->length;
			reqBuffer.pop_back();
			return nresult == 0 ? 202 : 417;
		}
	} while (((ioctl(reqSocket, FIONREAD, &ncount) == 0 && ncount > 0) || errno == EAGAIN));

	if (reqBuffer.back()->length <= 0) {
		reqBuffer.pop_back();
	}

	return 202;
}

crequest::type chttpserver::request::check_type_parse() {
	//invalid, unsupported, get, post, put, head, options, delete, trace, connect, patch;
	static constexpr struct {
		uint64_t		value; 
		crequest::type	type;
		ssize_t			offset;
	} http_request_types[]{
		{/* 'GET /'		*/ 0x0000002f20544547,crequest::get,	4},
		{/* 'POST /'	*/ 0x00002f2054534f50,crequest::post,	5},
		{/* 'PUT /'		*/ 0x0000002f20545550,crequest::put,	4},
		{/* 'HEAD /'	*/ 0x00002f2044414548,crequest::head,	5},
		{/* 'OPTINOS '	*/ 0x20534e4f4954504f,crequest::options,8},
		{/* 'DELETE	/'	*/ 0x2f204554454c4544,crequest::del,	7},
		{/* 'TRACE /'	*/ 0x002f204543415254,crequest::trace,	6},
		{/* 'CONNECT '	*/ 0x205443454e4e4f43,crequest::connect,8},
		{/* 'PATCH /'	*/ 0x002f204843544150,crequest::patch,	6}
	};

#if BYTE_ORDER == BIG_ENDIAN
#pragma message __FILE__ ":" "\n\t\thttp_request_types need declare with big endian conversion"
#endif // BIG_ENDIAN

	ci::cstringview header((const char*)reqBuffer.front().get()->data, reqBuffer.front().get()->length);
	if (header.length() > 9) {
		offset_payload = 0;
		for (size_t i = 0; i < sizeof(http_request_types); i++) {
			if ((http_request_types[i].value & header.cast<uint64_t>()) == http_request_types[i].value) {
				if (request_parse(uri, { header.begin() + http_request_types[i].offset, header.end() }) &&
					headers_parse(headers, offset_payload += uri.end() - header.begin(), { uri.end(), header.end() })) {
					return http_request_types[i].type;
				}
				break;
			}
		}
	}
	return crequest::invalid;
}

inline bool chttpserver::request::request_parse(ci::cstringview& val, const ci::cstringview& request) {
	val = request.find_next_space(request);
	return (!val.empty() && (*val.begin() == '/'));
}

inline bool chttpserver::request::headers_parse(crequest::headers& val,size_t& payload_offset, const ci::cstringview& request) {
	auto&& line = request.find_next('\n', request);
	for (line = request.find_next('\n', { line.end() + 1,request.end() }); !line.empty(); line = request.find_next('\n', { line.end() + 1,request.end() })) {
		auto&& pair = line.explode(':', 1);
		val.emplace(ci::cstringview( pair.front().begin(),pair.front().end() - 1 ), pair.back());
		
		if (line.cast<uint16_t>() == uint16_t(0x0a0d) || line.cast<uint32_t>(-2) == uint32_t(0x0a0d0a0d)) {
			payload_offset += (line.end() + 1) - request.begin();
			return true;
		}
	}
	return false;
}

chttpserver::request::~request() {
	printf("%s\n", __PRETTY_FUNCTION__);
	disconnect();
}

const crequest::payload& chttpserver::request::request_paload() {
	if (reqPayload.empty()) {
		auto&& it = reqBuffer.begin();
		reqPayload.emplace_back(ci::cstringview(it->get()->data + offset_payload,it->get()->data + it->get()->length));
		for (; ++it != reqBuffer.end();) {
			reqPayload.emplace_back(ci::cstringview(it->get()->data, it->get()->data + it->get()->length));
		}
	}
	return reqPayload;
}

size_t chttpserver::request::response(const uint8_t* response_data, ssize_t response_length) {
	if (reqSocket > 0) {
		return ::send(reqSocket, response_data, response_length, MSG_NOSIGNAL) == response_length ? 200 : 206 /* Partial content */;
	}
	return 499 /* Connection close */;
}


ssize_t chttpserver::request::response(const crequest::payload& data, size_t data_length, size_t msg_code, const std::string& msg_text, const std::unordered_map<std::string, std::string>& headers_list) {
	if (reqSocket > 0) {
		ci::cstringformat response_data;
		ssize_t result = -1;
		response_data.append("HTTP/1.1 %lu %s\r\n", msg_code, (msg_text.empty() ? message(msg_code).second : msg_text).c_str());
		for (auto&& [h, v] : headers_list) {
			response_data.append("%s: %s\r\n", h.c_str(), v.c_str());
		}

		if (!data_length) {
			for (auto&& p : data) {
				data_length += p.length();
			}
		}

		response_data.append("Content-Length: %lu\r\n\r\n", data_length);

		for (auto&& chunk : response_data) {
			if (result = send(reqSocket, chunk.data(), chunk.length(), 0); result == (ssize_t)chunk.length()) {
				continue;
			}
			if (result == -1) {
				fprintf(stderr, "[http-server(%s)]. error response send (%s)\n", strerror(errno));
				return 523;
			}
			return 206;
		}
		for (auto&& p : data) {
			if (result = send(reqSocket, p.data(), p.length(), 0); result == (ssize_t)p.length()) {
				continue;
			}
			if (result == -1) {
				fprintf(stderr, "[http-server(%s)]. error response send (%s)\n", strerror(errno));
				return 523;
			}
			return 206;
		}

		return 200;
	}
	return 499 /* Connection close */;
}


ssize_t chttpserver::request::response(const ci::cstringformat& data, size_t msg_code, const std::string& msg_text, const std::unordered_map<std::string, std::string>& headers_list) {
	if (reqSocket > 0) {
		ci::cstringformat response_data;
		ssize_t result = -1;
		response_data.append("HTTP/1.1 %lu %s\r\n", msg_code, (msg_text.empty() ? message(msg_code).second : msg_text).c_str());
		for (auto&& [h, v] : headers_list) {
			response_data.append("%s: %s\r\n",h.c_str(),v.c_str());
		}
		if(data.length())
			response_data.append("Content-Length: %lu\r\n\r\n", data.length());
		else
			response_data.append("\r\n");

		response_data.append(data);

		for (auto&& chunk : response_data) {
			if (result = send(reqSocket, chunk.data(), chunk.length(), 0); result == (ssize_t)chunk.length()) {
				continue;
			}
			if (result == -1) {
				fprintf(stderr, "[http-server(%s)]. error response send (%s)\n", strerror(errno));
				return 523;
			}
			return 206;
		}
		return 200;
	}
	return 499 /* Connection close */;
}

void chttpserver::request::disconnect() {
	if (reqSocket > 0) {
		::close(reqSocket);
		reqSocket = -1;
	}
}
