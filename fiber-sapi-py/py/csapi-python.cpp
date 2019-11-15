#include "csapi-python.h"
#include <thread>
#include <python3.6m/Python.h>
#include "../core/cexcept.h"
#include <regex>

using namespace fiber;

csapi_python::csapi_python(const core::coptions& options)
	: csapi(options.at("pipe", "/tmp/navekfiber-fpm")), sapiPipe(options.at("pipe", "/tmp/navekfiber-fpm").get()){
	Py_Initialize();
}

csapi_python::~csapi_python() {
	Py_Finalize();
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

	msg.response(ci::cstringformat(),"/", 102, crequest::type::patch, make_headers(std::time(nullptr), std::time(nullptr), msg.request_headers()));

	//std::this_thread::sleep_for(std::chrono::seconds(10));
	auto&& headers = msg.request_headers();
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

ssize_t csapi_python::execute(ci::cstringformat& result,const std::string& msgid, const std::string& method, const std::string& url, const std::string& script, const crequest::payload& payload) {

	try {
		//if (!Py_IsInitialized())
		//Py_Initialize();

		if (FILE* fd = fopen(script.c_str(), "r"); fd != NULL) {

			PyRun_SimpleString("import sys\nfrom io import StringIO\nclass StdoutCatcher:\n	def __init__(self):\n		self.data = ''\n	def write(self, stuff):\n		self.data = self.data + stuff\n	def flush(self):\n		self.data=self.data\ncatcher = StdoutCatcher()\noldstdout = sys.stdout\nsys.stdout = catcher");


			static const std::regex re("\'");
			std::string data("oldstdin = sys.stdin\nsys.stdin=StringIO(\"\"\"");
			for (auto&& s : payload) {
				data.append(std::regex_replace(s.str(), re, R"(\')"));
			}
			data.append("\"\"\")");

			if (PyRun_SimpleString(data.c_str()))
			{
				//Py_Finalize();
				return 404;
			}

			PyObject* m = PyImport_AddModule("__main__");
			if (PyRun_SimpleFileEx(fd, script.c_str(), 1))
			{
				//Py_Finalize();
				return 400;
			}
			int code = 499;
			if (PyObject* catcher = PyObject_GetAttrString(m, "catcher"); catcher) {
				if (PyObject* output = PyObject_GetAttrString(catcher, "data"); output) {
					if (PyObject* decoded = PyUnicode_AsEncodedString(output, "utf-8", "ERROR"); decoded) {
						result.append(PyBytes_AS_STRING(decoded), PyBytes_GET_SIZE(decoded));
						Py_XDECREF(decoded);
					}
					Py_XDECREF(output);
					code = 200;
				}
				Py_XDECREF(catcher);
			}
			PyRun_SimpleString("sys.stdin = oldstdin\nsys.stdout = oldstdout\n");
			//Py_Finalize();
			return code;
		}

		//Py_Finalize();
	}
	catch (...) {
		printf("csapi_python: exception(%s).\n", "internal python error");
	}
	return 404;

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