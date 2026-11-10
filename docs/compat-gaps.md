# Lacunas de compatibilidade no corpus macOS

Gerado por `python3 tools/corpus/harness.py gaps` depois de `machocheck --disco build/libunwind-x86`.
O disco aponta a libunwind do `tools/build.sh`. Não há `libc++.1.dylib` nem `libswift*` nessa árvore,
então os baldes libc++ e Swift continuam os imports que o bundle pede e o libSystem não exporta.
Binários não estão no git. URLs e sha256 ficam em `tools/corpus/manifest.json`.

Apps com pelo menos uma imagem Mach-O x86_64: **61**.

## Símbolos que faltam em mais apps

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_NSOpenGLPixelFormat` | 31 |
| `__ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9__grow_byEmmmmmm` | 31 |
| `_kCTFontURLAttribute` | 31 |
| `_OBJC_CLASS_$_NSOpenGLContext` | 29 |
| `_CGImageDestinationFinalize` | 28 |
| `_CGContextClip` | 27 |
| `_kCTFontAttributeName` | 27 |
| `_CTFontCreateWithGraphicsFont` | 26 |
| `__ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEmc` | 26 |
| `_CFStringCreateWithCharactersNoCopy` | 25 |
| `_CTFontDescriptorCreateCopyWithAttributes` | 25 |
| `_CTFontManagerCreateFontDescriptorsFromURL` | 25 |
| `__ZTVNSt3__115basic_streambufIcNS_11char_traitsIcEEEE` | 25 |
| `___cxa_bad_typeid` | 25 |
| `_CGImageDestinationAddImage` | 24 |
| `_CGContextEndTransparencyLayer` | 23 |
| `__ZNSt3__113basic_istreamIcNS_11char_traitsIcEEErsERd` | 23 |
| `_CFBundleGetBundleWithIdentifier` | 22 |
| `_CGImageSourceCreateImageAtIndex` | 22 |
| `_CGImageSourceCreateWithData` | 22 |
| `_NSAccessibilityUnknownRole` | 22 |
| `__ZNSt3__113basic_istreamIcNS_11char_traitsIcEEErsERj` | 22 |
| `__ZNSt3__16__sortIRNS_6__lessIjjEEPjEEvT0_S5_T_` | 22 |
| `_pthread_cancel` | 22 |
| `_CFDateFormatterCreate` | 21 |
| `_CGBitmapContextGetColorSpace` | 21 |
| `_CGContextSetLineDash` | 21 |
| `_CGImageDestinationCreateWithData` | 21 |
| `_CFDateFormatterGetFormat` | 20 |
| `_CFStringGetSystemEncoding` | 20 |
| `_CGContextBeginTransparencyLayerWithRect` | 20 |
| `_CGContextEOClip` | 20 |
| `_NSModalPanelRunLoopMode` | 20 |
| `_CFStringGetCharactersPtr` | 19 |
| `_CGContextClipToRects` | 19 |
| `_CGContextFlush` | 19 |
| `_CGContextGetClipBoundingBox` | 19 |
| `_CGContextSetFillColorSpace` | 19 |
| `_CTLineGetGlyphRuns` | 19 |
| `_CTRunGetGlyphCount` | 19 |

## Por origem

### sistema

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_NSOpenGLPixelFormat` | 31 |
| `_kCTFontURLAttribute` | 31 |
| `_OBJC_CLASS_$_NSOpenGLContext` | 29 |
| `_CGImageDestinationFinalize` | 28 |
| `_CGContextClip` | 27 |
| `_kCTFontAttributeName` | 27 |
| `_CTFontCreateWithGraphicsFont` | 26 |
| `_CFStringCreateWithCharactersNoCopy` | 25 |
| `_CTFontDescriptorCreateCopyWithAttributes` | 25 |
| `_CTFontManagerCreateFontDescriptorsFromURL` | 25 |
| `_CGImageDestinationAddImage` | 24 |
| `_CGContextEndTransparencyLayer` | 23 |
| `_CFBundleGetBundleWithIdentifier` | 22 |
| `_CGImageSourceCreateImageAtIndex` | 22 |
| `_CGImageSourceCreateWithData` | 22 |

