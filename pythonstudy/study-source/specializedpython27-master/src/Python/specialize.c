// #pragma optimize("", off)

#include "Python.h"
#include "opcode.h"
#include "pycore_code.h"
#include "pycore_dict.h"
#include "pycore_object.h"
#include "structmember.h"         // struct PyMemberDef, T_OFFSET_EX

#include <stdlib.h> // rand()

/* For guidance on adding or extending families of instructions see
 * ./adaptive.md
 */


 /* We layout the quickened data as a bi-directional array:
  * Instructions upwards, cache entries downwards.
  * first_instr is aligned to a SpecializedCacheEntry.
  * The nth instruction is located at first_instr[n]
  * The nth cache is located at ((SpecializedCacheEntry *)first_instr)[-1-n]
  * The first (index 0) cache entry is reserved for the count, to enable finding
  * the first instruction from the base pointer.
  * The cache_count argument must include space for the count.
  * We use the SpecializedCacheOrInstruction union to refer to the data
  * to avoid type punning.

  Layout of quickened data, each line 8 bytes for M cache entries and N instructions:

  <cache_count>                              <---- co->co_quickened
  <cache M-1>
  <cache M-2>
  ...
  <cache 0>
  <instr 0> <instr 1> <instr 2> <instr 3>    <--- co->co_first_instr
  <instr 4> <instr 5> <instr 6> <instr 7>
  ...
  <instr N-1>
 */

Py_ssize_t _Py_QuickenedCount = 0;
unsigned long _Py_QuickenedBytes = 0;
#if COLLECT_SPECIALIZATION_STATS
SpecializationStats _specialization_stats[256] = { 0 };

