#include "csapi-python.h"
#include <thread>

using namespace fiber;

csapi_python::csapi_python(const core::coptions& options)
	: csapi(options.at("fpm-sapi-pipe", "/tmp/navekfiber-fpm")), sapiPipe(options.at("fpm-sapi-pipe", "/tmp/navekfiber-fpm").get()){

}

csapi_python::~csapi_python() {

}

void csapi_python::onshutdown() {
	printf("csapi_python: %s\n","onshutdown");
}

void csapi_python::onconnect(int sock, const sockaddr_storage& sa) {
	printf("csapi_python: %s(%d)\n", "onconnect", sock);

	csapi::response msg;
	msg.emplace(std::string("CHELLO"));
	msg.reply(sock, "/", 200, crequest::type::connect, { {"X-FIBER-SAPI",banner} });
}

void csapi_python::ondisconnect(int sock) {
	printf("csapi_python: %s(%d)\n", "ondisconnect", sock);
}

void csapi_python::onsend(int sock) {
	printf("csapi_python: %s(%d)\n", "onsend", sock);
}

void csapi_python::ondata(int sock) {
	csapi::request msg(sock);
	switch (msg.request_type())
	{
	case crequest::connect:
		OnCONNECT(msg);
		break;
	case crequest::put:
		OnPUT(msg);
		break;
	default:
		printf("csapi_python: %s(%d) method %d not supported\n", "ondata", sock, msg.request_type());
		msg.response({}, 405, {}, make_headers(std::time(nullptr), std::time(nullptr), msg.request_headers()));
		break;
	}
}

void csapi_python::OnCONNECT(csapi::request& msg) {
	if (msg.http_code() == 202) {
		printf("csapi_python: %s(%s). Connection with %s established.\n", "connected", msg.request_paload().front().str().c_str(), sapiPipe.c_str());
	}
	else {
		printf("csapi_python: %s(%s). Connection with %s rejected.\n", "rejected", msg.request_paload().front().str().c_str(), sapiPipe.c_str());
	}
}
void csapi_python::OnPUT(csapi::request& msg) {
	printf("csapi_python: %s %s(%s)\n", "put", msg.request_uri().trim().str().c_str(), msg.request_paload().front().str().c_str());

	msg.response(ci::cstringformat(),"/", 102, crequest::type::patch, make_headers(std::time(nullptr), std::time(nullptr), msg.request_headers()));

	std::this_thread::sleep_for(std::chrono::seconds(10));

	msg.response(ci::cstringformat(), "/", 100, crequest::type::patch, make_headers(std::time(nullptr), std::time(nullptr), msg.request_headers()));
}

crequest::response_headers csapi_python::make_headers(std::time_t ctime, std::time_t mtime, const crequest::headers& reqeaders, const crequest::response_headers& additional)
{
	tm ctm, mtm;
	localtime_r(&ctime, &ctm);
	localtime_r(&mtime, &mtm);

	crequest::response_headers hdrslist({
		{ "X-FIBER-SAPI", banner },
		{ "X-FIBER-MSG-CTIME",ci::cstringformat("%04d-%02d-%02d %02d:%02d:%02d",ctm.tm_year + 1900,ctm.tm_mon,ctm.tm_mday,ctm.tm_hour,ctm.tm_min,ctm.tm_sec).str() },
		{ "X-FIBER-MSG-MTIME",ci::cstringformat("%04d-%02d-%02d %02d:%02d:%02d",mtm.tm_year + 1900,mtm.tm_mon,mtm.tm_mday,mtm.tm_hour,mtm.tm_min,mtm.tm_sec).str() },
	});

	if (!additional.empty()) {
		hdrslist.insert(additional.begin(), additional.end());
	}

	if (!reqeaders.empty()) {
		for (auto&& h : reqeaders) {
			if (!h.first.empty()) {
				hdrslist.emplace(h.first.trim().str(), h.second.trim().str());
			}
		}
	}
	return hdrslist;
}