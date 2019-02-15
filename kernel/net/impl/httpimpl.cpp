#include "httpimpl.h"
#include "socketimpl.h"
#include <coreexcept.h>
#include <utils/ascii.h>
#include <utils/findmatch.h>
#include "../../version.h"

using namespace Fiber;
using namespace Fiber::Proto;

static constexpr uint8_t* HttpLineEndians[] = { (uint8_t*)"\r\n\r\n", (uint8_t*)"\n\n", (uint8_t*)"\n", (uint8_t*)"\r\n" };

static inline int http_parse_request(const ReadBufferImpl&& data, RequestMethod& Method, zcstring& Url, zcstring& Params, deque<std::pair<zcstring, zcstring>>& Headers, size_t& PayloadOffset) {
	
	if (!data.empty()) {
		auto&& head = data.Head();
		const uint8_t*	current = head.first, *line_begin = head.first,*end = head.second;

		auto&& method = utils::match(head.first, head.second, RequestMethods);

		if (method.result()) {
			Method = (RequestMethod)method.result();
			size_t line_no = 0;
			for (auto&& line = utils::find(current, end, HttpLineEndians); line.result(); line_begin = (const uint8_t*)line.end(), line = utils::find((const uint8_t*)line.end(), end, HttpLineEndians), line_no++) {
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

HttpImpl::HttpImpl(msgid mid, int hSocket, sockaddr_storage& sa, socklen_t len) : sHandle(hSocket) {
	
	int result = Socket::ReadData(sHandle, sRequest.sRaw);
	
	if (result != 0 && result != ETIMEDOUT) {
		throw SystemException(result,"Request",__FILE__,__LINE__);
	}
	
	if (http_parse_request(std::move(sRequest.sRaw), sRequest.sMethod, sRequest.sUri, sRequest.sParams, sRequest.sHeaders, sRequest.sPayloadOffset)) {

	}

	/* copy if done */
	memcpy(&sAddress, &sa, len);
}

HttpImpl::~HttpImpl() { trace("Close socket: %ld", sHandle);  Socket::Close(sHandle);  }


HttpImpl::Response::Response(size_t Code, const char* Message = nullptr, unordered_map<string, string>&& Headers, list<string>&& Payload) {
	WriteBufferImpl header;
	WriteBufferImpl content;
}