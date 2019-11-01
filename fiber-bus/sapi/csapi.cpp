#include "csapi.h"
#include "../core/cexcept.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <thread>
#include "../core/csignal.h"

using namespace fiber;

csapi::csapi(const core::coption& connection_string) {
	sapiConnection = connection_string.dsn();

	if (sapiConnection.proto == "file" || sapiConnection.proto == "unix") {
		core::system_error(0, "protocol  not supported", __PRETTY_FUNCTION__, __LINE__);
	}
}

csapi::~csapi() 
{ 
	if (sapiSocket > 0) {
		::close(sapiSocket);
	}
}

int csapi::run() {
	struct sockaddr_un serveraddr;

	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sun_family = AF_UNIX;
	strncpy(serveraddr.sun_path, std::string(sapiConnection.path + sapiConnection.filename).c_str(), sizeof(serveraddr.sun_path) - 1);

	struct pollfd polllist[1]{ {.fd = 0,.events = POLLIN | POLLERR | POLLHUP | POLLNVAL} };

	if (sapiSocket = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0); sapiSocket <= 0) {
		throw core::system_error(errno, __PRETTY_FUNCTION__, __LINE__);
	}
	polllist[0].fd = sapiSocket;
	signal(SIGPIPE, SIG_IGN);

	{
		bool isContinue = true;
		while (isContinue) {
			if ((connect(sapiSocket, (struct sockaddr*) & serveraddr, (socklen_t)SUN_LEN(&serveraddr)) == 0)) {
				isContinue = false;
				onconnect(sapiSocket, (struct sockaddr_storage&)serveraddr);

				int nevents = 0;
				while ((nevents = poll(polllist, 1, -1)) || 1) {
					if (nevents > 0) {
						if (polllist[0].revents == POLLIN) {
							ondata(sapiSocket);
							polllist[0].revents = 0;
							continue;
						}
						printf("(%ld) %s%s%s\n", getpid(),
							(polllist[0].revents & POLLERR) ? "POLLERR" : "",
							(polllist[0].revents & POLLNVAL) ? "POLLNVAL" : "",
							(polllist[0].revents & POLLHUP) ? "POLLHUP" : "");
						ondisconnect(sapiSocket);
						polllist[0].revents = 0;
						break;
					}
					else if (nevents == -1) {
						printf("(%ld) sapi-fpm poll-error: %s (%d)\n", getpid(), strerror(errno), errno);
						break;
					}
					else {
						printf("(%ld) sapi-fpm poll-zero: %s (%d)\n", getpid(), strerror(errno), errno);
					}
				}
			}
			switch (errno) {
			case ECONNREFUSED:
				printf("(%ld) sapi-fpm reconnect-required: %s (%d) ... %d seconds timeout\n", getpid(), strerror(errno), errno, 5);
				isContinue = true;
				std::this_thread::sleep_for(std::chrono::seconds(5));
				break;
			case EISCONN: case ENOENT:
				printf("(%ld) sapi-fpm reconnect-required: %s (%d) ... %d seconds timeout\n", getpid(), strerror(errno), errno, 5);
				isContinue = true;
				::close(sapiSocket);
				std::this_thread::sleep_for(std::chrono::seconds(5));
				if (sapiSocket = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0); sapiSocket <= 0) {
					throw core::system_error(errno, __PRETTY_FUNCTION__, __LINE__);
				}
				polllist[0].fd = sapiSocket;
				break;
			default:
				printf("(%ld) sapi-fpm connection-error: %s (%d)\n", getpid(), strerror(errno), errno);
				isContinue = false;
				break;
			}
		}
	}
	if (sapiSocket > 0) {
		::close(sapiSocket);
		sapiSocket = -1;
	}
	onshutdown();
	return 0;
}

