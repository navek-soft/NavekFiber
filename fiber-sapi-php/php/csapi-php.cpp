#include "csapi-php.h"
#include <thread>
#include "../core/cexcept.h"
#include <regex>

using namespace fiber;

extern "C" void php_embed_init();
extern "C" void php_embed_shutdown();
extern int php_embed_execute(ci::cstringformat&, const std::string&, const std::string&, const std::string&, const std::string&, const crequest::payload&);
extern void php_embed_param_init(fiber::crequest::headers, std::string, std::string, fiber::crequest::payload);

csapi_php::csapi_php(const core::coptions& options)
	: csapi(options.at("pipe", "/tmp/navekfiber-fpm")), sapiPipe(options.at("pipe", "/tmp/navekfiber-fpm").get()){
	php_embed_init();
}

csapi_php::~csapi_php() {
	//Py_Finalize();

	php_embed_shutdown();
}

void csapi_php::onshutdown() {
	printf("csapi_php: %s\n","onshutdown");
}

void csapi_php::onconnect(int sock, const sockaddr_storage& sa) {
	printf("csapi_php: %s(%d)\n", "onconnect", sock);

	csapi::response msg;
	msg.emplace(std::string("CHELLO"));
	msg.reply(sock, "/", 200, crequest::type::connect, { {"X-FIBER-SAPI",banner} });
}

void csapi_php::ondisconnect(int sock) {
	printf("csapi_php: %s(%d)\n", "ondisconnect", sock);
}

void csapi_php::onsend(int sock) {
	printf("csapi_php: %s(%d)\n", "onsend", sock);
}

void csapi_php::ondata(int sock) {
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
		printf("csapi_php: %s(%d) method %d not supported\n", "ondata", sock, msg.request_type());
		msg.response({}, 405, {}, make_headers(std::time(nullptr), std::time(nullptr), msg.request_headers()));
		break;
	}
}

void csapi_php::OnCONNECT(csapi::request& msg) {
	if (msg.http_code() == 202) {
		printf("csapi_php: %s(%s). Connection with %s established.\n", "connected", msg.request_paload().front().str().c_str(), sapiPipe.c_str());
	}
	else {
		printf("csapi_php: %s(%s). Connection with %s rejected.\n", "rejected", msg.request_paload().front().str().c_str(), sapiPipe.c_str());
	}
}

void csapi_php::OnPUT(csapi::request& msg) {
	msg.response(ci::cstringformat(),"/", 102, crequest::type::patch, make_headers(std::time(nullptr), std::time(nullptr), msg.request_headers()));
	//std::this_thread::sleep_for(std::chrono::seconds(10));
	auto&& headers = msg.request_headers();
	php_embed_param_init(headers, msg.request_uri().trim().str(), headers.at({ "x-fiber-msg-id", 14 }).trim().str(), msg.request_paload());
	ci::cstringformat result;
	auto code = execute(result,
		headers.at({ "x-fiber-msg-id", 14 }).trim().str(),
		"PUT",
		msg.request_uri().trim().str(),
		headers.at({ "x-fiber-fpm-exec", 16 }).trim().str(),
		msg.request_paload());

	msg.response(result, msg.request_uri().trim().str(), 100, crequest::type::post, make_headers(std::time(nullptr), std::time(nullptr), msg.request_headers()));
	printf("csapi_python: %s(%s) %ld\n", "put", msg.request_uri().trim().str().c_str(), code);
}

ssize_t csapi_php::execute(ci::cstringformat& result,const std::string& msgid, const std::string& method, const std::string& url, const std::string& script, const crequest::payload& payload) {
	return php_embed_execute(result, msgid, method, url, script, payload);
}

crequest::response_headers csapi_php::make_headers(std::time_t ctime, std::time_t mtime, const crequest::headers& reqeaders, const crequest::response_headers& additional)
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