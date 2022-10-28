cmake_minimum_required(VERSION 3.6.0)

project(python27)

if ("${CMAKE_MODULE_PATH}" STREQUAL "")
	set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../CMake")
	include(DetermineStdAndLib)
	include(DetermineMSVCRuntime)
	include(FindDependencies)
	set(dependencies openssl)
	if (MSVC)
		list(APPEND dependencies zlib)
	endif()
	FindDependencies(dependencies "any" "any" link_libs)
	set(IS_IN_ENGINE_BUILD FALSE)
else()
	neox_check_threads()
	set(_THIRD_DEPENDS OPENSSL)
	if (MSVC)
		list(APPEND _THIRD_DEPENDS ZLIB)
	endif()
	neox_config_3rd_include_macro(_THIRD_DEPENDS)
	set(IS_IN_ENGINE_BUILD TRUE)
endif()

if (MSVC)
	remove_definitions("/w34189")
	add_definitions("/D_CRT_SECURE_NO_WARNINGS /wd4819 /wd4018 /wd4267 /wd4311 /wd4101 /wd4189 /wd4146 /wd4554 /wd4359 /wd4996")
	set(NEOX_LIBRARY_TYPE "SHARED" CACHE STRING "NEOX_LIBRARY_TYPE")
endif()

execute_process(COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/Python/makeopcodetargets.py WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} RESULT_VARIABLE ret)

if(NOT ret EQUAL "0")
    message( FATAL_ERROR "execution of makeopcodetargets.py failed!")
endif()

set(MODULES_SOURCE_FILES
			./Modules/_bisectmodule.c
			./Modules/_codecsmodule.c
			./Modules/_collectionsmodule.c
			./Modules/_csv.c
			./Modules/_functoolsmodule.c
			./Modules/_heapqmodule.c
			./Modules/_hotshot.c
			./Modules/_json.c
			./Modules/_localemodule.c
			./Modules/_lsprof.c
			./Modules/_math.c
			./Modules/_randommodule.c
			./Modules/_sre.c
			./Modules/_ssl.c
			./Modules/_struct.c
			./Modules/_weakref.c
			./Modules/arraymodule.c
			./Modules/audioop.c
			./Modules/binascii.c
			./Modules/cmathmodule.c
			./Modules/cPickle.c
			./Modules/cStringIO.c
			./Modules/datetimemodule.c
			./Modules/errnomodule.c
			./Modules/future_builtins.c
			./Modules/getbuildinfo.c
			./Modules/gcmodule.c
			./Modules/imageop.c
			./Modules/itertoolsmodule.c
			./Modules/mathmodule.c
			./Modules/md5.c
			./Modules/md5module.c
			./Modules/mmapmodule.c
			./Modules/operator.c
			./Modules/parsermodule.c
			./Modules/posixmodule.c
			./Modules/rotatingtree.c
			./Modules/rotormodule.c
			./Modules/selectmodule.c
			./Modules/sha256module.c
			./Modules/sha512module.c
			./Modules/shamodule.c
			./Modules/signalmodule.c
			./Modules/socketmodule.c
			./Modules/stropmodule.c
			./Modules/symtablemodule.c
			./Modules/threadmodule.c
			./Modules/timemodule.c
			./Modules/unicodedata.c
			./Modules/xxsubtype.c
			./Modules/zipimport.c
			./Modules/zlibmodule.c)

