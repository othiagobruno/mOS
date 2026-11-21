# Lacunas de Larissa (plataforma: IOKit, USB, áudio HW)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Larissa (plataforma: IOKit, USB, áudio HW) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**275** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| IOKit.framework | `_IODisplayCreateInfoDictionary` | 33 | adobe-acrobat-reader, bambu-studio, betterdisplay, blender |
| IOKit.framework | `_IOHIDDeviceGetValue` | 22 | bambu-studio, bettertouchtool, crossover, dolphin |
| IOKit.framework | `_IORegistryEntryGetChildIterator` | 22 | aldente, betterdisplay, bettertouchtool, coconutbattery |
| IOKit.framework | `_IOHIDElementGetTypeID` | 17 | bambu-studio, bettertouchtool, crossover, dolphin |
| IOKit.framework | `_IOIteratorIsValid` | 16 | capcut, crossover, dolphin, feishu |
| IOKit.framework | `_IOHIDElementGetCookie` | 14 | bettertouchtool, crossover, dolphin, freecad |
| IOKit.framework | `_IOObjectIsEqualTo` | 14 | bettertouchtool, crossover, dolphin, freecad |
| IOKit.framework | `_IOHIDElementGetChildren` | 13 | bettertouchtool, crossover, dolphin, freecad |
| IOKit.framework | `_IOHIDSetModifierLockState` | 12 | android-studio, bettertouchtool, datagrip, hammerspoon |
| IOKit.framework | `_IORegistryEntryGetPath` | 12 | betterdisplay, coconutbattery, gemini, istat-menus |
| ForceFeedback.framework | `_FFDeviceGetForceFeedbackProperty` | 11 | crossover, dolphin, freecad, godot |
| ForceFeedback.framework | `_FFDeviceSendForceFeedbackCommand` | 11 | crossover, dolphin, freecad, godot |
| ForceFeedback.framework | `_FFDeviceSetForceFeedbackProperty` | 11 | crossover, dolphin, freecad, godot |
| ForceFeedback.framework | `_FFEffectGetEffectStatus` | 11 | crossover, dolphin, freecad, godot |
| IOKit.framework | `_IODispatchCalloutFromMessage` | 11 | crossover, dolphin, freecad, godot |
| IOKit.framework | `_IONotificationPortGetMachPort` | 11 | crossover, dolphin, freecad, godot |
| IOKit.framework | `_IOObjectGetClass` | 11 | bettertouchtool, firefox, floorp, istat-menus |
| IOKit.framework | `_IORegistryEntryGetChildEntry` | 11 | calibre, dolphin, freecad, istat-menus |
| IOKit.framework | `_IOConnectCallScalarMethod` | 10 | aldente, android-studio, datagrip, intellij-idea |
| IOKit.framework | `_IODestroyPlugInInterface` | 10 | bettertouchtool, coconutbattery, displaylink, istat-menus |
| IOKit.framework | `_IOPSGetProvidingPowerSourceType` | 10 | bartender, codexbar, feishu, hammerspoon |
| IOKit.framework | `_IOIteratorReset` | 9 | anydesk, coconutbattery, firefox, floorp |
| IOKit.framework | `_IOPSGetTimeRemainingEstimate` | 9 | affinity, bartender, bettertouchtool, feishu |
| IOKit.framework | `_IOServiceNameMatching` | 9 | bettertouchtool, coconutbattery, feishu, hammerspoon |
| IOKit.framework | `_IOKitWaitQuiet` | 8 | android-file-transfer, calibre, dolphin, freecad |
| IOKit.framework | `_IOHIDDeviceRegisterInputValueCallback` | 7 | bettertouchtool, hammerspoon, linearmouse, microsoft-teams |
| IOKit.framework | `_IOHIDEventSystemClientCreateSimpleClient` | 6 | bettertouchtool, raycast, scroll-reverser, unnaturalscrollwheels |
| IOKit.framework | `_IOHIDGetModifierLockState` | 6 | bettertouchtool, hammerspoon, realvnc-connect-viewer, tigervnc |
| IOKit.framework | `_IOHIDServiceClientSetProperty` | 6 | bettertouchtool, linearmouse, mac-mouse-fix, raycast |
| IOKit.framework | `_IOObjectCopyClass` | 6 | adobe-acrobat-reader, capcut, coconutbattery, crossover |
| IOKit.framework | `_IORegistryEntryGetParentIterator` | 6 | karabiner-elements, mac-mouse-fix, orbstack, raspberry-pi-imager |
| IOKit.framework | `_IOServiceAuthorize` | 6 | calibre, dolphin, freecad, raspberry-pi-imager |
| IOKit.framework | `_IODisplayGetFloatParameter` | 5 | bartender, bettertouchtool, capcut, feishu |
| IOKit.framework | `_IOHIDDeviceConformsTo` | 5 | crossover, hammerspoon, karabiner-elements, virtualbox |
| IOKit.framework | `_IOHIDServiceClientConformsTo` | 5 | bettertouchtool, linearmouse, raycast, utm |
| IOKit.framework | `_IOHIDServiceClientGetRegistryID` | 5 | bettertouchtool, linearmouse, mac-mouse-fix, scroll-reverser |
| IOKit.framework | `_IOHIDValueGetTimeStamp` | 5 | feishu, karabiner-elements, linearmouse, orbstack |
| IOKit.framework | `_NXOpenEventStatus` | 5 | bettertouchtool, hammerspoon, mactex, mactex-no-gui |
| Hypervisor.framework | `_hv_vcpu_create` | 4 | docker-desktop, orbstack, rancher, utm |
| Hypervisor.framework | `_hv_vcpu_enable_native_msr` | 4 | docker-desktop, orbstack, rancher, utm |
| Hypervisor.framework | `_hv_vcpu_interrupt` | 4 | docker-desktop, orbstack, rancher, utm |
| Hypervisor.framework | `_hv_vcpu_read_register` | 4 | docker-desktop, orbstack, rancher, utm |
| Hypervisor.framework | `_hv_vcpu_write_register` | 4 | docker-desktop, orbstack, rancher, utm |
| Hypervisor.framework | `_hv_vm_create` | 4 | docker-desktop, orbstack, rancher, utm |
| Hypervisor.framework | `_hv_vm_map` | 4 | docker-desktop, orbstack, rancher, utm |
| Hypervisor.framework | `_hv_vm_protect` | 4 | docker-desktop, orbstack, rancher, utm |
| Hypervisor.framework | `_hv_vmx_read_capability` | 4 | docker-desktop, orbstack, rancher, utm |
| Hypervisor.framework | `_hv_vmx_vcpu_read_vmcs` | 4 | docker-desktop, orbstack, rancher, utm |
| Hypervisor.framework | `_hv_vmx_vcpu_write_vmcs` | 4 | docker-desktop, orbstack, rancher, utm |
| IOKit.framework | `_IOHIDDeviceSetValue` | 4 | hammerspoon, virtualbox, wispr-flow, zoom |
| IOKit.framework | `_IOHIDEventGetSenderID` | 4 | linearmouse, openlogi, orion, scroll-reverser |
| IOKit.framework | `_IOHIDValueCreateWithIntegerValue` | 4 | hammerspoon, microsoft-teams, virtualbox, zoom |
| IOKit.framework | `_IOPSCopyExternalPowerAdapterDetails` | 4 | aldente, hammerspoon, keka, stats |
| IOKit.framework | `_IORegistryEntrySetCFProperty` | 4 | bettertouchtool, coconutbattery, skim, zoom |
| PCSC.framework | `_SCardBeginTransaction` | 4 | anki, crossover, keepassxc, windows-app |
| PCSC.framework | `_SCardConnect` | 4 | anki, crossover, keepassxc, windows-app |
| PCSC.framework | `_SCardDisconnect` | 4 | anki, crossover, keepassxc, windows-app |
| PCSC.framework | `_SCardEndTransaction` | 4 | anki, crossover, keepassxc, windows-app |
| PCSC.framework | `_SCardEstablishContext` | 4 | anki, crossover, keepassxc, windows-app |
| PCSC.framework | `_SCardListReaders` | 4 | anki, crossover, keepassxc, windows-app |
| PCSC.framework | `_SCardReconnect` | 4 | anki, crossover, keepassxc, windows-app |
| PCSC.framework | `_SCardReleaseContext` | 4 | anki, crossover, keepassxc, windows-app |
| PCSC.framework | `_SCardStatus` | 4 | anki, crossover, keepassxc, windows-app |
| PCSC.framework | `_SCardTransmit` | 4 | anki, crossover, keepassxc, windows-app |
| GameController.framework | `_GCInputDualShockTouchpadOne` | 3 | bettertouchtool, dolphin, moonlight |
| GameController.framework | `_GCInputDualShockTouchpadTwo` | 3 | bettertouchtool, dolphin, moonlight |
| Hypervisor.framework | `_hv_vcpu_destroy` | 3 | orbstack, rancher, utm |
| Hypervisor.framework | `_hv_vcpu_invalidate_tlb` | 3 | docker-desktop, rancher, utm |
| Hypervisor.framework | `_hv_vcpu_read_msr` | 3 | orbstack, rancher, utm |
| Hypervisor.framework | `_hv_vcpu_run_until` | 3 | orbstack, rancher, utm |
| Hypervisor.framework | `_hv_vcpu_write_msr` | 3 | orbstack, rancher, utm |
| Hypervisor.framework | `_hv_vm_unmap` | 3 | orbstack, rancher, utm |
| IOKit.framework | `_IOHIDDeviceActivate` | 3 | bettertouchtool, linearmouse, openlogi |
| IOKit.framework | `_IOHIDDeviceCancel` | 3 | bettertouchtool, linearmouse, openlogi |
| IOKit.framework | `_IOHIDDeviceSetCancelHandler` | 3 | bettertouchtool, linearmouse, openlogi |
| IOKit.framework | `_IOHIDDeviceSetDispatchQueue` | 3 | bettertouchtool, linearmouse, openlogi |
| IOKit.framework | `_IOHIDElementIsRelative` | 3 | bettertouchtool, microsoft-teams, zoom |
| IOKit.framework | `_IOHIDEventSystemClientCopyServiceForRegistryID` | 3 | linearmouse, mac-mouse-fix, orion |
| IOKit.framework | `_IOHIDGetAccelerationWithKey` | 3 | bettertouchtool, epic-games, hammerspoon |
| IOKit.framework | `_IOHIDManagerActivate` | 3 | bettertouchtool, openlogi, wispr-flow |
| IOKit.framework | `_IOHIDManagerCancel` | 3 | bettertouchtool, openlogi, wispr-flow |
| IOKit.framework | `_IOHIDManagerSetCancelHandler` | 3 | bettertouchtool, openlogi, wispr-flow |
| IOKit.framework | `_IOHIDManagerSetDispatchQueue` | 3 | bettertouchtool, openlogi, wispr-flow |
| IOKit.framework | `_IOHIDPostEvent` | 3 | bettertouchtool, linearmouse, realvnc-connect-viewer |
| IOKit.framework | `_IOHIDRequestAccess` | 3 | bettertouchtool, cherry-studio, openlogi |
| IOKit.framework | `_IOHIDSetAccelerationWithKey` | 3 | bettertouchtool, epic-games, hammerspoon |
| IOKit.framework | `_IOHIDValueCreateWithBytes` | 3 | microsoft-teams, wispr-flow, zoom |
| IOKit.framework | `_IOPMCopyAssertionsStatus` | 3 | aldente, crossover, stats |
| IOKit.framework | `_IOPMFindPowerManagement` | 3 | aldente, hammerspoon, vorssaint |
| IOKit.framework | `_IOPMSleepSystem` | 3 | aldente, hammerspoon, vorssaint |
| IOKit.framework | `_IOPSCreateLimitedPowerNotification` | 3 | coconutbattery, handbrake-app, iterm2 |
| IOKit.framework | `_NXClickTime` | 3 | bettertouchtool, mactex, mactex-no-gui |
| IOKit.framework | `_NXCloseEventStatus` | 3 | bettertouchtool, mactex, mactex-no-gui |
| PCSC.framework | `_SCardGetAttrib` | 3 | anki, crossover, windows-app |
| PCSC.framework | `_SCardGetStatusChange` | 3 | anki, crossover, windows-app |
| PCSC.framework | `_SCardIsValidContext` | 3 | crossover, keepassxc, windows-app |
| Virtualization.framework | `_OBJC_CLASS_$_VZVirtioConsoleDeviceSerialPortConfiguration` | 3 | docker-desktop, orbstack, rancher |
| libswiftIOKit.dylib | `_$s5IOKit14kIOReturnErrors5Int32Vvg` | 3 | coconutbattery, finetune, stats |
| Hypervisor.framework | `_hv_vcpu_read_fpstate` | 2 | rancher, utm |
| Hypervisor.framework | `_hv_vcpu_write_fpstate` | 2 | rancher, utm |
| Hypervisor.framework | `_hv_vm_destroy` | 2 | docker-desktop, orbstack |
| IOKit.framework | `_IODisplaySetFloatParameter` | 2 | bettertouchtool, hammerspoon |
| IOKit.framework | `_IOFBCopyI2CInterfaceForBus` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOFBGetI2CInterfaceCount` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOHIDDeviceSetReportWithCallback` | 2 | feishu, orbstack |
| IOKit.framework | `_IOHIDElementGetPhysicalMax` | 2 | microsoft-teams, orion |
| IOKit.framework | `_IOHIDElementGetPhysicalMin` | 2 | microsoft-teams, orion |
| IOKit.framework | `_IOHIDElementHasPreferredState` | 2 | microsoft-teams, zoom |
| IOKit.framework | `_IOHIDEventSetFloatValue` | 2 | linearmouse, scroll-reverser |
| IOKit.framework | `_IOHIDEventSystemClientCopyProperty` | 2 | bettertouchtool, unnaturalscrollwheels |
| IOKit.framework | `_IOHIDEventSystemClientCreateWithType` | 2 | mac-mouse-fix, orion |
| IOKit.framework | `_IOHIDEventSystemClientRegisterDeviceMatchingBlock` | 2 | bettertouchtool, linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientScheduleWithDispatchQueue` | 2 | bettertouchtool, linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientSetMatchingMultiple` | 2 | bettertouchtool, linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientUnregisterDeviceMatchingBlock` | 2 | bettertouchtool, linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientUnscheduleFromDispatchQueue` | 2 | bettertouchtool, linearmouse |
| IOKit.framework | `_IOHIDServiceClientRegisterRemovalBlock` | 2 | bettertouchtool, linearmouse |
| IOKit.framework | `_IOI2CInterfaceClose` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOI2CInterfaceOpen` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOI2CSendRequest` | 2 | betterdisplay, monitorcontrol |
| IOKit.framework | `_IOPMAssertionSetProperty` | 2 | hammerspoon, openclaw |
| IOKit.framework | `_IOPSGetBatteryWarningLevel` | 2 | bartender, hammerspoon |
| IOKit.framework | `_IORegistryCreateIterator` | 2 | hammerspoon, macs-fan-control |
| IOKit.framework | `_IOServiceRequestProbe` | 2 | amethyst, hammerspoon |
| IOKit.framework | `_KextManagerCopyLoadedKextInfo` | 2 | dropbox, epic-games |
| IOUSBHost.framework | `_OBJC_CLASS_$_IOUSBHostDevice` | 2 | orbstack, sfm |
| MultitouchSupport.framework | `_MTDeviceCreateList` | 2 | bettertouchtool, middleclick |
| MultitouchSupport.framework | `_MTDeviceStart` | 2 | bettertouchtool, middleclick |
| MultitouchSupport.framework | `_MTDeviceStop` | 2 | bettertouchtool, middleclick |
| MultitouchSupport.framework | `_MTRegisterContactFrameCallback` | 2 | bettertouchtool, middleclick |
| MultitouchSupport.framework | `_MTUnregisterContactFrameCallback` | 2 | bettertouchtool, middleclick |
| PCSC.framework | `_SCardCancel` | 2 | crossover, windows-app |
| PCSC.framework | `_SCardControl132` | 2 | crossover, windows-app |
| Virtualization.framework | `_OBJC_CLASS_$_VZBridgedNetworkDeviceAttachment` | 2 | docker-desktop, rancher |
| Virtualization.framework | `_OBJC_CLASS_$_VZFileSerialPortAttachment` | 2 | docker-desktop, rancher |
| Virtualization.framework | `_OBJC_CLASS_$_VZSingleDirectoryShare` | 2 | docker-desktop, orbstack |
| Virtualization.framework | `_OBJC_CLASS_$__VZGDBDebugStubConfiguration` | 2 | docker-desktop, rancher |
| Virtualization.framework | `_OBJC_METACLASS_$_VZVirtualMachine` | 2 | docker-desktop, rancher |
| libswiftIOKit.dylib | `_$s5IOKit17kIOReturnNotFounds5Int32Vvg` | 2 | finetune, stats |
| libswiftIOKit.dylib | `_$s5IOKit20kIOReturnBadArguments5Int32Vvg` | 2 | linearmouse, stats |
| libswiftIOKit.dylib | `_$s5IOKit24kIOReturnExclusiveAccesss5Int32Vvg` | 2 | raycast, wispr-flow |
| CoreMotion.framework | `_OBJC_CLASS_$_CMAltimeter` | 1 | whatsapp |
| CoreMotion.framework | `_OBJC_CLASS_$_CMMotionManager` | 1 | whatsapp |
| Hypervisor.framework | `_hv_tsc_clock` | 1 | orbstack |
| Hypervisor.framework | `_hv_vcpu_apic_read` | 1 | orbstack |
| Hypervisor.framework | `_hv_vcpu_apic_write` | 1 | orbstack |
| Hypervisor.framework | `_hv_vcpu_exit_apic_access_read` | 1 | orbstack |
| Hypervisor.framework | `_hv_vcpu_exit_info` | 1 | orbstack |
| Hypervisor.framework | `_hv_vcpu_exit_startup_ap` | 1 | orbstack |
| Hypervisor.framework | `_hv_vcpu_run` | 1 | docker-desktop |
| Hypervisor.framework | `_hv_vcpu_set_tsc_relative` | 1 | orbstack |
| Hypervisor.framework | `_hv_vm_ioapic_pulse_irq` | 1 | orbstack |
| Hypervisor.framework | `_hv_vm_ioapic_read` | 1 | orbstack |
| Hypervisor.framework | `_hv_vm_ioapic_write` | 1 | orbstack |
| Hypervisor.framework | `_hv_vmx_get_msr_info` | 1 | orbstack |
| Hypervisor.framework | `_hv_vmx_vcpu_set_apic_address` | 1 | orbstack |
| IOKit.framework | `_IOConnectMapMemory` | 1 | obs |
| IOKit.framework | `_IOConnectRelease` | 1 | betterdisplay |
| IOKit.framework | `_IOConnectSetCFProperties` | 1 | bettertouchtool |
| IOKit.framework | `_IOCopySystemLoadAdvisoryDetailed` | 1 | feishu |
| IOKit.framework | `_IOHIDCopyCFTypeParameter` | 1 | linearmouse |
| IOKit.framework | `_IOHIDDeviceGetReportWithCallback` | 1 | orbstack |
| IOKit.framework | `_IOHIDDeviceGetTypeID` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDDeviceGetValueWithCallback` | 1 | zoom |
| IOKit.framework | `_IOHIDDeviceSetInputValueMatching` | 1 | linearmouse |
| IOKit.framework | `_IOHIDDeviceSetValueMultiple` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDDeviceSetValueWithCallback` | 1 | zoom |
| IOKit.framework | `_IOHIDElementGetCollectionType` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementGetName` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementGetProperty` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementGetReportCount` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementGetUnit` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementGetUnitExponent` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementHasNullState` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementIsArray` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementIsNonLinear` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDElementIsWrapping` | 1 | microsoft-teams |
| IOKit.framework | `_IOHIDEventGetScrollMomentum` | 1 | orion |
| IOKit.framework | `_IOHIDEventGetTimeStamp` | 1 | orion |
| IOKit.framework | `_IOHIDEventGetType` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientActivate` | 1 | orion |
| IOKit.framework | `_IOHIDEventSystemClientRegisterEventBlock` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientRegisterPropertyChangedCallback` | 1 | linearmouse |
| IOKit.framework | `_IOHIDEventSystemClientSetDispatchQueue` | 1 | orion |
| IOKit.framework | `_IOHIDEventSystemClientSetProperty` | 1 | unnaturalscrollwheels |
| IOKit.framework | `_IOHIDGetParameter` | 1 | superwhisper |
| IOKit.framework | `_IOHIDManagerRegisterInputReportWithTimeStampCallback` | 1 | feishu |
| IOKit.framework | `_IOHIDManagerSetInputValueMatching` | 1 | wispr-flow |
| IOKit.framework | `_IOHIDQueueAddElement` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDQueueCopyNextValueWithTimeout` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDQueueCreate` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDQueueRegisterValueAvailableCallback` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDQueueScheduleWithRunLoop` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDQueueStart` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDQueueStop` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDQueueUnscheduleFromRunLoop` | 1 | karabiner-elements |
| IOKit.framework | `_IOHIDSetCFTypeParameter` | 1 | bettertouchtool |
| IOKit.framework | `_IOHIDSetMouseLocation` | 1 | bettertouchtool |
| IOKit.framework | `_IOHIDUserDeviceCreateWithProperties` | 1 | openlogi |
| IOKit.framework | `_IOHIDUserDeviceHandleReportWithTimeStamp` | 1 | openlogi |
| IOKit.framework | `_IOHIDValueGetScaledValue` | 1 | orion |
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
| IOKit.framework | `_IORegistryEntryGetProperty` | 1 | virtualbox |
| IOKit.framework | `_IORegistryIteratorEnterEntry` | 1 | macs-fan-control |
| IOKit.framework | `_IORegistryIteratorExitEntry` | 1 | macs-fan-control |
| IOKit.framework | `_IOServiceWaitQuiet` | 1 | microsoft-teams |
| IOKit.framework | `_KextManagerCreateURLForBundleIdentifier` | 1 | epic-games |
| IOKit.framework | `_KextManagerLoadKextWithURL` | 1 | tunnelblick |
| IOKit.framework | `_OBJC_CLASS_$_HIDEvent` | 1 | mac-mouse-fix |
| IOUSBHost.framework | `_IOUSBHostDefaultControlCompletionTimeout` | 1 | sfm |
| IOUSBHost.framework | `_IOUSBHostDevicePropertyKeyCurrentConfiguration` | 1 | sfm |
| IOUSBHost.framework | `_IOUSBHostDevicePropertyKeySerialNumberString` | 1 | sfm |
| IOUSBHost.framework | `_IOUSBHostMatchingPropertyKeyDeviceClass` | 1 | sfm |
| IOUSBHost.framework | `_IOUSBHostMatchingPropertyKeyDeviceProtocol` | 1 | sfm |
| IOUSBHost.framework | `_IOUSBHostMatchingPropertyKeyDeviceReleaseNumber` | 1 | sfm |
| IOUSBHost.framework | `_IOUSBHostMatchingPropertyKeyDeviceSubClass` | 1 | sfm |
| IOUSBHost.framework | `_IOUSBHostMatchingPropertyKeyProductID` | 1 | sfm |
| IOUSBHost.framework | `_IOUSBHostMatchingPropertyKeySpeed` | 1 | sfm |
| IOUSBHost.framework | `_IOUSBHostMatchingPropertyKeyVendorID` | 1 | sfm |
| IOUSBHost.framework | `_IOUSBHostPropertyKeyLocationID` | 1 | sfm |
| IOUSBHost.framework | `_OBJC_CLASS_$_IOUSBHostCIDeviceStateMachine` | 1 | sfm |
| IOUSBHost.framework | `_OBJC_CLASS_$_IOUSBHostCIEndpointStateMachine` | 1 | sfm |
| IOUSBHost.framework | `_OBJC_CLASS_$_IOUSBHostControllerInterface` | 1 | sfm |
| IOUSBHost.framework | `_OBJC_CLASS_$_IOUSBHostInterface` | 1 | sfm |
| MachineSettings.framework | `_getEVSHandle` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTActuationActuate` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTActuationCreateFromDictionary` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTActuatorClose` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTActuatorGetSystemActuationsEnabled` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTActuatorOpen` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTActuatorSetSystemActuationsEnabled` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTDeviceGetFamilyID` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTDeviceGetMTActuator` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTDeviceGetSensorSurfaceDimensions` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTDeviceGetService` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTDeviceGetSystemForceResponseEnabled` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTDeviceIsBuiltIn` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTDeviceRelease` | 1 | middleclick |
| MultitouchSupport.framework | `_MTDeviceSetSurfaceOrientation` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTDeviceSetSystemForceResponseEnabled` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTDeviceSupportsForce` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTRegisterButtonStateCallback` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_MTUnregisterButtonStateCallback` | 1 | bettertouchtool |
| MultitouchSupport.framework | `_mt_CreateSavedNameForDevice` | 1 | bettertouchtool |
| PCSC.framework | `_SCardListReaderGroups` | 1 | crossover |
| PCSC.framework | `_SCardSetAttrib` | 1 | crossover |
| PCSC.framework | `_g_rgSCardT0Pci` | 1 | keepassxc |
| PCSC.framework | `_g_rgSCardT1Pci` | 1 | keepassxc |
| PCSC.framework | `_pcsc_stringify_error` | 1 | anki |
| Virtualization.framework | `_$sSo16VZVirtualMachineC14VirtualizationE5pause17completionHandleryys6ResultOyyts5Error_pGc_tF` | 1 | orbstack |
| Virtualization.framework | `_$sSo16VZVirtualMachineC14VirtualizationE5start17completionHandleryys6ResultOyyts5Error_pGc_tF` | 1 | orbstack |
| Virtualization.framework | `_$sSo16VZVirtualMachineC14VirtualizationE6resume17completionHandleryys6ResultOyyts5Error_pGc_tF` | 1 | orbstack |
| Virtualization.framework | `_$sSo20VZVirtioSocketDeviceC14VirtualizationE7connect6toPort17completionHandlerys6UInt32V_ys6ResultOySo0aB10ConnectionCs5Error_pGctF` | 1 | orbstack |
| Virtualization.framework | `_OBJC_CLASS_$_VZAudioDeviceConfiguration` | 1 | orbstack |
| Virtualization.framework | `_OBJC_CLASS_$_VZBridgedNetworkInterface` | 1 | rancher |
| Virtualization.framework | `_OBJC_CLASS_$_VZHostAudioOutputStreamSink` | 1 | orbstack |
| Virtualization.framework | `_OBJC_CLASS_$_VZSerialPortConfiguration` | 1 | orbstack |
| Virtualization.framework | `_OBJC_CLASS_$_VZVirtioSocketConnection` | 1 | orbstack |
| Virtualization.framework | `_OBJC_CLASS_$_VZVirtioSoundDeviceConfiguration` | 1 | orbstack |
| Virtualization.framework | `_OBJC_CLASS_$_VZVirtioSoundDeviceOutputStreamConfiguration` | 1 | orbstack |
| Virtualization.framework | `_OBJC_CLASS_$_VZVirtioSoundDeviceStreamConfiguration` | 1 | orbstack |
| libswiftIOKit.dylib | `_$s5IOKit16kIOReturnNotOpens5Int32Vvg` | 1 | linearmouse |
| libswiftIOKit.dylib | `_$s5IOKit17kIOReturnNotReadys5Int32Vvg` | 1 | finetune |
| libswiftIOKit.dylib | `_$s5IOKit20kIOReturnNoResourcess5Int32Vvg` | 1 | coconutbattery |
| libswiftIOKit.dylib | `_$s5IOKit21kIOReturnNotPermitteds5Int32Vvg` | 1 | raycast |
| libswiftIOKit.dylib | `_$s5IOKit22kIOReturnNotPrivilegeds5Int32Vvg` | 1 | aldente |
