# Lacunas de Plugins (flat lookup: o símbolo vem do app que carrega o plugin, não do mOS)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Plugins (flat lookup: o símbolo vem do app que carrega o plugin, não do mOS) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**301** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| self) | `_PyArg_UnpackTuple` | 2 | lm-studio, obs |
| self) | `_PyBool_FromLong` | 2 | lm-studio, obs |
| self) | `_PyBool_Type` | 2 | lm-studio, obs |
| self) | `_PyBytes_AsStringAndSize` | 2 | lm-studio, obs |
| self) | `_PyCapsule_GetPointer` | 2 | lm-studio, obs |
| self) | `_PyCapsule_New` | 2 | lm-studio, obs |
| self) | `_PyDict_New` | 2 | lm-studio, obs |
| self) | `_PyDict_SetItem` | 2 | lm-studio, obs |
| self) | `_PyDict_SetItemString` | 2 | lm-studio, obs |
| self) | `_PyErr_Clear` | 2 | lm-studio, obs |
| self) | `_PyErr_Format` | 2 | lm-studio, obs |
| self) | `_PyErr_Occurred` | 2 | lm-studio, obs |
| self) | `_PyErr_SetString` | 2 | lm-studio, obs |
| self) | `_PyExc_AttributeError` | 2 | lm-studio, obs |
| self) | `_PyExc_IndexError` | 2 | lm-studio, obs |
| self) | `_PyExc_OverflowError` | 2 | lm-studio, obs |
| self) | `_PyExc_RuntimeError` | 2 | lm-studio, obs |
| self) | `_PyExc_SystemError` | 2 | lm-studio, obs |
| self) | `_PyExc_TypeError` | 2 | lm-studio, obs |
| self) | `_PyExc_ValueError` | 2 | lm-studio, obs |
| self) | `_PyExc_ZeroDivisionError` | 2 | lm-studio, obs |
| self) | `_PyFloat_AsDouble` | 2 | lm-studio, obs |
| self) | `_PyFloat_FromDouble` | 2 | lm-studio, obs |
| self) | `_PyFloat_Type` | 2 | lm-studio, obs |
| self) | `_PyImport_AddModule` | 2 | lm-studio, obs |
| self) | `_PyLong_AsLong` | 2 | lm-studio, obs |
| self) | `_PyLong_AsLongLong` | 2 | lm-studio, obs |
| self) | `_PyLong_AsUnsignedLong` | 2 | lm-studio, obs |
| self) | `_PyLong_AsUnsignedLongLong` | 2 | lm-studio, obs |
| self) | `_PyLong_FromLong` | 2 | lm-studio, obs |
| self) | `_PyLong_FromSize_t` | 2 | lm-studio, obs |
| self) | `_PyLong_FromUnsignedLong` | 2 | lm-studio, obs |
| self) | `_PyLong_FromUnsignedLongLong` | 2 | lm-studio, obs |
| self) | `_PyModule_AddObject` | 2 | lm-studio, obs |
| self) | `_PyModule_Create2` | 2 | lm-studio, obs |
| self) | `_PyModule_GetDict` | 2 | lm-studio, obs |
| self) | `_PyObject_Call` | 2 | lm-studio, obs |
| self) | `_PyObject_CallFunctionObjArgs` | 2 | lm-studio, obs |
| self) | `_PyObject_GenericGetAttr` | 2 | lm-studio, obs |
| self) | `_PyObject_GetAttr` | 2 | lm-studio, obs |
| self) | `_PyObject_GetAttrString` | 2 | lm-studio, obs |
| self) | `_PyObject_IsTrue` | 2 | lm-studio, obs |
| self) | `_PyObject_SetAttr` | 2 | lm-studio, obs |
| self) | `_PyTuple_GetItem` | 2 | lm-studio, obs |
| self) | `_PyTuple_GetSlice` | 2 | lm-studio, obs |
| self) | `_PyTuple_New` | 2 | lm-studio, obs |
| self) | `_PyTuple_SetItem` | 2 | lm-studio, obs |
| self) | `_PyTuple_Size` | 2 | lm-studio, obs |
| self) | `_PyType_FromSpec` | 2 | lm-studio, obs |
| self) | `_PyType_GetFlags` | 2 | lm-studio, obs |
| self) | `_PyType_GetSlot` | 2 | lm-studio, obs |
| self) | `_PyType_IsSubtype` | 2 | lm-studio, obs |
| self) | `_PyType_Modified` | 2 | lm-studio, obs |
| self) | `_PyType_Type` | 2 | lm-studio, obs |
| self) | `_PyUnicode_Concat` | 2 | lm-studio, obs |
| self) | `_PyUnicode_DecodeUTF8` | 2 | lm-studio, obs |
| self) | `_PyUnicode_FromFormat` | 2 | lm-studio, obs |
| self) | `_PyUnicode_FromString` | 2 | lm-studio, obs |
| self) | `_PyUnicode_InternFromString` | 2 | lm-studio, obs |
| self) | `__Py_Dealloc` | 2 | lm-studio, obs |
| self) | `__Py_NoneStruct` | 2 | lm-studio, obs |
| self) | `__Py_NotImplementedStruct` | 2 | lm-studio, obs |
| self) | `_GetCurrentButtonState` | 1 | deluge |
| self) | `_Get_ng_vvp` | 1 | kicad |
| self) | `_OBJC_CLASS_$_ASCredentialIdentityStore` | 1 | bitwarden |
| self) | `_OBJC_CLASS_$_ASCredentialServiceIdentifier` | 1 | bitwarden |
| self) | `_OBJC_CLASS_$_ASPasswordCredentialIdentity` | 1 | bitwarden |
| self) | `_OBJC_CLASS_$_ASSettingsHelper` | 1 | bitwarden |
| self) | `_PyArg_ParseTuple` | 1 | lm-studio |
| self) | `_PyArg_ParseTupleAndKeywords` | 1 | lm-studio |
| self) | `_PyArg_ValidateKeywordArguments` | 1 | lm-studio |
| self) | `_PyBaseObject_Type` | 1 | lm-studio |
| self) | `_PyByteArray_AsString` | 1 | lm-studio |
| self) | `_PyByteArray_Size` | 1 | lm-studio |
| self) | `_PyByteArray_Type` | 1 | lm-studio |
| self) | `_PyBytes_AsString` | 1 | lm-studio |
| self) | `_PyBytes_FromFormat` | 1 | lm-studio |
| self) | `_PyBytes_FromStringAndSize` | 1 | lm-studio |
| self) | `_PyBytes_Size` | 1 | lm-studio |
| self) | `_PyBytes_Type` | 1 | lm-studio |
| self) | `_PyCFunction_GetFlags` | 1 | obs |
| self) | `_PyCFunction_GetFunction` | 1 | obs |
| self) | `_PyCFunction_GetSelf` | 1 | obs |
| self) | `_PyCFunction_Type` | 1 | lm-studio |
| self) | `_PyCMethod_New` | 1 | lm-studio |
| self) | `_PyCallable_Check` | 1 | lm-studio |
| self) | `_PyCapsule_GetName` | 1 | lm-studio |
| self) | `_PyCapsule_Import` | 1 | obs |
| self) | `_PyCapsule_IsValid` | 1 | lm-studio |
| self) | `_PyCode_NewEmpty` | 1 | lm-studio |
| self) | `_PyCode_NewWithPosOnlyArgs` | 1 | lm-studio |
| self) | `_PyDict_Contains` | 1 | lm-studio |
| self) | `_PyDict_DelItem` | 1 | lm-studio |
| self) | `_PyDict_DelItemString` | 1 | lm-studio |
| self) | `_PyDict_GetItem` | 1 | obs |
| self) | `_PyDict_GetItemString` | 1 | lm-studio |
| self) | `_PyDict_GetItemWithError` | 1 | lm-studio |
| self) | `_PyDict_Merge` | 1 | lm-studio |
| self) | `_PyDict_Next` | 1 | lm-studio |
| self) | `_PyDict_SetDefault` | 1 | lm-studio |
| self) | `_PyDict_Size` | 1 | lm-studio |
| self) | `_PyDict_Type` | 1 | lm-studio |
| self) | `_PyDict_Update` | 1 | lm-studio |
| self) | `_PyErr_ExceptionMatches` | 1 | lm-studio |
| self) | `_PyErr_Fetch` | 1 | obs |
| self) | `_PyErr_GivenExceptionMatches` | 1 | lm-studio |
| self) | `_PyErr_NoMemory` | 1 | lm-studio |
| self) | `_PyErr_NormalizeException` | 1 | lm-studio |
| self) | `_PyErr_Print` | 1 | lm-studio |
| self) | `_PyErr_Restore` | 1 | obs |
| self) | `_PyErr_SetNone` | 1 | lm-studio |
| self) | `_PyErr_SetObject` | 1 | lm-studio |
| self) | `_PyErr_WarnEx` | 1 | lm-studio |
| self) | `_PyErr_WarnFormat` | 1 | lm-studio |
| self) | `_PyErr_WriteUnraisable` | 1 | obs |
| self) | `_PyExc_DeprecationWarning` | 1 | lm-studio |
| self) | `_PyExc_Exception` | 1 | lm-studio |
| self) | `_PyExc_FutureWarning` | 1 | lm-studio |
| self) | `_PyExc_IOError` | 1 | obs |
| self) | `_PyExc_ImportError` | 1 | lm-studio |
| self) | `_PyExc_KeyError` | 1 | lm-studio |
| self) | `_PyExc_MemoryError` | 1 | lm-studio |
| self) | `_PyExc_NameError` | 1 | lm-studio |
| self) | `_PyExc_NotImplementedError` | 1 | lm-studio |
| self) | `_PyExc_OSError` | 1 | lm-studio |
| self) | `_PyExc_RuntimeWarning` | 1 | lm-studio |
| self) | `_PyExc_StopIteration` | 1 | lm-studio |
| self) | `_PyException_GetTraceback` | 1 | lm-studio |
| self) | `_PyException_SetTraceback` | 1 | lm-studio |
| self) | `_PyFrame_New` | 1 | lm-studio |
| self) | `_PyFrozenSet_New` | 1 | lm-studio |
| self) | `_PyFrozenSet_Type` | 1 | lm-studio |
| self) | `_PyGC_Disable` | 1 | lm-studio |
| self) | `_PyGC_Enable` | 1 | lm-studio |
| self) | `_PyGILState_Ensure` | 1 | obs |
| self) | `_PyGILState_Release` | 1 | obs |
| self) | `_PyImport_GetModule` | 1 | lm-studio |
| self) | `_PyImport_GetModuleDict` | 1 | lm-studio |
| self) | `_PyImport_ImportModule` | 1 | lm-studio |
| self) | `_PyImport_ImportModuleLevel` | 1 | lm-studio |
| self) | `_PyImport_ImportModuleLevelObject` | 1 | lm-studio |
| self) | `_PyInterpreterState_GetID` | 1 | lm-studio |
| self) | `_PyIter_Next` | 1 | lm-studio |
| self) | `_PyList_Append` | 1 | lm-studio |
| self) | `_PyList_GetItem` | 1 | lm-studio |
| self) | `_PyList_New` | 1 | lm-studio |
| self) | `_PyList_SetItem` | 1 | lm-studio |
| self) | `_PyList_Size` | 1 | lm-studio |
| self) | `_PyList_Sort` | 1 | lm-studio |
| self) | `_PyList_Type` | 1 | lm-studio |
| self) | `_PyLong_AsDouble` | 1 | obs |
| self) | `_PyLong_AsSsize_t` | 1 | lm-studio |
| self) | `_PyLong_FromLongLong` | 1 | lm-studio |
| self) | `_PyLong_FromSsize_t` | 1 | lm-studio |
| self) | `_PyLong_FromVoidPtr` | 1 | obs |
| self) | `_PyLong_Type` | 1 | lm-studio |
| self) | `_PyMem_Free` | 1 | lm-studio |
| self) | `_PyMem_Malloc` | 1 | lm-studio |
| self) | `_PyMem_Realloc` | 1 | lm-studio |
| self) | `_PyMemoryView_FromMemory` | 1 | lm-studio |
| self) | `_PyMemoryView_GetContiguous` | 1 | lm-studio |
| self) | `_PyMemoryView_Type` | 1 | lm-studio |
| self) | `_PyMethodDescr_Type` | 1 | lm-studio |
| self) | `_PyMethod_New` | 1 | lm-studio |
| self) | `_PyMethod_Type` | 1 | lm-studio |
| self) | `_PyModuleDef_Init` | 1 | lm-studio |
| self) | `_PyModule_AddIntConstant` | 1 | lm-studio |
| self) | `_PyModule_GetName` | 1 | lm-studio |
| self) | `_PyModule_GetState` | 1 | lm-studio |
| self) | `_PyModule_NewObject` | 1 | lm-studio |
| self) | `_PyNumber_Absolute` | 1 | lm-studio |
| self) | `_PyNumber_AsSsize_t` | 1 | lm-studio |
| self) | `_PyNumber_InPlaceAdd` | 1 | lm-studio |
| self) | `_PyNumber_Index` | 1 | lm-studio |
| self) | `_PyNumber_Or` | 1 | lm-studio |
| self) | `_PyNumber_TrueDivide` | 1 | lm-studio |
| self) | `_PyOS_double_to_string` | 1 | lm-studio |
| self) | `_PyOS_snprintf` | 1 | lm-studio |
| self) | `_PyObject_AsReadBuffer` | 1 | lm-studio |
| self) | `_PyObject_CallFinalizerFromDealloc` | 1 | lm-studio |
| self) | `_PyObject_CallFunction` | 1 | lm-studio |
| self) | `_PyObject_CallMethod` | 1 | lm-studio |
| self) | `_PyObject_CallMethodObjArgs` | 1 | lm-studio |
| self) | `_PyObject_CallObject` | 1 | lm-studio |
| self) | `_PyObject_ClearWeakRefs` | 1 | lm-studio |
| self) | `_PyObject_Format` | 1 | lm-studio |
| self) | `_PyObject_Free` | 1 | obs |
| self) | `_PyObject_GC_Del` | 1 | lm-studio |
| self) | `_PyObject_GC_IsFinalized` | 1 | lm-studio |
| self) | `_PyObject_GC_Track` | 1 | lm-studio |
| self) | `_PyObject_GC_UnTrack` | 1 | lm-studio |
| self) | `_PyObject_GenericGetDict` | 1 | lm-studio |
| self) | `_PyObject_GenericSetDict` | 1 | lm-studio |
| self) | `_PyObject_GetItem` | 1 | lm-studio |
| self) | `_PyObject_GetIter` | 1 | lm-studio |
| self) | `_PyObject_HasAttr` | 1 | lm-studio |
| self) | `_PyObject_HasAttrString` | 1 | lm-studio |
| self) | `_PyObject_Hash` | 1 | lm-studio |
| self) | `_PyObject_HashNotImplemented` | 1 | lm-studio |
| self) | `_PyObject_IsInstance` | 1 | obs |
| self) | `_PyObject_Repr` | 1 | lm-studio |
| self) | `_PyObject_RichCompare` | 1 | lm-studio |
| self) | `_PyObject_RichCompareBool` | 1 | lm-studio |
| self) | `_PyObject_SelfIter` | 1 | lm-studio |
| self) | `_PyObject_SetAttrString` | 1 | lm-studio |
| self) | `_PyObject_SetItem` | 1 | lm-studio |
| self) | `_PyObject_Size` | 1 | lm-studio |
| self) | `_PyObject_Str` | 1 | lm-studio |
| self) | `_PyObject_Vectorcall` | 1 | lm-studio |
| self) | `_PyObject_VectorcallDict` | 1 | lm-studio |
| self) | `_PyObject_VectorcallMethod` | 1 | lm-studio |
| self) | `_PySequence_Contains` | 1 | lm-studio |
| self) | `_PySequence_Fast` | 1 | lm-studio |
| self) | `_PySequence_GetItem` | 1 | lm-studio |
| self) | `_PySequence_List` | 1 | lm-studio |
| self) | `_PySequence_SetItem` | 1 | lm-studio |
| self) | `_PySet_New` | 1 | lm-studio |
| self) | `_PySet_Size` | 1 | lm-studio |
| self) | `_PySet_Type` | 1 | lm-studio |
| self) | `_PySlice_AdjustIndices` | 1 | lm-studio |
| self) | `_PySlice_New` | 1 | lm-studio |
| self) | `_PySlice_Type` | 1 | lm-studio |
| self) | `_PySlice_Unpack` | 1 | lm-studio |
| self) | `_PyState_FindModule` | 1 | lm-studio |
| self) | `_PyThreadState_Get` | 1 | lm-studio |
| self) | `_PyTraceBack_Here` | 1 | lm-studio |
| self) | `_PyTuple_Pack` | 1 | lm-studio |
| self) | `_PyTuple_Type` | 1 | lm-studio |
| self) | `_PyType_FromModuleAndSpec` | 1 | lm-studio |
| self) | `_PyType_FromSpecWithBases` | 1 | lm-studio |
| self) | `_PyType_GenericAlloc` | 1 | lm-studio |
| self) | `_PyType_Ready` | 1 | lm-studio |
| self) | `_PyUnicode_AsUTF8AndSize` | 1 | lm-studio |
| self) | `_PyUnicode_AsUTF8String` | 1 | obs |
| self) | `_PyUnicode_Compare` | 1 | lm-studio |
| self) | `_PyUnicode_CompareWithASCIIString` | 1 | obs |
| self) | `_PyUnicode_Contains` | 1 | lm-studio |
| self) | `_PyUnicode_FindChar` | 1 | lm-studio |
| self) | `_PyUnicode_FromEncodedObject` | 1 | lm-studio |
| self) | `_PyUnicode_FromKindAndData` | 1 | lm-studio |
| self) | `_PyUnicode_FromOrdinal` | 1 | lm-studio |
| self) | `_PyUnicode_FromStringAndSize` | 1 | lm-studio |
| self) | `_PyUnicode_InternInPlace` | 1 | lm-studio |
| self) | `_PyUnicode_Join` | 1 | lm-studio |
| self) | `_PyUnicode_New` | 1 | lm-studio |
| self) | `_PyUnicode_Resize` | 1 | lm-studio |
| self) | `_PyUnicode_Split` | 1 | lm-studio |
| self) | `_PyUnicode_Substring` | 1 | lm-studio |
| self) | `_PyUnicode_Tailmatch` | 1 | lm-studio |
| self) | `_PyUnicode_Type` | 1 | lm-studio |
| self) | `_Py_BuildValue` | 1 | lm-studio |
| self) | `_Py_DecRef` | 1 | obs |
| self) | `_Py_EnterRecursiveCall` | 1 | lm-studio |
| self) | `_Py_GenericAlias` | 1 | lm-studio |
| self) | `_Py_LeaveRecursiveCall` | 1 | lm-studio |
| self) | `_Py_Version` | 1 | lm-studio |
| self) | `_SLSCopyManagedDisplaySpaces` | 1 | hammerspoon |
| self) | `_SLSCopySpacesForWindows` | 1 | hammerspoon |
| self) | `_SLSCopyWindowsWithOptionsAndTags` | 1 | hammerspoon |
| self) | `_SLSGetActiveSpace` | 1 | hammerspoon |
| self) | `_SLSMainConnectionID` | 1 | hammerspoon |
| self) | `_SLSMoveWindowsToManagedSpace` | 1 | hammerspoon |
| self) | `_SLSSetWindowListWorkspace` | 1 | hammerspoon |
| self) | `_SLSSpaceGetType` | 1 | hammerspoon |
| self) | `_SLSSpaceSetCompatID` | 1 | hammerspoon |
| self) | `_SetSystemUIMode` | 1 | deluge |
| self) | `__PyByteArray_empty_string` | 1 | lm-studio |
| self) | `__PyDict_GetItem_KnownHash` | 1 | lm-studio |
| self) | `__PyDict_NewPresized` | 1 | lm-studio |
| self) | `__PyLong_Copy` | 1 | lm-studio |
| self) | `__PyObject_GC_New` | 1 | lm-studio |
| self) | `__PyObject_GenericGetAttrWithDict` | 1 | lm-studio |
| self) | `__PyObject_GetDictPtr` | 1 | lm-studio |
| self) | `__PyObject_New` | 1 | obs |
| self) | `__PySet_NextEntry` | 1 | lm-studio |
| self) | `__PyThreadState_UncheckedGet` | 1 | lm-studio |
| self) | `__PyType_Lookup` | 1 | lm-studio |
| self) | `__PyUnicode_FastCopyCharacters` | 1 | lm-studio |
| self) | `__PyUnicode_Ready` | 1 | lm-studio |
| self) | `__Py_FalseStruct` | 1 | lm-studio |
| self) | `__Py_TrueStruct` | 1 | lm-studio |
| self) | `__ZdlPvSt19__type_descriptor_t` | 1 | utm |
| self) | `___divdc3` | 1 | kicad |
| self) | `_csinh` | 1 | kicad |
| self) | `_ctanh` | 1 | kicad |
| self) | `_fmemopen` | 1 | insomnia |
| self) | `_pthread_getconcurrency` | 1 | insomnia |
| self) | `_pthread_setcanceltype` | 1 | libreoffice |
| self) | `_pthread_setconcurrency` | 1 | insomnia |
| self) | `_vpi_control` | 1 | kicad |
| self) | `_vpi_free_object` | 1 | kicad |
| self) | `_vpi_get` | 1 | kicad |
| self) | `_vpi_get_str` | 1 | kicad |
| self) | `_vpi_get_time` | 1 | kicad |
| self) | `_vpi_handle_by_name` | 1 | kicad |
| self) | `_vpi_iterate` | 1 | kicad |
| self) | `_vpi_printf` | 1 | kicad |
| self) | `_vpi_put_value` | 1 | kicad |
| self) | `_vpi_register_cb` | 1 | kicad |
| self) | `_vpi_remove_cb` | 1 | kicad |
| self) | `_vpi_scan` | 1 | kicad |