set(OBJECTS_SOURCE_FILES
			./Objects/abstract.c
			./Objects/boolobject.c
			./Objects/bufferobject.c
			./Objects/bytearrayobject.c
			./Objects/bytes_methods.c
			./Objects/capsule.c
			./Objects/cellobject.c
			./Objects/classobject.c
			./Objects/cobject.c
			./Objects/codeobject.c
			./Objects/complexobject.c
			./Objects/descrobject.c
			./Objects/dictobject.c
			./Objects/enumobject.c
			./Objects/exceptions.c
			./Objects/fileobject.c
			./Objects/floatobject.c
			./Objects/frameobject.c
			./Objects/funcobject.c
			./Objects/genobject.c
			./Objects/intobject.c
			./Objects/iterobject.c
			./Objects/listobject.c
			./Objects/longobject.c
			./Objects/memoryobject.c
			./Objects/methodobject.c
			./Objects/moduleobject.c
			./Objects/object.c
			./Objects/obmalloc.c
			./Objects/rangeobject.c
			./Objects/setobject.c
			./Objects/sliceobject.c
			./Objects/stringobject.c
			./Objects/structseq.c
			./Objects/tupleobject.c
			./Objects/typeobject.c
			./Objects/unicodectype.c
			./Objects/unicodeobject.c
			./Objects/weakrefobject.c)

set(PARSER_SOURCE_FILES
			./Parser/acceler.c
			./Parser/bitset.c
			./Parser/firstsets.c
			./Parser/grammar.c
			./Parser/grammar1.c
			./Parser/listnode.c
			./Parser/metagrammar.c
			./Parser/myreadline.c
			./Parser/node.c
			./Parser/parser.c
			./Parser/parsetok.c
			./Parser/tokenizer.c)

set(PYTHON_SOURCE_FILES
			./Python/_warnings.c
			./Python/asdl.c
			./Python/ast.c
			./Python/bltinmodule.c
			./Python/ceval.c
			./Python/codecs.c
			./Python/compile.c
			./Python/dtoa.c
			./Python/errors.c
			./Python/formatter_string.c
			./Python/formatter_unicode.c
			./Python/frozen.c
			./Python/future.c
			./Python/getargs.c
			./Python/getcompiler.c
			./Python/getcopyright.c
			./Python/getopt.c
			./Python/getplatform.c
			./Python/getversion.c
			./Python/graminit.c
			./Python/import.c
			./Python/importdl.c
			./Python/marshal.c
			./Python/modsupport.c
			./Python/mysnprintf.c
			./Python/mystrtoul.c
			./Python/peephole.c
			./Python/pyarena.c
			./Python/pyctype.c
			./Python/pyfpe.c
			./Python/pymath.c
			./Python/pystate.c
			./Python/pystrcmp.c
			./Python/pystrtod.c
			./Python/Python-ast.c
			./Python/pythonrun.c
			./Python/random.c
			./Python/structmember.c
			./Python/symtable.c
			./Python/sysmodule.c
			./Python/thread.c
			./Python/traceback.c
			./Python/specialize.c
			)

set(_IO_SOURCE_FILES
			./Modules/_io/_iomodule.c
			./Modules/_io/bufferedio.c
			./Modules/_io/bytesio.c
			./Modules/_io/fileio.c
			./Modules/_io/iobase.c
			./Modules/_io/stringio.c
			./Modules/_io/textio.c)
		
set(CJKCODECS_SOURCE_FILES
			./Modules/cjkcodecs/_codecs_cn.c
			./Modules/cjkcodecs/_codecs_hk.c
			./Modules/cjkcodecs/_codecs_iso2022.c
			./Modules/cjkcodecs/_codecs_jp.c
			./Modules/cjkcodecs/_codecs_kr.c
			./Modules/cjkcodecs/_codecs_tw.c
			./Modules/cjkcodecs/multibytecodec.c)

