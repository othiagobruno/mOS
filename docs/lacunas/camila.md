# Lacunas de Camila (desktop e window server)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Camila (desktop e window server) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**180** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreGraphics.framework | `_CGDisplayCopyAllDisplayModes` | 22 | android-studio, anydesk, dbeaver-community, deluge |
| CoreGraphics.framework | `_CGEventSourceButtonState` | 20 | android-studio, cmux, dbeaver-community, dockdoor |
| CoreGraphics.framework | `_CGEventTapCreateForPid` | 20 | audacity, calibre, db-browser-for-sqlite, dockdoor |
| CoreGraphics.framework | `_CGDisplayMoveCursorToPoint` | 18 | betterdisplay, blender, deluge, gimp |
| CoreGraphics.framework | `_CGDisplaySetDisplayMode` | 18 | alacritty, anydesk, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGDisplayCapture` | 17 | alacritty, android-studio, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGDisplayModeCopyPixelEncoding` | 17 | android-studio, dbeaver-community, deluge, intellij-idea |
| CoreGraphics.framework | `_CGEventSourceCounterForEventType` | 16 | adobe-acrobat-reader, codexbar, firefox, floorp |
| CoreGraphics.framework | `_CGAcquireDisplayFadeReservation` | 15 | alacritty, betterdisplay, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGDisplayFade` | 15 | alacritty, betterdisplay, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGDisplayRelease` | 15 | alacritty, android-studio, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGReleaseDisplayFadeReservation` | 15 | alacritty, betterdisplay, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGGetDisplayTransferByTable` | 14 | adobe-acrobat-reader, betterdisplay, deluge, hammerspoon |
| CoreGraphics.framework | `_CGSetDisplayTransferByTable` | 14 | adobe-acrobat-reader, betterdisplay, deluge, hammerspoon |
| CoreGraphics.framework | `_CGDisplayModeGetIOFlags` | 13 | android-studio, iina, intellij-idea, jetbrains-toolbox |
| CoreGraphics.framework | `_CGRestorePermanentDisplayConfiguration` | 12 | alacritty, android-studio, cc-switch, clash-verge-rev |
| CoreGraphics.framework | `_CGBeginDisplayConfiguration` | 11 | android-studio, betterdisplay, dbeaver-community, hammerspoon |
| CoreGraphics.framework | `_CGCompleteDisplayConfiguration` | 11 | android-studio, betterdisplay, dbeaver-community, hammerspoon |
| CoreGraphics.framework | `_CGDisplayIOServicePort` | 11 | blender, gemini, hammerspoon, krita |
| CoreGraphics.framework | `_kCGDisplayShowDuplicateLowResolutionModes` | 11 | android-studio, anydesk, intellij-idea, jetbrains-toolbox |
| CoreGraphics.framework | `_CGDisplayModeIsUsableForDesktopGUI` | 10 | android-studio, anydesk, intellij-idea, jetbrains-toolbox |
| CoreGraphics.framework | `_CGDisplayModeRetain` | 9 | alacritty, cc-switch, clash-verge-rev, dbeaver-community |
| CoreGraphics.framework | `_CGDisplayShowCursor` | 9 | claude, deluge, godot, jordanbaird-ice |
| CoreGraphics.framework | `_CGEventCreateCopy` | 9 | alfred, hammerspoon, iina, iterm2 |
| CoreGraphics.framework | `_CGGetLastMouseDelta` | 9 | bitwarden, deluge, gemini, lmms |
| CoreGraphics.framework | `_CGSCopySpacesForWindows` | 9 | alt-tab, dockdoor, ghostty, jordanbaird-ice |
| CoreGraphics.framework | `_CGCaptureAllDisplays` | 8 | deluge, krita, lmms, love |
| CoreGraphics.framework | `_CGReleaseAllDisplays` | 8 | deluge, krita, lmms, love |
| CoreGraphics.framework | `_CGSShutdownServerConnections` | 8 | firefox, floorp, librewolf, mullvad-browser |
| CoreGraphics.framework | `_CGDisplayHideCursor` | 7 | claude, godot, jordanbaird-ice, pixelorama |
| CoreGraphics.framework | `_CGDisplayRestoreColorSyncSettings` | 7 | anydesk, betterdisplay, deluge, hammerspoon |
| CoreGraphics.framework | `_CGDisplayStreamCreateWithDispatchQueue` | 7 | alt-tab, anydesk, obs, rustdesk |
| CoreGraphics.framework | `_CGSDefaultConnectionForThread` | 7 | alfred, cmux, ghostty, iterm2 |
| CoreGraphics.framework | `_CGSGetActiveSpace` | 7 | ghostty, jordanbaird-ice, kitty, openclaw |
| CoreGraphics.framework | `_CGCancelDisplayConfiguration` | 6 | android-studio, betterdisplay, hammerspoon, intellij-idea |
| CoreGraphics.framework | `_CGConfigureDisplayWithDisplayMode` | 6 | android-studio, dbeaver-community, intellij-idea, jetbrains-toolbox |
| CoreGraphics.framework | `_CGDisplayIsInHWMirrorSet` | 6 | android-studio, betterdisplay, intellij-idea, jetbrains-toolbox |
| CoreGraphics.framework | `_CGSSetWindowBackgroundBlurRadius` | 6 | alacritty, alfred, cmux, ghostty |
| CoreGraphics.framework | `_CGSSpaceGetType` | 6 | ghostty, jordanbaird-ice, openclaw, raycast |
| CoreGraphics.framework | `_CGSetLocalEventsSuppressionInterval` | 6 | android-studio, dbeaver-community, intellij-idea, jetbrains-toolbox |
| CoreGraphics.framework | `_CGEventSourceSetLocalEventsFilterDuringSuppressionState` | 5 | clipy, jordanbaird-ice, maccy, raycast |
| CoreGraphics.framework | `_CGSCopyManagedDisplaySpaces` | 5 | alt-tab, dockdoor, openclaw, raycast |
| CoreGraphics.framework | `_CGSetLocalEventsFilterDuringSuppressionState` | 5 | android-studio, dbeaver-community, intellij-idea, jetbrains-toolbox |
| CoreGraphics.framework | `_CGConfigureDisplayMirrorOfDisplay` | 4 | betterdisplay, hammerspoon, microsoft-teams, monitorcontrol |
| CoreGraphics.framework | `_CGDisplayIsCaptured` | 4 | deluge, lmms, xquartz, zoom |
| CoreGraphics.framework | `_CGDisplayPrimaryDisplay` | 4 | betterdisplay, microsoft-teams, openlogi, vorssaint |
| CoreGraphics.framework | `_CGEventSourceGetSourceStateID` | 4 | aldente, microsoft-teams, openclaw, raycast |
| CoreGraphics.framework | `_CGSGetWindowLevel` | 4 | alt-tab, dockdoor, openclaw, thaw |
| CoreGraphics.framework | `_CGSSetGlobalHotKeyOperatingMode` | 4 | moonlight, utm, virtualbox, zoom |
| CoreGraphics.framework | `_CGDisplayGammaTableCapacity` | 3 | hammerspoon, kitty, rustdesk |
| CoreGraphics.framework | `_CGEventCopyIOHIDEvent` | 3 | linearmouse, openlogi, scroll-reverser |
| CoreGraphics.framework | `_CGEventCreateSourceFromEvent` | 3 | anydesk, raycast, zoom |
| CoreGraphics.framework | `_CGEventGetUnflippedLocation` | 3 | jordanbaird-ice, linearmouse, thaw |
| CoreGraphics.framework | `_CGEventSourceSetKeyboardType` | 3 | android-studio, intellij-idea, jetbrains-toolbox |
| CoreGraphics.framework | `_CGSEventIsAppUnresponsive` | 3 | hammerspoon, jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSGetScreenRectForWindow` | 3 | jordanbaird-ice, thaw, zoom |
| CoreGraphics.framework | `_CGSManagedDisplayGetCurrentSpace` | 3 | alt-tab, thaw, zoom |
| CoreGraphics.framework | `_CGSSetSymbolicHotKeyEnabled` | 3 | alt-tab, linearmouse, raycast |
| CoreGraphics.framework | `_CGSetDisplayTransferByFormula` | 3 | anydesk, deluge, lmms |
| CoreGraphics.framework | `_CGConfigureDisplayOrigin` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGDisplayStreamUpdateGetDropCount` | 2 | obs, zoom |
| CoreGraphics.framework | `_CGEventTapPostEvent` | 2 | hammerspoon, vorssaint |
| CoreGraphics.framework | `_CGGetDisplayTransferByFormula` | 2 | deluge, lmms |
| CoreGraphics.framework | `_CGSAddWindowsToSpaces` | 2 | kitty, openclaw |
| CoreGraphics.framework | `_CGSConfigureDisplayMode` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSCopyActiveMenuBarDisplayIdentifier` | 2 | alt-tab, thaw |
| CoreGraphics.framework | `_CGSCopyManagedDisplayForSpace` | 2 | raycast, zoom |
| CoreGraphics.framework | `_CGSCopyWindowProperty` | 2 | alt-tab, dockdoor |
| CoreGraphics.framework | `_CGSCurrentCursorSeed` | 2 | rustdesk, zoom |
| CoreGraphics.framework | `_CGSGetCurrentDisplayMode` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSGetDisplayModeDescriptionOfLength` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSGetNumberOfDisplayModes` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSGetOnScreenWindowCount` | 2 | jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSGetOnScreenWindowList` | 2 | jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSGetProcessMenuBarWindowList` | 2 | jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSGetSymbolicHotKeyValue` | 2 | linearmouse, raycast |
| CoreGraphics.framework | `_CGSGetWindowCount` | 2 | jordanbaird-ice, thaw |
| CoreGraphics.framework | `_CGSHWCaptureWindowList` | 2 | alt-tab, dockdoor |
| CoreGraphics.framework | `_CGSIsSymbolicHotKeyEnabled` | 2 | linearmouse, raycast |
| CoreGraphics.framework | `_CGSSetConnectionProperty` | 2 | jordanbaird-ice, thaw |
| CoreGraphics.framework | `_kCGDisplayStreamQueueDepth` | 2 | obs, rustdesk |
| OSD.framework | `_OBJC_CLASS_$_OSDManager` | 2 | betterdisplay, monitorcontrol |
| ApplicationServices.framework | `_CGAcquireDisplayFadeReservation` | 1 | vlc |
| ApplicationServices.framework | `_CGDisplayFade` | 1 | vlc |
| ApplicationServices.framework | `_CGDisplayIOServicePort` | 1 | the-unarchiver |
| ApplicationServices.framework | `_CGReleaseDisplayFadeReservation` | 1 | vlc |
| ApplicationServices.framework | `_CGSCreateRegisteredCursorImage` | 1 | vlc |
| CoreGraphics.framework | `_CGConfigureDisplayFadeEffect` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGDisplayForceToGray` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGDisplayModeGetIODisplayModeID` | 1 | zoom |
| CoreGraphics.framework | `_CGDisplaySetInvertedPolarity` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGEventGetTypeID` | 1 | mos |
| CoreGraphics.framework | `_CGEventSourceGetKeyboardType` | 1 | linearmouse |
| CoreGraphics.framework | `_CGEventSourceGetPixelsPerLine` | 1 | zoom |
| CoreGraphics.framework | `_CGEventSourceGetUserData` | 1 | anydesk |
| CoreGraphics.framework | `_CGRegisterScreenRefreshCallback` | 1 | zoom |
| CoreGraphics.framework | `_CGSConfigureDisplayEnabled` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGSCopyDisplayInfoDictionary` | 1 | zoom |
| CoreGraphics.framework | `_CGSCopyRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSCopyRegisteredCursorImages` | 1 | raycast |
| CoreGraphics.framework | `_CGSCopySpaces` | 1 | openclaw |
| CoreGraphics.framework | `_CGSCopyWindowsWithOptionsAndTags` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSDiffRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSDisplayStatusQuery` | 1 | zoom |
| CoreGraphics.framework | `_CGSEventSetAppIsUnresponsiveNotificationTimeout` | 1 | thaw |
| CoreGraphics.framework | `_CGSGetCursorScale` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetDisplayList` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGSGetGlobalHotKeyOperatingMode` | 1 | virtualbox |
| CoreGraphics.framework | `_CGSGetRegionBounds` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetRegisteredCursorDataSize` | 1 | raycast |
| CoreGraphics.framework | `_CGSGetWindowList` | 1 | jordanbaird-ice |
| CoreGraphics.framework | `_CGSGetWindowWorkspace` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetWorkspace` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetZoomParameters` | 1 | zoom |
| CoreGraphics.framework | `_CGSIntersectRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSIntersectRegionWithRect` | 1 | zoom |
| CoreGraphics.framework | `_CGSManagedDisplaySetCurrentSpace` | 1 | openclaw |
| CoreGraphics.framework | `_CGSMoveWindowsToManagedSpace` | 1 | raycast |
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
| CoreGraphics.framework | `_CGSSetDebugOptions` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGSSetWindowListWorkspace` | 1 | raycast |
| CoreGraphics.framework | `_CGSSetWindowTags` | 1 | zoom |
| CoreGraphics.framework | `_CGSSpaceCopyName` | 1 | openclaw |
| CoreGraphics.framework | `_CGSSpaceCopyOwners` | 1 | openclaw |
| CoreGraphics.framework | `_CGSSpaceSetCompatID` | 1 | raycast |
| CoreGraphics.framework | `_CGSUnionRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSUnionRegionWithRect` | 1 | zoom |
| CoreGraphics.framework | `_CGSXorRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGShieldingWindowID` | 1 | xquartz |
| CoreGraphics.framework | `_kCGDisplayStreamSourceRect` | 1 | obs |
| CoreGraphics.framework | `_kCGDisplayStreamYCbCrMatrix` | 1 | zoom |
| CoreGraphics.framework | `_kCGDisplayStreamYCbCrMatrix_ITU_R_709_2` | 1 | zoom |
| SkyLight.framework | `_SLPSPostEventRecordTo` | 1 | alt-tab |
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
| SkyLight.framework | `__SLPSSetFrontProcessWithOptions` | 1 | alt-tab |
| SkyLight.framework | `_kSLSSpaceAbsoluteLevelNotificationCenterAtScreenLock` | 1 | betterdisplay |