#define ADD_STAT_TO_DICT(res, field) \
    do { \
        PyObject *val = PyLong_FromUnsignedLongLong(stats->field); \
        if (val == NULL) { \
            Py_DECREF(res); \
            return NULL; \
        } \
        if (PyDict_SetItemString(res, #field, val) == -1) { \
            Py_DECREF(res); \
            Py_DECREF(val); \
            return NULL; \
        } \
        Py_DECREF(val); \
    } while(0);

static PyObject*
stats_to_dict(SpecializationStats *stats)
{
	PyObject *res = PyDict_New();
	if (res == NULL) {
		return NULL;
	}
	ADD_STAT_TO_DICT(res, specialization_success);
	ADD_STAT_TO_DICT(res, specialization_failure);
	ADD_STAT_TO_DICT(res, hit);
	ADD_STAT_TO_DICT(res, deferred);
	ADD_STAT_TO_DICT(res, miss);
	ADD_STAT_TO_DICT(res, deopt);
	ADD_STAT_TO_DICT(res, unquickened);
#if COLLECT_SPECIALIZATION_STATS_DETAILED
	PyObject *failure_kinds = PyTuple_New(SPECIALIZATION_FAILURE_KINDS);
	if (failure_kinds == NULL) {
		Py_DECREF(res);
		return NULL;
	}
	for (int i = 0; i < SPECIALIZATION_FAILURE_KINDS; i++) {
		PyObject *stat = PyLong_FromUnsignedLongLong(stats->specialization_failure_kinds[i]);
		if (stat == NULL) {
			Py_DECREF(res);
			Py_DECREF(failure_kinds);
			return NULL;
		}
		PyTuple_SET_ITEM(failure_kinds, i, stat);
	}
	if (PyDict_SetItemString(res, "specialization_failure_kinds", failure_kinds)) {
		Py_DECREF(res);
		Py_DECREF(failure_kinds);
		return NULL;
	}
	Py_DECREF(failure_kinds);
#endif
	return res;
}
#undef ADD_STAT_TO_DICT

static int
add_stat_dict(
	PyObject *res,
	int opcode,
	const char *name) {

	SpecializationStats *stats = &_specialization_stats[opcode];
	PyObject *d = stats_to_dict(stats);
	if (d == NULL) {
		return -1;
	}
	int err = PyDict_SetItemString(res, name, d);
	Py_DECREF(d);
	return err;
}

#if COLLECT_SPECIALIZATION_STATS
PyObject*
_Py_GetSpecializationStats(void) {
	PyObject *stats = PyDict_New();
	if (stats == NULL) {
		return NULL;
	}
	int err = 0;
	err += add_stat_dict(stats, LOAD_ATTR, "load_attr");
	err += add_stat_dict(stats, LOAD_GLOBAL, "load_global");
	err += add_stat_dict(stats, LOAD_METHOD, "load_method");
	err += add_stat_dict(stats, BINARY_ADD, "binary_add");
	err += add_stat_dict(stats, BINARY_MULTIPLY, "binary_multiply");
	err += add_stat_dict(stats, BINARY_SUBSCR, "binary_subscr");
	err += add_stat_dict(stats, STORE_ATTR, "store_attr");
	err += add_stat_dict(stats, CALL_FUNCTION, "call_function");
	if (err < 0) {
		Py_DECREF(stats);
		return NULL;
	}
	return stats;
}
#endif


#define PRINT_STAT(name, field) fprintf(out, "    %s." #field " : %" PRIu64 "\n", name, stats->field);

static void
print_stats(FILE *out, SpecializationStats *stats, const char *name)
{
	PRINT_STAT(name, specialization_success);
	PRINT_STAT(name, specialization_failure);
	PRINT_STAT(name, hit);
	PRINT_STAT(name, deferred);
	PRINT_STAT(name, miss);
	PRINT_STAT(name, deopt);
	PRINT_STAT(name, unquickened);
#if PRINT_SPECIALIZATION_STATS_DETAILED
	for (int i = 0; i < SPECIALIZATION_FAILURE_KINDS; i++) {
		fprintf(out, "    %s.specialization_failure_kinds[%d] : %" PRIu64 "\n",
			name, i, stats->specialization_failure_kinds[i]);
	}
#endif
}
#undef PRINT_STAT

void
_Py_PrintSpecializationStats(void)
{
	FILE *out = stderr;
#if PRINT_SPECIALIZATION_STATS_TO_FILE
	/* Write to a file instead of stderr. */
# ifdef MS_WINDOWS
	const char *dirname = "c:\\temp\\py_stats\\";
# else
	const char *dirname = "/tmp/py_stats/";
# endif
	char buf[48];
	sprintf(buf, "%s%u_%u.txt", dirname, (unsigned)clock(), (unsigned)rand());
	FILE *fout = fopen(buf, "w");
	if (fout) {
		out = fout;
	}
#else
	fprintf(out, "Specialization stats:\n");
#endif
	print_stats(out, &_specialization_stats[LOAD_ATTR], "load_attr");
	print_stats(out, &_specialization_stats[LOAD_GLOBAL], "load_global");
	print_stats(out, &_specialization_stats[LOAD_METHOD], "load_method");
	print_stats(out, &_specialization_stats[BINARY_ADD], "binary_add");
	print_stats(out, &_specialization_stats[BINARY_MULTIPLY], "binary_multiply");
	print_stats(out, &_specialization_stats[BINARY_SUBSCR], "binary_subscr");
	print_stats(out, &_specialization_stats[STORE_ATTR], "store_attr");
	print_stats(out, &_specialization_stats[CALL_FUNCTION], "call_function");
	if (out != stderr) {
		fclose(out);
	}
}

#if COLLECT_SPECIALIZATION_STATS_DETAILED

#define SPECIALIZATION_FAIL(opcode, kind) _specialization_stats[opcode].specialization_failure_kinds[kind]++


#endif
#endif

#ifndef SPECIALIZATION_FAIL
#define SPECIALIZATION_FAIL(opcode, kind) ((void)0)
#endif

static SpecializedCacheOrInstruction *
allocate(int cache_count, int instruction_count)
{
	assert(sizeof(SpecializedCacheOrInstruction) == 2 * sizeof(int32_t));
	assert(sizeof(SpecializedCacheEntry) == 2 * sizeof(int32_t));
	assert(cache_count > 0);
	assert(instruction_count > 0);
	int count = cache_count + (instruction_count + INSTRUCTIONS_PER_ENTRY - 1) / INSTRUCTIONS_PER_ENTRY;
	SpecializedCacheOrInstruction *array = (SpecializedCacheOrInstruction *)
		PyMem_Malloc(sizeof(SpecializedCacheOrInstruction) * count);
	if (array == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	_Py_QuickenedCount++;
	array[0].entry.zero.cache_count = cache_count;
	array[0].entry.zero.instruction_count = instruction_count;
	_Py_QuickenedBytes += count * sizeof(SpecializedCacheOrInstruction);

	return array;
}

static int
get_cache_count(SpecializedCacheOrInstruction *quickened) {
	return quickened[0].entry.zero.cache_count;
}

uint8_t g_adaptive_opcode_disables[256] = {
	[LOAD_ATTR] = 0,
	[LOAD_GLOBAL] = 0,
	[BINARY_ADD] = 0,
	[STORE_SUBSCR] = 0,
	[BINARY_SUBSCR] = 0,
	[STORE_ATTR] = 0,
};

uint8_t g_enable_adaptive_overridden_loadattr = 1;
uint8_t g_enable_adaptive_overridden_storeattr = 1;

/* Map from opcode to adaptive opcode.
  Values of zero are ignored. */
static uint8_t adaptive_opcodes[256] = {
	[LOAD_ATTR] = LOAD_ATTR_ADAPTIVE,
	[LOAD_GLOBAL] = LOAD_GLOBAL_ADAPTIVE,
	// [LOAD_METHOD] = LOAD_METHOD_ADAPTIVE,
	[BINARY_ADD] = BINARY_ADD_ADAPTIVE,
	// [BINARY_MULTIPLY] = BINARY_MULTIPLY_ADAPTIVE,
	[STORE_SUBSCR] = STORE_SUBSCR_ADAPTIVE,
	[BINARY_SUBSCR] = BINARY_SUBSCR_ADAPTIVE,
	// [CALL_FUNCTION] = CALL_FUNCTION_ADAPTIVE,
	[STORE_ATTR] = STORE_ATTR_ADAPTIVE,
};

// IMPORTANT IMPORTANT IMPORTANT 
// NOTE: only opcode having argument can have cache!
// IMPORTANT IMPORTANT IMPORTANT 

/* The number of cache entries required for a "family" of instructions. */
static uint8_t cache_requirements[256] = {
	[LOAD_ATTR] = 2, /* _PyAdaptiveEntry and _PyAttrCache */
	[LOAD_GLOBAL] = 1, /* _PyAdaptiveEntry and _PyLoadGlobalCache */
//     [LOAD_METHOD] = 3, /* _PyAdaptiveEntry, _PyAttrCache and _PyObjectCache */
	[BINARY_ADD] = 0,
	//     [BINARY_MULTIPLY] = 0,
	[BINARY_SUBSCR] = 0, /* _PyAdaptiveEntry */
	[STORE_SUBSCR] = 0, /* _PyAdaptiveEntry */
	//     [CALL_FUNCTION] = 2, /* _PyAdaptiveEntry and _PyObjectCache/_PyCallCache */
	[STORE_ATTR] = 2, /* _PyAdaptiveEntry and _PyAttrCache */
};

static_assert(!(HAS_ARG(BINARY_ADD)) && !(HAS_ARG(BINARY_ADD_ADAPTIVE)), "oh no");
static_assert((HAS_ARG(LOAD_ATTR)) && (HAS_ARG(LOAD_ATTR_ADAPTIVE)), "oh no");
static_assert((HAS_ARG(LOAD_GLOBAL)) && (HAS_ARG(LOAD_GLOBAL_ADAPTIVE)), "oh no");
static_assert((HAS_ARG(STORE_ATTR)) && (HAS_ARG(STORE_ATTR_ADAPTIVE)), "oh no");
static_assert(!(HAS_ARG(BINARY_SUBSCR)) && !(HAS_ARG(BINARY_SUBSCR_ADAPTIVE)), "oh no");
static_assert(!(HAS_ARG(STORE_SUBSCR)) && !(HAS_ARG(STORE_SUBSCR_ADAPTIVE)), "oh no");

/* Return the oparg for the cache_offset and instruction index.
 *
 * If no cache is needed then return the original oparg.
 * If a cache is needed, but cannot be accessed because
 * oparg would be too large, then return -1.
 *
 * Also updates the cache_offset, as it may need to be incremented by
 * more than the cache requirements, if many instructions do not need caches.
 *
 * See pycore_code.h for details of how the cache offset,
 * instruction index and oparg are related */
static int
oparg_from_instruction_and_update_offset(int index, int opcode, int original_oparg, int *cache_offset) {
	/* The instruction pointer in the interpreter points to the next
	 * instruction, so we compute the offset using nexti (index + 1) */
	int nexti = index + 1;
	uint8_t need = cache_requirements[opcode];
	if (need == 0) {
		return original_oparg;
	}
	assert(adaptive_opcodes[opcode] != 0);
	int oparg = oparg_from_offset_and_nexti(*cache_offset, nexti);
	assert(*cache_offset == offset_from_oparg_and_nexti(oparg, nexti));
	/* Some cache space is wasted here as the minimum possible offset is (nexti>>1) */
	if (oparg < 0) {
		oparg = 0;
		*cache_offset = offset_from_oparg_and_nexti(oparg, nexti);
	}
	else if (oparg > 255) {
		return -1;
	}
	*cache_offset += need;
	return oparg;
}

static int
entries_needed(const _Py_CODEUNIT *code, int len, int *new_len)
{
	int cache_offset = 0;
	int previous_opcode = -1;
	int i = 0;
	*new_len = 0;
	while (i < len)
	{
		uint8_t opcode = _Py_OPCODE(code + i);

		if (previous_opcode != EXTENDED_ARG) {
			oparg_from_instruction_and_update_offset(i, opcode, 0, &cache_offset);
		}
		previous_opcode = opcode;

		++i;
		if (HAS_ARG(opcode))
		{
			i += 2;
		}
	}
	return cache_offset + 1;   // One extra for the count entry
}

static inline _Py_CODEUNIT *
first_instruction(SpecializedCacheOrInstruction *quickened)
{
	return &quickened[get_cache_count(quickened)].code[0];
}

/** Insert adaptive instructions and superinstructions.
 *
 * Skip instruction preceded by EXTENDED_ARG for adaptive
 * instructions as those are both very rare and tricky
 * to handle.
 */

 // static int
 // oparg_from_instruction_and_update_offset(int index, int opcode, int original_oparg, int *cache_offset) {
 //     /* The instruction pointer in the interpreter points to the next
 //      * instruction, so we compute the offset using nexti (index + 1) */
 //     int nexti = index + 1;
 //     uint8_t need = cache_requirements[opcode];
 //     if (need == 0) {
 //         return original_oparg;
 //     }
 //     assert(adaptive_opcodes[opcode] != 0);
 //     int oparg = oparg_from_offset_and_nexti(*cache_offset, nexti);
 //     assert(*cache_offset == offset_from_oparg_and_nexti(oparg, nexti));
 //     /* Some cache space is wasted here as the minimum possible offset is (nexti>>1) */
 //     if (oparg < 0) {
 //         oparg = 0;
 //         *cache_offset = offset_from_oparg_and_nexti(oparg, nexti);
 //     }
 //     else if (oparg > 255) {
 //         return -1;
 //     }
 //     *cache_offset += need;
 //     return oparg;
 // }

static void
optimize(SpecializedCacheOrInstruction *quickened, int len)
{
	_Py_CODEUNIT *instructions = first_instruction(quickened);
	int cache_offset = 0;
	int previous_opcode = -1;
	int previous_oparg = 0;
	for (int i = 0; i < len; HAS_ARG(instructions[i]) ? i += 3 : i++) {
		int opcode = _Py_OPCODE(instructions + i);
		int oparg = _Py_OPARG(instructions + i);
		uint8_t adaptive_opcode = adaptive_opcodes[opcode];
		uint8_t disable_adaptive_opcode = g_adaptive_opcode_disables[opcode];
		if (adaptive_opcode && previous_opcode != EXTENDED_ARG && !disable_adaptive_opcode) {
			int new_oparg = oparg_from_instruction_and_update_offset(
				i, opcode, oparg, &cache_offset
			);
			if (new_oparg < 0) {
				/* Not possible to allocate a cache for this instruction */
				previous_opcode = opcode;
				continue;
			}
			previous_opcode = adaptive_opcode;
			int entries_needed = cache_requirements[opcode];
			if (entries_needed) {
				/* Initialize the adpative cache entry */
				int cache0_offset = cache_offset - entries_needed;
				SpecializedCacheEntry *cache =
					_GetSpecializedCacheEntry(instructions, cache0_offset);
				cache->adaptive.original_oparg = oparg;
				cache->adaptive.counter = 0;
			}
			else {
				// oparg is the adaptive cache counter
				new_oparg = 0;
			}
			if (HAS_ARG(instructions[i]))
			{
				_Py_MAKECODEUNIT(instructions + i, adaptive_opcode, new_oparg);
			}
			else
			{
				_Py_MAKECODEUNIT_NOARG(instructions + i, adaptive_opcode);
			}
		}
		else {
			/* Super instructions don't use the cache,
			 * so no need to update the offset. */
			 //             switch (opcode) {
			 //                 case JUMP_ABSOLUTE:
			 //                     instructions[i] = _Py_MAKECODEUNIT(JUMP_ABSOLUTE_QUICK, oparg);
			 //                     break;
			 //                 case LOAD_FAST:
			 //                     switch(previous_opcode) {
			 //                         case LOAD_FAST:
			 //                             instructions[i-1] = _Py_MAKECODEUNIT(LOAD_FAST__LOAD_FAST, previous_oparg);
			 //                             break;
			 //                         case STORE_FAST:
			 //                             instructions[i-1] = _Py_MAKECODEUNIT(STORE_FAST__LOAD_FAST, previous_oparg);
			 //                             break;
			 //                         case LOAD_CONST:
			 //                             instructions[i-1] = _Py_MAKECODEUNIT(LOAD_CONST__LOAD_FAST, previous_oparg);
			 //                             break;
			 //                     }
			 //                     break;
			 //                 case STORE_FAST:
			 //                     if (previous_opcode == STORE_FAST) {
			 //                         instructions[i-1] = _Py_MAKECODEUNIT(STORE_FAST__STORE_FAST, previous_oparg);
			 //                     }
			 //                     break;
			 //                 case LOAD_CONST:
			 //                     if (previous_opcode == LOAD_FAST) {
			 //                         instructions[i-1] = _Py_MAKECODEUNIT(LOAD_FAST__LOAD_CONST, previous_oparg);
			 //                     }
			 //                     break;
			 //             }
			previous_opcode = opcode;
			previous_oparg = oparg;
		}
	}
	assert(cache_offset + 1 == get_cache_count(quickened));
}

int
_Py_Quicken(PyCodeObject *code) {
	if (code->co_quickened) {
		return 0;
	}
	Py_ssize_t size = PyString_GET_SIZE(code->co_code);
	int instr_count = (int)(size / sizeof(_Py_CODEUNIT));
	if (instr_count > MAX_SIZE_TO_QUICKEN) {
		code->co_warmup = QUICKENING_WARMUP_COLDEST;
		return 0;
	}
	int new_instr_count = 0;
	int entry_count = entries_needed(code->co_firstinstr, instr_count, &new_instr_count);
	SpecializedCacheOrInstruction *quickened = allocate(entry_count, instr_count);
	if (quickened == NULL) {
		return -1;
	}
	_Py_CODEUNIT *new_instructions = first_instruction(quickened);
	memcpy(new_instructions, code->co_firstinstr, size);
	optimize(quickened, instr_count);
	code->co_quickened = quickened;
	code->co_firstinstr = new_instructions;
	return 0;
}

static inline int
initial_counter_value(void) {
	/* Starting value for the counter.
	 * This value needs to be not too low, otherwise
	 * it would cause excessive de-optimization.
	 * Neither should it be too high, or that would delay
	 * de-optimization excessively when it is needed.
	 * A value around 50 seems to work, and we choose a
	 * prime number to avoid artifacts.
	 */
	return 53;
}

/* Common */

#define SPEC_FAIL_OTHER 0
#define SPEC_FAIL_NO_DICT 1
#define SPEC_FAIL_OVERRIDDEN 2
#define SPEC_FAIL_OUT_OF_VERSIONS 3
#define SPEC_FAIL_OUT_OF_RANGE 4
#define SPEC_FAIL_EXPECTED_ERROR 5

/* Attributes */

#define SPEC_FAIL_NON_STRING_OR_SPLIT 6
#define SPEC_FAIL_MODULE_ATTR_NOT_FOUND 7
#define SPEC_FAIL_OVERRIDING_DESCRIPTOR 8
#define SPEC_FAIL_NON_OVERRIDING_DESCRIPTOR 9
#define SPEC_FAIL_NOT_DESCRIPTOR 10
#define SPEC_FAIL_METHOD 11
#define SPEC_FAIL_MUTABLE_CLASS 12
#define SPEC_FAIL_PROPERTY 13
#define SPEC_FAIL_NON_OBJECT_SLOT 14
#define SPEC_FAIL_READ_ONLY 15
#define SPEC_FAIL_AUDITED_SLOT 16

/* Methods */

#define SPEC_FAIL_IS_ATTR 15
#define SPEC_FAIL_DICT_SUBCLASS 16
#define SPEC_FAIL_BUILTIN_CLASS_METHOD 17
#define SPEC_FAIL_CLASS_METHOD_OBJ 18
#define SPEC_FAIL_OBJECT_SLOT 19

/* Binary subscr */

#define SPEC_FAIL_ARRAY_INT 8
#define SPEC_FAIL_ARRAY_SLICE 9
#define SPEC_FAIL_LIST_SLICE 10
#define SPEC_FAIL_TUPLE_SLICE 11
#define SPEC_FAIL_STRING_INT 12
#define SPEC_FAIL_STRING_SLICE 13
#define SPEC_FAIL_BUFFER_INT 15
#define SPEC_FAIL_BUFFER_SLICE 16
#define SPEC_FAIL_SEQUENCE_INT 17

/* Binary add */

#define SPEC_FAIL_NON_FUNCTION_SCOPE 11
#define SPEC_FAIL_DIFFERENT_TYPES 12

/* Calls */
#define SPEC_FAIL_GENERATOR 7
#define SPEC_FAIL_COMPLEX_PARAMETERS 8
#define SPEC_FAIL_WRONG_NUMBER_ARGUMENTS 9
#define SPEC_FAIL_CO_NOT_OPTIMIZED 10
/* SPEC_FAIL_METHOD  defined as 11 above */
#define SPEC_FAIL_FREE_VARS 12
#define SPEC_FAIL_PYCFUNCTION 13
#define SPEC_FAIL_PYCFUNCTION_WITH_KEYWORDS 14
#define SPEC_FAIL_PYCFUNCTION_FAST_WITH_KEYWORDS 15
#define SPEC_FAIL_PYCFUNCTION_NOARGS 16
#define SPEC_FAIL_BAD_CALL_FLAGS 17
#define SPEC_FAIL_CLASS 18


// static int
// specialize_module_load_attr(
//     PyObject *owner, _Py_CODEUNIT *instr, PyObject *name,
//     _PyAdaptiveEntry *cache0, _PyAttrCache *cache1, int opcode,
//     int opcode_module)
// {
//     PyModuleObject *m = (PyModuleObject *)owner;
//     PyObject *value = NULL;
//     PyObject *getattr;
//     _Py_IDENTIFIER(__getattr__);
//     assert(owner->ob_type->tp_inline_values_offset == 0);
//     PyDictObject *dict = (PyDictObject *)m->md_dict;
//     if (dict == NULL) {
//         SPECIALIZATION_FAIL(opcode, SPEC_FAIL_NO_DICT);
//         return -1;
//     }
//     if (dict->ma_keys->dk_kind != DICT_KEYS_UNICODE) {
//         SPECIALIZATION_FAIL(opcode, SPEC_FAIL_NON_STRING_OR_SPLIT);
//         return -1;
//     }
//     getattr = _PyUnicode_FromId(&PyId___getattr__); /* borrowed */
//     if (getattr == NULL) {
//         SPECIALIZATION_FAIL(opcode, SPEC_FAIL_OVERRIDDEN);
//         PyErr_Clear();
//         return -1;
//     }
//     Py_ssize_t index = _PyDict_GetItemHint(dict, getattr, -1,  &value);
//     assert(index != DKIX_ERROR);
//     if (index != DKIX_EMPTY) {
//         SPECIALIZATION_FAIL(opcode, SPEC_FAIL_MODULE_ATTR_NOT_FOUND);
//         return -1;
//     }
//     index = _PyDict_GetItemHint(dict, name, -1, &value);
//     assert (index != DKIX_ERROR);
//     if (index != (uint16_t)index) {
//         SPECIALIZATION_FAIL(opcode, SPEC_FAIL_OUT_OF_RANGE);
//         return -1;
//     }
//     uint32_t keys_version = _PyDictKeys_GetVersionForCurrentState(dict->ma_keys);
//     if (keys_version == 0) {
//         SPECIALIZATION_FAIL(opcode, SPEC_FAIL_OUT_OF_VERSIONS);
//         return -1;
//     }
//     cache1->dk_version_or_hint = keys_version;
//     cache0->index = (uint16_t)index;
//     *instr = _Py_MAKECODEUNIT(opcode_module, _Py_OPARG(*instr));
//     return 0;
// }



/* Attribute specialization */

typedef enum {
	OVERRIDING, /* Is an overriding descriptor, and will remain so. */
	METHOD, /* Attribute has Py_TPFLAGS_METHOD_DESCRIPTOR set */
	PROPERTY, /* Is a property */
	OBJECT_SLOT, /* Is an object slot descriptor */
	OTHER_SLOT, /* Is a slot descriptor of another type */
	NON_OVERRIDING, /* Is another non-overriding descriptor, and is an instance of an immutable class*/
	BUILTIN_CLASSMETHOD, /* Builtin methods with METH_CLASS */
	PYTHON_STATICMETHOD,
	PYTHON_CLASSMETHOD, /* Python classmethod(func) object */
	NON_DESCRIPTOR, /* Is not a descriptor, and is an instance of an immutable class */
	MUTABLE,   /* Instance of a mutable class; might, or might not, be a descriptor */
	ABSENT, /* Attribute is not present on the class */
	DUNDER_CLASS, /* __class__ attribute */
	GETSET_OVERRIDDEN, /* __getattribute__ or __setattr__ has been overridden */
	ADAPTIVE_OVERRIDDEN_GETSET,
	CLASS_OVERRIDDEN_GETSET,
	GENERAL_FAIL,
} DesciptorClassification;

// liuhan debug only begin
#define GETATTR_TYPE_ADAPTIVE 0
#define GETATTR_TYPE_SLOT_TP_GETATTR_HOOK 1
#define GETATTR_TYPE_TYPE_GETATTRO 2
#define GETATTR_TYPE_INSTANCE_GETATTR 3
#define GETATTR_TYPE_OTHER 4
#define GETATTR_TYPE_MAX 5
static uint64_t s_getattr_types[GETATTR_TYPE_MAX] = { 0 };

#define RECORD_GETATTR_STATIC(type) s_getattr_types[type]++;

// liuhan debug only end

static DesciptorClassification
analyze_descriptor(PyTypeObject *type, PyObject *name, PyObject **descr, int store)
{
	if (store) {
		if (type->tp_flags & Py_TPFLAGS_ENABLE_ADAPTIVE_OPCODE)
		{
			RECORD_GETATTR_STATIC(GETATTR_TYPE_ADAPTIVE);
			if (type->tp_adaptive_setattro != NULL)
			{
				return ADAPTIVE_OVERRIDDEN_GETSET;
			}
		}
		if (type->tp_setattro != PyObject_GenericSetAttr) {
			*descr = NULL;
			return GETSET_OVERRIDDEN;
		}
	}
	else {
		if (type->tp_flags & Py_TPFLAGS_ENABLE_ADAPTIVE_OPCODE)
		{
			RECORD_GETATTR_STATIC(GETATTR_TYPE_ADAPTIVE);
			if (type->tp_adaptive_getattro != NULL)
			{
				return ADAPTIVE_OVERRIDDEN_GETSET;
			}
			return GETSET_OVERRIDDEN;
		}
		if (type->tp_getattro == (getattrofunc)type_getattro)
		{
			return CLASS_OVERRIDDEN_GETSET;
		}
		if (type->tp_getattro != PyObject_GenericGetAttr)
		{
			RECORD_GETATTR_STATIC(GETATTR_TYPE_OTHER);
			*descr = NULL;
			return GETSET_OVERRIDDEN;
		}
	}
	PyObject *descriptor = _PyType_Lookup(type, name);
	*descr = descriptor;
	if (descriptor == NULL) {
		return ABSENT;
	}
	PyTypeObject *desc_cls = Py_TYPE(descriptor);
	//     if (!(desc_cls->tp_flags & Py_TPFLAGS_IMMUTABLETYPE)) {
	//         return MUTABLE;
	//     }
	if (desc_cls->tp_descr_set) {
		if (desc_cls == &PyMemberDescr_Type) {
			PyMemberDescrObject *member = (PyMemberDescrObject *)descriptor;
			struct PyMemberDef *dmem = member->d_member;
			if (dmem->type == T_OBJECT_EX) {
				return OBJECT_SLOT;
			}
			return OTHER_SLOT;
		}
		if (desc_cls == &PyProperty_Type) {
			return PROPERTY;
		}
		if (strcmp(PyString_AS_STRING(name), "__class__") == 0) {
			if (descriptor == _PyType_Lookup(&PyBaseObject_Type, name)) {
				return DUNDER_CLASS;
			}
		}
		return OVERRIDING;
	}
	if (desc_cls->tp_descr_get) {
		//         if (desc_cls->tp_flags & Py_TPFLAGS_METHOD_DESCRIPTOR) {
		//             return METHOD;
		//         }
		// 		if (Py_IS_TYPE(descriptor, &PyFunction_Type))
		// 		{
		// 			return METHOD;
		// 		}
		if (Py_IS_TYPE(descriptor, &PyStaticMethod_Type)) {
			return PYTHON_STATICMETHOD;
		}
		else if (Py_IS_TYPE(descriptor, &PyClassMethod_Type)) {
			return PYTHON_CLASSMETHOD;
		}
		return NON_OVERRIDING;
	}
	return NON_DESCRIPTOR;
}

static inline int _specialize_dict_access(
	PyObject *dict, PyTypeObject *type, _Py_CODEUNIT *instr, PyObject *name,
	_PyAdaptiveEntry *cache, int replacing_op)
{
	if (dict == NULL || !PyDict_CheckExact(dict)) {
		// fail
		return 0;
	}
	PyDictObject* mp = (PyDictObject*)dict;
	PyDictEntry *ep = _PyDict_GetEntryPoint(dict, name);
	if (ep == NULL || ep->me_value == NULL)
	{
		return 0;
	}
	unsigned int index;
	index = ep - &mp->ma_table[0];
	if (index > ADAPTIVE_ATTR_MAX_INDEX)
	{
		return 0;
	}
	cache->index = index;
	// _Py_MAKECODEUNIT(instr, replacing_op, _Py_OPARG(*instr));
	// no need to change oparg
	_Py_MAKECODEUNIT_NOARG(instr, replacing_op);

	return 1;
}

static inline int
specialize_class_member_access(
	PyTypeObject *type, _Py_CODEUNIT *instr, PyTypeObject *metatype,
	PyObject *name, _PyAdaptiveEntry *cache0, _PyAttrCache *cache1,
	int base_op, int replacing_op)
{
	PyObject *meta_attribute, *attribute;
	descrgetfunc meta_get = NULL;
	if (!PyString_Check(name)) {
		return 0;
	}
	if (type->tp_dict == NULL) {
		if (PyType_Ready(type) < 0)
			return 0;
	}
	meta_attribute = _PyType_Lookup(metatype, name);
	if (meta_attribute != NULL) {
		meta_get = Py_TYPE(meta_attribute)->tp_descr_get;
		if (meta_get != NULL && PyDescr_IsData(meta_attribute)) {
			return 0;
		}
	}

	unsigned int h, version;
	attribute = _PyType_LookupDetail(type, name, &h, &version);
	if (attribute != NULL) {
		/* Implement descriptor functionality, if any */
		descrgetfunc local_get = Py_TYPE(attribute)->tp_descr_get;
		if (local_get != NULL) {
			// liuhan todo
			return 0;
		}
		cache1->method_cache_index = h;
		cache1->method_type_version = version;
		_Py_MAKECODEUNIT_NOARG(instr, replacing_op);
		return 1;
	}

	return 0;
}
static inline int
specialize_dict_access(
	PyObject *owner, _Py_CODEUNIT *instr, PyTypeObject *type,
	DesciptorClassification kind, PyObject *name,
	_PyAdaptiveEntry *cache0 , _PyAttrCache *cache1,
	int base_op, int replacing_op)
{
	assert(kind == NON_OVERRIDING || kind == NON_DESCRIPTOR || kind == ABSENT ||
		kind == BUILTIN_CLASSMETHOD || kind == PYTHON_CLASSMETHOD);
	
	// No descriptor, or non overriding.
	if (type->tp_dictoffset <= 0)
	{
		SPECIALIZATION_FAIL(base_op, SPEC_FAIL_OUT_OF_RANGE);
		return 0;
	}
	PyObject **dictptr = (PyObject **)((char *)owner + type->tp_dictoffset);
	PyObject *dict = *dictptr;
	return _specialize_dict_access(dict, type, instr, name, cache0, replacing_op);
}

static inline int
specialize_module_load_attr(
    PyObject *owner, _Py_CODEUNIT *instr, PyTypeObject *type, PyObject *name,
    _PyAdaptiveEntry *cache, int base_op,
    int replacing_op)
{
	PyObject *dict = ((PyModuleObject*)owner)->md_dict;
	return _specialize_dict_access(dict, type, instr, name, cache, replacing_op);
}
// liuhan debug only begin
enum LOAD_ATTR_TYPE
{
	RECORD_LOADATTR_TYPE_FAIL = 0,
	RECORD_LOADATTR_TYPE_MODULE,
	RECORD_LOADATTR_TYPE_MODULE_FAIL,
	RECORD_LOADATTR_TYPE_OVERRIDING,
	RECORD_LOADATTR_TYPE_METHOD,
	RECORD_LOADATTR_TYPE_PROPERTY,
	RECORD_LOADATTR_TYPE_OBJECT_SLOT,
	RECORD_LOADATTR_TYPE_DUNDER_CLASS,
	RECORD_LOADATTR_TYPE_OTHER_SLOT,
	RECORD_LOADATTR_TYPE_MUTABLE,
	RECORD_LOADATTR_TYPE_GETSET_OVERRIDDEN,
	RECORD_LOADATTR_TYPE_ADAPTIVE_GETSET,
	RECORD_LOADATTR_TYPE_ADAPTIVE_GETSET_FAIL,
	RECORD_LOADATTR_TYPE_CLASS_VALUE,
	RECORD_LOADATTR_TYPE_CLASS_VALUE_FAIL,
	RECORD_LOADATTR_TYPE_OTHERS,
	RECORD_LOADATTR_TYPE_GENERAL_FAIL,
	RECORD_STOREATTR_TYPE_OVERRIDING,

	RECORD_STOREATTR_TYPE_MODULE,
	RECORD_STOREATTR_TYPE_MODULE_FAIL,
	RECORD_STOREATTR_TYPE_FAIL,
	RECORD_STOREATTR_TYPE_METHOD,
	RECORD_STOREATTR_TYPE_PROPERTY,
	RECORD_STOREATTR_TYPE_OBJECT_SLOT,
	RECORD_STOREATTR_TYPE_DUNDER_CLASS,
	RECORD_STOREATTR_TYPE_OTHER_SLOT,
	RECORD_STOREATTR_TYPE_MUTABLE,
	RECORD_STOREATTR_TYPE_GETSET_OVERRIDDEN,
	RECORD_STOREATTR_TYPE_GENERAL_FAIL,
	RECORD_STOREATTR_TYPE_OTHERS,
	RECORD_STOREATTR_TYPE_ADAPTIVE_GETSET,
	RECORD_STOREATTR_TYPE_ADAPTIVE_GETSET_FAIL,

	RECORD_LOADATTR_TYPE_MAX,
};
static uint64_t s_loadattr_statics[RECORD_LOADATTR_TYPE_MAX] = { 0 };


#define RECORD_LOADATTR_STATIC(type) s_loadattr_statics[type]++;
// liuhan debug only end

int
_Py_Specialize_LoadGlobal(
	PyObject *globals, PyObject *builtins, _Py_CODEUNIT *instr, PyObject *name, SpecializedCacheEntry *cache_)
{
	_PyAdaptiveEntry *cache = &cache_->adaptive;

	PyTypeObject *tp = Py_TYPE(globals);
	int r = _specialize_dict_access(globals, tp, instr, name, cache, LOAD_GLOBAL_MODULE);

	if (r < 0)
	{
		return -1;
	}
// 	if (r == 0)
// 	{
// 		// liuhan: 这里暂时不能对BUILTIN进行加速，因为无法处理因为globals改变导致的cache miss问题
// 		// r = _specialize_dict_access(builtins, tp, instr, name, cache0, cache1, LOAD_GLOBAL_BUILTIN);
// 	}

	cache->counter = initial_counter_value();
	return 0;
}
int
_Py_Specialize_StoreAttr(PyObject *owner, _Py_CODEUNIT *instr, PyObject *name, SpecializedCacheEntry *cache_)
{
	_PyAdaptiveEntry *cache0 = &cache_->adaptive;
	_PyAttrCache *cache1 = &cache_[-1].attr;

	int err = 0;
	PyTypeObject *type = Py_TYPE(owner);

	if (PyModule_CheckExact(owner)) {
		// RECORD_LOADATTR_STATIC(RECORD_STORE_ATTR_TYPE_MODULE_FAIL);
		err = specialize_module_load_attr(owner, instr, type, name, cache0, STORE_ATTR, STORE_ATTR_INSTANCE_VALUE);
		PyTypeObject *type = Py_TYPE(owner);

		if (err == 1)
		{
			RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_MODULE);
		}
		else
		{
			RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_MODULE_FAIL);
		}

		goto process_result;

		goto fail_forever;

	}
	if (type->tp_dict == NULL) {
		if (PyType_Ready(type) < 0) {
			RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_FAIL);
			return -1;
		}
	}

	PyObject *descr;
	DesciptorClassification kind = analyze_descriptor(type, name, &descr, 1);
	switch (kind) {
	case OVERRIDING:
		SPECIALIZATION_FAIL(STORE_ATTR, SPEC_FAIL_OVERRIDING_DESCRIPTOR);
		RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_OVERRIDING);
		goto fail_forever;
	case METHOD:
		SPECIALIZATION_FAIL(STORE_ATTR, SPEC_FAIL_METHOD);
		RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_METHOD);
		goto fail_forever;
	case PROPERTY:
		SPECIALIZATION_FAIL(STORE_ATTR, SPEC_FAIL_PROPERTY);
		RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_PROPERTY);
		goto fail_forever;
	case OBJECT_SLOT:
	{
		SPECIALIZATION_FAIL(STORE_ATTR, SPEC_FAIL_NON_OBJECT_SLOT);
		RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_OBJECT_SLOT);
		goto fail_forever;
	}
	case DUNDER_CLASS:
	{
		SPECIALIZATION_FAIL(STORE_ATTR, SPEC_FAIL_NON_OBJECT_SLOT);
		RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_DUNDER_CLASS);
		goto fail_forever;
	}
	case OTHER_SLOT:
		SPECIALIZATION_FAIL(STORE_ATTR, SPEC_FAIL_NON_OBJECT_SLOT);
		RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_OTHER_SLOT);
		goto fail_forever;
	case MUTABLE:
		SPECIALIZATION_FAIL(STORE_ATTR, SPEC_FAIL_MUTABLE_CLASS);
		RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_MUTABLE);
		goto fail_forever;
	case GETSET_OVERRIDDEN:
		SPECIALIZATION_FAIL(STORE_ATTR, SPEC_FAIL_OVERRIDDEN);
		RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_GETSET_OVERRIDDEN);
		goto fail_forever;
	case GENERAL_FAIL:
		RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_GENERAL_FAIL);
		goto fail_forever;
	case PYTHON_STATICMETHOD:
	case BUILTIN_CLASSMETHOD:
	case PYTHON_CLASSMETHOD:
	case NON_OVERRIDING:
	case NON_DESCRIPTOR:
		SPECIALIZATION_FAIL(STORE_ATTR, SPEC_FAIL_OVERRIDDEN);
		RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_OTHERS);
		goto fail_forever;
	case ADAPTIVE_OVERRIDDEN_GETSET:
		if (g_enable_adaptive_overridden_storeattr)
			err = type->tp_adaptive_specializedictaccess(owner, name, cache_, 1);
		else
			goto fail_forever;

		if (err == 1) _Py_MAKECODEUNIT_NOARG(instr, STORE_ATTR_GETSET_OVERRIDDEN);
		if (err == 1)
		{
			RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_ADAPTIVE_GETSET);
		}
		else
		{
			RECORD_LOADATTR_STATIC(RECORD_STOREATTR_TYPE_ADAPTIVE_GETSET_FAIL);
		}

		goto process_result;
	case CLASS_OVERRIDDEN_GETSET:
		// liuhan todo: rare cases, implementing later
		goto fail_forever;
	case ABSENT:
		break;
	}
	err = specialize_dict_access(
		owner, instr, type, kind, name, cache0, cache1,
		STORE_ATTR, STORE_ATTR_INSTANCE_VALUE
	);