set(PYTHON27_HEADER_FILES
			./Include/abstract.h
			./Include/arrayobject.h
			./Include/asdl.h
			./Include/ast.h
			./Include/bitset.h
			./Include/boolobject.h
			./Include/bufferobject.h
			./Include/cellobject.h
			./Include/ceval.h
			./Include/classobject.h
			./Include/cobject.h
			./Include/code.h
			./Include/codecs.h
			./Include/compile.h
			./Include/complexobject.h
			./Include/cStringIO.h
			./Include/datetime.h
			./Include/descrobject.h
			./Include/dictobject.h
			./Include/enumobject.h
			./Include/errcode.h
			./Include/eval.h
			./Include/fileobject.h
			./Include/floatobject.h
			./Include/frameobject.h
			./Include/funcobject.h
			./Include/genobject.h
			./Include/graminit.h
			./Include/grammar.h
			./Include/import.h
			./Include/intobject.h
			./Include/intrcheck.h
			./Include/iterobject.h
			./Include/listobject.h
			./Include/longintrepr.h
			./Include/longobject.h
			./Include/marshal.h
			./Include/metagrammar.h
			./Include/methodobject.h
			./Include/modsupport.h
			./Include/moduleobject.h
			./Include/node.h
			./Include/object.h
			./Include/objimpl.h
			./Include/opcode.h
			./Include/osdefs.h
			./Include/parsetok.h
			./Include/patchlevel.h
			./Include/pgen.h
			./Include/pgenheaders.h
			./Include/py_curses.h
			./Include/pyarena.h
			./Include/pyconfig.h
			./Include/pydebug.h
			./Include/pyerrors.h
			./Include/pyexpat.h
			./Include/pyfpe.h
			./Include/pygetopt.h
			./Include/pymactoolbox.h
			./Include/pymem.h
			./Include/pyport.h
			./Include/pystate.h
			./Include/pystrtod.h
			./Include/Python-ast.h
			./Include/Python.h
			./Include/pythonrun.h
			./Include/pythread.h
			./Include/rangeobject.h
			./Include/setobject.h
			./Include/sliceobject.h
			./Include/stringobject.h
			./Include/structmember.h
			./Include/structseq.h
			./Include/symtable.h
			./Include/sysmodule.h
			./Include/timefuncs.h
			./Include/token.h
			./Include/traceback.h
			./Include/tupleobject.h
			./Include/ucnhash.h
			./Include/unicodeobject.h
			./Include/weakrefobject.h)
				
set(TRACEMALLOC_SOURCE_FILES
			./Modules/_tracemalloc.c
			./Modules/hashtable.c
			./Include/hashtable.h)

set(EXPAT_SOURCE_FILES
			./Modules/pyexpat.c
			./Modules/expat/loadlibrary.c
			./Modules/expat/xmlparse.c
			./Modules/expat/xmlrole.c
			./Modules/expat/xmltok.c)
			
if (ANDROID)
	list(APPEND PYTHON27_SOURCE_FILES 
			./Modules/fcntlmodule.c
			./Modules/getpath.c
			./PC/config.c
			./Python/dynload_shlib.c)
elseif (IOS)
	list(APPEND PYTHON27_SOURCE_FILES 
			./Modules/fcntlmodule.c
			./Modules/getpath.c
			./PC/config.c
			./Python/dynload_shlib.c)
elseif (WIN32)
	list(APPEND PYTHON27_SOURCE_FILES
			./PC/_subprocess.c
			./PC/_winreg.c
			./PC/config.c
			./PC/dl_nt.c
			./PC/getpathp.c
			./PC/import_nt.c
			./PC/msvcrtmodule.c
			./Python/dynload_win.c)
elseif (APPLE)
	list(APPEND PYTHON27_SOURCE_FILES
			./Modules/fcntlmodule.c
			./Modules/getpath.c
			./PC/config.c
			./Python/dynload_next.c)
else()
	list(APPEND PYTHON27_SOURCE_FILES 
			./Modules/fcntlmodule.c
			./Modules/getpath.c
			./PC/config.c
			./Python/dynload_shlib.c)
endif()

list(APPEND PYTHON27_SOURCE_FILES
			${MODULES_SOURCE_FILES}
			${TRACEMALLOC_SOURCE_FILES}
			${_IO_SOURCE_FILES}
			${CJKCODECS_SOURCE_FILES}
			${EXPAT_SOURCE_FILES}
			${OBJECTS_SOURCE_FILES}
			${PARSER_SOURCE_FILES}
			${PYTHON_SOURCE_FILES}
	)

