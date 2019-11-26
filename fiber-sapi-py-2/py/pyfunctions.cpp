#include <python3.6m/Python.h>
#include "../sapi/fparameters.h"

extern fparameters parameters;

static PyObject* fiber_header_option(PyObject *self, PyObject *args)
{
	char *option = nullptr, *value = nullptr;

	if (!PyArg_ParseTuple(args, "s|s", &option, &value))
		return NULL;
	if (option) {
		auto&& result = parameters.GetHeaderOption(option, value ? value : (char*)"");
		return Py_BuildValue("s", result.c_str());
	}

	return Py_BuildValue("");
}

static PyObject* fiber_geturi(PyObject *self, PyObject *args)
{
	return Py_BuildValue("s", parameters.GetUri().c_str());
}

static PyObject* fiber_getmsgid(PyObject *self, PyObject *args)
{
	return Py_BuildValue("s", parameters.msgid().c_str());
}


static PyObject* fiber_getpayload(PyObject *self, PyObject *args)
{
	auto&& data = parameters.GetPayload();
	return Py_BuildValue("y#", data.data(), data.size());
}

static PyObject* fiber_putresponse(PyObject *self, PyObject *args) 
{
	char *content = nullptr, *code = nullptr;
	int content_legth = 0;
		if (!PyArg_ParseTuple(args, "y#|s", &content,&content_legth, &code))
			return NULL;
	parameters.PutResponse(code, std::string(content, content_legth));
	return Py_BuildValue("i", 1);
}

PyMethodDef FiberMethods[] = {
	{"header_option", fiber_header_option, METH_VARARGS, "Get request HTTP header option"},
	{"geturi", fiber_geturi, METH_NOARGS, "Get request uri"},
	{"getmsgid", fiber_getmsgid, METH_NOARGS, "Get request message id"},
	{"getpayload", fiber_getpayload, METH_NOARGS, "Get request payload"},
	{"putresponse", fiber_putresponse, METH_VARARGS, "Put response"},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef fiber_module = {
	PyModuleDef_HEAD_INIT,
	"fiber",   /* name of module */
	NULL, /* module documentation, may be NULL */
	-1,       /* size of per-interpreter state of the module,
				 or -1 if the module keeps state in global variables. */
	FiberMethods
};

PyMODINIT_FUNC PyInit_fiber_module(void)
{
	return PyModule_Create(&fiber_module);
}