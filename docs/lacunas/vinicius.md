# Lacunas de Vinicius (arquivos e volumes)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Vinicius (arquivos e volumes) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**76** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libarchive.2.dylib | `_archive_entry_set_pathname` | 4 | crossover, dropbox, opera, utm |
| libarchive.2.dylib | `_archive_error_string` | 4 | crossover, dropbox, opera, utm |
| libSystem.B.dylib | `_getfsstat64` | 3 | docker-desktop, orbstack, rancher |
| libarchive.2.dylib | `_archive_entry_free` | 3 | crossover, dropbox, utm |
| libarchive.2.dylib | `_archive_entry_new` | 3 | crossover, dropbox, utm |
| libarchive.2.dylib | `_archive_write_data` | 3 | crossover, dropbox, utm |
| libarchive.2.dylib | `_archive_write_free` | 3 | crossover, dropbox, utm |
| libarchive.2.dylib | `_archive_write_header` | 3 | crossover, dropbox, utm |
| libarchive.2.dylib | `_archive_write_new` | 3 | crossover, dropbox, utm |
| libarchive.2.dylib | `_archive_entry_clear` | 2 | crossover, utm |
| libarchive.2.dylib | `_archive_entry_pathname` | 2 | dropbox, opera |
| libarchive.2.dylib | `_archive_entry_set_filetype` | 2 | crossover, utm |
| libarchive.2.dylib | `_archive_entry_set_perm` | 2 | crossover, utm |
| libarchive.2.dylib | `_archive_entry_set_size` | 2 | crossover, utm |
| libarchive.2.dylib | `_archive_errno` | 2 | dropbox, opera |
| libarchive.2.dylib | `_archive_read_new` | 2 | dropbox, opera |
| libarchive.2.dylib | `_archive_read_open_filename` | 2 | dropbox, opera |
| libarchive.2.dylib | `_archive_write_add_filter_bzip2` | 2 | crossover, utm |
| libarchive.2.dylib | `_archive_write_close` | 2 | crossover, utm |
| libarchive.2.dylib | `_archive_write_open` | 2 | crossover, utm |
| libarchive.2.dylib | `_archive_write_open_memory` | 2 | crossover, utm |
| libarchive.2.dylib | `_archive_write_set_format_pax_restricted` | 2 | crossover, utm |
| DiskArbitration.framework | `_kDADiskDescriptionMediaIconKey` | 1 | raspberry-pi-imager |
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
| libarchive.2.dylib | `_archive_entry_set_atime` | 1 | dropbox |
| libarchive.2.dylib | `_archive_entry_set_ctime` | 1 | dropbox |
| libarchive.2.dylib | `_archive_entry_set_gname` | 1 | dropbox |
| libarchive.2.dylib | `_archive_entry_set_mtime` | 1 | dropbox |
| libarchive.2.dylib | `_archive_entry_set_uname` | 1 | dropbox |
| libarchive.2.dylib | `_archive_entry_size` | 1 | dropbox |
| libarchive.2.dylib | `_archive_entry_unset_atime` | 1 | dropbox |
| libarchive.2.dylib | `_archive_entry_unset_ctime` | 1 | dropbox |
| libarchive.2.dylib | `_archive_entry_unset_mtime` | 1 | dropbox |
| libarchive.2.dylib | `_archive_read_data` | 1 | dropbox |
| libarchive.2.dylib | `_archive_read_disk_entry_from_file` | 1 | dropbox |
| libarchive.2.dylib | `_archive_read_disk_new` | 1 | dropbox |
| libarchive.2.dylib | `_archive_read_disk_set_standard_lookup` | 1 | dropbox |
| libarchive.2.dylib | `_archive_read_disk_set_symlink_logical` | 1 | dropbox |
| libarchive.2.dylib | `_archive_read_extract` | 1 | opera |
| libarchive.2.dylib | `_archive_read_finish` | 1 | opera |
| libarchive.2.dylib | `_archive_read_free` | 1 | dropbox |
| libarchive.2.dylib | `_archive_read_next_header` | 1 | opera |
| libarchive.2.dylib | `_archive_read_next_header2` | 1 | dropbox |
| libarchive.2.dylib | `_archive_read_support_compression_xz` | 1 | opera |
| libarchive.2.dylib | `_archive_read_support_filter_all` | 1 | dropbox |
| libarchive.2.dylib | `_archive_read_support_format_all` | 1 | dropbox |
| libarchive.2.dylib | `_archive_read_support_format_tar` | 1 | opera |
| libarchive.2.dylib | `_archive_read_support_format_zip` | 1 | opera |
| libarchive.2.dylib | `_archive_write_disk_new` | 1 | dropbox |
| libarchive.2.dylib | `_archive_write_disk_set_options` | 1 | dropbox |
| libarchive.2.dylib | `_archive_write_disk_set_standard_lookup` | 1 | dropbox |
| libarchive.2.dylib | `_archive_write_finish_entry` | 1 | dropbox |
| libarchive.2.dylib | `_archive_write_open_filename` | 1 | dropbox |
| libarchive.2.dylib | `_archive_write_set_format_pax` | 1 | dropbox |
