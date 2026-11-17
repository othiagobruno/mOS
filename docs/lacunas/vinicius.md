# Lacunas de Vinicius (arquivos e volumes)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Vinicius (arquivos e volumes) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**66** símbolos distintos.

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
| FSKit.framework | `_$s10FileSystem5FSKit05UnaryaB9ExtensionPTl` | 1 | fuse-t |
| FSKit.framework | `_$s5FSKit24UnaryFileSystemExtensionMp` | 1 | fuse-t |
| FSKit.framework | `_$s5FSKit24UnaryFileSystemExtensionP04fileD00cD0QzvgTq` | 1 | fuse-t |
| FSKit.framework | `_$s5FSKit24UnaryFileSystemExtensionP0E10Foundation03AppE0Tb` | 1 | fuse-t |
| FSKit.framework | `_$s5FSKit24UnaryFileSystemExtensionPAAE13configurationQrvg` | 1 | fuse-t |
| FSKit.framework | `_$s5FSKit24UnaryFileSystemExtensionPAAE13configurationQrvpQOMQ` | 1 | fuse-t |
| FSKit.framework | `_OBJC_CLASS_$_FSClient` | 1 | fuse-t |
| FSKit.framework | `_OBJC_CLASS_$_FSContainerIdentifier` | 1 | fuse-t |
| FSKit.framework | `_OBJC_CLASS_$_FSContainerStatus` | 1 | fuse-t |
| FSKit.framework | `_OBJC_CLASS_$_FSFileName` | 1 | fuse-t |
| FSKit.framework | `_OBJC_CLASS_$_FSItem` | 1 | fuse-t |
| FSKit.framework | `_OBJC_CLASS_$_FSItemAttributes` | 1 | fuse-t |
| FSKit.framework | `_OBJC_CLASS_$_FSProbeResult` | 1 | fuse-t |
| FSKit.framework | `_OBJC_CLASS_$_FSStatFSResult` | 1 | fuse-t |
| FSKit.framework | `_OBJC_CLASS_$_FSUnaryFileSystem` | 1 | fuse-t |
| FSKit.framework | `_OBJC_CLASS_$_FSVolume` | 1 | fuse-t |
| FSKit.framework | `_OBJC_CLASS_$_FSVolumeIdentifier` | 1 | fuse-t |
| FSKit.framework | `_OBJC_CLASS_$_FSVolumeSupportedCapabilities` | 1 | fuse-t |
| FSKit.framework | `_OBJC_METACLASS_$_FSItem` | 1 | fuse-t |
| FSKit.framework | `_OBJC_METACLASS_$_FSUnaryFileSystem` | 1 | fuse-t |
| FSKit.framework | `_OBJC_METACLASS_$_FSVolume` | 1 | fuse-t |
| NetFS.framework | `_NetFSMountURLSync` | 1 | adobe-acrobat-reader |
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
