# Lacunas de Renata (BSD/Mach/XPC)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Renata (BSD/Mach/XPC) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**32** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libSystem.B.dylib | `_os_activity_scope_enter` | 13 | audacity, bitwarden, db-browser-for-sqlite, dolphin |
| libSystem.B.dylib | `_os_activity_scope_leave` | 13 | audacity, bitwarden, db-browser-for-sqlite, dolphin |
| libSystem.B.dylib | `_bootstrap_parent` | 4 | brave-browser, google-chrome, iterm2, microsoft-edge |
| libSystem.B.dylib | `_mach_vm_write` | 3 | gimp, kitty, sublime-text |
| libSystem.B.dylib | `_posix_spawnattr_getflags` | 3 | kitty, openclaw, orbstack |
| libSystem.B.dylib | `_bootstrap_create_server` | 2 | steam, vlc |
| libSystem.B.dylib | `_bootstrap_create_service` | 2 | steam, vlc |
| libSystem.B.dylib | `_bootstrap_subset` | 2 | steam, vlc |
| libSystem.B.dylib | `_mig_strncpy` | 2 | hex-fiend, xquartz |
| libSystem.B.dylib | `__dyld_bind_fully_image_containing_address` | 1 | inkscape |
| libSystem.B.dylib | `__dyld_get_image_uuid` | 1 | utm |
| libSystem.B.dylib | `__dyld_get_shared_cache_range` | 1 | android-studio |
| libSystem.B.dylib | `__dyld_get_shared_cache_uuid` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_image_copy_uuid` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_image_for_each_segment_info` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_image_get_installname` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_image_header_containing_address` | 1 | utm |
| libSystem.B.dylib | `_dyld_image_path_containing_address` | 1 | cmux |
| libSystem.B.dylib | `_dyld_shared_cache_copy_uuid` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_shared_cache_file_path` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_shared_cache_for_each_image` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_shared_cache_for_file` | 1 | android-studio |
| libSystem.B.dylib | `_dyld_shared_cache_iterate_text` | 1 | android-studio |
| libSystem.B.dylib | `_mach_msg_server` | 1 | xquartz |
| libSystem.B.dylib | `_mig_deallocate` | 1 | xquartz |
| libSystem.B.dylib | `_posix_spawnattr_getpgroup` | 1 | orbstack |
| libSystem.B.dylib | `_posix_spawnattr_getsigdefault` | 1 | orbstack |
| libSystem.B.dylib | `_posix_spawnattr_getsigmask` | 1 | orbstack |
| libSystem.B.dylib | `_posix_spawnattr_set_qos_class_np` | 1 | openclaw |
| libSystem.B.dylib | `_posix_spawnattr_setarchpref_np` | 1 | utm |
| libSystem.B.dylib | `_xpc_shmem_create` | 1 | orbstack |
| libSystem.B.dylib | `_xpc_shmem_map` | 1 | orbstack |