process_result:
	if (err < 0) return -1; // NOTE this return maybe not correct?
	else if (err == 1) goto success;
	else if (err == 2) goto fail_forever;
	// fail:

	STAT_INC(STORE_ATTR, specialization_failure);
	assert(!PyErr_Occurred());

	cache_backoff(cache0);
	return 0;
fail_forever:
	assert(!PyErr_Occurred());
	_Py_MAKECODEUNIT(instr, STORE_ATTR, cache0->original_oparg);
	return 0;

success:
	STAT_INC(STORE_ATTR, specialization_success);
	assert(!PyErr_Occurred());
	cache0->counter = initial_counter_value();
	return 0;
}

int
_Py_Specialize_LoadAttr(PyObject *owner, _Py_CODEUNIT *instr, PyObject *name, SpecializedCacheEntry *cache_)
{
	_PyAdaptiveEntry *cache0 = &cache_->adaptive;
	_PyAttrCache *cache1 = &cache_[-1].attr;

	int err = 0;
	PyTypeObject *type = Py_TYPE(owner);

	if (PyModule_CheckExact(owner)) {
		err = specialize_module_load_attr(owner, instr, type, name, cache0, LOAD_ATTR, LOAD_ATTR_INSTANCE_VALUE);
		PyTypeObject *type = Py_TYPE(owner);

		if (err == 1)
		{
			RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_MODULE);
		}
		else
		{
			RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_MODULE_FAIL);
		}

		goto process_result;

	}
	if (type->tp_dict == NULL) {
		if (PyType_Ready(type) < 0) {
			RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_FAIL);
			return -1;
		}
	}
	PyObject *descr;
	DesciptorClassification kind = analyze_descriptor(type, name, &descr, 0);
	switch (kind) {
	case OVERRIDING:
		SPECIALIZATION_FAIL(LOAD_ATTR, SPEC_FAIL_OVERRIDING_DESCRIPTOR);
		RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_OVERRIDING);
		goto fail_forever;
	case METHOD:
		SPECIALIZATION_FAIL(LOAD_ATTR, SPEC_FAIL_METHOD);
		RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_METHOD);
		goto fail_forever;
	case PROPERTY:
		SPECIALIZATION_FAIL(LOAD_ATTR, SPEC_FAIL_PROPERTY);
		RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_PROPERTY);
		goto fail_forever;
	case OBJECT_SLOT:
	{
		SPECIALIZATION_FAIL(LOAD_ATTR, SPEC_FAIL_NON_OBJECT_SLOT);
		RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_OBJECT_SLOT);
		goto fail_forever;
	}
	case DUNDER_CLASS:
	{
		SPECIALIZATION_FAIL(LOAD_ATTR, SPEC_FAIL_NON_OBJECT_SLOT);
		RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_DUNDER_CLASS);
		goto fail_forever;
	}
	case OTHER_SLOT:
		SPECIALIZATION_FAIL(LOAD_ATTR, SPEC_FAIL_NON_OBJECT_SLOT);
		RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_OTHER_SLOT);
		goto fail_forever;
	case MUTABLE:
		SPECIALIZATION_FAIL(LOAD_ATTR, SPEC_FAIL_MUTABLE_CLASS);
		RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_MUTABLE);
		goto fail_forever;
	case GETSET_OVERRIDDEN:
		SPECIALIZATION_FAIL(LOAD_ATTR, SPEC_FAIL_OVERRIDDEN);
		RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_GETSET_OVERRIDDEN);
		goto fail_forever;
	case GENERAL_FAIL:
		RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_GENERAL_FAIL);
		goto fail_forever;
	case PYTHON_STATICMETHOD:
	case BUILTIN_CLASSMETHOD:
	case PYTHON_CLASSMETHOD:
	case NON_OVERRIDING:
	case NON_DESCRIPTOR:
		SPECIALIZATION_FAIL(LOAD_ATTR, SPEC_FAIL_OVERRIDDEN);
		RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_OTHERS);
		goto fail_forever;
	case ADAPTIVE_OVERRIDDEN_GETSET:
		if (g_enable_adaptive_overridden_loadattr)
			err = type->tp_adaptive_specializedictaccess(owner, name, cache_, 0);
		else
			goto fail_forever;

		if (err == 1) _Py_MAKECODEUNIT_NOARG(instr, LOAD_ATTR_GETSET_OVERRIDDEN);

		if (err == 1)
		{
			RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_ADAPTIVE_GETSET);
		}
		else
		{
			RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_ADAPTIVE_GETSET_FAIL);
		}

		goto process_result;
	case CLASS_OVERRIDDEN_GETSET:
		err = specialize_class_member_access((PyTypeObject*)owner, instr, type, name, cache0, cache1, LOAD_ATTR, LOAD_ATTR_CLASS_VALUE);
		
		if (err == 1)
		{
			RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_CLASS_VALUE);
		}
		else
		{
			RECORD_LOADATTR_STATIC(RECORD_LOADATTR_TYPE_CLASS_VALUE_FAIL);
		}
		goto process_result;
	case ABSENT:
		err = specialize_dict_access(
			owner, instr, type, kind, name, cache0, cache1,
			LOAD_ATTR, LOAD_ATTR_INSTANCE_VALUE
		);
		break;
	}
	