ssize_t csapi::response::reply(int sock, const crequest::payload& payload, std::size_t payload_length, const std::string& uri, std::size_t msg_code, crequest::type msg_type, const crequest::response_headers& headers_list) {
	std::size_t headers_length = uri.length() + 1;

	for (auto&& p : headers_list) {
		headers_length += p.first.length() + 2 + p.second.length() + 1 /* always zero endian data string */;
	}

	auto&& msg = (csapi::header*) alloca(headers_length + sizeof(csapi::header) + payload_length);
#pragma GCC diagnostic ignored "-Wconversion"
	msg->poffset = (headers_length + sizeof(csapi::header));
	msg->tlen = headers_length + sizeof(csapi::header) + payload_length;
	msg->code = msg_code;
	msg->type = msg_type;
	msg->argc = 1 /* uri is first argv[0] */ + headers_list.size();
#pragma GCC diagnostic warning "-Wconversion"	
	uint8_t* buffer = msg->buffer;

	{	// uri add
		memcpy(buffer, uri.data(), uri.length());
		buffer += uri.length() + 1;
		*(buffer - 1) = '\0';
	}
	for (auto&& p : headers_list) {
		memcpy(buffer, p.first.data(), p.first.length());
		buffer += p.first.length() + 2;
		memcpy(buffer - 2, ": ", 2);
		memcpy(buffer, p.second.data(), p.second.length());
		buffer += p.second.length() + 1;
		*(buffer - 1) = '\0';
	}

	for (auto&& p : payload) {
		memcpy(buffer, p.data(), p.length());
		buffer += p.length();
	}

	if (auto&& result = ::send(sock, msg, msg->tlen, 0); result == msg->tlen) {
		return 200;
	}
	fprintf(stderr, "[http-server(%s)]. error response send (%s)\n", strerror(errno));
	return 523;
}


ssize_t csapi::response::reply(int sock, const std::string& uri, std::size_t msg_code, crequest::type msg_type, const crequest::response_headers& headers_list) {
	std::size_t payload_length = 0;
	std::size_t headers_length = uri.length() + 1;

	for (auto&& p : headers_list) {
		headers_length += p.first.length() + 2 + p.second.length() + 1 /* always zero endian data string */;
	}

	for (auto&& p : msgPayload) {
		payload_length += p.length();
	}
	auto&& msg = (csapi::header*) alloca(headers_length + sizeof(csapi::header) + payload_length);
#pragma GCC diagnostic ignored "-Wconversion"
	msg->poffset = (headers_length + sizeof(csapi::header));
	msg->tlen = headers_length + sizeof(csapi::header) + payload_length;
	msg->code = msg_code;
	msg->type = msg_type;
	msg->argc = 1 /* uri is first argv[0] */ + headers_list.size();
#pragma GCC diagnostic warning "-Wconversion"	
	uint8_t* buffer = msg->buffer;

	{	// uri add
		memcpy(buffer, uri.data(), uri.length());
		buffer += uri.length() + 1;
		*(buffer - 1) = '\0';
	}
	for (auto&& p : headers_list) {
		memcpy(buffer, p.first.data(), p.first.length());
		buffer += p.first.length() + 2;
		memcpy(buffer - 2, ": ", 2);
		memcpy(buffer, p.second.data(), p.second.length());
		buffer += p.second.length() + 1;
		*(buffer - 1) = '\0';
	}

	for (auto&& p : msgPayload) {
		memcpy(buffer, p.data(), p.length());
		buffer += p.length();
	}

	if (auto&& result = ::send(sock, msg, msg->tlen, 0); result == msg->tlen) {
		return 200;
	}
	fprintf(stderr, "[http-server(%s)]. error response send (%s)\n", strerror(errno));
	return 523;
}

csapi::request::request(int sock) : msgSocket(sock) {
	csapi::header hdr;
	if (recv(sock, &hdr, sizeof(hdr), MSG_PEEK) == sizeof(hdr)) {
		msgData.resize(hdr.tlen);
		if (recv(sock, msgData.data(), hdr.tlen, 0) == hdr.tlen) {
			auto&& msg = get();
			
			/* parse uri */
			uint8_t* ptr = msg->buffer;
			msgUri.assign(ptr, ptr + std::strlen((char*)ptr));
			/* parse payload */

			ptr = msgUri.end() + 1;

			msgPayload.emplace_back(ci::cstringview(((uint8_t*)msg) + msg->poffset, ((uint8_t*)msg) + msg->tlen));

			/* parse headers */
			for (std::size_t n = 1; n < msg->argc; n++) {
				ci::cstringview line(ptr, ptr + std::strlen((char*)ptr));
				auto&& pair = line.explode(':', 1);
				msgHeaders.emplace(ci::cstringview(pair.front().begin(), pair.front().end() - 1), pair.back());
				ptr = line.end() + 1;
			}
			return;
		}
	}
	((csapi::header*)msgData.data())->type = crequest::invalid;
}

csapi::request::~request() 
{ 
	printf("%s\n", __PRETTY_FUNCTION__);
	disconnect();
}

