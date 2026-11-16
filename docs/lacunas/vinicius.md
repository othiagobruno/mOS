# Lacunas de Vinicius (arquivos e volumes)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Vinicius (arquivos e volumes) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**44** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| DiskArbitration.framework | `_DADiskCreateFromVolumePath` | 7 | appcleaner, bbedit, daisydisk, mounty |
| DiskArbitration.framework | `_DADiskGetBSDName` | 5 | hex-fiend, mounty, raspberry-pi-imager, stats |
| DiskArbitration.framework | `_kDADiskDescriptionDeviceProtocolKey` | 4 | daisydisk, prusaslicer, raspberry-pi-imager, stats |
| DiskArbitration.framework | `_kDADiskDescriptionMediaNameKey` | 4 | daisydisk, hex-fiend, raspberry-pi-imager, stats |
| DiskArbitration.framework | `_DAUnregisterCallback` | 3 | hex-fiend, raspberry-pi-imager, spotify |
| DiskArbitration.framework | `_DAUnregisterApprovalCallback` | 2 | spotify, virtualbox |
| DiskArbitration.framework | `_kDADiskDescriptionBusPathKey` | 2 | daisydisk, raspberry-pi-imager |
| DiskArbitration.framework | `_kDADiskDescriptionDeviceInternalKey` | 2 | daisydisk, raspberry-pi-imager |
| DiskArbitration.framework | `_kDADiskDescriptionMediaEjectableKey` | 2 | prusaslicer, raspberry-pi-imager |
| DiskArbitration.framework | `_kDADiskDescriptionMediaWritableKey` | 2 | raspberry-pi-imager, stats |
| DiskArbitration.framework | `_kDADiskDescriptionVolumeKindKey` | 2 | bbedit, stats |
| DiskArbitration.framework | `_kDADiskDescriptionVolumeNetworkKey` | 2 | bbedit, hex-fiend |
| libSystem.B.dylib | `_getfsstat64` | 2 | docker-desktop, orbstack |
| DiskArbitration.framework | `_DAApprovalSessionCreate` | 1 | spotify |
| DiskArbitration.framework | `_DAApprovalSessionScheduleWithRunLoop` | 1 | spotify |
| DiskArbitration.framework | `_DAApprovalSessionUnscheduleFromRunLoop` | 1 | spotify |
| DiskArbitration.framework | `_DARegisterDiskMountApprovalCallback` | 1 | virtualbox |
| DiskArbitration.framework | `_DARegisterDiskUnmountApprovalCallback` | 1 | spotify |
| DiskArbitration.framework | `_DASessionSetDispatchQueue` | 1 | vorssaint |
| DiskArbitration.framework | `_kDADiskDescriptionBusNameKey` | 1 | hex-fiend |
| DiskArbitration.framework | `_kDADiskDescriptionMediaBlockSizeKey` | 1 | raspberry-pi-imager |
| DiskArbitration.framework | `_kDADiskDescriptionMediaEncryptedKey` | 1 | stats |
| DiskArbitration.framework | `_kDADiskDescriptionMediaIconKey` | 1 | raspberry-pi-imager |
| DiskArbitration.framework | `_kDADiskDescriptionMediaKindKey` | 1 | daisydisk |
| DiskArbitration.framework | `_kDADiskDescriptionMediaUUIDKey` | 1 | stats |
| DiskArbitration.framework | `_kDADiskDescriptionMediaWholeKey` | 1 | hex-fiend |
| libSystem.B.dylib | `_fs_snapshot_list` | 1 | daisydisk |
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