process_result:
	if (err < 0) return -1;
	else if (err == 1) goto success;
	else if (err == 2) goto fail_forever;
// fail:

	STAT_INC(LOAD_ATTR, specialization_failure);
	assert(!PyErr_Occurred());

	cache_backoff(cache0);
	return 0;
fail_forever:
	assert(!PyErr_Occurred());
	_Py_MAKECODEUNIT(instr, LOAD_ATTR, cache0->original_oparg);
	return 0;

success:
	STAT_INC(LOAD_ATTR, specialization_success);
	assert(!PyErr_Occurred());
	cache0->counter = initial_counter_value();
	return 0;
}

#if COLLECT_SPECIALIZATION_STATS_DETAILED
static int
load_method_fail_kind(DesciptorClassification kind)
{
	switch (kind) {
	case OVERRIDING:
		return SPEC_FAIL_OVERRIDING_DESCRIPTOR;
	case METHOD:
		return SPEC_FAIL_METHOD;
	case PROPERTY:
		return SPEC_FAIL_PROPERTY;
	case OBJECT_SLOT:
		return SPEC_FAIL_OBJECT_SLOT;
	case OTHER_SLOT:
		return SPEC_FAIL_NON_OBJECT_SLOT;
	case DUNDER_CLASS:
		return SPEC_FAIL_OTHER;
	case MUTABLE:
		return SPEC_FAIL_MUTABLE_CLASS;
	case GETSET_OVERRIDDEN:
		return SPEC_FAIL_OVERRIDDEN;
	case BUILTIN_CLASSMETHOD:
		return SPEC_FAIL_BUILTIN_CLASS_METHOD;
	case PYTHON_CLASSMETHOD:
		return SPEC_FAIL_CLASS_METHOD_OBJ;
	case NON_OVERRIDING:
		return SPEC_FAIL_NON_OVERRIDING_DESCRIPTOR;
	case NON_DESCRIPTOR:
		return SPEC_FAIL_NOT_DESCRIPTOR;
	case ABSENT:
		return SPEC_FAIL_EXPECTED_ERROR;
	}
	Py_UNREACHABLE();
}
#endif

