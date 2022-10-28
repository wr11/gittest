
/* Buffer object interface */

/* Note: the object's structure is private */

#ifndef Py_ARRAYOBJECT_H
#define Py_ARRAYOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

struct PyArrayObject;

typedef struct PyArrayDescr {
	int typecode;
	int itemsize;
	PyObject * (*getitem)(struct PyArrayObject *, Py_ssize_t);
	int (*setitem)(struct PyArrayObject *, Py_ssize_t, PyObject *);
} PyArrayDescr;

typedef struct PyArrayObject {
	PyObject_VAR_HEAD
	char *ob_item;
	Py_ssize_t allocated;
	struct PyArrayDescr *ob_descr;
	PyObject *weakreflist; /* List of weak references */
} PyArrayObject;

PyAPI_DATA(PyTypeObject) PyArray_Type;
PyAPI_DATA(struct PyArrayDescr) PyArrayDescriptors[];

#define PyArray_Check(op) ((op)->ob_type == &PyArray_Type)

PyAPI_FUNC(PyObject *) PyArray_New(Py_ssize_t size, struct PyArrayDescr *descr);

#ifdef __cplusplus
}
#endif
#endif /* !Py_ARRAYOBJECT_H */
