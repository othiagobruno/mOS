# Lacunas de Thais (GPU e telas)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Thais (GPU e telas) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**17** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreDisplay.framework | `_CoreDisplay_DisplayCreateInfoDictionary` | 4 | betterdisplay, iina, monitorcontrol, stremio |
| DisplayServices.framework | `_DisplayServicesGetBrightness` | 2 | betterdisplay, monitorcontrol |
| DisplayServices.framework | `_DisplayServicesSetBrightness` | 2 | betterdisplay, monitorcontrol |
| CoreBrightness.framework | `_OBJC_CLASS_$_CBBlueLightClient` | 1 | betterdisplay |
| CoreBrightness.framework | `_OBJC_CLASS_$_CBTrueToneClient` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesAmbientLightCompensationEnabled` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesCanChangeBrightness` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesEnableAmbientLightCompensation` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesHasAmbientLightCompensation` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesRegisterForBrightnessChangeNotifications` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesResetAmbientLight` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesUnregisterForAmbientLightCompensationNotifications` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesUnregisterForBrightnessChangeNotifications` | 1 | betterdisplay |
| ParavirtualizedGraphics.framework | `_OBJC_CLASS_$_PGDeviceDescriptor` | 1 | utm |
| ParavirtualizedGraphics.framework | `_OBJC_CLASS_$_PGDisplayDescriptor` | 1 | utm |
| ParavirtualizedGraphics.framework | `_OBJC_CLASS_$_PGDisplayMode` | 1 | utm |
| ParavirtualizedGraphics.framework | `_PGCopyOptionROMURL` | 1 | utm |
