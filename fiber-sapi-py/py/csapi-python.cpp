#include "csapi-python.h"

using namespace fiber;

csapi_python::csapi_python(const core::coptions& options)
	: csapi(options.at("fpm-sapi-pipe", "/tmp/navekfiber-fpm")) {

}
csapi_python::~csapi_python() {

}

void csapi_python::onshutdown() {
	printf("csapi_python: %s\n","onshutdown");
}

void csapi_python::onconnect(int sock, const sockaddr_storage& sa) {
	printf("csapi_python: %s(%d)\n", "onconnect", sock);

	csapi::response msg;
	msg.emplace(std::string("HELLO"));
	msg.reply(sock, "/", 200, crequest::type::connect, { {"X-FIBER-SAPI","python"} });
}

void csapi_python::ondisconnect(int sock) {
	printf("csapi_python: %s(%d)\n", "ondisconnect", sock);
}

void csapi_python::onsend(int sock) {
	printf("csapi_python: %s(%d)\n", "onsend", sock);
}

void csapi_python::ondata(int sock) {
	printf("csapi_python: %s(%d)\n", "ondata", sock);

	csapi::request msg(sock);

	switch (msg.request_type())
	{
	case crequest::connect:
		printf("csapi_python: %s(%s)\n", "connected", msg.request_paload().front().str().c_str());
		break;
	default:
		break;
	}
}