crequest::type csapi::request::request_type() {
	if (auto&& msg = get(); msg) {
		return (crequest::type)msg->type;
	}
	return crequest::invalid;
}

const ci::cstringview& csapi::request::request_uri() {
	return msgUri;
}

const crequest::headers& csapi::request::request_headers() {
	return msgHeaders;
}

const crequest::payload& csapi::request::request_paload() {
	return msgPayload;
}

ssize_t csapi::request::request_paload_length() {
	if (auto&& msg = get(); msg && msg->type > crequest::unsupported) {
		return msg->tlen - msg->poffset;
	}
	return 0;
}

ssize_t csapi::request::response(const payload& data, const std::string& uri, std::size_t msg_code, crequest::type msg_type, const crequest::response_headers& headers_list) {
	std::size_t payload_length = 0;
	std::size_t headers_length = uri.length() + 1;

	for (auto&& p : headers_list) {
		headers_length += p.first.length() + 2 + p.second.length() + 1 /* always zero endian data string */;
	}

	for (auto&& p : data) {
		payload_length += p.length();
	}
	auto&& msg = (csapi::header*) alloca(headers_length + sizeof(csapi::header) + payload_length);
#pragma GCC diagnostic ignored "-Wconversion"
	msg->poffset = headers_length + sizeof(csapi::header);
	msg->tlen = headers_length + sizeof(csapi::header) + payload_length;
	msg->code = msg_code;
	msg->type = msg_type;
	msg->argc = 1 /* uri is first argv[0] */ + headers_list.size();
#pragma GCC diagnostic warning "-Wconversion"
	uint8_t* buffer = msg->buffer;

	{	// uri add
		memcpy(buffer, uri.data(), uri.length());
		buffer += uri.length() + 1;
		*(buffer - 1) = '\0';
	}
	for (auto&& p : headers_list) {
		memcpy(buffer, p.first.data(), p.first.length());
		buffer += p.first.length() + 2;
		memcpy(buffer - 2, ": ", 2);
		memcpy(buffer, p.second.data(), p.second.length());
		buffer += p.second.length() + 1;
		*(buffer - 1) = '\0';
	}

	for (auto&& p : data) {
		memcpy(buffer, p.data(), p.length());
		buffer += p.length();
	}

	if (auto&& result = ::send(msgSocket, msg, msg->tlen, 0); result == msg->tlen) {
		return 200;
	}
	fprintf(stderr, "[sapi-server(%s)]. error response send (%s)\n", strerror(errno));
	return 523;
}

ssize_t csapi::request::response(const ci::cstringformat& data, const std::string& uri, std::size_t msg_code, crequest::type msg_type, const response_headers& headers_list) {
	std::size_t payload_length = 0;
	std::size_t headers_length = uri.length() + 1;

	for (auto&& p : headers_list) {
		headers_length += p.first.length() + 2 + p.second.length() + 1 /* always zero endian data string */;
	}

	for (auto&& p : data) {
		payload_length += p.length();
	}
	auto&& msg = (csapi::header*) alloca(headers_length + sizeof(csapi::header) + payload_length);
#pragma GCC diagnostic ignored "-Wconversion"
	msg->poffset = headers_length + sizeof(csapi::header);
	msg->tlen = headers_length + sizeof(csapi::header) + payload_length;
	msg->code = msg_code;
	msg->type = msg_type;
	msg->argc = 1 /* uri is first argv[0] */ + headers_list.size();
#pragma GCC diagnostic warning "-Wconversion"

	uint8_t* buffer = msg->buffer;

	{	// uri add
		memcpy(buffer, uri.data(), uri.length());
		buffer += uri.length() + 1;
		*(buffer - 1) = '\0';
	}
	for (auto&& p : headers_list) {
		memcpy(buffer, p.first.data(), p.first.length());
		buffer += p.first.length() + 2;
		memcpy(buffer - 2, ": ", 2);
		memcpy(buffer, p.second.data(), p.second.length());
		buffer += p.second.length() + 1;
		*(buffer - 1) = '\0';
	}

	for (auto&& p : data) {
		memcpy(buffer, p.data(), p.length());
		buffer += p.length();
	}

	if (auto&& result = ::send(msgSocket, msg, msg->tlen, 0); result == msg->tlen) {
		return 200;
	}
	fprintf(stderr, "[sapi-server(%s)]. error response send (%s)\n", strerror(errno));
	return 523;
}