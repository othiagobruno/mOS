# Lacunas de Leonardo (runtime C/C++)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Leonardo (runtime C/C++) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**552** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libSystem.B.dylib | `_getprotobyname` | 4 | firefox, gimp, inkscape, vlc |
| libSystem.B.dylib | `_getsectdatafromheader_64` | 3 | alt-tab, firefox, inkscape |
| libSystem.B.dylib | `_if_freenameindex` | 3 | gimp, inkscape, vlc |
| libSystem.B.dylib | `_if_nameindex` | 3 | gimp, inkscape, vlc |
| libSystem.B.dylib | `_putenv` | 3 | firefox, gimp, vlc |
| libcups.2.dylib | `_httpGetHostname` | 3 | firefox, gimp, inkscape |
| libSystem.B.dylib | `_OSAtomicAdd32Barrier` | 2 | firefox, vlc |
| libSystem.B.dylib | `_csqrt` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_csqrtf` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_csqrtl` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_ctermid_r` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_dbm_clearerr` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_dbm_close` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_dbm_delete` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_dbm_error` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_dbm_fetch` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_dbm_firstkey` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_dbm_nextkey` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_dbm_open` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_dbm_store` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_drand48` | 2 | gimp, iterm2 |
| libSystem.B.dylib | `_exc_server` | 2 | inkscape, vlc |
| libSystem.B.dylib | `_gethostbyname2` | 2 | firefox, gimp |
| libSystem.B.dylib | `_getloadavg` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_getprotobynumber` | 2 | firefox, gimp |
| libSystem.B.dylib | `_hash_create` | 2 | alt-tab, vlc |
| libSystem.B.dylib | `_hash_search` | 2 | alt-tab, vlc |
| libSystem.B.dylib | `_hstrerror` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_sethostname` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_setpgrp` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_uuid_generate_time` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_wcsrchr` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_wcstok` | 2 | gimp, inkscape |
| libSystem.B.dylib | `_wcsxfrm` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsArrayFirst` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsDoAuthentication` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsEncryption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsFreeOptions` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsLangDefault` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsLangFree` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsParseOptions` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsSetEncryption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsSetPasswordCB` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsSetUser` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpAddrFreeList` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpAddrGetList` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpAddrLength` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpAssembleURIf` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpBlocking` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpClearFields` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpEncryption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpError` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpFlush` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpGet` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpGetAuthString` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpGetFd` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpGetField` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpGetLength2` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpPost` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpRead2` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpReconnect2` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpSeparateURI` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpSetField` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpUpdate` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpWait` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpWrite2` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippAddResolution` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippErrorString` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippGetBoolean` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippGetOperation` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippGetValueTag` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippLength` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippRead` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippSetOperation` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippSetRequestId` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippSetState` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippWrite` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdClose` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdConflicts` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdFindAttr` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdFindChoice` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdFindCustomOption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdFindOption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdFirstCustomParam` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdLocalize` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdMarkDefaults` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdMarkOption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdNextCustomParam` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdOpenFd` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdPageSize` | 2 | gimp, inkscape |
| libncurses.5.4.dylib | `_setupterm` | 2 | inkscape, iterm2 |
| libncurses.5.4.dylib | `_tigetflag` | 2 | inkscape, iterm2 |
| libncurses.5.4.dylib | `_tigetnum` | 2 | inkscape, iterm2 |
| libncurses.5.4.dylib | `_tigetstr` | 2 | inkscape, iterm2 |
| Tcl.framework | `_TclBN_mp_clear` | 1 | inkscape |
| Tcl.framework | `_TclBN_mp_init` | 1 | inkscape |
| Tcl.framework | `_TclBN_mp_read_radix` | 1 | inkscape |
| Tcl.framework | `_TclBN_mp_to_unsigned_bin_n` | 1 | inkscape |
| Tcl.framework | `_TclBN_mp_unsigned_bin_size` | 1 | inkscape |
| Tcl.framework | `_TclFreeObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_AddErrorInfo` | 1 | inkscape |
| Tcl.framework | `_Tcl_AttemptAlloc` | 1 | inkscape |
| Tcl.framework | `_Tcl_ConditionFinalize` | 1 | inkscape |
| Tcl.framework | `_Tcl_ConditionNotify` | 1 | inkscape |
| Tcl.framework | `_Tcl_ConditionWait` | 1 | inkscape |
| Tcl.framework | `_Tcl_CreateFileHandler` | 1 | inkscape |
| Tcl.framework | `_Tcl_CreateInterp` | 1 | inkscape |
| Tcl.framework | `_Tcl_CreateObjCommand` | 1 | inkscape |
| Tcl.framework | `_Tcl_CreateTimerHandler` | 1 | inkscape |
| Tcl.framework | `_Tcl_DeleteCommand` | 1 | inkscape |
| Tcl.framework | `_Tcl_DeleteFileHandler` | 1 | inkscape |
| Tcl.framework | `_Tcl_DeleteInterp` | 1 | inkscape |
| Tcl.framework | `_Tcl_DeleteTimerHandler` | 1 | inkscape |
| Tcl.framework | `_Tcl_DoOneEvent` | 1 | inkscape |
| Tcl.framework | `_Tcl_Eval` | 1 | inkscape |
| Tcl.framework | `_Tcl_EvalFile` | 1 | inkscape |
| Tcl.framework | `_Tcl_EvalObjv` | 1 | inkscape |
| Tcl.framework | `_Tcl_ExprBoolean` | 1 | inkscape |
| Tcl.framework | `_Tcl_ExprDouble` | 1 | inkscape |
| Tcl.framework | `_Tcl_ExprLong` | 1 | inkscape |
| Tcl.framework | `_Tcl_ExprString` | 1 | inkscape |
| Tcl.framework | `_Tcl_FindExecutable` | 1 | inkscape |
| Tcl.framework | `_Tcl_Free` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetBignumFromObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetBoolean` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetBooleanFromObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetByteArrayFromObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetCurrentThread` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetDouble` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetDoubleFromObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetLongFromObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetObjResult` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetObjType` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetString` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetStringFromObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetStringResult` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetThreadData` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetVar` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetVar2Ex` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetWideIntFromObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_Init` | 1 | inkscape |
| Tcl.framework | `_Tcl_ListObjGetElements` | 1 | inkscape |
| Tcl.framework | `_Tcl_ListObjIndex` | 1 | inkscape |
| Tcl.framework | `_Tcl_ListObjLength` | 1 | inkscape |
| Tcl.framework | `_Tcl_MutexLock` | 1 | inkscape |
| Tcl.framework | `_Tcl_MutexUnlock` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewBignumObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewBooleanObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewByteArrayObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewDoubleObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewListObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewLongObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewStringObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewWideIntObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_RecordAndEval` | 1 | inkscape |
| Tcl.framework | `_Tcl_ResetResult` | 1 | inkscape |
| Tcl.framework | `_Tcl_SetObjResult` | 1 | inkscape |
| Tcl.framework | `_Tcl_SetVar` | 1 | inkscape |
| Tcl.framework | `_Tcl_SetVar2` | 1 | inkscape |
| Tcl.framework | `_Tcl_SetVar2Ex` | 1 | inkscape |
| Tcl.framework | `_Tcl_SplitList` | 1 | inkscape |
| Tcl.framework | `_Tcl_ThreadAlert` | 1 | inkscape |
| Tcl.framework | `_Tcl_ThreadQueueEvent` | 1 | inkscape |
| Tcl.framework | `_Tcl_UnsetVar2` | 1 | inkscape |
| Tk.framework | `_Tk_GetNumMainWindows` | 1 | inkscape |
| Tk.framework | `_Tk_Init` | 1 | inkscape |
| Tk.framework | `_Tk_MainWindow` | 1 | inkscape |
| libSystem.B.dylib | `_NSLibraryNameForModule` | 1 | inkscape |
| libSystem.B.dylib | `_NSLookupAndBindSymbol` | 1 | inkscape |
| libSystem.B.dylib | `_NSModuleForSymbol` | 1 | inkscape |
| libSystem.B.dylib | `_NXGetArchInfoFromName` | 1 | vlc |
| libSystem.B.dylib | `_OSAtomicCompareAndSwapIntBarrier` | 1 | firefox |
| libSystem.B.dylib | `_OSSpinLockLock` | 1 | alt-tab |
| libSystem.B.dylib | `_OSSpinLockUnlock` | 1 | alt-tab |
| libSystem.B.dylib | `___invert_d2` | 1 | mos |
| libSystem.B.dylib | `___invert_d3` | 1 | mos |
| libSystem.B.dylib | `___invert_d4` | 1 | mos |
| libSystem.B.dylib | `___invert_f2` | 1 | mos |
| libSystem.B.dylib | `___invert_f4` | 1 | mos |
| libSystem.B.dylib | `__keymgr_get_and_lock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__keymgr_set_and_unlock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__keymgr_unlock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__os_activity_initiate` | 1 | alt-tab |
| libSystem.B.dylib | `__os_trace_with_buffer` | 1 | alt-tab |
| libSystem.B.dylib | `_cabs` | 1 | inkscape |
| libSystem.B.dylib | `_cabsf` | 1 | inkscape |
| libSystem.B.dylib | `_cabsl` | 1 | inkscape |
| libSystem.B.dylib | `_cacos` | 1 | inkscape |
| libSystem.B.dylib | `_cacosf` | 1 | inkscape |
| libSystem.B.dylib | `_cacosh` | 1 | inkscape |
| libSystem.B.dylib | `_cacoshf` | 1 | inkscape |
| libSystem.B.dylib | `_cacoshl` | 1 | inkscape |
| libSystem.B.dylib | `_cacosl` | 1 | inkscape |
| libSystem.B.dylib | `_carg` | 1 | inkscape |
| libSystem.B.dylib | `_cargf` | 1 | inkscape |
| libSystem.B.dylib | `_cargl` | 1 | inkscape |
| libSystem.B.dylib | `_casin` | 1 | inkscape |
| libSystem.B.dylib | `_casinf` | 1 | inkscape |
| libSystem.B.dylib | `_casinh` | 1 | inkscape |
| libSystem.B.dylib | `_casinhf` | 1 | inkscape |
| libSystem.B.dylib | `_casinhl` | 1 | inkscape |
| libSystem.B.dylib | `_casinl` | 1 | inkscape |
| libSystem.B.dylib | `_catan` | 1 | inkscape |
| libSystem.B.dylib | `_catanf` | 1 | inkscape |
| libSystem.B.dylib | `_catanh` | 1 | inkscape |
| libSystem.B.dylib | `_catanhf` | 1 | inkscape |
| libSystem.B.dylib | `_catanhl` | 1 | inkscape |
| libSystem.B.dylib | `_catanl` | 1 | inkscape |
| libSystem.B.dylib | `_ccos` | 1 | inkscape |
| libSystem.B.dylib | `_ccosf` | 1 | inkscape |
| libSystem.B.dylib | `_ccosh` | 1 | inkscape |
| libSystem.B.dylib | `_ccoshf` | 1 | inkscape |
| libSystem.B.dylib | `_ccoshl` | 1 | inkscape |
| libSystem.B.dylib | `_ccosl` | 1 | inkscape |
| libSystem.B.dylib | `_cexp` | 1 | inkscape |
| libSystem.B.dylib | `_cexpf` | 1 | inkscape |
| libSystem.B.dylib | `_cexpl` | 1 | inkscape |
| libSystem.B.dylib | `_clnt_create` | 1 | inkscape |
| libSystem.B.dylib | `_clnt_spcreateerror` | 1 | inkscape |
| libSystem.B.dylib | `_clog` | 1 | inkscape |
| libSystem.B.dylib | `_clogf` | 1 | inkscape |
| libSystem.B.dylib | `_clogl` | 1 | inkscape |
| libSystem.B.dylib | `_cpow` | 1 | inkscape |
| libSystem.B.dylib | `_cpowf` | 1 | inkscape |
| libSystem.B.dylib | `_cpowl` | 1 | inkscape |
| libSystem.B.dylib | `_csin` | 1 | inkscape |
| libSystem.B.dylib | `_csinf` | 1 | inkscape |
| libSystem.B.dylib | `_csinh` | 1 | inkscape |
| libSystem.B.dylib | `_csinhf` | 1 | inkscape |
| libSystem.B.dylib | `_csinhl` | 1 | inkscape |
| libSystem.B.dylib | `_csinl` | 1 | inkscape |
| libSystem.B.dylib | `_ctan` | 1 | inkscape |
| libSystem.B.dylib | `_ctanf` | 1 | inkscape |
| libSystem.B.dylib | `_ctanh` | 1 | inkscape |
| libSystem.B.dylib | `_ctanhf` | 1 | inkscape |
| libSystem.B.dylib | `_ctanhl` | 1 | inkscape |
| libSystem.B.dylib | `_ctanl` | 1 | inkscape |
| libSystem.B.dylib | `_daemon$1050` | 1 | vlc |
| libSystem.B.dylib | `_duplocale` | 1 | gimp |
| libSystem.B.dylib | `_endhostent` | 1 | vlc |
| libSystem.B.dylib | `_erand48` | 1 | vlc |
| libSystem.B.dylib | `_fmemopen` | 1 | gimp |
| libSystem.B.dylib | `_funopen` | 1 | gimp |
| libSystem.B.dylib | `_get_end` | 1 | inkscape |
| libSystem.B.dylib | `_get_etext` | 1 | inkscape |
| libSystem.B.dylib | `_getsectbynamefromheader_64` | 1 | inkscape |
| libSystem.B.dylib | `_getsectdatafromheader` | 1 | alt-tab |
| libSystem.B.dylib | `_jrand48` | 1 | vlc |
| libSystem.B.dylib | `_nrand48` | 1 | vlc |
| libSystem.B.dylib | `_os_trace_debug_enabled` | 1 | alt-tab |
| libSystem.B.dylib | `_pthread_key_init_np` | 1 | mos |
| libSystem.B.dylib | `_pthread_rwlockattr_destroy` | 1 | firefox |
| libSystem.B.dylib | `_pthread_rwlockattr_init` | 1 | firefox |
| libSystem.B.dylib | `_pthread_setcanceltype` | 1 | inkscape |
| libSystem.B.dylib | `_putwc` | 1 | inkscape |
| libSystem.B.dylib | `_regcomp_l` | 1 | iterm2 |
| libSystem.B.dylib | `_sethostent` | 1 | vlc |
| libSystem.B.dylib | `_strunvis` | 1 | gimp |
| libSystem.B.dylib | `_strvis` | 1 | gimp |
| libSystem.B.dylib | `_swab` | 1 | vlc |
| libSystem.B.dylib | `_sys_errlist` | 1 | vlc |
| libSystem.B.dylib | `_tempnam` | 1 | vlc |
| libSystem.B.dylib | `_vfprintf_l` | 1 | inkscape |
| libSystem.B.dylib | `_vis` | 1 | gimp |
| libSystem.B.dylib | `_wcscspn` | 1 | gimp |
| libSystem.B.dylib | `_wcsncat` | 1 | inkscape |
| libSystem.B.dylib | `_wcsstr` | 1 | gimp |
| libSystem.B.dylib | `_xdr_enum` | 1 | inkscape |
| libSystem.B.dylib | `_xdr_pointer` | 1 | inkscape |
| libSystem.B.dylib | `_xdr_string` | 1 | inkscape |
| libSystem.B.dylib | `_yp_all` | 1 | inkscape |
| libSystem.B.dylib | `_yp_get_default_domain` | 1 | inkscape |
| libSystem.B.dylib | `_yp_master` | 1 | inkscape |
| libSystem.B.dylib | `_yp_match` | 1 | inkscape |
| libSystem.B.dylib | `_yperr_string` | 1 | inkscape |
| libaprutil-1.0.dylib | `_apr_base64_decode` | 1 | iterm2 |
| libaprutil-1.0.dylib | `_apr_base64_decode_len` | 1 | iterm2 |
| libaprutil-1.0.dylib | `_apr_base64_encode_binary` | 1 | iterm2 |
| libaprutil-1.0.dylib | `_apr_base64_encode_len` | 1 | iterm2 |
| libbz2.1.0.dylib | `_BZ2_bzclose` | 1 | inkscape |
| libbz2.1.0.dylib | `_BZ2_bzerror` | 1 | inkscape |
| libbz2.1.0.dylib | `_BZ2_bzflush` | 1 | inkscape |
| libbz2.1.0.dylib | `_BZ2_bzopen` | 1 | inkscape |
| libbz2.1.0.dylib | `_BZ2_bzread` | 1 | inkscape |
| libbz2.1.0.dylib | `_BZ2_bzwrite` | 1 | inkscape |
| libcups.2.dylib | `_cupsConnectDest` | 1 | firefox |
| libcups.2.dylib | `_cupsGetDestMediaByIndex` | 1 | firefox |
| libcups.2.dylib | `_cupsGetDestMediaCount` | 1 | firefox |
| libcups.2.dylib | `_cupsGetDestMediaDefault` | 1 | firefox |
| libcups.2.dylib | `_cupsGetDests` | 1 | firefox |
| libcups.2.dylib | `_cupsGetDests2` | 1 | firefox |
| libcups.2.dylib | `_cupsLocalizeDestMedia` | 1 | firefox |
| libcups.2.dylib | `_httpAddrPort` | 1 | firefox |
| libcups.2.dylib | `_httpGetAddress` | 1 | firefox |
| libcups.2.dylib | `_ppdOpenFile` | 1 | inkscape |
| libcupsimage.2.dylib | `_cupsRasterClose` | 1 | inkscape |
| libcupsimage.2.dylib | `_cupsRasterOpen` | 1 | inkscape |
| libcupsimage.2.dylib | `_cupsRasterWriteHeader2` | 1 | inkscape |
| libcupsimage.2.dylib | `_cupsRasterWritePixels` | 1 | inkscape |
| libexpat.1.dylib | `_XML_ErrorString` | 1 | inkscape |
| libexpat.1.dylib | `_XML_GetBuffer` | 1 | inkscape |
| libexpat.1.dylib | `_XML_GetCurrentLineNumber` | 1 | inkscape |
| libexpat.1.dylib | `_XML_GetErrorCode` | 1 | inkscape |
| libexpat.1.dylib | `_XML_ParseBuffer` | 1 | inkscape |
| libexpat.1.dylib | `_XML_ParserCreate` | 1 | inkscape |
| libexpat.1.dylib | `_XML_ParserFree` | 1 | inkscape |
| libexpat.1.dylib | `_XML_SetCharacterDataHandler` | 1 | inkscape |
| libexpat.1.dylib | `_XML_SetDoctypeDeclHandler` | 1 | inkscape |
| libexpat.1.dylib | `_XML_SetElementHandler` | 1 | inkscape |
| libexpat.1.dylib | `_XML_SetUserData` | 1 | inkscape |
| libexslt.0.dylib | `_exsltDateXpathCtxtRegister` | 1 | inkscape |
| libexslt.0.dylib | `_exsltMathXpathCtxtRegister` | 1 | inkscape |
| libexslt.0.dylib | `_exsltRegisterAll` | 1 | inkscape |
| libexslt.0.dylib | `_exsltSetsXpathCtxtRegister` | 1 | inkscape |
| libexslt.0.dylib | `_exsltStrXpathCtxtRegister` | 1 | inkscape |
| libicucore.A.dylib | `_u_charAge` | 1 | mos |
| libicucore.A.dylib | `_u_charName` | 1 | mos |
| libicucore.A.dylib | `_u_errorName` | 1 | iterm2 |
| libicucore.A.dylib | `_u_getNumericValue` | 1 | mos |
| libicucore.A.dylib | `_u_strToLower` | 1 | mos |
| libicucore.A.dylib | `_u_strToTitle` | 1 | mos |
| libicucore.A.dylib | `_u_strToUpper` | 1 | mos |
| libicucore.A.dylib | `_u_strlen` | 1 | iterm2 |
| libicucore.A.dylib | `_ubrk_close` | 1 | mos |
| libicucore.A.dylib | `_ubrk_following` | 1 | mos |
| libicucore.A.dylib | `_ubrk_open` | 1 | mos |
| libicucore.A.dylib | `_ubrk_preceding` | 1 | mos |
| libicucore.A.dylib | `_ubrk_setText` | 1 | mos |
| libicucore.A.dylib | `_ubrk_setUText` | 1 | mos |
| libicucore.A.dylib | `_unorm2_getNFCInstance` | 1 | mos |
| libicucore.A.dylib | `_unorm2_hasBoundaryBefore` | 1 | mos |
| libicucore.A.dylib | `_unorm2_normalize` | 1 | mos |
| libicucore.A.dylib | `_unorm2_spanQuickCheckYes` | 1 | mos |
| libicucore.A.dylib | `_uregex_appendReplacement` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_appendTail` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_clone` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_close` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_end` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_find` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_findNext` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_groupCount` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_open` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_reset` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_setText` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_start` | 1 | iterm2 |
| libicucore.A.dylib | `_utext_openUChars` | 1 | mos |
| libicucore.A.dylib | `_utext_openUTF8` | 1 | mos |
| libncurses.5.4.dylib | `_BC` | 1 | inkscape |
| libncurses.5.4.dylib | `_COLORS` | 1 | inkscape |
| libncurses.5.4.dylib | `_COLOR_PAIRS` | 1 | inkscape |
| libncurses.5.4.dylib | `_COLS` | 1 | inkscape |
| libncurses.5.4.dylib | `_ESCDELAY` | 1 | inkscape |
| libncurses.5.4.dylib | `_LINES` | 1 | inkscape |
| libncurses.5.4.dylib | `_PC` | 1 | inkscape |
| libncurses.5.4.dylib | `_TABSIZE` | 1 | inkscape |
| libncurses.5.4.dylib | `_UP` | 1 | inkscape |
| libncurses.5.4.dylib | `_acs_map` | 1 | inkscape |
| libncurses.5.4.dylib | `_baudrate` | 1 | inkscape |
| libncurses.5.4.dylib | `_beep` | 1 | inkscape |
| libncurses.5.4.dylib | `_boolnames` | 1 | iterm2 |
| libncurses.5.4.dylib | `_can_change_color` | 1 | inkscape |
| libncurses.5.4.dylib | `_cbreak` | 1 | inkscape |
| libncurses.5.4.dylib | `_clearok` | 1 | inkscape |
| libncurses.5.4.dylib | `_color_content` | 1 | inkscape |
| libncurses.5.4.dylib | `_copywin` | 1 | inkscape |
| libncurses.5.4.dylib | `_cur_term` | 1 | iterm2 |
| libncurses.5.4.dylib | `_curs_set` | 1 | inkscape |
| libncurses.5.4.dylib | `_def_prog_mode` | 1 | inkscape |
| libncurses.5.4.dylib | `_def_shell_mode` | 1 | inkscape |
| libncurses.5.4.dylib | `_delay_output` | 1 | inkscape |
| libncurses.5.4.dylib | `_delwin` | 1 | inkscape |
| libncurses.5.4.dylib | `_derwin` | 1 | inkscape |
| libncurses.5.4.dylib | `_doupdate` | 1 | inkscape |
| libncurses.5.4.dylib | `_echo` | 1 | inkscape |
| libncurses.5.4.dylib | `_endwin` | 1 | inkscape |
| libncurses.5.4.dylib | `_erasechar` | 1 | inkscape |
| libncurses.5.4.dylib | `_filter` | 1 | inkscape |
| libncurses.5.4.dylib | `_flash` | 1 | inkscape |
| libncurses.5.4.dylib | `_flushinp` | 1 | inkscape |
| libncurses.5.4.dylib | `_getmouse` | 1 | inkscape |
| libncurses.5.4.dylib | `_getwin` | 1 | inkscape |
| libncurses.5.4.dylib | `_halfdelay` | 1 | inkscape |
| libncurses.5.4.dylib | `_has_colors` | 1 | inkscape |
| libncurses.5.4.dylib | `_has_ic` | 1 | inkscape |
| libncurses.5.4.dylib | `_has_il` | 1 | inkscape |
| libncurses.5.4.dylib | `_has_key` | 1 | inkscape |
| libncurses.5.4.dylib | `_idcok` | 1 | inkscape |
| libncurses.5.4.dylib | `_idlok` | 1 | inkscape |
| libncurses.5.4.dylib | `_immedok` | 1 | inkscape |
| libncurses.5.4.dylib | `_init_color` | 1 | inkscape |
| libncurses.5.4.dylib | `_init_pair` | 1 | inkscape |
| libncurses.5.4.dylib | `_initscr` | 1 | inkscape |
| libncurses.5.4.dylib | `_intrflush` | 1 | inkscape |
| libncurses.5.4.dylib | `_is_linetouched` | 1 | inkscape |
| libncurses.5.4.dylib | `_is_term_resized` | 1 | inkscape |
| libncurses.5.4.dylib | `_is_wintouched` | 1 | inkscape |
| libncurses.5.4.dylib | `_isendwin` | 1 | inkscape |
| libncurses.5.4.dylib | `_keyname` | 1 | inkscape |
| libncurses.5.4.dylib | `_keypad` | 1 | inkscape |
| libncurses.5.4.dylib | `_killchar` | 1 | inkscape |
| libncurses.5.4.dylib | `_leaveok` | 1 | inkscape |
| libncurses.5.4.dylib | `_longname` | 1 | inkscape |
| libncurses.5.4.dylib | `_meta` | 1 | inkscape |
| libncurses.5.4.dylib | `_mouseinterval` | 1 | inkscape |
| libncurses.5.4.dylib | `_mousemask` | 1 | inkscape |
| libncurses.5.4.dylib | `_mvderwin` | 1 | inkscape |
| libncurses.5.4.dylib | `_mvwin` | 1 | inkscape |
| libncurses.5.4.dylib | `_napms` | 1 | inkscape |
| libncurses.5.4.dylib | `_newpad` | 1 | inkscape |
| libncurses.5.4.dylib | `_newscr` | 1 | inkscape |
| libncurses.5.4.dylib | `_newwin` | 1 | inkscape |
| libncurses.5.4.dylib | `_nl` | 1 | inkscape |
| libncurses.5.4.dylib | `_nocbreak` | 1 | inkscape |
| libncurses.5.4.dylib | `_nodelay` | 1 | inkscape |
| libncurses.5.4.dylib | `_noecho` | 1 | inkscape |
| libncurses.5.4.dylib | `_nonl` | 1 | inkscape |
| libncurses.5.4.dylib | `_noqiflush` | 1 | inkscape |
| libncurses.5.4.dylib | `_noraw` | 1 | inkscape |
| libncurses.5.4.dylib | `_notimeout` | 1 | inkscape |
| libncurses.5.4.dylib | `_numnames` | 1 | iterm2 |
| libncurses.5.4.dylib | `_overlay` | 1 | inkscape |
| libncurses.5.4.dylib | `_overwrite` | 1 | inkscape |
| libncurses.5.4.dylib | `_pair_content` | 1 | inkscape |
| libncurses.5.4.dylib | `_pechochar` | 1 | inkscape |
| libncurses.5.4.dylib | `_pnoutrefresh` | 1 | inkscape |
| libncurses.5.4.dylib | `_prefresh` | 1 | inkscape |
| libncurses.5.4.dylib | `_putp` | 1 | inkscape |
| libncurses.5.4.dylib | `_putwin` | 1 | inkscape |
| libncurses.5.4.dylib | `_qiflush` | 1 | inkscape |
| libncurses.5.4.dylib | `_raw` | 1 | inkscape |
| libncurses.5.4.dylib | `_reset_prog_mode` | 1 | inkscape |
| libncurses.5.4.dylib | `_reset_shell_mode` | 1 | inkscape |
| libncurses.5.4.dylib | `_resetty` | 1 | inkscape |
| libncurses.5.4.dylib | `_resize_term` | 1 | inkscape |
| libncurses.5.4.dylib | `_resizeterm` | 1 | inkscape |
| libncurses.5.4.dylib | `_savetty` | 1 | inkscape |
| libncurses.5.4.dylib | `_scrollok` | 1 | inkscape |
| libncurses.5.4.dylib | `_set_escdelay` | 1 | inkscape |
| libncurses.5.4.dylib | `_set_tabsize` | 1 | inkscape |
| libncurses.5.4.dylib | `_setcchar` | 1 | inkscape |
| libncurses.5.4.dylib | `_start_color` | 1 | inkscape |
| libncurses.5.4.dylib | `_stdscr` | 1 | inkscape |
| libncurses.5.4.dylib | `_strnames` | 1 | iterm2 |
| libncurses.5.4.dylib | `_subpad` | 1 | inkscape |
| libncurses.5.4.dylib | `_subwin` | 1 | inkscape |
| libncurses.5.4.dylib | `_syncok` | 1 | inkscape |
| libncurses.5.4.dylib | `_termattrs` | 1 | inkscape |
| libncurses.5.4.dylib | `_termname` | 1 | inkscape |
| libncurses.5.4.dylib | `_tgetent` | 1 | inkscape |
| libncurses.5.4.dylib | `_tgetflag` | 1 | inkscape |
| libncurses.5.4.dylib | `_tgetnum` | 1 | inkscape |
| libncurses.5.4.dylib | `_tgetstr` | 1 | inkscape |
| libncurses.5.4.dylib | `_tgoto` | 1 | inkscape |
| libncurses.5.4.dylib | `_tparm` | 1 | inkscape |
| libncurses.5.4.dylib | `_tputs` | 1 | inkscape |
| libncurses.5.4.dylib | `_typeahead` | 1 | inkscape |
| libncurses.5.4.dylib | `_unctrl` | 1 | inkscape |
| libncurses.5.4.dylib | `_unget_wch` | 1 | inkscape |
| libncurses.5.4.dylib | `_ungetch` | 1 | inkscape |
| libncurses.5.4.dylib | `_ungetmouse` | 1 | inkscape |
| libncurses.5.4.dylib | `_use_default_colors` | 1 | inkscape |
| libncurses.5.4.dylib | `_use_env` | 1 | inkscape |
| libncurses.5.4.dylib | `_wadd_wch` | 1 | inkscape |
| libncurses.5.4.dylib | `_waddch` | 1 | inkscape |
| libncurses.5.4.dylib | `_waddnstr` | 1 | inkscape |
| libncurses.5.4.dylib | `_waddnwstr` | 1 | inkscape |
| libncurses.5.4.dylib | `_wattr_off` | 1 | inkscape |
| libncurses.5.4.dylib | `_wattr_on` | 1 | inkscape |
| libncurses.5.4.dylib | `_wbkgd` | 1 | inkscape |
| libncurses.5.4.dylib | `_wbkgdset` | 1 | inkscape |
| libncurses.5.4.dylib | `_wborder` | 1 | inkscape |
| libncurses.5.4.dylib | `_wchgat` | 1 | inkscape |
| libncurses.5.4.dylib | `_wclear` | 1 | inkscape |
| libncurses.5.4.dylib | `_wclrtobot` | 1 | inkscape |
| libncurses.5.4.dylib | `_wclrtoeol` | 1 | inkscape |
| libncurses.5.4.dylib | `_wcursyncup` | 1 | inkscape |
| libncurses.5.4.dylib | `_wdelch` | 1 | inkscape |
| libncurses.5.4.dylib | `_wechochar` | 1 | inkscape |
| libncurses.5.4.dylib | `_wenclose` | 1 | inkscape |
| libncurses.5.4.dylib | `_werase` | 1 | inkscape |
| libncurses.5.4.dylib | `_wget_wch` | 1 | inkscape |
| libncurses.5.4.dylib | `_wgetch` | 1 | inkscape |
| libncurses.5.4.dylib | `_wgetnstr` | 1 | inkscape |
| libncurses.5.4.dylib | `_whline` | 1 | inkscape |
| libncurses.5.4.dylib | `_winch` | 1 | inkscape |
| libncurses.5.4.dylib | `_winnstr` | 1 | inkscape |
| libncurses.5.4.dylib | `_wins_nwstr` | 1 | inkscape |
| libncurses.5.4.dylib | `_winsch` | 1 | inkscape |
| libncurses.5.4.dylib | `_winsdelln` | 1 | inkscape |
| libncurses.5.4.dylib | `_winsnstr` | 1 | inkscape |
| libncurses.5.4.dylib | `_wmove` | 1 | inkscape |
| libncurses.5.4.dylib | `_wnoutrefresh` | 1 | inkscape |
| libncurses.5.4.dylib | `_wredrawln` | 1 | inkscape |
| libncurses.5.4.dylib | `_wrefresh` | 1 | inkscape |
| libncurses.5.4.dylib | `_wresize` | 1 | inkscape |
| libncurses.5.4.dylib | `_wscrl` | 1 | inkscape |
| libncurses.5.4.dylib | `_wsetscrreg` | 1 | inkscape |
| libncurses.5.4.dylib | `_wsyncdown` | 1 | inkscape |
| libncurses.5.4.dylib | `_wsyncup` | 1 | inkscape |
| libncurses.5.4.dylib | `_wtimeout` | 1 | inkscape |
| libncurses.5.4.dylib | `_wtouchln` | 1 | inkscape |
| libncurses.5.4.dylib | `_wvline` | 1 | inkscape |
| libpanel.5.4.dylib | `_bottom_panel` | 1 | inkscape |
| libpanel.5.4.dylib | `_del_panel` | 1 | inkscape |
| libpanel.5.4.dylib | `_hide_panel` | 1 | inkscape |
| libpanel.5.4.dylib | `_move_panel` | 1 | inkscape |
| libpanel.5.4.dylib | `_new_panel` | 1 | inkscape |
| libpanel.5.4.dylib | `_panel_above` | 1 | inkscape |
| libpanel.5.4.dylib | `_panel_below` | 1 | inkscape |
| libpanel.5.4.dylib | `_panel_hidden` | 1 | inkscape |
| libpanel.5.4.dylib | `_panel_userptr` | 1 | inkscape |
| libpanel.5.4.dylib | `_replace_panel` | 1 | inkscape |
| libpanel.5.4.dylib | `_set_panel_userptr` | 1 | inkscape |
| libpanel.5.4.dylib | `_show_panel` | 1 | inkscape |
| libpanel.5.4.dylib | `_top_panel` | 1 | inkscape |
| libpanel.5.4.dylib | `_update_panels` | 1 | inkscape |
| libsandbox.1.dylib | `_sandbox_apply_container` | 1 | iterm2 |
| libxslt.1.dylib | `_xsltApplyOneTemplate` | 1 | inkscape |
| libxslt.1.dylib | `_xsltApplyStylesheet` | 1 | inkscape |
| libxslt.1.dylib | `_xsltApplyStylesheetUser` | 1 | inkscape |
| libxslt.1.dylib | `_xsltCleanupGlobals` | 1 | inkscape |
| libxslt.1.dylib | `_xsltDocDefaultLoader` | 1 | inkscape |
| libxslt.1.dylib | `_xsltFreeSecurityPrefs` | 1 | inkscape |
| libxslt.1.dylib | `_xsltFreeStylesheet` | 1 | inkscape |
| libxslt.1.dylib | `_xsltFreeTransformContext` | 1 | inkscape |
| libxslt.1.dylib | `_xsltGenericError` | 1 | inkscape |
| libxslt.1.dylib | `_xsltGenericErrorContext` | 1 | inkscape |
| libxslt.1.dylib | `_xsltGetProfileInformation` | 1 | inkscape |
| libxslt.1.dylib | `_xsltGetSecurityPrefs` | 1 | inkscape |
| libxslt.1.dylib | `_xsltLibxsltVersion` | 1 | inkscape |
| libxslt.1.dylib | `_xsltMaxDepth` | 1 | inkscape |
| libxslt.1.dylib | `_xsltNewSecurityPrefs` | 1 | inkscape |
| libxslt.1.dylib | `_xsltNewTransformContext` | 1 | inkscape |
| libxslt.1.dylib | `_xsltNextImport` | 1 | inkscape |
| libxslt.1.dylib | `_xsltParseStylesheetDoc` | 1 | inkscape |
| libxslt.1.dylib | `_xsltProcessOneNode` | 1 | inkscape |
| libxslt.1.dylib | `_xsltQuoteOneUserParam` | 1 | inkscape |
| libxslt.1.dylib | `_xsltRegisterAllExtras` | 1 | inkscape |
| libxslt.1.dylib | `_xsltRegisterExtElement` | 1 | inkscape |
| libxslt.1.dylib | `_xsltRegisterExtFunction` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSaveResultTo` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSaveResultToFilename` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSaveResultToString` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSecurityAllow` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSecurityForbid` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSetCtxtParseOptions` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSetCtxtSecurityPrefs` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSetGenericErrorFunc` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSetLoaderFunc` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSetSecurityPrefs` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSetTransformErrorFunc` | 1 | inkscape |
| libxslt.1.dylib | `_xsltTransformError` | 1 | inkscape |
