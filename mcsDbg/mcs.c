#include <Python.h>
#include <structmember.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "follow.h"

typedef struct {
	PyObject_HEAD

	mcs_parameters persistent_pars;
} _mcs_McsParamsObject;

static PyObject *_mcs_get_bool(int *ptr) {
	return PyBool_FromLong(*ptr);
}

static int _mcs_set_bool(int *ptr, PyObject *value) {
	if(!PyBool_Check(value)) {
		PyErr_SetString(PyExc_ValueError, "Not a boolean value");
		return -1;
	}
	*ptr = (value == Py_True) ? 1 : 0;

	return 0;
}

static PyObject *_mcs_get_double(double *ptr) {
	return Py_BuildValue("d", *ptr);
}

static double _mcs_set_double(double *ptr, PyObject *value) {
	if(PyFloat_Check(value)) {
		*ptr = PyFloat_AsDouble(value);
	}
	else if (PyInt_Check(value)) {
		*ptr = (double)PyInt_AsLong(value);
	}
	else if (PyLong_Check(value)) {
		*ptr = PyLong_AsDouble(value);
	}
	else {
		PyObject *repr;
		char *message;

		repr = PyObject_Repr(value);
		if (asprintf(&message, "Invalid value %s", PyString_AsString(repr)) != -1) {
			PyErr_SetString(PyExc_ValueError, message);
			free(message);
		}
		else {
			PyErr_SetString(PyExc_ValueError, "Invalid value <unknown>");
		}
		Py_DECREF(repr);
		return -1;
	}

	return 0;
}

static PyObject *_mcs_get_double_arr(double ptr[], unsigned sz) {
	unsigned elements = sz / sizeof(double);
	unsigned i;

	PyObject *lst = PyList_New(elements);
	for (i = 0; i < elements; i++) {
		PyList_SET_ITEM(lst, i, Py_BuildValue("d", ptr[i]));
	}
	return lst;
}

static double _mcs_set_double_arr(double ptr[], unsigned sz, PyObject *value) {
	unsigned elements = sz / sizeof(double);
	unsigned i;

	if(!PySequence_Check(value)) {
		PyErr_SetString(PyExc_ValueError, "Invalid value (not a sequence)");
		return -1;
	} if (PySequence_Length(value) != elements) {
		char message[50];
		sprintf(message, "Invalid length %ld", PySequence_Length(value));
		PyErr_SetString(PyExc_ValueError, message);
		return -1;
	}
	for (i = 0; i < elements; i++) {
		int ret;
		PyObject *item = PySequence_GetItem(value, i);

		ret = _mcs_set_double(&ptr[i], item);
		Py_XDECREF(item);
		if (ret == -1)
			return -1;
	}

	return 0;
}


#define PY_ATTR_GETSET(NAME, TYPE) \
static PyObject *_mcs_McsParams_ ## NAME ## _getter(PyObject *self, void *closure) {\
	mcs_parameters *p = &((_mcs_McsParamsObject *)self)->persistent_pars;\
	return _mcs_get_ ## TYPE (&p->NAME);\
}\
static int _mcs_McsParams_ ## NAME ## _setter(PyObject *self, PyObject *value, void *closure) {\
	mcs_parameters *p = &((_mcs_McsParamsObject *)self)->persistent_pars;\
	return _mcs_set_ ## TYPE (&p->NAME, value);\
}

#define PY_ATTR_GETSET_ARR(NAME, TYPE) \
static PyObject *_mcs_McsParams_ ## NAME ## _getter(PyObject *self, void *closure) {\
	mcs_parameters *p = &((_mcs_McsParamsObject *)self)->persistent_pars;\
	return _mcs_get_ ## TYPE ## _arr (p->NAME, sizeof(p->NAME));\
}\
static int _mcs_McsParams_ ## NAME ## _setter(PyObject *self, PyObject *value, void *closure) {\
	mcs_parameters *p = &((_mcs_McsParamsObject *)self)->persistent_pars;\
	return _mcs_set_ ## TYPE ## _arr (p->NAME, sizeof(p->NAME), value);\
}

