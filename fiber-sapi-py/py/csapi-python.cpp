#include "csapi-python.h"

using namespace fiber;

csapi_python::csapi_python(const core::coptions& options)
	: csapi(options.at("fpm-pipe", "/tmp/navekfiber-fpm-python")) {

}
csapi_python::~csapi_python() {

}

void csapi_python::onshutdown() {
	printf("csapi_python: %s\n","onshutdown");
}

void csapi_python::onconnect(int sock, const sockaddr_storage& sa) {
	printf("csapi_python: %s(%d)\n", "onconnect", sock);
}

void csapi_python::ondisconnect(int sock) {
	printf("csapi_python: %s(%d)\n", "ondisconnect", sock);
}

void csapi_python::onsend(int sock) {
	printf("csapi_python: %s(%d)\n", "onsend", sock);
}

void csapi_python::ondata(int sock) {
	printf("csapi_python: %s(%d)\n", "ondata", sock);

	csapi::request request(sock);

	if (request.request_type() > 2) {

	}
}