// static int
// specialize_class_load_method(PyObject *owner, _Py_CODEUNIT *instr, PyObject *name,
//                            _PyAttrCache *cache1, _PyObjectCache *cache2)
// {
// 
//     PyObject *descr = NULL;
//     DesciptorClassification kind = 0;
//     kind = analyze_descriptor((PyTypeObject *)owner, name, &descr, 0);
//     switch (kind) {
//         case METHOD:
//         case NON_DESCRIPTOR:
//             cache1->tp_version = ((PyTypeObject *)owner)->tp_version_tag;
//             cache2->obj = descr;
//             *instr = _Py_MAKECODEUNIT(LOAD_METHOD_CLASS, _Py_OPARG(*instr));
//             return 0;
//         default:
//             SPECIALIZATION_FAIL(LOAD_METHOD, load_method_fail_kind(kind));
//             return -1;
//     }
// }

// Please collect stats carefully before and after modifying. A subtle change
// can cause a significant drop in cache hits. A possible test is
// python.exe -m test_typing test_re test_dis test_zlib.
// int
// _Py_Specialize_LoadMethod(PyObject *owner, _Py_CODEUNIT *instr, PyObject *name, SpecializedCacheEntry *cache)
// {
//     _PyAdaptiveEntry *cache0 = &cache->adaptive;
//     _PyAttrCache *cache1 = &cache[-1].attr;
//     _PyObjectCache *cache2 = &cache[-2].obj;
// 
//     PyTypeObject *owner_cls = Py_TYPE(owner);
//     if (PyModule_CheckExact(owner)) {
//         int err = specialize_module_load_attr(owner, instr, name, cache0, cache1,
//             LOAD_METHOD, LOAD_METHOD_MODULE);
//         if (err) {
//             goto fail;
//         }
//         goto success;
//     }
//     if (owner_cls->tp_dict == NULL) {
//         if (PyType_Ready(owner_cls) < 0) {
//             return -1;
//         }
//     }
//     if (PyType_Check(owner)) {
//         int err = specialize_class_load_method(owner, instr, name, cache1, cache2);
//         if (err) {
//             goto fail;
//         }
//         goto success;
//     }
//     // Technically this is fine for bound method calls, but it's uncommon and
//     // slightly slower at runtime to get dict.
//     if (owner_cls->tp_dictoffset < 0) {
//         SPECIALIZATION_FAIL(LOAD_METHOD, SPEC_FAIL_OUT_OF_RANGE);
//         goto fail;
//     }
// 
//     PyObject *descr = NULL;
//     DesciptorClassification kind = 0;
//     kind = analyze_descriptor(owner_cls, name, &descr, 0);
//     assert(descr != NULL || kind == ABSENT || kind == GETSET_OVERRIDDEN);
//     if (kind != METHOD) {
//         SPECIALIZATION_FAIL(LOAD_METHOD, load_method_fail_kind(kind));
//         goto fail;
//     }
//     if (owner_cls->tp_inline_values_offset) {
//         PyObject **owner_dictptr = _PyObject_DictPointer(owner);
//         assert(owner_dictptr);
//         if (*owner_dictptr) {
//             SPECIALIZATION_FAIL(LOAD_METHOD, SPEC_FAIL_IS_ATTR);
//             goto fail;
//         }
//         PyDictKeysObject *keys = ((PyHeapTypeObject *)owner_cls)->ht_cached_keys;
//         Py_ssize_t index = _PyDictKeys_StringLookup(keys, name);
//         if (index != DKIX_EMPTY) {
//             SPECIALIZATION_FAIL(LOAD_METHOD, SPEC_FAIL_IS_ATTR);
//             goto fail;
//         }
//         uint32_t keys_version = _PyDictKeys_GetVersionForCurrentState(keys);
//         if (keys_version == 0) {
//             SPECIALIZATION_FAIL(LOAD_METHOD, SPEC_FAIL_OUT_OF_VERSIONS);
//             goto fail;
//         }
//         cache1->dk_version_or_hint = keys_version;
//         *instr = _Py_MAKECODEUNIT(LOAD_METHOD_CACHED, _Py_OPARG(*instr));
//     }
//     else {
//         if (owner_cls->tp_dictoffset == 0) {
//             *instr = _Py_MAKECODEUNIT(LOAD_METHOD_NO_DICT, _Py_OPARG(*instr));
//         }
//         else {
//             SPECIALIZATION_FAIL(LOAD_METHOD, SPEC_FAIL_IS_ATTR);
//             goto fail;
//         }
//     }
//     /* `descr` is borrowed. This is safe for methods (even inherited ones from
//     *  super classes!) as long as tp_version_tag is validated for two main reasons:
//     *
//     *  1. The class will always hold a reference to the method so it will
//     *  usually not be GC-ed. Should it be deleted in Python, e.g.
//     *  `del obj.meth`, tp_version_tag will be invalidated, because of reason 2.
//     *
//     *  2. The pre-existing type method cache (MCACHE) uses the same principles
//     *  of caching a borrowed descriptor. The MCACHE infrastructure does all the
//     *  heavy lifting for us. E.g. it invalidates tp_version_tag on any MRO
//     *  modification, on any type object change along said MRO, etc. (see
//     *  PyType_Modified usages in typeobject.c). The MCACHE has been
//     *  working since Python 2.6 and it's battle-tested.
//     */
//     cache1->tp_version = owner_cls->tp_version_tag;
//     cache2->obj = descr;
//     // Fall through.
// success:
//     STAT_INC(LOAD_METHOD, specialization_success);
//     assert(!PyErr_Occurred());
//     cache0->counter = initial_counter_value();
//     return 0;
// fail:
//     STAT_INC(LOAD_METHOD, specialization_failure);
//     assert(!PyErr_Occurred());
//     cache_backoff(cache0);
//     return 0;
// 
// }

