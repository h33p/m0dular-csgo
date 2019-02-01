#include "windows_loader.h"
#include <Python.h>

static PyObject* PreRelocateImage(PyObject* self, PyObject* args) {
	int is64Bit;
	const char* inpBuf;
	int count;
	const char* moduleListBuf;
	int moduleListCount;

	if (!PyArg_ParseTuple(args, "ps#s#", &is64Bit, &inpBuf, &count, &moduleListBuf, &moduleListCount))
		return nullptr;

	ModuleList moduleList, moduleList2;

	const char* curModuleListBuf = moduleListBuf;

	uint32_t namesCount = *(uint32_t*)curModuleListBuf;
	curModuleListBuf += sizeof(uint32_t);
	moduleList.names.resize(namesCount);
	memcpy(moduleList.names.data(), curModuleListBuf, namesCount);
	curModuleListBuf += namesCount;
	uint32_t moduleInfoSize = *(uint32_t*)curModuleListBuf;
	curModuleListBuf += sizeof(uint32_t);
	moduleList.modules.resize(moduleInfoSize / sizeof(RemoteModuleInfo));
	memcpy(moduleList.modules.data(), curModuleListBuf, moduleInfoSize);

	WinModule module(inpBuf, count, &moduleList, is64Bit);
	PackedWinModule packedModule(module);

	uint32_t allocSize = 0;
	char* fullCBuf = packedModule.ToBuffer(&allocSize);
	PyObject* fullBuf = PyBytes_FromStringAndSize(fullCBuf, allocSize);
	free(fullCBuf);

	return Py_BuildValue("iO", packedModule.allocSize, fullBuf);
}

static PyObject* RelocateImage(PyObject* self, PyObject* args) {
	uint64_t address;
	int count;
	const char* inpBuf = nullptr;

	if (!PyArg_ParseTuple(args, "is#", &address, &inpBuf, &count))
		return nullptr;

	PackedWinModule packedModule(inpBuf);

	packedModule.PerformRelocations(address);
	packedModule.RunCrypt();

	uint32_t allocSize = 0;
	char* fullCBuf = packedModule.ToBuffer(&allocSize);
	PyObject* fullBuf = PyBytes_FromStringAndSize(fullCBuf, allocSize);
	free(fullCBuf);

	return Py_BuildValue("O", fullBuf);
}

static PyMethodDef relocateMethods[] = {
	{"pre_relocate_image", (PyCFunction)PreRelocateImage, METH_VARARGS, "Perform the first pass at server based module loading."}, {"relocate_image", (PyCFunction)RelocateImage, METH_VARARGS, "Relocate image and prepare for sending to the user."}, {nullptr, nullptr, 0, nullptr}
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
