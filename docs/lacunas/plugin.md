# Lacunas de Plugins (flat lookup: o símbolo vem do app que carrega o plugin, não do mOS)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Plugins (flat lookup: o símbolo vem do app que carrega o plugin, não do mOS) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**674** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| self) | `_PyDict_New` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyDict_SetItem` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyDict_SetItemString` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyErr_Clear` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyErr_Format` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyErr_Occurred` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyErr_SetString` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyExc_AttributeError` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyExc_RuntimeError` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyExc_TypeError` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyExc_ValueError` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyFloat_FromDouble` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyFloat_Type` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyLong_AsLong` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyLong_AsLongLong` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyLong_FromLong` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyLong_FromUnsignedLong` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyLong_FromUnsignedLongLong` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyModule_GetDict` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyObject_GetAttrString` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyTuple_GetItem` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyTuple_New` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyTuple_SetItem` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyTuple_Size` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyType_IsSubtype` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyType_Type` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `__Py_NoneStruct` | 4 | lm-studio, obs, sourcetree, virtualbox |
| self) | `_PyArg_ParseTuple` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyBool_FromLong` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyBool_Type` | 3 | lm-studio, obs, sourcetree |
| self) | `_PyDict_GetItem` | 3 | obs, sourcetree, virtualbox |
| self) | `_PyErr_NoMemory` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyErr_SetObject` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyExc_IndexError` | 3 | lm-studio, obs, sourcetree |
| self) | `_PyExc_MemoryError` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyExc_OverflowError` | 3 | lm-studio, obs, sourcetree |
| self) | `_PyExc_SystemError` | 3 | lm-studio, obs, sourcetree |
| self) | `_PyFloat_AsDouble` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyImport_AddModule` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyImport_ImportModule` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyList_Append` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyList_New` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyList_SetItem` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyLong_AsUnsignedLongLong` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyLong_FromSize_t` | 3 | lm-studio, obs, sourcetree |
| self) | `_PyMem_Free` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyMem_Malloc` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyModule_AddObject` | 3 | lm-studio, obs, sourcetree |
| self) | `_PyModule_Create2` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyObject_CallFunctionObjArgs` | 3 | lm-studio, obs, sourcetree |
| self) | `_PyObject_CallMethod` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyObject_CallObject` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyObject_GetAttr` | 3 | lm-studio, obs, sourcetree |
| self) | `_PyObject_HasAttrString` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyObject_IsTrue` | 3 | lm-studio, obs, sourcetree |
| self) | `_PySequence_GetItem` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyTuple_GetSlice` | 3 | lm-studio, obs, sourcetree |
| self) | `_PyType_FromSpec` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyType_GetFlags` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyUnicode_DecodeUTF8` | 3 | lm-studio, obs, virtualbox |
| self) | `_PyUnicode_FromString` | 3 | lm-studio, obs, virtualbox |
| self) | `_Py_BuildValue` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `__Py_Dealloc` | 3 | lm-studio, obs, virtualbox |
| self) | `__Py_TrueStruct` | 3 | lm-studio, sourcetree, virtualbox |
| self) | `_PyArg_ParseTupleAndKeywords` | 2 | lm-studio, sourcetree |
| self) | `_PyArg_UnpackTuple` | 2 | lm-studio, obs |
| self) | `_PyBaseObject_Type` | 2 | lm-studio, sourcetree |
| self) | `_PyBytes_AsString` | 2 | lm-studio, virtualbox |
| self) | `_PyBytes_AsStringAndSize` | 2 | lm-studio, obs |
| self) | `_PyBytes_FromStringAndSize` | 2 | lm-studio, virtualbox |
| self) | `_PyBytes_Size` | 2 | lm-studio, virtualbox |
| self) | `_PyCallable_Check` | 2 | lm-studio, sourcetree |
| self) | `_PyCapsule_GetPointer` | 2 | lm-studio, obs |
| self) | `_PyCapsule_New` | 2 | lm-studio, obs |
| self) | `_PyDict_Contains` | 2 | lm-studio, sourcetree |
| self) | `_PyDict_DelItem` | 2 | lm-studio, sourcetree |
| self) | `_PyDict_DelItemString` | 2 | lm-studio, sourcetree |
| self) | `_PyDict_GetItemString` | 2 | lm-studio, sourcetree |
| self) | `_PyDict_Next` | 2 | lm-studio, sourcetree |
| self) | `_PyDict_Size` | 2 | lm-studio, sourcetree |
| self) | `_PyDict_Type` | 2 | lm-studio, sourcetree |
| self) | `_PyErr_ExceptionMatches` | 2 | lm-studio, virtualbox |
| self) | `_PyErr_Fetch` | 2 | obs, virtualbox |
| self) | `_PyErr_NormalizeException` | 2 | lm-studio, virtualbox |
| self) | `_PyErr_Print` | 2 | lm-studio, virtualbox |
| self) | `_PyErr_Restore` | 2 | obs, virtualbox |
| self) | `_PyErr_SetNone` | 2 | lm-studio, sourcetree |
| self) | `_PyEval_CallObjectWithKeywords` | 2 | sourcetree, virtualbox |
| self) | `_PyEval_RestoreThread` | 2 | sourcetree, virtualbox |
| self) | `_PyEval_SaveThread` | 2 | sourcetree, virtualbox |
| self) | `_PyExc_Exception` | 2 | lm-studio, virtualbox |
| self) | `_PyExc_ImportError` | 2 | lm-studio, sourcetree |
| self) | `_PyExc_KeyError` | 2 | lm-studio, sourcetree |
| self) | `_PyExc_NotImplementedError` | 2 | lm-studio, sourcetree |
| self) | `_PyExc_OSError` | 2 | lm-studio, sourcetree |
| self) | `_PyExc_StopIteration` | 2 | lm-studio, sourcetree |
| self) | `_PyExc_ZeroDivisionError` | 2 | lm-studio, obs |
| self) | `_PyGILState_Ensure` | 2 | obs, virtualbox |
| self) | `_PyGILState_Release` | 2 | obs, virtualbox |
| self) | `_PyIter_Next` | 2 | lm-studio, sourcetree |
| self) | `_PyList_GetItem` | 2 | lm-studio, sourcetree |
| self) | `_PyList_Size` | 2 | lm-studio, sourcetree |
| self) | `_PyList_Sort` | 2 | lm-studio, sourcetree |
| self) | `_PyList_Type` | 2 | lm-studio, sourcetree |
| self) | `_PyLong_AsUnsignedLong` | 2 | lm-studio, obs |
| self) | `_PyLong_FromLongLong` | 2 | lm-studio, virtualbox |
| self) | `_PyLong_FromSsize_t` | 2 | lm-studio, sourcetree |
| self) | `_PyMem_Realloc` | 2 | lm-studio, sourcetree |
| self) | `_PyModule_AddIntConstant` | 2 | lm-studio, sourcetree |
| self) | `_PyNumber_AsSsize_t` | 2 | lm-studio, sourcetree |
| self) | `_PyObject_AsReadBuffer` | 2 | lm-studio, virtualbox |
| self) | `_PyObject_Call` | 2 | lm-studio, obs |
| self) | `_PyObject_CallMethodObjArgs` | 2 | lm-studio, sourcetree |
| self) | `_PyObject_Free` | 2 | obs, sourcetree |
| self) | `_PyObject_GC_UnTrack` | 2 | lm-studio, sourcetree |
| self) | `_PyObject_GenericGetAttr` | 2 | lm-studio, obs |
| self) | `_PyObject_GetItem` | 2 | lm-studio, sourcetree |
| self) | `_PyObject_GetIter` | 2 | lm-studio, sourcetree |
| self) | `_PyObject_HasAttr` | 2 | lm-studio, sourcetree |
| self) | `_PyObject_Repr` | 2 | lm-studio, virtualbox |
| self) | `_PyObject_RichCompareBool` | 2 | lm-studio, sourcetree |
| self) | `_PyObject_SelfIter` | 2 | lm-studio, sourcetree |
| self) | `_PyObject_SetAttr` | 2 | lm-studio, obs |
| self) | `_PyObject_SetAttrString` | 2 | lm-studio, virtualbox |
| self) | `_PyObject_Str` | 2 | lm-studio, virtualbox |
| self) | `_PySequence_Check` | 2 | sourcetree, virtualbox |
| self) | `_PySequence_Size` | 2 | sourcetree, virtualbox |
| self) | `_PySet_New` | 2 | lm-studio, sourcetree |
| self) | `_PySlice_Type` | 2 | lm-studio, sourcetree |
| self) | `_PyTuple_Pack` | 2 | lm-studio, sourcetree |
| self) | `_PyTuple_Type` | 2 | lm-studio, sourcetree |
| self) | `_PyType_GetSlot` | 2 | lm-studio, obs |
| self) | `_PyType_Modified` | 2 | lm-studio, obs |
| self) | `_PyType_Ready` | 2 | lm-studio, sourcetree |
| self) | `_PyUnicode_AsUTF8AndSize` | 2 | lm-studio, virtualbox |
| self) | `_PyUnicode_AsUTF8String` | 2 | obs, virtualbox |
| self) | `_PyUnicode_Concat` | 2 | lm-studio, obs |
| self) | `_PyUnicode_FromFormat` | 2 | lm-studio, obs |
| self) | `_PyUnicode_FromStringAndSize` | 2 | lm-studio, virtualbox |
| self) | `_PyUnicode_InternFromString` | 2 | lm-studio, obs |
| self) | `_SLSMainConnectionID` | 2 | bettertouchtool, hammerspoon |
| self) | `_SLSSetWindowListWorkspace` | 2 | bettertouchtool, hammerspoon |
| self) | `_SLSSpaceSetCompatID` | 2 | bettertouchtool, hammerspoon |
| self) | `__PyDict_NewPresized` | 2 | lm-studio, sourcetree |
| self) | `__PyObject_GetDictPtr` | 2 | lm-studio, sourcetree |
| self) | `__PyObject_New` | 2 | obs, sourcetree |
| self) | `__Py_FalseStruct` | 2 | lm-studio, virtualbox |
| self) | `__Py_NotImplementedStruct` | 2 | lm-studio, obs |
| self) | `_ALCALSCopyALSServiceClient` | 1 | bettertouchtool |
| self) | `_CRYPTO_memcmp` | 1 | mongodb-compass |
| self) | `_ERR_clear_error` | 1 | mongodb-compass |
| self) | `_ERR_error_string_n` | 1 | mongodb-compass |
| self) | `_ERR_get_error` | 1 | mongodb-compass |
| self) | `_ERR_pop_to_mark` | 1 | mongodb-compass |
| self) | `_ERR_set_mark` | 1 | mongodb-compass |
| self) | `_EVP_CIPHER_CTX_ctrl` | 1 | mongodb-compass |
| self) | `_EVP_CIPHER_CTX_free` | 1 | mongodb-compass |
| self) | `_EVP_CIPHER_CTX_get0_cipher` | 1 | mongodb-compass |
| self) | `_EVP_CIPHER_CTX_new` | 1 | mongodb-compass |
| self) | `_EVP_CIPHER_CTX_set_key_length` | 1 | mongodb-compass |
| self) | `_EVP_CIPHER_CTX_set_padding` | 1 | mongodb-compass |
| self) | `_EVP_CIPHER_get_mode` | 1 | mongodb-compass |
| self) | `_EVP_DecryptFinal_ex` | 1 | mongodb-compass |
| self) | `_EVP_DecryptInit_ex` | 1 | mongodb-compass |
| self) | `_EVP_DecryptUpdate` | 1 | mongodb-compass |
| self) | `_EVP_EncryptFinal_ex` | 1 | mongodb-compass |
| self) | `_EVP_EncryptInit_ex` | 1 | mongodb-compass |
| self) | `_EVP_EncryptUpdate` | 1 | mongodb-compass |
| self) | `_EVP_MAC_CTX_dup` | 1 | mongodb-compass |
| self) | `_EVP_MAC_CTX_free` | 1 | mongodb-compass |
| self) | `_EVP_MAC_CTX_get_mac_size` | 1 | mongodb-compass |
| self) | `_EVP_MAC_CTX_new` | 1 | mongodb-compass |
| self) | `_EVP_MAC_fetch` | 1 | mongodb-compass |
| self) | `_EVP_MAC_final` | 1 | mongodb-compass |
| self) | `_EVP_MAC_free` | 1 | mongodb-compass |
| self) | `_EVP_MAC_init` | 1 | mongodb-compass |
| self) | `_EVP_MAC_update` | 1 | mongodb-compass |
| self) | `_EVP_aes_128_gcm` | 1 | mongodb-compass |
| self) | `_EVP_aes_256_gcm` | 1 | mongodb-compass |
| self) | `_EVP_get_cipherbyname` | 1 | mongodb-compass |
| self) | `_EVP_rc4` | 1 | mongodb-compass |
| self) | `_GetCurrentButtonState` | 1 | deluge |
| self) | `_Get_ng_vvp` | 1 | kicad |
| self) | `_OBJC_CLASS_$_ASCredentialIdentityStore` | 1 | bitwarden |
| self) | `_OBJC_CLASS_$_ASCredentialServiceIdentifier` | 1 | bitwarden |
| self) | `_OBJC_CLASS_$_ASPasswordCredentialIdentity` | 1 | bitwarden |
| self) | `_OBJC_CLASS_$_ASSettingsHelper` | 1 | bitwarden |
| self) | `_OBJC_CLASS_$_CBBlueLightClient` | 1 | bettertouchtool |
| self) | `_OBJC_CLASS_$_SKReceiptRefreshRequest` | 1 | termius |
| self) | `_OSSL_PARAM_construct_utf8_string` | 1 | mongodb-compass |
| self) | `_PyArg_ValidateKeywordArguments` | 1 | lm-studio |
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
| self) | `_PyCFunction_NewEx` | 1 | virtualbox |
| self) | `_PyCFunction_Type` | 1 | lm-studio |
| self) | `_PyCMethod_New` | 1 | lm-studio |
| self) | `_PyCapsule_GetName` | 1 | lm-studio |
| self) | `_PyCapsule_Import` | 1 | obs |
| self) | `_PyCapsule_IsValid` | 1 | lm-studio |
| self) | `_PyCode_NewEmpty` | 1 | lm-studio |
| self) | `_PyCode_NewWithPosOnlyArgs` | 1 | lm-studio |
| self) | `_PyDict_GetItemWithError` | 1 | lm-studio |
| self) | `_PyDict_Keys` | 1 | sourcetree |
| self) | `_PyDict_Merge` | 1 | lm-studio |
| self) | `_PyDict_SetDefault` | 1 | lm-studio |
| self) | `_PyDict_Update` | 1 | lm-studio |
| self) | `_PyErr_CheckSignals` | 1 | sourcetree |
| self) | `_PyErr_GivenExceptionMatches` | 1 | lm-studio |
| self) | `_PyErr_NewException` | 1 | sourcetree |
| self) | `_PyErr_SetFromErrno` | 1 | sourcetree |
| self) | `_PyErr_SetFromErrnoWithFilename` | 1 | sourcetree |
| self) | `_PyErr_WarnEx` | 1 | lm-studio |
| self) | `_PyErr_WarnFormat` | 1 | lm-studio |
| self) | `_PyErr_WriteUnraisable` | 1 | obs |
| self) | `_PyEval_EvalCode` | 1 | virtualbox |
| self) | `_PyExc_DeprecationWarning` | 1 | lm-studio |
| self) | `_PyExc_FutureWarning` | 1 | lm-studio |
| self) | `_PyExc_IOError` | 1 | obs |
| self) | `_PyExc_NameError` | 1 | lm-studio |
| self) | `_PyExc_RuntimeWarning` | 1 | lm-studio |
| self) | `_PyExc_UnicodeDecodeError` | 1 | sourcetree |
| self) | `_PyException_GetTraceback` | 1 | lm-studio |
| self) | `_PyException_SetTraceback` | 1 | lm-studio |
| self) | `_PyFrame_New` | 1 | lm-studio |
| self) | `_PyFrozenSet_New` | 1 | lm-studio |
| self) | `_PyFrozenSet_Type` | 1 | lm-studio |
| self) | `_PyFunction_Type` | 1 | sourcetree |
| self) | `_PyGC_Disable` | 1 | lm-studio |
| self) | `_PyGC_Enable` | 1 | lm-studio |
| self) | `_PyImport_GetModule` | 1 | lm-studio |
| self) | `_PyImport_GetModuleDict` | 1 | lm-studio |
| self) | `_PyImport_Import` | 1 | sourcetree |
| self) | `_PyImport_ImportModuleLevel` | 1 | lm-studio |
| self) | `_PyImport_ImportModuleLevelObject` | 1 | lm-studio |
| self) | `_PyInt_AsLong` | 1 | sourcetree |
| self) | `_PyInt_AsSsize_t` | 1 | sourcetree |
| self) | `_PyInt_AsUnsignedLongLongMask` | 1 | sourcetree |
| self) | `_PyInt_FromLong` | 1 | sourcetree |
| self) | `_PyInt_FromSsize_t` | 1 | sourcetree |
| self) | `_PyInt_Type` | 1 | sourcetree |
| self) | `_PyInterpreterState_GetID` | 1 | lm-studio |
| self) | `_PyList_Reverse` | 1 | sourcetree |
| self) | `_PyList_SetSlice` | 1 | sourcetree |
| self) | `_PyLong_AsDouble` | 1 | obs |
| self) | `_PyLong_AsSsize_t` | 1 | lm-studio |
| self) | `_PyLong_FromVoidPtr` | 1 | obs |
| self) | `_PyLong_Type` | 1 | lm-studio |
| self) | `_PyMapping_Check` | 1 | sourcetree |
| self) | `_PyMapping_Size` | 1 | sourcetree |
| self) | `_PyMemoryView_FromMemory` | 1 | lm-studio |
| self) | `_PyMemoryView_GetContiguous` | 1 | lm-studio |
| self) | `_PyMemoryView_Type` | 1 | lm-studio |
| self) | `_PyMethodDescr_Type` | 1 | lm-studio |
| self) | `_PyMethod_New` | 1 | lm-studio |
| self) | `_PyMethod_Type` | 1 | lm-studio |
| self) | `_PyModuleDef_Init` | 1 | lm-studio |
| self) | `_PyModule_AddStringConstant` | 1 | sourcetree |
| self) | `_PyModule_GetName` | 1 | lm-studio |
| self) | `_PyModule_GetState` | 1 | lm-studio |
| self) | `_PyModule_NewObject` | 1 | lm-studio |
| self) | `_PyNumber_Absolute` | 1 | lm-studio |
| self) | `_PyNumber_Float` | 1 | virtualbox |
| self) | `_PyNumber_InPlaceAdd` | 1 | lm-studio |
| self) | `_PyNumber_Index` | 1 | lm-studio |
| self) | `_PyNumber_Long` | 1 | virtualbox |
| self) | `_PyNumber_Or` | 1 | lm-studio |
| self) | `_PyNumber_TrueDivide` | 1 | lm-studio |
| self) | `_PyOS_double_to_string` | 1 | lm-studio |
| self) | `_PyOS_snprintf` | 1 | lm-studio |
| self) | `_PyObject_AsCharBuffer` | 1 | sourcetree |
| self) | `_PyObject_AsWriteBuffer` | 1 | virtualbox |
| self) | `_PyObject_CallFinalizerFromDealloc` | 1 | lm-studio |
| self) | `_PyObject_CallFunction` | 1 | lm-studio |
| self) | `_PyObject_CheckReadBuffer` | 1 | virtualbox |
| self) | `_PyObject_ClearWeakRefs` | 1 | lm-studio |
| self) | `_PyObject_Format` | 1 | lm-studio |
| self) | `_PyObject_GC_Del` | 1 | lm-studio |
| self) | `_PyObject_GC_IsFinalized` | 1 | lm-studio |
| self) | `_PyObject_GC_Track` | 1 | lm-studio |
| self) | `_PyObject_GenericGetDict` | 1 | lm-studio |
| self) | `_PyObject_GenericSetDict` | 1 | lm-studio |
| self) | `_PyObject_GetBuffer` | 1 | sourcetree |
| self) | `_PyObject_Hash` | 1 | lm-studio |
| self) | `_PyObject_HashNotImplemented` | 1 | lm-studio |
| self) | `_PyObject_Init` | 1 | virtualbox |
| self) | `_PyObject_IsInstance` | 1 | obs |
| self) | `_PyObject_RichCompare` | 1 | lm-studio |
| self) | `_PyObject_SetItem` | 1 | lm-studio |
| self) | `_PyObject_Size` | 1 | lm-studio |
| self) | `_PyObject_Vectorcall` | 1 | lm-studio |
| self) | `_PyObject_VectorcallDict` | 1 | lm-studio |
| self) | `_PyObject_VectorcallMethod` | 1 | lm-studio |
| self) | `_PySequence_Contains` | 1 | lm-studio |
| self) | `_PySequence_Fast` | 1 | lm-studio |
| self) | `_PySequence_List` | 1 | lm-studio |
| self) | `_PySequence_SetItem` | 1 | lm-studio |
| self) | `_PySet_Add` | 1 | sourcetree |
| self) | `_PySet_Size` | 1 | lm-studio |
| self) | `_PySet_Type` | 1 | lm-studio |
| self) | `_PySlice_AdjustIndices` | 1 | lm-studio |
| self) | `_PySlice_GetIndicesEx` | 1 | sourcetree |
| self) | `_PySlice_New` | 1 | lm-studio |
| self) | `_PySlice_Unpack` | 1 | lm-studio |
| self) | `_PyState_FindModule` | 1 | lm-studio |
| self) | `_PyString_AsString` | 1 | sourcetree |
| self) | `_PyString_AsStringAndSize` | 1 | sourcetree |
| self) | `_PyString_FromString` | 1 | sourcetree |
| self) | `_PyString_FromStringAndSize` | 1 | sourcetree |
| self) | `_PyString_Size` | 1 | sourcetree |
| self) | `_PyString_Type` | 1 | sourcetree |
| self) | `_PySys_GetObject` | 1 | virtualbox |
| self) | `_PySys_SetObject` | 1 | virtualbox |
| self) | `_PyThreadState_Get` | 1 | lm-studio |
| self) | `_PyTraceBack_Here` | 1 | lm-studio |
| self) | `_PyType_FromModuleAndSpec` | 1 | lm-studio |
| self) | `_PyType_FromSpecWithBases` | 1 | lm-studio |
| self) | `_PyType_GenericAlloc` | 1 | lm-studio |
| self) | `_PyType_GenericNew` | 1 | sourcetree |
| self) | `_PyUnicodeDecodeError_Create` | 1 | sourcetree |
| self) | `_PyUnicodeUCS2_AsEncodedString` | 1 | sourcetree |
| self) | `_PyUnicodeUCS2_AsUTF8String` | 1 | sourcetree |
| self) | `_PyUnicodeUCS2_Decode` | 1 | sourcetree |
| self) | `_PyUnicodeUCS2_FromStringAndSize` | 1 | sourcetree |
| self) | `_PyUnicode_AsUTF16String` | 1 | virtualbox |
| self) | `_PyUnicode_Compare` | 1 | lm-studio |
| self) | `_PyUnicode_CompareWithASCIIString` | 1 | obs |
| self) | `_PyUnicode_Contains` | 1 | lm-studio |
| self) | `_PyUnicode_DecodeUTF16` | 1 | virtualbox |
| self) | `_PyUnicode_FindChar` | 1 | lm-studio |
| self) | `_PyUnicode_FromEncodedObject` | 1 | lm-studio |
| self) | `_PyUnicode_FromKindAndData` | 1 | lm-studio |
| self) | `_PyUnicode_FromObject` | 1 | virtualbox |
| self) | `_PyUnicode_FromOrdinal` | 1 | lm-studio |
| self) | `_PyUnicode_GetLength` | 1 | virtualbox |
| self) | `_PyUnicode_InternInPlace` | 1 | lm-studio |
| self) | `_PyUnicode_Join` | 1 | lm-studio |
| self) | `_PyUnicode_New` | 1 | lm-studio |
| self) | `_PyUnicode_ReadChar` | 1 | virtualbox |
| self) | `_PyUnicode_Resize` | 1 | lm-studio |
| self) | `_PyUnicode_Split` | 1 | lm-studio |
| self) | `_PyUnicode_Substring` | 1 | lm-studio |
| self) | `_PyUnicode_Tailmatch` | 1 | lm-studio |
| self) | `_PyUnicode_Type` | 1 | lm-studio |
| self) | `_Py_CompileString` | 1 | virtualbox |
| self) | `_Py_DecRef` | 1 | obs |
| self) | `_Py_EnterRecursiveCall` | 1 | lm-studio |
| self) | `_Py_GenericAlias` | 1 | lm-studio |
| self) | `_Py_GetArgcArgv` | 1 | sourcetree |
| self) | `_Py_GetProgramFullPath` | 1 | sourcetree |
| self) | `_Py_GetVersion` | 1 | sourcetree |
| self) | `_Py_InitModule4_64` | 1 | sourcetree |
| self) | `_Py_Initialize` | 1 | virtualbox |
| self) | `_Py_IsInitialized` | 1 | virtualbox |
| self) | `_Py_LeaveRecursiveCall` | 1 | lm-studio |
| self) | `_Py_MakePendingCalls` | 1 | virtualbox |
| self) | `_Py_VaBuildValue` | 1 | virtualbox |
| self) | `_Py_Version` | 1 | lm-studio |
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
| self) | `_SetSystemUIMode` | 1 | deluge |
| self) | `_WacomMTGetAttachedDeviceIDs` | 1 | bettertouchtool |
| self) | `_WacomMTInitialize` | 1 | bettertouchtool |
| self) | `_WacomMTRegisterAttachCallback` | 1 | bettertouchtool |
| self) | `_WacomMTRegisterFingerReadCallback` | 1 | bettertouchtool |
| self) | `__PyArg_ParseTupleAndKeywords_SizeT` | 1 | sourcetree |
| self) | `__PyArg_ParseTuple_SizeT` | 1 | sourcetree |
| self) | `__PyByteArray_empty_string` | 1 | lm-studio |
| self) | `__PyDict_GetItem_KnownHash` | 1 | lm-studio |
| self) | `__PyLong_Copy` | 1 | lm-studio |
| self) | `__PyObject_CallFunction_SizeT` | 1 | sourcetree |
| self) | `__PyObject_CallMethod_SizeT` | 1 | sourcetree |
| self) | `__PyObject_GC_New` | 1 | lm-studio |
| self) | `__PyObject_GenericGetAttrWithDict` | 1 | lm-studio |
| self) | `__PySet_NextEntry` | 1 | lm-studio |
| self) | `__PyString_Resize` | 1 | sourcetree |
| self) | `__PyThreadState_UncheckedGet` | 1 | lm-studio |
| self) | `__PyType_Lookup` | 1 | lm-studio |
| self) | `__PyUnicode_FastCopyCharacters` | 1 | lm-studio |
| self) | `__PyUnicode_Ready` | 1 | lm-studio |
| self) | `__Py_BuildValue_SizeT` | 1 | sourcetree |
| self) | `__Py_ZeroStruct` | 1 | sourcetree |
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
| self) | `__ZN2v811HandleScope12CreateHandleEPNS_8internal7IsolateEm` | 1 | mongodb-compass |
| self) | `__ZN2v811HandleScopeC1EPNS_7IsolateE` | 1 | mongodb-compass |
| self) | `__ZN2v811HandleScopeD1Ev` | 1 | mongodb-compass |
| self) | `__ZN2v811HandleScopeD2Ev` | 1 | mongodb-compass |
| self) | `__ZN2v86Object32SetAlignedPointerInInternalFieldEiPv` | 1 | mongodb-compass |
| self) | `__ZN2v86Object38SlowGetAlignedPointerFromInternalFieldEi` | 1 | mongodb-compass |
| self) | `__ZN2v87Context10GetIsolateEv` | 1 | mongodb-compass |
| self) | `__ZN2v88External3NewEPNS_7IsolateEPv` | 1 | mongodb-compass |
| self) | `__ZN2wi11lts_p_largeEPKxjjS1_j` | 1 | emacs-app |
| self) | `__ZNK2v86String11WriteUtf8V2EPNS_7IsolateEPcmiPm` | 1 | mongodb-compass |
| self) | `__ZNK2v88External5ValueEv` | 1 | mongodb-compass |
| self) | `__ZdlPvSt19__type_descriptor_t` | 1 | utm |
| self) | `___divdc3` | 1 | kicad |
| self) | `_c_binding_oracle` | 1 | emacs-app |
| self) | `_c_global_trees` | 1 | emacs-app |
| self) | `_cfun` | 1 | emacs-app |
| self) | `_cp_binding_oracle` | 1 | emacs-app |
| self) | `_cp_global_trees` | 1 | emacs-app |
| self) | `_csinh` | 1 | kicad |
| self) | `_ctanh` | 1 | kicad |
| self) | `_current_function_decl` | 1 | emacs-app |
| self) | `_fmemopen` | 1 | insomnia |
| self) | `_function_depth` | 1 | emacs-app |
| self) | `_gcry_check_version` | 1 | vagrant |
| self) | `_gcry_cipher_close` | 1 | vagrant |
| self) | `_gcry_cipher_decrypt` | 1 | vagrant |
| self) | `_gcry_cipher_encrypt` | 1 | vagrant |
| self) | `_gcry_cipher_open` | 1 | vagrant |
| self) | `_gcry_cipher_setkey` | 1 | vagrant |
| self) | `_gcry_md_hash_buffer` | 1 | vagrant |
| self) | `_gcry_strerror` | 1 | vagrant |
| self) | `_global_trees` | 1 | emacs-app |
| self) | `_input_location` | 1 | emacs-app |
| self) | `_integer_types` | 1 | emacs-app |
| self) | `_lang_hooks` | 1 | emacs-app |
| self) | `_line_table` | 1 | emacs-app |
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
| self) | `_ovl_op_info` | 1 | emacs-app |
| self) | `_ovl_op_mapping` | 1 | emacs-app |
| self) | `_prime_tab` | 1 | emacs-app |
| self) | `_pthread_getconcurrency` | 1 | insomnia |
| self) | `_pthread_setcanceltype` | 1 | libreoffice |
| self) | `_pthread_setconcurrency` | 1 | insomnia |
| self) | `_register_callback` | 1 | emacs-app |
| self) | `_ridpointers` | 1 | emacs-app |
| self) | `_scope_chain` | 1 | emacs-app |
| self) | `_tree_contains_struct` | 1 | emacs-app |
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
