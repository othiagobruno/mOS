# Lacunas de Vinicius (arquivos e volumes)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Vinicius (arquivos e volumes) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**36** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| DiskArbitration.framework | `_DADiskCreateFromVolumePath` | 3 | appcleaner, prusaslicer, stats |
| DiskArbitration.framework | `_DADiskGetBSDName` | 2 | hex-fiend, stats |
| DiskArbitration.framework | `_DAUnregisterCallback` | 2 | hex-fiend, spotify |
| DiskArbitration.framework | `_kDADiskDescriptionDeviceProtocolKey` | 2 | prusaslicer, stats |
| DiskArbitration.framework | `_kDADiskDescriptionMediaNameKey` | 2 | hex-fiend, stats |
| libSystem.B.dylib | `_getfsstat64` | 2 | docker-desktop, orbstack |
| DiskArbitration.framework | `_DAApprovalSessionCreate` | 1 | spotify |
| DiskArbitration.framework | `_DAApprovalSessionScheduleWithRunLoop` | 1 | spotify |
| DiskArbitration.framework | `_DAApprovalSessionUnscheduleFromRunLoop` | 1 | spotify |
| DiskArbitration.framework | `_DARegisterDiskUnmountApprovalCallback` | 1 | spotify |
| DiskArbitration.framework | `_DAUnregisterApprovalCallback` | 1 | spotify |
| DiskArbitration.framework | `_kDADiskDescriptionBusNameKey` | 1 | hex-fiend |
| DiskArbitration.framework | `_kDADiskDescriptionMediaEjectableKey` | 1 | prusaslicer |
| DiskArbitration.framework | `_kDADiskDescriptionMediaEncryptedKey` | 1 | stats |
| DiskArbitration.framework | `_kDADiskDescriptionMediaUUIDKey` | 1 | stats |
| DiskArbitration.framework | `_kDADiskDescriptionMediaWholeKey` | 1 | hex-fiend |
| DiskArbitration.framework | `_kDADiskDescriptionMediaWritableKey` | 1 | stats |
| DiskArbitration.framework | `_kDADiskDescriptionVolumeKindKey` | 1 | stats |
| DiskArbitration.framework | `_kDADiskDescriptionVolumeNetworkKey` | 1 | hex-fiend |
| libarchive.2.dylib | `_archive_entry_clear` | 1 | utm |
| libarchive.2.dylib | `_archive_entry_free` | 1 | utm |
| libarchive.2.dylib | `_archive_entry_new` | 1 | utm |
| libarchive.2.dylib | `_archive_entry_set_filetype` | 1 | utm |
| libarchive.2.dylib | `_archive_entry_set_pathname` | 1 | utm |
| libarchive.2.dylib | `_archive_entry_set_perm` | 1 | utm |
| libarchive.2.dylib | `_archive_entry_set_size` | 1 | utm |
| libarchive.2.dylib | `_archive_error_string` | 1 | utm |
| libarchive.2.dylib | `_archive_write_add_filter_bzip2` | 1 | utm |
| libarchive.2.dylib | `_archive_write_close` | 1 | utm |
| libarchive.2.dylib | `_archive_write_data` | 1 | utm |
| libarchive.2.dylib | `_archive_write_free` | 1 | utm |
| libarchive.2.dylib | `_archive_write_header` | 1 | utm |
| libarchive.2.dylib | `_archive_write_new` | 1 | utm |
| libarchive.2.dylib | `_archive_write_open` | 1 | utm |
| libarchive.2.dylib | `_archive_write_open_memory` | 1 | utm |
| libarchive.2.dylib | `_archive_write_set_format_pax_restricted` | 1 | utm |
