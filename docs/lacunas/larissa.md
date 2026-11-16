# Lacunas de Larissa (plataforma: IOKit, USB, áudio HW)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Larissa (plataforma: IOKit, USB, áudio HW) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**6** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| IOKit.framework | `_IOObjectGetClass` | 2 | firefox, vlc |
| IOKit.framework | `_IOConnectCallScalarMethod` | 1 | vlc |
| IOKit.framework | `_IOIteratorReset` | 1 | firefox |
| IOKit.framework | `_IOPSCreateLimitedPowerNotification` | 1 | iterm2 |
| IOKit.framework | `_IOPSGetProvidingPowerSourceType` | 1 | iterm2 |
| IOKit.framework | `_IORegistryEntryGetParentIterator` | 1 | vlc |