#define PY_TP_GETSET(NAME) { #NAME, _mcs_McsParams_ ## NAME ## _getter, _mcs_McsParams_ ## NAME ## _setter }

PY_ATTR_GETSET(firstAzFit, bool)
PY_ATTR_GETSET(firstElFit, bool)
PY_ATTR_GETSET_ARR(prevAzDemand, double)
PY_ATTR_GETSET_ARR(prevElDemand, double)

static PyGetSetDef _mcs_McsParams_getsetters[] = {
	PY_TP_GETSET(firstAzFit),
	PY_TP_GETSET(firstElFit),
	PY_TP_GETSET(prevAzDemand),
	PY_TP_GETSET(prevElDemand),
	{NULL} // Sentinel
};

static PyMemberDef _mcs_McsParams_members[] = {
	{"azA", T_DOUBLE, offsetof(_mcs_McsParamsObject, persistent_pars.azA), 0, NULL},
	{"azB", T_DOUBLE, offsetof(_mcs_McsParamsObject, persistent_pars.azB), 0, NULL},
	{"azC", T_DOUBLE, offsetof(_mcs_McsParamsObject, persistent_pars.azC), 0, NULL},
	{"lastAzVelocity", T_DOUBLE, offsetof(_mcs_McsParamsObject, persistent_pars.lastAzVelocity), 0, NULL},
	{"prevAzVel", T_DOUBLE, offsetof(_mcs_McsParamsObject, persistent_pars.prevAzVel), 0, NULL},
	{"elA", T_DOUBLE, offsetof(_mcs_McsParamsObject, persistent_pars.elA), 0, NULL},
	{"elB", T_DOUBLE, offsetof(_mcs_McsParamsObject, persistent_pars.elB), 0, NULL},
	{"elC", T_DOUBLE, offsetof(_mcs_McsParamsObject, persistent_pars.elC), 0, NULL},
	{"lastElVelocity", T_DOUBLE, offsetof(_mcs_McsParamsObject, persistent_pars.lastElVelocity), 0, NULL},
	{"prevElVel", T_DOUBLE, offsetof(_mcs_McsParamsObject, persistent_pars.prevElVel), 0, NULL},
	{NULL} // Sentinel
};

static int
_mcs_McsParams_init(_mcs_McsParamsObject *self, PyObject *args, PyObject *kwds) {
	if ((PySequence_Length(args) > 0)  || (kwds != NULL)) {
		PyErr_SetString(PyExc_TypeError, "McsParams.__init__ takes no parameters");
		return -1;
	}

	self->persistent_pars.firstAzFit = 1;
	self->persistent_pars.firstElFit = 1;

	return 0;
}

static PyTypeObject _mcs_McsParamsType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"_mcs.McsParams",
	sizeof(_mcs_McsParamsObject),
	0,                         /* tp_itemsize */
	0,                         /* tp_dealloc */
	0,                         /* tp_print */
	0,                         /* tp_getattr */
	0,                         /* tp_setattr */
	0,                         /* tp_compare */
	0,                         /* tp_repr */
	0,                         /* tp_as_number */
	0,                         /* tp_as_sequence */
	0,                         /* tp_as_mapping */
	0,                         /* tp_hash */
	0,                         /* tp_call */
	0,                         /* tp_str */
	0,                         /* tp_getattro */
	0,                         /* tp_setattro */
	0,                         /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,        /* tp_flags */
	"MCS Calc Simulation Persistent Parameters",           /* tp_doc */
	0,                         /* tp_traverse */
	0,                         /* tp_clear */
	0,                         /* tp_richcompare */
	0,                         /* tp_weaklistoffset */
	0,                         /* tp_iter */
	0,                         /* tp_iternext */
	0,                         /* tp_methods */
	_mcs_McsParams_members,    /* tp_members */
	_mcs_McsParams_getsetters, /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)_mcs_McsParams_init, /* tp_init */
};