// int
// _Py_Specialize_LoadGlobal(
//     PyObject *globals, PyObject *builtins,
//     _Py_CODEUNIT *instr, PyObject *name,
//     SpecializedCacheEntry *cache)
// {
//     _PyAdaptiveEntry *cache0 = &cache->adaptive;
//     _PyLoadGlobalCache *cache1 = &cache[-1].load_global;
//     assert(PyUnicode_CheckExact(name));
//     if (!PyDict_CheckExact(globals)) {
//         goto fail;
//     }
//     PyDictKeysObject * globals_keys = ((PyDictObject *)globals)->ma_keys;
//     Py_ssize_t index = _PyDictKeys_StringLookup(globals_keys, name);
//     if (index == DKIX_ERROR) {
//         SPECIALIZATION_FAIL(LOAD_GLOBAL, SPEC_FAIL_NON_STRING_OR_SPLIT);
//         goto fail;
//     }
//     if (index != DKIX_EMPTY) {
//         if (index != (uint16_t)index) {
//             goto fail;
//         }
//         uint32_t keys_version = _PyDictKeys_GetVersionForCurrentState(globals_keys);
//         if (keys_version == 0) {
//             goto fail;
//         }
//         cache1->module_keys_version = keys_version;
//         cache0->index = (uint16_t)index;
//         *instr = _Py_MAKECODEUNIT(LOAD_GLOBAL_MODULE, _Py_OPARG(*instr));
//         goto success;
//     }
//     if (!PyDict_CheckExact(builtins)) {
//         goto fail;
//     }
//     PyDictKeysObject * builtin_keys = ((PyDictObject *)builtins)->ma_keys;
//     index = _PyDictKeys_StringLookup(builtin_keys, name);
//     if (index == DKIX_ERROR) {
//         SPECIALIZATION_FAIL(LOAD_GLOBAL, SPEC_FAIL_NON_STRING_OR_SPLIT);
//         goto fail;
//     }
//     if (index != (uint16_t)index) {
//         goto fail;
//     }
//     uint32_t globals_version = _PyDictKeys_GetVersionForCurrentState(globals_keys);
//     if (globals_version == 0) {
//         SPECIALIZATION_FAIL(LOAD_GLOBAL, SPEC_FAIL_OUT_OF_VERSIONS);
//         goto fail;
//     }
//     uint32_t builtins_version = _PyDictKeys_GetVersionForCurrentState(builtin_keys);
//     if (builtins_version == 0) {
//         SPECIALIZATION_FAIL(LOAD_GLOBAL, SPEC_FAIL_OUT_OF_VERSIONS);
//         goto fail;
//     }
//     cache1->module_keys_version = globals_version;
//     cache1->builtin_keys_version = builtins_version;
//     cache0->index = (uint16_t)index;
//     *instr = _Py_MAKECODEUNIT(LOAD_GLOBAL_BUILTIN, _Py_OPARG(*instr));
//     goto success;
// fail:
//     STAT_INC(LOAD_GLOBAL, specialization_failure);
//     assert(!PyErr_Occurred());
//     cache_backoff(cache0);
//     return 0;
// success:
//     STAT_INC(LOAD_GLOBAL, specialization_success);
//     assert(!PyErr_Occurred());
//     cache0->counter = initial_counter_value();
//     return 0;
// }

