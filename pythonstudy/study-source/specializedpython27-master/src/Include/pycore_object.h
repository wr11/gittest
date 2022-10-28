#ifndef Py_INTERNAL_OBJECT_H
#define Py_INTERNAL_OBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "objimpl.h"
// #include "pycore_gc.h"            // _PyObject_GC_IS_TRACKED()
// #include "pycore_interp.h"        // PyInterpreterState.gc
// #include "pycore_pystate.h"       // _PyInterpreterState_GET()

// Fast inlined version of PyType_HasFeature()
static inline int
_PyType_HasFeature(PyTypeObject *type, unsigned long feature) {
    return ((type->tp_flags & feature) != 0);
}

// extern void _PyType_InitCache(PyInterpreterState *interp);

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_OBJECT_H */
