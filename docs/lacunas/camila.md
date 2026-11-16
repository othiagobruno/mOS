# Lacunas de Camila (desktop e window server)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Camila (desktop e window server) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**39** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreGraphics.framework | `_CGDisplayMoveCursorToPoint` | 2 | gimp, inkscape |
| CoreGraphics.framework | `_CGEventCreateCopy` | 2 | iterm2, mos |
| ApplicationServices.framework | `_CGAcquireDisplayFadeReservation` | 1 | vlc |
| ApplicationServices.framework | `_CGDisplayFade` | 1 | vlc |
| ApplicationServices.framework | `_CGReleaseDisplayFadeReservation` | 1 | vlc |
| ApplicationServices.framework | `_CGSCreateRegisteredCursorImage` | 1 | vlc |
| CoreGraphics.framework | `_CGDisplayStreamCreateWithDispatchQueue` | 1 | alt-tab |
| CoreGraphics.framework | `_CGEventGetTypeID` | 1 | mos |
| CoreGraphics.framework | `_CGEventSourceButtonState` | 1 | firefox |
| CoreGraphics.framework | `_CGEventSourceCounterForEventType` | 1 | firefox |
| CoreGraphics.framework | `_CGGetLastMouseDelta` | 1 | mos |
| CoreGraphics.framework | `_CGSCopyActiveMenuBarDisplayIdentifier` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSCopyManagedDisplaySpaces` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSCopySpacesForWindows` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSCopyWindowProperty` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSCopyWindowsWithOptionsAndTags` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSDefaultConnectionForThread` | 1 | iterm2 |
| CoreGraphics.framework | `_CGSGetWindowLevel` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSHWCaptureWindowList` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSManagedDisplayGetCurrentSpace` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSSetSymbolicHotKeyEnabled` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSShutdownServerConnections` | 1 | firefox |
| SkyLight.framework | `_SLPSPostEventRecordTo` | 1 | alt-tab |
| SkyLight.framework | `_SLSRegisterConnectionNotifyProc` | 1 | alt-tab |
| SkyLight.framework | `_SLSRequestNotificationsForWindows` | 1 | alt-tab |
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
