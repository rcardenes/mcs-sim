#include <Python.h>

static PyMethodDef McsMethods[] = {
	{NULL, NULL, 0, NULL} // Sentinel
};

PyMODINIT_FUNC
init_mcs(void)
{
	PyObject *mod;

	mod = Py_InitModule("_mcs", McsMethods);
	if (mod == NULL)
		return;

	// Add extras...
}