### libc++

| símbolo | apps |
|---|---:|
| `__ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9__grow_byEmmmmmm` | 31 |
| `__ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEmc` | 26 |
| `__ZTVNSt3__115basic_streambufIcNS_11char_traitsIcEEEE` | 25 |
| `___cxa_bad_typeid` | 25 |
| `__ZNSt3__113basic_istreamIcNS_11char_traitsIcEEErsERd` | 23 |
| `__ZNSt3__113basic_istreamIcNS_11char_traitsIcEEErsERj` | 22 |
| `__ZNSt3__16__sortIRNS_6__lessIjjEEPjEEvT0_S5_T_` | 22 |
| `__ZNSt3__113basic_istreamIcNS_11char_traitsIcEEErsERt` | 19 |
| `__ZNSt3__19to_stringEd` | 19 |
| `__ZNSt3__16__sortIRNS_6__lessIddEEPdEEvT0_S5_T_` | 17 |
| `__ZNSt14overflow_errorD1Ev` | 16 |
| `__ZNSt3__113basic_istreamIcNS_11char_traitsIcEEErsERy` | 16 |
| `__ZTVSt14overflow_error` | 16 |
| `__ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE2atEm` | 15 |
| `__ZTIPKc` | 15 |

### Swift

| símbolo | apps |
|---|---:|
| `_$sBOWV` | 16 |
| `_swift_errorRetain` | 16 |
| `_swift_getObjectType` | 16 |
| `_$sSS6appendyySSF` | 15 |
| `_$ss11_StringGutsV4growyySiF` | 15 |
| `_$ss18_CocoaArrayWrapperV8endIndexSivg` | 15 |
| `_swift_allocObject` | 15 |
| `_swift_beginAccess` | 15 |
| `_swift_bridgeObjectRelease` | 15 |
| `_swift_deallocObject` | 15 |
| `_swift_dynamicCastObjCClass` | 15 |
| `_swift_endAccess` | 15 |
| `_swift_errorRelease` | 15 |
| `_swift_getForeignTypeMetadata` | 15 |
| `_swift_getObjCClassMetadata` | 15 |

### unwind

| símbolo | apps |
|---|---:|
| — | 0 |

## Por framework

### libswiftCore.dylib

| símbolo | apps |
|---|---:|
| `_$sBOWV` | 16 |
| `_swift_errorRetain` | 16 |
| `_swift_getObjectType` | 16 |
| `_$sSS6appendyySSF` | 15 |
| `_$ss11_StringGutsV4growyySiF` | 15 |
| `_$ss18_CocoaArrayWrapperV8endIndexSivg` | 15 |
| `_swift_allocObject` | 15 |
| `_swift_beginAccess` | 15 |

### SwiftUI.framework

| símbolo | apps |
|---|---:|
| `_$s4Body7SwiftUI4ViewPTl` | 9 |
| `_$s7SwiftUI12_FrameLayoutV5width6height9alignmentAC12CoreGraphics7CGFloatVSg_AjA9AlignmentVtcfC` | 9 |
| `_$s7SwiftUI12_FrameLayoutVMn` | 9 |
| `_$s7SwiftUI13_HStackLayoutVMn` | 9 |
| `_$s7SwiftUI13_VStackLayoutVMn` | 9 |
| `_$s7SwiftUI13_VariadicViewO4TreeVMn` | 9 |
| `_$s7SwiftUI15ModifiedContentVMn` | 9 |
| `_$s7SwiftUI15ModifiedContentVyxq_GAA4ViewA2aERzAA0E8ModifierR_rlMc` | 9 |

### Foundation.framework