#if COLLECT_SPECIALIZATION_STATS_DETAILED
static int
binary_subscr_faiL_kind(PyTypeObject *container_type, PyObject *sub)
{
	if (container_type == &PyUnicode_Type) {
		if (PyLong_CheckExact(sub)) {
			return SPEC_FAIL_STRING_INT;
		}
		if (PySlice_Check(sub)) {
			return SPEC_FAIL_STRING_SLICE;
		}
		return SPEC_FAIL_OTHER;
	}
	else if (strcmp(container_type->tp_name, "array.array") == 0) {
		if (PyLong_CheckExact(sub)) {
			return SPEC_FAIL_ARRAY_INT;
		}
		if (PySlice_Check(sub)) {
			return SPEC_FAIL_ARRAY_SLICE;
		}
		return SPEC_FAIL_OTHER;
	}
	else if (container_type->tp_as_buffer) {
		if (PyLong_CheckExact(sub)) {
			return SPEC_FAIL_BUFFER_INT;
		}
		if (PySlice_Check(sub)) {
			return SPEC_FAIL_BUFFER_SLICE;
		}
		return SPEC_FAIL_OTHER;
	}
	else if (container_type->tp_as_sequence) {
		if (PyLong_CheckExact(sub) && container_type->tp_as_sequence->sq_item) {
			return SPEC_FAIL_SEQUENCE_INT;
		}
	}
	return SPEC_FAIL_OTHER;
}
#endif

int
_Py_Specialize_StoreSubscr(PyObject *container, PyObject *sub, _Py_CODEUNIT *instr)
{
	PyTypeObject *container_type = Py_TYPE(container);
	if (container_type == &PyList_Type) {
		if (PyInt_CheckExact(sub)) {
			_Py_MAKECODEUNIT_NOARG(instr, STORE_SUBSCR_LIST_INT);
			goto success;
		}
		goto fail;
	}
	if (container_type == &PyDict_Type) {
		_Py_MAKECODEUNIT_NOARG(instr, STORE_SUBSCR_DICT);
		goto success;
	}

fail:
	STAT_INC(BINARY_SUBSCR, specialization_failure);
	assert(!PyErr_Occurred());
	// _Py_MAKECODEUNIT(instr, _Py_OPCODE(instr), ADAPTIVE_CACHE_BACKOFF);
	_Py_MAKECODEUNIT_NOARG(instr, STORE_SUBSCR);
	return 0;
success:
	STAT_INC(BINARY_SUBSCR, specialization_success);
	assert(!PyErr_Occurred());
	return 0;
}

int
_Py_Specialize_BinarySubscr(
	PyObject *container, PyObject *sub, _Py_CODEUNIT *instr)
{
	// _PyAdaptiveEntry *cache0 = &cache->adaptive;
	PyTypeObject *container_type = Py_TYPE(container);
	if (container_type == &PyList_Type) {
		if (PyInt_CheckExact(sub)) {
			_Py_MAKECODEUNIT_NOARG(instr, BINARY_SUBSCR_LIST_INT);
			goto success;
		}
		SPECIALIZATION_FAIL(BINARY_SUBSCR,
			PySlice_Check(sub) ? SPEC_FAIL_LIST_SLICE : SPEC_FAIL_OTHER);
		goto fail;
	}
	if (container_type == &PyTuple_Type) {
		if (PyInt_CheckExact(sub)) {
			_Py_MAKECODEUNIT_NOARG(instr, BINARY_SUBSCR_TUPLE_INT);
			goto success;
		}
		SPECIALIZATION_FAIL(BINARY_SUBSCR,
			PySlice_Check(sub) ? SPEC_FAIL_TUPLE_SLICE : SPEC_FAIL_OTHER);
		goto fail;
	}
	if (container_type == &PyDict_Type) {
		_Py_MAKECODEUNIT_NOARG(instr, BINARY_SUBSCR_DICT);
		goto success;
	}
// 	PyTypeObject *cls = Py_TYPE(container);
// 	PyObject *descriptor = _PyType_LookupId(cls, &PyId___getitem__);
// 	if (descriptor && Py_TYPE(descriptor) == &PyFunction_Type) {
// 		PyFunctionObject *func = (PyFunctionObject *)descriptor;
// 		PyCodeObject *code = (PyCodeObject *)func->func_code;
// 		int kind = function_kind(code);
// 		if (kind != SIMPLE_FUNCTION) {
// 			SPECIALIZATION_FAIL(BINARY_SUBSCR, kind);
// 			goto fail;
// 		}
// 		if (code->co_argcount != 2) {
// 			SPECIALIZATION_FAIL(BINARY_SUBSCR, SPEC_FAIL_WRONG_NUMBER_ARGUMENTS);
// 			goto fail;
// 		}
// 		assert(cls->tp_version_tag != 0);
// 		cache0->version = cls->tp_version_tag;
// 		int version = _PyFunction_GetVersionForCurrentState(func);
// 		if (version == 0 || version != (uint16_t)version) {
// 			SPECIALIZATION_FAIL(BINARY_SUBSCR, SPEC_FAIL_OUT_OF_VERSIONS);
// 			goto fail;
// 		}
// 		cache0->index = version;
// 		cache[-1].obj.obj = descriptor;
// 		*instr = _Py_MAKECODEUNIT(BINARY_SUBSCR_GETITEM, _Py_OPARG(*instr));
// 		goto success;
// 	}
// 	SPECIALIZATION_FAIL(BINARY_SUBSCR,
// 		binary_subscr_fail_kind(container_type, sub));
fail:
	STAT_INC(BINARY_SUBSCR, specialization_failure);
	assert(!PyErr_Occurred());
	// _Py_MAKECODEUNIT(instr, _Py_OPCODE(instr), ADAPTIVE_CACHE_BACKOFF);
	_Py_MAKECODEUNIT_NOARG(instr, BINARY_SUBSCR);
	return 0;
success:
	STAT_INC(BINARY_SUBSCR, specialization_success);
	assert(!PyErr_Occurred());
	return 0;
}

int
_Py_Specialize_BinaryAdd(PyObject *left, PyObject *right, _Py_CODEUNIT *instr)
{
	PyTypeObject *left_type = Py_TYPE(left);
	if (left_type != Py_TYPE(right)) {
		SPECIALIZATION_FAIL(BINARY_ADD, SPEC_FAIL_DIFFERENT_TYPES);
		goto fail;
	}
	//     if (left_type == &PyString_Type) {
	//         int next_opcode = _Py_OPCODE(instr + 1);
	//         if (next_opcode == STORE_FAST) {
	//             *instr = _Py_MAKECODEUNIT(BINARY_ADD_UNICODE_INPLACE_FAST, initial_counter_value());
	//         }
	//         else {
	//             *instr = _Py_MAKECODEUNIT(BINARY_ADD_UNICODE, initial_counter_value());
	//         }
	//         goto success;
	//     }
	if (left_type == &PyLong_Type) {
		_Py_MAKECODEUNIT_NOARG(instr, BINARY_ADD_INT);

		// INVALID_OPCODE
		goto success;
	}
	else if (left_type == &PyFloat_Type) {
		_Py_MAKECODEUNIT_NOARG(instr, BINARY_ADD_FLOAT);
		goto success;

	}
	else {
		SPECIALIZATION_FAIL(BINARY_ADD, SPEC_FAIL_OTHER);
	}
fail:
	STAT_INC(BINARY_ADD, specialization_failure);
	assert(!PyErr_Occurred());
	// _Py_MAKECODEUNIT(instr, _Py_OPCODE(instr), ADAPTIVE_CACHE_BACKOFF);
	_Py_MAKECODEUNIT_NOARG(instr, BINARY_ADD);
	return 0;
success:
	STAT_INC(BINARY_ADD, specialization_success);
	assert(!PyErr_Occurred());
	return 0;
}

