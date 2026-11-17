# Lacunas de Camila (desktop e window server)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Camila (desktop e window server) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**191** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreGraphics.framework | `_CGDisplayCopyAllDisplayModes` | 28 | android-studio, anydesk, bettertouchtool, cryptomator |
| CoreGraphics.framework | `_CGEventTapCreateForPid` | 26 | audacity, bartender, bettertouchtool, calibre |
| CoreGraphics.framework | `_CGEventSourceButtonState` | 24 | android-studio, bartender, bettertouchtool, cmux |
| CoreGraphics.framework | `_CGDisplaySetDisplayMode` | 23 | alacritty, anydesk, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGDisplayCapture` | 21 | alacritty, android-studio, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGDisplayModeCopyPixelEncoding` | 20 | android-studio, cryptomator, dbeaver-community, deluge |
| CoreGraphics.framework | `_CGEventSourceCounterForEventType` | 20 | adobe-acrobat-reader, another-redis-desktop-manager, bartender, battery |
| CoreGraphics.framework | `_CGDisplayRelease` | 19 | alacritty, android-studio, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGAcquireDisplayFadeReservation` | 18 | alacritty, betterdisplay, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGDisplayFade` | 18 | alacritty, betterdisplay, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGDisplayMoveCursorToPoint` | 18 | betterdisplay, blender, deluge, gimp |
| CoreGraphics.framework | `_CGReleaseDisplayFadeReservation` | 18 | alacritty, betterdisplay, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGGetDisplayTransferByTable` | 16 | adobe-acrobat-reader, betterdisplay, bettertouchtool, deluge |
| CoreGraphics.framework | `_CGRestorePermanentDisplayConfiguration` | 16 | alacritty, android-studio, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGDisplayModeGetIOFlags` | 15 | android-studio, iina, intellij-idea, jetbrains-toolbox |
| CoreGraphics.framework | `_CGEventCreateCopy` | 15 | alfred, bettertouchtool, cleanshot, duckduckgo |
| CoreGraphics.framework | `_CGSetDisplayTransferByTable` | 15 | adobe-acrobat-reader, betterdisplay, bettertouchtool, deluge |
| CoreGraphics.framework | `_CGDisplayIOServicePort` | 14 | amethyst, bettertouchtool, blender, gemini |
| CoreGraphics.framework | `_CGDisplayModeRetain` | 14 | alacritty, cc-switch, clash-verge-rev, cryptomator |
| CoreGraphics.framework | `_CGDisplayShowCursor` | 14 | bartender, bettertouchtool, claude, cryptomator |
| CoreGraphics.framework | `_CGBeginDisplayConfiguration` | 13 | android-studio, betterdisplay, bettertouchtool, cryptomator |
| CoreGraphics.framework | `_CGCompleteDisplayConfiguration` | 13 | android-studio, betterdisplay, bettertouchtool, cryptomator |
| CoreGraphics.framework | `_kCGDisplayShowDuplicateLowResolutionModes` | 13 | android-studio, anydesk, bettertouchtool, intellij-idea |
| CoreGraphics.framework | `_CGDisplayHideCursor` | 12 | bartender, bettertouchtool, claude, cryptomator |
| CoreGraphics.framework | `_CGGetLastMouseDelta` | 12 | bitwarden, deluge, gemini, lmms |
| CoreGraphics.framework | `_CGSCopySpacesForWindows` | 12 | alt-tab, amethyst, bartender, bettertouchtool |
| CoreGraphics.framework | `_CGDisplayModeIsUsableForDesktopGUI` | 11 | android-studio, anydesk, intellij-idea, jetbrains-toolbox |
| CoreGraphics.framework | `_CGSGetActiveSpace` | 9 | bartender, bettertouchtool, ghostty, jordanbaird-ice |
| CoreGraphics.framework | `_CGSetLocalEventsSuppressionInterval` | 9 | android-studio, bettertouchtool, cryptomator, dbeaver-community |
| CoreGraphics.framework | `_CGCaptureAllDisplays` | 8 | deluge, krita, lmms, love |
| CoreGraphics.framework | `_CGConfigureDisplayWithDisplayMode` | 8 | android-studio, bettertouchtool, cryptomator, dbeaver-community |
| CoreGraphics.framework | `_CGDisplayRestoreColorSyncSettings` | 8 | anydesk, betterdisplay, bettertouchtool, deluge |
| CoreGraphics.framework | `_CGDisplayStreamCreateWithDispatchQueue` | 8 | alt-tab, anydesk, obs, rustdesk |
| CoreGraphics.framework | `_CGEventSourceSetLocalEventsFilterDuringSuppressionState` | 8 | bartender, bettertouchtool, clipy, jordanbaird-ice |
| CoreGraphics.framework | `_CGReleaseAllDisplays` | 8 | deluge, krita, lmms, love |
| CoreGraphics.framework | `_CGSDefaultConnectionForThread` | 8 | alfred, cmux, ghostty, iterm2 |
| CoreGraphics.framework | `_CGSSetWindowBackgroundBlurRadius` | 8 | alacritty, alfred, cmux, ghostty |
| CoreGraphics.framework | `_CGSShutdownServerConnections` | 8 | firefox, floorp, librewolf, mullvad-browser |
| CoreGraphics.framework | `_CGSSpaceGetType` | 8 | bartender, bettertouchtool, ghostty, jordanbaird-ice |
| CoreGraphics.framework | `_CGCancelDisplayConfiguration` | 7 | android-studio, betterdisplay, bettertouchtool, hammerspoon |
| CoreGraphics.framework | `_CGSCopyManagedDisplaySpaces` | 7 | alt-tab, amethyst, bettertouchtool, dockdoor |
| CoreGraphics.framework | `_CGSetLocalEventsFilterDuringSuppressionState` | 7 | android-studio, bettertouchtool, cryptomator, dbeaver-community |
| CoreGraphics.framework | `_CGDisplayIsInHWMirrorSet` | 6 | android-studio, betterdisplay, intellij-idea, jetbrains-toolbox |
| CoreGraphics.framework | `_CGDisplayPrimaryDisplay` | 6 | betterdisplay, mac-mouse-fix, microsoft-teams, openlogi |
| CoreGraphics.framework | `_CGSSetSymbolicHotKeyEnabled` | 6 | alt-tab, amethyst, bettertouchtool, linearmouse |
| CoreGraphics.framework | `_CGConfigureDisplayMirrorOfDisplay` | 5 | betterdisplay, bettertouchtool, hammerspoon, microsoft-teams |
| CoreGraphics.framework | `_CGSGetScreenRectForWindow` | 5 | bartender, bettertouchtool, jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSGetSymbolicHotKeyValue` | 5 | amethyst, bettertouchtool, linearmouse, mac-mouse-fix |
| CoreGraphics.framework | `_CGSIsSymbolicHotKeyEnabled` | 5 | amethyst, bettertouchtool, linearmouse, mac-mouse-fix |
| CoreGraphics.framework | `_CGSSetConnectionProperty` | 5 | bartender, bettertouchtool, jordanbaird-ice, mac-mouse-fix |
| CoreGraphics.framework | `_CGSSetGlobalHotKeyOperatingMode` | 5 | moonlight, realvnc-connect-viewer, utm, virtualbox |
| CoreGraphics.framework | `_CGDisplayIsCaptured` | 4 | deluge, lmms, xquartz, zoom |
| CoreGraphics.framework | `_CGEventCopyIOHIDEvent` | 4 | linearmouse, mac-mouse-fix, openlogi, scroll-reverser |
| CoreGraphics.framework | `_CGEventCreateSourceFromEvent` | 4 | anydesk, bettertouchtool, raycast, zoom |
| CoreGraphics.framework | `_CGEventSourceGetSourceStateID` | 4 | aldente, microsoft-teams, openclaw, raycast |
| CoreGraphics.framework | `_CGEventTapPostEvent` | 4 | bettertouchtool, fluidvoice, hammerspoon, vorssaint |
| CoreGraphics.framework | `_CGSEventIsAppUnresponsive` | 4 | bettertouchtool, hammerspoon, jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSGetOnScreenWindowCount` | 4 | bartender, bettertouchtool, jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSGetOnScreenWindowList` | 4 | bartender, bettertouchtool, jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSGetProcessMenuBarWindowList` | 4 | bartender, bettertouchtool, jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSGetWindowCount` | 4 | bartender, bettertouchtool, jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSGetWindowLevel` | 4 | alt-tab, dockdoor, openclaw, thaw |
| CoreGraphics.framework | `_CGSManagedDisplayGetCurrentSpace` | 4 | alt-tab, bettertouchtool, thaw, zoom |
| CoreGraphics.framework | `_CGConfigureDisplayOrigin` | 3 | betterdisplay, bettertouchtool, hammerspoon |
| CoreGraphics.framework | `_CGDisplayGammaTableCapacity` | 3 | hammerspoon, kitty, rustdesk |
| CoreGraphics.framework | `_CGEventGetUnflippedLocation` | 3 | jordanbaird-ice, linearmouse, thaw |
| CoreGraphics.framework | `_CGEventSourceSetKeyboardType` | 3 | android-studio, intellij-idea, jetbrains-toolbox |
| CoreGraphics.framework | `_CGSAddWindowsToSpaces` | 3 | bettertouchtool, kitty, openclaw |
| CoreGraphics.framework | `_CGSCopyActiveMenuBarDisplayIdentifier` | 3 | alt-tab, bartender, thaw |
| CoreGraphics.framework | `_CGSCopySpaces` | 3 | bettertouchtool, mac-mouse-fix, openclaw |
| CoreGraphics.framework | `_CGSCurrentCursorSeed` | 3 | cleanshot, rustdesk, zoom |
| CoreGraphics.framework | `_CGSGetWindowList` | 3 | bartender, bettertouchtool, jordanbaird-ice |
| CoreGraphics.framework | `_CGSHWCaptureWindowList` | 3 | alt-tab, bettertouchtool, dockdoor |
| CoreGraphics.framework | `_CGSetDisplayTransferByFormula` | 3 | anydesk, deluge, lmms |
| CoreGraphics.framework | `_CGDisplayStreamUpdateGetDropCount` | 2 | obs, zoom |
| CoreGraphics.framework | `_CGEventSourceGetPixelsPerLine` | 2 | bettertouchtool, zoom |
| CoreGraphics.framework | `_CGGetDisplayTransferByFormula` | 2 | deluge, lmms |
| CoreGraphics.framework | `_CGSConfigureDisplayMode` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSCopyManagedDisplayForSpace` | 2 | raycast, zoom |
| CoreGraphics.framework | `_CGSCopyWindowProperty` | 2 | alt-tab, dockdoor |
| CoreGraphics.framework | `_CGSCopyWindowsWithOptionsAndTags` | 2 | alt-tab, bettertouchtool |
| CoreGraphics.framework | `_CGSGetCurrentDisplayMode` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSGetCursorScale` | 2 | bettertouchtool, zoom |
| CoreGraphics.framework | `_CGSGetDisplayModeDescriptionOfLength` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSGetGlobalHotKeyOperatingMode` | 2 | realvnc-connect-viewer, virtualbox |
| CoreGraphics.framework | `_CGSGetNumberOfDisplayModes` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSMoveWindowsToManagedSpace` | 2 | bettertouchtool, raycast |
| CoreGraphics.framework | `_CGSSetSymbolicHotKeyValue` | 2 | bettertouchtool, mac-mouse-fix |
| CoreGraphics.framework | `_kCGDisplayStreamQueueDepth` | 2 | obs, rustdesk |
| CoreGraphics.framework | `_kCGDisplayStreamSourceRect` | 2 | obs, teamviewer |
| CoreGraphics.framework | `_kCGDisplayStreamYCbCrMatrix_ITU_R_709_2` | 2 | cleanshot, zoom |
| OSD.framework | `_OBJC_CLASS_$_OSDManager` | 2 | betterdisplay, monitorcontrol |
| SkyLight.framework | `_SLPSPostEventRecordTo` | 2 | alt-tab, amethyst |
| SkyLight.framework | `__SLPSSetFrontProcessWithOptions` | 2 | alt-tab, amethyst |
| ApplicationServices.framework | `_CGAcquireDisplayFadeReservation` | 1 | vlc |
| ApplicationServices.framework | `_CGDisplayFade` | 1 | vlc |
| ApplicationServices.framework | `_CGDisplayIOServicePort` | 1 | the-unarchiver |
| ApplicationServices.framework | `_CGReleaseDisplayFadeReservation` | 1 | vlc |
| ApplicationServices.framework | `_CGSCreateRegisteredCursorImage` | 1 | vlc |
| BezelServices.framework | `_ALCALSCopyALSServiceClient` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGConfigureDisplayFadeEffect` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGDisplayForceToGray` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGDisplayModeGetIODisplayModeID` | 1 | zoom |
| CoreGraphics.framework | `_CGDisplaySetInvertedPolarity` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGDisplayStreamUpdateCreateMergedUpdate` | 1 | teamviewer |
| CoreGraphics.framework | `_CGEventGetTypeID` | 1 | mos |
| CoreGraphics.framework | `_CGEventSetSource` | 1 | bettertouchtool |
| CoreGraphics.framework | `_CGEventSourceGetKeyboardType` | 1 | linearmouse |
| CoreGraphics.framework | `_CGEventSourceGetLocalEventsSuppressionInterval` | 1 | mac-mouse-fix |
| CoreGraphics.framework | `_CGEventSourceGetUserData` | 1 | anydesk |
| CoreGraphics.framework | `_CGRegisterScreenRefreshCallback` | 1 | zoom |
| CoreGraphics.framework | `_CGSActuateDeviceWithPattern` | 1 | bettertouchtool |
| CoreGraphics.framework | `_CGSConfigureDisplayEnabled` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGSCopyBestManagedDisplayForRect` | 1 | amethyst |
| CoreGraphics.framework | `_CGSCopyConnectionProperty` | 1 | bettertouchtool |
| CoreGraphics.framework | `_CGSCopyDisplayInfoDictionary` | 1 | zoom |
| CoreGraphics.framework | `_CGSCopyRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSCopyRegisteredCursorImages` | 1 | raycast |
| CoreGraphics.framework | `_CGSDiffRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSDisplayStatusQuery` | 1 | zoom |
| CoreGraphics.framework | `_CGSEventSetAppIsUnresponsiveNotificationTimeout` | 1 | thaw |
| CoreGraphics.framework | `_CGSGetDisplayList` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGSGetRegionBounds` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetRegisteredCursorDataSize` | 1 | raycast |
| CoreGraphics.framework | `_CGSGetWindowWorkspace` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetWorkspace` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetZoomParameters` | 1 | zoom |
| CoreGraphics.framework | `_CGSIntersectRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSIntersectRegionWithRect` | 1 | zoom |
| CoreGraphics.framework | `_CGSIsMenuBarVisibleOnSpace` | 1 | bettertouchtool |
| CoreGraphics.framework | `_CGSManagedDisplaySetCurrentSpace` | 1 | openclaw |
| CoreGraphics.framework | `_CGSNewEmptyRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSNewRegionWithRect` | 1 | zoom |
| CoreGraphics.framework | `_CGSNewRegionWithRectList` | 1 | zoom |
| CoreGraphics.framework | `_CGSNextRect` | 1 | zoom |
| CoreGraphics.framework | `_CGSOffsetRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSPointInRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSRegionEnumerator` | 1 | zoom |
| CoreGraphics.framework | `_CGSRegionInRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSRegionIntersectsRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSRegionIsEmpty` | 1 | zoom |
| CoreGraphics.framework | `_CGSRegionIsRectangular` | 1 | zoom |
| CoreGraphics.framework | `_CGSRegionsEqual` | 1 | zoom |
| CoreGraphics.framework | `_CGSRegisterCursorWithImages` | 1 | raycast |
| CoreGraphics.framework | `_CGSRegisterNotifyProc` | 1 | zoom |
| CoreGraphics.framework | `_CGSReleaseRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSReleaseRegionEnumerator` | 1 | zoom |
| CoreGraphics.framework | `_CGSRemoveNotifyProc` | 1 | zoom |
| CoreGraphics.framework | `_CGSRemoveRegisteredCursor` | 1 | raycast |
| CoreGraphics.framework | `_CGSRemoveWindowsFromSpaces` | 1 | openclaw |
| CoreGraphics.framework | `_CGSServiceForDisplayNumber` | 1 | monitorcontrol |
| CoreGraphics.framework | `_CGSSetCursorScale` | 1 | bettertouchtool |
| CoreGraphics.framework | `_CGSSetDebugOptions` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGSSetWindowListWorkspace` | 1 | raycast |
| CoreGraphics.framework | `_CGSSetWindowTags` | 1 | zoom |
| CoreGraphics.framework | `_CGSSpaceCopyName` | 1 | openclaw |
| CoreGraphics.framework | `_CGSSpaceCopyOwners` | 1 | openclaw |
| CoreGraphics.framework | `_CGSSpaceSetCompatID` | 1 | raycast |
| CoreGraphics.framework | `_CGSUnionRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSUnionRegionWithRect` | 1 | zoom |
| CoreGraphics.framework | `_CGSWarpCursorPosition` | 1 | bettertouchtool |
| CoreGraphics.framework | `_CGSXorRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGShieldingWindowID` | 1 | xquartz |
| CoreGraphics.framework | `_kCGDisplayStreamYCbCrMatrix` | 1 | zoom |
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