| símbolo | apps |
|---|---:|
| `_NSFileTypeForHFSTypeCode` | 11 |
| `_$s10Foundation11JSONDecoderC6decode_4fromxxm_AA4DataVtKSeRzlFTj` | 9 |
| `_$s10Foundation11JSONDecoderCACycfc` | 9 |
| `_$s10Foundation11JSONDecoderCMa` | 9 |
| `_$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF` | 9 |
| `_$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF` | 9 |
| `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 9 |
| `_$s10Foundation3URLVMa` | 9 |

### CoreFoundation.framework

| símbolo | apps |
|---|---:|
| `_CFStringCreateWithCharactersNoCopy` | 25 |
| `_CFBundleGetBundleWithIdentifier` | 21 |
| `_CFDateFormatterCreate` | 21 |
| `_CFDateFormatterGetFormat` | 20 |
| `_CFStringGetSystemEncoding` | 20 |
| `_CFStringGetCharactersPtr` | 19 |
| `_CFBundleCopyInfoDictionaryForURL` | 18 |
| `_CFURLEnumeratorGetNextURL` | 17 |

### AppKit.framework

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_NSOpenGLPixelFormat` | 31 |
| `_OBJC_CLASS_$_NSOpenGLContext` | 29 |
| `_NSAccessibilityUnknownRole` | 22 |
| `_NSModalPanelRunLoopMode` | 20 |
| `_NSAccessibilityMenuButtonRole` | 19 |
| `_NSBitsPerPixelFromDepth` | 18 |
| `_OBJC_CLASS_$_NSComboBox` | 18 |
| `_OBJC_CLASS_$_NSFontPanel` | 18 |

### libc++.1.dylib

| símbolo | apps |
|---|---:|
| `__ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9__grow_byEmmmmmm` | 31 |
| `__ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEmc` | 26 |
| `__ZTVNSt3__115basic_streambufIcNS_11char_traitsIcEEEE` | 25 |
| `___cxa_bad_typeid` | 25 |
| `__ZNSt3__113basic_istreamIcNS_11char_traitsIcEEErsERd` | 23 |
| `__ZNSt3__113basic_istreamIcNS_11char_traitsIcEEErsERj` | 22 |
| `__ZNSt3__16__sortIRNS_6__lessIjjEEPjEEvT0_S5_T_` | 22 |
| `__ZNSt3__113basic_istreamIcNS_11char_traitsIcEEErsERt` | 19 |

### libSystem.B.dylib

| símbolo | apps |
|---|---:|
| `_pthread_cancel` | 21 |
| `_semctl` | 19 |
| `_semget` | 19 |
| `_semop` | 19 |
| `_ftok` | 18 |
| `_getprotobyname` | 15 |
| `_putenv` | 15 |
| `_shmdt` | 15 |

### CoreGraphics.framework

| símbolo | apps |
|---|---:|
| `_CGContextClip` | 26 |
| `_CGContextEndTransparencyLayer` | 23 |
| `_CGBitmapContextGetColorSpace` | 20 |
| `_CGContextBeginTransparencyLayerWithRect` | 20 |
| `_CGContextSetLineDash` | 20 |
| `_CGContextClipToRects` | 19 |
| `_CGContextEOClip` | 19 |
| `_CGContextFlush` | 19 |

### libswiftFoundation.dylib

