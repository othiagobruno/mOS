# Lacunas de Thais (GPU e telas)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Thais (GPU e telas) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**11** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreDisplay.framework | `_CoreDisplay_DisplayCreateInfoDictionary` | 4 | betterdisplay, iina, monitorcontrol, stremio |
| DisplayServices.framework | `_DisplayServicesGetBrightness` | 2 | betterdisplay, monitorcontrol |
| DisplayServices.framework | `_DisplayServicesSetBrightness` | 2 | betterdisplay, monitorcontrol |
| DisplayServices.framework | `_DisplayServicesAmbientLightCompensationEnabled` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesCanChangeBrightness` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesEnableAmbientLightCompensation` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesHasAmbientLightCompensation` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesRegisterForBrightnessChangeNotifications` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesResetAmbientLight` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesUnregisterForAmbientLightCompensationNotifications` | 1 | betterdisplay |
| DisplayServices.framework | `_DisplayServicesUnregisterForBrightnessChangeNotifications` | 1 | betterdisplay |
