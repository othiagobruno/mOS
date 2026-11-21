# Lacunas de Renata (BSD/Mach/XPC)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Renata (BSD/Mach/XPC) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**51** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libSystem.B.dylib | `_posix_spawnattr_getflags` | 8 | capcut, feishu, homebrew-app, kitty |
| libSystem.B.dylib | `_posix_spawnattr_set_qos_class_np` | 4 | homebrew-app, macpacker, openclaw, thaw |
| libSystem.B.dylib | `__dyld_get_image_uuid` | 3 | crossover, orion, utm |
| libSystem.B.dylib | `_dyld_image_header_containing_address` | 3 | crossover, orion, utm |
| libSystem.B.dylib | `_mach_vm_page_query` | 3 | microsoft-excel, microsoft-powerpoint, microsoft-word |
| libSystem.B.dylib | `_mig_strncpy` | 3 | hex-fiend, proxyman, xquartz |
| libSystem.B.dylib | `__dyld_get_shared_cache_uuid` | 2 | android-studio, orion |
| libSystem.B.dylib | `_dyld_image_path_containing_address` | 2 | cmux, supacode |
| libSystem.B.dylib | `_dyld_shared_cache_file_path` | 2 | android-studio, orion |
| libSystem.B.dylib | `_xpc_shmem_create` | 2 | orbstack, virtualbox |
| libSystem.B.dylib | `_xpc_shmem_map` | 2 | orbstack, virtualbox |
| libSystem.B.dylib | `__dyld_get_dlopen_image_header` | 1 | orion |
| libSystem.B.dylib | `__dyld_get_shared_cache_range` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_image_copy_uuid` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_image_for_each_segment_info` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_image_get_installname` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_shared_cache_copy_uuid` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_shared_cache_for_each_image` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_shared_cache_for_file` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_shared_cache_iterate_text` | 1 | android-studio |
| libSystem.B.dylib | `_kdebug_signpost_end` | 1 | capcut |
| libSystem.B.dylib | `_mach_memory_entry_ownership` | 1 | orion |
| libSystem.B.dylib | `_mach_msg_server` | 1 | xquartz |
| libSystem.B.dylib | `_mach_voucher_deallocate` | 1 | orion |
| libSystem.B.dylib | `_mig_deallocate` | 1 | xquartz |
| libSystem.B.dylib | `_notify_monitor_file` | 1 | emacs-app |
| libSystem.B.dylib | `_notify_register_plain` | 1 | emacs-app |
| libSystem.B.dylib | `_notify_set_options` | 1 | orion |
| libSystem.B.dylib | `_os_log_with_args` | 1 | orion |
| libSystem.B.dylib | `_os_unfair_lock_lock_with_flags` | 1 | orion |
| libSystem.B.dylib | `_posix_spawnattr_getpgroup` | 1 | orbstack |
| libSystem.B.dylib | `_posix_spawnattr_getsigdefault` | 1 | orbstack |
| libSystem.B.dylib | `_posix_spawnattr_getsigmask` | 1 | orbstack |
| libSystem.B.dylib | `_posix_spawnattr_setarchpref_np` | 1 | utm |
| libSystem.B.dylib | `_sandbox_enable_state_flag` | 1 | orion |
| libSystem.B.dylib | `_sandbox_extension_issue_iokit_registry_entry_class` | 1 | orion |
| libSystem.B.dylib | `_sandbox_extension_issue_iokit_registry_entry_class_to_process` | 1 | orion |
| libSystem.B.dylib | `_xpc_activity_copy_criteria` | 1 | orion |
| libSystem.B.dylib | `_xpc_activity_get_state` | 1 | orion |
| libSystem.B.dylib | `_xpc_activity_register` | 1 | orion |
| libSystem.B.dylib | `_xpc_activity_set_criteria` | 1 | orion |
| libSystem.B.dylib | `_xpc_add_bundle` | 1 | bettertouchtool |
| libSystem.B.dylib | `_xpc_connection_copy_entitlement_value` | 1 | orion |
| libSystem.B.dylib | `_xpc_connection_copy_invalidation_reason` | 1 | orion |
| libSystem.B.dylib | `_xpc_connection_kill` | 1 | orion |
| libSystem.B.dylib | `_xpc_connection_set_bootstrap` | 1 | orion |
| libSystem.B.dylib | `_xpc_connection_set_oneshot_instance` | 1 | orion |
| libSystem.B.dylib | `_xpc_copy_bootstrap` | 1 | orion |
| libSystem.B.dylib | `_xpc_dictionary_create_connection` | 1 | dropbox |
| libSystem.B.dylib | `_xpc_dictionary_set_connection` | 1 | dropbox |
| libSystem.B.dylib | `_xpc_transaction_exit_clean` | 1 | orion |
