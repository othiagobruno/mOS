# Lacunas de Patricia (rede para apps)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Patricia (rede para apps) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**6** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| SystemConfiguration.framework | `_kSCPropNetProxiesGopherEnable` | 2 | gimp, inkscape |
| SystemConfiguration.framework | `_kSCPropNetProxiesGopherPort` | 2 | gimp, inkscape |
| SystemConfiguration.framework | `_kSCPropNetProxiesGopherProxy` | 2 | gimp, inkscape |
| SystemConfiguration.framework | `_kSCCompAnyRegex` | 1 | firefox |
| SystemConfiguration.framework | `_kSCDynamicStoreDomainSetup` | 1 | firefox |
| SystemConfiguration.framework | `_kSCEntNetDNS` | 1 | firefox |