source_group("Modules" FILES ${MODULES_SOURCE_FILES})
source_group("_io" FILES ${_IO_SOURCE_FILES})
source_group("cjkcodecs" FILES ${CJKCODECS_SOURCE_FILES})
source_group("expat" FILES ${EXPAT_SOURCE_FILES})
source_group("Objects" FILES ${OBJECTS_SOURCE_FILES})
source_group("Parser" FILES ${PARSER_SOURCE_FILES})
source_group("Python" FILES ${PYTHON_SOURCE_FILES})
source_group("tracemalloc" FILES ${TRACEMALLOC_SOURCE_FILES})

add_definitions(-DPy_BUILD_CORE -DNO_PYC)

if (${NEOX_LIBRARY_TYPE} STREQUAL "STATIC")
	add_definitions(-DPy_NO_ENABLE_SHARED)
endif()

add_definitions(-DXML_STATIC)

if (ANDROID)
	message(STATUS "android")
	add_definitions(-DHAVE_EXPAT_CONFIG_H)
	add_definitions(-D_FILE_OFFSET_BITS=64)
	add_definitions("-Wno-compare-distinct-pointer-types")
elseif (IOS)
	add_definitions(-DHAVE_EXPAT_CONFIG_H)
	add_definitions("-pipe -fno-strict-aliasing -fvisibility=hidden -Wno-invalid-source-encoding 
		-Wno-deprecated-declarations -Wno-shorten-64-to-32 -Wno-tautological-constant-out-of-range-compare")
elseif (WIN32)
	if (NEOX_ENABLE_PY_COUNT_ALLOCS)
		add_definitions(-DCOUNT_ALLOCS)
	endif()
	if (NEOX_ENABLE_PY_GC_STATISTICS)
		add_definitions("-DENABLE_GC_STATISTICS")
	endif()
	add_definitions(-DCOMPILED_FROM_DSP)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
		"${CMAKE_CURRENT_SOURCE_DIR}/Include"
		"${CMAKE_CURRENT_SOURCE_DIR}/Include/internal"
		"${CMAKE_CURRENT_SOURCE_DIR}/PC"
		"${CMAKE_CURRENT_SOURCE_DIR}/Python"
		"${CMAKE_CURRENT_SOURCE_DIR}/Modules/expat")

add_library(${PROJECT_NAME} ${NEOX_LIBRARY_TYPE} ${PYTHON27_HEADER_FILES} ${PYTHON27_SOURCE_FILES})

if (ANDROID)
elseif (IOS)
elseif (WIN32)
	target_link_libraries(${PROJECT_NAME} ws2_32.lib crypt32.lib)
elseif (APPLE)
endif()

if (link_libs)
	target_link_libraries(${PROJECT_NAME} ${link_libs})
endif()

if (IS_IN_ENGINE_BUILD)
neox_set_third_property(${PROJECT_NAME} _THIRD_DEPENDS)
neox_set_output_dir(${PROJECT_NAME} engine)
else ()
	if (ANDROID)
		message(STATUS "~~~~~~~~~~~~~~~~~ 1 ${NEOX_PGO_TYPE}")
		# add_definitions(-DPGO_PROFILE)
		# add_definitions(-fprofile-generate=/storage/emulated/0/Android/data/com.netease.s3gb/files/)
		if (${NEOX_PGO_TYPE} STREQUAL "PGO_PROFILE")
			message(STATUS "~~~~~~~~~~~~~~~~~ 2")
			add_definitions(-DPGO_PROFILE)
			add_definitions(-fprofile-generate=/storage/emulated/0/Android/data/${NEOX_BUNDLE_ID}/files/)

			set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX "-pgo-g.a")

		elseif(${NEOX_PGO_TYPE} STREQUAL "PGO_USE")
			add_definitions(-DPGO_USE)
			# add_definitions(-fprofile-use=C:/Users/liuhan/profraw/test.profdata)
			add_definitions(-fprofile-use=${NEOX_PGO_PROFILE_FILE})
			set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX "-pgo-u.a")
			message(STATUS "~~~~~~~~~~~~~~~~~! 3 ${NEOX_PGO_PROFILE_FILE}")
		endif()
		# add_definitions(-flto)
	endif()
endif(IS_IN_ENGINE_BUILD)
