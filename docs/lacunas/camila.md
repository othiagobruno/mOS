# Lacunas de Camila (desktop e window server)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Camila (desktop e window server) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**171** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreGraphics.framework | `_CGDisplayCopyAllDisplayModes` | 15 | android-studio, dbeaver-community, deluge, iina |
| CoreGraphics.framework | `_CGDisplayMoveCursorToPoint` | 14 | betterdisplay, blender, deluge, gimp |
| CoreGraphics.framework | `_CGDisplaySetDisplayMode` | 13 | alacritty, cc-switch, deluge, kicad |
| CoreGraphics.framework | `_CGEventTapCreateForPid` | 13 | audacity, db-browser-for-sqlite, dolphin, jordanbaird-ice |
| CoreGraphics.framework | `_CGDisplayModeCopyPixelEncoding` | 12 | android-studio, dbeaver-community, deluge, kicad |
| CoreGraphics.framework | `_CGAcquireDisplayFadeReservation` | 11 | alacritty, betterdisplay, cc-switch, deluge |
| CoreGraphics.framework | `_CGDisplayCapture` | 11 | alacritty, android-studio, cc-switch, dbeaver-community |
| CoreGraphics.framework | `_CGDisplayFade` | 11 | alacritty, betterdisplay, cc-switch, deluge |
| CoreGraphics.framework | `_CGEventSourceButtonState` | 11 | android-studio, cmux, dbeaver-community, dolphin |
| CoreGraphics.framework | `_CGGetDisplayTransferByTable` | 11 | betterdisplay, deluge, hammerspoon, kitty |
| CoreGraphics.framework | `_CGReleaseDisplayFadeReservation` | 11 | alacritty, betterdisplay, cc-switch, deluge |
| CoreGraphics.framework | `_CGSetDisplayTransferByTable` | 11 | betterdisplay, deluge, hammerspoon, kitty |
| CoreGraphics.framework | `_CGDisplayModeGetIOFlags` | 10 | android-studio, iina, kitty, krita |
| CoreGraphics.framework | `_CGDisplayRelease` | 9 | alacritty, android-studio, cc-switch, dbeaver-community |
| CoreGraphics.framework | `_CGCaptureAllDisplays` | 8 | deluge, krita, lmms, love |
| CoreGraphics.framework | `_CGDisplayIOServicePort` | 8 | blender, hammerspoon, krita, love |
| CoreGraphics.framework | `_CGEventSourceCounterForEventType` | 8 | firefox, floorp, mullvad-browser, pencil |
| CoreGraphics.framework | `_CGReleaseAllDisplays` | 8 | deluge, krita, lmms, love |
| CoreGraphics.framework | `_CGEventCreateCopy` | 7 | hammerspoon, iina, iterm2, linearmouse |
| CoreGraphics.framework | `_CGDisplayModeIsUsableForDesktopGUI` | 6 | android-studio, krita, love, openrct2 |
| CoreGraphics.framework | `_CGDisplayShowCursor` | 6 | claude, deluge, godot, jordanbaird-ice |
| CoreGraphics.framework | `_CGRestorePermanentDisplayConfiguration` | 6 | alacritty, android-studio, cc-switch, dbeaver-community |
| CoreGraphics.framework | `_CGSCopySpacesForWindows` | 6 | alt-tab, ghostty, jordanbaird-ice, kitty |
| CoreGraphics.framework | `_CGSShutdownServerConnections` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| CoreGraphics.framework | `_kCGDisplayShowDuplicateLowResolutionModes` | 6 | android-studio, krita, love, openrct2 |
| CoreGraphics.framework | `_CGBeginDisplayConfiguration` | 5 | android-studio, betterdisplay, dbeaver-community, hammerspoon |
| CoreGraphics.framework | `_CGCompleteDisplayConfiguration` | 5 | android-studio, betterdisplay, dbeaver-community, hammerspoon |
| CoreGraphics.framework | `_CGDisplayModeRetain` | 5 | alacritty, cc-switch, dbeaver-community, deluge |
| CoreGraphics.framework | `_CGDisplayRestoreColorSyncSettings` | 5 | betterdisplay, deluge, hammerspoon, lmms |
| CoreGraphics.framework | `_CGGetLastMouseDelta` | 5 | bitwarden, deluge, lmms, monitorcontrol |
| CoreGraphics.framework | `_CGSGetActiveSpace` | 5 | ghostty, jordanbaird-ice, kitty, raycast |
| CoreGraphics.framework | `_CGDisplayHideCursor` | 4 | claude, godot, jordanbaird-ice, pixelorama |
| CoreGraphics.framework | `_CGDisplayIsCaptured` | 4 | deluge, lmms, xquartz, zoom |
| CoreGraphics.framework | `_CGDisplayIsInHWMirrorSet` | 4 | android-studio, betterdisplay, monitorcontrol, zoom |
| CoreGraphics.framework | `_CGEventSourceSetLocalEventsFilterDuringSuppressionState` | 4 | clipy, jordanbaird-ice, maccy, raycast |
| CoreGraphics.framework | `_CGSDefaultConnectionForThread` | 4 | cmux, ghostty, iterm2, kitty |
| CoreGraphics.framework | `_CGSSetWindowBackgroundBlurRadius` | 4 | alacritty, cmux, ghostty, kitty |
| CoreGraphics.framework | `_CGSSpaceGetType` | 4 | ghostty, jordanbaird-ice, raycast, zoom |
| CoreGraphics.framework | `_CGCancelDisplayConfiguration` | 3 | android-studio, betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGConfigureDisplayMirrorOfDisplay` | 3 | betterdisplay, hammerspoon, monitorcontrol |
| CoreGraphics.framework | `_CGDisplayStreamCreateWithDispatchQueue` | 3 | alt-tab, obs, zoom |
| CoreGraphics.framework | `_CGSSetSymbolicHotKeyEnabled` | 3 | alt-tab, linearmouse, raycast |
| CoreGraphics.framework | `_CGConfigureDisplayOrigin` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGConfigureDisplayWithDisplayMode` | 2 | android-studio, dbeaver-community |
| CoreGraphics.framework | `_CGDisplayGammaTableCapacity` | 2 | hammerspoon, kitty |
| CoreGraphics.framework | `_CGDisplayStreamUpdateGetDropCount` | 2 | obs, zoom |
| CoreGraphics.framework | `_CGEventCreateSourceFromEvent` | 2 | raycast, zoom |
| CoreGraphics.framework | `_CGEventGetUnflippedLocation` | 2 | jordanbaird-ice, linearmouse |
| CoreGraphics.framework | `_CGGetDisplayTransferByFormula` | 2 | deluge, lmms |
| CoreGraphics.framework | `_CGSConfigureDisplayMode` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSCopyManagedDisplayForSpace` | 2 | raycast, zoom |
| CoreGraphics.framework | `_CGSCopyManagedDisplaySpaces` | 2 | alt-tab, raycast |
| CoreGraphics.framework | `_CGSEventIsAppUnresponsive` | 2 | hammerspoon, jordanbaird-ice |
| CoreGraphics.framework | `_CGSGetCurrentDisplayMode` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSGetDisplayModeDescriptionOfLength` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSGetNumberOfDisplayModes` | 2 | betterdisplay, hammerspoon |
| CoreGraphics.framework | `_CGSGetScreenRectForWindow` | 2 | jordanbaird-ice, zoom |
| CoreGraphics.framework | `_CGSGetSymbolicHotKeyValue` | 2 | linearmouse, raycast |
| CoreGraphics.framework | `_CGSIsSymbolicHotKeyEnabled` | 2 | linearmouse, raycast |
| CoreGraphics.framework | `_CGSManagedDisplayGetCurrentSpace` | 2 | alt-tab, zoom |
| CoreGraphics.framework | `_CGSSetGlobalHotKeyOperatingMode` | 2 | utm, zoom |
| CoreGraphics.framework | `_CGSetDisplayTransferByFormula` | 2 | deluge, lmms |
| CoreGraphics.framework | `_CGSetLocalEventsFilterDuringSuppressionState` | 2 | android-studio, dbeaver-community |
| CoreGraphics.framework | `_CGSetLocalEventsSuppressionInterval` | 2 | android-studio, dbeaver-community |
| OSD.framework | `_OBJC_CLASS_$_OSDManager` | 2 | betterdisplay, monitorcontrol |
| ApplicationServices.framework | `_CGAcquireDisplayFadeReservation` | 1 | vlc |
| ApplicationServices.framework | `_CGDisplayFade` | 1 | vlc |
| ApplicationServices.framework | `_CGReleaseDisplayFadeReservation` | 1 | vlc |
| ApplicationServices.framework | `_CGSCreateRegisteredCursorImage` | 1 | vlc |
| CoreGraphics.framework | `_CGConfigureDisplayFadeEffect` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGDisplayForceToGray` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGDisplayModeGetIODisplayModeID` | 1 | zoom |
| CoreGraphics.framework | `_CGDisplayPrimaryDisplay` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGDisplaySetInvertedPolarity` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGEventCopyIOHIDEvent` | 1 | linearmouse |
| CoreGraphics.framework | `_CGEventGetTypeID` | 1 | mos |
| CoreGraphics.framework | `_CGEventSourceGetKeyboardType` | 1 | linearmouse |
| CoreGraphics.framework | `_CGEventSourceGetPixelsPerLine` | 1 | zoom |
| CoreGraphics.framework | `_CGEventSourceGetSourceStateID` | 1 | raycast |
| CoreGraphics.framework | `_CGEventSourceSetKeyboardType` | 1 | android-studio |
| CoreGraphics.framework | `_CGEventTapPostEvent` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGRegisterScreenRefreshCallback` | 1 | zoom |
| CoreGraphics.framework | `_CGSAddWindowsToSpaces` | 1 | kitty |
| CoreGraphics.framework | `_CGSConfigureDisplayEnabled` | 1 | betterdisplay |
| CoreGraphics.framework | `_CGSCopyActiveMenuBarDisplayIdentifier` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSCopyDisplayInfoDictionary` | 1 | zoom |
| CoreGraphics.framework | `_CGSCopyRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSCopyRegisteredCursorImages` | 1 | raycast |
| CoreGraphics.framework | `_CGSCopyWindowProperty` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSCopyWindowsWithOptionsAndTags` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSCurrentCursorSeed` | 1 | zoom |
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
| CoreGraphics.framework | `_CGSGetWindowLevel` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSGetWindowList` | 1 | jordanbaird-ice |
| CoreGraphics.framework | `_CGSGetWindowWorkspace` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetWorkspace` | 1 | zoom |
| CoreGraphics.framework | `_CGSGetZoomParameters` | 1 | zoom |
| CoreGraphics.framework | `_CGSHWCaptureWindowList` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSIntersectRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSIntersectRegionWithRect` | 1 | zoom |
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
| CoreGraphics.framework | `_CGSServiceForDisplayNumber` | 1 | monitorcontrol |
| CoreGraphics.framework | `_CGSSetConnectionProperty` | 1 | jordanbaird-ice |
| CoreGraphics.framework | `_CGSSetDebugOptions` | 1 | hammerspoon |
| CoreGraphics.framework | `_CGSSetWindowListWorkspace` | 1 | raycast |
| CoreGraphics.framework | `_CGSSetWindowTags` | 1 | zoom |
| CoreGraphics.framework | `_CGSSpaceSetCompatID` | 1 | raycast |
| CoreGraphics.framework | `_CGSUnionRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGSUnionRegionWithRect` | 1 | zoom |
| CoreGraphics.framework | `_CGSXorRegion` | 1 | zoom |
| CoreGraphics.framework | `_CGShieldingWindowID` | 1 | xquartz |
| CoreGraphics.framework | `_kCGDisplayStreamQueueDepth` | 1 | obs |
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
