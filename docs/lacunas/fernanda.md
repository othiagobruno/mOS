# Lacunas de Fernanda (Swift runtime, Dispatch)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Fernanda (Swift runtime, Dispatch) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**21** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libobjc.A.dylib | `_class_getIvarLayout` | 2 | alt-tab, vlc |
| libobjc.A.dylib | `_objc_initializeClassPair` | 2 | alt-tab, vlc |
| libswiftos.dylib | `_$sSo9OS_os_logC0B0E7defaultABvgZ` | 2 | iterm2, rectangle |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarV12withMutation2of7keyPath_q0_x_s03KeyG0Cyxq_Gq0_yKXEtKAA10ObservableRzr1_lF` | 1 | rectangle |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarV6access_7keyPathyx_s03KeyE0Cyxq_GtAA10ObservableRzr0_lF` | 1 | rectangle |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarV6didSet_7keyPathyx_s03KeyF0Cyxq_GtAA10ObservableRzr0_lF` | 1 | rectangle |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarV7willSet_7keyPathyx_s03KeyF0Cyxq_GtAA10ObservableRzr0_lF` | 1 | rectangle |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarVACycfC` | 1 | rectangle |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarVMa` | 1 | rectangle |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarVMn` | 1 | rectangle |
| libswiftObservation.dylib | `_$s11Observation10ObservableMp` | 1 | rectangle |
| libswiftos.dylib | `_$s2os0A4_log_3dso0B04type_ys12StaticStringV_SVSgSo03OS_a1_B0CSo0a1_b1_D2_tas7CVarArg_pdtF` | 1 | iterm2 |
| libswiftos.dylib | `_$s2os21OSAllocatedUnfairLockVMn` | 1 | iterm2 |
| libswiftos.dylib | `_$s2os6LoggerV9logObjectSo03OS_a1_C0Cvg` | 1 | iterm2 |
| libswiftos.dylib | `_$s2os6LoggerV9subsystem8categoryACSS_SStcfC` | 1 | iterm2 |
| libswiftos.dylib | `_$s2os6LoggerVMa` | 1 | iterm2 |
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E4infoABvgZ` | 1 | iterm2 |
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E5debugABvgZ` | 1 | iterm2 |
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E5errorABvgZ` | 1 | iterm2 |
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E5faultABvgZ` | 1 | iterm2 |
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E7defaultABvgZ` | 1 | rectangle |
