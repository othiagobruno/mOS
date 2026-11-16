# Lacunas de Larissa (plataforma: IOKit, USB, áudio HW)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Larissa (plataforma: IOKit, USB, áudio HW) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**36** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| IOKit.framework | `_IODisplayCreateInfoDictionary` | 4 | blender, krita, monitorcontrol, shotcut |
| IOKit.framework | `_IOObjectGetClass` | 4 | firefox, libreoffice, thunderbird, vlc |
| IOKit.framework | `_IOIteratorIsValid` | 3 | krita, shotcut, stats |
| IOKit.framework | `_IOServiceNameMatching` | 3 | monitorcontrol, obs, stats |
| ForceFeedback.framework | `_FFDeviceGetForceFeedbackProperty` | 2 | krita, shotcut |
| ForceFeedback.framework | `_FFDeviceSendForceFeedbackCommand` | 2 | krita, shotcut |
| ForceFeedback.framework | `_FFDeviceSetForceFeedbackProperty` | 2 | krita, shotcut |
| ForceFeedback.framework | `_FFEffectGetEffectStatus` | 2 | krita, shotcut |
| IOKit.framework | `_IOConnectCallScalarMethod` | 2 | obs, vlc |
| IOKit.framework | `_IODispatchCalloutFromMessage` | 2 | krita, shotcut |
| IOKit.framework | `_IOHIDDeviceGetValue` | 2 | krita, shotcut |
| IOKit.framework | `_IOHIDElementGetChildren` | 2 | krita, shotcut |
| IOKit.framework | `_IOHIDElementGetCookie` | 2 | krita, shotcut |
| IOKit.framework | `_IOHIDElementGetTypeID` | 2 | krita, shotcut |
| IOKit.framework | `_IOIteratorReset` | 2 | firefox, thunderbird |
| IOKit.framework | `_IONotificationPortGetMachPort` | 2 | krita, shotcut |
| IOKit.framework | `_IOObjectIsEqualTo` | 2 | krita, shotcut |
| IOKit.framework | `_IORegistryEntryGetParentIterator` | 2 | stats, vlc |
| CoreWLAN.framework | `_OBJC_CLASS_$_CWChannel` | 1 | stats |
| CoreWLAN.framework | `_OBJC_CLASS_$_CWInterface` | 1 | stats |
| IOKit.framework | `_IOConnectMapMemory` | 1 | obs |
| IOKit.framework | `_IODestroyPlugInInterface` | 1 | stats |
| IOKit.framework | `_IOFBCopyI2CInterfaceForBus` | 1 | monitorcontrol |
| IOKit.framework | `_IOFBGetI2CInterfaceCount` | 1 | monitorcontrol |
| IOKit.framework | `_IOI2CInterfaceClose` | 1 | monitorcontrol |
| IOKit.framework | `_IOI2CInterfaceOpen` | 1 | monitorcontrol |
| IOKit.framework | `_IOI2CSendRequest` | 1 | monitorcontrol |
| IOKit.framework | `_IOPMCopyAssertionsStatus` | 1 | stats |
| IOKit.framework | `_IOPSCopyExternalPowerAdapterDetails` | 1 | stats |
| IOKit.framework | `_IOPSCreateLimitedPowerNotification` | 1 | iterm2 |
| IOKit.framework | `_IOPSGetProvidingPowerSourceType` | 1 | iterm2 |
| IOKit.framework | `_IORegistryEntryGetChildIterator` | 1 | stats |
| IOKit.framework | `_SCPreferencesCreate` | 1 | stats |
| libswiftIOKit.dylib | `_$s5IOKit14kIOReturnErrors5Int32Vvg` | 1 | stats |
| libswiftIOKit.dylib | `_$s5IOKit17kIOReturnNotFounds5Int32Vvg` | 1 | stats |
| libswiftIOKit.dylib | `_$s5IOKit20kIOReturnBadArguments5Int32Vvg` | 1 | stats |