| símbolo | apps |
|---|---:|
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 7 |
| `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 6 |
| `_$s10Foundation3URLV4pathSSvg` | 6 |
| `_$s10Foundation3URLVMa` | 6 |
| `_$s10Foundation3URLVMn` | 6 |
| `_$s10Foundation4DataV36_unconditionallyBridgeFromObjectiveCyACSo6NSDataCSgFZ` | 6 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 6 |
| `_$sSa10FoundationE36_unconditionallyBridgeFromObjectiveCySayxGSo7NSArrayCSgFZ` | 6 |

### libncurses.5.4.dylib

| símbolo | apps |
|---|---:|
| `_setupterm` | 6 |
| `_tigetflag` | 6 |
| `_tigetnum` | 6 |
| `_tigetstr` | 6 |
| `_COLORS` | 5 |
| `_COLOR_PAIRS` | 5 |
| `_COLS` | 5 |
| `_LINES` | 5 |

### CoreText.framework

| símbolo | apps |
|---|---:|
| `_kCTFontURLAttribute` | 30 |
| `_kCTFontAttributeName` | 26 |
| `_CTFontCreateWithGraphicsFont` | 25 |
| `_CTFontDescriptorCreateCopyWithAttributes` | 25 |
| `_CTFontManagerCreateFontDescriptorsFromURL` | 24 |
| `_CTLineGetGlyphRuns` | 18 |
| `_CTRunGetGlyphCount` | 18 |
| `_CTRunGetStringIndices` | 18 |

### CoreServices.framework

| símbolo | apps |
|---|---:|
| `_kUTTypeFileURL` | 16 |
| `_kUTTagClassNSPboardType` | 15 |
| `_kUTTypeBundle` | 14 |
| `_GetIconRef` | 13 |
| `_UCCreateCollator` | 12 |
| `_kUTTypeApplication` | 12 |
| `_ReleaseIconRef` | 11 |
| `_UCGetCollationKey` | 11 |

### ApplicationServices.framework

| símbolo | apps |
|---|---:|
| `_PasteboardCreate` | 19 |
| `_PasteboardSynchronize` | 19 |
| `_PMRetain` | 11 |
| `_PasteboardClear` | 11 |
| `_PasteboardCopyItemFlavorData` | 11 |
| `_PasteboardCopyItemFlavors` | 11 |
| `_PasteboardCopyName` | 11 |
| `_PasteboardGetItemCount` | 11 |

### Combine.framework

| símbolo | apps |
|---|---:|
| `_$s25ObjectWillChangePublisher7Combine010ObservableA0PTl` | 6 |
| `_$s7Combine14AnyCancellableCMn` | 6 |
| `_$s7Combine16ObservableObjectMp` | 6 |
| `_$s7Combine16ObservableObjectP0C19WillChangePublisherAC_AA0F0Tn` | 6 |
| `_$s7Combine16ObservableObjectP16objectWillChange0ceF9PublisherQzvgTq` | 6 |
| `_$s7Combine16ObservableObjectPA2A0bC9PublisherC0c10WillChangeD0RtzrlE06objecteF0AEvg` | 6 |
| `_$s7Combine25ObservableObjectPublisherCAA0D0AAWP` | 6 |
| `_$s7Combine25ObservableObjectPublisherCMn` | 6 |

### libswift_Concurrency.dylib

| símbolo | apps |
|---|---:|
| `_$sScM6sharedScMvgZ` | 10 |
| `_$sScMMa` | 10 |
| `_$sScMScAsMc` | 9 |
| `_$sScC6resume9returningyxn_tF` | 8 |
| `_$sScC12continuation8functionScCyxq_GSccyxq_G_SStcfC` | 7 |
| `_$sScT6cancelyyF` | 6 |
| `_$sScEMa` | 5 |
| `_$sScS12ContinuationV11YieldResultOMn` | 5 |

### Security.framework

| símbolo | apps |
|---|---:|
| `_kSecAttrServer` | 10 |
| `_SSLSetDiffieHellmanParams` | 9 |
| `_kSecAttrProtocol` | 9 |
| `_kSecPolicyClient` | 9 |
| `_kSecImportExportKeychain` | 8 |
| `_kSecImportItemCertChain` | 8 |
| `_kSecAttrProtocolFTPProxy` | 7 |
| `_kSecAttrProtocolHTTPProxy` | 7 |

### OpenGL.framework

| símbolo | apps |
|---|---:|
| `_CGLTexImageIOSurface2D` | 13 |
| `_glOrtho` | 13 |
| `_glGetIntegerv` | 12 |
| `_glGetString` | 11 |
| `_glLoadIdentity` | 9 |
| `_glMatrixMode` | 9 |
| `_CGLSetVirtualScreen` | 8 |
| `_CGLCreateContext` | 7 |

### Carbon.framework

| símbolo | apps |
|---|---:|
| `_TISCopyInputMethodKeyboardLayoutOverride` | 17 |
| `_TISCreateInputSourceList` | 13 |
| `_GetEventDispatcherTarget` | 12 |
| `_GetThemeMetric` | 10 |
| `_HIThemeDrawButton` | 10 |
| `_TISSelectInputSource` | 10 |
| `_CopySymbolicHotKeys` | 9 |
| `_kTISPropertyInputSourceIsASCIICapable` | 9 |

### ImageIO.framework

| símbolo | apps |
|---|---:|
| `_CGImageDestinationFinalize` | 27 |
| `_CGImageDestinationAddImage` | 23 |
| `_CGImageSourceCreateImageAtIndex` | 22 |
| `_CGImageSourceCreateWithData` | 22 |
| `_CGImageDestinationCreateWithData` | 20 |
| `_kCGImagePropertyPixelHeight` | 19 |
| `_kCGImagePropertyPixelWidth` | 19 |
| `_CGImageSourceCreateWithURL` | 17 |

### libswiftDispatch.dylib

| símbolo | apps |
|---|---:|
| `_$s8Dispatch0A3QoSV0B6SClassOMa` | 10 |
| `_$s8Dispatch0A8WorkItemC6cancelyyFTj` | 10 |
| `_$s8Dispatch0A8WorkItemCMa` | 10 |
| `_$s8Dispatch0A8WorkItemCMn` | 10 |
| `_$sSo17OS_dispatch_queueC8DispatchE10asyncAfter8deadline7executeyAC0D4TimeV_AC0D8WorkItemCtF` | 10 |
| `_$sSo17OS_dispatch_queueC8DispatchE6global3qosAbC0D3QoSV0G6SClassO_tFZ` | 10 |
| `_$s8Dispatch0A8WorkItemC5flags5blockAcA0abC5FlagsV_yyXBtcfc` | 9 |
| `_$sSo17OS_dispatch_queueC8DispatchE10asyncAfter8deadline3qos5flags7executeyAC0D4TimeV_AC0D3QoSVAC0D13WorkItemFlagsVyyXBtF` | 9 |

### libz.1.dylib

| símbolo | apps |
|---|---:|
| `_crc32` | 19 |
| `_deflate` | 17 |
| `_deflateEnd` | 17 |
| `_deflateInit2_` | 17 |
| `_inflate` | 16 |
| `_inflateEnd` | 16 |
| `_inflateInit2_` | 16 |
| `_deflateInit_` | 12 |

### libxml2.2.dylib

| símbolo | apps |
|---|---:|
| `_xmlFreeDoc` | 2 |
| `_xmlReadMemory` | 2 |
| `_xmlStrcmp` | 2 |
| `___xmlGenericError` | 1 |
| `___xmlGenericErrorContext` | 1 |
| `___xmlKeepBlanksDefaultValue` | 1 |
| `___xmlStructuredError` | 1 |
| `_htmlParseDoc` | 1 |

### libcups.2.dylib

| símbolo | apps |
|---|---:|
| `_ppdClose` | 9 |
| `_ppdFindOption` | 9 |
| `_cupsGetDests2` | 8 |
| `_httpGetHostname` | 8 |
| `_ppdFindMarkedChoice` | 7 |
| `_ppdOpenFile` | 7 |
| `_cupsConnectDest` | 6 |
| `_cupsGetDestMediaByIndex` | 6 |

### libedit.3.dylib

| símbolo | apps |
|---|---:|
| `_add_history` | 7 |
| `_history_truncate_file` | 6 |
| `_read_history` | 6 |
| `_rl_attempted_completion_function` | 6 |
| `_rl_attempted_completion_over` | 6 |
| `_rl_completion_append_character` | 6 |
| `_rl_completion_matches` | 6 |
| `_write_history` | 6 |

### AVFoundation.framework

| símbolo | apps |
|---|---:|
| `_AVCaptureSessionDidStartRunningNotification` | 6 |
| `_AVCaptureSessionDidStopRunningNotification` | 6 |
| `_AVCaptureSessionPreset1280x720` | 6 |
| `_AVCaptureSessionPreset352x288` | 6 |
| `_AVCaptureSessionPreset640x480` | 6 |
| `_AVCaptureSessionPresetLow` | 6 |
| `_OBJC_CLASS_$_AVAssetReader` | 6 |
| `_OBJC_CLASS_$_AVAssetReaderTrackOutput` | 6 |

## Contagem distinta por app

| app | distintos | sistema | libc++ | Swift | unwind |
|---|---:|---:|---:|---:|---:|
| coteditor | 3962 | 2440 | 0 | 1522 | 0 |
| clipy | 3048 | 1189 | 28 | 1831 | 0 |
| iterm2 | 2758 | 1079 | 3 | 1676 | 0 |
| maccy | 2010 | 1243 | 0 | 767 | 0 |
| sequel-ace | 1938 | 998 | 7 | 933 | 0 |
| iina | 1735 | 551 | 3 | 1181 | 0 |
| linearmouse | 1570 | 661 | 0 | 909 | 0 |
| hammerspoon | 1410 | 1070 | 12 | 328 | 0 |
| stats | 974 | 223 | 3 | 748 | 0 |
| rectangle | 902 | 508 | 0 | 394 | 0 |
| obs | 863 | 505 | 72 | 286 | 0 |
| hex-fiend | 842 | 106 | 0 | 736 | 0 |
| alt-tab | 832 | 116 | 3 | 713 | 0 |
| libreoffice | 764 | 570 | 194 | 0 | 0 |
| kicad | 726 | 513 | 213 | 0 | 0 |
| krita | 686 | 594 | 41 | 51 | 0 |
| inkscape | 682 | 612 | 70 | 0 | 0 |
| shotcut | 625 | 583 | 42 | 0 | 0 |
| blender | 583 | 474 | 109 | 0 | 0 |
| deluge | 581 | 548 | 33 | 0 | 0 |
| openscad | 499 | 434 | 65 | 0 | 0 |
| lmms | 490 | 468 | 22 | 0 | 0 |
| kitty | 427 | 415 | 12 | 0 | 0 |
| vnote | 368 | 323 | 45 | 0 | 0 |
| bitwarden | 359 | 334 | 1 | 24 | 0 |
| audacity | 358 | 277 | 81 | 0 | 0 |
| musescore | 350 | 307 | 43 | 0 | 0 |
| thunderbird | 323 | 302 | 21 | 0 | 0 |
| prusaslicer | 310 | 226 | 84 | 0 | 0 |
| monitorcontrol | 306 | 305 | 1 | 0 | 0 |
| firefox | 295 | 271 | 24 | 0 | 0 |
| zen | 292 | 269 | 23 | 0 | 0 |
| floorp | 291 | 268 | 23 | 0 | 0 |
| mullvad-browser | 285 | 267 | 18 | 0 | 0 |
| gimp | 282 | 239 | 43 | 0 | 0 |
| tor-browser | 281 | 264 | 17 | 0 | 0 |
| vlc | 278 | 259 | 19 | 0 | 0 |
| dolphin | 277 | 240 | 37 | 0 | 0 |
| keepassxc | 260 | 245 | 15 | 0 | 0 |
| mos | 259 | 258 | 1 | 0 | 0 |
| hiddenbar | 229 | 37 | 0 | 192 | 0 |
| webtorrent | 223 | 223 | 0 | 0 | 0 |
| qbittorrent | 215 | 203 | 12 | 0 | 0 |
| pencil | 199 | 199 | 0 | 0 | 0 |
| cyberduck | 187 | 179 | 0 | 8 | 0 |
| transmission | 101 | 79 | 22 | 0 | 0 |
| openttd | 85 | 69 | 16 | 0 | 0 |
| godot | 79 | 61 | 18 | 0 | 0 |
| pixelorama | 77 | 59 | 18 | 0 | 0 |
| openrct2 | 73 | 61 | 12 | 0 | 0 |
| love | 71 | 61 | 10 | 0 | 0 |
| tic80 | 71 | 68 | 3 | 0 | 0 |
| flycut | 49 | 49 | 0 | 0 | 0 |
| macdown | 43 | 43 | 0 | 0 | 0 |
| insomnia | 40 | 10 | 30 | 0 | 0 |
| vscodium | 31 | 9 | 22 | 0 | 0 |
| keepingyouawake | 26 | 26 | 0 | 0 | 0 |
| dbgate | 20 | 4 | 16 | 0 | 0 |
| alacritty | 17 | 17 | 0 | 0 | 0 |
| element | 4 | 4 | 0 | 0 | 0 |
| finicky | 0 | 0 | 0 | 0 | 0 |
