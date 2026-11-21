# Lacunas de Camila (desktop e window server)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Camila (desktop e window server) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**221** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreGraphics.framework | `_CGDisplayCopyAllDisplayModes` | 36 | android-studio, anydesk, bambu-studio, bettertouchtool |
| CoreGraphics.framework | `_CGEventSourceButtonState` | 33 | android-studio, bartender, bettertouchtool, capcut |
| CoreGraphics.framework | `_CGDisplayModeCopyPixelEncoding` | 31 | android-studio, bambu-studio, crossover, cryptomator |
| CoreGraphics.framework | `_CGEventTapCreateForPid` | 31 | anki, audacity, bartender, bettertouchtool |
| CoreGraphics.framework | `_CGDisplaySetDisplayMode` | 29 | alacritty, anydesk, bambu-studio, cc-switch |
| CoreGraphics.framework | `_CGEventSourceCounterForEventType` | 29 | adobe-acrobat-reader, another-redis-desktop-manager, anythingllm, arduino-ide |
| CoreGraphics.framework | `_CGDisplayCapture` | 24 | alacritty, android-studio, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGDisplayModeGetIOFlags` | 23 | android-studio, bambu-studio, crossover, datagrip |
| CoreGraphics.framework | `_CGRestorePermanentDisplayConfiguration` | 23 | alacritty, android-studio, bambu-studio, cc-switch |
| CoreGraphics.framework | `_CGDisplayMoveCursorToPoint` | 22 | bambu-studio, betterdisplay, blender, crossover |
| CoreGraphics.framework | `_CGDisplayRelease` | 22 | alacritty, android-studio, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGGetDisplayTransferByTable` | 19 | adobe-acrobat-reader, bambu-studio, betterdisplay, bettertouchtool |
| CoreGraphics.framework | `_kCGDisplayShowDuplicateLowResolutionModes` | 19 | android-studio, anydesk, bettertouchtool, crossover |
| CoreGraphics.framework | `_CGEventCreateCopy` | 18 | alfred, bettertouchtool, cleanshot, duckduckgo |
| CoreGraphics.framework | `_CGSetDisplayTransferByTable` | 18 | adobe-acrobat-reader, bambu-studio, betterdisplay, bettertouchtool |
| CoreGraphics.framework | `_CGBeginDisplayConfiguration` | 17 | android-studio, betterdisplay, bettertouchtool, cryptomator |
| CoreGraphics.framework | `_CGCompleteDisplayConfiguration` | 17 | android-studio, betterdisplay, bettertouchtool, cryptomator |
| CoreGraphics.framework | `_CGDisplayIOServicePort` | 17 | amethyst, bettertouchtool, blender, crossover |
| CoreGraphics.framework | `_CGDisplayShowCursor` | 16 | bartender, bettertouchtool, claude, cryptomator |
| CoreGraphics.framework | `_CGDisplayModeIsUsableForDesktopGUI` | 15 | android-studio, anydesk, crossover, datagrip |
| CoreGraphics.framework | `_CGDisplayModeRetain` | 15 | alacritty, cc-switch, clash-verge-rev, crossover |
| CoreGraphics.framework | `_CGDisplayHideCursor` | 14 | bartender, bettertouchtool, claude, cryptomator |
| CoreGraphics.framework | `_CGGetLastMouseDelta` | 13 | bitwarden, capcut, deluge, gemini |
| CoreGraphics.framework | `_CGSCopySpacesForWindows` | 12 | alt-tab, amethyst, bartender, bettertouchtool |
| CoreGraphics.framework | `_CGSetLocalEventsSuppressionInterval` | 12 | android-studio, bettertouchtool, crossover, cryptomator |
| CoreGraphics.framework | `_CGCancelDisplayConfiguration` | 11 | android-studio, betterdisplay, bettertouchtool, datagrip |
| CoreGraphics.framework | `_CGConfigureDisplayWithDisplayMode` | 10 | android-studio, bettertouchtool, cryptomator, datagrip |
| CoreGraphics.framework | `_CGDisplayStreamCreateWithDispatchQueue` | 10 | alt-tab, anydesk, anythingllm, capcut |
| CoreGraphics.framework | `_CGCaptureAllDisplays` | 9 | crossover, deluge, krita, lmms |
| CoreGraphics.framework | `_CGReleaseAllDisplays` | 9 | crossover, deluge, krita, lmms |
| CoreGraphics.framework | `_CGSGetActiveSpace` | 9 | bartender, bettertouchtool, ghostty, jordanbaird-ice |
| CoreGraphics.framework | `_CGSetLocalEventsFilterDuringSuppressionState` | 9 | android-studio, bettertouchtool, cryptomator, datagrip |
| CoreGraphics.framework | `_CGDisplayIsInHWMirrorSet` | 8 | android-studio, betterdisplay, datagrip, intellij-idea |
| CoreGraphics.framework | `_CGDisplayRestoreColorSyncSettings` | 8 | anydesk, betterdisplay, bettertouchtool, deluge |
| CoreGraphics.framework | `_CGEventSourceSetLocalEventsFilterDuringSuppressionState` | 8 | bartender, bettertouchtool, clipy, jordanbaird-ice |
| CoreGraphics.framework | `_CGSDefaultConnectionForThread` | 8 | alfred, cmux, ghostty, iterm2 |
| CoreGraphics.framework | `_CGSSetWindowBackgroundBlurRadius` | 8 | alacritty, alfred, cmux, ghostty |
| CoreGraphics.framework | `_CGSSpaceGetType` | 8 | bartender, bettertouchtool, ghostty, jordanbaird-ice |
| CoreGraphics.framework | `_CGConfigureDisplayMirrorOfDisplay` | 7 | betterdisplay, bettertouchtool, hammerspoon, microsoft-office |
| CoreGraphics.framework | `_CGSCopyManagedDisplaySpaces` | 7 | alt-tab, amethyst, bettertouchtool, dockdoor |
| CoreGraphics.framework | `_CGSGetScreenRectForWindow` | 7 | bartender, bettertouchtool, feishu, jordanbaird-ice |
| CoreGraphics.framework | `_CGSShutdownServerConnections` | 7 | firefox, floorp, librewolf, thunderbird |
| CoreGraphics.framework | `_CGDisplayGammaTableCapacity` | 6 | bambu-studio, crossover, hammerspoon, kitty |
| CoreGraphics.framework | `_CGDisplayPrimaryDisplay` | 6 | betterdisplay, mac-mouse-fix, microsoft-teams, openlogi |
| CoreGraphics.framework | `_CGEventCreateSourceFromEvent` | 6 | anydesk, bettertouchtool, capcut, crossover |
| CoreGraphics.framework | `_CGSSetConnectionProperty` | 6 | bartender, bettertouchtool, jordanbaird-ice, mac-mouse-fix |
| CoreGraphics.framework | `_CGSSetSymbolicHotKeyEnabled` | 6 | alt-tab, amethyst, bettertouchtool, linearmouse |
| CoreGraphics.framework | `_CGEventCopyIOHIDEvent` | 5 | linearmouse, mac-mouse-fix, openlogi, orion |
| CoreGraphics.framework | `_CGEventSourceSetKeyboardType` | 5 | android-studio, datagrip, intellij-idea, jetbrains-toolbox |
| CoreGraphics.framework | `_CGSEventIsAppUnresponsive` | 5 | bettertouchtool, hammerspoon, istat-menus, jordanbaird-ice |
| CoreGraphics.framework | `_CGSGetSymbolicHotKeyValue` | 5 | amethyst, bettertouchtool, linearmouse, mac-mouse-fix |
| CoreGraphics.framework | `_CGSGetWindowLevel` | 5 | alt-tab, dockdoor, dropbox, openclaw |
| CoreGraphics.framework | `_CGSIsSymbolicHotKeyEnabled` | 5 | amethyst, bettertouchtool, linearmouse, mac-mouse-fix |
| CoreGraphics.framework | `_CGSSetGlobalHotKeyOperatingMode` | 5 | moonlight, realvnc-connect-viewer, utm, virtualbox |
| CoreGraphics.framework | `_CGDisplayGetDrawingContext` | 4 | microsoft-excel, microsoft-office, microsoft-powerpoint, microsoft-word |
| CoreGraphics.framework | `_CGDisplayIsCaptured` | 4 | deluge, lmms, xquartz, zoom |
| CoreGraphics.framework | `_CGEventSourceGetPixelsPerLine` | 4 | bettertouchtool, capcut, crossover, zoom |
| CoreGraphics.framework | `_CGEventSourceGetSourceStateID` | 4 | aldente, microsoft-teams, openclaw, raycast |
| CoreGraphics.framework | `_CGEventTapPostEvent` | 4 | bettertouchtool, fluidvoice, hammerspoon, vorssaint |
| CoreGraphics.framework | `_CGSGetOnScreenWindowCount` | 4 | bartender, bettertouchtool, jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSGetOnScreenWindowList` | 4 | bartender, bettertouchtool, jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSGetProcessMenuBarWindowList` | 4 | bartender, bettertouchtool, jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSGetWindowCount` | 4 | bartender, bettertouchtool, jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSGetWindowList` | 4 | bartender, bettertouchtool, dropbox, jordanbaird-ice |
| CoreGraphics.framework | `_CGSHWCaptureWindowList` | 4 | alt-tab, bettertouchtool, dockdoor, orion |
| CoreGraphics.framework | `_CGSManagedDisplayGetCurrentSpace` | 4 | alt-tab, bettertouchtool, thaw, zoom |
| CoreGraphics.framework | `_CGConfigureDisplayOrigin` | 3 | betterdisplay, bettertouchtool, hammerspoon |
| CoreGraphics.framework | `_CGEventGetUnflippedLocation` | 3 | jordanbaird-ice, linearmouse, thaw |
| CoreGraphics.framework | `_CGSAddWindowsToSpaces` | 3 | bettertouchtool, kitty, openclaw |
| CoreGraphics.framework | `_CGSCopyActiveMenuBarDisplayIdentifier` | 3 | alt-tab, bartender, thaw |
| CoreGraphics.framework | `_CGSCopySpaces` | 3 | bettertouchtool, mac-mouse-fix, openclaw |
| CoreGraphics.framework | `_CGSCopyWindowProperty` | 3 | alt-tab, dockdoor, dropbox |
| CoreGraphics.framework | `_CGSCurrentCursorSeed` | 3 | cleanshot, rustdesk, zoom |
| CoreGraphics.framework | `_CGSetDisplayTransferByFormula` | 3 | anydesk, deluge, lmms |
| ApplicationServices.framework | `_CGEventGetUnflippedLocation` | 2 | mactex, mactex-no-gui |
| ApplicationServices.framework | `_CGEventTapCreateForPSN` | 2 | mactex, mactex-no-gui |
| CoreGraphics.framework | `_CGDisplayIsAlwaysInMirrorSet` | 2 | microsoft-office, microsoft-powerpoint |
| CoreGraphics.framework | `_CGDisplayStreamUpdateGetDropCount` | 2 | obs, zoom |
| CoreGraphics.framework | `_CGGetDisplayTransferByFormula` | 2 | deluge, lmms |
| CoreGraphics.framework | `_CGSConfigureDisplayMode` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSCopyConnectionProperty` | 2 | bettertouchtool, orion |
| CoreGraphics.framework | `_CGSCopyManagedDisplayForSpace` | 2 | raycast, zoom |
| CoreGraphics.framework | `_CGSCopyWindowsWithOptionsAndTags` | 2 | alt-tab, bettertouchtool |
| CoreGraphics.framework | `_CGSGetCurrentDisplayMode` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSGetCursorScale` | 2 | bettertouchtool, zoom |
| CoreGraphics.framework | `_CGSGetDisplayModeDescriptionOfLength` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSGetGlobalHotKeyOperatingMode` | 2 | realvnc-connect-viewer, virtualbox |
| CoreGraphics.framework | `_CGSGetNumberOfDisplayModes` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSGetWindowBounds` | 2 | dropbox, feishu |
| CoreGraphics.framework | `_CGSMoveWindowsToManagedSpace` | 2 | bettertouchtool, raycast |
| CoreGraphics.framework | `_CGSNewRegionWithRect` | 2 | orion, zoom |
| CoreGraphics.framework | `_CGSNextRect` | 2 | orion, zoom |
| CoreGraphics.framework | `_CGSRegionEnumerator` | 2 | orion, zoom |
| CoreGraphics.framework | `_CGSRegisterConnectionNotifyProc` | 2 | dropbox, orion |
| CoreGraphics.framework | `_CGSRegisterNotifyProc` | 2 | orion, zoom |
| CoreGraphics.framework | `_CGSReleaseRegion` | 2 | orion, zoom |
| CoreGraphics.framework | `_CGSReleaseRegionEnumerator` | 2 | orion, zoom |
| CoreGraphics.framework | `_CGSSetSymbolicHotKeyValue` | 2 | bettertouchtool, mac-mouse-fix |
| CoreGraphics.framework | `_kCGDisplayStreamQueueDepth` | 2 | obs, rustdesk |
| CoreGraphics.framework | `_kCGDisplayStreamSourceRect` | 2 | obs, teamviewer |
| CoreGraphics.framework | `_kCGDisplayStreamYCbCrMatrix_ITU_R_709_2` | 2 | cleanshot, zoom |
| OSD.framework | `_OBJC_CLASS_$_OSDManager` | 2 | betterdisplay, monitorcontrol |
| SkyLight.framework | `_SLPSPostEventRecordTo` | 2 | alt-tab, amethyst |
| SkyLight.framework | `__SLPSSetFrontProcessWithOptions` | 2 | alt-tab, amethyst |
| ApplicationServices.framework | `_CGDisplayIOServicePort` | 1 | the-unarchiver |
| BezelServices.framework | `_ALCALSCopyALSServiceClient` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGConfigureDisplayFadeEffect` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGDisplayForceToGray` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGDisplayModeGetIODisplayModeID` | 1 | zoom |
| CoreGraphics.framework | `_CGDisplaySetInvertedPolarity` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGDisplayStreamUpdateCreateMergedUpdate` | 1 | teamviewer |
| CoreGraphics.framework | `_CGEventCreateNextEvent` | 1 | dropbox |
| CoreGraphics.framework | `_CGEventGetTypeID` | 1 | mos |
| CoreGraphics.framework | `_CGEventSetSource` | 1 | bettertouchtool |
| CoreGraphics.framework | `_CGEventSourceGetKeyboardType` | 1 | linearmouse |
| CoreGraphics.framework | `_CGEventSourceGetLocalEventsSuppressionInterval` | 1 | mac-mouse-fix |
| CoreGraphics.framework | `_CGEventSourceGetUserData` | 1 | anydesk |
| CoreGraphics.framework | `_CGRegisterScreenRefreshCallback` | 1 | zoom |
| CoreGraphics.framework | `_CGSActuateDeviceWithPattern` | 1 | bettertouchtool |
| CoreGraphics.framework | `_CGSConfigureDisplayEnabled` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGSCopyBestManagedDisplayForRect` | 1 | amethyst |
| CoreGraphics.framework | `_CGSCopyDisplayInfoDictionary` | 1 | zoom |
| CoreGraphics.framework | `_CGSCopyRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSCopyRegisteredCursorImages` | 1 | raycast |
| CoreGraphics.framework | `_CGSDiffRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSDisplayStatusQuery` | 1 | zoom |
| CoreGraphics.framework | `_CGSEventSetAppIsUnresponsiveNotificationTimeout` | 1 | thaw |
| CoreGraphics.framework | `_CGSGetDisplayList` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGSGetEventPort` | 1 | dropbox |
| CoreGraphics.framework | `_CGSGetPerformanceData` | 1 | istat-menus |
| CoreGraphics.framework | `_CGSGetRegionBounds` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetRegisteredCursorDataSize` | 1 | raycast |
| CoreGraphics.framework | `_CGSGetWindowWorkspace` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetWorkspace` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetZoomParameters` | 1 | zoom |
| CoreGraphics.framework | `_CGSIntersectRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSIntersectRegionWithRect` | 1 | zoom |
| CoreGraphics.framework | `_CGSIsMenuBarVisibleOnSpace` | 1 | bettertouchtool |
| CoreGraphics.framework | `_CGSManagedDisplaySetCurrentSpace` | 1 | openclaw |
| CoreGraphics.framework | `_CGSNewConnection` | 1 | dropbox |
| CoreGraphics.framework | `_CGSNewEmptyRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSNewRegionWithRectList` | 1 | zoom |
| CoreGraphics.framework | `_CGSOffsetRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSPackagesEnableConnectionOcclusionNotifications` | 1 | orion |
| CoreGraphics.framework | `_CGSPackagesEnableConnectionWindowModificationNotifications` | 1 | orion |
| CoreGraphics.framework | `_CGSPointInRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSRegionInRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSRegionIntersectsRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSRegionIsEmpty` | 1 | zoom |
| CoreGraphics.framework | `_CGSRegionIsRectangular` | 1 | zoom |
| CoreGraphics.framework | `_CGSRegionsEqual` | 1 | zoom |
| CoreGraphics.framework | `_CGSRegisterCursorWithImages` | 1 | raycast |
| CoreGraphics.framework | `_CGSRemoveConnectionNotifyProc` | 1 | dropbox |
| CoreGraphics.framework | `_CGSRemoveNotifyProc` | 1 | zoom |
| CoreGraphics.framework | `_CGSRemoveRegisteredCursor` | 1 | raycast |
| CoreGraphics.framework | `_CGSRemoveWindowsFromSpaces` | 1 | openclaw |
| CoreGraphics.framework | `_CGSRequestNotificationsForWindows` | 1 | dropbox |
| CoreGraphics.framework | `_CGSSecondsSinceLastInputEvent` | 1 | dropbox |
| CoreGraphics.framework | `_CGSServiceForDisplayNumber` | 1 | monitorcontrol |
| CoreGraphics.framework | `_CGSSetCursorScale` | 1 | bettertouchtool |
| CoreGraphics.framework | `_CGSSetDebugOptions` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGSSetWindowClipShape` | 1 | orion |
| CoreGraphics.framework | `_CGSSetWindowListWorkspace` | 1 | raycast |
| CoreGraphics.framework | `_CGSSetWindowTags` | 1 | zoom |
| CoreGraphics.framework | `_CGSSpaceCopyName` | 1 | openclaw |
| CoreGraphics.framework | `_CGSSpaceCopyOwners` | 1 | openclaw |
| CoreGraphics.framework | `_CGSSpaceSetCompatID` | 1 | raycast |
| CoreGraphics.framework | `_CGSUnionRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSUnionRegionWithRect` | 1 | zoom |
| CoreGraphics.framework | `_CGSWarpCursorPosition` | 1 | bettertouchtool |
| CoreGraphics.framework | `_CGSWindowIsOrderedIn` | 1 | dropbox |
| CoreGraphics.framework | `_CGSXorRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGShadingCreateConic` | 1 | orion |
| CoreGraphics.framework | `_CGShieldingWindowID` | 1 | xquartz |
| CoreGraphics.framework | `_CGStyleCreateColorMatrix` | 1 | orion |
| CoreGraphics.framework | `_CGStyleCreateFocusRingWithColor` | 1 | orion |
| CoreGraphics.framework | `_CGStyleCreateGaussianBlur` | 1 | orion |
| CoreGraphics.framework | `_CGStyleCreateShadow2` | 1 | orion |
| CoreGraphics.framework | `_CGStyleGetColor` | 1 | orion |
| CoreGraphics.framework | `_CGStyleGetData` | 1 | orion |
| CoreGraphics.framework | `_CGStyleGetType` | 1 | orion |
| CoreGraphics.framework | `_kCGDisplayStreamYCbCrMatrix` | 1 | zoom |
| CoreGraphics.framework | `_kCGDisplayStreamYCbCrMatrix_SMPTE_240M_1995` | 1 | orion |
| SkyLight.framework | `_SLSCopyDisplayInfoDictionary` | 1 | betterdisplay |
| SkyLight.framework | `_SLSDetectDisplays` | 1 | betterdisplay |
| SkyLight.framework | `_SLSDisplayGetActivePresetIndex` | 1 | betterdisplay |
| SkyLight.framework | `_SLSDisplayGetPresetCount` | 1 | betterdisplay |
| SkyLight.framework | `_SLSDisplaySetActivePresetIndex` | 1 | betterdisplay |
| SkyLight.framework | `_SLSDisplaySetUnderscan` | 1 | betterdisplay |
| SkyLight.framework | `_SLSGetAppearanceThemeLegacy` | 1 | betterdisplay |
| SkyLight.framework | `_SLSGetZoomParameters` | 1 | betterdisplay |
| SkyLight.framework | `_SLSMainConnectionID` | 1 | betterdisplay |
| SkyLight.framework | `_SLSRegisterConnectionNotifyProc` | 1 | alt-tab |
| SkyLight.framework | `_SLSRequestNotificationsForWindows` | 1 | alt-tab |
| SkyLight.framework | `_SLSSetAppearanceThemeLegacy` | 1 | betterdisplay |
| SkyLight.framework | `_SLSSetDisplayRotation` | 1 | betterdisplay |
| SkyLight.framework | `_SLSSpaceSetFrontPSN` | 1 | alt-tab |
| SkyLight.framework | `_SLSWindowIteratorAdvance` | 1 | alt-tab |
| SkyLight.framework | `_SLSWindowIteratorCopyTitle` | 1 | alt-tab |
| SkyLight.framework | `_SLSWindowIteratorGetAlpha` | 1 | alt-tab |
| SkyLight.framework | `_SLSWindowIteratorGetAttributes` | 1 | alt-tab |
| SkyLight.framework | `_SLSWindowIteratorGetLevel` | 1 | alt-tab |
| SkyLight.framework | `_SLSWindowIteratorGetPID` | 1 | alt-tab |
| SkyLight.framework | `_SLSWindowIteratorGetParentID` | 1 | alt-tab |
| SkyLight.framework | `_SLSWindowIteratorGetSpaceTypeMask` | 1 | alt-tab |
| SkyLight.framework | `_SLSWindowIteratorGetTags` | 1 | alt-tab |
| SkyLight.framework | `_SLSWindowIteratorGetWindowID` | 1 | alt-tab |
| SkyLight.framework | `_SLSWindowQueryResultCopyWindows` | 1 | alt-tab |
| SkyLight.framework | `_SLSWindowQueryWindows` | 1 | alt-tab |
| SkyLight.framework | `_kSLSSpaceAbsoluteLevelNotificationCenterAtScreenLock` | 1 | betterdisplay |
| self) | `_CGBeginDisplayConfiguration` | 1 | feishu |
| self) | `_CGCompleteDisplayConfiguration` | 1 | feishu |
| self) | `_CGEventTapCreateForPSN` | 1 | dropbox |
| self) | `_CGRegisterScreenRefreshCallback` | 1 | dropbox |
| self) | `_CGSConfigureDisplayMode` | 1 | feishu |
| self) | `_CGSGetCurrentDisplayMode` | 1 | feishu |
| self) | `_CGSGetDisplayModeDescriptionOfLength` | 1 | feishu |
| self) | `_CGSGetNumberOfDisplayModes` | 1 | feishu |
| self) | `_CGSNewConnection` | 1 | feishu |
| self) | `_CGScreenRegisterMoveCallback` | 1 | dropbox |
| self) | `_CGScreenUnregisterMoveCallback` | 1 | dropbox |
