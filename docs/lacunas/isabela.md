# Lacunas de Isabela (mídia)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Isabela (mídia) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**927** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreImage.framework | `_OBJC_CLASS_$_CIContext` | 39 | anydesk, applite, bartender, betterdisplay |
| CoreAudio.framework | `_AudioConvertHostTimeToNanos` | 32 | adobe-acrobat-reader, android-studio, another-redis-desktop-manager, audacity |
| CoreImage.framework | `_kCIInputImageKey` | 28 | alfred, anydesk, applite, bartender |
| CoreAudio.framework | `_AudioGetCurrentHostTime` | 25 | android-studio, audacity, cryptomator, cyberduck |
| CoreImage.framework | `_OBJC_CLASS_$_CIColor` | 25 | alfred, applite, betterdisplay, calibre |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerLayer` | 18 | calibre, cleanshot, cmux, handbrake-app |
| Accelerate.framework | `_vDSP_maxv` | 17 | android-studio, block-goose, cleanshot, firefox |
| Accelerate.framework | `_vDSP_measqv` | 15 | android-studio, block-goose, firefox, floorp |
| Accelerate.framework | `_vDSP_vsmsa` | 15 | android-studio, block-goose, firefox, floorp |
| CoreMIDI.framework | `_MIDIPortDispose` | 14 | bettertouchtool, firefox, floorp, godot |
| VideoToolbox.framework | `_VTRegisterProfessionalVideoWorkflowVideoDecoders` | 14 | calibre, handbrake-app, iina, jellyfin |
| Vision.framework | `_OBJC_CLASS_$_VNRecognizedText` | 14 | bartender, bettertouchtool, cleanshot, clipy |
| Vision.framework | `_OBJC_CLASS_$_VNRecognizedTextObservation` | 14 | bartender, bettertouchtool, cleanshot, clipy |
| CoreMIDI.framework | `_MIDIPortDisconnectSource` | 13 | android-studio, bettertouchtool, cryptomator, cyberduck |
| CoreImage.framework | `_kCIInputBackgroundImageKey` | 12 | applite, betterdisplay, cleanshot, cmux |
| CoreMIDI.framework | `_MIDIEndpointDispose` | 12 | bettertouchtool, deluge, firefox, floorp |
| CoreImage.framework | `_kCIOutputImageKey` | 11 | anydesk, calibre, cmux, iina |
| CoreMIDI.framework | `_kMIDIPropertyName` | 11 | android-studio, bettertouchtool, cryptomator, cyberduck |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureStillImageOutput` | 10 | adobe-acrobat-reader, another-redis-desktop-manager, calibre, cleanshot |
| AudioToolbox.framework | `_AudioFileStreamClose` | 10 | firefox, floorp, librewolf, mullvad-browser |
| AudioToolbox.framework | `_AudioFileStreamGetProperty` | 10 | firefox, floorp, librewolf, mullvad-browser |
| AudioToolbox.framework | `_AudioFileStreamGetPropertyInfo` | 10 | firefox, floorp, librewolf, mullvad-browser |
| AudioToolbox.framework | `_AudioFileStreamOpen` | 10 | firefox, floorp, librewolf, mullvad-browser |
| AudioToolbox.framework | `_AudioFileStreamParseBytes` | 10 | firefox, floorp, librewolf, mullvad-browser |
| CoreImage.framework | `_kCIContextCacheIntermediates` | 10 | bartender, betterdisplay, claude, cleanshot |
| CoreImage.framework | `_kCIInputScaleKey` | 10 | betterdisplay, claude, cleanshot, duckduckgo |
| CoreMIDI.framework | `_MIDIReceived` | 10 | bettertouchtool, firefox, floorp, hammerspoon |
| CoreMIDI.framework | `_MIDIFlushOutput` | 9 | android-studio, bettertouchtool, cryptomator, cyberduck |
| VideoToolbox.framework | `_kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder` | 9 | firefox, floorp, librewolf, microsoft-teams |
| AVFoundation.framework | `_AVCaptureSessionPresetLow` | 8 | firefox, floorp, librewolf, mullvad-browser |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureVideoPreviewLayer` | 8 | cleanshot, microsoft-teams, nextcloud, sfm |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableMetadataItem` | 8 | calibre, cleanshot, iterm2, localsend |
| Accelerate.framework | `_vDSP_create_fftsetupD` | 8 | fluidvoice, handy, iina, shotcut |
| Accelerate.framework | `_vDSP_destroy_fftsetupD` | 8 | fluidvoice, handy, iina, shotcut |
| CoreAudio.framework | `_AudioDeviceDuck` | 8 | firefox, floorp, librewolf, mullvad-browser |
| CoreImage.framework | `_kCIAttributeClass` | 8 | calibre, iina, jellyfin, krita |
| CoreImage.framework | `_kCIAttributeDefault` | 8 | calibre, iina, jellyfin, krita |
| CoreImage.framework | `_kCIAttributeSliderMax` | 8 | calibre, iina, jellyfin, krita |
| CoreImage.framework | `_kCIAttributeSliderMin` | 8 | calibre, iina, jellyfin, krita |
| CoreImage.framework | `_kCIAttributeType` | 8 | calibre, iina, jellyfin, krita |
| CoreImage.framework | `_kCIFormatRGBA8` | 8 | bartender, cleanshot, figma, openclaw |
| CoreMIDI.framework | `_MIDIInputPortCreateWithBlock` | 8 | firefox, floorp, librewolf, mullvad-browser |
| CoreMedia.framework | `_CMSampleBufferGetOutputDuration` | 8 | firefox, floorp, librewolf, mullvad-browser |
| CoreMedia.framework | `_CMTimebaseCreateWithMasterClock` | 8 | firefox, floorp, librewolf, mullvad-browser |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetExportSession` | 7 | cleanshot, localsend, macwhisper, openclaw |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetWriterInputPixelBufferAdaptor` | 7 | cmux, iterm2, microsoft-teams, openclaw |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMetadataItem` | 7 | cleanshot, iterm2, localsend, macwhisper |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerItemVideoOutput` | 7 | calibre, localsend, raycast, rustdesk |
| Accelerate.framework | `_vDSP_dotpr` | 7 | bettertouchtool, fluidvoice, iina, macwhisper |
| Accelerate.framework | `_vImageLookupTable_Planar8toPlanarF` | 7 | android-studio, cryptomator, dbeaver-community, gemini |
| Accelerate.framework | `_vvlogf` | 7 | fluidvoice, iina, opensuperwhisper, shotcut |
| CoreAudio.framework | `_AudioConvertNanosToHostTime` | 7 | adobe-acrobat-reader, another-redis-desktop-manager, lmms, openmtp |
| CoreImage.framework | `_kCICategoryGenerator` | 7 | calibre, iina, jellyfin, krita |
| CoreImage.framework | `_kCIFormatARGB8` | 7 | calibre, iina, jellyfin, krita |
| CoreImage.framework | `_kCIInputSaturationKey` | 7 | applite, betterdisplay, cleanshot, proxyman |
| CoreMedia.framework | `_CMVideoFormatDescriptionGetCleanAperture` | 7 | adobe-acrobat-reader, another-redis-desktop-manager, battery, openmtp |
| CoreMediaIO.framework | `_CMIOObjectGetPropertyDataSize` | 7 | hammerspoon, macwhisper, obs, openlogi |
| AVFoundation.framework | `_AVMetadataCommonKeyTitle` | 6 | calibre, localsend, macwhisper, nextcloud |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableComposition` | 6 | cleanshot, fluidvoice, macwhisper, telegram |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableVideoComposition` | 6 | cleanshot, localsend, rustdesk, telegram |
| Accelerate.framework | `_vDSP_vspdp` | 6 | fluidvoice, iina, opensuperwhisper, shotcut |
| Accelerate.framework | `_vImageBufferFill_ARGB8888` | 6 | android-studio, cryptomator, dbeaver-community, intellij-idea |
| Accelerate.framework | `_vvlog` | 6 | fluidvoice, iina, opensuperwhisper, shotcut |
| AudioToolbox.framework | `_AUGraphNodeInfo` | 6 | bettertouchtool, deluge, hammerspoon, openttd |
| CoreImage.framework | `_kCIContextWorkingColorSpace` | 6 | betterdisplay, cleanshot, macwhisper, pearcleaner |
| CoreImage.framework | `_kCIInputColorKey` | 6 | alfred, applite, betterdisplay, cleanshot |
| CoreImage.framework | `_kCIInputMaskImageKey` | 6 | figma, shottr, skim, telegram |
| CoreMedia.framework | `_CMSampleBufferGetOutputSampleTimingInfoArray` | 6 | calibre, iina, jellyfin, obs |
| ScreenCaptureKit.framework | `_SCStreamErrorDomain` | 6 | displaylink, fluidvoice, macwhisper, openclaw |
| Speech.framework | `_OBJC_CLASS_$_SFSpeechRecognizer` | 6 | epic-games, fluidvoice, openclaw, raycast |
| VideoToolbox.framework | `_kVTDecompressionPropertyKey_RealTime` | 6 | android-studio, jellyfin, microsoft-teams, stremio |
| AVFAudio.framework | `_OBJC_CLASS_$_AVAudioChannelLayout` | 5 | fluidvoice, macwhisper, opensuperwhisper, vorssaint |
| AVFoundation.framework | `_AVFileTypeWAVE` | 5 | calibre, localsend, raycast, shotcut |
| AVFoundation.framework | `_AVMetadataCommonKeyAlbumName` | 5 | calibre, macwhisper, nextcloud, shotcut |
| AVFoundation.framework | `_AVMetadataCommonKeyArtist` | 5 | calibre, macwhisper, nextcloud, shotcut |
| AVFoundation.framework | `_AVVideoAllowFrameReorderingKey` | 5 | cleanshot, macwhisper, microsoft-teams, vorssaint |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetReaderAudioMixOutput` | 5 | cleanshot, telegram, vorssaint, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableVideoCompositionInstruction` | 5 | localsend, rustdesk, telegram, vorssaint |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableVideoCompositionLayerInstruction` | 5 | localsend, rustdesk, telegram, vorssaint |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerLooper` | 5 | cleanshot, cmux, duckduckgo, superwhisper |
| Accelerate.framework | `_vDSP_mmul` | 5 | fluidvoice, macwhisper, opensuperwhisper, superwhisper |
| Accelerate.framework | `_vDSP_vclrD` | 5 | fluidvoice, iina, shotcut, spotify |
| Accelerate.framework | `_vDSP_vflt16` | 5 | cleanshot, macwhisper, raycast, superwhisper |
| Accelerate.framework | `_vvexp` | 5 | fluidvoice, iina, opensuperwhisper, shotcut |
| AudioToolbox.framework | `_AUGraphInitialize` | 5 | bettertouchtool, hammerspoon, openttd, telegram |
| AudioToolbox.framework | `_AUGraphOpen` | 5 | bettertouchtool, hammerspoon, openttd, telegram |
| CoreImage.framework | `_kCIInputAspectRatioKey` | 5 | claude, duckduckgo, telegram, vorssaint |
| CoreImage.framework | `_kCIInputBrightnessKey` | 5 | applite, betterdisplay, cleanshot, protonvpn |
| Speech.framework | `_OBJC_CLASS_$_SFSpeechAudioBufferRecognitionRequest` | 5 | epic-games, fluidvoice, openclaw, raycast |
| AVFoundation.framework | `_AVCaptureDeviceTypeDeskViewCamera` | 4 | calibre, macwhisper, microsoft-teams, obs |
| AVFoundation.framework | `_AVErrorDeviceKey` | 4 | gemini, monitorcontrol, rustdesk, telegram |
| AVFoundation.framework | `_AVErrorFileSizeKey` | 4 | gemini, monitorcontrol, rustdesk, telegram |
| AVFoundation.framework | `_AVErrorMediaSubTypeKey` | 4 | gemini, monitorcontrol, rustdesk, telegram |
| AVFoundation.framework | `_AVErrorMediaTypeKey` | 4 | gemini, monitorcontrol, rustdesk, telegram |
| AVFoundation.framework | `_AVErrorPIDKey` | 4 | gemini, monitorcontrol, rustdesk, telegram |
| AVFoundation.framework | `_AVErrorRecordingSuccessfullyFinishedKey` | 4 | gemini, monitorcontrol, rustdesk, telegram |
| AVFoundation.framework | `_AVErrorTimeKey` | 4 | gemini, monitorcontrol, rustdesk, telegram |
| AVFoundation.framework | `_AVFileTypeMPEGLayer3` | 4 | calibre, localsend, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMediaTypeSubtitle` | 4 | calibre, handbrake-app, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierArtist` | 4 | calibre, shotcut, telegram, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierArtwork` | 4 | calibre, shotcut, telegram, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierTitle` | 4 | calibre, shotcut, telegram, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyAuthor` | 4 | calibre, nextcloud, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyCopyrights` | 4 | calibre, nextcloud, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyCreationDate` | 4 | calibre, nextcloud, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyDescription` | 4 | calibre, nextcloud, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyLanguage` | 4 | calibre, nextcloud, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyPublisher` | 4 | calibre, nextcloud, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyType` | 4 | calibre, nextcloud, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataLeadPerformer` | 4 | calibre, shotcut, telegram, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataTitleDescription` | 4 | calibre, shotcut, telegram, wireshark-app |
| AVFoundation.framework | `_AVMetadataKeySpaceID3` | 4 | calibre, hammerspoon, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataKeySpaceiTunes` | 4 | calibre, hammerspoon, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyArtist` | 4 | calibre, shotcut, telegram, wireshark-app |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetReaderVideoCompositionOutput` | 4 | cleanshot, telegram, vorssaint, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureDeviceFormat` | 4 | macwhisper, obs, openclaw, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableCompositionTrack` | 4 | cleanshot, telegram, vorssaint, whatsapp |
| Accelerate.framework | `_cblas_cgemm$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_cblas_sgemv` | 4 | fluidvoice, handy, opensuperwhisper, t3-code |
| Accelerate.framework | `_cgesdd$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_cheevd$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_dgeev$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_dgeqrf$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_dgesdd$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_dgetrf$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_dorgqr$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_dpotrf$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_dsyevd$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_sgeev$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_sgesdd$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_sgetrf$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_spotrf$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_ssyevd$NEWLAPACK` | 4 | blender, ollama-app, openclaw, voiceink |
| Accelerate.framework | `_vDSP_ctozD` | 4 | iina, shotcut, spotify, stremio |
| Accelerate.framework | `_vDSP_fft_zripD` | 4 | fluidvoice, handy, t3-code, voiceink |
| Accelerate.framework | `_vDSP_fft_zriptD` | 4 | iina, shotcut, spotify, stremio |
| Accelerate.framework | `_vDSP_vsdivD` | 4 | iina, shotcut, spotify, stremio |
| Accelerate.framework | `_vDSP_vsq` | 4 | fluidvoice, opensuperwhisper, superwhisper, voiceink |
| Accelerate.framework | `_vDSP_ztocD` | 4 | iina, shotcut, spotify, stremio |
| Accelerate.framework | `_vDSP_zvmagsD` | 4 | iina, shotcut, spotify, stremio |
| Accelerate.framework | `_vvatan2` | 4 | iina, shotcut, spotify, stremio |
| Accelerate.framework | `_vvatan2f` | 4 | iina, shotcut, spotify, stremio |
| Accelerate.framework | `_vvsincos` | 4 | iina, shotcut, spotify, stremio |
| Accelerate.framework | `_vvsincosf` | 4 | iina, shotcut, spotify, stremio |
| Accelerate.framework | `_vvsqrt` | 4 | iina, shotcut, spotify, stremio |
| AudioToolbox.framework | `_AUGraphAddNode` | 4 | bettertouchtool, hammerspoon, telegram, vlc |
| AudioToolbox.framework | `_AUGraphConnectNodeInput` | 4 | bettertouchtool, hammerspoon, telegram, vlc |
| AudioToolbox.framework | `_AUGraphStart` | 4 | bettertouchtool, hammerspoon, telegram, vlc |
| AudioToolbox.framework | `_DisposeAUGraph` | 4 | bettertouchtool, hammerspoon, telegram, vlc |
| AudioToolbox.framework | `_DisposeMusicPlayer` | 4 | bettertouchtool, deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_DisposeMusicSequence` | 4 | bettertouchtool, deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicPlayerGetTime` | 4 | bettertouchtool, deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicPlayerPreroll` | 4 | bettertouchtool, deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicPlayerSetSequence` | 4 | bettertouchtool, deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicPlayerStart` | 4 | bettertouchtool, deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicPlayerStop` | 4 | bettertouchtool, deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicSequenceGetIndTrack` | 4 | bettertouchtool, deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicSequenceGetTrackCount` | 4 | bettertouchtool, deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicTrackGetProperty` | 4 | bettertouchtool, deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_NewAUGraph` | 4 | bettertouchtool, hammerspoon, telegram, vlc |
| AudioToolbox.framework | `_NewMusicPlayer` | 4 | bettertouchtool, deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_NewMusicSequence` | 4 | bettertouchtool, deluge, hammerspoon, openttd |
| CoreAudio.framework | `_AudioDeviceGetProperty` | 4 | bbedit, deluge, lmms, shotcut |
| CoreAudio.framework | `_AudioHardwareGetProperty` | 4 | bbedit, deluge, lmms, shotcut |
| CoreImage.framework | `_kCIContextOutputColorSpace` | 4 | betterdisplay, macwhisper, telegram, whatsapp |
| CoreImage.framework | `_kCIImageColorSpace` | 4 | betterdisplay, claude, telegram, whatsapp |
| CoreImage.framework | `_kCIInputAngleKey` | 4 | betterdisplay, cleanshot, protonvpn, skim |
| CoreImage.framework | `_kCIInputEVKey` | 4 | applite, betterdisplay, supacode, whatsapp |
| CoreImage.framework | `_kCIInputExtentKey` | 4 | bartender, pearcleaner, skim, whatsapp |
| CoreImage.framework | `_kCIInputIntensityKey` | 4 | betterdisplay, cleanshot, daisydisk, whatsapp |
| CoreImage.framework | `_kCIInputTransformKey` | 4 | betterdisplay, cleanshot, skim, whatsapp |
| CoreMIDI.framework | `_MIDIDestinationCreate` | 4 | bettertouchtool, deluge, hammerspoon, lmms |
| CoreMedia.framework | `_CMMemoryPoolCreate` | 4 | telegram, telegram-desktop, windows-app, zed |
| CoreMedia.framework | `_CMMemoryPoolGetAllocator` | 4 | telegram, telegram-desktop, windows-app, zed |
| CoreMedia.framework | `_CMMemoryPoolInvalidate` | 4 | telegram, telegram-desktop, windows-app, zed |
| CoreMedia.framework | `_CMSimpleQueueDequeue` | 4 | handbrake-app, obs, telegram, telegram-desktop |
| CoreMedia.framework | `_CMTimeMakeFromDictionary` | 4 | gemini, monitorcontrol, rustdesk, telegram |
| CoreMedia.framework | `_CMTimeRangeGetUnion` | 4 | gemini, monitorcontrol, rustdesk, telegram |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Baseline_4_0` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Baseline_4_2` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Baseline_5_0` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Baseline_5_1` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Baseline_5_2` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_3_0` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_3_1` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_3_2` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_4_0` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_4_1` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_4_2` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_5_1` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_5_2` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Main_4_2` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Main_5_1` | 4 | telegram, telegram-desktop, windows-app, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Main_5_2` | 4 | telegram, telegram-desktop, windows-app, zed |
| Vision.framework | `_VNImageRectForNormalizedRect` | 4 | bettertouchtool, cleanshot, fluidvoice, shottr |
| libswiftVision.dylib | `_$sSo16VNRecognizedTextC6VisionE11boundingBox3forSo22VNRectangleObservationCSgSnySS5IndexVG_tKF` | 4 | bettertouchtool, raycast, shottr, vorssaint |
| AVFAudio.framework | `_OBJC_CLASS_$_AVAudioRecorder` | 3 | openclaw, opensuperwhisper, whatsapp |
| AVFoundation.framework | `_AVCaptureSessionPresetMedium` | 3 | obs, telegram, vorssaint |
| AVFoundation.framework | `_AVMediaCharacteristicLegible` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyAlbumAndTrack` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyAuthor` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyDescription` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyGenre` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyPerformer` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyTitle` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierAlbumName` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierAuthor` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierCopyrights` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierCreationDate` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierDescription` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierLanguage` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierPublisher` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierType` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyArtwork` | 3 | macwhisper, nextcloud, telegram |
| AVFoundation.framework | `_AVMetadataISOUserDataKeyDate` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataAlbumTitle` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataAttachedPicture` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataComments` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataComposer` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataContentType` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataCopyright` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataDate` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataFileType` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataLanguage` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataLength` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataOfficialAudioSourceWebpage` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataPublisher` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataTrackNumber` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataAlbum` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataArtist` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataArtwork` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataAuthor` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataCameraFrameReadoutTime` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataComment` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataComposer` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataCopyright` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataCreationDate` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataDescription` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataGenre` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataPerformer` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataPublisher` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataTitle` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataVideoOrientation` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataAlbum` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataAlbumArtist` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataArtist` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataAuthor` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataComposer` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataCopyright` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataCoverArt` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataDescription` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataPerformer` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataPublisher` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataReleaseDate` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataSongName` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataTrackNumber` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataUserComment` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataUserGenre` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataKeySpaceQuickTimeUserData` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyAlbum` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyAuthor` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyComment` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyCopyright` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyCreationDate` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyDescription` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyGenre` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_AVSampleBufferAudioRendererWasFlushedAutomaticallyNotification` | 3 | iina, spotify, stremio |
| AVFoundation.framework | `_AVVideoH264EntropyModeCABAC` | 3 | cleanshot, telegram, zoom |
| AVFoundation.framework | `_AVVideoH264EntropyModeKey` | 3 | cleanshot, telegram, zoom |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAudioMixInputParameters` | 3 | cleanshot, vorssaint, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureInput` | 3 | fluidvoice, macwhisper, vorssaint |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureMovieFileOutput` | 3 | nextcloud, openclaw, zoom |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableAudioMix` | 3 | cleanshot, vorssaint, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableAudioMixInputParameters` | 3 | cleanshot, vorssaint, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerItemLegibleOutput` | 3 | calibre, shotcut, wireshark-app |
| AVFoundation.framework | `_OBJC_CLASS_$_AVVideoCompositionLayerInstruction` | 3 | telegram, vorssaint, whatsapp |
| Accelerate.framework | `_BNNSFilterApplyTwoInput` | 3 | ollama-app, openclaw, voiceink |
| Accelerate.framework | `_BNNSFilterCreateLayerBroadcastMatMul` | 3 | ollama-app, openclaw, voiceink |
| Accelerate.framework | `_BNNSFilterDestroy` | 3 | ollama-app, openclaw, voiceink |
| Accelerate.framework | `_cblas_scopy` | 3 | fluidvoice, opensuperwhisper, voiceink |
| Accelerate.framework | `_cgeev$NEWLAPACK` | 3 | ollama-app, openclaw, voiceink |
| Accelerate.framework | `_dgetri$NEWLAPACK` | 3 | ollama-app, openclaw, voiceink |
| Accelerate.framework | `_dtrtri$NEWLAPACK` | 3 | ollama-app, openclaw, voiceink |
| Accelerate.framework | `_sgeqrf$NEWLAPACK` | 3 | ollama-app, openclaw, voiceink |
| Accelerate.framework | `_sgetri$NEWLAPACK` | 3 | ollama-app, openclaw, voiceink |
| Accelerate.framework | `_sorgqr$NEWLAPACK` | 3 | ollama-app, openclaw, voiceink |
| Accelerate.framework | `_strtri$NEWLAPACK` | 3 | ollama-app, openclaw, voiceink |
| Accelerate.framework | `_vDSP_vdpsp` | 3 | iina, shotcut, stremio |
| Accelerate.framework | `_vImageConvert_PlanarFtoPlanar16F` | 3 | fluidvoice, opensuperwhisper, voiceink |
| AudioToolbox.framework | `_MusicPlayerSetTime` | 3 | bettertouchtool, deluge, hammerspoon |
| AudioToolbox.framework | `_MusicSequenceFileLoadData` | 3 | bettertouchtool, deluge, hammerspoon |
| CoreAudio.framework | `_AudioDeviceAddIOProc` | 3 | deluge, shotcut, zoom |
| CoreAudio.framework | `_AudioDeviceRemoveIOProc` | 3 | deluge, shotcut, zoom |
| CoreAudio.framework | `_AudioDeviceSetProperty` | 3 | deluge, lmms, shotcut |
| CoreImage.framework | `_OBJC_CLASS_$_CIFeature` | 3 | sfm, shottr, whatsapp |
| CoreImage.framework | `_OBJC_CLASS_$_CIQRCodeFeature` | 3 | sfm, shottr, whatsapp |
| CoreImage.framework | `_kCIContextUseSoftwareRenderer` | 3 | macwhisper, telegram, whatsapp |
| CoreImage.framework | `_kCIFormatBGRA8` | 3 | cmux, telegram, whatsapp |
| CoreImage.framework | `_kCIInputCenterKey` | 3 | cleanshot, skim, vorssaint |
| CoreImage.framework | `_kCIInputContrastKey` | 3 | applite, betterdisplay, supacode |
| CoreMIDI.framework | `_MIDIDeviceGetEntity` | 3 | bettertouchtool, hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIDeviceGetNumberOfEntities` | 3 | bettertouchtool, hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIEntityGetDestination` | 3 | bettertouchtool, hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIEntityGetNumberOfDestinations` | 3 | bettertouchtool, hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIEntityGetNumberOfSources` | 3 | bettertouchtool, hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIEntityGetSource` | 3 | bettertouchtool, hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIGetDevice` | 3 | bettertouchtool, hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIGetNumberOfDevices` | 3 | bettertouchtool, hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIObjectFindByUniqueID` | 3 | bettertouchtool, hammerspoon, musescore |
| CoreMIDI.framework | `_kMIDIPropertyOffline` | 3 | bettertouchtool, hammerspoon, lmms |
| CoreMedia.framework | `_CMAudioFormatDescriptionGetChannelLayout` | 3 | fluidvoice, macwhisper, vorssaint |
| CoreMediaIO.framework | `_CMIODeviceStartStream` | 3 | obs, telegram, telegram-desktop |
| CoreMediaIO.framework | `_CMIODeviceStopStream` | 3 | obs, telegram, telegram-desktop |
| CoreMediaIO.framework | `_CMIOStreamCopyBufferQueue` | 3 | obs, telegram, telegram-desktop |
| MediaPlayer.framework | `_MPNowPlayingInfoPropertyMediaType` | 3 | iina, telegram, telegram-desktop |
| MediaPlayer.framework | `_OBJC_CLASS_$_MPChangePlaybackPositionCommandEvent` | 3 | iina, macwhisper, telegram |
| libswiftCoreMedia.dylib | `_$sSo17CMSampleBufferRefa9CoreMediaE10numSamplesSivg` | 3 | cleanshot, macwhisper, superwhisper |
| AVFAudio.framework | `_AVEncoderBitDepthHintKey` | 2 | calibre, shotcut |
| AVFAudio.framework | `_AVSampleRateConverterAlgorithm_Mastering` | 2 | fluidvoice, voiceink |
| AVFAudio.framework | `_OBJC_CLASS_$_AVAudioConnectionPoint` | 2 | fluidvoice, whatsapp |
| AVFoundation.framework | `_AVAssetExportPresetAppleM4A` | 2 | cleanshot, macwhisper |
| AVFoundation.framework | `_AVFileTypeAppleM4V` | 2 | localsend, nextcloud |
| AVFoundation.framework | `_AVFileTypeProfileMPEG4AppleHLS` | 2 | macwhisper, whatsapp |
| AVFoundation.framework | `_AVVideoCleanApertureHeightKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoCleanApertureHorizontalOffsetKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoCleanApertureKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoCleanApertureVerticalOffsetKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoCleanApertureWidthKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoPixelAspectRatioHorizontalSpacingKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoPixelAspectRatioKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoPixelAspectRatioVerticalSpacingKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoProfileLevelH264MainAutoLevel` | 2 | cleanshot, iterm2 |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetWriterInputMetadataAdaptor` | 2 | localsend, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureAudioChannel` | 2 | macwhisper, telegram |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureConnection` | 2 | macwhisper, telegram |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureOutput` | 2 | fluidvoice, macwhisper |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerItemOutput` | 2 | raycast, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVTimedMetadataGroup` | 2 | localsend, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVVideoComposition` | 2 | cleanshot, whatsapp |
| Accelerate.framework | `_cblas_daxpy` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_cblas_dgemv` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_cblas_saxpy` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_vDSP_biquad` | 2 | finetune, spotify |
| Accelerate.framework | `_vDSP_biquad_CreateSetup` | 2 | finetune, spotify |
| Accelerate.framework | `_vDSP_biquad_DestroySetup` | 2 | finetune, spotify |
| Accelerate.framework | `_vDSP_distancesq` | 2 | macwhisper, superwhisper |
| Accelerate.framework | `_vDSP_dotprD` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_vDSP_maxvD` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_vDSP_minmgv` | 2 | macwhisper, superwhisper |
| Accelerate.framework | `_vDSP_mmovD` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_vDSP_svesqD` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_vDSP_vaddD` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_vDSP_vdivD` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_vDSP_vfillD` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_vDSP_vflt32` | 2 | cleanshot, macwhisper |
| Accelerate.framework | `_vDSP_vmulD` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_vDSP_vrampmul` | 2 | spotify, vorssaint |
| Accelerate.framework | `_vDSP_vrampmul2` | 2 | spotify, vorssaint |
| Accelerate.framework | `_vDSP_vsaddD` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_vDSP_vsmulD` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_vDSP_vsqD` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_vDSP_vsubD` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_vDSP_vthrD` | 2 | fluidvoice, opensuperwhisper |
| Accelerate.framework | `_vImageConvert_AnyToAny` | 2 | gemini, telegram |
| Accelerate.framework | `_vImageConvert_Planar16FtoPlanarF` | 2 | fluidvoice, voiceink |
| Accelerate.framework | `_vImageConverter_CreateWithCGImageFormat` | 2 | gemini, telegram |
| Accelerate.framework | `_vImageConvolve_Planar8` | 2 | gemini, openclaw |
| Accelerate.framework | `_vImagePermuteChannels_ARGB8888` | 2 | shottr, telegram |
| Accelerate.framework | `_vvlog1pf` | 2 | fluidvoice, opensuperwhisper |
| AudioToolbox.framework | `_AUGraphGetIndNode` | 2 | deluge, openttd |
| AudioToolbox.framework | `_AUGraphGetNodeCount` | 2 | deluge, openttd |
| AudioToolbox.framework | `_AUGraphStop` | 2 | telegram, vlc |
| AudioToolbox.framework | `_AudioHardwareServiceGetPropertyData` | 2 | background-music, superwhisper |
| AudioToolbox.framework | `_AudioHardwareServiceHasProperty` | 2 | background-music, superwhisper |
| AudioToolbox.framework | `_AudioHardwareServiceSetPropertyData` | 2 | background-music, superwhisper |
| AudioToolbox.framework | `_DisposeMusicEventIterator` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicDeviceMIDIEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorDeleteEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorGetEventInfo` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorHasCurrentEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorHasNextEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorHasPreviousEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorNextEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorPreviousEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorSeek` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorSetEventTime` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicPlayerIsPlaying` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicSequenceDisposeTrack` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicSequenceFileCreateData` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicSequenceGetAUGraph` | 2 | deluge, openttd |
| AudioToolbox.framework | `_MusicSequenceGetSecondsForBeats` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicSequenceGetTempoTrack` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicSequenceGetTrackIndex` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicSequenceNewTrack` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicSequenceSetUserCallback` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicTrackGetSequence` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewAUPresetEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewExtendedNoteEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewExtendedTempoEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewMIDIChannelEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewMIDINoteEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewMIDIRawDataEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewMetaEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewParameterEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewUserEvent` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicTrackSetDestMIDIEndpoint` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_MusicTrackSetProperty` | 2 | bettertouchtool, hammerspoon |
| AudioToolbox.framework | `_NewMusicEventIterator` | 2 | bettertouchtool, hammerspoon |
| CoreAudio.framework | `_AudioDeviceGetPropertyInfo` | 2 | bbedit, lmms |
| CoreAudio.framework | `_AudioHardwareGetPropertyInfo` | 2 | lmms, shotcut |
| CoreImage.framework | `_OBJC_CLASS_$_CIColorKernel` | 2 | telegram, whatsapp |
| CoreImage.framework | `_OBJC_CLASS_$_CIKernel` | 2 | skim, whatsapp |
| CoreImage.framework | `_OBJC_CLASS_$_CIRenderDestination` | 2 | vorssaint, whatsapp |
| CoreImage.framework | `_kCIContextHighQualityDownsample` | 2 | duckduckgo, figma |
| CoreImage.framework | `_kCIContextName` | 2 | betterdisplay, whatsapp |
| CoreImage.framework | `_kCIContextPriorityRequestLow` | 2 | bartender, raycast |
| CoreImage.framework | `_kCIFormatRGBAf` | 2 | macwhisper, tailscale-app |
| CoreImage.framework | `_kCIImageApplyOrientationProperty` | 2 | vorssaint, whatsapp |
| CoreImage.framework | `_kCIInputSharpnessKey` | 2 | betterdisplay, vorssaint |
| CoreImage.framework | `_kCIInputTargetImageKey` | 2 | skim, telegram |
| CoreImage.framework | `_kCIInputWidthKey` | 2 | betterdisplay, skim |
| CoreMIDI.framework | `_MIDIObjectGetProperties` | 2 | bettertouchtool, hammerspoon |
| CoreMIDI.framework | `_MIDIObjectSetIntegerProperty` | 2 | bettertouchtool, hammerspoon |
| CoreMIDI.framework | `_MIDIObjectSetStringProperty` | 2 | bettertouchtool, hammerspoon |
| CoreMIDI.framework | `_MIDISourceCreate` | 2 | bettertouchtool, hammerspoon |
| CoreMIDI.framework | `_kMIDIPropertyModel` | 2 | bettertouchtool, hammerspoon |
| CoreMIDI.framework | `_kMIDIPropertyPrivate` | 2 | bettertouchtool, hammerspoon |
| CoreMedia.framework | `_CMSimpleQueueCreate` | 2 | handbrake-app, obs |
| CoreMedia.framework | `_CMSimpleQueueEnqueue` | 2 | handbrake-app, obs |
| CoreMedia.framework | `_kCMSampleBufferAttachmentKey_DroppedFrameReason` | 2 | telegram, telegram-desktop |
| CoreMediaIO.framework | `_CMIOObjectSetPropertyData` | 2 | macwhisper, obs |
| MediaPlayer.framework | `_MPNowPlayingInfoPropertyDefaultPlaybackRate` | 2 | iina, telegram-desktop |
| MediaPlayer.framework | `_OBJC_CLASS_$_MPRemoteCommand` | 2 | iina, telegram |
| MediaPlayer.framework | `_OBJC_CLASS_$_MPSkipIntervalCommandEvent` | 2 | iina, macwhisper |
| Speech.framework | `_OBJC_CLASS_$_SFTranscriptionSegment` | 2 | openclaw, whatsapp |
| VideoToolbox.framework | `_VTCopyVideoEncoderList` | 2 | obs, zoom |
| VideoToolbox.framework | `_VTPixelRotationSessionCreate` | 2 | calibre, macwhisper |
| VideoToolbox.framework | `_VTPixelRotationSessionInvalidate` | 2 | calibre, macwhisper |
| VideoToolbox.framework | `_VTPixelRotationSessionRotateImage` | 2 | calibre, macwhisper |
| VideoToolbox.framework | `_kVTDecompressionPropertyKey_OnlyTheseFrames` | 2 | jellyfin, stremio |
| VideoToolbox.framework | `_kVTDecompressionProperty_OnlyTheseFrames_KeyFrames` | 2 | jellyfin, stremio |
| VideoToolbox.framework | `_kVTH264EntropyMode_CAVLC` | 2 | handbrake-app, zoom |
| VideoToolbox.framework | `_kVTPixelRotationPropertyKey_FlipHorizontalOrientation` | 2 | calibre, macwhisper |
| VideoToolbox.framework | `_kVTPixelRotationPropertyKey_FlipVerticalOrientation` | 2 | calibre, macwhisper |
| VideoToolbox.framework | `_kVTPixelRotationPropertyKey_Rotation` | 2 | calibre, macwhisper |
| VideoToolbox.framework | `_kVTRotation_0` | 2 | calibre, macwhisper |
| VideoToolbox.framework | `_kVTRotation_180` | 2 | calibre, macwhisper |
| VideoToolbox.framework | `_kVTRotation_CCW90` | 2 | calibre, macwhisper |
| VideoToolbox.framework | `_kVTRotation_CW90` | 2 | calibre, macwhisper |
| VideoToolbox.framework | `_kVTVideoEncoderList_CodecType` | 2 | obs, zoom |
| Vision.framework | `_OBJC_CLASS_$_VNDetectHumanRectanglesRequest` | 2 | figma, whatsapp |
| Vision.framework | `_OBJC_CLASS_$_VNImageTranslationAlignmentObservation` | 2 | cleanshot, shottr |
| Vision.framework | `_OBJC_CLASS_$_VNPixelBufferObservation` | 2 | figma, macwhisper |
| Vision.framework | `_OBJC_CLASS_$_VNTrackObjectRequest` | 2 | teamviewer, whatsapp |
| Vision.framework | `_OBJC_CLASS_$_VNTranslationalImageRegistrationRequest` | 2 | cleanshot, shottr |
| libswiftAVFoundation.dylib | `_$sSo21AVAssetImageGeneratorC12AVFoundationE5image2atSo10CGImageRefaAD_So6CMTimea10actualTimetAI_tYaKF` | 2 | openclaw, vorssaint |
| libswiftAVFoundation.dylib | `_$sSo21AVAssetImageGeneratorC12AVFoundationE5image2atSo10CGImageRefaAD_So6CMTimea10actualTimetAI_tYaKFTu` | 2 | openclaw, vorssaint |
| libswiftAccelerate.dylib | `_$s10Accelerate0A6BufferP010withUnsafeB7Pointeryqd__qd__SRy7ElementQzGKXEKlFTj` | 2 | fluidvoice, voiceink |
| libswiftAccelerate.dylib | `_$s10Accelerate4BNNSO14applyReduction_5input6output7weights16filterParametersyAC0D8FunctionO_So21BNNSNDArrayDescriptoraA2LSgSo010BNNSFilterI0aSgtKFZ` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$s10Accelerate4BNNSO15applyActivation10activation5input6output9batchSize16filterParametersyAC0D8FunctionO_So21BNNSNDArrayDescriptoraAMSiSo010BNNSFilterK0aSgtKFZ` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$s10Accelerate4BNNSO17ReductionFunctionO3maxyA2EmFWC` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$s10Accelerate4BNNSO17ReductionFunctionO6argMaxyA2EmFWC` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$s10Accelerate4BNNSO17ReductionFunctionO9logSumExpyA2EmFWC` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$s10Accelerate4BNNSO17ReductionFunctionOMa` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$s10Accelerate4BNNSO18ActivationFunctionO10logSoftmaxyA2EmFWC` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$s10Accelerate4BNNSO18ActivationFunctionO6linearyAESf_tcAEmFWC` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$s10Accelerate4BNNSO18ActivationFunctionO7softmaxyA2EmFWC` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$s10Accelerate4BNNSO18ActivationFunctionOMa` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$s10Accelerate4BNNSO5ShapeO6vectoryAESi_SitcAEmFWC` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$s10Accelerate4BNNSO5ShapeOMa` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$s10Accelerate4BNNSO9applyTopK1k5input10bestValues0F7Indices4axis9batchSize16filterParametersySi_So21BNNSNDArrayDescriptoraA2MS2iSo010BNNSFilterM0aSgtKFZ` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO3FFTC5log2n5radix6ofTypeAEy_xGSgSu_AC5RadixOxmtcfc` | 2 | raycast, voiceink |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO3FFTC7forward5input6outputySo15DSPSplitComplexV_AJztFTj` | 2 | raycast, voiceink |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO3FFTCMn` | 2 | raycast, voiceink |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO5RadixO6radix2yA2EmFWC` | 2 | raycast, voiceink |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO5RadixOMa` | 2 | raycast, voiceink |
| libswiftAccelerate.dylib | `_$sSf10Accelerate10BNNSScalarAAWP` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$sSo21BNNSNDArrayDescriptora10AccelerateE10deallocateyyF` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$sSo21BNNSNDArrayDescriptora10AccelerateE21allocateUninitialized10scalarType5shape9batchSizeAbC10BNNSScalar_pXp_AC4BNNSO5ShapeOSitFZ` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$sSo21BNNSNDArrayDescriptora10AccelerateE4data10scalarType5shapeABSgSw_AC10BNNSScalar_pXpAC4BNNSO5ShapeOtcfC` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$sSo21BNNSNDArrayDescriptora10AccelerateE9makeArray2of9batchSizeSayxGSgxm_SitlF` | 2 | macwhisper, superwhisper |
| libswiftAccelerate.dylib | `_$ss5Int32V10Accelerate10BNNSScalarACWP` | 2 | macwhisper, superwhisper |
| libswiftCoreAudio.dylib | `_$sSo16CATapDescriptionC9CoreAudioE24stereoMixdownOfProcessesABSays6UInt32VG_tcfC` | 2 | finetune, superwhisper |
| AVFAudio.framework | `_AVAudioBitRateStrategy_Constant` | 1 | cleanshot |
| AVFAudio.framework | `_AVEncoderBitRateStrategyKey` | 1 | cleanshot |
| AVFAudio.framework | `_AVSpeechUtteranceMinimumSpeechRate` | 1 | calibre |
| AVFAudio.framework | `_OBJC_CLASS_$_AVAudioRoutingArbiter` | 1 | spotify |
| AVFoundation.framework | `_AVCaptureSessionPreset320x240` | 1 | obs |
| AVFoundation.framework | `_AVCaptureSessionPreset3840x2160` | 1 | obs |
| AVFoundation.framework | `_AVCaptureSessionPreset960x540` | 1 | obs |
| AVFoundation.framework | `_AVEncoderBitDepthHintKey` | 1 | wireshark-app |
| AVFoundation.framework | `_AVFileType3GPP` | 1 | localsend |
| AVFoundation.framework | `_AVFileType3GPP2` | 1 | localsend |
| AVFoundation.framework | `_AVFileTypeAC3` | 1 | localsend |
| AVFoundation.framework | `_AVFileTypeAIFC` | 1 | localsend |
| AVFoundation.framework | `_AVFileTypeAIFF` | 1 | localsend |
| AVFoundation.framework | `_AVFileTypeAMR` | 1 | localsend |
| AVFoundation.framework | `_AVFileTypeEnhancedAC3` | 1 | localsend |
| AVFoundation.framework | `_AVFileTypeSunAU` | 1 | localsend |
| AVFoundation.framework | `_AVMediaCharacteristicAudible` | 1 | localsend |
| AVFoundation.framework | `_AVMediaCharacteristicVisual` | 1 | cleanshot |
| AVFoundation.framework | `_AVMetadataCommonKeySubject` | 1 | nextcloud |
| AVFoundation.framework | `_AVMetadataFormatISOUserData` | 1 | macwhisper |
| AVFoundation.framework | `_AVMetadataFormatQuickTimeMetadata` | 1 | cleanshot |
| AVFoundation.framework | `_AVMetadataFormatQuickTimeUserData` | 1 | macwhisper |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataContentIdentifier` | 1 | vorssaint |
| AVFoundation.framework | `_AVMetadataQuickTimeMetadataKeySoftware` | 1 | cleanshot |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyTrackName` | 1 | telegram |
| AVFoundation.framework | `_AVMetadataiTunesMetadataKeyArtist` | 1 | telegram |
| AVFoundation.framework | `_AVMetadataiTunesMetadataKeySongName` | 1 | telegram |
| AVFoundation.framework | `_AVOutputSettingsPreset1920x1080` | 1 | macwhisper |
| AVFoundation.framework | `_AVOutputSettingsPreset3840x2160` | 1 | macwhisper |
| AVFoundation.framework | `_AVOutputSettingsPresetHEVC7680x4320` | 1 | macwhisper |
| AVFoundation.framework | `_AVPlayerItemNewErrorLogEntryNotification` | 1 | easydict |
| AVFoundation.framework | `_AVPlayerRateDidChangeNotification` | 1 | cmux |
| AVFoundation.framework | `_AVSampleBufferAudioRendererFlushTimeKey` | 1 | spotify |
| AVFoundation.framework | `_AVVideoCodecH264` | 1 | telegram |
| AVFoundation.framework | `_AVVideoCodecJPEG` | 1 | nextcloud |
| AVFoundation.framework | `_AVVideoCodecTypeAppleProRes422` | 1 | macwhisper |
| AVFoundation.framework | `_AVVideoCodecTypeAppleProRes4444` | 1 | macwhisper |
| AVFoundation.framework | `_AVVideoProfileLevelH264High40` | 1 | telegram |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAggregateAssetDownloadTask` | 1 | protonvpn |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetResourceLoadingDataRequest` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetSegmentTrackReport` | 1 | macwhisper |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetTrackSegment` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAudioCompressedBuffer` | 1 | telegram |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureInputPort` | 1 | macwhisper |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureMetadataOutput` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureMultiCamSession` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVComposition` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCompositionTrack` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCompositionTrackSegment` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMetadataFaceObject` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMetadataMachineReadableCodeObject` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVOutputSettingsAssistant` | 1 | macwhisper |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerItemTrack` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVVideoCompositionCoreAnimationTool` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVVideoCompositionInstruction` | 1 | whatsapp |
| AVKit.framework | `_OBJC_CLASS_$_AVPictureInPictureControllerContentSource` | 1 | whatsapp |
| AVKit.framework | `_OBJC_CLASS_$_AVPictureInPictureVideoCallViewController` | 1 | whatsapp |
| AVKit.framework | `_OBJC_METACLASS_$_AVPictureInPictureVideoCallViewController` | 1 | whatsapp |
| Accelerate.framework | `_cblas_caxpy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_cdotc_sub$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_cdotu_sub$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_cgemm` | 1 | bettertouchtool |
| Accelerate.framework | `_cblas_cgemv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_csyrk$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_daxpy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_ddot$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_dgemv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_dsyrk$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_saxpy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_sdot$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_sgemv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_ssyrk$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_zaxpy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_zdotc_sub$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_zdotu_sub$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_zgemm` | 1 | bettertouchtool |
| Accelerate.framework | `_cblas_zgemm$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_zgemv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_zsyrk$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_ccopy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cgelsd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cgesv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cgetrf$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cpotrf$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dcopy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dgels_` | 1 | bettertouchtool |
| Accelerate.framework | `_dgelsd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dgemm_` | 1 | gimp |
| Accelerate.framework | `_dgemv_` | 1 | gimp |
| Accelerate.framework | `_dgeqrf_` | 1 | bettertouchtool |
| Accelerate.framework | `_dger_` | 1 | gimp |
| Accelerate.framework | `_dgesdd_` | 1 | bettertouchtool |
| Accelerate.framework | `_dgesv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dgesv_` | 1 | bettertouchtool |
| Accelerate.framework | `_dgetrf_` | 1 | bettertouchtool |
| Accelerate.framework | `_dposv_` | 1 | bettertouchtool |
| Accelerate.framework | `_dpotrf_` | 1 | bettertouchtool |
| Accelerate.framework | `_dtrsm_` | 1 | gimp |
| Accelerate.framework | `_dtrsv_` | 1 | gimp |
| Accelerate.framework | `_kvImage_ARGBToYpCbCrMatrix_ITU_R_709_2` | 1 | gemini |
| Accelerate.framework | `_kvImage_YpCbCrToARGBMatrix_ITU_R_601_4` | 1 | telegram |
| Accelerate.framework | `_kvImage_YpCbCrToARGBMatrix_ITU_R_709_2` | 1 | telegram |
| Accelerate.framework | `_la_matrix_from_float_buffer` | 1 | gemini |
| Accelerate.framework | `_la_matrix_from_float_buffer_nocopy` | 1 | gemini |
| Accelerate.framework | `_la_matrix_product` | 1 | gemini |
| Accelerate.framework | `_la_matrix_to_float_buffer` | 1 | gemini |
| Accelerate.framework | `_la_transpose` | 1 | gemini |
| Accelerate.framework | `_scopy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_sgels_` | 1 | bettertouchtool |
| Accelerate.framework | `_sgelsd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_sgeqrf_` | 1 | bettertouchtool |
| Accelerate.framework | `_sgesdd_` | 1 | bettertouchtool |
| Accelerate.framework | `_sgesv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_sgesv_` | 1 | bettertouchtool |
| Accelerate.framework | `_sgetrf_` | 1 | bettertouchtool |
| Accelerate.framework | `_sposv_` | 1 | bettertouchtool |
| Accelerate.framework | `_spotrf_` | 1 | bettertouchtool |
| Accelerate.framework | `_vDSP_DFT_zrop_CreateSetup` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquad_SetCoefficientsDouble` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquadm` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquadm_CreateSetup` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquadm_DestroySetup` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquadm_ResetState` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquadm_SetCoefficientsDouble` | 1 | spotify |
| Accelerate.framework | `_vDSP_desamp` | 1 | vorssaint |
| Accelerate.framework | `_vDSP_fft_zrop` | 1 | spotify |
| Accelerate.framework | `_vDSP_meamgv` | 1 | bbedit |
| Accelerate.framework | `_vDSP_vfix16` | 1 | superwhisper |
| Accelerate.framework | `_vDSP_vlint` | 1 | superwhisper |
| Accelerate.framework | `_vDSP_vrampmuladd` | 1 | vorssaint |
| Accelerate.framework | `_vDSP_vsmsma` | 1 | spotify |
| Accelerate.framework | `_vDSP_vsort` | 1 | macwhisper |
| Accelerate.framework | `_vDSP_zvma` | 1 | spotify |
| Accelerate.framework | `_vImageAffineWarpCG_ARGB8888` | 1 | telegram |
| Accelerate.framework | `_vImageConvert_16Uto16F` | 1 | iina |
| Accelerate.framework | `_vImageConvert_420Yp8_Cb8_Cr8ToARGB8888` | 1 | telegram |
| Accelerate.framework | `_vImageConvert_ARGB16UtoRGB16U` | 1 | cleanshot |
| Accelerate.framework | `_vImageConvert_ARGB8888toRGB888` | 1 | cleanshot |
| Accelerate.framework | `_vImageConvert_ARGBFFFFtoRGBFFF` | 1 | cleanshot |
| Accelerate.framework | `_vImageConvert_YpCbCrToARGB_GenerateConversion` | 1 | telegram |
| Accelerate.framework | `_vImageConverter_MustOperateOutOfPlace` | 1 | telegram |
| Accelerate.framework | `_vImageConverter_Release` | 1 | gemini |
| Accelerate.framework | `_vImageHorizontalReflect_ARGB8888` | 1 | telegram |
| Accelerate.framework | `_vImageMatrixMultiply_ARGB8888ToPlanar8` | 1 | gemini |
| Accelerate.framework | `_vImagePremultiplyData_ARGB8888` | 1 | telegram |
| Accelerate.framework | `_vImagePremultiplyData_Planar8` | 1 | telegram |
| Accelerate.framework | `_vImagePremultiplyData_RGBA8888` | 1 | quicklook-video |
| Accelerate.framework | `_vImageRotate90_PlanarF` | 1 | gemini |
| Accelerate.framework | `_vImageScale_ARGB16U` | 1 | cleanshot |
| Accelerate.framework | `_vImageScale_ARGBFFFF` | 1 | cleanshot |
| Accelerate.framework | `_vImageUnpremultiplyData_ARGB8888` | 1 | telegram |
| Accelerate.framework | `_zcopy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_zgeev$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_zgelsd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_zgemm_` | 1 | gimp |
| Accelerate.framework | `_zgemv_` | 1 | gimp |
| Accelerate.framework | `_zgeqrf$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_zgeru_` | 1 | gimp |
| Accelerate.framework | `_zgesdd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_zgesv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_zgetrf$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_zheevd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_zpotrf$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_ztrsm_` | 1 | gimp |
| Accelerate.framework | `_ztrsv_` | 1 | gimp |
| Accelerate.framework | `_zungqr$NEWLAPACK` | 1 | blender |
| AudioToolbox.framework | `_AUEventListenerAddEventType` | 1 | audacity |
| AudioToolbox.framework | `_AUEventListenerCreate` | 1 | audacity |
| AudioToolbox.framework | `_AUGraphClose` | 1 | telegram |
| AudioToolbox.framework | `_AUGraphSetNodeInputCallback` | 1 | telegram |
| AudioToolbox.framework | `_AUGraphUninitialize` | 1 | telegram |
| AudioToolbox.framework | `_AUListenerDispose` | 1 | audacity |
| AudioToolbox.framework | `_AUParameterListenerNotify` | 1 | audacity |
| AudioToolbox.framework | `_AudioComponentInstantiate` | 1 | audacity |
| AudioToolbox.framework | `_AudioConverterFillBuffer` | 1 | lmms |
| AudioToolbox.framework | `_AudioFileGetGlobalInfo` | 1 | spotify |
| AudioToolbox.framework | `_AudioHardwareServiceIsPropertySettable` | 1 | background-music |
| AudioToolbox.framework | `_AudioQueueNewInputWithDispatchQueue` | 1 | windows-app |
| AudioToolbox.framework | `_CAClockDispose` | 1 | blender |
| AudioToolbox.framework | `_CAClockGetCurrentTime` | 1 | blender |
| AudioToolbox.framework | `_CAClockGetStartTime` | 1 | blender |
| AudioToolbox.framework | `_CAClockNew` | 1 | blender |
| AudioToolbox.framework | `_CAClockSetCurrentTime` | 1 | blender |
| AudioToolbox.framework | `_CAClockSetProperty` | 1 | blender |
| AudioToolbox.framework | `_CAClockStart` | 1 | blender |
| AudioToolbox.framework | `_CAClockStop` | 1 | blender |
| AudioToolbox.framework | `_MusicSequenceFileLoad` | 1 | openttd |
| AudioToolbox.framework | `_OBJC_CLASS_$_AUAudioUnit` | 1 | whatsapp |
| AudioToolbox.framework | `_OBJC_CLASS_$_AUAudioUnitBus` | 1 | whatsapp |
| AudioToolbox.framework | `_OBJC_CLASS_$_AUAudioUnitBusArray` | 1 | whatsapp |
| AudioToolbox.framework | `_OBJC_CLASS_$_AUParameter` | 1 | whatsapp |
| AudioToolbox.framework | `_OBJC_CLASS_$_AUParameterTree` | 1 | whatsapp |
| AudioToolbox.framework | `_OBJC_METACLASS_$_AUAudioUnit` | 1 | whatsapp |
| AudioUnit.framework | `_MusicDeviceMIDIEvent` | 1 | vlc |
| CoreAudio.framework | `_AudioDeviceAddPropertyListener` | 1 | lmms |
| CoreAudio.framework | `_AudioDeviceRemovePropertyListener` | 1 | lmms |
| CoreAudio.framework | `_AudioStreamGetProperty` | 1 | lmms |
| CoreAudioKit.framework | `_OBJC_CLASS_$_AUGenericView` | 1 | audacity |
| CoreAudioKit.framework | `_OBJC_CLASS_$_AUPannerView` | 1 | audacity |
| CoreImage.framework | `_OBJC_CLASS_$_CIBlendKernel` | 1 | cmux |
| CoreImage.framework | `_OBJC_CLASS_$_CIFaceFeature` | 1 | whatsapp |
| CoreImage.framework | `_OBJC_CLASS_$_CIPlugIn` | 1 | skim |
| CoreImage.framework | `_OBJC_CLASS_$_CIQRCodeDescriptor` | 1 | whatsapp |
| CoreImage.framework | `_OBJC_CLASS_$_CISampler` | 1 | skim |
| CoreImage.framework | `_kCIApplyOptionDefinition` | 1 | skim |
| CoreImage.framework | `_kCIApplyOptionExtent` | 1 | skim |
| CoreImage.framework | `_kCIApplyOptionUserInfo` | 1 | skim |
| CoreImage.framework | `_kCIAttributeIdentity` | 1 | skim |
| CoreImage.framework | `_kCIAttributeMax` | 1 | skim |
| CoreImage.framework | `_kCIAttributeMin` | 1 | skim |
| CoreImage.framework | `_kCIAttributeTypeAngle` | 1 | skim |
| CoreImage.framework | `_kCIAttributeTypeBoolean` | 1 | skim |
| CoreImage.framework | `_kCIAttributeTypeDistance` | 1 | skim |
| CoreImage.framework | `_kCIAttributeTypePosition` | 1 | skim |
| CoreImage.framework | `_kCIAttributeTypeRectangle` | 1 | skim |
| CoreImage.framework | `_kCIAttributeTypeTime` | 1 | skim |
| CoreImage.framework | `_kCICategoryTransition` | 1 | skim |
| CoreImage.framework | `_kCIContextOutputPremultiplied` | 1 | whatsapp |
| CoreImage.framework | `_kCIContextWorkingFormat` | 1 | whatsapp |
| CoreImage.framework | `_kCIFormatABGR8` | 1 | microsoft-teams |
| CoreImage.framework | `_kCIFormatRGBAh` | 1 | whatsapp |
| CoreImage.framework | `_kCIImageAutoAdjustRedEye` | 1 | telegram |
| CoreImage.framework | `_kCIImageProperties` | 1 | whatsapp |
| CoreImage.framework | `_kCIImageToneMapHDRtoSDR` | 1 | whatsapp |
| CoreImage.framework | `_kCIInputAmountKey` | 1 | betterdisplay |
| CoreImage.framework | `_kCIInputShadingImageKey` | 1 | skim |
| CoreImage.framework | `_kCIInputTimeKey` | 1 | skim |
| CoreMIDI.framework | `_MIDIEndpointGetEntity` | 1 | lmms |
| CoreMIDI.framework | `_MIDIEntityGetDevice` | 1 | lmms |
| CoreMedia.framework | `_CMAudioDeviceClockCreate` | 1 | telegram |
| CoreMedia.framework | `_CMAudioDeviceClockCreateFromAudioDeviceID` | 1 | telegram |
| CoreMedia.framework | `_CMFormatDescriptionCreate` | 1 | obs |
| CoreMedia.framework | `_CMSampleBufferSetDataReady` | 1 | vorssaint |
| CoreMedia.framework | `_CMSetAttachments` | 1 | fluidvoice |
| CoreMedia.framework | `_CMSimpleQueueGetCapacity` | 1 | obs |
| CoreMedia.framework | `_CMSimpleQueueGetCount` | 1 | obs |
| CoreMedia.framework | `_CMSyncGetTime` | 1 | telegram |
| CoreMedia.framework | `_CMTimebaseAddTimer` | 1 | telegram |
| CoreMedia.framework | `_CMTimebaseGetMaster` | 1 | telegram |
| CoreMedia.framework | `_CMTimebaseGetTimeWithTimeScale` | 1 | spotify |
| CoreMedia.framework | `_CMTimebaseRemoveTimer` | 1 | telegram |
| CoreMedia.framework | `_CMTimebaseSetRateAndAnchorTime` | 1 | telegram |
| CoreMedia.framework | `_CMTimebaseSetTimerNextFireTime` | 1 | telegram |
| CoreMedia.framework | `_CMVideoFormatDescriptionMatchesImageBuffer` | 1 | moonlight |
| CoreMedia.framework | `_kCMFormatDescriptionExtension_BitsPerComponent` | 1 | libndi |
| CoreMedia.framework | `_kCMFormatDescriptionExtension_ContainsAlphaChannel` | 1 | vorssaint |
| CoreMedia.framework | `_kCMFormatDescriptionTransferFunction_ITU_R_709_2` | 1 | vorssaint |
| CoreMedia.framework | `_kCMSampleAttachmentKey_DependsOnOthers` | 1 | obs |
| CoreMedia.framework | `_kCMSampleAttachmentKey_PartialSync` | 1 | handbrake-app |
| CoreMedia.framework | `_kCMSampleBufferAttachmentKey_EndsPreviousSampleDuration` | 1 | telegram |
| CoreMediaIO.framework | `_CMIOExtensionPropertyDeviceModel` | 1 | obs |
| CoreMediaIO.framework | `_CMIOExtensionPropertyDeviceTransportType` | 1 | obs |
| CoreMediaIO.framework | `_CMIOExtensionPropertyProviderManufacturer` | 1 | obs |
| CoreMediaIO.framework | `_CMIOExtensionPropertyProviderName` | 1 | obs |
| CoreMediaIO.framework | `_CMIOExtensionPropertyStreamActiveFormatIndex` | 1 | obs |
| CoreMediaIO.framework | `_CMIOExtensionPropertyStreamFrameDuration` | 1 | obs |
| CoreMediaIO.framework | `_CMIOExtensionPropertyStreamSinkBufferQueueSize` | 1 | obs |
| CoreMediaIO.framework | `_CMIOExtensionPropertyStreamSinkBufferUnderrunCount` | 1 | obs |
| CoreMediaIO.framework | `_CMIOExtensionPropertyStreamSinkBuffersRequiredForStartup` | 1 | obs |
| CoreMediaIO.framework | `_CMIOExtensionPropertyStreamSinkEndOfData` | 1 | obs |
| CoreMediaIO.framework | `_CMIOObjectAddPropertyListenerBlock` | 1 | hammerspoon |
| CoreMediaIO.framework | `_CMIOObjectCreate` | 1 | obs |
| CoreMediaIO.framework | `_CMIOObjectRemovePropertyListenerBlock` | 1 | hammerspoon |
| CoreMediaIO.framework | `_CMIOObjectsPublishedAndDied` | 1 | obs |
| CoreMediaIO.framework | `_CMIOSampleBufferCreateForImageBuffer` | 1 | obs |
| CoreMediaIO.framework | `_CMIOStreamClockCreate` | 1 | obs |
| CoreMediaIO.framework | `_CMIOStreamClockInvalidate` | 1 | obs |
| CoreMediaIO.framework | `_CMIOStreamClockPostTimingEvent` | 1 | obs |
| CoreMediaIO.framework | `_OBJC_CLASS_$_CMIOExtensionDevice` | 1 | obs |
| CoreMediaIO.framework | `_OBJC_CLASS_$_CMIOExtensionDeviceProperties` | 1 | obs |
| CoreMediaIO.framework | `_OBJC_CLASS_$_CMIOExtensionProvider` | 1 | obs |
| CoreMediaIO.framework | `_OBJC_CLASS_$_CMIOExtensionProviderProperties` | 1 | obs |
| CoreMediaIO.framework | `_OBJC_CLASS_$_CMIOExtensionScheduledOutput` | 1 | obs |
| CoreMediaIO.framework | `_OBJC_CLASS_$_CMIOExtensionStream` | 1 | obs |
| CoreMediaIO.framework | `_OBJC_CLASS_$_CMIOExtensionStreamFormat` | 1 | obs |
| CoreMediaIO.framework | `_OBJC_CLASS_$_CMIOExtensionStreamProperties` | 1 | obs |
| MediaPlayer.framework | `_MPNowPlayingInfoPropertyAssetURL` | 1 | iina |
| MediaPlayer.framework | `_MPNowPlayingInfoPropertyChapterCount` | 1 | iina |
| MediaPlayer.framework | `_MPNowPlayingInfoPropertyChapterNumber` | 1 | iina |
| MediaPlayer.framework | `_MPNowPlayingInfoPropertyPlaybackQueueCount` | 1 | iina |
| MediaPlayer.framework | `_MPNowPlayingInfoPropertyPlaybackQueueIndex` | 1 | iina |
| MediaPlayer.framework | `_OBJC_CLASS_$_MPChangePlaybackRateCommandEvent` | 1 | iina |
| MediaPlayer.framework | `_OBJC_CLASS_$_MPVolumeView` | 1 | whatsapp |
| OpenAL.framework | `_alBufferData` | 1 | blender |
| OpenAL.framework | `_alDeleteBuffers` | 1 | blender |
| OpenAL.framework | `_alDeleteSources` | 1 | blender |
| OpenAL.framework | `_alDistanceModel` | 1 | blender |
| OpenAL.framework | `_alDopplerFactor` | 1 | blender |
| OpenAL.framework | `_alGenBuffers` | 1 | blender |
| OpenAL.framework | `_alGenSources` | 1 | blender |
| OpenAL.framework | `_alGetEnumValue` | 1 | blender |
| OpenAL.framework | `_alGetError` | 1 | blender |
| OpenAL.framework | `_alGetFloat` | 1 | blender |
| OpenAL.framework | `_alGetInteger` | 1 | blender |
| OpenAL.framework | `_alGetListenerf` | 1 | blender |
| OpenAL.framework | `_alGetListenerfv` | 1 | blender |
| OpenAL.framework | `_alGetSourcef` | 1 | blender |
| OpenAL.framework | `_alGetSourcefv` | 1 | blender |
| OpenAL.framework | `_alGetSourcei` | 1 | blender |
| OpenAL.framework | `_alIsExtensionPresent` | 1 | blender |
| OpenAL.framework | `_alListenerf` | 1 | blender |
| OpenAL.framework | `_alListenerfv` | 1 | blender |
| OpenAL.framework | `_alSourcePause` | 1 | blender |
| OpenAL.framework | `_alSourcePlay` | 1 | blender |
| OpenAL.framework | `_alSourceQueueBuffers` | 1 | blender |
| OpenAL.framework | `_alSourceRewind` | 1 | blender |
| OpenAL.framework | `_alSourceStop` | 1 | blender |
| OpenAL.framework | `_alSourceUnqueueBuffers` | 1 | blender |
| OpenAL.framework | `_alSourcef` | 1 | blender |
| OpenAL.framework | `_alSourcefv` | 1 | blender |
| OpenAL.framework | `_alSourcei` | 1 | blender |
| OpenAL.framework | `_alSpeedOfSound` | 1 | blender |
| OpenAL.framework | `_alcCloseDevice` | 1 | blender |
| OpenAL.framework | `_alcCreateContext` | 1 | blender |
| OpenAL.framework | `_alcDestroyContext` | 1 | blender |
| OpenAL.framework | `_alcGetEnumValue` | 1 | blender |
| OpenAL.framework | `_alcGetError` | 1 | blender |
| OpenAL.framework | `_alcGetIntegerv` | 1 | blender |
| OpenAL.framework | `_alcGetString` | 1 | blender |
| OpenAL.framework | `_alcIsExtensionPresent` | 1 | blender |
| OpenAL.framework | `_alcMakeContextCurrent` | 1 | blender |
| OpenAL.framework | `_alcOpenDevice` | 1 | blender |
| OpenAL.framework | `_alcProcessContext` | 1 | blender |
| OpenAL.framework | `_alcSuspendContext` | 1 | blender |
| PIP.framework | `_OBJC_CLASS_$_PIPViewController` | 1 | iina |
| PIP.framework | `_OBJC_METACLASS_$_PIPViewController` | 1 | iina |
| QuartzCore.framework | `_OBJC_CLASS_$_CIContext` | 1 | vlc |
| QuartzCore.framework | `_kCIContextOutputColorSpace` | 1 | vlc |
| QuartzCore.framework | `_kCIContextWorkingColorSpace` | 1 | vlc |
| QuartzCore.framework | `_kCIInputImageKey` | 1 | vlc |
| QuartzCore.framework | `_kCIOutputImageKey` | 1 | vlc |
| Speech.framework | `_OBJC_CLASS_$_SFSpeechURLRecognitionRequest` | 1 | whatsapp |
| VideoToolbox.framework | `_VTCompressionSessionBeginPass` | 1 | handbrake-app |
| VideoToolbox.framework | `_VTCompressionSessionEndPass` | 1 | handbrake-app |
| VideoToolbox.framework | `_VTCompressionSessionGetTimeRangesForNextPass` | 1 | handbrake-app |
| VideoToolbox.framework | `_VTMultiPassStorageClose` | 1 | handbrake-app |
| VideoToolbox.framework | `_VTMultiPassStorageCreate` | 1 | handbrake-app |
| VideoToolbox.framework | `_kVTCompressionPropertyKey_AllowTemporalCompression` | 1 | handbrake-app |
| VideoToolbox.framework | `_kVTCompressionPropertyKey_ConstantBitRate` | 1 | obs |
| VideoToolbox.framework | `_kVTCompressionPropertyKey_FieldCount` | 1 | handbrake-app |
| VideoToolbox.framework | `_kVTCompressionPropertyKey_FieldDetail` | 1 | handbrake-app |
| VideoToolbox.framework | `_kVTCompressionPropertyKey_MinAllowedFrameQP` | 1 | microsoft-teams |
| VideoToolbox.framework | `_kVTCompressionPropertyKey_MultiPassStorage` | 1 | handbrake-app |
| VideoToolbox.framework | `_kVTCompressionPropertyKey_SupportsBaseFrameQP` | 1 | microsoft-teams |
| VideoToolbox.framework | `_kVTDecompressionPropertyKey_DeinterlaceMode` | 1 | vlc |
| VideoToolbox.framework | `_kVTDecompressionPropertyKey_FieldMode` | 1 | vlc |
| VideoToolbox.framework | `_kVTDecompressionProperty_DeinterlaceMode_Temporal` | 1 | vlc |
| VideoToolbox.framework | `_kVTDecompressionProperty_FieldMode_DeinterlaceFields` | 1 | vlc |
| VideoToolbox.framework | `_kVTDownsamplingMode_Average` | 1 | handbrake-app |
| VideoToolbox.framework | `_kVTPixelTransferPropertyKey_DestinationCleanAperture` | 1 | handbrake-app |
| VideoToolbox.framework | `_kVTPixelTransferPropertyKey_DownsamplingMode` | 1 | handbrake-app |
| VideoToolbox.framework | `_kVTPixelTransferPropertyKey_RealTime` | 1 | microsoft-teams |
| VideoToolbox.framework | `_kVTPropertySupportedValueListKey` | 1 | handbrake-app |
| VideoToolbox.framework | `_kVTScalingMode_Letterbox` | 1 | microsoft-teams |
| VideoToolbox.framework | `_kVTScalingMode_Normal` | 1 | microsoft-teams |
| VideoToolbox.framework | `_kVTVideoEncoderList_DisplayName` | 1 | obs |
| VideoToolbox.framework | `_kVTVideoEncoderList_EncoderID` | 1 | obs |
| VideoToolbox.framework | `_kVTVideoEncoderList_IsHardwareAccelerated` | 1 | obs |
| VideoToolbox.framework | `_kVTVideoEncoderSpecification_EncoderID` | 1 | obs |
| Vision.framework | `_OBJC_CLASS_$_VNContour` | 1 | whatsapp |
| Vision.framework | `_OBJC_CLASS_$_VNContoursObservation` | 1 | whatsapp |
| Vision.framework | `_OBJC_CLASS_$_VNDetectContoursRequest` | 1 | whatsapp |
| Vision.framework | `_OBJC_CLASS_$_VNDetectTextRectanglesRequest` | 1 | cleanshot |
| Vision.framework | `_OBJC_CLASS_$_VNGenerateObjectnessBasedSaliencyImageRequest` | 1 | whatsapp |
| Vision.framework | `_OBJC_CLASS_$_VNGeneratePersonSegmentationRequest` | 1 | macwhisper |
| Vision.framework | `_OBJC_CLASS_$_VNHumanObservation` | 1 | whatsapp |
| Vision.framework | `_OBJC_CLASS_$_VNSaliencyImageObservation` | 1 | whatsapp |
| Vision.framework | `_OBJC_CLASS_$_VNTextObservation` | 1 | cleanshot |
| VisionKit.framework | `_$s9VisionKit13ImageAnalysisC10transcriptSSvg` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit13ImageAnalysisCMn` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit13ImageAnalyzerC13AnalysisTypesV4textAEvgZ` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit13ImageAnalyzerC13AnalysisTypesVMa` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit13ImageAnalyzerC13AnalysisTypesVMn` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit13ImageAnalyzerC13AnalysisTypesVs10SetAlgebraAAMc` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit13ImageAnalyzerC13ConfigurationVMa` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit13ImageAnalyzerC13ConfigurationVyAeC13AnalysisTypesVcfC` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit13ImageAnalyzerC7analyze_11orientation13configurationAA0C8AnalysisCSo7NSImageC_So26CGImagePropertyOrientationVAC13ConfigurationVtYaKF` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit13ImageAnalyzerC7analyze_11orientation13configurationAA0C8AnalysisCSo7NSImageC_So26CGImagePropertyOrientationVAC13ConfigurationVtYaKFTu` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit13ImageAnalyzerCACycfc` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit13ImageAnalyzerCMa` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit24ImageAnalysisOverlayViewC16InteractionTypesV13textSelectionAEvgZ` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit24ImageAnalysisOverlayViewC16InteractionTypesVMa` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit24ImageAnalysisOverlayViewC25preferredInteractionTypesAC0hI0Vvs` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit24ImageAnalysisOverlayViewC26selectableItemsHighlightedSbvs` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit24ImageAnalysisOverlayViewC8analysisAA0cD0CSgvs` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit24ImageAnalysisOverlayViewCMa` | 1 | coteditor |
| VisionKit.framework | `_$s9VisionKit24ImageAnalysisOverlayViewCMn` | 1 | coteditor |
| VisionKit.framework | `_OBJC_CLASS_$_VNDocumentCameraViewController` | 1 | whatsapp |
| libswiftAVFoundation.dylib | `_$s12AVFoundation22AVPartialAsyncPropertyCAASo12AVAssetTrackCRbzlE20mediaCharacteristicsAA07AVAsyncD0CyxSaySo21AVMediaCharacteristicaGGvgZ` | 1 | vorssaint |
| libswiftAVFoundation.dylib | `_$s12AVFoundation22AVPartialAsyncPropertyCAASo12AVAssetTrackCRbzlE8metadataAA07AVAsyncD0CyxSaySo14AVMetadataItemCGGvgZ` | 1 | vorssaint |
| libswiftAVFoundation.dylib | `_$s12AVFoundation22AVPartialAsyncPropertyCAASo14AVMetadataItemCRbzlE9dataValueAA07AVAsyncD0Cyx10Foundation4DataVSgGvgZ` | 1 | macwhisper |
| libswiftAVFoundation.dylib | `_$s12AVFoundation22AVPartialAsyncPropertyCAASo7AVAssetCRbzlE033availableMediaCharacteristicsWithG16SelectionOptionsAA07AVAsyncD0CyxSaySo21AVMediaCharacteristicaGGvgZ` | 1 | cmux |
| libswiftAVFoundation.dylib | `_$s12AVFoundation22AVPartialAsyncPropertyCMn` | 1 | raycast |
| libswiftAVFoundation.dylib | `_$sSo12AVPlayerItemC12AVFoundationE5asset28automaticallyLoadedAssetKeysABSo7AVAssetC_SayAC22AVPartialAsyncPropertyCyAGGGtcfC` | 1 | raycast |
| libswiftAVFoundation.dylib | `_$sSo20AVAssetExportSessionC12AVFoundationE6export2to2as9isolationy10Foundation3URLV_So10AVFileTypeaScA_pSgYitYaKF` | 1 | openclaw |
| libswiftAVFoundation.dylib | `_$sSo20AVAssetExportSessionC12AVFoundationE6export2to2as9isolationy10Foundation3URLV_So10AVFileTypeaScA_pSgYitYaKFTu` | 1 | openclaw |
| libswiftAVFoundation.dylib | `_$sSo29AVAsynchronousKeyValueLoadingP12AVFoundationE4load_9isolationqd__AC15AVAsyncPropertyCyxqd__G_ScA_pSgYitYaKlF` | 1 | coteditor |
| libswiftAVFoundation.dylib | `_$sSo29AVAsynchronousKeyValueLoadingP12AVFoundationE4load_9isolationqd__AC15AVAsyncPropertyCyxqd__G_ScA_pSgYitYaKlFTu` | 1 | coteditor |
| libswiftAVFoundation.dylib | `_$sSo29AVAsynchronousKeyValueLoadingP12AVFoundationE4load___9isolationqd___qd_0_qd_1_qd_1_QptAC15AVAsyncPropertyCyxqd__G_AGyxqd_0_GAGyxqd_1_Gqd_1_QpScA_pSgYitYaKRvd_1_r1_lF` | 1 | coteditor |
| libswiftAVFoundation.dylib | `_$sSo29AVAsynchronousKeyValueLoadingP12AVFoundationE4load___9isolationqd___qd_0_qd_1_qd_1_QptAC15AVAsyncPropertyCyxqd__G_AGyxqd_0_GAGyxqd_1_Gqd_1_QpScA_pSgYitYaKRvd_1_r1_lFTu` | 1 | coteditor |
| libswiftAVFoundation.dylib | `_$sSo29AVAsynchronousKeyValueLoadingP12AVFoundationE4loadyqd__AC15AVAsyncPropertyCyxqd__GYaKlF` | 1 | opensuperwhisper |
| libswiftAVFoundation.dylib | `_$sSo29AVAsynchronousKeyValueLoadingP12AVFoundationE4loadyqd__AC15AVAsyncPropertyCyxqd__GYaKlFTu` | 1 | opensuperwhisper |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO10formWindow13usingSequence6result06isHalfD0yAC0dF0O_xzSbtAA0A13MutableBufferRzSf7ElementRtzlFZ` | 1 | raycast |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO14WindowSequenceO19hanningDenormalizedyA2EmFWC` | 1 | raycast |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO14WindowSequenceOMa` | 1 | raycast |
| libswiftAccelerate.dylib | `_$sSryxG10Accelerate0A13MutableBufferABMc` | 1 | raycast |
| libswiftCoreAudio.dylib | `_$sSo16CATapDescriptionC9CoreAudioE32monoGlobalTapButExcludeProcessesABSays6UInt32VG_tcfC` | 1 | superwhisper |
| libswiftCoreAudio.dylib | `_$sSo16CATapDescriptionC9CoreAudioE9processes9deviceUID6streamABSays6UInt32VG_SSSutcfC` | 1 | finetune |
| libswiftCoreAudio.dylib | `_$sSo26AudioObjectPropertyAddressV04CoreA0E2eeoiySbAB_ABtFZ` | 1 | finetune |
| libswiftCoreMedia.dylib | `_$s9CoreMedia29CMAttachmentBearerAttachmentsV5ValueO5valueypvg` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$s9CoreMedia29CMAttachmentBearerAttachmentsV5ValueOMa` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$s9CoreMedia29CMAttachmentBearerAttachmentsV5ValueOMn` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$s9CoreMedia29CMAttachmentBearerAttachmentsVMa` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$s9CoreMedia29CMAttachmentBearerAttachmentsVyAC5ValueOSgSo17CMSampleBufferRefaAAE13AttachmentKeyVcig` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$s9CoreMedia29_CMSampleBufferInitTrampolinePAAE04dataD017formatDescription10numSamples13sampleTimings0L5SizesxSo07CMBlockD3RefaSg_So08CMFormatiP0aSgSiSaySo0C10TimingInfoaGSaySiGtKcfC` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$s9CoreMedia29_CMSampleBufferInitTrampolinePAAE7copying13withNewTimingxSo0cD3Refa_SaySo0cJ4InfoaGtKcfC` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$s9CoreMedia34_CMFormatDescriptionInitTrampolinePAAE14videoCodecType5width6height10extensionsxSo0cD3RefaAAE0b3SubI0V_S2iAiAE10ExtensionsVSgtKcfC` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo10CMClockRefa9CoreMedia14CMSyncProtocolACMc` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo10CMClockRefa9CoreMediaE11convertTime_2toSo6CMTimeaAG_xtAC14CMSyncProtocolRzlF` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo10CMClockRefa9CoreMediaE13hostTimeClockABvgZ` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo10CMClockRefa9CoreMediaE4timeSo6CMTimeavg` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo17CMSampleBufferRefa9CoreMedia01_aB14InitTrampolineACWP` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo17CMSampleBufferRefa9CoreMediaE07setDataB009fromAudioB4List05blockB15MemoryAllocator5flagsySPySo0ibJ0VG_So011CFAllocatorC0aSgAbCE5FlagsVtKF` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo17CMSampleBufferRefa9CoreMediaE11attachmentsAC29CMAttachmentBearerAttachmentsVvg` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo17CMSampleBufferRefa9CoreMediaE11copyPCMData9fromRange4intoySnySiG_SpySo05AudioB4ListVGtKF` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo17CMSampleBufferRefa9CoreMediaE13AttachmentKeyV18droppedFrameReasonAEvgZ` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo17CMSampleBufferRefa9CoreMediaE13AttachmentKeyV22droppedFrameReasonInfoAEvgZ` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo17CMSampleBufferRefa9CoreMediaE13AttachmentKeyVMa` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo17CMSampleBufferRefa9CoreMediaE17sampleTimingInfosSaySo0aG4InfoaGyKF` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo17CMSampleBufferRefa9CoreMediaE5FlagsV05audioB25ListAssure16ByteAlignmentAEvgZ` | 1 | raycast |
| libswiftCoreMedia.dylib | `_$sSo22CMFormatDescriptionRefa9CoreMedia01_aB14InitTrampolineACWP` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo22CMFormatDescriptionRefa9CoreMediaE0E4TypeV5videoAEvgZ` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo22CMFormatDescriptionRefa9CoreMediaE0E4TypeVMa` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo22CMFormatDescriptionRefa9CoreMediaE0E4TypeVSYACMc` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo22CMFormatDescriptionRefa9CoreMediaE10ExtensionsVMn` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo22CMFormatDescriptionRefa9CoreMediaE13frameDurationSo6CMTimeavg` | 1 | macwhisper |
| libswiftCoreMedia.dylib | `_$sSo22CMFormatDescriptionRefa9CoreMediaE9mediaTypeAbCE0eG0Vvg` | 1 | macwhisper |
| libswiftCoreMediaIO.dylib | `_$sSo25CMIOExtensionStreamFormatC11CoreMediaIOE17formatDescription16maxFrameDuration03minjK005validJ9DurationsABSo08CMFormatH3Refa_So6CMTimeaAKSayAKGSgtcfC` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionDevicePropertiesC11CoreMediaIOE13transportTypeSiSgvs` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE13frameDurationSo6CMTimeaSgvs` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE17activeFormatIndexSiSgvg` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE17activeFormatIndexSiSgvs` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE19sinkBufferQueueSizeSiSgvs` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE29sinkBuffersRequiredForStartupSiSgvs` | 1 | obs |
