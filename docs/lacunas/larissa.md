# Lacunas de Larissa (plataforma: IOKit, USB, áudio HW)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Larissa (plataforma: IOKit, USB, áudio HW) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**127** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| IOKit.framework | `_IODisplayCreateInfoDictionary` | 15 | betterdisplay, blender, db-browser-for-sqlite, dolphin |
| IOKit.framework | `_IOIteratorIsValid` | 11 | dolphin, godot, hammerspoon, krita |
| IOKit.framework | `_IOHIDDeviceGetValue` | 10 | dolphin, godot, kitty, krita |
| IOKit.framework | `_IOHIDElementGetTypeID` | 9 | dolphin, godot, kitty, krita |
| IOKit.framework | `_IOObjectIsEqualTo` | 9 | dolphin, godot, hammerspoon, krita |
| ForceFeedback.framework | `_FFDeviceGetForceFeedbackProperty` | 8 | dolphin, godot, krita, love |
| ForceFeedback.framework | `_FFDeviceSendForceFeedbackCommand` | 8 | dolphin, godot, krita, love |
| ForceFeedback.framework | `_FFDeviceSetForceFeedbackProperty` | 8 | dolphin, godot, krita, love |
| ForceFeedback.framework | `_FFEffectGetEffectStatus` | 8 | dolphin, godot, krita, love |
| IOKit.framework | `_IODispatchCalloutFromMessage` | 8 | dolphin, godot, krita, love |
| IOKit.framework | `_IOHIDElementGetChildren` | 8 | dolphin, godot, krita, love |
| IOKit.framework | `_IOHIDElementGetCookie` | 8 | dolphin, godot, krita, love |
| IOKit.framework | `_IONotificationPortGetMachPort` | 8 | dolphin, godot, krita, love |
| IOKit.framework | `_IOObjectGetClass` | 7 | firefox, floorp, libreoffice, orbstack |
| IOKit.framework | `_IOIteratorReset` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| IOKit.framework | `_IOServiceNameMatching` | 4 | hammerspoon, monitorcontrol, obs, stats |
| Hypervisor.framework | `_hv_vcpu_create` | 3 | docker-desktop, orbstack, utm |
| Hypervisor.framework | `_hv_vcpu_enable_native_msr` | 3 | docker-desktop, orbstack, utm |
| Hypervisor.framework | `_hv_vcpu_interrupt` | 3 | docker-desktop, orbstack, utm |
| Hypervisor.framework | `_hv_vcpu_read_register` | 3 | docker-desktop, orbstack, utm |
| Hypervisor.framework | `_hv_vcpu_write_register` | 3 | docker-desktop, orbstack, utm |
| Hypervisor.framework | `_hv_vm_create` | 3 | docker-desktop, orbstack, utm |
| Hypervisor.framework | `_hv_vm_map` | 3 | docker-desktop, orbstack, utm |
| Hypervisor.framework | `_hv_vm_protect` | 3 | docker-desktop, orbstack, utm |
| Hypervisor.framework | `_hv_vmx_read_capability` | 3 | docker-desktop, orbstack, utm |
| Hypervisor.framework | `_hv_vmx_vcpu_read_vmcs` | 3 | docker-desktop, orbstack, utm |
| Hypervisor.framework | `_hv_vmx_vcpu_write_vmcs` | 3 | docker-desktop, orbstack, utm |
| IOKit.framework | `_IOConnectCallScalarMethod` | 3 | android-studio, obs, vlc |
| IOKit.framework | `_IOHIDDeviceRegisterInputValueCallback` | 3 | hammerspoon, linearmouse, zoom |
| IOKit.framework | `_IOHIDServiceClientConformsTo` | 3 | linearmouse, raycast, utm |
| IOKit.framework | `_IOHIDServiceClientSetProperty` | 3 | linearmouse, raycast, utm |
| IOKit.framework | `_IOHIDSetModifierLockState` | 3 | android-studio, hammerspoon, raycast |
| IOKit.framework | `_IORegistryEntryGetChildIterator` | 3 | betterdisplay, orbstack, stats |
| IOKit.framework | `_IORegistryEntryGetParentIterator` | 3 | orbstack, stats, vlc |
| IOKit.framework | `_IORegistryEntryGetPath` | 3 | betterdisplay, openscad, zoom |
| Hypervisor.framework | `_hv_vcpu_destroy` | 2 | orbstack, utm |
| Hypervisor.framework | `_hv_vcpu_invalidate_tlb` | 2 | docker-desktop, utm |
| Hypervisor.framework | `_hv_vcpu_read_msr` | 2 | orbstack, utm |
| Hypervisor.framework | `_hv_vcpu_run_until` | 2 | orbstack, utm |
| Hypervisor.framework | `_hv_vcpu_write_msr` | 2 | orbstack, utm |
| Hypervisor.framework | `_hv_vm_destroy` | 2 | docker-desktop, orbstack |
| Hypervisor.framework | `_hv_vm_unmap` | 2 | orbstack, utm |
| IOKit.framework | `_IODestroyPlugInInterface` | 2 | stats, zoom |
| IOKit.framework | `_IOFBCopyI2CInterfaceForBus` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOFBGetI2CInterfaceCount` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOHIDDeviceConformsTo` | 2 | hammerspoon, zoom |
| IOKit.framework | `_IOHIDDeviceSetValue` | 2 | hammerspoon, zoom |
| IOKit.framework | `_IOHIDEventSystemClientCreateSimpleClient` | 2 | raycast, utm |
| IOKit.framework | `_IOHIDValueCreateWithIntegerValue` | 2 | hammerspoon, zoom |
| IOKit.framework | `_IOHIDValueGetTimeStamp` | 2 | linearmouse, orbstack |
| IOKit.framework | `_IOI2CInterfaceClose` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOI2CInterfaceOpen` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOI2CSendRequest` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOKitWaitQuiet` | 2 | dolphin, utm |
| IOKit.framework | `_IOPSCopyExternalPowerAdapterDetails` | 2 | hammerspoon, stats |
| IOKit.framework | `_IOPSGetProvidingPowerSourceType` | 2 | hammerspoon, iterm2 |
| IOKit.framework | `_IORegistryEntryGetChildEntry` | 2 | dolphin, utm |
| IOKit.framework | `_IOServiceAuthorize` | 2 | dolphin, utm |
| IOKit.framework | `_NXOpenEventStatus` | 2 | hammerspoon, xquartz |
| libswiftIOKit.dylib | `_$s5IOKit20kIOReturnBadArguments5Int32Vvg` | 2 | linearmouse, stats |
| GameController.framework | `_GCInputDualShockTouchpadOne` | 1 | dolphin |
| GameController.framework | `_GCInputDualShockTouchpadTwo` | 1 | dolphin |
| Hypervisor.framework | `_hv_tsc_clock` | 1 | orbstack |
| Hypervisor.framework | `_hv_vcpu_apic_read` | 1 | orbstack |
| Hypervisor.framework | `_hv_vcpu_apic_write` | 1 | orbstack |
| Hypervisor.framework | `_hv_vcpu_exit_apic_access_read` | 1 | orbstack |
| Hypervisor.framework | `_hv_vcpu_exit_info` | 1 | orbstack |
| Hypervisor.framework | `_hv_vcpu_exit_startup_ap` | 1 | orbstack |
| Hypervisor.framework | `_hv_vcpu_read_fpstate` | 1 | utm |
| Hypervisor.framework | `_hv_vcpu_run` | 1 | docker-desktop |
| Hypervisor.framework | `_hv_vcpu_set_tsc_relative` | 1 | orbstack |
| Hypervisor.framework | `_hv_vcpu_write_fpstate` | 1 | utm |
| Hypervisor.framework | `_hv_vm_ioapic_pulse_irq` | 1 | orbstack |
| Hypervisor.framework | `_hv_vm_ioapic_read` | 1 | orbstack |
| Hypervisor.framework | `_hv_vm_ioapic_write` | 1 | orbstack |
| Hypervisor.framework | `_hv_vmx_get_msr_info` | 1 | orbstack |
| Hypervisor.framework | `_hv_vmx_vcpu_set_apic_address` | 1 | orbstack |
| IOKit.framework | `_IOConnectMapMemory` | 1 | obs |
| IOKit.framework | `_IOConnectRelease` | 1 | betterdisplay |
| IOKit.framework | `_IODisplayGetFloatParameter` | 1 | hammerspoon |
| IOKit.framework | `_IODisplaySetFloatParameter` | 1 | hammerspoon |
| IOKit.framework | `_IOHIDCopyCFTypeParameter` | 1 | linearmouse |
| IOKit.framework | `_IOHIDDeviceActivate` | 1 | linearmouse |
| IOKit.framework | `_IOHIDDeviceCancel` | 1 | linearmouse |
| IOKit.framework | `_IOHIDDeviceGetReportWithCallback` | 1 | orbstack |
| IOKit.framework | `_IOHIDDeviceGetValueWithCallback` | 1 | zoom |
| IOKit.framework | `_IOHIDDeviceSetCancelHandler` | 1 | linearmouse |
| IOKit.framework | `_IOHIDDeviceSetDispatchQueue` | 1 | linearmouse |
| IOKit.framework | `_IOHIDDeviceSetInputValueMatching` | 1 | linearmouse |
| IOKit.framework | `_IOHIDDeviceSetReportWithCallback` | 1 | orbstack |
| IOKit.framework | `_IOHIDDeviceSetValueWithCallback` | 1 | zoom |
| IOKit.framework | `_IOHIDElementHasPreferredState` | 1 | zoom |
| IOKit.framework | `_IOHIDElementIsRelative` | 1 | zoom |
| IOKit.framework | `_IOHIDEventGetSenderID` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventGetType` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSetFloatValue` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientCopyServiceForRegistryID` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientRegisterDeviceMatchingBlock` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientRegisterEventBlock` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientRegisterPropertyChangedCallback` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientScheduleWithDispatchQueue` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientSetMatchingMultiple` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientUnregisterDeviceMatchingBlock` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientUnscheduleFromDispatchQueue` | 1 | linearmouse |
| IOKit.framework | `_IOHIDGetAccelerationWithKey` | 1 | hammerspoon |
| IOKit.framework | `_IOHIDGetModifierLockState` | 1 | hammerspoon |
| IOKit.framework | `_IOHIDPostEvent` | 1 | linearmouse |
| IOKit.framework | `_IOHIDServiceClientGetRegistryID` | 1 | linearmouse |
| IOKit.framework | `_IOHIDServiceClientRegisterRemovalBlock` | 1 | linearmouse |
| IOKit.framework | `_IOHIDSetAccelerationWithKey` | 1 | hammerspoon |
| IOKit.framework | `_IOHIDValueCreateWithBytes` | 1 | zoom |
| IOKit.framework | `_IOPMAssertionSetProperty` | 1 | hammerspoon |
| IOKit.framework | `_IOPMCopyAssertionsStatus` | 1 | stats |
| IOKit.framework | `_IOPMCopyBatteryInfo` | 1 | hammerspoon |
| IOKit.framework | `_IOPMFindPowerManagement` | 1 | hammerspoon |
| IOKit.framework | `_IOPMSleepSystem` | 1 | hammerspoon |
| IOKit.framework | `_IOPSCreateLimitedPowerNotification` | 1 | iterm2 |
| IOKit.framework | `_IOPSGetBatteryWarningLevel` | 1 | hammerspoon |
| IOKit.framework | `_IOPSGetTimeRemainingEstimate` | 1 | hammerspoon |
| IOKit.framework | `_IORegistryCreateIterator` | 1 | hammerspoon |
| IOKit.framework | `_IORegistryEntrySetCFProperty` | 1 | zoom |
| IOKit.framework | `_IOServiceRequestProbe` | 1 | hammerspoon |
| libswiftIOKit.dylib | `_$s5IOKit14kIOReturnErrors5Int32Vvg` | 1 | stats |
| libswiftIOKit.dylib | `_$s5IOKit16kIOReturnNotOpens5Int32Vvg` | 1 | linearmouse |
| libswiftIOKit.dylib | `_$s5IOKit17kIOReturnNotFounds5Int32Vvg` | 1 | stats |
| libswiftIOKit.dylib | `_$s5IOKit21kIOReturnNotPermitteds5Int32Vvg` | 1 | raycast |
| libswiftIOKit.dylib | `_$s5IOKit24kIOReturnExclusiveAccesss5Int32Vvg` | 1 | raycast |