// int
// _Py_Specialize_BinaryMultiply(PyObject *left, PyObject *right, _Py_CODEUNIT *instr)
// {
//     if (!Py_IS_TYPE(left, Py_TYPE(right))) {
//         SPECIALIZATION_FAIL(BINARY_MULTIPLY, SPEC_FAIL_DIFFERENT_TYPES);
//         goto fail;
//     }
//     if (PyLong_CheckExact(left)) {
//         *instr = _Py_MAKECODEUNIT(BINARY_MULTIPLY_INT, initial_counter_value());
//         goto success;
//     }
//     else if (PyFloat_CheckExact(left)) {
//         *instr = _Py_MAKECODEUNIT(BINARY_MULTIPLY_FLOAT, initial_counter_value());
//         goto success;
//     }
//     else {
//         SPECIALIZATION_FAIL(BINARY_MULTIPLY, SPEC_FAIL_OTHER);
//     }
// fail:
//     STAT_INC(BINARY_MULTIPLY, specialization_failure);
//     assert(!PyErr_Occurred());
//     *instr = _Py_MAKECODEUNIT(_Py_OPCODE(*instr), ADAPTIVE_CACHE_BACKOFF);
//     return 0;
// success:
//     STAT_INC(BINARY_MULTIPLY, specialization_success);
//     assert(!PyErr_Occurred());
//     return 0;
// }

// static int
// specialize_class_call(
//     PyObject *callable, _Py_CODEUNIT *instr,
//     int nargs, SpecializedCacheEntry *cache)
// {
//     SPECIALIZATION_FAIL(CALL_FUNCTION, SPEC_FAIL_CLASS);
//     return -1;
// }
// 
// static int
// specialize_py_call(
//     PyFunctionObject *func, _Py_CODEUNIT *instr,
//     int nargs, SpecializedCacheEntry *cache)
// {
//     _PyCallCache *cache1 = &cache[-1].call;
//     /* Exclude generator or coroutines for now */
//     PyCodeObject *code = (PyCodeObject *)func->func_code;
//     int flags = code->co_flags;
//     if (flags & (CO_GENERATOR | CO_COROUTINE | CO_ASYNC_GENERATOR)) {
//         SPECIALIZATION_FAIL(CALL_FUNCTION, SPEC_FAIL_GENERATOR);
//         return -1;
//     }
//     if ((flags & (CO_VARKEYWORDS | CO_VARARGS)) || code->co_kwonlyargcount) {
//         SPECIALIZATION_FAIL(CALL_FUNCTION, SPEC_FAIL_COMPLEX_PARAMETERS);
//         return -1;
//     }
//     if ((flags & CO_OPTIMIZED) == 0) {
//         SPECIALIZATION_FAIL(CALL_FUNCTION, SPEC_FAIL_CO_NOT_OPTIMIZED);
//         return -1;
//     }
//     if (code->co_nfreevars) {
//         SPECIALIZATION_FAIL(CALL_FUNCTION, SPEC_FAIL_FREE_VARS);
//         return -1;
//     }
//     int argcount = code->co_argcount;
//     int defcount = func->func_defaults == NULL ? 0 : (int)PyTuple_GET_SIZE(func->func_defaults);
//     assert(defcount <= argcount);
//     int min_args = argcount-defcount;
//     if (nargs > argcount || nargs < min_args) {
//         SPECIALIZATION_FAIL(CALL_FUNCTION, SPEC_FAIL_WRONG_NUMBER_ARGUMENTS);
//         return -1;
//     }
//     assert(nargs <= argcount && nargs >= min_args);
//     int defstart = nargs - min_args;
//     int deflen = argcount - nargs;
//     assert(defstart >= 0 && deflen >= 0);
//     assert(deflen == 0 || func->func_defaults != NULL);
//     if (defstart > 0xffff || deflen > 0xffff) {
//         SPECIALIZATION_FAIL(CALL_FUNCTION, SPEC_FAIL_OUT_OF_RANGE);
//         return -1;
//     }
//     int version = _PyFunction_GetVersionForCurrentState(func);
//     if (version == 0) {
//         SPECIALIZATION_FAIL(CALL_FUNCTION, SPEC_FAIL_OUT_OF_VERSIONS);
//         return -1;
//     }
//     cache1->func_version = version;
//     cache1->defaults_start = defstart;
//     cache1->defaults_len = deflen;
//     *instr = _Py_MAKECODEUNIT(CALL_FUNCTION_PY_SIMPLE, _Py_OPARG(*instr));
//     return 0;
// }

#if COLLECT_SPECIALIZATION_STATS_DETAILED
static int
builtin_call_fail_kind(int ml_flags)
{
	switch (ml_flags & (METH_VARARGS | METH_FASTCALL | METH_NOARGS | METH_O |
		METH_KEYWORDS | METH_METHOD)) {
	case METH_VARARGS:
		return SPEC_FAIL_PYCFUNCTION;
	case METH_VARARGS | METH_KEYWORDS:
		return SPEC_FAIL_PYCFUNCTION_WITH_KEYWORDS;
	case METH_FASTCALL | METH_KEYWORDS:
		return SPEC_FAIL_PYCFUNCTION_FAST_WITH_KEYWORDS;
	case METH_NOARGS:
		return SPEC_FAIL_PYCFUNCTION_NOARGS;
		/* This case should never happen with PyCFunctionObject -- only
			PyMethodObject. See zlib.compressobj()'s methods for an example.
		*/
	case METH_METHOD | METH_FASTCALL | METH_KEYWORDS:
	default:
		return SPEC_FAIL_BAD_CALL_FLAGS;
	}
}
#endif

// static int
// specialize_c_call(PyObject *callable, _Py_CODEUNIT *instr, int nargs,
//     SpecializedCacheEntry *cache, PyObject *builtins)
// {
//     _PyObjectCache *cache1 = &cache[-1].obj;
//     if (PyCFunction_GET_FUNCTION(callable) == NULL) {
//         return 1;
//     }
//     switch (PyCFunction_GET_FLAGS(callable) &
//         (METH_VARARGS | METH_FASTCALL | METH_NOARGS | METH_O |
//         METH_KEYWORDS | METH_METHOD)) {
//         case METH_O: {
//             if (nargs != 1) {
//                 SPECIALIZATION_FAIL(CALL_FUNCTION, SPEC_FAIL_OUT_OF_RANGE);
//                 return 1;
//             }
//             /* len(o) */
//             PyObject *builtin_len = PyDict_GetItemString(builtins, "len");
//             if (callable == builtin_len) {
//                 cache1->obj = builtin_len;  // borrowed
//                 *instr = _Py_MAKECODEUNIT(CALL_FUNCTION_LEN,
//                     _Py_OPARG(*instr));
//                 return 0;
//             }
//             *instr = _Py_MAKECODEUNIT(CALL_FUNCTION_BUILTIN_O,
//                 _Py_OPARG(*instr));
//             return 0;
//         }
//         case METH_FASTCALL: {
//             if (nargs == 2) {
//                 /* isinstance(o1, o2) */
//                 PyObject *builtin_isinstance = PyDict_GetItemString(
//                     builtins, "isinstance");
//                 if (callable == builtin_isinstance) {
//                     cache1->obj = builtin_isinstance;  // borrowed
//                     *instr = _Py_MAKECODEUNIT(CALL_FUNCTION_ISINSTANCE,
//                         _Py_OPARG(*instr));
//                     return 0;
//                 }
//             }
//             *instr = _Py_MAKECODEUNIT(CALL_FUNCTION_BUILTIN_FAST,
//                 _Py_OPARG(*instr));
//             return 0;
//         }
//         default:
//             SPECIALIZATION_FAIL(CALL_FUNCTION,
//                 builtin_call_fail_kind(PyCFunction_GET_FLAGS(callable)));
//             return 1;
//     }
// }

#if COLLECT_SPECIALIZATION_STATS_DETAILED
static int
call_fail_kind(PyObject *callable)
{
	if (PyInstanceMethod_Check(callable)) {
		return SPEC_FAIL_METHOD;
	}
	else if (PyMethod_Check(callable)) {
		return SPEC_FAIL_METHOD;
	}
	// builtin method
	else if (PyCMethod_Check(callable)) {
		return SPEC_FAIL_METHOD;
	}
	else if (PyType_Check(callable)) {
		return  SPEC_FAIL_CLASS;
	}
	return SPEC_FAIL_OTHER;
}
#endif

/* TODO:
	- Specialize calling classes.
*/
// int
// _Py_Specialize_CallFunction(
//     PyObject *callable, _Py_CODEUNIT *instr,
//     int nargs, SpecializedCacheEntry *cache,
//     PyObject *builtins)
// {
//     int fail;
//     if (PyCFunction_CheckExact(callable)) {
//         fail = specialize_c_call(callable, instr, nargs, cache, builtins);
//     }
//     else if (PyFunction_Check(callable)) {
//         fail = specialize_py_call((PyFunctionObject *)callable, instr, nargs, cache);
//     }
//     else if (PyType_Check(callable)) {
//         fail = specialize_class_call(callable, instr, nargs, cache);
//     }
//     else {
//         SPECIALIZATION_FAIL(CALL_FUNCTION, call_fail_kind(callable));
//         fail = -1;
//     }
//     _PyAdaptiveEntry *cache0 = &cache->adaptive;
//     if (fail) {
//         STAT_INC(CALL_FUNCTION, specialization_failure);
//         assert(!PyErr_Occurred());
//         cache_backoff(cache0);
//     }
//     else {
//         STAT_INC(CALL_FUNCTION, specialization_success);
//         assert(!PyErr_Occurred());
//         cache0->counter = initial_counter_value();
//     }
//     return 0;
// }
