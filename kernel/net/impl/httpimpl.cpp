#include "httpimpl.h"
#include "socketimpl.h"
#include <coreexcept.h>
#include <utils/ascii.h>
#include <utils/findmatch.h>
#include "../../version.h"
#include "httpcodes.hpp"

using namespace Fiber;
using namespace Fiber::Proto;

static constexpr uint8_t* HttpLineEndians[] = { (uint8_t*)"\r\n\r\n", (uint8_t*)"\n\n", (uint8_t*)"\n", (uint8_t*)"\r\n" };

static inline int http_parse_request(const ReadBufferImpl&& data, RequestMethod& Method, zcstring& Url, zcstring& Params, deque<std::pair<zcstring, zcstring>>& Headers, size_t& PayloadOffset) {
	
	if (!data.empty()) {
		auto&& head = data.Head();
		const uint8_t*	current = head.first, *line_begin = head.first,*end = head.second;
		
		auto&& method = utils::match_values(head.first, head.second, RequestMethods,9);

		if (method.result()) {
			Method = (RequestMethod)method.result();
			size_t line_no = 0;
			for (auto&& line = utils::find_values(current, end, HttpLineEndians,4); line.result(); line_begin = (const uint8_t*)line.end(), line = utils::find_values((const uint8_t*)line.end(), end, HttpLineEndians,4), line_no++) {
				if (line.result() > 2) {
					if (line_no) {
						if (auto&& name = utils::find_char(line_begin, (const uint8_t*)line.end(), ": \n")) {
							if (name.result() == 1) {
								if (auto&& value = utils::find_char((const uint8_t*)name.end(), (const uint8_t*)line.end(), "\r\n")) {
									Headers.emplace_back(make_pair(zcstring((const uint8_t*)name.begin(), (const uint8_t*)name.end() - 1),
										zcstring((const uint8_t*)value.begin(), (const uint8_t*)value.end() - 1) ));
									continue;
								}
							}
						}
					}
					else /* First line <Method> <Uri> <...>*/ {
						if (auto&& path = utils::find_char((const uint8_t*)method.end(), (const uint8_t*)line.end(), " \t\n\r")) {
							Url.assign((uint8_t*)path.begin(), (uint8_t*)path.end() - 1);

							if (auto&& params = utils::find_char((const uint8_t*)path.end(), (const uint8_t*)path.end(), "?")) {
								Params.assign((uint8_t*)params.begin() + 1, (uint8_t*)path.end() - 1);
								Url.assign((uint8_t*)path.begin(), (uint8_t*)params.end() - 1);
							}
							continue;
						}
						return 400;
					}
				}
				else /* header separate */ {
					PayloadOffset = (const uint8_t*)line.end() - head.first;
					return 0;
				}
			}
			return 431;
		}
		return 405;
	}
	return 400;
}

static inline void http_close_connection(int& Handle) {
	if (Handle) {
		trace("Close socket: %ld", Handle);
		shutdown(Handle, SHUT_RDWR);
		Socket::Close(Handle);
		Handle = 0;
	}
}

HttpImpl::HttpImpl(msgid mid, int hSocket, sockaddr_storage& sa, size_t max_request_length, size_t max_header_length ) noexcept
	: Handle(hSocket), MsgId(mid), MaxRequestLength(max_request_length),MaxHeaderLength(max_header_length), Address(sa) {
	
	Telemetry.tmCreatedAt = utils::timestamp();
}

HttpImpl::~HttpImpl() { 
	http_close_connection(Handle); 
}

void HttpImpl::Process() {
	
	if (Socket::SetOpt(Handle, SOL_TCP, TCP_NODELAY, 1) != 0) {
		trace("Invalid TCP_NODELAY %s(%ld)", strerror(errno), errno);
	}

	ssize_t result = Socket::ReadData(Handle, Request.Raw, MaxRequestLength, MaxHeaderLength);
	Telemetry.tmReadingAt = utils::timestamp();

	if (result != 0) {
		Reply(408);
		throw RuntimeException(__FILE__, __LINE__, "Invalid receive data %s:%u. Socket error %s(%lu). MsgId: %lu", Address.toString(), Address.Port, strerror((int)result), result, MsgId);
	}

	if (auto code = http_parse_request(std::move(Request.Raw), Request.Method, Request.Uri, Request.Params, Request.Headers, Request.PayloadOffset)) {
		Reply(code);
		throw RuntimeException(__FILE__, __LINE__, "Invalid HTTP request %s:%u. HTTP error: %ld.  MsgId: %lu", Address.toString(), Address.Port, code, MsgId);
	}
}

void HttpImpl::Reply(size_t Code, const char* Message, const uint8_t* Content, size_t ContentLength, const unordered_map<const char*, const char*>& HttpHeaders,bool ConnectionClose) {
	
	const char* MsgText = nullptr;
	string Answer("HTTP/1.1 ");
	Answer.append(toString(Code)).append(" ").append(MsgText = HttpMessage(Code, Message)).
		append("\r\nServer: " FIBER_SERVER "\r\nX-Powered-By: " FIBER_XPOWERED "/" FIBER_VERSION "\r\n");
	{
		for (auto&& h : HttpHeaders) {
			Answer.append(h.first).append(": ").append(h.second).append("\r\n");
		}
	}

	Answer.append("Content-Length: ").append(toString(ContentLength ? ContentLength : (Content != nullptr ? std::strlen((const char*)Content) : 0))).append("\r\n");

	if (ConnectionClose) {
		Answer.append("Connection: close\r\n");
	}

	Answer.append("X-Fiber-Msg-Uid: ").append(toString(MsgId)).append("\r\n");
	{
		/* Telemetry */
		Answer.append("X-Fiber-Msg-Telemetry: created-at=").append(std::to_string(Telemetry.tmCreatedAt)).
			append(";reading-at=").	append(std::to_string(Telemetry.tmReadingAt)).
			append(";finished-at=").append(std::to_string(Telemetry.tmFinishedAt = utils::timestamp())).
			append("\r\n");
	}
	Answer.append("\r\n");

#ifdef NDEBUG
	log_print("[ Http::Response:%lu ] %s:%u - %ld (%s) # Thread: %lx", MsgId, Address.toString(), Address.Port, Code, MsgText,std::this_thread::get_id());
#else
	log_print("[ Http::Response:%lu ] %s:%u - %ld (%s) # Thread: %lx", MsgId, Address.toString(), Address.Port, Code, MsgText, std::this_thread::get_id());
#endif // NDEBUG


	if (Socket::WriteData(Handle, Answer.data(), Answer.length()) != (ssize_t)Answer.length() || Socket::WriteData(Handle, Content, ContentLength) != (ssize_t)ContentLength) {
		http_close_connection(Handle);
		throw RuntimeException(__FILE__, __LINE__, "Invalid send data %s:%u. MsgId: %lu", Address.toString(), Address.Port, MsgId);
	}

	if (ConnectionClose)
		http_close_connection(Handle);
}
