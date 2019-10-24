#include "csapiserver.h"
#include <list>
#include <sys/ioctl.h>
#include <unistd.h>
#include <endian.h>
#include <cstdarg>
#include "capp.h"
#include <netinet/tcp.h>

using namespace fiber;

csapiserver::csapiserver(const core::coptions& options)
	: core::cserver::pipe(options.at("sapi-pipe","/tmp/.fiber-sapi-pipe").get())
{
	optReceiveTimeout = (time_t)(options.at("receive-timeout", "3000").number());
	optMaxRequestSize = (ssize_t)options.at("max-request-size", "5048576").bytes();
}

void csapiserver::onshutdown() const {
	printf("csapiserver::onshutdown\n");
}

void csapiserver::onclose(int soc) const {
	//if (auto&& cli = requestsClient.find(soc); cli != requestsClient.end()) {
	//	requestsClient.erase(cli);
	//}
	printf("csapiserver::onclose\n");
}

void csapiserver::onconnect(int soc, const sockaddr_storage&, uint64_t time) const {

	printf("csapiserver::onconnect\n");

	//auto&& req = requestsClient.emplace(soc, std::shared_ptr<request>(new request(soc, optReceiveTimeout, time)));
	//if (!req.second) {
	//	req.first->second.get()->reset();
	//	req.first->second->update_time();
	//}
	//else {
	//	int opt = 1;	setsockopt(soc, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
	//	opt = 30;		setsockopt(soc, IPPROTO_TCP, TCP_KEEPINTVL, &opt, sizeof(opt));
	//}
	//printf("onconnect (%ld)\n", soc);
	//struct timeval tv = { optReceiveTimeout / 1000, optReceiveTimeout % 1000 };
	//setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO, (struct timeval*) & tv, sizeof(struct timeval));
}

void csapiserver::ondisconnect(int soc) const {
	printf("csapiserver::ondisconnect\n");
	//printf("ondisconnect (%ld)\n", soc);
	//if (auto&& cli = requestsClient.find(soc); cli != requestsClient.end()) {
	//	requestsClient.erase(cli);
	//}
}

void csapiserver::onidle(uint64_t time) const {
	printf("csapiserver::onidle\n");
}

void csapiserver::ondata(int soc) const {

	csapi::msgheader hdr;

	if (recv(soc, &hdr, sizeof(hdr), MSG_PEEK) == sizeof(hdr)) {
		auto&& msg = csapi::message(hdr.length);

		if (recv(soc, msg.get(), hdr.length, 0) == hdr.length) {
			std::string str(msg->payload, msg->payload + msg->len());

			printf("csapiserver::ondata: %s\n",str.c_str());
		}
	}
	else {
		printf("csapiserver::ondata\n");
	}
	/*
	if (auto&& cli = requestsClient.find(soc); cli != requestsClient.end()) {
		auto&& result = cli->second->get_chunk(optMaxRequestSize);
		cli->second->update_time();
		if (result == 200) {
			capp::broker().enqueue(std::shared_ptr<fiber::crequest>(cli->second));
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
	*/
}

void csapiserver::onwrite(int soc) const {
	printf("csapiserver::ondata\n");
	//printf("onwrite (%ld)\n", soc);
}
