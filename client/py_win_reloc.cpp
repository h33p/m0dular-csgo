#include "windows_loader.h"
#include <Python.h>

static PyObject* RelocateImage(PyObject* self, PyObject* args) {
	int is64Bit;
	uintptr_t address;
	const char* inpBuf;
	int count;

	if (!PyArg_ParseTuple(args, "ips#", &address, &is64Bit, &inpBuf, &count))
		return nullptr;

	WinModule module(inpBuf, count, nullptr, is64Bit);
	PackedWinModule packedModule(module);

	PyObject* classBuf = PyBytes_FromStringAndSize((char*)&packedModule, sizeof(packedModule) - 2 * sizeof(char*));
	PyObject* modBuf = PyBytes_FromStringAndSize(packedModule.moduleBuffer, packedModule.modBufSize);
	PyObject* dataBuf = PyBytes_FromStringAndSize(packedModule.buffer, packedModule.bufSize);

	return Py_BuildValue("OOO", classBuf, modBuf, dataBuf);
}

static PyMethodDef relocateMethods[] = {
	{"relocate_image", (PyCFunction)RelocateImage, METH_VARARGS, "Relocate image and prepare for sending to the user."}, {nullptr, nullptr, 0, nullptr}
};

static struct PyModuleDef relocatemodule = {
	PyModuleDef_HEAD_INIT,
	"relocate",
	nullptr,
	-1,
	relocateMethods
};

PyMODINIT_FUNC PyInit_relocate(void) {
	return PyModule_Create(&relocatemodule);
}
