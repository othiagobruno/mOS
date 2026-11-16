# Lacunas de Camila (desktop e window server)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Camila (desktop e window server) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**176** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreGraphics.framework | `_CGDisplayCopyAllDisplayModes` | 17 | android-studio, dbeaver-community, deluge, iina |
| CoreGraphics.framework | `_CGDisplayMoveCursorToPoint` | 15 | betterdisplay, blender, deluge, gimp |
| CoreGraphics.framework | `_CGEventSourceButtonState` | 15 | android-studio, cmux, dbeaver-community, dolphin |
| CoreGraphics.framework | `_CGDisplayModeCopyPixelEncoding` | 14 | android-studio, dbeaver-community, deluge, kicad |
| CoreGraphics.framework | `_CGDisplaySetDisplayMode` | 14 | alacritty, cc-switch, deluge, kicad |
| CoreGraphics.framework | `_CGEventTapCreateForPid` | 14 | audacity, db-browser-for-sqlite, dolphin, jordanbaird-ice |
| CoreGraphics.framework | `_CGDisplayCapture` | 13 | alacritty, android-studio, cc-switch, dbeaver-community |
| CoreGraphics.framework | `_CGGetDisplayTransferByTable` | 13 | betterdisplay, deluge, hammerspoon, kitty |
| CoreGraphics.framework | `_CGSetDisplayTransferByTable` | 13 | betterdisplay, deluge, hammerspoon, kitty |
| CoreGraphics.framework | `_CGAcquireDisplayFadeReservation` | 12 | alacritty, betterdisplay, cc-switch, deluge |
| CoreGraphics.framework | `_CGDisplayFade` | 12 | alacritty, betterdisplay, cc-switch, deluge |
| CoreGraphics.framework | `_CGReleaseDisplayFadeReservation` | 12 | alacritty, betterdisplay, cc-switch, deluge |
| CoreGraphics.framework | `_CGDisplayRelease` | 11 | alacritty, android-studio, cc-switch, dbeaver-community |
| CoreGraphics.framework | `_CGEventSourceCounterForEventType` | 11 | firefox, floorp, librewolf, mullvad-browser |
| CoreGraphics.framework | `_CGDisplayModeGetIOFlags` | 10 | android-studio, iina, kitty, krita |
| CoreGraphics.framework | `_CGBeginDisplayConfiguration` | 8 | android-studio, betterdisplay, dbeaver-community, hammerspoon |
| CoreGraphics.framework | `_CGCaptureAllDisplays` | 8 | deluge, krita, lmms, love |
| CoreGraphics.framework | `_CGCompleteDisplayConfiguration` | 8 | android-studio, betterdisplay, dbeaver-community, hammerspoon |
| CoreGraphics.framework | `_CGDisplayIOServicePort` | 8 | blender, hammerspoon, krita, love |
| CoreGraphics.framework | `_CGEventCreateCopy` | 8 | hammerspoon, iina, iterm2, linearmouse |
| CoreGraphics.framework | `_CGReleaseAllDisplays` | 8 | deluge, krita, lmms, love |
| CoreGraphics.framework | `_CGRestorePermanentDisplayConfiguration` | 8 | alacritty, android-studio, cc-switch, dbeaver-community |
| CoreGraphics.framework | `_CGDisplayModeRetain` | 7 | alacritty, cc-switch, dbeaver-community, deluge |
| CoreGraphics.framework | `_CGGetLastMouseDelta` | 7 | bitwarden, deluge, lmms, monitorcontrol |
| CoreGraphics.framework | `_CGSCopySpacesForWindows` | 7 | alt-tab, ghostty, jordanbaird-ice, kitty |
| CoreGraphics.framework | `_CGSShutdownServerConnections` | 7 | firefox, floorp, librewolf, mullvad-browser |
| CoreGraphics.framework | `_kCGDisplayShowDuplicateLowResolutionModes` | 7 | android-studio, krita, love, openrct2 |
| CoreGraphics.framework | `_CGDisplayModeIsUsableForDesktopGUI` | 6 | android-studio, krita, love, openrct2 |
| CoreGraphics.framework | `_CGDisplayRestoreColorSyncSettings` | 6 | betterdisplay, deluge, hammerspoon, lmms |
| CoreGraphics.framework | `_CGDisplayShowCursor` | 6 | claude, deluge, godot, jordanbaird-ice |
| CoreGraphics.framework | `_CGSGetActiveSpace` | 6 | ghostty, jordanbaird-ice, kitty, openclaw |
| CoreGraphics.framework | `_CGDisplayStreamCreateWithDispatchQueue` | 5 | alt-tab, obs, rustdesk, telegram |
| CoreGraphics.framework | `_CGSDefaultConnectionForThread` | 5 | cmux, ghostty, iterm2, kitty |
| CoreGraphics.framework | `_CGSSetWindowBackgroundBlurRadius` | 5 | alacritty, cmux, ghostty, kitty |
| CoreGraphics.framework | `_CGSSpaceGetType` | 5 | ghostty, jordanbaird-ice, openclaw, raycast |
| CoreGraphics.framework | `_CGCancelDisplayConfiguration` | 4 | android-studio, betterdisplay, hammerspoon, vorssaint |
| CoreGraphics.framework | `_CGConfigureDisplayWithDisplayMode` | 4 | android-studio, dbeaver-community, rustdesk, temurin |
| CoreGraphics.framework | `_CGDisplayHideCursor` | 4 | claude, godot, jordanbaird-ice, pixelorama |
| CoreGraphics.framework | `_CGDisplayIsCaptured` | 4 | deluge, lmms, xquartz, zoom |
| CoreGraphics.framework | `_CGDisplayIsInHWMirrorSet` | 4 | android-studio, betterdisplay, monitorcontrol, zoom |
| CoreGraphics.framework | `_CGEventSourceSetLocalEventsFilterDuringSuppressionState` | 4 | clipy, jordanbaird-ice, maccy, raycast |
| CoreGraphics.framework | `_CGConfigureDisplayMirrorOfDisplay` | 3 | betterdisplay, hammerspoon, monitorcontrol |
| CoreGraphics.framework | `_CGDisplayGammaTableCapacity` | 3 | hammerspoon, kitty, rustdesk |
| CoreGraphics.framework | `_CGSCopyManagedDisplaySpaces` | 3 | alt-tab, openclaw, raycast |
| CoreGraphics.framework | `_CGSSetSymbolicHotKeyEnabled` | 3 | alt-tab, linearmouse, raycast |
| CoreGraphics.framework | `_CGSetLocalEventsFilterDuringSuppressionState` | 3 | android-studio, dbeaver-community, temurin |
| CoreGraphics.framework | `_CGSetLocalEventsSuppressionInterval` | 3 | android-studio, dbeaver-community, temurin |
| CoreGraphics.framework | `_CGConfigureDisplayOrigin` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGDisplayPrimaryDisplay` | 2 | betterdisplay, vorssaint |
| CoreGraphics.framework | `_CGDisplayStreamUpdateGetDropCount` | 2 | obs, zoom |
| CoreGraphics.framework | `_CGEventCreateSourceFromEvent` | 2 | raycast, zoom |
| CoreGraphics.framework | `_CGEventGetUnflippedLocation` | 2 | jordanbaird-ice, linearmouse |
| CoreGraphics.framework | `_CGEventSourceGetSourceStateID` | 2 | openclaw, raycast |
| CoreGraphics.framework | `_CGEventTapPostEvent` | 2 | hammerspoon, vorssaint |
| CoreGraphics.framework | `_CGGetDisplayTransferByFormula` | 2 | deluge, lmms |
| CoreGraphics.framework | `_CGSAddWindowsToSpaces` | 2 | kitty, openclaw |
| CoreGraphics.framework | `_CGSConfigureDisplayMode` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSCopyManagedDisplayForSpace` | 2 | raycast, zoom |
| CoreGraphics.framework | `_CGSCurrentCursorSeed` | 2 | rustdesk, zoom |
| CoreGraphics.framework | `_CGSEventIsAppUnresponsive` | 2 | hammerspoon, jordanbaird-ice |
| CoreGraphics.framework | `_CGSGetCurrentDisplayMode` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSGetDisplayModeDescriptionOfLength` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSGetNumberOfDisplayModes` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSGetScreenRectForWindow` | 2 | jordanbaird-ice, zoom |
| CoreGraphics.framework | `_CGSGetSymbolicHotKeyValue` | 2 | linearmouse, raycast |
| CoreGraphics.framework | `_CGSGetWindowLevel` | 2 | alt-tab, openclaw |
| CoreGraphics.framework | `_CGSIsSymbolicHotKeyEnabled` | 2 | linearmouse, raycast |
| CoreGraphics.framework | `_CGSManagedDisplayGetCurrentSpace` | 2 | alt-tab, zoom |
| CoreGraphics.framework | `_CGSSetGlobalHotKeyOperatingMode` | 2 | utm, zoom |
| CoreGraphics.framework | `_CGSetDisplayTransferByFormula` | 2 | deluge, lmms |
| CoreGraphics.framework | `_kCGDisplayStreamQueueDepth` | 2 | obs, rustdesk |
| OSD.framework | `_OBJC_CLASS_$_OSDManager` | 2 | betterdisplay, monitorcontrol |
| ApplicationServices.framework | `_CGAcquireDisplayFadeReservation` | 1 | vlc |
| ApplicationServices.framework | `_CGDisplayFade` | 1 | vlc |
| ApplicationServices.framework | `_CGReleaseDisplayFadeReservation` | 1 | vlc |
| ApplicationServices.framework | `_CGSCreateRegisteredCursorImage` | 1 | vlc |
| CoreGraphics.framework | `_CGConfigureDisplayFadeEffect` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGDisplayForceToGray` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGDisplayModeGetIODisplayModeID` | 1 | zoom |
| CoreGraphics.framework | `_CGDisplaySetInvertedPolarity` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGEventCopyIOHIDEvent` | 1 | linearmouse |
| CoreGraphics.framework | `_CGEventGetTypeID` | 1 | mos |
| CoreGraphics.framework | `_CGEventSourceGetKeyboardType` | 1 | linearmouse |
| CoreGraphics.framework | `_CGEventSourceGetPixelsPerLine` | 1 | zoom |
| CoreGraphics.framework | `_CGEventSourceSetKeyboardType` | 1 | android-studio |
| CoreGraphics.framework | `_CGRegisterScreenRefreshCallback` | 1 | zoom |
| CoreGraphics.framework | `_CGSConfigureDisplayEnabled` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGSCopyActiveMenuBarDisplayIdentifier` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSCopyDisplayInfoDictionary` | 1 | zoom |
| CoreGraphics.framework | `_CGSCopyRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSCopyRegisteredCursorImages` | 1 | raycast |
| CoreGraphics.framework | `_CGSCopySpaces` | 1 | openclaw |
| CoreGraphics.framework | `_CGSCopyWindowProperty` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSCopyWindowsWithOptionsAndTags` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSDiffRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSDisplayStatusQuery` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetCursorScale` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetDisplayList` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGSGetOnScreenWindowCount` | 1 | jordanbaird-ice |
| CoreGraphics.framework | `_CGSGetOnScreenWindowList` | 1 | jordanbaird-ice |
| CoreGraphics.framework | `_CGSGetProcessMenuBarWindowList` | 1 | jordanbaird-ice |
| CoreGraphics.framework | `_CGSGetRegionBounds` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetRegisteredCursorDataSize` | 1 | raycast |
| CoreGraphics.framework | `_CGSGetWindowCount` | 1 | jordanbaird-ice |
| CoreGraphics.framework | `_CGSGetWindowList` | 1 | jordanbaird-ice |
| CoreGraphics.framework | `_CGSGetWindowWorkspace` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetWorkspace` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetZoomParameters` | 1 | zoom |
| CoreGraphics.framework | `_CGSHWCaptureWindowList` | 1 | alt-tab |
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
| CoreGraphics.framework | `_CGSSetConnectionProperty` | 1 | jordanbaird-ice |
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
