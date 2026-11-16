# Lacunas de Vinicius (arquivos e volumes)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Vinicius (arquivos e volumes) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**20** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libSystem.B.dylib | `_acl_add_flag_np` | 2 | gimp, iina |
| libSystem.B.dylib | `_acl_add_perm` | 2 | gimp, iina |
| libSystem.B.dylib | `_acl_clear_flags_np` | 2 | gimp, iina |
| libSystem.B.dylib | `_acl_clear_perms` | 2 | gimp, iina |
| libSystem.B.dylib | `_acl_dup` | 2 | gimp, iina |
| libSystem.B.dylib | `_acl_get_flag_np` | 2 | gimp, iina |
| libSystem.B.dylib | `_acl_get_flagset_np` | 2 | gimp, iina |
| libSystem.B.dylib | `_acl_get_perm_np` | 2 | gimp, iina |
| libSystem.B.dylib | `_acl_get_permset` | 2 | gimp, iina |
| libSystem.B.dylib | `_acl_get_qualifier` | 2 | gimp, iina |
| libSystem.B.dylib | `_acl_set_link_np` | 2 | gimp, iina |
| libSystem.B.dylib | `_acl_set_qualifier` | 2 | gimp, iina |
| DiskArbitration.framework | `_DADiskCreateFromVolumePath` | 1 | stats |
| DiskArbitration.framework | `_DADiskGetBSDName` | 1 | stats |
| DiskArbitration.framework | `_kDADiskDescriptionDeviceProtocolKey` | 1 | stats |
| DiskArbitration.framework | `_kDADiskDescriptionMediaEncryptedKey` | 1 | stats |
| DiskArbitration.framework | `_kDADiskDescriptionMediaNameKey` | 1 | stats |
| DiskArbitration.framework | `_kDADiskDescriptionMediaUUIDKey` | 1 | stats |
| DiskArbitration.framework | `_kDADiskDescriptionMediaWritableKey` | 1 | stats |
| DiskArbitration.framework | `_kDADiskDescriptionVolumeKindKey` | 1 | stats |