static PyObject *
iface_mcs_sim_fillBuffer(PyObject *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {
		"params", "demands", "offset", "jump", "max_vel", "max_acc",
		"curr_pos", "curr_vel", "recent", NULL
	};

	_mcs_McsParamsObject *mcs_params;
	PyObject *dem[3];
	double AA[2], BB[2], CC[2];
	int axis;
	int recent;
	double offset;
	double jump;
	double max_vel, max_acc;
	double curr_pos, curr_vel;
	PyObject *ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!(OOO)", kwlist,
			&_mcs_McsParamsType, &mcs_params,
			&dem[0], &dem[1], &dem[2],
			&axis, &offset, &jump,
			&max_vel, &max_acc,
			&curr_pos, &curr_vel,
			&recent))
		return NULL;

	{
		PyObject *tuple;
		PyObject *item;
		int i, j, ret;
		double *arr;
		char *message;

		for (i = 0; i < 3; i++) {
			tuple = dem[i];
			if (!PySequence_Check(tuple) || (PySequence_Length(tuple) != 2)) {
				if (asprintf(&message, "Demand #%d is not a 2-element sequence", i) != -1) {
					PyErr_SetString(PyExc_ValueError, message);
					free(message);
				} else {
					PyErr_SetString(PyExc_ValueError, "One of the demands is not a 2-element sequence");
				}
				return NULL;
			}

			switch (i) {
				case 0:
					arr = AA;
					break;
				case 1:
					arr = BB;
					break;
				case 2:
					arr = CC;
					break;
				default:
					// Should never happen...
					fputs("Something is seriously wrong with this loop...", stderr);
					abort();
					break;
			}

			for (j = 0; j < 2; j++) {
				item = PySequence_GetItem(tuple, j);
				ret = _mcs_set_double(&arr[j], item);
				Py_DECREF(item);
				if (ret != 0)
					return NULL;
			}
		}
	}

	{
		double pos[NUM_EXTRAP];
		double vel[NUM_EXTRAP];
		double prevDemand;
		PyObject *ret, *demand_tuple;
		int i;

		if (fillBuffer(AA, BB, CC, pos, vel, offset, axis, &prevDemand, jump,
			   max_vel, max_acc, curr_pos, curr_vel, 0, recent,
			   &mcs_params->persistent_pars) == 1)
		{
			PyErr_SetString(PyExc_RuntimeError, "TCS has not connected");
			return NULL;
		}

		/* TODO: Check for errors in the following code... */
		ret = PyTuple_New(2);
		demand_tuple = PyTuple_New(NUM_EXTRAP);
		PyTuple_SET_ITEM(ret, 0, Py_BuildValue("d", prevDemand));
		PyTuple_SET_ITEM(ret, 1, demand_tuple);
		for (i = 0; i < NUM_EXTRAP; i++) {
			PyTuple_SET_ITEM(demand_tuple, i, Py_BuildValue("(dd)", pos[i], vel[i]));
		}
	}

	return ret;
}

static PyMethodDef McsMethods[] = {
	{"fillBuffer", (PyCFunction)iface_mcs_sim_fillBuffer, METH_KEYWORDS,
	 "Extrapolate demands"},
	{NULL, NULL, 0, NULL} // Sentinel
};

PyMODINIT_FUNC
init_mcs(void)
{
	PyObject *mod;

	// Add extras...
	_mcs_McsParamsType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&_mcs_McsParamsType) < 0)
		return;

	mod = Py_InitModule("_mcs", McsMethods);
	if (mod == NULL)
		return;

	Py_INCREF(&_mcs_McsParamsType);
	PyModule_AddObject(mod, "McsParams", (PyObject *)&_mcs_McsParamsType);
}
