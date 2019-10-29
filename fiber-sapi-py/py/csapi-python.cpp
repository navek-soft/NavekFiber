#include "csapi-python.h"
using namespace fiber;

csapi_python::csapi_python(const core::coptions& options)
	: csapi(options.at("fpm-sapi-pipe", "/tmp/navekfiber-fpm")) {
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
	case crequest::put:
	{
		std::string scriptFile;
		auto&& headers = msg.request_headers();
		if (auto&& h = headers.find({ "X-FIBER-FPM-EXEC",16 }); h != headers.end())
			scriptFile = h->second.str();
		auto&& fpmHeader = headers.find({ "X-FIBER-FPM", 11 });
		auto&& msgId = headers.find({ "X-FIBER-MSG-ID", 14 });
		if (!Py_IsInitialized())
			Py_Initialize();
		PyRun_SimpleString("import sys\nfrom io import StringIO\nclass StdoutCatcher:\n	def __init__(self):\n		self.data = ''\n	def write(self, stuff):\n		self.data = self.data + stuff\n	def flush(self):\n		self.data=self.data\ncatcher = StdoutCatcher()\noldstdout = sys.stdout\nsys.stdout = catcher");
		std::unique_ptr<char[]> input(new char[msg.request_paload_length() + 46]);
		std::string payload;
		payload.reserve(msg.request_paload_length()); 
		for (auto&& s : msg.request_paload())
			payload.append(s.str());
		sprintf(input.get(), "oldstdin = sys.stdin\nsys.stdin=StringIO('%s')", payload.c_str());
		if (PyRun_SimpleString(input.get()))
		{
			printf("input failed\n");
			msg.response("python intepreter error, input failed", 500, "Internal Server Error", {
							{"X-FIBER-FPM",fpmHeader->second.str()},
							{"X-FIBER-MSG-ID",msgId->second.str()},
				});
			Py_Finalize();
			return;
		}
		FILE *fd = fopen(scriptFile.c_str(), "r");
		if (fd == NULL) {
			printf("script file opening failed\n");
			msg.response("script file opening failed", 404, "Not Found", {
							{"X-FIBER-FPM",fpmHeader->second.str()},
							{"X-FIBER-MSG-ID",msgId->second.str()},
				});
			return;
		}
		PyObject* m = PyImport_AddModule("__main__");
		if (PyRun_SimpleFile(fd, "name"))
		{
			printf("script running failed\n");
			msg.response("python intepreter error, script running failed", 500, "Internal Server Error", {
							{"X-FIBER-FPM",fpmHeader->second.str()},
							{"X-FIBER-MSG-ID",msgId->second.str()},
				});
			Py_Finalize();
			return;
		}
		PyObject* catcher = PyObject_GetAttrString(m, "catcher");
		PyObject* output = PyObject_GetAttrString(catcher, "data");
		char *buffer = strdup(PyBytes_AS_STRING(PyUnicode_AsEncodedString(output, "utf-8", "ERROR")));
		ci::cstringformat coutput(buffer);
		msg.response(coutput, 200, "OK", {
							{"X-FIBER-FPM",fpmHeader->second.str()},
							{"X-FIBER-MSG-ID",msgId->second.str()},
			});
		PyRun_SimpleString("sys.stdin = oldstdin\nsys.stdout = oldstdout\n");
		fclose(fd);
		Py_XDECREF(catcher);
		Py_XDECREF(output);
	}
		break;
	case crequest::connect:
		printf("csapi_python: %s(%s)\n", "connected", msg.request_paload().front().str().c_str());
		break;
	default:
		break;
	}
}