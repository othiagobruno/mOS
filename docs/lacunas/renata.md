# Lacunas de Renata (BSD/Mach/XPC)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Renata (BSD/Mach/XPC) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**6** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libSystem.B.dylib | `__dyld_bind_fully_image_containing_address` | 1 | inkscape |
| libSystem.B.dylib | `_bootstrap_create_server` | 1 | vlc |
| libSystem.B.dylib | `_bootstrap_create_service` | 1 | vlc |
| libSystem.B.dylib | `_bootstrap_parent` | 1 | iterm2 |
| libSystem.B.dylib | `_bootstrap_subset` | 1 | vlc |
| libSystem.B.dylib | `_mach_vm_write` | 1 | gimp |
