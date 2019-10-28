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
	case crequest::put:
	{
		std::string execHeader = "X-FIBER-FPM-EXEC";
		uint8_t* beginPtrFile = (*(msg.request_headers().find(execHeader))).second.begin();
		uint8_t* endPtrFile = (*(msg.request_headers().find(execHeader))).second.end();
		int filelen = endPtrFile - beginPtrFile + 1;
		std::unique_ptr<char[]> scriptFile(new char[filelen + 1]);
		memcpy(scriptFile.get(), beginPtrFile, filelen);
		scriptFile.get()[filelen] = '\0';
		Py_Initialize();
		if (!Py_IsInitialized())
		{
			printf("init fail\n");
			ci::cstringformat strf("python intepreter initialization failed");
			msg.response(strf, 500);
			exit(1);
		}
		PyRun_SimpleString("import sys\nfrom io import StringIO\nclass StdoutCatcher:\n	def __init__(self):\n		self.data = ''\n	def write(self, stuff):\n		self.data = self.data + stuff\n	def flush(self):\n		self.data=self.data\ncatcher = StdoutCatcher()\nsys.stdout = catcher");
		std::unique_ptr<char[]> input(new char[msg.request_paload_length() + 46]);
		std::string payload;
		payload.reserve(msg.request_paload_length()); 
		for (auto&& s : msg.request_paload())
			payload.append(s.str());
		sprintf(input.get(), "oldstdin = sys.stdin\nsys.stdin=StringIO('%s')", payload.c_str());
		if (PyRun_SimpleString(input.get()))
		{
			printf("input failed\n");
			ci::cstringformat strf("python intepreter error, input failed");
			msg.response(strf,500);
			exit(4);
		}
		FILE *fd = fopen(scriptFile.get(), "r");
		if (fd == NULL) {
			printf("script file opening failed\n");
			ci::cstringformat strf("script file opening failed");
			msg.response(strf, 500);
			exit(2);
		}
		PyObject* m = PyImport_AddModule("__main__");
		if (PyRun_SimpleFile(fd, "name"))
		{
			printf("script running failed\n");
			ci::cstringformat strf("python intepreter error, script running failed");
			msg.response(strf, 500);
			exit(3);
		}
		PyObject* catcher = PyObject_GetAttrString(m, "catcher");
		PyObject* output = PyObject_GetAttrString(catcher, "data");
		char *buffer = strdup(PyBytes_AS_STRING(PyUnicode_AsEncodedString(output, "utf-8", "ERROR")));
	/*	if (buffer != NULL)
			printf("data in child process was: %s\n", buffer);
		else
			printf("error in data fetching from python script");*/
		ci::cstringformat coutput(buffer);
		msg.response(coutput,200);
		fclose(fd);
		Py_XDECREF(catcher);
		Py_XDECREF(output);
		Py_Finalize();
	}
		break;
	case crequest::connect:
		printf("csapi_python: %s(%s)\n", "connected", msg.request_paload().front().str().c_str());
		break;
	default:
		break;
	}
}