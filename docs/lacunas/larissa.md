# Lacunas de Larissa (plataforma: IOKit, USB, áudio HW)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Larissa (plataforma: IOKit, USB, áudio HW) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**170** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| IOKit.framework | `_IODisplayCreateInfoDictionary` | 20 | adobe-acrobat-reader, betterdisplay, blender, db-browser-for-sqlite |
| IOKit.framework | `_IOHIDDeviceGetValue` | 13 | dolphin, godot, kitty, krita |
| IOKit.framework | `_IOIteratorIsValid` | 12 | dolphin, godot, hammerspoon, krita |
| IOKit.framework | `_IOHIDElementGetTypeID` | 11 | dolphin, godot, kitty, krita |
| IOKit.framework | `_IOObjectIsEqualTo` | 11 | dolphin, godot, hammerspoon, krita |
| IOKit.framework | `_IOHIDElementGetCookie` | 10 | dolphin, godot, krita, love |
| ForceFeedback.framework | `_FFDeviceGetForceFeedbackProperty` | 9 | dolphin, godot, krita, love |
| ForceFeedback.framework | `_FFDeviceSendForceFeedbackCommand` | 9 | dolphin, godot, krita, love |
| ForceFeedback.framework | `_FFDeviceSetForceFeedbackProperty` | 9 | dolphin, godot, krita, love |
| ForceFeedback.framework | `_FFEffectGetEffectStatus` | 9 | dolphin, godot, krita, love |
| IOKit.framework | `_IODispatchCalloutFromMessage` | 9 | dolphin, godot, krita, love |
| IOKit.framework | `_IOHIDElementGetChildren` | 9 | dolphin, godot, krita, love |
| IOKit.framework | `_IONotificationPortGetMachPort` | 9 | dolphin, godot, krita, love |
| IOKit.framework | `_IOObjectGetClass` | 9 | firefox, floorp, libreoffice, librewolf |
| IOKit.framework | `_IOIteratorReset` | 8 | anydesk, firefox, floorp, librewolf |
| IOKit.framework | `_IORegistryEntryGetChildIterator` | 8 | aldente, betterdisplay, gemini, microsoft-teams |
| IOKit.framework | `_IOHIDSetModifierLockState` | 7 | android-studio, hammerspoon, intellij-idea, jetbrains-toolbox |
| IOKit.framework | `_IORegistryEntryGetPath` | 7 | betterdisplay, gemini, microsoft-teams, openscad |
| IOKit.framework | `_IOConnectCallScalarMethod` | 6 | aldente, android-studio, intellij-idea, jetbrains-toolbox |
| IOKit.framework | `_IOHIDDeviceRegisterInputValueCallback` | 5 | hammerspoon, linearmouse, microsoft-teams, virtualbox |
| IOKit.framework | `_IOKitWaitQuiet` | 5 | calibre, dolphin, openmtp, raspberry-pi-imager |
| IOKit.framework | `_IORegistryEntryGetParentIterator` | 5 | karabiner-elements, orbstack, raspberry-pi-imager, stats |
| IOKit.framework | `_IOServiceNameMatching` | 5 | hammerspoon, karabiner-elements, monitorcontrol, obs |
| IOKit.framework | `_IODestroyPlugInInterface` | 4 | openlogi, stats, virtualbox, zoom |
| IOKit.framework | `_IOHIDDeviceConformsTo` | 4 | hammerspoon, karabiner-elements, virtualbox, zoom |
| IOKit.framework | `_IOHIDEventSystemClientCreateSimpleClient` | 4 | raycast, scroll-reverser, utm, vorssaint |
| IOKit.framework | `_IOHIDServiceClientConformsTo` | 4 | linearmouse, raycast, utm, vorssaint |
| IOKit.framework | `_IOHIDServiceClientSetProperty` | 4 | linearmouse, raycast, utm, vorssaint |
| IOKit.framework | `_IOHIDValueCreateWithIntegerValue` | 4 | hammerspoon, microsoft-teams, virtualbox, zoom |
| IOKit.framework | `_IOPSCopyExternalPowerAdapterDetails` | 4 | aldente, hammerspoon, keka, stats |
| IOKit.framework | `_IOPSGetProvidingPowerSourceType` | 4 | codexbar, hammerspoon, handbrake-app, iterm2 |
| IOKit.framework | `_IORegistryEntryGetChildEntry` | 4 | calibre, dolphin, raspberry-pi-imager, utm |
| IOKit.framework | `_IOServiceAuthorize` | 4 | calibre, dolphin, raspberry-pi-imager, utm |
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
| IOKit.framework | `_IOHIDDeviceSetValue` | 3 | hammerspoon, virtualbox, zoom |
| IOKit.framework | `_IOHIDEventGetSenderID` | 3 | linearmouse, openlogi, scroll-reverser |
| IOKit.framework | `_IOHIDGetModifierLockState` | 3 | hammerspoon, tigervnc, vorssaint |
| IOKit.framework | `_IOHIDServiceClientGetRegistryID` | 3 | linearmouse, scroll-reverser, vorssaint |
| IOKit.framework | `_IOHIDValueGetTimeStamp` | 3 | karabiner-elements, linearmouse, orbstack |
| IOKit.framework | `_IOPMFindPowerManagement` | 3 | aldente, hammerspoon, vorssaint |
| IOKit.framework | `_IOPMSleepSystem` | 3 | aldente, hammerspoon, vorssaint |
| GameController.framework | `_GCInputDualShockTouchpadOne` | 2 | dolphin, moonlight |
| GameController.framework | `_GCInputDualShockTouchpadTwo` | 2 | dolphin, moonlight |
| Hypervisor.framework | `_hv_vcpu_destroy` | 2 | orbstack, utm |
| Hypervisor.framework | `_hv_vcpu_invalidate_tlb` | 2 | docker-desktop, utm |
| Hypervisor.framework | `_hv_vcpu_read_msr` | 2 | orbstack, utm |
| Hypervisor.framework | `_hv_vcpu_run_until` | 2 | orbstack, utm |
| Hypervisor.framework | `_hv_vcpu_write_msr` | 2 | orbstack, utm |
| Hypervisor.framework | `_hv_vm_destroy` | 2 | docker-desktop, orbstack |
| Hypervisor.framework | `_hv_vm_unmap` | 2 | orbstack, utm |
| IOKit.framework | `_IOFBCopyI2CInterfaceForBus` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOFBGetI2CInterfaceCount` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOHIDDeviceActivate` | 2 | linearmouse, openlogi |
| IOKit.framework | `_IOHIDDeviceCancel` | 2 | linearmouse, openlogi |
| IOKit.framework | `_IOHIDDeviceSetCancelHandler` | 2 | linearmouse, openlogi |
| IOKit.framework | `_IOHIDDeviceSetDispatchQueue` | 2 | linearmouse, openlogi |
| IOKit.framework | `_IOHIDElementHasPreferredState` | 2 | microsoft-teams, zoom |
| IOKit.framework | `_IOHIDElementIsRelative` | 2 | microsoft-teams, zoom |
| IOKit.framework | `_IOHIDEventSetFloatValue` | 2 | linearmouse, scroll-reverser |
| IOKit.framework | `_IOHIDValueCreateWithBytes` | 2 | microsoft-teams, zoom |
| IOKit.framework | `_IOI2CInterfaceClose` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOI2CInterfaceOpen` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOI2CSendRequest` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOPMAssertionSetProperty` | 2 | hammerspoon, openclaw |
| IOKit.framework | `_IOPMCopyAssertionsStatus` | 2 | aldente, stats |
| IOKit.framework | `_IOPSCreateLimitedPowerNotification` | 2 | handbrake-app, iterm2 |
| IOKit.framework | `_NXOpenEventStatus` | 2 | hammerspoon, xquartz |
| libswiftIOKit.dylib | `_$s5IOKit14kIOReturnErrors5Int32Vvg` | 2 | finetune, stats |
| libswiftIOKit.dylib | `_$s5IOKit17kIOReturnNotFounds5Int32Vvg` | 2 | finetune, stats |
| libswiftIOKit.dylib | `_$s5IOKit20kIOReturnBadArguments5Int32Vvg` | 2 | linearmouse, stats |
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
| IOKit.framework | `_IOHIDDeviceGetReportWithCallback` | 1 | orbstack |
| IOKit.framework | `_IOHIDDeviceGetTypeID` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDDeviceGetValueWithCallback` | 1 | zoom |
| IOKit.framework | `_IOHIDDeviceSetInputValueMatching` | 1 | linearmouse |
| IOKit.framework | `_IOHIDDeviceSetReportWithCallback` | 1 | orbstack |
| IOKit.framework | `_IOHIDDeviceSetValueMultiple` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDDeviceSetValueWithCallback` | 1 | zoom |
| IOKit.framework | `_IOHIDElementGetCollectionType` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementGetName` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementGetPhysicalMax` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementGetPhysicalMin` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementGetProperty` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementGetReportCount` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementGetUnit` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementGetUnitExponent` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementHasNullState` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementIsArray` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementIsNonLinear` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementIsWrapping` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDEventGetType` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientCopyServiceForRegistryID` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientRegisterDeviceMatchingBlock` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientRegisterEventBlock` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientRegisterPropertyChangedCallback` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientScheduleWithDispatchQueue` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientSetMatchingMultiple` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientUnregisterDeviceMatchingBlock` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientUnscheduleFromDispatchQueue` | 1 | linearmouse |
| IOKit.framework | `_IOHIDGetAccelerationWithKey` | 1 | hammerspoon |
| IOKit.framework | `_IOHIDManagerActivate` | 1 | openlogi |
| IOKit.framework | `_IOHIDManagerCancel` | 1 | openlogi |
| IOKit.framework | `_IOHIDManagerSetCancelHandler` | 1 | openlogi |
| IOKit.framework | `_IOHIDManagerSetDispatchQueue` | 1 | openlogi |
| IOKit.framework | `_IOHIDPostEvent` | 1 | linearmouse |
| IOKit.framework | `_IOHIDQueueAddElement` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDQueueCopyNextValueWithTimeout` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDQueueCreate` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDQueueRegisterValueAvailableCallback` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDQueueScheduleWithRunLoop` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDQueueStart` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDQueueStop` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDQueueUnscheduleFromRunLoop` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDRequestAccess` | 1 | openlogi |
| IOKit.framework | `_IOHIDServiceClientRegisterRemovalBlock` | 1 | linearmouse |
| IOKit.framework | `_IOHIDSetAccelerationWithKey` | 1 | hammerspoon |
| IOKit.framework | `_IOHIDUserDeviceCreateWithProperties` | 1 | openlogi |
| IOKit.framework | `_IOHIDUserDeviceHandleReportWithTimeStamp` | 1 | openlogi |
| IOKit.framework | `_IOObjectCopyClass` | 1 | adobe-acrobat-reader |
| IOKit.framework | `_IOPMConnectionAcknowledgeEvent` | 1 | openlogi |
| IOKit.framework | `_IOPMConnectionCreate` | 1 | openlogi |
| IOKit.framework | `_IOPMConnectionGetSystemCapabilities` | 1 | openlogi |
| IOKit.framework | `_IOPMConnectionRelease` | 1 | openlogi |
| IOKit.framework | `_IOPMConnectionSetDispatchQueue` | 1 | openlogi |
| IOKit.framework | `_IOPMConnectionSetNotification` | 1 | openlogi |
| IOKit.framework | `_IOPMCopyActivePMPreferences` | 1 | aldente |
| IOKit.framework | `_IOPMCopyBatteryInfo` | 1 | hammerspoon |
| IOKit.framework | `_IOPMCopySystemPowerSettings` | 1 | aldente |
| IOKit.framework | `_IOPMSetSystemPowerSetting` | 1 | aldente |
| IOKit.framework | `_IOPSGetBatteryWarningLevel` | 1 | hammerspoon |
| IOKit.framework | `_IOPSGetTimeRemainingEstimate` | 1 | hammerspoon |
| IOKit.framework | `_IORegistryCreateIterator` | 1 | hammerspoon |
| IOKit.framework | `_IORegistryEntryGetProperty` | 1 | virtualbox |
| IOKit.framework | `_IORegistryEntrySetCFProperty` | 1 | zoom |
| IOKit.framework | `_IOServiceRequestProbe` | 1 | hammerspoon |
| IOKit.framework | `_IOServiceWaitQuiet` | 1 | microsoft-teams |
| libswiftIOKit.dylib | `_$s5IOKit16kIOReturnNotOpens5Int32Vvg` | 1 | linearmouse |
| libswiftIOKit.dylib | `_$s5IOKit17kIOReturnNotReadys5Int32Vvg` | 1 | finetune |
| libswiftIOKit.dylib | `_$s5IOKit21kIOReturnNotPermitteds5Int32Vvg` | 1 | raycast |
| libswiftIOKit.dylib | `_$s5IOKit22kIOReturnNotPrivilegeds5Int32Vvg` | 1 | aldente |
| libswiftIOKit.dylib | `_$s5IOKit24kIOReturnExclusiveAccesss5Int32Vvg` | 1 | raycast |
