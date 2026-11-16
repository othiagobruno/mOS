# Lacunas de Thais (GPU e telas)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Thais (GPU e telas) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**3** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreDisplay.framework | `_CoreDisplay_DisplayCreateInfoDictionary` | 2 | iina, monitorcontrol |
| DisplayServices.framework | `_DisplayServicesGetBrightness` | 1 | monitorcontrol |
| DisplayServices.framework | `_DisplayServicesSetBrightness` | 1 | monitorcontrol |
