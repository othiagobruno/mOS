# Lacunas de Plugins (flat lookup: o símbolo vem do app que carrega o plugin, não do mOS)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Plugins (flat lookup: o símbolo vem do app que carrega o plugin, não do mOS) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**2418** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| self) | `_PyDict_New` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyDict_SetItem` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyDict_SetItemString` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyErr_Clear` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyErr_Format` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyErr_Occurred` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyErr_SetString` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyExc_AttributeError` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyExc_RuntimeError` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyExc_TypeError` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyExc_ValueError` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyFloat_FromDouble` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyFloat_Type` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyLong_AsLong` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyLong_FromLong` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyModule_GetDict` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyObject_GetAttrString` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyTuple_GetItem` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyTuple_New` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyTuple_Size` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyType_IsSubtype` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyType_Type` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `__Py_NoneStruct` | 5 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyDict_GetItem` | 4 | obs, pycharm, sourcetree, virtualbox |
| self) | `_PyErr_NoMemory` | 4 | lm-studio, pycharm, sourcetree, virtualbox |
| self) | `_PyErr_SetObject` | 4 | lm-studio, pycharm, sourcetree, virtualbox |
| self) | `_PyExc_IndexError` | 4 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyExc_OverflowError` | 4 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyExc_SystemError` | 4 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyImport_AddModule` | 4 | lm-studio, obs, pycharm, virtualbox |
| self) | `_PyImport_ImportModule` | 4 | lm-studio, pycharm, sourcetree, virtualbox |
| self) | `_PyList_Append` | 4 | lm-studio, pycharm, sourcetree, virtualbox |
| self) | `_PyList_New` | 4 | lm-studio, pycharm, sourcetree, virtualbox |
| self) | `_PyLong_AsLongLong` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyLong_FromUnsignedLong` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyLong_FromUnsignedLongLong` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyMem_Free` | 4 | lm-studio, pycharm, sourcetree, virtualbox |
| self) | `_PyMem_Malloc` | 4 | lm-studio, pycharm, sourcetree, virtualbox |
| self) | `_PyModule_AddObject` | 4 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyObject_CallFunctionObjArgs` | 4 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyObject_GetAttr` | 4 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyObject_IsTrue` | 4 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyTuple_GetSlice` | 4 | lm-studio, obs, pycharm, sourcetree |
| self) | `_PyTuple_SetItem` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyUnicode_FromString` | 4 | lm-studio, obs, pycharm, virtualbox |
| self) | `__Py_Dealloc` | 4 | lm-studio, obs, pycharm, virtualbox |
| self) | `__Py_TrueStruct` | 4 | lm-studio, pycharm, sourcetree, virtualbox |
| self) | `_PyArg_ParseTuple` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyBaseObject_Type` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyBool_FromLong` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyBool_Type` | 3 | lm-studio, obs, sourcetree |
| self) | `_PyBytes_AsString` | 3 | lm-studio, pycharm, virtualbox |
| self) | `_PyBytes_FromStringAndSize` | 3 | lm-studio, pycharm, virtualbox |
| self) | `_PyCapsule_GetPointer` | 3 | lm-studio, obs, pycharm |
| self) | `_PyCapsule_New` | 3 | lm-studio, obs, pycharm |
| self) | `_PyDict_Contains` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyDict_DelItem` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyDict_GetItemString` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyDict_Next` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyDict_Size` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyDict_Type` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyErr_ExceptionMatches` | 3 | lm-studio, pycharm, virtualbox |
| self) | `_PyErr_Fetch` | 3 | obs, pycharm, virtualbox |
| self) | `_PyErr_NormalizeException` | 3 | lm-studio, pycharm, virtualbox |
| self) | `_PyErr_Restore` | 3 | obs, pycharm, virtualbox |
| self) | `_PyExc_Exception` | 3 | lm-studio, pycharm, virtualbox |
| self) | `_PyExc_ImportError` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyExc_KeyError` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyExc_MemoryError` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyExc_NotImplementedError` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyExc_StopIteration` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyFloat_AsDouble` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyIter_Next` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyList_SetItem` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyList_Type` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyLong_AsUnsignedLongLong` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyLong_FromSize_t` | 3 | lm-studio, obs, sourcetree |
| self) | `_PyLong_FromSsize_t` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyMem_Realloc` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyModule_Create2` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyObject_Call` | 3 | lm-studio, obs, pycharm |
| self) | `_PyObject_CallMethod` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyObject_CallMethodObjArgs` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyObject_CallObject` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyObject_Free` | 3 | obs, pycharm, sourcetree |
| self) | `_PyObject_GC_UnTrack` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyObject_GenericGetAttr` | 3 | lm-studio, obs, pycharm |
| self) | `_PyObject_GetItem` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyObject_GetIter` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyObject_HasAttr` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyObject_HasAttrString` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyObject_RichCompareBool` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyObject_SetAttr` | 3 | lm-studio, obs, pycharm |
| self) | `_PyObject_SetAttrString` | 3 | lm-studio, pycharm, virtualbox |
| self) | `_PyObject_Str` | 3 | lm-studio, pycharm, virtualbox |
| self) | `_PySequence_GetItem` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PySet_New` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyTuple_Pack` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyTuple_Type` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyType_FromSpec` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyType_GetFlags` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyType_Modified` | 3 | lm-studio, obs, pycharm |
| self) | `_PyType_Ready` | 3 | lm-studio, pycharm, sourcetree |
| self) | `_PyUnicode_Concat` | 3 | lm-studio, obs, pycharm |
| self) | `_PyUnicode_DecodeUTF8` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyUnicode_FromFormat` | 3 | lm-studio, obs, pycharm |
| self) | `_PyUnicode_FromStringAndSize` | 3 | lm-studio, pycharm, virtualbox |
| self) | `_PyUnicode_InternFromString` | 3 | lm-studio, obs, pycharm |
| self) | `_Py_BuildValue` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `__PyObject_GetDictPtr` | 3 | lm-studio, pycharm, sourcetree |
| self) | `__Py_FalseStruct` | 3 | lm-studio, pycharm, virtualbox |
| self) | `_CRYPTO_memcmp` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_ERR_clear_error` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_ERR_error_string_n` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_ERR_get_error` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_ERR_pop_to_mark` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_ERR_set_mark` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_CIPHER_CTX_ctrl` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_CIPHER_CTX_free` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_CIPHER_CTX_get0_cipher` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_CIPHER_CTX_new` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_CIPHER_CTX_set_key_length` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_CIPHER_CTX_set_padding` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_CIPHER_get_mode` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_DecryptFinal_ex` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_DecryptInit_ex` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_DecryptUpdate` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_EncryptFinal_ex` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_EncryptInit_ex` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_EncryptUpdate` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_MAC_CTX_dup` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_MAC_CTX_free` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_MAC_CTX_get_mac_size` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_MAC_CTX_new` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_MAC_fetch` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_MAC_final` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_MAC_free` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_MAC_init` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_MAC_update` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_aes_128_gcm` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_aes_256_gcm` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_get_cipherbyname` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_EVP_rc4` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_OSSL_PARAM_construct_utf8_string` | 2 | beekeeper-studio, mongodb-compass |
| self) | `_PyArg_ParseTupleAndKeywords` | 2 | lm-studio, sourcetree |
| self) | `_PyArg_UnpackTuple` | 2 | lm-studio, obs |
| self) | `_PyArg_ValidateKeywordArguments` | 2 | lm-studio, pycharm |
| self) | `_PyBytes_AsStringAndSize` | 2 | lm-studio, obs |
| self) | `_PyBytes_Size` | 2 | lm-studio, virtualbox |
| self) | `_PyCFunction_NewEx` | 2 | pycharm, virtualbox |
| self) | `_PyCFunction_Type` | 2 | lm-studio, pycharm |
| self) | `_PyCMethod_New` | 2 | lm-studio, pycharm |
| self) | `_PyCallable_Check` | 2 | lm-studio, sourcetree |
| self) | `_PyCode_NewEmpty` | 2 | lm-studio, pycharm |
| self) | `_PyDict_DelItemString` | 2 | lm-studio, sourcetree |
| self) | `_PyDict_GetItemWithError` | 2 | lm-studio, pycharm |
| self) | `_PyDict_SetDefault` | 2 | lm-studio, pycharm |
| self) | `_PyErr_GivenExceptionMatches` | 2 | lm-studio, pycharm |
| self) | `_PyErr_Print` | 2 | lm-studio, virtualbox |
| self) | `_PyErr_SetNone` | 2 | lm-studio, sourcetree |
| self) | `_PyErr_WarnEx` | 2 | lm-studio, pycharm |
| self) | `_PyErr_WarnFormat` | 2 | lm-studio, pycharm |
| self) | `_PyErr_WriteUnraisable` | 2 | obs, pycharm |
| self) | `_PyEval_CallObjectWithKeywords` | 2 | sourcetree, virtualbox |
| self) | `_PyEval_RestoreThread` | 2 | sourcetree, virtualbox |
| self) | `_PyEval_SaveThread` | 2 | sourcetree, virtualbox |
| self) | `_PyExc_DeprecationWarning` | 2 | lm-studio, pycharm |
| self) | `_PyExc_NameError` | 2 | lm-studio, pycharm |
| self) | `_PyExc_OSError` | 2 | lm-studio, sourcetree |
| self) | `_PyExc_RuntimeWarning` | 2 | lm-studio, pycharm |
| self) | `_PyExc_ZeroDivisionError` | 2 | lm-studio, obs |
| self) | `_PyException_GetTraceback` | 2 | lm-studio, pycharm |
| self) | `_PyException_SetTraceback` | 2 | lm-studio, pycharm |
| self) | `_PyFrame_New` | 2 | lm-studio, pycharm |
| self) | `_PyFrozenSet_New` | 2 | lm-studio, pycharm |
| self) | `_PyFrozenSet_Type` | 2 | lm-studio, pycharm |
| self) | `_PyFunction_Type` | 2 | pycharm, sourcetree |
| self) | `_PyGC_Disable` | 2 | lm-studio, pycharm |
| self) | `_PyGC_Enable` | 2 | lm-studio, pycharm |
| self) | `_PyGILState_Ensure` | 2 | obs, virtualbox |
| self) | `_PyGILState_Release` | 2 | obs, virtualbox |
| self) | `_PyImport_GetModule` | 2 | lm-studio, pycharm |
| self) | `_PyImport_GetModuleDict` | 2 | lm-studio, pycharm |
| self) | `_PyImport_Import` | 2 | pycharm, sourcetree |
| self) | `_PyImport_ImportModuleLevelObject` | 2 | lm-studio, pycharm |
| self) | `_PyInt_AsSsize_t` | 2 | pycharm, sourcetree |
| self) | `_PyInt_FromLong` | 2 | pycharm, sourcetree |
| self) | `_PyInt_FromSsize_t` | 2 | pycharm, sourcetree |
| self) | `_PyInt_Type` | 2 | pycharm, sourcetree |
| self) | `_PyInterpreterState_GetID` | 2 | lm-studio, pycharm |
| self) | `_PyList_GetItem` | 2 | lm-studio, sourcetree |
| self) | `_PyList_Size` | 2 | lm-studio, sourcetree |
| self) | `_PyList_Sort` | 2 | lm-studio, sourcetree |
| self) | `_PyLong_AsSsize_t` | 2 | lm-studio, pycharm |
| self) | `_PyLong_AsUnsignedLong` | 2 | lm-studio, obs |
| self) | `_PyLong_FromLongLong` | 2 | lm-studio, virtualbox |
| self) | `_PyLong_Type` | 2 | lm-studio, pycharm |
| self) | `_PyMethodDescr_Type` | 2 | lm-studio, pycharm |
| self) | `_PyMethod_New` | 2 | lm-studio, pycharm |
| self) | `_PyMethod_Type` | 2 | lm-studio, pycharm |
| self) | `_PyModuleDef_Init` | 2 | lm-studio, pycharm |
| self) | `_PyModule_AddIntConstant` | 2 | lm-studio, sourcetree |
| self) | `_PyModule_GetName` | 2 | lm-studio, pycharm |
| self) | `_PyModule_NewObject` | 2 | lm-studio, pycharm |
| self) | `_PyNumber_AsSsize_t` | 2 | lm-studio, sourcetree |
| self) | `_PyNumber_InPlaceAdd` | 2 | lm-studio, pycharm |
| self) | `_PyNumber_Index` | 2 | lm-studio, pycharm |
| self) | `_PyNumber_Or` | 2 | lm-studio, pycharm |
| self) | `_PyOS_snprintf` | 2 | lm-studio, pycharm |
| self) | `_PyObject_AsReadBuffer` | 2 | lm-studio, virtualbox |
| self) | `_PyObject_CallFinalizerFromDealloc` | 2 | lm-studio, pycharm |
| self) | `_PyObject_ClearWeakRefs` | 2 | lm-studio, pycharm |
| self) | `_PyObject_Format` | 2 | lm-studio, pycharm |
| self) | `_PyObject_GC_Del` | 2 | lm-studio, pycharm |
| self) | `_PyObject_GC_IsFinalized` | 2 | lm-studio, pycharm |
| self) | `_PyObject_GC_Track` | 2 | lm-studio, pycharm |
| self) | `_PyObject_Hash` | 2 | lm-studio, pycharm |
| self) | `_PyObject_IsInstance` | 2 | obs, pycharm |
| self) | `_PyObject_Repr` | 2 | lm-studio, virtualbox |
| self) | `_PyObject_RichCompare` | 2 | lm-studio, pycharm |
| self) | `_PyObject_SelfIter` | 2 | lm-studio, sourcetree |
| self) | `_PyObject_SetItem` | 2 | lm-studio, pycharm |
| self) | `_PyObject_Size` | 2 | lm-studio, pycharm |
| self) | `_PyObject_Vectorcall` | 2 | lm-studio, pycharm |
| self) | `_PyObject_VectorcallDict` | 2 | lm-studio, pycharm |
| self) | `_PyObject_VectorcallMethod` | 2 | lm-studio, pycharm |
| self) | `_PySequence_Check` | 2 | sourcetree, virtualbox |
| self) | `_PySequence_Contains` | 2 | lm-studio, pycharm |
| self) | `_PySequence_List` | 2 | lm-studio, pycharm |
| self) | `_PySequence_Size` | 2 | sourcetree, virtualbox |
| self) | `_PySet_Add` | 2 | pycharm, sourcetree |
| self) | `_PySet_Type` | 2 | lm-studio, pycharm |
| self) | `_PySlice_Type` | 2 | lm-studio, sourcetree |
| self) | `_PyString_AsString` | 2 | pycharm, sourcetree |
| self) | `_PyString_FromString` | 2 | pycharm, sourcetree |
| self) | `_PyString_FromStringAndSize` | 2 | pycharm, sourcetree |
| self) | `_PyString_Type` | 2 | pycharm, sourcetree |
| self) | `_PyThreadState_Get` | 2 | lm-studio, pycharm |
| self) | `_PyTraceBack_Here` | 2 | lm-studio, pycharm |
| self) | `_PyType_GetSlot` | 2 | lm-studio, obs |
| self) | `_PyUnicodeUCS2_FromStringAndSize` | 2 | pycharm, sourcetree |
| self) | `_PyUnicode_AsUTF8AndSize` | 2 | lm-studio, virtualbox |
| self) | `_PyUnicode_AsUTF8String` | 2 | obs, virtualbox |
| self) | `_PyUnicode_Compare` | 2 | lm-studio, pycharm |
| self) | `_PyUnicode_FromOrdinal` | 2 | lm-studio, pycharm |
| self) | `_PyUnicode_Join` | 2 | lm-studio, pycharm |
| self) | `_PyUnicode_New` | 2 | lm-studio, pycharm |
| self) | `_PyUnicode_Type` | 2 | lm-studio, pycharm |
| self) | `_Py_EnterRecursiveCall` | 2 | lm-studio, pycharm |
| self) | `_Py_GetVersion` | 2 | pycharm, sourcetree |
| self) | `_Py_InitModule4_64` | 2 | pycharm, sourcetree |
| self) | `_Py_LeaveRecursiveCall` | 2 | lm-studio, pycharm |
| self) | `_Py_Version` | 2 | lm-studio, pycharm |
| self) | `_SLSMainConnectionID` | 2 | bettertouchtool, hammerspoon |
| self) | `_SLSSetWindowListWorkspace` | 2 | bettertouchtool, hammerspoon |
| self) | `_SLSSpaceSetCompatID` | 2 | bettertouchtool, hammerspoon |
| self) | `__PyDict_GetItem_KnownHash` | 2 | lm-studio, pycharm |
| self) | `__PyDict_NewPresized` | 2 | lm-studio, sourcetree |
| self) | `__PyObject_GC_New` | 2 | lm-studio, pycharm |
| self) | `__PyObject_GenericGetAttrWithDict` | 2 | lm-studio, pycharm |
| self) | `__PyObject_New` | 2 | obs, sourcetree |
| self) | `__PyThreadState_UncheckedGet` | 2 | lm-studio, pycharm |
| self) | `__PyType_Lookup` | 2 | lm-studio, pycharm |
| self) | `__PyUnicode_FastCopyCharacters` | 2 | lm-studio, pycharm |
| self) | `__PyUnicode_Ready` | 2 | lm-studio, pycharm |
| self) | `__Py_NotImplementedStruct` | 2 | lm-studio, obs |
| self) | `__Py_ZeroStruct` | 2 | pycharm, sourcetree |
| self) | `__ZN2v811HandleScope12CreateHandleEPNS_8internal7IsolateEm` | 2 | beekeeper-studio, mongodb-compass |
| self) | `__ZN2v811HandleScopeC1EPNS_7IsolateE` | 2 | beekeeper-studio, mongodb-compass |
| self) | `__ZN2v811HandleScopeD1Ev` | 2 | beekeeper-studio, mongodb-compass |
| self) | `__ZN2v811HandleScopeD2Ev` | 2 | beekeeper-studio, mongodb-compass |
| self) | `__ZN2v86Object32SetAlignedPointerInInternalFieldEiPv` | 2 | beekeeper-studio, mongodb-compass |
| self) | `__ZN2v86Object38SlowGetAlignedPointerFromInternalFieldEi` | 2 | beekeeper-studio, mongodb-compass |
| self) | `__ZN2v87Context10GetIsolateEv` | 2 | beekeeper-studio, mongodb-compass |
| self) | `__ZN2v88External3NewEPNS_7IsolateEPv` | 2 | beekeeper-studio, mongodb-compass |
| self) | `__ZNK2v86String11WriteUtf8V2EPNS_7IsolateEPcmiPm` | 2 | beekeeper-studio, mongodb-compass |
| self) | `__ZNK2v88External5ValueEv` | 2 | beekeeper-studio, mongodb-compass |
| self) | `__ZdlPvSt19__type_descriptor_t` | 2 | crossover, utm |
| self) | `_ALCALSCopyALSServiceClient` | 1 | bettertouchtool |
| self) | `_AllocSetContextCreateInternal` | 1 | freecad |
| self) | `_BeginInternalSubTransaction` | 1 | freecad |
| self) | `_BlessTupleDesc` | 1 | freecad |
| self) | `_BuildTupleFromCStrings` | 1 | freecad |
| self) | `_CADDR` | 1 | rstudio |
| self) | `_CADR` | 1 | rstudio |
| self) | `_CAR` | 1 | rstudio |
| self) | `_CDDDR` | 1 | rstudio |
| self) | `_CDDR` | 1 | rstudio |
| self) | `_CDR` | 1 | rstudio |
| self) | `_COMPLEX` | 1 | rstudio |
| self) | `_CacheMemoryContext` | 1 | freecad |
| self) | `_CacheRegisterRelcacheCallback` | 1 | freecad |
| self) | `_CacheRegisterSyscacheCallback` | 1 | freecad |
| self) | `_CachedPlanAllowsSimpleValidityCheck` | 1 | freecad |
| self) | `_CachedPlanIsSimplyValid` | 1 | freecad |
| self) | `_CheckFunctionValidatorAccess` | 1 | freecad |
| self) | `_CommandCounterIncrement` | 1 | freecad |
| self) | `_CopyErrorData` | 1 | freecad |
| self) | `_CreateDestReceiver` | 1 | freecad |
| self) | `_CreateExecutorState` | 1 | freecad |
| self) | `_CreateExprContext` | 1 | freecad |
| self) | `_CreateItemMoniker` | 1 | microsoft-office |
| self) | `_CreateTemplateTupleDesc` | 1 | freecad |
| self) | `_CreateTupleDescCopy` | 1 | freecad |
| self) | `_CreateTupleDescCopyConstr` | 1 | freecad |
| self) | `_CurrentMemoryContext` | 1 | freecad |
| self) | `_CurrentResourceOwner` | 1 | freecad |
| self) | `_DatumGetEOHP` | 1 | freecad |
| self) | `_DecrTupleDescRefCount` | 1 | freecad |
| self) | `_DefineCustomBoolVariable` | 1 | freecad |
| self) | `_DefineCustomEnumVariable` | 1 | freecad |
| self) | `_DefineCustomStringVariable` | 1 | freecad |
| self) | `_DeleteExpandedObject` | 1 | freecad |
| self) | `_DirectFunctionCall1Coll` | 1 | freecad |
| self) | `_EOH_flatten_into` | 1 | freecad |
| self) | `_EOH_get_flat_size` | 1 | freecad |
| self) | `_EnsurePortalSnapshotExists` | 1 | freecad |
| self) | `_ExecDropSingleTupleTableSlot` | 1 | freecad |
| self) | `_ExecInitExpr` | 1 | freecad |
| self) | `_ExecInitExprWithParams` | 1 | freecad |
| self) | `_ExecInitRangeTable` | 1 | freecad |
| self) | `_ExecPrepareExpr` | 1 | freecad |
| self) | `_ExecStoreHeapTuple` | 1 | freecad |
| self) | `_ExecStoreVirtualTuple` | 1 | freecad |
| self) | `_ExprEvalPushStep` | 1 | freecad |
| self) | `_FlushErrorState` | 1 | freecad |
| self) | `_FreeCachedExpression` | 1 | freecad |
| self) | `_FreeExecutorState` | 1 | freecad |
| self) | `_FreeExprContext` | 1 | freecad |
| self) | `_FreeTupleDesc` | 1 | freecad |
| self) | `_GEaddDevice2` | 1 | rstudio |
| self) | `_GEcopyDisplayList` | 1 | rstudio |
| self) | `_GEcreateDevDesc` | 1 | rstudio |
| self) | `_GEcurrentDevice` | 1 | rstudio |
| self) | `_GEdeviceNumber` | 1 | rstudio |
| self) | `_GEgetDevice` | 1 | rstudio |
| self) | `_GEkillDevice` | 1 | rstudio |
| self) | `_GEplayDisplayList` | 1 | rstudio |
| self) | `_GUC_check_errdetail_string` | 1 | freecad |
| self) | `_GetCachedExpression` | 1 | freecad |
| self) | `_GetCommandTagName` | 1 | freecad |
| self) | `_GetCurrentCommandId` | 1 | freecad |
| self) | `_GetCurrentSubTransactionId` | 1 | freecad |
| self) | `_GetDatabaseEncoding` | 1 | freecad |
| self) | `_GetDatabaseEncodingName` | 1 | freecad |
| self) | `_GetErrorContextStack` | 1 | freecad |
| self) | `_GetPublicationByName` | 1 | freecad |
| self) | `_GetRelationPublications` | 1 | freecad |
| self) | `_GetRunningObjectTable` | 1 | microsoft-office |
| self) | `_GetSchemaPublications` | 1 | freecad |
| self) | `_GetTopMostAncestorInPublication` | 1 | freecad |
| self) | `_GetTransactionSnapshot` | 1 | freecad |
| self) | `_Get_ng_vvp` | 1 | kicad |
| self) | `_HeapTupleHeaderGetDatum` | 1 | freecad |
| self) | `_INTEGER` | 1 | rstudio |
| self) | `_IODestroyPlugInInterface` | 1 | feishu |
| self) | `_InterruptPending` | 1 | freecad |
| self) | `_ItemPointerEquals` | 1 | freecad |
| self) | `_LENGTH` | 1 | rstudio |
| self) | `_LOGICAL` | 1 | rstudio |
| self) | `_LdiDFSResetStat` | 1 | qgis |
| self) | `_LdiDateArrayFromLocalArray` | 1 | qgis |
| self) | `_LdiDateArrayToLocalArray` | 1 | qgis |
| self) | `_LdiDateCheck` | 1 | qgis |
| self) | `_LdiDateCompare` | 1 | qgis |
| self) | `_LdiDateConstruct` | 1 | qgis |
| self) | `_LdiDateConstructN` | 1 | qgis |
| self) | `_LdiDateCopy` | 1 | qgis |
| self) | `_LdiDateDateConvert` | 1 | qgis |
| self) | `_LdiDateDateOverlap` | 1 | qgis |
| self) | `_LdiDateDateSubtract` | 1 | qgis |
| self) | `_LdiDateDateSubtractWithTZ` | 1 | qgis |
| self) | `_LdiDateExtract` | 1 | qgis |
| self) | `_LdiDateExtractTZ` | 1 | qgis |
| self) | `_LdiDateFromArray` | 1 | qgis |
| self) | `_LdiDateFromLDX` | 1 | qgis |
| self) | `_LdiDateFromLiteral` | 1 | qgis |
| self) | `_LdiDateFromLocalArray` | 1 | qgis |
| self) | `_LdiDateFromString` | 1 | qgis |
| self) | `_LdiDateFromString1` | 1 | qgis |
| self) | `_LdiDateFromStringU` | 1 | qgis |
| self) | `_LdiDateInterAdd` | 1 | qgis |
| self) | `_LdiDateInterAddTZ` | 1 | qgis |
| self) | `_LdiDateInterSubtract` | 1 | qgis |
| self) | `_LdiDateInterSubtractTZ` | 1 | qgis |
| self) | `_LdiDateTimeArrayUpgrade` | 1 | qgis |
| self) | `_LdiDateToArray` | 1 | qgis |
| self) | `_LdiDateToJulian` | 1 | qgis |
| self) | `_LdiDateToLDX` | 1 | qgis |
| self) | `_LdiDateToLocalArray` | 1 | qgis |
| self) | `_LdiDateToString` | 1 | qgis |
| self) | `_LdiDateToStringU` | 1 | qgis |
| self) | `_LdiInitDef` | 1 | qgis |
| self) | `_LdiInterCheck` | 1 | qgis |
| self) | `_LdiInterCompare` | 1 | qgis |
| self) | `_LdiInterConstruct` | 1 | qgis |
| self) | `_LdiInterCopy` | 1 | qgis |
| self) | `_LdiInterDivide` | 1 | qgis |
| self) | `_LdiInterExtract` | 1 | qgis |
| self) | `_LdiInterFromArray` | 1 | qgis |
| self) | `_LdiInterFromNumber` | 1 | qgis |
| self) | `_LdiInterFromString` | 1 | qgis |
| self) | `_LdiInterFromStringU` | 1 | qgis |
| self) | `_LdiInterFromTZ` | 1 | qgis |
| self) | `_LdiInterFromTZU` | 1 | qgis |
| self) | `_LdiInterInterAdd` | 1 | qgis |
| self) | `_LdiInterInterConvert` | 1 | qgis |
| self) | `_LdiInterInterSubtract` | 1 | qgis |
| self) | `_LdiInterMultiply` | 1 | qgis |
| self) | `_LdiInterToArray` | 1 | qgis |
| self) | `_LdiInterToNumber` | 1 | qgis |
| self) | `_LdiInterToString` | 1 | qgis |
| self) | `_LdiInterToStringISO` | 1 | qgis |
| self) | `_LdiInterToStringU` | 1 | qgis |
| self) | `_LdiInterToTN` | 1 | qgis |
| self) | `_LdiParseContextInit` | 1 | qgis |
| self) | `_LdiParseContextTerm` | 1 | qgis |
| self) | `_LdiParseForInput` | 1 | qgis |
| self) | `_LdiParseForInputType` | 1 | qgis |
| self) | `_LdiParseForInputTypeU` | 1 | qgis |
| self) | `_LdiParseForOutput` | 1 | qgis |
| self) | `_LdiParseForOutputU` | 1 | qgis |
| self) | `_LdiPrecisionCode` | 1 | qgis |
| self) | `_LdiRound` | 1 | qgis |
| self) | `_LdiTZGetDelta` | 1 | qgis |
| self) | `_LdiTZGetDeltaBitmap` | 1 | qgis |
| self) | `_LdiTZGetDeltaMinAffectedTime` | 1 | qgis |
| self) | `_LdiTZGetDeltaSize` | 1 | qgis |
| self) | `_LdiTZGetPathAndSize` | 1 | qgis |
| self) | `_LdiTZReadFile` | 1 | qgis |
| self) | `_LdiTimeZoneAdjust` | 1 | qgis |
| self) | `_LdiTimeZoneShift` | 1 | qgis |
| self) | `_LehInitialize` | 1 | qgis |
| self) | `_LehTerminate` | 1 | qgis |
| self) | `_LhtIntBeginIter` | 1 | qgis |
| self) | `_LhtIntCreate` | 1 | qgis |
| self) | `_LhtIntDestroy` | 1 | qgis |
| self) | `_LhtIntEndIter` | 1 | qgis |
| self) | `_LhtIntInsert` | 1 | qgis |
| self) | `_LhtIntRemove` | 1 | qgis |
| self) | `_LhtIntYield` | 1 | qgis |
| self) | `_LhtStrBeginIter` | 1 | qgis |
| self) | `_LhtStrCreate` | 1 | qgis |
| self) | `_LhtStrDestroy` | 1 | qgis |
| self) | `_LhtStrEndIter` | 1 | qgis |
| self) | `_LhtStrInsert` | 1 | qgis |
| self) | `_LhtStrRemove` | 1 | qgis |
| self) | `_LhtStrSearch` | 1 | qgis |
| self) | `_LhtStrYield` | 1 | qgis |
| self) | `_LocalToUtf` | 1 | freecad |
| self) | `_MakeExpandedObjectReadOnlyInternal` | 1 | freecad |
| self) | `_MakePerTupleExprContext` | 1 | freecad |
| self) | `_MakeSingleTupleTableSlot` | 1 | freecad |
| self) | `_MakeTupleTableSlot` | 1 | freecad |
| self) | `_MarkGUCPrefixReserved` | 1 | freecad |
| self) | `_MemoryContextAlloc` | 1 | freecad |
| self) | `_MemoryContextAllocZero` | 1 | freecad |
| self) | `_MemoryContextDelete` | 1 | freecad |
| self) | `_MemoryContextDeleteChildren` | 1 | freecad |
| self) | `_MemoryContextGetParent` | 1 | freecad |
| self) | `_MemoryContextRegisterResetCallback` | 1 | freecad |
| self) | `_MemoryContextReset` | 1 | freecad |
| self) | `_MemoryContextSetIdentifier` | 1 | freecad |
| self) | `_MemoryContextSetParent` | 1 | freecad |
| self) | `_MemoryContextStrdup` | 1 | freecad |
| self) | `_MyDatabaseId` | 1 | freecad |
| self) | `_MyLatch` | 1 | freecad |
| self) | `_MyProc` | 1 | freecad |
| self) | `_NameListToString` | 1 | freecad |
| self) | `_OBJC_CLASS_$_CBBlueLightClient` | 1 | bettertouchtool |
| self) | `_OBJC_CLASS_$_SKReceiptRefreshRequest` | 1 | termius |
| self) | `_OidOutputFunctionCall` | 1 | freecad |
| self) | `_OraMemAlloc` | 1 | qgis |
| self) | `_OraMemCalloc` | 1 | qgis |
| self) | `_OraMemFree` | 1 | qgis |
| self) | `_OraMemInit` | 1 | qgis |
| self) | `_OraMemTerm` | 1 | qgis |
| self) | `_OraPropGet` | 1 | qgis |
| self) | `_OraPropGetPointer` | 1 | qgis |
| self) | `_OraStreamClone` | 1 | qgis |
| self) | `_OraStreamClose` | 1 | qgis |
| self) | `_OraStreamHandle` | 1 | qgis |
| self) | `_OraStreamInit` | 1 | qgis |
| self) | `_OraStreamIsOpen` | 1 | qgis |
| self) | `_OraStreamOpen` | 1 | qgis |
| self) | `_OraStreamRead` | 1 | qgis |
| self) | `_OraStreamReadChar` | 1 | qgis |
| self) | `_OraStreamSid` | 1 | qgis |
| self) | `_OraStreamTerm` | 1 | qgis |
| self) | `_OraStreamWritable` | 1 | qgis |
| self) | `_OraStreamWrite` | 1 | qgis |
| self) | `_OraStreamWriteChar` | 1 | qgis |
| self) | `_OutputPluginPrepareWrite` | 1 | freecad |
| self) | `_OutputPluginUpdateProgress` | 1 | freecad |
| self) | `_OutputPluginWrite` | 1 | freecad |
| self) | `_PG_exception_stack` | 1 | freecad |
| self) | `_PRINTNAME` | 1 | rstudio |
| self) | `_PinPortal` | 1 | freecad |
| self) | `_PopActiveSnapshot` | 1 | freecad |
| self) | `_ProcessInterrupts` | 1 | freecad |
| self) | `_ProcessWalRcvInterrupts` | 1 | freecad |
| self) | `_PushActiveSnapshot` | 1 | freecad |
| self) | `_PyBuffer_IsContiguous` | 1 | sourcetree |
| self) | `_PyBuffer_Release` | 1 | sourcetree |
| self) | `_PyByteArray_AsString` | 1 | lm-studio |
| self) | `_PyByteArray_Size` | 1 | lm-studio |
| self) | `_PyByteArray_Type` | 1 | lm-studio |
| self) | `_PyBytes_FromFormat` | 1 | lm-studio |
| self) | `_PyBytes_FromString` | 1 | virtualbox |
| self) | `_PyBytes_Type` | 1 | lm-studio |
| self) | `_PyCFunction_GetFlags` | 1 | obs |
| self) | `_PyCFunction_GetFunction` | 1 | obs |
| self) | `_PyCFunction_GetSelf` | 1 | obs |
| self) | `_PyCapsule_GetName` | 1 | lm-studio |
| self) | `_PyCapsule_Import` | 1 | obs |
| self) | `_PyCapsule_IsValid` | 1 | lm-studio |
| self) | `_PyClass_Type` | 1 | pycharm |
| self) | `_PyCode_New` | 1 | pycharm |
| self) | `_PyCode_NewWithPosOnlyArgs` | 1 | lm-studio |
| self) | `_PyDescr_IsData` | 1 | pycharm |
| self) | `_PyDict_Copy` | 1 | pycharm |
| self) | `_PyDict_Keys` | 1 | sourcetree |
| self) | `_PyDict_Merge` | 1 | lm-studio |
| self) | `_PyDict_Update` | 1 | lm-studio |
| self) | `_PyErr_CheckSignals` | 1 | sourcetree |
| self) | `_PyErr_NewException` | 1 | sourcetree |
| self) | `_PyErr_PrintEx` | 1 | pycharm |
| self) | `_PyErr_SetFromErrno` | 1 | sourcetree |
| self) | `_PyErr_SetFromErrnoWithFilename` | 1 | sourcetree |
| self) | `_PyEval_EvalCode` | 1 | virtualbox |
| self) | `_PyEval_EvalCodeEx` | 1 | pycharm |
| self) | `_PyEval_EvalFrameEx` | 1 | pycharm |
| self) | `_PyExc_BaseException` | 1 | pycharm |
| self) | `_PyExc_FutureWarning` | 1 | lm-studio |
| self) | `_PyExc_IOError` | 1 | obs |
| self) | `_PyExc_UnboundLocalError` | 1 | pycharm |
| self) | `_PyExc_UnicodeDecodeError` | 1 | sourcetree |
| self) | `_PyFrame_Type` | 1 | pycharm |
| self) | `_PyImport_ImportModuleLevel` | 1 | lm-studio |
| self) | `_PyInstance_Type` | 1 | pycharm |
| self) | `_PyInt_AsLong` | 1 | sourcetree |
| self) | `_PyInt_AsUnsignedLongLongMask` | 1 | sourcetree |
| self) | `_PyList_Reverse` | 1 | sourcetree |
| self) | `_PyList_SetSlice` | 1 | sourcetree |
| self) | `_PyLong_AsDouble` | 1 | obs |
| self) | `_PyLong_FromVoidPtr` | 1 | obs |
| self) | `_PyMapping_Check` | 1 | sourcetree |
| self) | `_PyMapping_Size` | 1 | sourcetree |
| self) | `_PyMemoryView_FromMemory` | 1 | lm-studio |
| self) | `_PyMemoryView_GetContiguous` | 1 | lm-studio |
| self) | `_PyMemoryView_Type` | 1 | lm-studio |
| self) | `_PyModule_AddStringConstant` | 1 | sourcetree |
| self) | `_PyModule_GetState` | 1 | lm-studio |
| self) | `_PyNumber_Absolute` | 1 | lm-studio |
| self) | `_PyNumber_Add` | 1 | pycharm |
| self) | `_PyNumber_And` | 1 | pycharm |
| self) | `_PyNumber_Float` | 1 | virtualbox |
| self) | `_PyNumber_Invert` | 1 | pycharm |
| self) | `_PyNumber_Long` | 1 | virtualbox |
| self) | `_PyNumber_TrueDivide` | 1 | lm-studio |
| self) | `_PyOS_double_to_string` | 1 | lm-studio |
| self) | `_PyObject_AsCharBuffer` | 1 | sourcetree |
| self) | `_PyObject_AsWriteBuffer` | 1 | virtualbox |
| self) | `_PyObject_CallFunction` | 1 | lm-studio |
| self) | `_PyObject_CheckReadBuffer` | 1 | virtualbox |
| self) | `_PyObject_GenericGetDict` | 1 | lm-studio |
| self) | `_PyObject_GenericSetDict` | 1 | lm-studio |
| self) | `_PyObject_GetBuffer` | 1 | sourcetree |
| self) | `_PyObject_HashNotImplemented` | 1 | lm-studio |
| self) | `_PyObject_Init` | 1 | virtualbox |
| self) | `_PyObject_IsSubclass` | 1 | pycharm |
| self) | `_PyObject_Not` | 1 | pycharm |
| self) | `_PySequence_Fast` | 1 | lm-studio |
| self) | `_PySequence_SetItem` | 1 | lm-studio |
| self) | `_PySet_Contains` | 1 | pycharm |
| self) | `_PySet_Size` | 1 | lm-studio |
| self) | `_PySlice_AdjustIndices` | 1 | lm-studio |
| self) | `_PySlice_GetIndicesEx` | 1 | sourcetree |
| self) | `_PySlice_New` | 1 | lm-studio |
| self) | `_PySlice_Unpack` | 1 | lm-studio |
| self) | `_PyState_FindModule` | 1 | lm-studio |
| self) | `_PyString_AsStringAndSize` | 1 | sourcetree |
| self) | `_PyString_Format` | 1 | pycharm |
| self) | `_PyString_FromFormat` | 1 | pycharm |
| self) | `_PyString_InternFromString` | 1 | pycharm |
| self) | `_PyString_Size` | 1 | sourcetree |
| self) | `_PySys_GetObject` | 1 | virtualbox |
| self) | `_PySys_SetObject` | 1 | virtualbox |
| self) | `_PyType_FromMetaclass` | 1 | pycharm |
| self) | `_PyType_FromModuleAndSpec` | 1 | lm-studio |
| self) | `_PyType_FromSpecWithBases` | 1 | lm-studio |
| self) | `_PyType_GenericAlloc` | 1 | lm-studio |
| self) | `_PyType_GenericNew` | 1 | sourcetree |
| self) | `_PyUnicodeDecodeError_Create` | 1 | sourcetree |
| self) | `_PyUnicodeUCS2_AsEncodedString` | 1 | sourcetree |
| self) | `_PyUnicodeUCS2_AsUTF8String` | 1 | sourcetree |
| self) | `_PyUnicodeUCS2_Compare` | 1 | pycharm |
| self) | `_PyUnicodeUCS2_Concat` | 1 | pycharm |
| self) | `_PyUnicodeUCS2_Decode` | 1 | sourcetree |
| self) | `_PyUnicodeUCS2_DecodeUTF8` | 1 | pycharm |
| self) | `_PyUnicodeUCS2_FromString` | 1 | pycharm |
| self) | `_PyUnicode_AsUTF16String` | 1 | virtualbox |
| self) | `_PyUnicode_AsUTF8` | 1 | pycharm |
| self) | `_PyUnicode_CompareWithASCIIString` | 1 | obs |
| self) | `_PyUnicode_Contains` | 1 | lm-studio |
| self) | `_PyUnicode_Decode` | 1 | pycharm |
| self) | `_PyUnicode_DecodeUTF16` | 1 | virtualbox |
| self) | `_PyUnicode_FindChar` | 1 | lm-studio |
| self) | `_PyUnicode_Format` | 1 | pycharm |
| self) | `_PyUnicode_FromEncodedObject` | 1 | lm-studio |
| self) | `_PyUnicode_FromKindAndData` | 1 | lm-studio |
| self) | `_PyUnicode_FromObject` | 1 | virtualbox |
| self) | `_PyUnicode_GetLength` | 1 | virtualbox |
| self) | `_PyUnicode_InternInPlace` | 1 | lm-studio |
| self) | `_PyUnicode_ReadChar` | 1 | virtualbox |
| self) | `_PyUnicode_Resize` | 1 | lm-studio |
| self) | `_PyUnicode_Split` | 1 | lm-studio |
| self) | `_PyUnicode_Substring` | 1 | lm-studio |
| self) | `_PyUnicode_Tailmatch` | 1 | lm-studio |
| self) | `_PyUnstable_Code_NewWithPosOnlyArgs` | 1 | pycharm |
| self) | `_PyWrapperDescr_Type` | 1 | pycharm |
| self) | `_Py_CompileString` | 1 | virtualbox |
| self) | `_Py_DecRef` | 1 | obs |
| self) | `_Py_GenericAlias` | 1 | lm-studio |
| self) | `_Py_GetArgcArgv` | 1 | sourcetree |
| self) | `_Py_GetProgramFullPath` | 1 | sourcetree |
| self) | `_Py_Initialize` | 1 | virtualbox |
| self) | `_Py_IsInitialized` | 1 | virtualbox |
| self) | `_Py_MakePendingCalls` | 1 | virtualbox |
| self) | `_Py_VaBuildValue` | 1 | virtualbox |
| self) | `_RAW` | 1 | rstudio |
| self) | `_REAL` | 1 | rstudio |
| self) | `_REprintf` | 1 | rstudio |
| self) | `_R_BaseEnv` | 1 | rstudio |
| self) | `_R_BaseNamespace` | 1 | rstudio |
| self) | `_R_BindingIsActive` | 1 | rstudio |
| self) | `_R_BlankString` | 1 | rstudio |
| self) | `_R_BraceSymbol` | 1 | rstudio |
| self) | `_R_CHAR` | 1 | rstudio |
| self) | `_R_CheckDeviceAvailable` | 1 | rstudio |
| self) | `_R_CheckUserInterrupt` | 1 | rstudio |
| self) | `_R_ClearExternalPtr` | 1 | rstudio |
| self) | `_R_Consolefile` | 1 | rstudio |
| self) | `_R_DefParams` | 1 | rstudio |
| self) | `_R_DirtyImage` | 1 | rstudio |
| self) | `_R_EmptyEnv` | 1 | rstudio |
| self) | `_R_ExpandFileName` | 1 | rstudio |
| self) | `_R_ExternalPtrAddr` | 1 | rstudio |
| self) | `_R_ExternalPtrProtected` | 1 | rstudio |
| self) | `_R_ExternalPtrTag` | 1 | rstudio |
| self) | `_R_GE_getVersion` | 1 | rstudio |
| self) | `_R_GetCurrentSrcref` | 1 | rstudio |
| self) | `_R_GlobalEnv` | 1 | rstudio |
| self) | `_R_HomeDir` | 1 | rstudio |
| self) | `_R_InputHandlers` | 1 | rstudio |
| self) | `_R_Interactive` | 1 | rstudio |
| self) | `_R_IsNA` | 1 | rstudio |
| self) | `_R_IsNamespaceEnv` | 1 | rstudio |
| self) | `_R_IsPackageEnv` | 1 | rstudio |
| self) | `_R_LastvalueSymbol` | 1 | rstudio |
| self) | `_R_MakeExternalPtr` | 1 | rstudio |
| self) | `_R_MakeWeakRefC` | 1 | rstudio |
| self) | `_R_MissingArg` | 1 | rstudio |
| self) | `_R_NaInt` | 1 | rstudio |
| self) | `_R_NaReal` | 1 | rstudio |
| self) | `_R_NaString` | 1 | rstudio |
| self) | `_R_NamesSymbol` | 1 | rstudio |
| self) | `_R_NamespaceEnvSpec` | 1 | rstudio |
| self) | `_R_NegInf` | 1 | rstudio |
| self) | `_R_NilValue` | 1 | rstudio |
| self) | `_R_Outputfile` | 1 | rstudio |
| self) | `_R_PackageEnvName` | 1 | rstudio |
| self) | `_R_ParseVector` | 1 | rstudio |
| self) | `_R_PolledEvents` | 1 | rstudio |
| self) | `_R_PosInf` | 1 | rstudio |
| self) | `_R_PreserveObject` | 1 | rstudio |
| self) | `_R_ProcessEvents` | 1 | rstudio |
| self) | `_R_RegisterCFinalizer` | 1 | rstudio |
| self) | `_R_ReleaseObject` | 1 | rstudio |
| self) | `_R_SaveGlobalEnvToFile` | 1 | rstudio |
| self) | `_R_SetParams` | 1 | rstudio |
| self) | `_R_SignalHandlers` | 1 | rstudio |
| self) | `_R_Suicide` | 1 | rstudio |
| self) | `_R_TempDir` | 1 | rstudio |
| self) | `_R_ToplevelExec` | 1 | rstudio |
| self) | `_R_checkActivity` | 1 | rstudio |
| self) | `_R_compute_identical` | 1 | rstudio |
| self) | `_R_curErrorBuf` | 1 | rstudio |
| self) | `_R_dot_Last` | 1 | rstudio |
| self) | `_R_getEmbeddingDllInfo` | 1 | rstudio |
| self) | `_R_interrupts_pending` | 1 | rstudio |
| self) | `_R_interrupts_suspended` | 1 | rstudio |
| self) | `_R_isnancpp` | 1 | rstudio |
| self) | `_R_lsInternal3` | 1 | rstudio |
| self) | `_R_registerRoutines` | 1 | rstudio |
| self) | `_R_runHandlers` | 1 | rstudio |
| self) | `_R_running_as_main_program` | 1 | rstudio |
| self) | `_R_tryEval` | 1 | rstudio |
| self) | `_R_wait_usec` | 1 | rstudio |
| self) | `_RangeVarGetRelidExtended` | 1 | freecad |
| self) | `_ReThrowError` | 1 | freecad |
| self) | `_RegisterSubXactCallback` | 1 | freecad |
| self) | `_RegisterXactCallback` | 1 | freecad |
| self) | `_RelationClose` | 1 | freecad |
| self) | `_RelationIdGetRelation` | 1 | freecad |
| self) | `_ReleaseAllPlanCacheRefsInOwner` | 1 | freecad |
| self) | `_ReleaseCachedPlan` | 1 | freecad |
| self) | `_ReleaseCurrentSubTransaction` | 1 | freecad |
| self) | `_ReleaseSysCache` | 1 | freecad |
| self) | `_RelnameGetRelid` | 1 | freecad |
| self) | `_ResetLatch` | 1 | freecad |
| self) | `_ResourceOwnerCreate` | 1 | freecad |
| self) | `_ResourceOwnerDelete` | 1 | freecad |
| self) | `_Rf_NoDevices` | 1 | rstudio |
| self) | `_Rf_PrintValue` | 1 | rstudio |
| self) | `_Rf_ScalarInteger` | 1 | rstudio |
| self) | `_Rf_ScalarLogical` | 1 | rstudio |
| self) | `_Rf_allocList` | 1 | rstudio |
| self) | `_Rf_allocVector` | 1 | rstudio |
| self) | `_Rf_asChar` | 1 | rstudio |
| self) | `_Rf_asInteger` | 1 | rstudio |
| self) | `_Rf_asLogical` | 1 | rstudio |
| self) | `_Rf_asReal` | 1 | rstudio |
| self) | `_Rf_coerceVector` | 1 | rstudio |
| self) | `_Rf_curDevice` | 1 | rstudio |
| self) | `_Rf_desc2GEDesc` | 1 | rstudio |
| self) | `_Rf_error` | 1 | rstudio |
| self) | `_Rf_errorcall` | 1 | rstudio |
| self) | `_Rf_eval` | 1 | rstudio |
| self) | `_Rf_getAttrib` | 1 | rstudio |
| self) | `_Rf_getCharCE` | 1 | rstudio |
| self) | `_Rf_inherits` | 1 | rstudio |
| self) | `_Rf_initialize_R` | 1 | rstudio |
| self) | `_Rf_install` | 1 | rstudio |
| self) | `_Rf_isFunction` | 1 | rstudio |
| self) | `_Rf_isInteger` | 1 | rstudio |
| self) | `_Rf_isLanguage` | 1 | rstudio |
| self) | `_Rf_isMatrix` | 1 | rstudio |
| self) | `_Rf_isNull` | 1 | rstudio |
| self) | `_Rf_isNumeric` | 1 | rstudio |
| self) | `_Rf_isPrimitive` | 1 | rstudio |
| self) | `_Rf_isReal` | 1 | rstudio |
| self) | `_Rf_isString` | 1 | rstudio |
| self) | `_Rf_killDevice` | 1 | rstudio |
| self) | `_Rf_lang2` | 1 | rstudio |
| self) | `_Rf_lang3` | 1 | rstudio |
| self) | `_Rf_lang4` | 1 | rstudio |
| self) | `_Rf_lcons` | 1 | rstudio |
| self) | `_Rf_length` | 1 | rstudio |
| self) | `_Rf_list4` | 1 | rstudio |
| self) | `_Rf_mkChar` | 1 | rstudio |
| self) | `_Rf_mkCharLenCE` | 1 | rstudio |
| self) | `_Rf_mkString` | 1 | rstudio |
| self) | `_Rf_ndevNumber` | 1 | rstudio |
| self) | `_Rf_nextDevice` | 1 | rstudio |
| self) | `_Rf_onintr` | 1 | rstudio |
| self) | `_Rf_protect` | 1 | rstudio |
| self) | `_Rf_selectDevice` | 1 | rstudio |
| self) | `_Rf_setAttrib` | 1 | rstudio |
| self) | `_Rf_translateChar` | 1 | rstudio |
| self) | `_Rf_translateCharUTF8` | 1 | rstudio |
| self) | `_Rf_type2char` | 1 | rstudio |
| self) | `_Rf_unprotect` | 1 | rstudio |
| self) | `_Rf_warning` | 1 | rstudio |
| self) | `_Rf_warningcall` | 1 | rstudio |
| self) | `_Rf_xlength` | 1 | rstudio |
| self) | `_Riconv` | 1 | rstudio |
| self) | `_Riconv_close` | 1 | rstudio |
| self) | `_Riconv_open` | 1 | rstudio |
| self) | `_RollbackAndReleaseCurrentSubTransaction` | 1 | freecad |
| self) | `_Rprintf` | 1 | rstudio |
| self) | `_SETCAR` | 1 | rstudio |
| self) | `_SETCDR` | 1 | rstudio |
| self) | `_SET_STRING_ELT` | 1 | rstudio |
| self) | `_SET_TAG` | 1 | rstudio |
| self) | `_SET_VECTOR_ELT` | 1 | rstudio |
| self) | `_SLPSPostEventRecordTo` | 1 | bettertouchtool |
| self) | `_SLSCopyManagedDisplaySpaces` | 1 | hammerspoon |
| self) | `_SLSCopySpacesForWindows` | 1 | hammerspoon |
| self) | `_SLSCopyWindowsWithOptionsAndTags` | 1 | hammerspoon |
| self) | `_SLSGetActiveSpace` | 1 | hammerspoon |
| self) | `_SLSMoveWindowsToManagedSpace` | 1 | hammerspoon |
| self) | `_SLSShowSpaces` | 1 | bettertouchtool |
| self) | `_SLSSpaceAddWindowsAndRemoveFromSpaces` | 1 | bettertouchtool |
| self) | `_SLSSpaceCreate` | 1 | bettertouchtool |
| self) | `_SLSSpaceGetType` | 1 | hammerspoon |
| self) | `_SLSSpaceSetAbsoluteLevel` | 1 | bettertouchtool |
| self) | `_SPI_commit` | 1 | freecad |
| self) | `_SPI_commit_and_chain` | 1 | freecad |
| self) | `_SPI_connect` | 1 | freecad |
| self) | `_SPI_connect_ext` | 1 | freecad |
| self) | `_SPI_copytuple` | 1 | freecad |
| self) | `_SPI_cursor_close` | 1 | freecad |
| self) | `_SPI_cursor_fetch` | 1 | freecad |
| self) | `_SPI_cursor_find` | 1 | freecad |
| self) | `_SPI_cursor_open_with_paramlist` | 1 | freecad |
| self) | `_SPI_cursor_parse_open` | 1 | freecad |
| self) | `_SPI_datumTransfer` | 1 | freecad |
| self) | `_SPI_execute_extended` | 1 | freecad |
| self) | `_SPI_execute_plan_extended` | 1 | freecad |
| self) | `_SPI_execute_plan_with_paramlist` | 1 | freecad |
| self) | `_SPI_finish` | 1 | freecad |
| self) | `_SPI_freeplan` | 1 | freecad |
| self) | `_SPI_freetuptable` | 1 | freecad |
| self) | `_SPI_getbinval` | 1 | freecad |
| self) | `_SPI_keepplan` | 1 | freecad |
| self) | `_SPI_palloc` | 1 | freecad |
| self) | `_SPI_plan_get_cached_plan` | 1 | freecad |
| self) | `_SPI_plan_get_plan_sources` | 1 | freecad |
| self) | `_SPI_prepare_extended` | 1 | freecad |
| self) | `_SPI_processed` | 1 | freecad |
| self) | `_SPI_register_trigger_data` | 1 | freecad |
| self) | `_SPI_result` | 1 | freecad |
| self) | `_SPI_result_code_string` | 1 | freecad |
| self) | `_SPI_returntuple` | 1 | freecad |
| self) | `_SPI_rollback` | 1 | freecad |
| self) | `_SPI_rollback_and_chain` | 1 | freecad |
| self) | `_SPI_scroll_cursor_fetch` | 1 | freecad |
| self) | `_SPI_scroll_cursor_move` | 1 | freecad |
| self) | `_SPI_tuptable` | 1 | freecad |
| self) | `_STRING_ELT` | 1 | rstudio |
| self) | `_ScanKeywordLookup` | 1 | freecad |
| self) | `_SearchSysCache1` | 1 | freecad |
| self) | `_SearchSysCache2` | 1 | freecad |
| self) | `_SearchSysCacheAttName` | 1 | freecad |
| self) | `_SearchSysCacheExists` | 1 | freecad |
| self) | `_SetTuplestoreDestReceiverParams` | 1 | freecad |
| self) | `_SldirClose` | 1 | qgis |
| self) | `_SldirCount` | 1 | qgis |
| self) | `_SldirOpen` | 1 | qgis |
| self) | `_SldirRead` | 1 | qgis |
| self) | `_SlfAccess` | 1 | qgis |
| self) | `_SlfClose` | 1 | qgis |
| self) | `_SlfFclose` | 1 | qgis |
| self) | `_SlfFflush` | 1 | qgis |
| self) | `_SlfFopen` | 1 | qgis |
| self) | `_SlfFopen2` | 1 | qgis |
| self) | `_SlfFptc` | 1 | qgis |
| self) | `_SlfFread` | 1 | qgis |
| self) | `_SlfFseekn` | 1 | qgis |
| self) | `_SlfFsync` | 1 | qgis |
| self) | `_SlfFwrite` | 1 | qgis |
| self) | `_SlfLseek` | 1 | qgis |
| self) | `_SlfLseekn` | 1 | qgis |
| self) | `_SlfOpen` | 1 | qgis |
| self) | `_SlfRead` | 1 | qgis |
| self) | `_SlfStat` | 1 | qgis |
| self) | `_SlfStatn` | 1 | qgis |
| self) | `_SlfWrite` | 1 | qgis |
| self) | `_Slfgfn` | 1 | qgis |
| self) | `_Slfpfn` | 1 | qgis |
| self) | `_Sls16ConstZero` | 1 | qgis |
| self) | `_Sls16Div` | 1 | qgis |
| self) | `_Sls16Mul` | 1 | qgis |
| self) | `_Sls16ToText` | 1 | qgis |
| self) | `_Sls16pAdd` | 1 | qgis |
| self) | `_Sls16pFrUb8` | 1 | qgis |
| self) | `_Sls16pLeft` | 1 | qgis |
| self) | `_Sls16pRight` | 1 | qgis |
| self) | `_Sls8FrTextErr` | 1 | qgis |
| self) | `_SltsPrDestroy` | 1 | qgis |
| self) | `_SltsPrInit` | 1 | qgis |
| self) | `_SltsPrRead` | 1 | qgis |
| self) | `_SltsPrUnlock` | 1 | qgis |
| self) | `_SltsPrWrite` | 1 | qgis |
| self) | `_Slu8FrTextErr` | 1 | qgis |
| self) | `_Slu8FrTextl` | 1 | qgis |
| self) | `_SplitIdentifierString` | 1 | freecad |
| self) | `_SysCacheGetAttr` | 1 | freecad |
| self) | `_SysCacheGetAttrNotNull` | 1 | freecad |
| self) | `_TAG` | 1 | rstudio |
| self) | `_TTSOpsHeapTuple` | 1 | freecad |
| self) | `_TTSOpsVirtual` | 1 | freecad |
| self) | `_TYPEOF` | 1 | rstudio |
| self) | `_TopMemoryContext` | 1 | freecad |
| self) | `_TopTransactionContext` | 1 | freecad |
| self) | `_TopTransactionResourceOwner` | 1 | freecad |
| self) | `_TransferExpandedObject` | 1 | freecad |
| self) | `_TupleDescGetAttInMetadata` | 1 | freecad |
| self) | `_TupleDescInitEntry` | 1 | freecad |
| self) | `_TupleDescInitEntryCollation` | 1 | freecad |
| self) | `_UnpinPortal` | 1 | freecad |
| self) | `_UtfToLocal` | 1 | freecad |
| self) | `_VECTOR_ELT` | 1 | rstudio |
| self) | `_WacomMTGetAttachedDeviceIDs` | 1 | bettertouchtool |
| self) | `_WacomMTInitialize` | 1 | bettertouchtool |
| self) | `_WacomMTRegisterAttachCallback` | 1 | bettertouchtool |
| self) | `_WacomMTRegisterFingerReadCallback` | 1 | bettertouchtool |
| self) | `_WaitLatchOrSocket` | 1 | freecad |
| self) | `_WalReceiverFunctions` | 1 | freecad |
| self) | `_XLENGTH` | 1 | rstudio |
| self) | `__PyArg_ParseTupleAndKeywords_SizeT` | 1 | sourcetree |
| self) | `__PyArg_ParseTuple_SizeT` | 1 | sourcetree |
| self) | `__PyByteArray_empty_string` | 1 | lm-studio |
| self) | `__PyCFunction_FastCallKeywords` | 1 | pycharm |
| self) | `__PyCode_GetExtra` | 1 | pycharm |
| self) | `__PyCode_SetExtra` | 1 | pycharm |
| self) | `__PyDict_SetItem_KnownHash` | 1 | pycharm |
| self) | `__PyEval_EvalFrameDefault` | 1 | pycharm |
| self) | `__PyEval_RequestCodeExtraIndex` | 1 | pycharm |
| self) | `__PyLong_Copy` | 1 | lm-studio |
| self) | `__PyMethodDescr_FastCallKeywords` | 1 | pycharm |
| self) | `__PyObject_CallFunction_SizeT` | 1 | sourcetree |
| self) | `__PyObject_CallMethod_SizeT` | 1 | sourcetree |
| self) | `__PyObject_MakeTpCall` | 1 | pycharm |
| self) | `__PyRuntime` | 1 | pycharm |
| self) | `__PySet_NextEntry` | 1 | lm-studio |
| self) | `__PyString_Eq` | 1 | pycharm |
| self) | `__PyString_Join` | 1 | pycharm |
| self) | `__PyString_Resize` | 1 | sourcetree |
| self) | `__PyThreadState_Current` | 1 | pycharm |
| self) | `__Py_BuildValue_SizeT` | 1 | sourcetree |
| self) | `__Py_CheckFunctionResult` | 1 | pycharm |
| self) | `__Py_CheckRecursionLimit` | 1 | pycharm |
| self) | `__Py_CheckRecursiveCall` | 1 | pycharm |
| self) | `__SLPSSetFrontProcessWithOptions` | 1 | bettertouchtool |
| self) | `__Z10add_friendP9tree_nodeS0_b` | 1 | emacs-app |
| self) | `__Z10build_declj9tree_codeP9tree_nodeS1_` | 1 | emacs-app |
| self) | `__Z10get_typeidP9tree_nodei` | 1 | emacs-app |
| self) | `__Z10start_enumP9tree_nodeS0_S0_S0_bPb` | 1 | emacs-app |
| self) | `__Z11begin_scope10scope_kindP9tree_node` | 1 | emacs-app |
| self) | `__Z11build_throwjP9tree_nodei` | 1 | emacs-app |
| self) | `__Z11clone_cdtorP9tree_nodeb` | 1 | emacs-app |
| self) | `__Z11fancy_abortPKciS0_` | 1 | emacs-app |
| self) | `__Z11fatal_errorjPKcz` | 1 | emacs-app |
| self) | `__Z11finish_enumP9tree_node` | 1 | emacs-app |
| self) | `__Z11ggc_reallocPvm` | 1 | emacs-app |
| self) | `__Z11layout_typeP9tree_node` | 1 | emacs-app |
| self) | `__Z11leave_scopev` | 1 | emacs-app |
| self) | `__Z11linemap_addP9line_maps9lc_reasonjPKcj` | 1 | emacs-app |
| self) | `__Z11lookup_nameP9tree_node` | 1 | emacs-app |
| self) | `__Z11walk_tree_1PP9tree_nodePFS0_S1_PiPvES3_P8hash_setIS0_Lb0E19default_hash_traitsIS0_EEPFS0_S1_S2_S5_S3_SA_E` | 1 | emacs-app |
| self) | `__Z12build_stringjPKc` | 1 | emacs-app |
| self) | `__Z12build_typeidP9tree_nodei` | 1 | emacs-app |
| self) | `__Z12get_first_fnP9tree_node` | 1 | emacs-app |
| self) | `__Z12ggc_set_markPKv` | 1 | emacs-app |
| self) | `__Z12pos_from_bitPP9tree_nodeS1_jS0_` | 1 | emacs-app |
| self) | `__Z12push_bindingP9tree_nodeS0_P16cp_binding_level` | 1 | emacs-app |
| self) | `__Z13build_int_cstP9tree_node8poly_intILj1ExE` | 1 | emacs-app |
| self) | `__Z13build_one_cstP9tree_node` | 1 | emacs-app |
| self) | `__Z13build_x_arrowjP9tree_nodei` | 1 | emacs-app |
| self) | `__Z13cp_type_qualsPK9tree_node` | 1 | emacs-app |
| self) | `__Z13current_scopev` | 1 | emacs-app |
| self) | `__Z13delete_sanityjP9tree_nodeS0_bii` | 1 | emacs-app |
| self) | `__Z13finish_structP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z13make_tree_veci` | 1 | emacs-app |
| self) | `__Z13pop_namespacev` | 1 | emacs-app |
| self) | `__Z13size_int_kind8poly_intILj1ExE14size_type_kind` | 1 | emacs-app |
| self) | `__Z14cp_finish_declP9tree_nodeS0_bS0_iP9cp_decomp` | 1 | emacs-app |
| self) | `__Z14get_identifierPKc` | 1 | emacs-app |
| self) | `__Z14grokdeclaratorPK13cp_declaratorP21cp_decl_specifier_seq12decl_contextiPP9tree_node` | 1 | emacs-app |
| self) | `__Z14make_anon_namev` | 1 | emacs-app |
| self) | `__Z14push_namespaceP9tree_nodeb` | 1 | emacs-app |
| self) | `__Z14xref_basetypesP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z15build_this_parmP9tree_nodeS0_i` | 1 | emacs-app |
| self) | `__Z15build_tree_listP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z15cp_build_c_castjP9tree_nodeS0_i` | 1 | emacs-app |
| self) | `__Z15duplicate_declsP9tree_nodeS0_bb` | 1 | emacs-app |
| self) | `__Z15fix_string_typeP9tree_node` | 1 | emacs-app |
| self) | `__Z15fold_build1_locj9tree_codeP9tree_nodeS1_` | 1 | emacs-app |
| self) | `__Z15fold_build2_locj9tree_codeP9tree_nodeS1_S1_` | 1 | emacs-app |
| self) | `__Z15make_class_type9tree_code` | 1 | emacs-app |
| self) | `__Z16at_class_scope_pv` | 1 | emacs-app |
| self) | `__Z16build_array_typeP9tree_nodeS0_b` | 1 | emacs-app |
| self) | `__Z16build_const_castjP9tree_nodeS0_i` | 1 | emacs-app |
| self) | `__Z16build_enumeratorP9tree_nodeS0_S0_S0_j` | 1 | emacs-app |
| self) | `__Z16build_index_typeP9tree_node` | 1 | emacs-app |
| self) | `__Z16build_memfn_typeP9tree_nodeS0_i16cp_ref_qualifier` | 1 | emacs-app |
| self) | `__Z16build_min_nt_locj9tree_codez` | 1 | emacs-app |
| self) | `__Z16build_offset_refP9tree_nodeS0_bi` | 1 | emacs-app |
| self) | `__Z16build_x_unary_opj9tree_code7cp_exprP9tree_nodei` | 1 | emacs-app |
| self) | `__Z16compare_tree_intPK9tree_nodey` | 1 | emacs-app |
| self) | `__Z16dependent_type_pP9tree_node` | 1 | emacs-app |
| self) | `__Z16finish_call_exprP9tree_nodePP3vecIS0_5va_gc8vl_embedEbbi` | 1 | emacs-app |
| self) | `__Z16fold_convert_locjP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z16is_overloaded_fnP9tree_node` | 1 | emacs-app |
| self) | `__Z16make_tree_vectorv` | 1 | emacs-app |
| self) | `__Z16strip_using_declP9tree_node` | 1 | emacs-app |
| self) | `__Z16suppress_warningP9tree_node8opt_codeb` | 1 | emacs-app |
| self) | `__Z16vector_type_modePK9tree_node` | 1 | emacs-app |
| self) | `__Z17apply_memfn_qualsP9tree_nodei16cp_ref_qualifier` | 1 | emacs-app |
| self) | `__Z17begin_lambda_typeP9tree_node` | 1 | emacs-app |
| self) | `__Z17build_lambda_exprv` | 1 | emacs-app |
| self) | `__Z17build_nt_call_vecP9tree_nodeP3vecIS0_5va_gc8vl_embedE` | 1 | emacs-app |
| self) | `__Z17build_ptrmem_typeP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z17build_static_castjP9tree_nodeS0_i` | 1 | emacs-app |
| self) | `__Z17build_vector_typeP9tree_node8poly_intILj1ExE` | 1 | emacs-app |
| self) | `__Z17build_x_binary_opRK13op_location_t9tree_codeP9tree_nodeS2_S4_S2_S4_PS4_i` | 1 | emacs-app |
| self) | `__Z17c_register_pragmaPKcS0_PFvP10cpp_readerE` | 1 | emacs-app |
| self) | `__Z17dependent_scope_pP9tree_node` | 1 | emacs-app |
| self) | `__Z17end_template_declv` | 1 | emacs-app |
| self) | `__Z17make_conv_op_nameP9tree_node` | 1 | emacs-app |
| self) | `__Z17make_friend_classP9tree_nodeS0_b` | 1 | emacs-app |
| self) | `__Z17name_unnamed_typeP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z17push_to_top_levelv` | 1 | emacs-app |
| self) | `__Z17set_global_friendP9tree_node` | 1 | emacs-app |
| self) | `__Z18build_complex_typeP9tree_nodeb` | 1 | emacs-app |
| self) | `__Z18build_dynamic_castjP9tree_nodeS0_i` | 1 | emacs-app |
| self) | `__Z18build_int_cst_typeP9tree_node8poly_intILj1ExE` | 1 | emacs-app |
| self) | `__Z18build_pointer_typeP9tree_node` | 1 | emacs-app |
| self) | `__Z18cp_build_parm_declP9tree_nodeS0_S0_` | 1 | emacs-app |
| self) | `__Z18do_namespace_aliasP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z18linemap_line_startP9line_mapsjj` | 1 | emacs-app |
| self) | `__Z18make_decltype_autov` | 1 | emacs-app |
| self) | `__Z18make_typename_typeP9tree_nodeS0_9tag_typesi` | 1 | emacs-app |
| self) | `__Z18pop_from_top_levelv` | 1 | emacs-app |
| self) | `__Z18push_template_declP9tree_nodeb` | 1 | emacs-app |
| self) | `__Z19at_function_scope_pv` | 1 | emacs-app |
| self) | `__Z19build_lambda_objectP9tree_node` | 1 | emacs-app |
| self) | `__Z19build_lang_decl_locj9tree_codeP9tree_nodeS1_` | 1 | emacs-app |
| self) | `__Z19compute_record_modeP9tree_node` | 1 | emacs-app |
| self) | `__Z19decl_assembler_nameP9tree_node` | 1 | emacs-app |
| self) | `__Z19do_class_using_declP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z19make_pack_expansionP9tree_nodei` | 1 | emacs-app |
| self) | `__Z19release_tree_vectorP3vecIP9tree_node5va_gc8vl_embedE` | 1 | emacs-app |
| self) | `__Z19toplevel_bindings_pv` | 1 | emacs-app |
| self) | `__Z19uses_template_parmsP9tree_node` | 1 | emacs-app |
| self) | `__Z20at_namespace_scope_pv` | 1 | emacs-app |
| self) | `__Z20build_qualified_nameP9tree_nodeS0_S0_b` | 1 | emacs-app |
| self) | `__Z20build_qualified_typeP9tree_nodei` | 1 | emacs-app |
| self) | `__Z20determine_visibilityP9tree_node` | 1 | emacs-app |
| self) | `__Z20finish_noexcept_exprP9tree_nodei` | 1 | emacs-app |
| self) | `__Z20finish_static_assertP9tree_nodeS0_jbb` | 1 | emacs-app |
| self) | `__Z20finish_template_typeP9tree_nodeS0_i` | 1 | emacs-app |
| self) | `__Z20ggc_round_alloc_sizem` | 1 | emacs-app |
| self) | `__Z21build_functional_castjP9tree_nodeS0_i` | 1 | emacs-app |
| self) | `__Z21build_new_method_callP9tree_nodeS0_PP3vecIS0_5va_gc8vl_embedES0_iPS0_i` | 1 | emacs-app |
| self) | `__Z21cp_build_indirect_refjP9tree_node12ref_operatori` | 1 | emacs-app |
| self) | `__Z21finish_base_specifierP9tree_nodeS0_b` | 1 | emacs-app |
| self) | `__Z21lookup_qualified_nameP9tree_nodeS0_9LOOK_wantb` | 1 | emacs-app |
| self) | `__Z21perform_koenig_lookup7cp_exprP3vecIP9tree_node5va_gc8vl_embedEi` | 1 | emacs-app |
| self) | `__Z21process_template_parmP9tree_nodejS0_bb` | 1 | emacs-app |
| self) | `__Z21template_parm_scope_pv` | 1 | emacs-app |
| self) | `__Z22begin_class_definitionP9tree_node` | 1 | emacs-app |
| self) | `__Z22build_array_type_neltsP9tree_node8poly_intILj1EyE` | 1 | emacs-app |
| self) | `__Z22build_cplus_array_typeP9tree_nodeS0_i` | 1 | emacs-app |
| self) | `__Z22build_reinterpret_castjP9tree_nodeS0_i` | 1 | emacs-app |
| self) | `__Z22c_common_type_for_sizeji` | 1 | emacs-app |
| self) | `__Z22cp_literal_operator_idPKc` | 1 | emacs-app |
| self) | `__Z22current_decl_namespacev` | 1 | emacs-app |
| self) | `__Z22end_template_parm_listP9tree_node` | 1 | emacs-app |
| self) | `__Z22finish_bitfield_layoutP9tree_node` | 1 | emacs-app |
| self) | `__Z22finish_enum_value_listP9tree_node` | 1 | emacs-app |
| self) | `__Z22finish_using_directiveP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z22maybe_retrofit_in_chrgP9tree_node` | 1 | emacs-app |
| self) | `__Z23add_exception_specifierP9tree_nodeS0_i` | 1 | emacs-app |
| self) | `__Z23build_exception_variantP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z23cp_build_reference_typeP9tree_nodeb` | 1 | emacs-app |
| self) | `__Z23finish_compound_literalP9tree_nodeS0_i5fcl_t` | 1 | emacs-app |
| self) | `__Z23identifier_global_valueP9tree_node` | 1 | emacs-app |
| self) | `__Z24begin_template_parm_listv` | 1 | emacs-app |
| self) | `__Z24build_x_conditional_exprjP9tree_nodeS0_S0_i` | 1 | emacs-app |
| self) | `__Z24compute_array_index_typeP9tree_nodeS0_i` | 1 | emacs-app |
| self) | `__Z24lhd_print_error_functionP18diagnostic_contextPKcPK15diagnostic_info` | 1 | emacs-app |
| self) | `__Z24lookup_template_functionP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z24rest_of_decl_compilationP9tree_nodeii` | 1 | emacs-app |
| self) | `__Z25build_function_type_arrayP9tree_nodeiPS0_` | 1 | emacs-app |
| self) | `__Z25finish_member_declarationP9tree_node` | 1 | emacs-app |
| self) | `__Z25finish_template_type_parmP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z25skip_artificial_parms_forPK9tree_nodePS_` | 1 | emacs-app |
| self) | `__Z26cxx_sizeof_or_alignof_exprjP9tree_node9tree_codebb` | 1 | emacs-app |
| self) | `__Z26cxx_sizeof_or_alignof_typejP9tree_node9tree_codebb` | 1 | emacs-app |
| self) | `__Z26get_identifier_with_lengthPKcm` | 1 | emacs-app |
| self) | `__Z26ggc_internal_cleared_allocmPFvPvEmm` | 1 | emacs-app |
| self) | `__Z27finish_member_template_declP9tree_node` | 1 | emacs-app |
| self) | `__Z27finish_nonmember_using_declP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z27make_unbound_class_templateP9tree_nodeS0_S0_i` | 1 | emacs-app |
| self) | `__Z27type_dependent_expression_pP9tree_node` | 1 | emacs-app |
| self) | `__Z28value_dependent_expression_pP9tree_node` | 1 | emacs-app |
| self) | `__Z29c_build_bitfield_integer_typeyi` | 1 | emacs-app |
| self) | `__Z29finish_template_template_parmP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z29hash_table_higher_prime_indexm` | 1 | emacs-app |
| self) | `__Z30any_type_dependent_arguments_pPK3vecIP9tree_node5va_gc8vl_embedE` | 1 | emacs-app |
| self) | `__Z30grok_special_member_propertiesP9tree_node` | 1 | emacs-app |
| self) | `__Z31build_offset_ref_call_from_treeP9tree_nodePP3vecIS0_5va_gc8vl_embedEi` | 1 | emacs-app |
| self) | `__Z31finish_class_member_access_expr7cp_exprP9tree_nodebi` | 1 | emacs-app |
| self) | `__Z33build_varargs_function_type_arrayP9tree_nodeiPS0_` | 1 | emacs-app |
| self) | `__Z5errorPKcz` | 1 | emacs-app |
| self) | `__Z6c_bindjP9tree_nodeb` | 1 | emacs-app |
| self) | `__Z6tsubstP9tree_nodeS0_iS0_` | 1 | emacs-app |
| self) | `__Z7pushtagP9tree_nodeS0_7TAG_how` | 1 | emacs-app |
| self) | `__Z8ggc_freePv` | 1 | emacs-app |
| self) | `__Z8nreverseP9tree_node` | 1 | emacs-app |
| self) | `__Z8popclassv` | 1 | emacs-app |
| self) | `__Z8pushdeclP9tree_node` | 1 | emacs-app |
| self) | `__Z8pushdeclP9tree_nodeb` | 1 | emacs-app |
| self) | `__Z9build_newjPP3vecIP9tree_node5va_gc8vl_embedES1_S1_S6_ii` | 1 | emacs-app |
| self) | `__Z9c_pushtagjP9tree_nodeS0_` | 1 | emacs-app |
| self) | `__Z9comptypesP9tree_nodeS0_i` | 1 | emacs-app |
| self) | `__Z9make_node9tree_code` | 1 | emacs-app |
| self) | `__Z9pushclassP9tree_node` | 1 | emacs-app |
| self) | `__Z9tree_consP9tree_nodeS0_S0_` | 1 | emacs-app |
| self) | `__Z9tree_lastP9tree_node` | 1 | emacs-app |
| self) | `__ZN10vec_prefix22calculate_allocation_1Ejj` | 1 | emacs-app |
| self) | `__ZN13ObjCObjectRefI18ObjCObjectTypeWeakEC1EP8NSObjectRKN10ObjCObject8retain_tE` | 1 | microsoft-office |
| self) | `__ZN13ObjCObjectRefI18ObjCObjectTypeWeakED1Ev` | 1 | microsoft-office |
| self) | `__ZN13ObjCObjectRefI20ObjCObjectTypeStrongEC1Ev` | 1 | microsoft-office |
| self) | `__ZN13ObjCObjectRefI20ObjCObjectTypeStrongED1Ev` | 1 | microsoft-office |
| self) | `__ZN13ObjCObjectRefI20ObjCObjectTypeStrongEaSEP8NSObject` | 1 | microsoft-office |
| self) | `__ZN2Dr23MaybeConstGroupIteratorIKNS_14DrawingElementEKNS_19GroupDrawingElementEE5FNextERN3Ofc13TReferringPtrIS2_EE` | 1 | microsoft-office |
| self) | `__ZN2Dr23MaybeConstGroupIteratorIKNS_14DrawingElementEKNS_19GroupDrawingElementEE5ResetEv` | 1 | microsoft-office |
| self) | `__ZN2Dr23MaybeConstGroupIteratorIKNS_14DrawingElementEKNS_19GroupDrawingElementEEC1ERKN3Ofc8TWeakPtrIS2_EERKNS7_IS4_EE` | 1 | microsoft-office |
| self) | `__ZN2Dr23MaybeConstGroupIteratorIKNS_14DrawingElementEKNS_19GroupDrawingElementEEC1ERKN3Ofc8TWeakPtrIS4_EE` | 1 | microsoft-office |
| self) | `__ZN2Dr23MaybeConstGroupIteratorINS_14DrawingElementENS_19GroupDrawingElementEE5FNextERN3Ofc13TReferringPtrIS1_EE` | 1 | microsoft-office |
| self) | `__ZN2Dr23MaybeConstGroupIteratorINS_14DrawingElementENS_19GroupDrawingElementEE5ResetEv` | 1 | microsoft-office |
| self) | `__ZN2Dr23MaybeConstGroupIteratorINS_14DrawingElementENS_19GroupDrawingElementEEC1ERKN3Ofc8TWeakPtrIS1_EERKNS5_IS2_EE` | 1 | microsoft-office |
| self) | `__ZN2Dr23MaybeConstGroupIteratorINS_14DrawingElementENS_19GroupDrawingElementEEC1ERKN3Ofc8TWeakPtrIS2_EE` | 1 | microsoft-office |
| self) | `__ZN2Dr23MaybeConstGroupIteratorINS_14DrawingElementENS_19GroupDrawingElementEEC2ERKN3Ofc8TWeakPtrIS2_EE` | 1 | microsoft-office |
| self) | `__ZN2Dr27MaybeConstPrevGroupIteratorINS_14DrawingElementENS_19GroupDrawingElementEE5FPrevERN3Ofc13TReferringPtrIS1_EE` | 1 | microsoft-office |
| self) | `__ZN2Dr27MaybeConstPrevGroupIteratorINS_14DrawingElementENS_19GroupDrawingElementEEC1ERKN3Ofc13TReferringPtrIS1_EERKNS4_8TWeakPtrIS2_EE` | 1 | microsoft-office |
| self) | `__ZN2v816FunctionTemplate3NewEPNS_7IsolateEPFvRKNS_20FunctionCallbackInfoINS_5ValueEEEENS_5LocalIS4_EENSA_INS_9SignatureEEEiNS_19ConstructorBehaviorENS_14SideEffectTypeEPKNS_9CFunctionE` | 1 | arduino-ide |
| self) | `__ZN2v820EscapableHandleScope6EscapeEPm` | 1 | arduino-ide |
| self) | `__ZN2v820EscapableHandleScopeC1EPNS_7IsolateE` | 1 | arduino-ide |
| self) | `__ZN2v82V812ToLocalEmptyEv` | 1 | arduino-ide |
| self) | `__ZN2v82V813DisposeGlobalEPm` | 1 | arduino-ide |
| self) | `__ZN2v82V818GlobalizeReferenceEPNS_8internal7IsolateEPm` | 1 | arduino-ide |
| self) | `__ZN2v82V88MakeWeakEPmPvPFvRKNS_16WeakCallbackInfoIvEEENS_16WeakCallbackTypeE` | 1 | arduino-ide |
| self) | `__ZN2v82V89ClearWeakEPm` | 1 | arduino-ide |
| self) | `__ZN2v86Object16SetInternalFieldEiNS_5LocalINS_5ValueEEE` | 1 | arduino-ide |
| self) | `__ZN2v86Object18InternalFieldCountEv` | 1 | arduino-ide |
| self) | `__ZN2v89Exception5ErrorENS_5LocalINS_6StringEEE` | 1 | arduino-ide |
| self) | `__ZN2v89Exception9TypeErrorENS_5LocalINS_6StringEEE` | 1 | arduino-ide |
| self) | `__ZN2wi11lts_p_largeEPKxjjS1_j` | 1 | emacs-app |
| self) | `__ZN3Art12CoordMethodsIN3Ofc16TRangeRestrictedIxNS_10CoordRangeEEEEaSERKS5_` | 1 | microsoft-office |
| self) | `__ZN3Art12CoordMethodsIN3Ofc16TRangeRestrictedIxNS_10CoordRangeEEEEaSEd` | 1 | microsoft-office |
| self) | `__ZN3Art12CoordMethodsIN3Ofc16TRangeRestrictedIxNS_10CoordRangeEEEEaSEi` | 1 | microsoft-office |
| self) | `__ZN3Art12CoordMethodsIN3Ofc16TRangeRestrictedIxNS_10CoordRangeEEEEaSEx` | 1 | microsoft-office |
| self) | `__ZN3Art12CoordMethodsIN3Ofc16TRangeRestrictedIxNS_10CoordRangeEEEEmIERKS5_` | 1 | microsoft-office |
| self) | `__ZN3Art12CoordMethodsIN3Ofc16TRangeRestrictedIxNS_10CoordRangeEEEEpLERKS5_` | 1 | microsoft-office |
| self) | `__ZN3Art12CoordMethodsIN3Ofc16TRangeRestrictedIxNS_13PosCoordRangeEEEEaSERKNS0_INS2_IxNS_10CoordRangeEEEEE` | 1 | microsoft-office |
| self) | `__ZN3Art12CoordMethodsIN3Ofc16TRangeRestrictedIxNS_13PosCoordRangeEEEEaSERKS5_` | 1 | microsoft-office |
| self) | `__ZN3Art12CoordMethodsIN3Ofc16TRangeRestrictedIxNS_13PosCoordRangeEEEEaSEd` | 1 | microsoft-office |
| self) | `__ZN3Art13EffectBuilderINS_11GelGeometryEE14SetEffectPropsERKNS_11EffectPropsE` | 1 | microsoft-office |
| self) | `__ZN3Art13EffectBuilderINS_11GelGeometryEE9GetOutputEv` | 1 | microsoft-office |
| self) | `__ZN3Art13EffectBuilderINS_11GelGeometryEEC1EN3Gfx15RenderingPolicyEPKN3GEL7IEffectES8_S8_PKNS5_15ITopLevelEffectERKN3Ofc10TSharedPtrIKS1_EERKNSC_7TBitsetIhLj1EEEbb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_11LockingDataEE11SetNoResizeEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_11LockingDataEE11SetNoRotateEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_11LockingDataEE11SetNoSelectEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_11LockingDataEE14SetNoTranslateEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_11LockingDataEE15SetNoEditPointsEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_11LockingDataEE17SetNoChangeAspectEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_11LockingDataEE18SetNoAdjustHandlesEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_11LockingDataEE20SetNoChangeShapeTypeEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_11LockingDataEE21SetNoChangeArrowheadsEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_11LockingDataEEC1Ev` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_11LockingDataEED1Ev` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_16ShapeLockingDataEE10SetNoGroupEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_16ShapeLockingDataEE11SetNoResizeEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_16ShapeLockingDataEE11SetNoRotateEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_16ShapeLockingDataEE11SetNoSelectEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_16ShapeLockingDataEE14SetNoTranslateEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_16ShapeLockingDataEE15SetNoEditPointsEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_16ShapeLockingDataEE17SetNoChangeAspectEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_16ShapeLockingDataEE18SetNoAdjustHandlesEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_16ShapeLockingDataEE20SetNoChangeShapeTypeEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_16ShapeLockingDataEE21SetNoChangeArrowheadsEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_18PictureLockingDataEE10SetNoGroupEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_18PictureLockingDataEE11SetNoResizeEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_18PictureLockingDataEE11SetNoRotateEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_18PictureLockingDataEE11SetNoSelectEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_18PictureLockingDataEE14SetNoTranslateEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_18PictureLockingDataEE15SetNoEditPointsEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_18PictureLockingDataEE17SetNoChangeAspectEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_18PictureLockingDataEE18SetNoAdjustHandlesEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_18PictureLockingDataEE20SetNoChangeShapeTypeEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_18PictureLockingDataEE21SetNoChangeArrowheadsEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_22ContentPartLockingDataEE10SetNoGroupEb` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_22ContentPartLockingDataEEC1ERKS2_` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_22ContentPartLockingDataEEC1Ev` | 1 | microsoft-office |
| self) | `__ZN3Art14LockingMethodsINS_22ContentPartLockingDataEED1Ev` | 1 | microsoft-office |
| self) | `__ZN3Art15TextBodyMutator14SetPropertyBagINS_19TextCharPropertyBagEEEvRN3Ofc12CTransactionERKNS_9TextRangeERKT_b` | 1 | microsoft-office |
| self) | `__ZN3Art15TextBodyMutator14SetPropertyBagINS_19TextParaPropertyBagEEEvRN3Ofc12CTransactionERKNS_9TextRangeERKT_b` | 1 | microsoft-office |
| self) | `__ZN3Art15TextRunIteratorINS_19TextCharPropertyBagEE8MoveNextEv` | 1 | microsoft-office |
| self) | `__ZN3Art15TextRunIteratorINS_19TextCharPropertyBagEEC1ERKNS_9TextRangeERKNS_8TextBodyE` | 1 | microsoft-office |
| self) | `__ZN3Art15TextRunIteratorINS_19TextParaPropertyBagEE8MoveNextEv` | 1 | microsoft-office |
| self) | `__ZN3Art15TextRunIteratorINS_19TextParaPropertyBagEEC1ERKNS_9TextRangeERKNS_8TextBodyE` | 1 | microsoft-office |
| self) | `__ZN3Art16MediaFileMethodsINS_13AudioFileDataEEC1Ev` | 1 | microsoft-office |
| self) | `__ZN3Art16MediaFileMethodsINS_13VideoFileDataEEC1Ev` | 1 | microsoft-office |
| self) | `__ZN3Art16MediaFileMethodsINS_17QuickTimeFileDataEEC1Ev` | 1 | microsoft-office |
| self) | `__ZN3Art17EffectInfoCommandIN3Ofc9TNinchSetINS_18ShadowEffectStructEEENS_31ShadowEffectPropSelectionSubsetEE15FGetEffectStateERKNS_9SelectionEbNS_15PropCmdIterTypeERS4_` | 1 | microsoft-office |
| self) | `__ZN3Art17ShapePropsMethodsINS_15ShapePropsData_ILi0EEEE4SwapERS3_` | 1 | microsoft-office |
| self) | `__ZN3Art17ShapePropsMethodsINS_15ShapePropsData_ILi0EEEE8OnLoadedERN3Ofc13IReaderParamsE` | 1 | microsoft-office |
| self) | `__ZN3Art17ShapePropsMethodsINS_15ShapePropsData_ILi0EEEEC1EOS3_` | 1 | microsoft-office |
| self) | `__ZN3Art17ShapePropsMethodsINS_15ShapePropsData_ILi0EEEEC1ERKS3_` | 1 | microsoft-office |
| self) | `__ZN3Art17ShapePropsMethodsINS_15ShapePropsData_ILi0EEEEC1Ev` | 1 | microsoft-office |
| self) | `__ZN3Art17ShapePropsMethodsINS_15ShapePropsData_ILi0EEEED1Ev` | 1 | microsoft-office |
| self) | `__ZN3Art17ShapePropsMethodsINS_15ShapePropsData_ILi0EEEEaSEOS3_` | 1 | microsoft-office |
| self) | `__ZN3Art17ShapePropsMethodsINS_15ShapePropsData_ILi0EEEEaSERKS3_` | 1 | microsoft-office |
| self) | `__ZN3Art17ShapePropsMethodsINS_25ShapePropsDataTransacted_ILi0EEEEC2Ev` | 1 | microsoft-office |
| self) | `__ZN3Art17ShapePropsMethodsINS_25ShapePropsDataTransacted_ILi0EEEED2Ev` | 1 | microsoft-office |
| self) | `__ZN3Art18TextPropertyBagDuoINS_19TextCharPropertyBagEE9OnPerformERN3Ofc12CTransactionE` | 1 | microsoft-office |
| self) | `__ZN3Art18TextPropertyBagDuoINS_19TextParaPropertyBagEE9OnPerformERN3Ofc12CTransactionE` | 1 | microsoft-office |
| self) | `__ZN3Art27BackgroundFormattingMethodsINS_25BackgroundFormattingData_ILi0EEEEC1Ev` | 1 | microsoft-office |
| self) | `__ZN3Art27BackgroundFormattingMethodsINS_25BackgroundFormattingData_ILi0EEEED1Ev` | 1 | microsoft-office |
| self) | `__ZN3Art5Color8SwitchToINS_3Clr3HslEEERNT_4TypeEv` | 1 | microsoft-office |
| self) | `__ZN3Art5Color8SwitchToINS_3Clr4SRgbEEERNT_4TypeEv` | 1 | microsoft-office |
| self) | `__ZN3Art5Color8SwitchToINS_3Clr5ScRgbEEERNT_4TypeEv` | 1 | microsoft-office |
| self) | `__ZN3Art5Color8SwitchToINS_3Clr6PresetEEERNT_4TypeEv` | 1 | microsoft-office |
| self) | `__ZN3Art5Color8SwitchToINS_3Clr6SchemeEEERNT_4TypeEv` | 1 | microsoft-office |
| self) | `__ZN3Art5Color8SwitchToINS_3Clr6SystemEEERNT_4TypeEv` | 1 | microsoft-office |
| self) | `__ZN5InkML10InkMethodsINS_7InkDataEEC1ERKS2_` | 1 | microsoft-office |
| self) | `__ZN5InkML10InkMethodsINS_7InkDataEED1Ev` | 1 | microsoft-office |
| self) | `__ZN5InkML20InkAnnotationMethodsINS_17InkAnnotationDataEEC1Ev` | 1 | microsoft-office |
| self) | `__ZN6PixmapI14byteplus_pixelILi4EEEC1ERKS2_` | 1 | microsoft-office |
| self) | `__ZN6PixmapI14byteplus_pixelILi4EEEC1Emm` | 1 | microsoft-office |
| self) | `__ZN6cfutil16cft_dynamic_castINS_5CFURLEEET_PKvNS_12RetainPolicyE` | 1 | microsoft-office |
| self) | `__ZN6cfutil16cft_dynamic_castINS_8CFStringEEET_PKvNS_12RetainPolicyE` | 1 | microsoft-office |
| self) | `__ZNK13ObjCObjectRefI18ObjCObjectTypeWeakEcvP8NSObjectEv` | 1 | microsoft-office |
| self) | `__ZNK13ObjCObjectRefI20ObjCObjectTypeStrongEcvP8NSObjectEv` | 1 | microsoft-office |
| self) | `__ZNK2Dr23MaybeConstGroupIteratorIKNS_14DrawingElementEKNS_19GroupDrawingElementEE8FCurrentERN3Ofc13TReferringPtrIS2_EE` | 1 | microsoft-office |
| self) | `__ZNK2Dr23MaybeConstGroupIteratorINS_14DrawingElementENS_19GroupDrawingElementEE8FCurrentERN3Ofc13TReferringPtrIS1_EE` | 1 | microsoft-office |
| self) | `__ZNK3Art12CoordMethodsIN3Ofc16TRangeRestrictedIxNS_10CoordRangeEEEEngEv` | 1 | microsoft-office |
| self) | `__ZNK3Art14LockingMethodsINS_11LockingDataEEeqERKS2_` | 1 | microsoft-office |
| self) | `__ZNK3Art14LockingMethodsINS_22ContentPartLockingDataEEeqERKS2_` | 1 | microsoft-office |
| self) | `__ZNK3Art15TextRunIteratorINS_19TextCharPropertyBagEE14GetPropertyBagERKNS_9TextRangeERS1_` | 1 | microsoft-office |
| self) | `__ZNK3Art15TextRunIteratorINS_19TextCharPropertyBagEE15PropertyBagAtCpEi` | 1 | microsoft-office |
| self) | `__ZNK3Art15TextRunIteratorINS_19TextCharPropertyBagEE22GetPropertyBagResolvedERKNS_9TextRangeERS1_` | 1 | microsoft-office |
| self) | `__ZNK3Art15TextRunIteratorINS_19TextParaPropertyBagEE14GetPropertyBagERKNS_9TextRangeERS1_` | 1 | microsoft-office |
| self) | `__ZNK3Art15TextRunIteratorINS_19TextParaPropertyBagEE15PropertyBagAtCpEi` | 1 | microsoft-office |
| self) | `__ZNK3Art15TextRunIteratorINS_19TextParaPropertyBagEE22GetPropertyBagResolvedERKNS_9TextRangeERS1_` | 1 | microsoft-office |
| self) | `__ZNK3Art16MediaFileMethodsINS_13AudioFileDataEE4SaveERN3Ofc13IWriterParamsEP14IMetroProgressPNS3_9TFixedStrILi2085EEE` | 1 | microsoft-office |
| self) | `__ZNK3Art16MediaFileMethodsINS_13VideoFileDataEE4SaveERN3Ofc13IWriterParamsEP14IMetroProgressPNS3_9TFixedStrILi2085EEE` | 1 | microsoft-office |
| self) | `__ZNK3Art17ShapePropsMethodsINS_15ShapePropsData_ILi0EEEE10BeforeSaveERN3Ofc13IWriterParamsE` | 1 | microsoft-office |
| self) | `__ZNK3Art17ShapePropsMethodsINS_15ShapePropsData_ILi0EEEE9AfterSaveERN3Ofc13IWriterParamsE` | 1 | microsoft-office |
| self) | `__ZNK3Art17ShapePropsMethodsINS_15ShapePropsData_ILi0EEEEeqERKS3_` | 1 | microsoft-office |
| self) | `__ZNK3Art17TextSelectionInfo18GetTextPropertyBagINS_19TextCharPropertyBagEEEvRT_b` | 1 | microsoft-office |
| self) | `__ZNK3Art17TextSelectionInfo18GetTextPropertyBagINS_19TextParaPropertyBagEEEvRT_b` | 1 | microsoft-office |
| self) | `__ZNK3Art30TSchemeGalleryApplyInfoCommandINS_10FontSchemeEE25GetSchemeGalleryApplyDataEv` | 1 | microsoft-office |
| self) | `__ZNK3Art30TSchemeGalleryApplyInfoCommandINS_11StyleMatrixEE25GetSchemeGalleryApplyDataEv` | 1 | microsoft-office |
| self) | `__ZNK3Art30TSchemeGalleryApplyInfoCommandINS_21ColorSchemeAndMappingEE25GetSchemeGalleryApplyDataEv` | 1 | microsoft-office |
| self) | `__ZNK3Art31TSchemeGalleryUpdateInfoCommandINS_10FontSchemeEE26GetSchemeGalleryUpdateDataEv` | 1 | microsoft-office |
| self) | `__ZNK3Art31TSchemeGalleryUpdateInfoCommandINS_11StyleMatrixEE26GetSchemeGalleryUpdateDataEv` | 1 | microsoft-office |
| self) | `__ZNK3Art31TSchemeGalleryUpdateInfoCommandINS_21ColorSchemeAndMappingEE26GetSchemeGalleryUpdateDataEv` | 1 | microsoft-office |
| self) | `__ZNK3Art5Color3GetINS_3Clr3HslEEERKNT_4TypeEv` | 1 | microsoft-office |
| self) | `__ZNK3Art5Color3GetINS_3Clr4SRgbEEERKNT_4TypeEv` | 1 | microsoft-office |
| self) | `__ZNK3Art5Color3GetINS_3Clr5ScRgbEEERKNT_4TypeEv` | 1 | microsoft-office |
| self) | `__ZNK3Art5Color3GetINS_3Clr6PresetEEERKNT_4TypeEv` | 1 | microsoft-office |
| self) | `__ZNK3Art5Color3GetINS_3Clr6SchemeEEERKNT_4TypeEv` | 1 | microsoft-office |
| self) | `__ZNK3Art5Color3GetINS_3Clr6SystemEEERKNT_4TypeEv` | 1 | microsoft-office |
| self) | `__ZNK3Art8TextBody14GetPropertyBagINS_19TextCharPropertyBagEEEvRKNS_9TextRangeERT_PNS_18TextBodyImplCursorE` | 1 | microsoft-office |
| self) | `__ZNK3Art8TextBody14GetPropertyBagINS_19TextParaPropertyBagEEEvRKNS_9TextRangeERT_PNS_18TextBodyImplCursorE` | 1 | microsoft-office |
| self) | `__ZNK3Art8TextBody19GetStylePropertyBagINS_19TextCharPropertyBagEEEvN3Ofc16TRangeRestrictedIiNS_24TextIndentLevelTypeRangeEEERT_` | 1 | microsoft-office |
| self) | `__ZNK3Art8TextBody19GetStylePropertyBagINS_19TextParaPropertyBagEEEvN3Ofc16TRangeRestrictedIiNS_24TextIndentLevelTypeRangeEEERT_` | 1 | microsoft-office |
| self) | `__ZNK6cfutil11CFData_baseIP8__CFDataE4sizeEv` | 1 | microsoft-office |
| self) | `__ZNK6cfutil11CFData_baseIPK8__CFDataE4sizeEv` | 1 | microsoft-office |
| self) | `__ZNK6cfutil12CFArray_baseIPK9__CFArrayE4sizeEv` | 1 | microsoft-office |
| self) | `__ZNK6cfutil12CFArray_baseIPK9__CFArrayEixEl` | 1 | microsoft-office |
| self) | `__ZTIN3GEL16FailureExceptionE` | 1 | microsoft-office |
| self) | `__ZTIN3Gfx14AbortExceptionE` | 1 | microsoft-office |
| self) | `__ZTIN5MsoCF15CErrorExceptionE` | 1 | microsoft-office |
| self) | `_addRTEPermissionInfo` | 1 | freecad |
| self) | `_appendStringInfoStringQuoted` | 1 | freecad |
| self) | `_array_create_iterator` | 1 | freecad |
| self) | `_array_iterate` | 1 | freecad |
| self) | `_bindresvport` | 1 | qgis |
| self) | `_bms_add_member` | 1 | freecad |
| self) | `_bms_equal` | 1 | freecad |
| self) | `_bms_free` | 1 | freecad |
| self) | `_bms_is_member` | 1 | freecad |
| self) | `_bms_next_member` | 1 | freecad |
| self) | `_bms_num_members` | 1 | freecad |
| self) | `_build_attrmap_by_name_if_req` | 1 | freecad |
| self) | `_c_binding_oracle` | 1 | emacs-app |
| self) | `_c_global_trees` | 1 | emacs-app |
| self) | `_cfun` | 1 | emacs-app |
| self) | `_check_encoding_conversion_args` | 1 | freecad |
| self) | `_check_function_bodies` | 1 | freecad |
| self) | `_coerce_to_target_type` | 1 | freecad |
| self) | `_construct_md_array` | 1 | freecad |
| self) | `_contain_mutable_functions` | 1 | freecad |
| self) | `_convert_tuples_by_position` | 1 | freecad |
| self) | `_core_yylex` | 1 | freecad |
| self) | `_cp_binding_oracle` | 1 | emacs-app |
| self) | `_cp_global_trees` | 1 | emacs-app |
| self) | `_csinh` | 1 | kicad |
| self) | `_cstring_to_text` | 1 | freecad |
| self) | `_ctanh` | 1 | kicad |
| self) | `_current_function_decl` | 1 | emacs-app |
| self) | `_datumCopy` | 1 | freecad |
| self) | `_datumTransfer` | 1 | freecad |
| self) | `_deconstruct_expanded_record` | 1 | freecad |
| self) | `_defGetBoolean` | 1 | freecad |
| self) | `_defGetStreamingMode` | 1 | freecad |
| self) | `_defGetString` | 1 | freecad |
| self) | `_detoast_external_attr` | 1 | freecad |
| self) | `_dns_free` | 1 | qgis |
| self) | `_dns_open` | 1 | qgis |
| self) | `_dns_search` | 1 | qgis |
| self) | `_dns_search_list_domain` | 1 | qgis |
| self) | `_domain_check` | 1 | freecad |
| self) | `_err_generic_string` | 1 | freecad |
| self) | `_errcode` | 1 | freecad |
| self) | `_errcontext_msg` | 1 | freecad |
| self) | `_errdetail` | 1 | freecad |
| self) | `_errdetail_internal` | 1 | freecad |
| self) | `_errfinish` | 1 | freecad |
| self) | `_errhint` | 1 | freecad |
| self) | `_errmsg` | 1 | freecad |
| self) | `_errmsg_internal` | 1 | freecad |
| self) | `_errmsg_plural` | 1 | freecad |
| self) | `_error_context_stack` | 1 | freecad |
| self) | `_errposition` | 1 | freecad |
| self) | `_errstart` | 1 | freecad |
| self) | `_errstart_cold` | 1 | freecad |
| self) | `_execute_attr_map_slot` | 1 | freecad |
| self) | `_execute_attr_map_tuple` | 1 | freecad |
| self) | `_expand_array` | 1 | freecad |
| self) | `_expanded_record_fetch_field` | 1 | freecad |
| self) | `_expanded_record_fetch_tupdesc` | 1 | freecad |
| self) | `_expanded_record_get_tuple` | 1 | freecad |
| self) | `_expanded_record_lookup_field` | 1 | freecad |
| self) | `_expanded_record_set_field_internal` | 1 | freecad |
| self) | `_expanded_record_set_fields` | 1 | freecad |
| self) | `_expanded_record_set_tuple` | 1 | freecad |
| self) | `_exprType` | 1 | freecad |
| self) | `_exprTypmod` | 1 | freecad |
| self) | `_find_rendezvous_variable` | 1 | freecad |
| self) | `_fmemopen` | 1 | insomnia |
| self) | `_format_elog_string` | 1 | freecad |
| self) | `_format_procedure` | 1 | freecad |
| self) | `_format_type_be` | 1 | freecad |
| self) | `_free_attrmap` | 1 | freecad |
| self) | `_function_depth` | 1 | emacs-app |
| self) | `_function_parse_error_transpose` | 1 | freecad |
| self) | `_gcry_check_version` | 1 | vagrant |
| self) | `_gcry_cipher_close` | 1 | vagrant |
| self) | `_gcry_cipher_decrypt` | 1 | vagrant |
| self) | `_gcry_cipher_encrypt` | 1 | vagrant |
| self) | `_gcry_cipher_open` | 1 | vagrant |
| self) | `_gcry_cipher_setkey` | 1 | vagrant |
| self) | `_gcry_md_hash_buffer` | 1 | vagrant |
| self) | `_gcry_strerror` | 1 | vagrant |
| self) | `_getTypeOutputInfo` | 1 | freecad |
| self) | `_get_array_type` | 1 | freecad |
| self) | `_get_base_element_type` | 1 | freecad |
| self) | `_get_call_expr_argtype` | 1 | freecad |
| self) | `_get_call_result_type` | 1 | freecad |
| self) | `_get_collation_oid` | 1 | freecad |
| self) | `_get_element_type` | 1 | freecad |
| self) | `_get_fn_expr_rettype` | 1 | freecad |
| self) | `_get_func_arg_info` | 1 | freecad |
| self) | `_get_namespace_name` | 1 | freecad |
| self) | `_get_partition_ancestors` | 1 | freecad |
| self) | `_get_rel_namespace` | 1 | freecad |
| self) | `_get_rel_relispartition` | 1 | freecad |
| self) | `_get_rel_relkind` | 1 | freecad |
| self) | `_get_rel_type_id` | 1 | freecad |
| self) | `_get_typcollation` | 1 | freecad |
| self) | `_get_typlenbyval` | 1 | freecad |
| self) | `_get_typsubscript` | 1 | freecad |
| self) | `_get_typtype` | 1 | freecad |
| self) | `_geterrposition` | 1 | freecad |
| self) | `_gethostent` | 1 | qgis |
| self) | `_gethrtime` | 1 | qgis |
| self) | `_getinternalerrposition` | 1 | freecad |
| self) | `_global_trees` | 1 | emacs-app |
| self) | `_guc_malloc` | 1 | freecad |
| self) | `_hash_create` | 1 | freecad |
| self) | `_hash_destroy` | 1 | freecad |
| self) | `_hash_search` | 1 | freecad |
| self) | `_hash_seq_init` | 1 | freecad |
| self) | `_hash_seq_search` | 1 | freecad |
| self) | `_heap_deform_tuple` | 1 | freecad |
| self) | `_heap_form_tuple` | 1 | freecad |
| self) | `_input_location` | 1 | emacs-app |
| self) | `_integer_types` | 1 | emacs-app |
| self) | `_internalerrposition` | 1 | freecad |
| self) | `_internalerrquery` | 1 | freecad |
| self) | `_is_publishable_relation` | 1 | freecad |
| self) | `_js_add_deferred_teardown_callback` | 1 | anythingllm |
| self) | `_js_call_function` | 1 | anythingllm |
| self) | `_js_close_handle_scope` | 1 | anythingllm |
| self) | `_js_create_array_with_length` | 1 | anythingllm |
| self) | `_js_create_arraybuffer` | 1 | anythingllm |
| self) | `_js_create_error` | 1 | anythingllm |
| self) | `_js_create_function` | 1 | anythingllm |
| self) | `_js_create_int32` | 1 | anythingllm |
| self) | `_js_create_int64` | 1 | anythingllm |
| self) | `_js_create_object` | 1 | anythingllm |
| self) | `_js_create_reference` | 1 | anythingllm |
| self) | `_js_create_string_latin1` | 1 | anythingllm |
| self) | `_js_create_string_utf16le` | 1 | anythingllm |
| self) | `_js_create_string_utf8` | 1 | anythingllm |
| self) | `_js_create_typed_function` | 1 | anythingllm |
| self) | `_js_create_uint32` | 1 | anythingllm |
| self) | `_js_create_unsafe_arraybuffer` | 1 | anythingllm |
| self) | `_js_delete_reference` | 1 | anythingllm |
| self) | `_js_finish_deferred_teardown_callback` | 1 | anythingllm |
| self) | `_js_get_array_elements` | 1 | anythingllm |
| self) | `_js_get_array_length` | 1 | anythingllm |
| self) | `_js_get_arraybuffer_info` | 1 | anythingllm |
| self) | `_js_get_boolean` | 1 | anythingllm |
| self) | `_js_get_callback_info` | 1 | anythingllm |
| self) | `_js_get_env_loop` | 1 | anythingllm |
| self) | `_js_get_env_platform` | 1 | anythingllm |
| self) | `_js_get_null` | 1 | anythingllm |
| self) | `_js_get_platform_limits` | 1 | anythingllm |
| self) | `_js_get_reference_value` | 1 | anythingllm |
| self) | `_js_get_sharedarraybuffer_info` | 1 | anythingllm |
| self) | `_js_get_string_view` | 1 | anythingllm |
| self) | `_js_get_typed_callback_info` | 1 | anythingllm |
| self) | `_js_get_typedarray_info` | 1 | anythingllm |
| self) | `_js_get_value_bool` | 1 | anythingllm |
| self) | `_js_get_value_double` | 1 | anythingllm |
| self) | `_js_get_value_int32` | 1 | anythingllm |
| self) | `_js_get_value_int64` | 1 | anythingllm |
| self) | `_js_get_value_string_latin1` | 1 | anythingllm |
| self) | `_js_get_value_string_utf16le` | 1 | anythingllm |
| self) | `_js_get_value_string_utf8` | 1 | anythingllm |
| self) | `_js_get_value_uint32` | 1 | anythingllm |
| self) | `_js_is_arraybuffer` | 1 | anythingllm |
| self) | `_js_is_boolean` | 1 | anythingllm |
| self) | `_js_is_detached_arraybuffer` | 1 | anythingllm |
| self) | `_js_is_number` | 1 | anythingllm |
| self) | `_js_is_sharedarraybuffer` | 1 | anythingllm |
| self) | `_js_is_string` | 1 | anythingllm |
| self) | `_js_is_typedarray` | 1 | anythingllm |
| self) | `_js_open_handle_scope` | 1 | anythingllm |
| self) | `_js_release_string_view` | 1 | anythingllm |
| self) | `_js_set_element` | 1 | anythingllm |
| self) | `_js_set_named_property` | 1 | anythingllm |
| self) | `_js_throw_error` | 1 | anythingllm |
| self) | `_js_throw_range_error` | 1 | anythingllm |
| self) | `_js_throw_range_errorf` | 1 | anythingllm |
| self) | `_js_throw_type_error` | 1 | anythingllm |
| self) | `_js_throw_type_errorf` | 1 | anythingllm |
| self) | `_js_typeof` | 1 | anythingllm |
| self) | `_kDADiskDescriptionMediaIconKey` | 1 | arduino-ide |
| self) | `_lang_hooks` | 1 | emacs-app |
| self) | `_lappend` | 1 | freecad |
| self) | `_lappend_xid` | 1 | freecad |
| self) | `_larch_query` | 1 | qgis |
| self) | `_latin2mic` | 1 | freecad |
| self) | `_latin2mic_with_table` | 1 | freecad |
| self) | `_lbiv_popcount` | 1 | qgis |
| self) | `_lbivand` | 1 | qgis |
| self) | `_lbivctx_getsize` | 1 | qgis |
| self) | `_lbivctx_init` | 1 | qgis |
| self) | `_lbivctx_term` | 1 | qgis |
| self) | `_lbivffc` | 1 | qgis |
| self) | `_lbivffs` | 1 | qgis |
| self) | `_lbivncpy` | 1 | qgis |
| self) | `_lbivnot` | 1 | qgis |
| self) | `_lbivor` | 1 | qgis |
| self) | `_lcddmy` | 1 | qgis |
| self) | `_lcddrv` | 1 | qgis |
| self) | `_lctbnam` | 1 | qgis |
| self) | `_lctbval` | 1 | qgis |
| self) | `_lcv42b` | 1 | qgis |
| self) | `_lcv42h` | 1 | qgis |
| self) | `_lcvb24` | 1 | qgis |
| self) | `_lcvb2bg` | 1 | qgis |
| self) | `_lcvb2w` | 1 | qgis |
| self) | `_lcvb2werr` | 1 | qgis |
| self) | `_lcvbg2b` | 1 | qgis |
| self) | `_lcvd2e` | 1 | qgis |
| self) | `_lcvw2b` | 1 | qgis |
| self) | `_ldigtzd` | 1 | qgis |
| self) | `_ldimtzd` | 1 | qgis |
| self) | `_ldirtzd` | 1 | qgis |
| self) | `_lditzver` | 1 | qgis |
| self) | `_ldiutzd` | 1 | qgis |
| self) | `_ldsbget_buffer_size` | 1 | qgis |
| self) | `_ldsbget_prec_ondigs` | 1 | qgis |
| self) | `_ldsbget_width` | 1 | qgis |
| self) | `_ldsbinit` | 1 | qgis |
| self) | `_ldsbvbinit` | 1 | qgis |
| self) | `_ldsbvget_ints` | 1 | qgis |
| self) | `_ldsbvget_minmax` | 1 | qgis |
| self) | `_ldsbvget_ons` | 1 | qgis |
| self) | `_ldsbvinit` | 1 | qgis |
| self) | `_ldsbvinitx` | 1 | qgis |
| self) | `_ldsbvrescale` | 1 | qgis |
| self) | `_ldsbvscan_ons` | 1 | qgis |
| self) | `_ldsbvset_ons` | 1 | qgis |
| self) | `_ldxadm` | 1 | qgis |
| self) | `_ldxads` | 1 | qgis |
| self) | `_ldxatz` | 1 | qgis |
| self) | `_ldxbegin` | 1 | qgis |
| self) | `_ldxcbeg` | 1 | qgis |
| self) | `_ldxchk` | 1 | qgis |
| self) | `_ldxcmp` | 1 | qgis |
| self) | `_ldxdtd` | 1 | qgis |
| self) | `_ldxdts` | 1 | qgis |
| self) | `_ldxdtsu` | 1 | qgis |
| self) | `_ldxdyf` | 1 | qgis |
| self) | `_ldxdyn` | 1 | qgis |
| self) | `_ldxend` | 1 | qgis |
| self) | `_ldxeti` | 1 | qgis |
| self) | `_ldxftd` | 1 | qgis |
| self) | `_ldxgtz` | 1 | qgis |
| self) | `_ldxini` | 1 | qgis |
| self) | `_ldxite` | 1 | qgis |
| self) | `_ldxldd` | 1 | qgis |
| self) | `_ldxmdctx` | 1 | qgis |
| self) | `_ldxmxsz` | 1 | qgis |
| self) | `_ldxnbeg` | 1 | qgis |
| self) | `_ldxnxd` | 1 | qgis |
| self) | `_ldxstd` | 1 | qgis |
| self) | `_ldxstd1` | 1 | qgis |
| self) | `_ldxstdu` | 1 | qgis |
| self) | `_ldxsti` | 1 | qgis |
| self) | `_ldxstiu` | 1 | qgis |
| self) | `_ldxsto` | 1 | qgis |
| self) | `_ldxstou` | 1 | qgis |
| self) | `_ldxx_parse_context_init` | 1 | qgis |
| self) | `_ldxx_parse_context_term` | 1 | qgis |
| self) | `_lehpcmp` | 1 | qgis |
| self) | `_lehpdt` | 1 | qgis |
| self) | `_lehpinf` | 1 | qgis |
| self) | `_lehptrf` | 1 | qgis |
| self) | `_lembem` | 1 | qgis |
| self) | `_lemces` | 1 | qgis |
| self) | `_lemdec` | 1 | qgis |
| self) | `_lemfaa` | 1 | qgis |
| self) | `_lemfaf` | 1 | qgis |
| self) | `_lemgec` | 1 | qgis |
| self) | `_lemged` | 1 | qgis |
| self) | `_lemgef` | 1 | qgis |
| self) | `_lemgem` | 1 | qgis |
| self) | `_lemgpd` | 1 | qgis |
| self) | `_lempbar` | 1 | qgis |
| self) | `_lempbas` | 1 | qgis |
| self) | `_lempfrec` | 1 | qgis |
| self) | `_lempfsec` | 1 | qgis |
| self) | `_lempfsic` | 1 | qgis |
| self) | `_lempres2` | 1 | qgis |
| self) | `_lemprse` | 1 | qgis |
| self) | `_lemptfr` | 1 | qgis |
| self) | `_lemptfs` | 1 | qgis |
| self) | `_lfiabt` | 1 | qgis |
| self) | `_lficls` | 1 | qgis |
| self) | `_lfidlb` | 1 | qgis |
| self) | `_lfifex` | 1 | qgis |
| self) | `_lfifini` | 1 | qgis |
| self) | `_lfiflu` | 1 | qgis |
| self) | `_lfifno` | 1 | qgis |
| self) | `_lfifpo` | 1 | qgis |
| self) | `_lfigfn` | 1 | qgis |
| self) | `_lfignam` | 1 | qgis |
| self) | `_lfilen` | 1 | qgis |
| self) | `_lfilenn` | 1 | qgis |
| self) | `_lfilini` | 1 | qgis |
| self) | `_lfimknam` | 1 | qgis |
| self) | `_lfimkpth` | 1 | qgis |
| self) | `_lfiopn` | 1 | qgis |
| self) | `_lfipthad` | 1 | qgis |
| self) | `_lfird` | 1 | qgis |
| self) | `_lfiskb` | 1 | qgis |
| self) | `_lfiskbn` | 1 | qgis |
| self) | `_lfitell` | 1 | qgis |
| self) | `_lfitrim` | 1 | qgis |
| self) | `_lfiwr` | 1 | qgis |
| self) | `_lfpcd2cf` | 1 | qgis |
| self) | `_lfpcd2lnx` | 1 | qgis |
| self) | `_lfpcd2s` | 1 | qgis |
| self) | `_lfpcdcoerce` | 1 | qgis |
| self) | `_lfpcf2cd` | 1 | qgis |
| self) | `_lfpcf2lnx` | 1 | qgis |
| self) | `_lfpcf2s` | 1 | qgis |
| self) | `_lfpcfcoerce` | 1 | qgis |
| self) | `_lfpinit` | 1 | qgis |
| self) | `_lfplnx2cd` | 1 | qgis |
| self) | `_lfplnx2cf` | 1 | qgis |
| self) | `_lfps2cde` | 1 | qgis |
| self) | `_lfps2cfdlnxe` | 1 | qgis |
| self) | `_lfps2cfe` | 1 | qgis |
| self) | `_lfvini2` | 1 | qgis |
| self) | `_lfvinit` | 1 | qgis |
| self) | `_lfvpkgname` | 1 | qgis |
| self) | `_lfvtyp` | 1 | qgis |
| self) | `_line_table` | 1 | emacs-app |
| self) | `_lio_listio` | 1 | qgis |
| self) | `_list_copy` | 1 | freecad |
| self) | `_list_delete_last` | 1 | freecad |
| self) | `_list_delete_nth_cell` | 1 | freecad |
| self) | `_list_free` | 1 | freecad |
| self) | `_list_free_deep` | 1 | freecad |
| self) | `_list_make1_impl` | 1 | freecad |
| self) | `_list_make2_impl` | 1 | freecad |
| self) | `_list_make3_impl` | 1 | freecad |
| self) | `_list_member_oid` | 1 | freecad |
| self) | `_list_member_xid` | 1 | freecad |
| self) | `_lmebco` | 1 | qgis |
| self) | `_lmebucp` | 1 | qgis |
| self) | `_lmlinit` | 1 | qgis |
| self) | `_lmlterm` | 1 | qgis |
| self) | `_lmmcalloc` | 1 | qgis |
| self) | `_lmmfree` | 1 | qgis |
| self) | `_lmmhpfree` | 1 | qgis |
| self) | `_lmmhpinit` | 1 | qgis |
| self) | `_lmmhpsz` | 1 | qgis |
| self) | `_lmmmalloc` | 1 | qgis |
| self) | `_lmmrealloc` | 1 | qgis |
| self) | `_lmmtophp` | 1 | qgis |
| self) | `_lmsacbn` | 1 | qgis |
| self) | `_lmsacin` | 1 | qgis |
| self) | `_lmsagbf` | 1 | qgis |
| self) | `_lmsaicmt` | 1 | qgis |
| self) | `_lmsatrm` | 1 | qgis |
| self) | `_lmtprime` | 1 | qgis |
| self) | `_lmx_check_ser_version_compat` | 1 | qgis |
| self) | `_lmx_get_ser_version` | 1 | qgis |
| self) | `_lmxconpar2` | 1 | qgis |
| self) | `_lnxabs` | 1 | qgis |
| self) | `_lnxadd` | 1 | qgis |
| self) | `_lnxatn2` | 1 | qgis |
| self) | `_lnxban` | 1 | qgis |
| self) | `_lnxbex` | 1 | qgis |
| self) | `_lnxchk` | 1 | qgis |
| self) | `_lnxcmp` | 1 | qgis |
| self) | `_lnxcopy` | 1 | qgis |
| self) | `_lnxcpn` | 1 | qgis |
| self) | `_lnxcpng` | 1 | qgis |
| self) | `_lnxcpngarr` | 1 | qgis |
| self) | `_lnxdec` | 1 | qgis |
| self) | `_lnxdisp2num` | 1 | qgis |
| self) | `_lnxdiv` | 1 | qgis |
| self) | `_lnxfcn` | 1 | qgis |
| self) | `_lnxfcng` | 1 | qgis |
| self) | `_lnxfcnu` | 1 | qgis |
| self) | `_lnxflo` | 1 | qgis |
| self) | `_lnxfpr` | 1 | qgis |
| self) | `_lnxfprarr` | 1 | qgis |
| self) | `_lnxinc` | 1 | qgis |
| self) | `_lnxint` | 1 | qgis |
| self) | `_lnxln` | 1 | qgis |
| self) | `_lnxlog` | 1 | qgis |
| self) | `_lnxmin` | 1 | qgis |
| self) | `_lnxmod` | 1 | qgis |
| self) | `_lnxmul` | 1 | qgis |
| self) | `_lnxn2c` | 1 | qgis |
| self) | `_lnxn2cg` | 1 | qgis |
| self) | `_lnxn2copt` | 1 | qgis |
| self) | `_lnxn2coptg` | 1 | qgis |
| self) | `_lnxn2disp` | 1 | qgis |
| self) | `_lnxneg` | 1 | qgis |
| self) | `_lnxnft` | 1 | qgis |
| self) | `_lnxnftg` | 1 | qgis |
| self) | `_lnxnftu` | 1 | qgis |
| self) | `_lnxnuc` | 1 | qgis |
| self) | `_lnxnucg` | 1 | qgis |
| self) | `_lnxnucopt` | 1 | qgis |
| self) | `_lnxnucoptg` | 1 | qgis |
| self) | `_lnxnur` | 1 | qgis |
| self) | `_lnxpd2n` | 1 | qgis |
| self) | `_lnxpfl` | 1 | qgis |
| self) | `_lnxpflg` | 1 | qgis |
| self) | `_lnxpflu` | 1 | qgis |
| self) | `_lnxpow` | 1 | qgis |
| self) | `_lnxqgtone` | 1 | qgis |
| self) | `_lnxqgtpi` | 1 | qgis |
| self) | `_lnxqgtzero` | 1 | qgis |
| self) | `_lnxqtra` | 1 | qgis |
| self) | `_lnxqtri` | 1 | qgis |
| self) | `_lnxr2n` | 1 | qgis |
| self) | `_lnxren` | 1 | qgis |
| self) | `_lnxrou` | 1 | qgis |
| self) | `_lnxsca` | 1 | qgis |
| self) | `_lnxscn` | 1 | qgis |
| self) | `_lnxsgn` | 1 | qgis |
| self) | `_lnxshift` | 1 | qgis |
| self) | `_lnxsni` | 1 | qgis |
| self) | `_lnxsqr` | 1 | qgis |
| self) | `_lnxsub` | 1 | qgis |
| self) | `_lnxsum` | 1 | qgis |
| self) | `_lnxtru` | 1 | qgis |
| self) | `_lnxzd2n` | 1 | qgis |
| self) | `_local2local` | 1 | freecad |
| self) | `_logicalrep_write_begin` | 1 | freecad |
| self) | `_logicalrep_write_begin_prepare` | 1 | freecad |
| self) | `_logicalrep_write_commit` | 1 | freecad |
| self) | `_logicalrep_write_commit_prepared` | 1 | freecad |
| self) | `_logicalrep_write_delete` | 1 | freecad |
| self) | `_logicalrep_write_insert` | 1 | freecad |
| self) | `_logicalrep_write_message` | 1 | freecad |
| self) | `_logicalrep_write_origin` | 1 | freecad |
| self) | `_logicalrep_write_prepare` | 1 | freecad |
| self) | `_logicalrep_write_rel` | 1 | freecad |
| self) | `_logicalrep_write_rollback_prepared` | 1 | freecad |
| self) | `_logicalrep_write_stream_abort` | 1 | freecad |
| self) | `_logicalrep_write_stream_commit` | 1 | freecad |
| self) | `_logicalrep_write_stream_prepare` | 1 | freecad |
| self) | `_logicalrep_write_stream_start` | 1 | freecad |
| self) | `_logicalrep_write_stream_stop` | 1 | freecad |
| self) | `_logicalrep_write_truncate` | 1 | freecad |
| self) | `_logicalrep_write_typ` | 1 | freecad |
| self) | `_logicalrep_write_update` | 1 | freecad |
| self) | `_lookup_rowtype_tupdesc` | 1 | freecad |
| self) | `_lookup_type_cache` | 1 | freecad |
| self) | `_lowerstr` | 1 | freecad |
| self) | `_lowerstr_with_len` | 1 | freecad |
| self) | `_lpmapd` | 1 | qgis |
| self) | `_lpmarrfuncs` | 1 | qgis |
| self) | `_lpmatterm` | 1 | qgis |
| self) | `_lpmdelete` | 1 | qgis |
| self) | `_lpmexitprog` | 1 | qgis |
| self) | `_lpmgetcompctx` | 1 | qgis |
| self) | `_lpminit` | 1 | qgis |
| self) | `_lpminitm` | 1 | qgis |
| self) | `_lpmloadpkg` | 1 | qgis |
| self) | `_lpmmkpri` | 1 | qgis |
| self) | `_lpmsavecompctx` | 1 | qgis |
| self) | `_lpmterm` | 1 | qgis |
| self) | `_lpuclose` | 1 | qgis |
| self) | `_lpucompose` | 1 | qgis |
| self) | `_lpudecode` | 1 | qgis |
| self) | `_lpuinit` | 1 | qgis |
| self) | `_lpuopen` | 1 | qgis |
| self) | `_lpuparse` | 1 | qgis |
| self) | `_lpuread` | 1 | qgis |
| self) | `_lpuresolve` | 1 | qgis |
| self) | `_lpusets` | 1 | qgis |
| self) | `_lputerm` | 1 | qgis |
| self) | `_lrmfkv` | 1 | qgis |
| self) | `_lrmgakv` | 1 | qgis |
| self) | `_lrmgbv` | 1 | qgis |
| self) | `_lrmgiv` | 1 | qgis |
| self) | `_lrmgonv` | 1 | qgis |
| self) | `_lrmgsp` | 1 | qgis |
| self) | `_lrmgsv` | 1 | qgis |
| self) | `_lrmini` | 1 | qgis |
| self) | `_lrminiu` | 1 | qgis |
| self) | `_lrmpaa` | 1 | qgis |
| self) | `_lrmpfi` | 1 | qgis |
| self) | `_lrmpst` | 1 | qgis |
| self) | `_lrmsps` | 1 | qgis |
| self) | `_lrmtrm` | 1 | qgis |
| self) | `_lsfcln` | 1 | qgis |
| self) | `_lsfdi` | 1 | qgis |
| self) | `_lsfdl` | 1 | qgis |
| self) | `_lsfdrm` | 1 | qgis |
| self) | `_lsfini` | 1 | qgis |
| self) | `_lsfiniu` | 1 | qgis |
| self) | `_lsfp` | 1 | qgis |
| self) | `_lsfpd` | 1 | qgis |
| self) | `_lsfpv` | 1 | qgis |
| self) | `_lsrbsx` | 1 | qgis |
| self) | `_lsrsrt` | 1 | qgis |
| self) | `_lsrsrx` | 1 | qgis |
| self) | `_lstclo` | 1 | qgis |
| self) | `_lstcpad` | 1 | qgis |
| self) | `_lstcpyr` | 1 | qgis |
| self) | `_lstlo` | 1 | qgis |
| self) | `_lstmclo` | 1 | qgis |
| self) | `_lstmlo` | 1 | qgis |
| self) | `_lstmup` | 1 | qgis |
| self) | `_lstprintf` | 1 | qgis |
| self) | `_lstrep` | 1 | qgis |
| self) | `_lstrj` | 1 | qgis |
| self) | `_lstss` | 1 | qgis |
| self) | `_lsttokr` | 1 | qgis |
| self) | `_lstup` | 1 | qgis |
| self) | `_ltmctm` | 1 | qgis |
| self) | `_ltmdei` | 1 | qgis |
| self) | `_ltmdif` | 1 | qgis |
| self) | `_ltmftm` | 1 | qgis |
| self) | `_ltmini` | 1 | qgis |
| self) | `_ltmngid` | 1 | qgis |
| self) | `_ltmntm` | 1 | qgis |
| self) | `_ltmstm` | 1 | qgis |
| self) | `_ltmtxp` | 1 | qgis |
| self) | `_ltsini` | 1 | qgis |
| self) | `_ltsmdi` | 1 | qgis |
| self) | `_ltsmxd` | 1 | qgis |
| self) | `_ltsmxi` | 1 | qgis |
| self) | `_ltster` | 1 | qgis |
| self) | `_ltstidd` | 1 | qgis |
| self) | `_ltstidi` | 1 | qgis |
| self) | `_lxBomDetect` | 1 | qgis |
| self) | `_lxCmpStr` | 1 | qgis |
| self) | `_lxDesLxglo` | 1 | qgis |
| self) | `_lxDesLxhnd` | 1 | qgis |
| self) | `_lxGetCollationID` | 1 | qgis |
| self) | `_lxGetCollationName` | 1 | qgis |
| self) | `_lxGetCollationType` | 1 | qgis |
| self) | `_lxGetEffectiveCollation` | 1 | qgis |
| self) | `_lxLxitmToCollationID` | 1 | qgis |
| self) | `_lxSerGetVersion` | 1 | qgis |
| self) | `_lxSerLxglo` | 1 | qgis |
| self) | `_lxSerLxhnd2` | 1 | qgis |
| self) | `_lxXmlCvEsc` | 1 | qgis |
| self) | `_lxXmlCvEsc0` | 1 | qgis |
| self) | `_lxXmlGEntEsc2` | 1 | qgis |
| self) | `_lxXmlGEntEscImpl` | 1 | qgis |
| self) | `_lxXmlIdConv` | 1 | qgis |
| self) | `_lxXmlPrologueParser` | 1 | qgis |
| self) | `_lxcsCompose` | 1 | qgis |
| self) | `_lxcsDeCompose` | 1 | qgis |
| self) | `_lxdobl` | 1 | qgis |
| self) | `_lxdsupid` | 1 | qgis |
| self) | `_lxdsupsn` | 1 | qgis |
| self) | `_lxgConvertToAL32UTF8` | 1 | qgis |
| self) | `_lxgXmlConv` | 1 | qgis |
| self) | `_lxgXmlPcnv` | 1 | qgis |
| self) | `_lxgcnv` | 1 | qgis |
| self) | `_lxgcnvb` | 1 | qgis |
| self) | `_lxgcnvc` | 1 | qgis |
| self) | `_lxgcvp` | 1 | qgis |
| self) | `_lxgcvp_init` | 1 | qgis |
| self) | `_lxgcvpc` | 1 | qgis |
| self) | `_lxgncc` | 1 | qgis |
| self) | `_lxgratio` | 1 | qgis |
| self) | `_lxgrls` | 1 | qgis |
| self) | `_lxgstm` | 1 | qgis |
| self) | `_lxgt2u` | 1 | qgis |
| self) | `_lxgt2uc` | 1 | qgis |
| self) | `_lxgu2t` | 1 | qgis |
| self) | `_lxgucs2utf` | 1 | qgis |
| self) | `_lxgutf2ucs` | 1 | qgis |
| self) | `_lxhLaToId` | 1 | qgis |
| self) | `_lxhLangEnv` | 1 | qgis |
| self) | `_lxhasc` | 1 | qgis |
| self) | `_lxhchtoid` | 1 | qgis |
| self) | `_lxhci2h` | 1 | qgis |
| self) | `_lxhclrsave` | 1 | qgis |
| self) | `_lxhcnv` | 1 | qgis |
| self) | `_lxhcsn` | 1 | qgis |
| self) | `_lxhdStatus` | 1 | qgis |
| self) | `_lxhddump` | 1 | qgis |
| self) | `_lxhdtchid` | 1 | qgis |
| self) | `_lxhdumpbufsz` | 1 | qgis |
| self) | `_lxhdvld` | 1 | qgis |
| self) | `_lxhebc` | 1 | qgis |
| self) | `_lxhh2ci` | 1 | qgis |
| self) | `_lxhid2name` | 1 | qgis |
| self) | `_lxhidtolang` | 1 | qgis |
| self) | `_lxhlcmod` | 1 | qgis |
| self) | `_lxhlinfo` | 1 | qgis |
| self) | `_lxhlmod` | 1 | qgis |
| self) | `_lxhlod` | 1 | qgis |
| self) | `_lxhmcnv` | 1 | qgis |
| self) | `_lxhname2id` | 1 | qgis |
| self) | `_lxhnamemap` | 1 | qgis |
| self) | `_lxhninfo` | 1 | qgis |
| self) | `_lxhnlangid` | 1 | qgis |
| self) | `_lxhnmap` | 1 | qgis |
| self) | `_lxhnmod` | 1 | qgis |
| self) | `_lxhnsize` | 1 | qgis |
| self) | `_lxhsftime` | 1 | qgis |
| self) | `_lxinitc` | 1 | qgis |
| self) | `_lxinitsc` | 1 | qgis |
| self) | `_lxkAsciistr` | 1 | qgis |
| self) | `_lxkCSConvTest` | 1 | qgis |
| self) | `_lxkInstr` | 1 | qgis |
| self) | `_lxkLikPatPref` | 1 | qgis |
| self) | `_lxkLike` | 1 | qgis |
| self) | `_lxkLike2` | 1 | qgis |
| self) | `_lxkRegexpComp` | 1 | qgis |
| self) | `_lxkRegexpComp2` | 1 | qgis |
| self) | `_lxkRegexpCountLob` | 1 | qgis |
| self) | `_lxkRegexpFree` | 1 | qgis |
| self) | `_lxkRegexpInstrLobNSub` | 1 | qgis |
| self) | `_lxkRegexpInstrNSub2` | 1 | qgis |
| self) | `_lxkRegexpLike` | 1 | qgis |
| self) | `_lxkRegexpLike2` | 1 | qgis |
| self) | `_lxkRegexpLikeLob` | 1 | qgis |
| self) | `_lxkRegexpReplaceLob` | 1 | qgis |
| self) | `_lxkRegexpSubstrLobNSub` | 1 | qgis |
| self) | `_lxkTrim` | 1 | qgis |
| self) | `_lxkUnistr` | 1 | qgis |
| self) | `_lxldfcb` | 1 | qgis |
| self) | `_lxldini` | 1 | qgis |
| self) | `_lxlfterm` | 1 | qgis |
| self) | `_lxlinit` | 1 | qgis |
| self) | `_lxlterm` | 1 | qgis |
| self) | `_lxmalnx` | 1 | qgis |
| self) | `_lxmalpx` | 1 | qgis |
| self) | `_lxmblax` | 1 | qgis |
| self) | `_lxmbteqx` | 1 | qgis |
| self) | `_lxmc2wx` | 1 | qgis |
| self) | `_lxmcpbx` | 1 | qgis |
| self) | `_lxmcpen` | 1 | qgis |
| self) | `_lxmctex` | 1 | qgis |
| self) | `_lxmdigx` | 1 | qgis |
| self) | `_lxmfwdx` | 1 | qgis |
| self) | `_lxmfwtx` | 1 | qgis |
| self) | `_lxmnceq` | 1 | qgis |
| self) | `_lxmopen` | 1 | qgis |
| self) | `_lxmr2w` | 1 | qgis |
| self) | `_lxmral` | 1 | qgis |
| self) | `_lxmspax` | 1 | qgis |
| self) | `_lxmtos` | 1 | qgis |
| self) | `_lxmvopen` | 1 | qgis |
| self) | `_lxmxdix` | 1 | qgis |
| self) | `_lxnpdp` | 1 | qgis |
| self) | `_lxnpdpc` | 1 | qgis |
| self) | `_lxoCmpNStr` | 1 | qgis |
| self) | `_lxoCmpStr` | 1 | qgis |
| self) | `_lxoCntByte` | 1 | qgis |
| self) | `_lxoCntChar` | 1 | qgis |
| self) | `_lxoCnvCase` | 1 | qgis |
| self) | `_lxoCnvCh2Wide` | 1 | qgis |
| self) | `_lxoCnvNumStrToInt` | 1 | qgis |
| self) | `_lxoCpChar` | 1 | qgis |
| self) | `_lxoCpStr` | 1 | qgis |
| self) | `_lxoCpToOp` | 1 | qgis |
| self) | `_lxoCvChar` | 1 | qgis |
| self) | `_lxoPadStr` | 1 | qgis |
| self) | `_lxoSchPat` | 1 | qgis |
| self) | `_lxoSkip` | 1 | qgis |
| self) | `_lxoVldStr` | 1 | qgis |
| self) | `_lxoWriChar` | 1 | qgis |
| self) | `_lxoWriWChar` | 1 | qgis |
| self) | `_lxovid` | 1 | qgis |
| self) | `_lxregcomp` | 1 | qgis |
| self) | `_lxregexec` | 1 | qgis |
| self) | `_lxregexec2` | 1 | qgis |
| self) | `_lxregfree` | 1 | qgis |
| self) | `_lxsCatStr` | 1 | qgis |
| self) | `_lxsCmpStr` | 1 | qgis |
| self) | `_lxsCntByte` | 1 | qgis |
| self) | `_lxsCntChar` | 1 | qgis |
| self) | `_lxsCntDisp` | 1 | qgis |
| self) | `_lxsCntExc` | 1 | qgis |
| self) | `_lxsCnvCase` | 1 | qgis |
| self) | `_lxsCnvEqui` | 1 | qgis |
| self) | `_lxsCnvIntToNumStr` | 1 | qgis |
| self) | `_lxsCnvNumStrToInt` | 1 | qgis |
| self) | `_lxsCnvNumStrToLong` | 1 | qgis |
| self) | `_lxsCnvSimple` | 1 | qgis |
| self) | `_lxsCpFrWide` | 1 | qgis |
| self) | `_lxsCpStr` | 1 | qgis |
| self) | `_lxsCpToWide` | 1 | qgis |
| self) | `_lxsNextCol2` | 1 | qgis |
| self) | `_lxsRepStr` | 1 | qgis |
| self) | `_lxsStrCol2` | 1 | qgis |
| self) | `_lxsTrnStr` | 1 | qgis |
| self) | `_lxsVldStr` | 1 | qgis |
| self) | `_lxsbcpl` | 1 | qgis |
| self) | `_lxsbcpu` | 1 | qgis |
| self) | `_lxscat` | 1 | qgis |
| self) | `_lxscop` | 1 | qgis |
| self) | `_lxsulen` | 1 | qgis |
| self) | `_lxu4CnvCase` | 1 | qgis |
| self) | `_lxu4CvCplrChar` | 1 | qgis |
| self) | `_lxu4GUb1AbstChar` | 1 | qgis |
| self) | `_lxu4Property` | 1 | qgis |
| self) | `_lxu4SchChar` | 1 | qgis |
| self) | `_lxu4TstClass` | 1 | qgis |
| self) | `_lxu4TstClsIsCompatibleChar` | 1 | qgis |
| self) | `_lxu4TstClsXMLPubidChar0` | 1 | qgis |
| self) | `_lxuAddToStr` | 1 | qgis |
| self) | `_lxuCmpBinStr` | 1 | qgis |
| self) | `_lxuCmpStr` | 1 | qgis |
| self) | `_lxuCnvCase` | 1 | qgis |
| self) | `_lxuCnvNumStrToInt` | 1 | qgis |
| self) | `_lxuCpCplrStr` | 1 | qgis |
| self) | `_lxuCpStr` | 1 | qgis |
| self) | `_lxuCvtToCtx` | 1 | qgis |
| self) | `_lxuFreCtx` | 1 | qgis |
| self) | `_lxuSchPattern` | 1 | qgis |
| self) | `_lxuSchSet` | 1 | qgis |
| self) | `_lxuStrLen` | 1 | qgis |
| self) | `_lxumsgbcmt` | 1 | qgis |
| self) | `_lxumsicmt` | 1 | qgis |
| self) | `_lxwCmpStr` | 1 | qgis |
| self) | `_lxwCnvCase` | 1 | qgis |
| self) | `_lxwCvCplrChar` | 1 | qgis |
| self) | `_lxwalnx` | 1 | qgis |
| self) | `_lxwalpx` | 1 | qgis |
| self) | `_lxwblax` | 1 | qgis |
| self) | `_lxwc2lx` | 1 | qgis |
| self) | `_lxwc2ux` | 1 | qgis |
| self) | `_lxwcntx` | 1 | qgis |
| self) | `_lxwctex` | 1 | qgis |
| self) | `_lxwdigx` | 1 | qgis |
| self) | `_lxwdspx` | 1 | qgis |
| self) | `_lxwgrax` | 1 | qgis |
| self) | `_lxwlowx` | 1 | qgis |
| self) | `_lxwprix` | 1 | qgis |
| self) | `_lxwpunx` | 1 | qgis |
| self) | `_lxwspax` | 1 | qgis |
| self) | `_lxwuppx` | 1 | qgis |
| self) | `_lxwxdix` | 1 | qgis |
| self) | `_lxzinit` | 1 | qgis |
| self) | `_makeParamList` | 1 | freecad |
| self) | `_makeRangeVar` | 1 | freecad |
| self) | `_makeRangeVarFromNameList` | 1 | freecad |
| self) | `_makeString` | 1 | freecad |
| self) | `_makeTypeName` | 1 | freecad |
| self) | `_makeTypeNameFromNameList` | 1 | freecad |
| self) | `_make_expanded_record_from_exprecord` | 1 | freecad |
| self) | `_make_expanded_record_from_tupdesc` | 1 | freecad |
| self) | `_make_expanded_record_from_typeid` | 1 | freecad |
| self) | `_make_orclause` | 1 | freecad |
| self) | `_mic2latin` | 1 | freecad |
| self) | `_mic2latin_with_table` | 1 | freecad |
| self) | `_namein` | 1 | freecad |
| self) | `_napi_add_finalizer` | 1 | comfy |
| self) | `_napi_call_function` | 1 | comfy |
| self) | `_napi_call_threadsafe_function` | 1 | comfy |
| self) | `_napi_close_escapable_handle_scope` | 1 | comfy |
| self) | `_napi_close_handle_scope` | 1 | comfy |
| self) | `_napi_create_double` | 1 | comfy |
| self) | `_napi_create_error` | 1 | comfy |
| self) | `_napi_create_function` | 1 | comfy |
| self) | `_napi_create_object` | 1 | comfy |
| self) | `_napi_create_reference` | 1 | comfy |
| self) | `_napi_create_string_utf8` | 1 | comfy |
| self) | `_napi_create_threadsafe_function` | 1 | comfy |
| self) | `_napi_create_type_error` | 1 | comfy |
| self) | `_napi_define_properties` | 1 | comfy |
| self) | `_napi_delete_reference` | 1 | comfy |
| self) | `_napi_escape_handle` | 1 | comfy |
| self) | `_napi_fatal_error` | 1 | comfy |
| self) | `_napi_get_and_clear_last_exception` | 1 | comfy |
| self) | `_napi_get_array_length` | 1 | comfy |
| self) | `_napi_get_cb_info` | 1 | comfy |
| self) | `_napi_get_element` | 1 | comfy |
| self) | `_napi_get_last_error_info` | 1 | comfy |
| self) | `_napi_get_named_property` | 1 | comfy |
| self) | `_napi_get_property` | 1 | comfy |
| self) | `_napi_get_reference_value` | 1 | comfy |
| self) | `_napi_get_undefined` | 1 | comfy |
| self) | `_napi_get_value_bool` | 1 | comfy |
| self) | `_napi_get_value_int32` | 1 | comfy |
| self) | `_napi_get_value_string_utf8` | 1 | comfy |
| self) | `_napi_has_property` | 1 | comfy |
| self) | `_napi_is_array` | 1 | comfy |
| self) | `_napi_is_exception_pending` | 1 | comfy |
| self) | `_napi_open_escapable_handle_scope` | 1 | comfy |
| self) | `_napi_open_handle_scope` | 1 | comfy |
| self) | `_napi_release_threadsafe_function` | 1 | comfy |
| self) | `_napi_set_named_property` | 1 | comfy |
| self) | `_napi_throw` | 1 | comfy |
| self) | `_napi_typeof` | 1 | comfy |
| self) | `_nzSWB_SetWalletArray` | 1 | qgis |
| self) | `_nzcrl_CreateCtx` | 1 | qgis |
| self) | `_nzcsf_AddtoCredStore` | 1 | qgis |
| self) | `_nzdcfcx_free_cert_ctx` | 1 | qgis |
| self) | `_nzdcgui_get_userinfo` | 1 | qgis |
| self) | `_nzdsi_init` | 1 | qgis |
| self) | `_nzdsi_initialize` | 1 | qgis |
| self) | `_nzdst_terminate` | 1 | qgis |
| self) | `_nzduui1_create_ui` | 1 | qgis |
| self) | `_nzduui2_destroy_ui` | 1 | qgis |
| self) | `_nzduui6_get_name` | 1 | qgis |
| self) | `_nzduui7_parse_x509_name` | 1 | qgis |
| self) | `_nzdycs0_stop` | 1 | qgis |
| self) | `_nzdycs1_start` | 1 | qgis |
| self) | `_nzdye_encrypt` | 1 | qgis |
| self) | `_nzgbllsm_location_shared_memory` | 1 | qgis |
| self) | `_nzhdcgldn_getldapdn` | 1 | qgis |
| self) | `_nzosCipherSpecToStr` | 1 | qgis |
| self) | `_nzosDestroyCredential` | 1 | qgis |
| self) | `_nzosGetPeerCredential` | 1 | qgis |
| self) | `_nzosGetSupportedCipher` | 1 | qgis |
| self) | `_nzosSCCS_SetCRLCacheSize` | 1 | qgis |
| self) | `_nzosSCSP_SetCertSelectionParams` | 1 | qgis |
| self) | `_nzosSetCipherSuite` | 1 | qgis |
| self) | `_nzosSetCredential` | 1 | qgis |
| self) | `_nzos_Close` | 1 | qgis |
| self) | `_nzos_CompareDN` | 1 | qgis |
| self) | `_nzos_Configure` | 1 | qgis |
| self) | `_nzos_Create_Ctx` | 1 | qgis |
| self) | `_nzos_Deinitialize` | 1 | qgis |
| self) | `_nzos_Destroy_Ctx` | 1 | qgis |
| self) | `_nzos_DisableClientAuthentication` | 1 | qgis |
| self) | `_nzos_Get_Ctx` | 1 | qgis |
| self) | `_nzos_Handshake` | 1 | qgis |
| self) | `_nzos_Initialize` | 1 | qgis |
| self) | `_nzos_MatchHostname` | 1 | qgis |
| self) | `_nzos_ModifyCipherSuites` | 1 | qgis |
| self) | `_nzos_OToolkitContext` | 1 | qgis |
| self) | `_nzos_OpenWallet` | 1 | qgis |
| self) | `_nzos_Read` | 1 | qgis |
| self) | `_nzos_RetrievePersonaCopy` | 1 | qgis |
| self) | `_nzos_ServiceWriteQueue` | 1 | qgis |
| self) | `_nzos_SetLdapFunc` | 1 | qgis |
| self) | `_nzos_SetServerNameList` | 1 | qgis |
| self) | `_nzos_SetSqlnetFunc` | 1 | qgis |
| self) | `_nzos_Write` | 1 | qgis |
| self) | `_nzos_enableHWAcc` | 1 | qgis |
| self) | `_nzos_setCRLFile` | 1 | qgis |
| self) | `_nzos_setCRLPath` | 1 | qgis |
| self) | `_nzos_setCertValPolicy` | 1 | qgis |
| self) | `_nzos_setCertValProtocol` | 1 | qgis |
| self) | `_nzos_setOIDConnInfo` | 1 | qgis |
| self) | `_nzos_setThrUse` | 1 | qgis |
| self) | `_nzosaccrb_aysnCheckRecbuf` | 1 | qgis |
| self) | `_nzosgnc_GetNegotiatedCipher` | 1 | qgis |
| self) | `_nzosgpc_GetProtocolVer` | 1 | qgis |
| self) | `_nzssGEBV_GetEntryByValue` | 1 | qgis |
| self) | `_nzssGSL_GetSecretLength` | 1 | qgis |
| self) | `_nzssGS_GetSecret` | 1 | qgis |
| self) | `_nzstr_alloc` | 1 | qgis |
| self) | `_nzstrcpy` | 1 | qgis |
| self) | `_nzstrfc_free_content` | 1 | qgis |
| self) | `_nzstrlen` | 1 | qgis |
| self) | `_nztGetSubjectName` | 1 | qgis |
| self) | `_nztSearchNZDefault` | 1 | qgis |
| self) | `_nztSetAppDefaultLocation` | 1 | qgis |
| self) | `_nzteDestroyPersona` | 1 | qgis |
| self) | `_nzteMyCert` | 1 | qgis |
| self) | `_nzteOpenPersona` | 1 | qgis |
| self) | `_nztiFIL_Free_Identity_List` | 1 | qgis |
| self) | `_nztiGCC_Get_CertCtx` | 1 | qgis |
| self) | `_nztiGetSecInfo` | 1 | qgis |
| self) | `_nztiee_IsEncrEnabled` | 1 | qgis |
| self) | `_nztnFPPL_Free_PersonaPvt_List` | 1 | qgis |
| self) | `_nztnGCL_Get_Certificate_List` | 1 | qgis |
| self) | `_nztwCloseWallet` | 1 | qgis |
| self) | `_nztwDAW_Destroy_A_Wallet` | 1 | qgis |
| self) | `_nztwOpenWallet` | 1 | qgis |
| self) | `_nztwRetrievePersonaCopy` | 1 | qgis |
| self) | `_nzu_exit_trace` | 1 | qgis |
| self) | `_nzu_init_trace` | 1 | qgis |
| self) | `_nzu_print_trace` | 1 | qgis |
| self) | `_nzumalloc` | 1 | qgis |
| self) | `_nzumfree` | 1 | qgis |
| self) | `_nzusnt_save_null_terminate` | 1 | qgis |
| self) | `_nzustralloc` | 1 | qgis |
| self) | `_ovl_op_info` | 1 | emacs-app |
| self) | `_ovl_op_mapping` | 1 | emacs-app |
| self) | `_parser_errposition` | 1 | freecad |
| self) | `_pchomp` | 1 | freecad |
| self) | `_pg_any_to_server` | 1 | freecad |
| self) | `_pg_bindtextdomain` | 1 | freecad |
| self) | `_pg_detoast_datum` | 1 | freecad |
| self) | `_pg_detoast_datum_copy` | 1 | freecad |
| self) | `_pg_encoding_verifymbchar` | 1 | freecad |
| self) | `_pg_lsn_in` | 1 | freecad |
| self) | `_pg_mbstrlen_with_len` | 1 | freecad |
| self) | `_pg_re_throw` | 1 | freecad |
| self) | `_pg_server_to_any` | 1 | freecad |
| self) | `_pg_strcasecmp` | 1 | freecad |
| self) | `_pg_strtoint32` | 1 | freecad |
| self) | `_pg_utf8_islegal` | 1 | freecad |
| self) | `_pg_utf_mblen_private` | 1 | freecad |
| self) | `_pre_format_elog_string` | 1 | freecad |
| self) | `_prime_tab` | 1 | emacs-app |
| self) | `_pthread_getconcurrency` | 1 | insomnia |
| self) | `_pthread_setconcurrency` | 1 | insomnia |
| self) | `_ptr_R_Busy` | 1 | rstudio |
| self) | `_ptr_R_ChooseFile` | 1 | rstudio |
| self) | `_ptr_R_CleanUp` | 1 | rstudio |
| self) | `_ptr_R_EditFile` | 1 | rstudio |
| self) | `_ptr_R_ReadConsole` | 1 | rstudio |
| self) | `_ptr_R_ResetConsole` | 1 | rstudio |
| self) | `_ptr_R_ShowFiles` | 1 | rstudio |
| self) | `_ptr_R_ShowMessage` | 1 | rstudio |
| self) | `_ptr_R_Suicide` | 1 | rstudio |
| self) | `_ptr_R_WriteConsole` | 1 | rstudio |
| self) | `_ptr_R_WriteConsoleEx` | 1 | rstudio |
| self) | `_ptr_R_addhistory` | 1 | rstudio |
| self) | `_ptr_R_loadhistory` | 1 | rstudio |
| self) | `_ptr_R_savehistory` | 1 | rstudio |
| self) | `_pub_collist_to_bitmapset` | 1 | freecad |
| self) | `_pz5ckcallb7` | 1 | qgis |
| self) | `_pz5ctxfr7` | 1 | qgis |
| self) | `_pz5goto7` | 1 | qgis |
| self) | `_pz5spus7` | 1 | qgis |
| self) | `_pz5strt7` | 1 | qgis |
| self) | `_pz7arlcl` | 1 | qgis |
| self) | `_pz7arlfr` | 1 | qgis |
| self) | `_pz7arlgc` | 1 | qgis |
| self) | `_pz7arlge` | 1 | qgis |
| self) | `_quote_identifier` | 1 | freecad |
| self) | `_raw_parser` | 1 | freecad |
| self) | `_readstoplist` | 1 | freecad |
| self) | `_register_callback` | 1 | emacs-app |
| self) | `_replorigin_by_oid` | 1 | freecad |
| self) | `_report_invalid_encoding` | 1 | freecad |
| self) | `_report_untranslatable_char` | 1 | freecad |
| self) | `_res_9_dn_skipname` | 1 | qgis |
| self) | `_resolve_polymorphic_argtypes` | 1 | freecad |
| self) | `_ridpointers` | 1 | emacs-app |
| self) | `_run_Rmainloop` | 1 | rstudio |
| self) | `_sLdiGetDate` | 1 | qgis |
| self) | `_sLdiInitLazyDt` | 1 | qgis |
| self) | `_scanner_finish` | 1 | freecad |
| self) | `_scanner_init` | 1 | freecad |
| self) | `_scanner_isspace` | 1 | freecad |
| self) | `_scope_chain` | 1 | emacs-app |
| self) | `_searchstoplist` | 1 | freecad |
| self) | `_set_errcontext_domain` | 1 | freecad |
| self) | `_setup_Rmainloop` | 1 | rstudio |
| self) | `_slbiv_popcount_ub8` | 1 | qgis |
| self) | `_slbivec2wvec` | 1 | qgis |
| self) | `_slbiwvand` | 1 | qgis |
| self) | `_slbiwvec2vec` | 1 | qgis |
| self) | `_slbiwvffs` | 1 | qgis |
| self) | `_slbiwvfls` | 1 | qgis |
| self) | `_slbiwvnot` | 1 | qgis |
| self) | `_slbiwvor` | 1 | qgis |
| self) | `_slbiwvpopc` | 1 | qgis |
| self) | `_slcdalloc` | 1 | qgis |
| self) | `_slcgems` | 1 | qgis |
| self) | `_slcvs2ptr` | 1 | qgis |
| self) | `_sldxgd` | 1 | qgis |
| self) | `_slfitcl` | 1 | qgis |
| self) | `_slfitcn` | 1 | qgis |
| self) | `_slfitlb` | 1 | qgis |
| self) | `_slfitlt` | 1 | qgis |
| self) | `_slfpd2f` | 1 | qgis |
| self) | `_slfpd2lnx` | 1 | qgis |
| self) | `_slfpd2s` | 1 | qgis |
| self) | `_slfpdadd` | 1 | qgis |
| self) | `_slfpdceil` | 1 | qgis |
| self) | `_slfpddiv` | 1 | qgis |
| self) | `_slfpdeq` | 1 | qgis |
| self) | `_slfpdgt` | 1 | qgis |
| self) | `_slfpdisnan` | 1 | qgis |
| self) | `_slfpdisnzero` | 1 | qgis |
| self) | `_slfpdle` | 1 | qgis |
| self) | `_slfpdlt` | 1 | qgis |
| self) | `_slfpdmul` | 1 | qgis |
| self) | `_slfpdne` | 1 | qgis |
| self) | `_slfpdneg` | 1 | qgis |
| self) | `_slfpdsub` | 1 | qgis |
| self) | `_slfpf2d` | 1 | qgis |
| self) | `_slfpf2lnx` | 1 | qgis |
| self) | `_slfpf2s` | 1 | qgis |
| self) | `_slfpfadd` | 1 | qgis |
| self) | `_slfpfceil` | 1 | qgis |
| self) | `_slfpfdiv` | 1 | qgis |
| self) | `_slfpfeq` | 1 | qgis |
| self) | `_slfpfgt` | 1 | qgis |
| self) | `_slfpfisnan` | 1 | qgis |
| self) | `_slfpfle` | 1 | qgis |
| self) | `_slfpflt` | 1 | qgis |
| self) | `_slfpfmul` | 1 | qgis |
| self) | `_slfpfne` | 1 | qgis |
| self) | `_slfpfsub` | 1 | qgis |
| self) | `_slfplnx2d` | 1 | qgis |
| self) | `_slfplnx2f` | 1 | qgis |
| self) | `_slfps2de` | 1 | qgis |
| self) | `_slfps2fe` | 1 | qgis |
| self) | `_slmaacb` | 1 | qgis |
| self) | `_slmeswap` | 1 | qgis |
| self) | `_slmtnatof` | 1 | qgis |
| self) | `_slmtnatol` | 1 | qgis |
| self) | `_slmtnl10` | 1 | qgis |
| self) | `_slmtnpow` | 1 | qgis |
| self) | `_slmtrdrand32` | 1 | qgis |
| self) | `_slmttpow` | 1 | qgis |
| self) | `_slmxgetncpu` | 1 | qgis |
| self) | `_slosDep2Mesg` | 1 | qgis |
| self) | `_slosDep2String` | 1 | qgis |
| self) | `_slosFillErr` | 1 | qgis |
| self) | `_slosFillInt` | 1 | qgis |
| self) | `_slosOtherInfo` | 1 | qgis |
| self) | `_slot_getsomeattrs_int` | 1 | freecad |
| self) | `_slputcpaccept` | 1 | qgis |
| self) | `_slputcpclose` | 1 | qgis |
| self) | `_slputcpinit` | 1 | qgis |
| self) | `_slputcplisten` | 1 | qgis |
| self) | `_slputcpopen` | 1 | qgis |
| self) | `_slputcpread` | 1 | qgis |
| self) | `_slputcprecv` | 1 | qgis |
| self) | `_slputcpwrite` | 1 | qgis |
| self) | `_slsta2e` | 1 | qgis |
| self) | `_slste2a` | 1 | qgis |
| self) | `_sltmdf` | 1 | qgis |
| self) | `_sltmgcs` | 1 | qgis |
| self) | `_sltrgcs` | 1 | qgis |
| self) | `_sltrusleep` | 1 | qgis |
| self) | `_sltsThndIsNull` | 1 | qgis |
| self) | `_sltsThndIsSame` | 1 | qgis |
| self) | `_sltsThrIsNull` | 1 | qgis |
| self) | `_sltsThrIsSame` | 1 | qgis |
| self) | `_slts_get_running_status` | 1 | qgis |
| self) | `_slts_runmode` | 1 | qgis |
| self) | `_sltsima` | 1 | qgis |
| self) | `_sltsimr` | 1 | qgis |
| self) | `_sltsini` | 1 | qgis |
| self) | `_sltskyc` | 1 | qgis |
| self) | `_sltskydestroy` | 1 | qgis |
| self) | `_sltskyg` | 1 | qgis |
| self) | `_sltskys` | 1 | qgis |
| self) | `_sltsmna` | 1 | qgis |
| self) | `_sltsmnr` | 1 | qgis |
| self) | `_sltsmnt` | 1 | qgis |
| self) | `_sltsmxd` | 1 | qgis |
| self) | `_sltsmxi` | 1 | qgis |
| self) | `_sltspcbroadcast` | 1 | qgis |
| self) | `_sltspcdestroy` | 1 | qgis |
| self) | `_sltspcinit` | 1 | qgis |
| self) | `_sltspcsignal` | 1 | qgis |
| self) | `_sltspctimewait` | 1 | qgis |
| self) | `_sltspcwait` | 1 | qgis |
| self) | `_sltspin` | 1 | qgis |
| self) | `_sltspla` | 1 | qgis |
| self) | `_sltsplr` | 1 | qgis |
| self) | `_sltstah` | 1 | qgis |
| self) | `_sltstai` | 1 | qgis |
| self) | `_sltstan` | 1 | qgis |
| self) | `_sltstcl` | 1 | qgis |
| self) | `_sltstcr` | 1 | qgis |
| self) | `_sltstcu` | 1 | qgis |
| self) | `_sltster` | 1 | qgis |
| self) | `_sltstgh` | 1 | qgis |
| self) | `_sltstgi` | 1 | qgis |
| self) | `_sltsthan` | 1 | qgis |
| self) | `_sltsthnddestroy` | 1 | qgis |
| self) | `_sltsthndinit` | 1 | qgis |
| self) | `_sltstiddestroy` | 1 | qgis |
| self) | `_sltstidinit` | 1 | qgis |
| self) | `_sltstjn` | 1 | qgis |
| self) | `_sltstkill` | 1 | qgis |
| self) | `_sltstprint` | 1 | qgis |
| self) | `_sltstspawn` | 1 | qgis |
| self) | `_sltsttr` | 1 | qgis |
| self) | `_sltstyield` | 1 | qgis |
| self) | `_slzabort` | 1 | qgis |
| self) | `_slzerrstr` | 1 | qgis |
| self) | `_slzexit` | 1 | qgis |
| self) | `_slzgetevar` | 1 | qgis |
| self) | `_slzgetohorabase` | 1 | qgis |
| self) | `_slzgetohorabaseconfig` | 1 | qgis |
| self) | `_slzgetohorabasehome` | 1 | qgis |
| self) | `_slzprintf` | 1 | qgis |
| self) | `_slzsetevar` | 1 | qgis |
| self) | `_slztln` | 1 | qgis |
| self) | `_snzdafn_assemble_filename` | 1 | qgis |
| self) | `_ssMemFree_shared` | 1 | qgis |
| self) | `_ssMemMalloc_shared` | 1 | qgis |
| self) | `_ssOswRecvSockFd` | 1 | qgis |
| self) | `_sslsigprobe` | 1 | qgis |
| self) | `_sslsigreghndlr` | 1 | qgis |
| self) | `_sslssalck` | 1 | qgis |
| self) | `_sslssmodhdlr` | 1 | qgis |
| self) | `_sslssreghdlr` | 1 | qgis |
| self) | `_sslssunreghdlr` | 1 | qgis |
| self) | `_ssltstraise` | 1 | qgis |
| self) | `_stringToNode` | 1 | freecad |
| self) | `_text_to_cstring` | 1 | freecad |
| self) | `_tree_contains_struct` | 1 | emacs-app |
| self) | `_tuplestore_begin_heap` | 1 | freecad |
| self) | `_tuplestore_puttuple` | 1 | freecad |
| self) | `_tuplestore_putvalues` | 1 | freecad |
| self) | `_tuplestore_tuple_count` | 1 | freecad |
| self) | `_typeStringToTypeName` | 1 | freecad |
| self) | `_type_is_rowtype` | 1 | freecad |
| self) | `_typenameTypeIdAndMod` | 1 | freecad |
| self) | `_unpack_sql_state` | 1 | freecad |
| self) | `_vmaxget` | 1 | rstudio |
| self) | `_vmaxset` | 1 | rstudio |
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
| self) | `_work_mem` | 1 | freecad |
| self) | `_zt_init` | 1 | qgis |
| self) | `_ztce_getctx` | 1 | qgis |
| self) | `_ztce_setctx` | 1 | qgis |
| self) | `_ztcedchk` | 1 | qgis |
| self) | `_ztcedd` | 1 | qgis |
| self) | `_ztcede` | 1 | qgis |
| self) | `_ztcedec` | 1 | qgis |
| self) | `_ztcedecb` | 1 | qgis |
| self) | `_ztcedgks` | 1 | qgis |
| self) | `_ztcedst` | 1 | qgis |
| self) | `_ztceenc` | 1 | qgis |
| self) | `_ztcef` | 1 | qgis |
| self) | `_ztcegml` | 1 | qgis |
| self) | `_ztcei` | 1 | qgis |
| self) | `_ztcen` | 1 | qgis |
| self) | `_ztch` | 1 | qgis |
| self) | `_ztchdst` | 1 | qgis |
| self) | `_ztchf` | 1 | qgis |
| self) | `_ztchi` | 1 | qgis |
| self) | `_ztchn` | 1 | qgis |
| self) | `_ztcr2rnd` | 1 | qgis |
| self) | `_ztcr2seed` | 1 | qgis |
| self) | `_ztcr2ub4` | 1 | qgis |
| self) | `_ztcrandom` | 1 | qgis |
| self) | `_ztcrseed3` | 1 | qgis |
| self) | `_ztcsh` | 1 | qgis |
| self) | `_ztcsl` | 1 | qgis |
| self) | `_ztcsr` | 1 | qgis |
| self) | `_ztcx` | 1 | qgis |
| self) | `_ztcxdst` | 1 | qgis |
| self) | `_ztcxf` | 1 | qgis |
| self) | `_ztcxi` | 1 | qgis |
| self) | `_ztcxn` | 1 | qgis |
| self) | `_zterr2ora` | 1 | qgis |
| self) | `_zterr2trc` | 1 | qgis |
| self) | `_ztpk_DestroyKey` | 1 | qgis |
| self) | `_ztpk_SetKeyInfo` | 1 | qgis |
| self) | `_ztpk_Sign` | 1 | qgis |
| self) | `_ztub64d` | 1 | qgis |
| self) | `_ztub64e` | 1 | qgis |
| self) | `_ztub64ecr` | 1 | qgis |
| self) | `_ztub64en` | 1 | qgis |
| self) | `_ztub64gol` | 1 | qgis |
| self) | `_ztub64scn` | 1 | qgis |
| self) | `_ztub64te` | 1 | qgis |
| self) | `_ztuc1tx` | 1 | qgis |
| self) | `_ztuc2t8` | 1 | qgis |
| self) | `_ztuc8t2` | 1 | qgis |
| self) | `_ztuc8tx` | 1 | qgis |
| self) | `_ztucbtx` | 1 | qgis |
| self) | `_ztucxt1` | 1 | qgis |
| self) | `_ztucxt8` | 1 | qgis |
| self) | `_ztucxtb` | 1 | qgis |
| self) | `_ztuguid` | 1 | qgis |
| self) | `_ztv2gcrypt` | 1 | qgis |
| self) | `_ztv2ghash` | 1 | qgis |
| self) | `_ztv2glmv` | 1 | qgis |
| self) | `_ztv2gntv` | 1 | qgis |
| self) | `_ztv2gntvc` | 1 | qgis |
| self) | `_ztv2gorcl` | 1 | qgis |
| self) | `_ztv2gsaslmd5` | 1 | qgis |
| self) | `_ztvovg` | 1 | qgis |
| self) | `_ztvovgn` | 1 | qgis |
| self) | `_ztvp522` | 1 | qgis |
| self) | `_ztvulc` | 1 | qgis |
| self) | `_ztvulp` | 1 | qgis |
