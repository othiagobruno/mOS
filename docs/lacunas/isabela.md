# Lacunas de Isabela (mídia)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Isabela (mídia) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**724** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreAudio.framework | `_AudioConvertHostTimeToNanos` | 22 | android-studio, audacity, cyberduck, dbeaver-community |
| CoreImage.framework | `_OBJC_CLASS_$_CIContext` | 19 | betterdisplay, claude, cmux, figma |
| CoreAudio.framework | `_AudioGetCurrentHostTime` | 18 | android-studio, audacity, cyberduck, dbeaver-community |
| CoreImage.framework | `_kCIInputImageKey` | 14 | betterdisplay, claude, cmux, iina |
| CoreImage.framework | `_OBJC_CLASS_$_CIColor` | 12 | betterdisplay, claude, cmux, figma |
| CoreMIDI.framework | `_MIDIPortDispose` | 12 | firefox, floorp, godot, hammerspoon |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerLayer` | 11 | cmux, libreoffice, localsend, raycast |
| CoreMIDI.framework | `_MIDIEndpointDispose` | 10 | deluge, firefox, floorp, hammerspoon |
| AudioToolbox.framework | `_AudioFileStreamClose` | 9 | firefox, floorp, librewolf, mullvad-browser |
| AudioToolbox.framework | `_AudioFileStreamGetProperty` | 9 | firefox, floorp, librewolf, mullvad-browser |
| AudioToolbox.framework | `_AudioFileStreamGetPropertyInfo` | 9 | firefox, floorp, librewolf, mullvad-browser |
| AudioToolbox.framework | `_AudioFileStreamOpen` | 9 | firefox, floorp, librewolf, mullvad-browser |
| AudioToolbox.framework | `_AudioFileStreamParseBytes` | 9 | firefox, floorp, librewolf, mullvad-browser |
| CoreMIDI.framework | `_MIDIPortDisconnectSource` | 9 | android-studio, cyberduck, dbeaver-community, godot |
| Accelerate.framework | `_vDSP_maxv` | 8 | android-studio, firefox, floorp, insomnia |
| Accelerate.framework | `_vDSP_measqv` | 8 | android-studio, firefox, floorp, insomnia |
| Accelerate.framework | `_vDSP_vsmsa` | 8 | android-studio, firefox, floorp, insomnia |
| CoreMIDI.framework | `_MIDIInputPortCreateWithBlock` | 8 | firefox, floorp, librewolf, mullvad-browser |
| CoreMIDI.framework | `_MIDIReceived` | 8 | firefox, floorp, hammerspoon, librewolf |
| AVFoundation.framework | `_AVCaptureSessionPresetLow` | 7 | firefox, floorp, librewolf, mullvad-browser |
| CoreAudio.framework | `_AudioDeviceDuck` | 7 | firefox, floorp, librewolf, mullvad-browser |
| CoreImage.framework | `_kCIContextCacheIntermediates` | 7 | betterdisplay, claude, cmux, figma |
| CoreImage.framework | `_kCIInputScaleKey` | 7 | betterdisplay, claude, raycast, telegram |
| CoreMIDI.framework | `_kMIDIPropertyName` | 7 | android-studio, cyberduck, dbeaver-community, hammerspoon |
| CoreMedia.framework | `_CMSampleBufferGetOutputDuration` | 7 | firefox, floorp, librewolf, mullvad-browser |
| CoreMedia.framework | `_CMTimebaseCreateWithMasterClock` | 7 | firefox, floorp, librewolf, mullvad-browser |
| VideoToolbox.framework | `_VTRegisterProfessionalVideoWorkflowVideoDecoders` | 7 | iina, krita, obs, rustdesk |
| VideoToolbox.framework | `_kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder` | 7 | firefox, floorp, librewolf, mullvad-browser |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetWriterInputPixelBufferAdaptor` | 6 | cmux, iterm2, openclaw, telegram |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableMetadataItem` | 6 | iterm2, localsend, shotcut, vorssaint |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerItemVideoOutput` | 6 | localsend, raycast, rustdesk, shotcut |
| CoreImage.framework | `_kCIInputBackgroundImageKey` | 6 | betterdisplay, cmux, figma, telegram |
| CoreImage.framework | `_kCIOutputImageKey` | 6 | cmux, iina, krita, obs |
| Vision.framework | `_OBJC_CLASS_$_VNRecognizedText` | 6 | clipy, maccy, openclaw, raycast |
| Vision.framework | `_OBJC_CLASS_$_VNRecognizedTextObservation` | 6 | clipy, maccy, openclaw, raycast |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMetadataItem` | 5 | iterm2, localsend, telegram, vorssaint |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableVideoComposition` | 5 | localsend, rustdesk, telegram, vorssaint |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableVideoCompositionInstruction` | 5 | localsend, rustdesk, telegram, vorssaint |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableVideoCompositionLayerInstruction` | 5 | localsend, rustdesk, telegram, vorssaint |
| AudioToolbox.framework | `_AUGraphNodeInfo` | 5 | deluge, hammerspoon, openttd, telegram |
| CoreImage.framework | `_kCIFormatRGBA8` | 5 | figma, openclaw, pearcleaner, telegram |
| CoreMIDI.framework | `_MIDIFlushOutput` | 5 | android-studio, cyberduck, dbeaver-community, hammerspoon |
| AVFoundation.framework | `_AVFileTypeWAVE` | 4 | localsend, raycast, shotcut, wireshark-app |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetExportSession` | 4 | localsend, openclaw, telegram, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetReaderAudioMixOutput` | 4 | telegram, vorssaint, whatsapp, zoom |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureStillImageOutput` | 4 | pencil, shotcut, webtorrent, wireshark-app |
| AudioToolbox.framework | `_AUGraphInitialize` | 4 | hammerspoon, openttd, telegram, vlc |
| AudioToolbox.framework | `_AUGraphOpen` | 4 | hammerspoon, openttd, telegram, vlc |
| CoreAudio.framework | `_AudioConvertNanosToHostTime` | 4 | lmms, pencil, shotcut, webtorrent |
| CoreImage.framework | `_kCIAttributeClass` | 4 | iina, krita, obs, shotcut |
| CoreImage.framework | `_kCIAttributeDefault` | 4 | iina, krita, obs, shotcut |
| CoreImage.framework | `_kCIAttributeSliderMax` | 4 | iina, krita, obs, shotcut |
| CoreImage.framework | `_kCIAttributeSliderMin` | 4 | iina, krita, obs, shotcut |
| CoreImage.framework | `_kCIAttributeType` | 4 | iina, krita, obs, shotcut |
| CoreImage.framework | `_kCICategoryGenerator` | 4 | iina, krita, obs, shotcut |
| CoreImage.framework | `_kCIContextWorkingColorSpace` | 4 | betterdisplay, pearcleaner, telegram, whatsapp |
| CoreImage.framework | `_kCIFormatARGB8` | 4 | iina, krita, obs, shotcut |
| CoreImage.framework | `_kCIImageColorSpace` | 4 | betterdisplay, claude, telegram, whatsapp |
| CoreImage.framework | `_kCIInputAspectRatioKey` | 4 | claude, telegram, vorssaint, whatsapp |
| CoreImage.framework | `_kCIInputMaskImageKey` | 4 | figma, telegram, vorssaint, whatsapp |
| AVFoundation.framework | `_AVCaptureSessionPresetMedium` | 3 | obs, telegram, vorssaint |
| AVFoundation.framework | `_AVErrorDeviceKey` | 3 | monitorcontrol, rustdesk, telegram |
| AVFoundation.framework | `_AVErrorFileSizeKey` | 3 | monitorcontrol, rustdesk, telegram |
| AVFoundation.framework | `_AVErrorMediaSubTypeKey` | 3 | monitorcontrol, rustdesk, telegram |
| AVFoundation.framework | `_AVErrorMediaTypeKey` | 3 | monitorcontrol, rustdesk, telegram |
| AVFoundation.framework | `_AVErrorPIDKey` | 3 | monitorcontrol, rustdesk, telegram |
| AVFoundation.framework | `_AVErrorRecordingSuccessfullyFinishedKey` | 3 | monitorcontrol, rustdesk, telegram |
| AVFoundation.framework | `_AVErrorTimeKey` | 3 | monitorcontrol, rustdesk, telegram |
| AVFoundation.framework | `_AVFileTypeMPEGLayer3` | 3 | localsend, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierArtist` | 3 | shotcut, telegram, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierArtwork` | 3 | shotcut, telegram, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierTitle` | 3 | shotcut, telegram, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyTitle` | 3 | localsend, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataLeadPerformer` | 3 | shotcut, telegram, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataTitleDescription` | 3 | shotcut, telegram, wireshark-app |
| AVFoundation.framework | `_AVMetadataKeySpaceID3` | 3 | hammerspoon, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataKeySpaceiTunes` | 3 | hammerspoon, shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyArtist` | 3 | shotcut, telegram, wireshark-app |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetReaderVideoCompositionOutput` | 3 | telegram, vorssaint, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureDeviceFormat` | 3 | obs, openclaw, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureVideoPreviewLayer` | 3 | telegram, vorssaint, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableComposition` | 3 | telegram, vorssaint, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableCompositionTrack` | 3 | telegram, vorssaint, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVVideoCompositionLayerInstruction` | 3 | telegram, vorssaint, whatsapp |
| Accelerate.framework | `_cblas_cgemm$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_cgesdd$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_cheevd$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_dgeev$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_dgeqrf$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_dgesdd$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_dgetrf$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_dorgqr$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_dpotrf$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_dsyevd$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_sgeev$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_sgesdd$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_sgetrf$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_spotrf$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_ssyevd$NEWLAPACK` | 3 | blender, ollama-app, openclaw |
| Accelerate.framework | `_vDSP_create_fftsetupD` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vDSP_ctozD` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vDSP_destroy_fftsetupD` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vDSP_fft_zriptD` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vDSP_vclrD` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vDSP_vsdivD` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vDSP_vspdp` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vDSP_ztocD` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vDSP_zvmagsD` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vImageBufferFill_ARGB8888` | 3 | android-studio, dbeaver-community, temurin |
| Accelerate.framework | `_vImageLookupTable_Planar8toPlanarF` | 3 | android-studio, dbeaver-community, temurin |
| Accelerate.framework | `_vvatan2` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vvatan2f` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vvlog` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vvlogf` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vvsincos` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vvsincosf` | 3 | iina, shotcut, spotify |
| Accelerate.framework | `_vvsqrt` | 3 | iina, shotcut, spotify |
| AudioToolbox.framework | `_AUGraphAddNode` | 3 | hammerspoon, telegram, vlc |
| AudioToolbox.framework | `_AUGraphConnectNodeInput` | 3 | hammerspoon, telegram, vlc |
| AudioToolbox.framework | `_AUGraphStart` | 3 | hammerspoon, telegram, vlc |
| AudioToolbox.framework | `_DisposeAUGraph` | 3 | hammerspoon, telegram, vlc |
| AudioToolbox.framework | `_DisposeMusicPlayer` | 3 | deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_DisposeMusicSequence` | 3 | deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicPlayerGetTime` | 3 | deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicPlayerPreroll` | 3 | deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicPlayerSetSequence` | 3 | deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicPlayerStart` | 3 | deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicPlayerStop` | 3 | deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicSequenceGetIndTrack` | 3 | deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicSequenceGetTrackCount` | 3 | deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_MusicTrackGetProperty` | 3 | deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_NewAUGraph` | 3 | hammerspoon, telegram, vlc |
| AudioToolbox.framework | `_NewMusicPlayer` | 3 | deluge, hammerspoon, openttd |
| AudioToolbox.framework | `_NewMusicSequence` | 3 | deluge, hammerspoon, openttd |
| CoreAudio.framework | `_AudioDeviceAddIOProc` | 3 | deluge, shotcut, zoom |
| CoreAudio.framework | `_AudioDeviceGetProperty` | 3 | deluge, lmms, shotcut |
| CoreAudio.framework | `_AudioDeviceRemoveIOProc` | 3 | deluge, shotcut, zoom |
| CoreAudio.framework | `_AudioDeviceSetProperty` | 3 | deluge, lmms, shotcut |
| CoreAudio.framework | `_AudioHardwareGetProperty` | 3 | deluge, lmms, shotcut |
| CoreImage.framework | `_kCIContextOutputColorSpace` | 3 | betterdisplay, telegram, whatsapp |
| CoreImage.framework | `_kCIFormatBGRA8` | 3 | cmux, telegram, whatsapp |
| CoreMIDI.framework | `_MIDIDestinationCreate` | 3 | deluge, hammerspoon, lmms |
| CoreMedia.framework | `_CMSampleBufferGetOutputSampleTimingInfoArray` | 3 | iina, obs, shotcut |
| CoreMedia.framework | `_CMTimeMakeFromDictionary` | 3 | monitorcontrol, rustdesk, telegram |
| CoreMedia.framework | `_CMTimeRangeGetUnion` | 3 | monitorcontrol, rustdesk, telegram |
| CoreMedia.framework | `_CMVideoFormatDescriptionGetCleanAperture` | 3 | pencil, vorssaint, webtorrent |
| CoreMediaIO.framework | `_CMIOObjectGetPropertyDataSize` | 3 | hammerspoon, obs, telegram |
| Speech.framework | `_OBJC_CLASS_$_SFSpeechRecognizer` | 3 | openclaw, raycast, whatsapp |
| AVFAudio.framework | `_OBJC_CLASS_$_AVAudioChannelLayout` | 2 | vorssaint, whatsapp |
| AVFAudio.framework | `_OBJC_CLASS_$_AVAudioRecorder` | 2 | openclaw, whatsapp |
| AVFoundation.framework | `_AVMediaCharacteristicLegible` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMediaTypeSubtitle` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyAlbumAndTrack` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyAuthor` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyDescription` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyGenre` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyPerformer` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyTitle` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierAlbumName` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierAuthor` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierCopyrights` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierCreationDate` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierDescription` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierLanguage` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierPublisher` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonIdentifierType` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyAlbumName` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyArtist` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyAuthor` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyCopyrights` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyCreationDate` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyDescription` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyLanguage` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyPublisher` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataCommonKeyType` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataISOUserDataKeyDate` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataAlbumTitle` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataAttachedPicture` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataComments` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataComposer` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataContentType` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataCopyright` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataDate` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataFileType` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataLanguage` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataLength` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataOfficialAudioSourceWebpage` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataPublisher` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataTrackNumber` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataAlbum` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataArtist` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataArtwork` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataAuthor` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataCameraFrameReadoutTime` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataComment` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataComposer` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataCopyright` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataCreationDate` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataDescription` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataGenre` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataPerformer` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataPublisher` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataTitle` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataVideoOrientation` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataAlbum` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataAlbumArtist` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataArtist` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataAuthor` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataComposer` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataCopyright` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataCoverArt` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataDescription` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataPerformer` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataPublisher` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataReleaseDate` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataSongName` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataTrackNumber` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataUserComment` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataUserGenre` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataKeySpaceQuickTimeUserData` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyAlbum` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyAuthor` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyComment` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyCopyright` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyCreationDate` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyDescription` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyGenre` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_AVSampleBufferAudioRendererWasFlushedAutomaticallyNotification` | 2 | iina, spotify |
| AVFoundation.framework | `_AVVideoAllowFrameReorderingKey` | 2 | vorssaint, zoom |
| AVFoundation.framework | `_AVVideoCleanApertureHeightKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoCleanApertureHorizontalOffsetKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoCleanApertureKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoCleanApertureVerticalOffsetKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoCleanApertureWidthKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoH264EntropyModeCABAC` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoH264EntropyModeKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoPixelAspectRatioHorizontalSpacingKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoPixelAspectRatioKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_AVVideoPixelAspectRatioVerticalSpacingKey` | 2 | telegram, zoom |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetWriterInputMetadataAdaptor` | 2 | localsend, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAudioMixInputParameters` | 2 | vorssaint, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureMovieFileOutput` | 2 | openclaw, zoom |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableAudioMix` | 2 | vorssaint, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableAudioMixInputParameters` | 2 | vorssaint, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerItemLegibleOutput` | 2 | shotcut, wireshark-app |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerItemOutput` | 2 | raycast, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerLooper` | 2 | cmux, whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVTimedMetadataGroup` | 2 | localsend, whatsapp |
| Accelerate.framework | `_BNNSFilterApplyTwoInput` | 2 | ollama-app, openclaw |
| Accelerate.framework | `_BNNSFilterCreateLayerBroadcastMatMul` | 2 | ollama-app, openclaw |
| Accelerate.framework | `_BNNSFilterDestroy` | 2 | ollama-app, openclaw |
| Accelerate.framework | `_cgeev$NEWLAPACK` | 2 | ollama-app, openclaw |
| Accelerate.framework | `_dgetri$NEWLAPACK` | 2 | ollama-app, openclaw |
| Accelerate.framework | `_dtrtri$NEWLAPACK` | 2 | ollama-app, openclaw |
| Accelerate.framework | `_sgeqrf$NEWLAPACK` | 2 | ollama-app, openclaw |
| Accelerate.framework | `_sgetri$NEWLAPACK` | 2 | ollama-app, openclaw |
| Accelerate.framework | `_sorgqr$NEWLAPACK` | 2 | ollama-app, openclaw |
| Accelerate.framework | `_strtri$NEWLAPACK` | 2 | ollama-app, openclaw |
| Accelerate.framework | `_vDSP_dotpr` | 2 | iina, spotify |
| Accelerate.framework | `_vDSP_vdpsp` | 2 | iina, shotcut |
| Accelerate.framework | `_vDSP_vflt16` | 2 | raycast, telegram |
| Accelerate.framework | `_vDSP_vrampmul` | 2 | spotify, vorssaint |
| Accelerate.framework | `_vDSP_vrampmul2` | 2 | spotify, vorssaint |
| Accelerate.framework | `_vvexp` | 2 | iina, shotcut |
| AudioToolbox.framework | `_AUGraphGetIndNode` | 2 | deluge, openttd |
| AudioToolbox.framework | `_AUGraphGetNodeCount` | 2 | deluge, openttd |
| AudioToolbox.framework | `_AUGraphStop` | 2 | telegram, vlc |
| AudioToolbox.framework | `_MusicPlayerSetTime` | 2 | deluge, hammerspoon |
| AudioToolbox.framework | `_MusicSequenceFileLoadData` | 2 | deluge, hammerspoon |
| AudioToolbox.framework | `_MusicSequenceGetAUGraph` | 2 | deluge, openttd |
| CoreAudio.framework | `_AudioHardwareGetPropertyInfo` | 2 | lmms, shotcut |
| CoreImage.framework | `_OBJC_CLASS_$_CIColorKernel` | 2 | telegram, whatsapp |
| CoreImage.framework | `_OBJC_CLASS_$_CIRenderDestination` | 2 | vorssaint, whatsapp |
| CoreImage.framework | `_kCIContextName` | 2 | betterdisplay, whatsapp |
| CoreImage.framework | `_kCIContextUseSoftwareRenderer` | 2 | telegram, whatsapp |
| CoreImage.framework | `_kCIImageApplyOrientationProperty` | 2 | vorssaint, whatsapp |
| CoreImage.framework | `_kCIInputColorKey` | 2 | betterdisplay, whatsapp |
| CoreImage.framework | `_kCIInputEVKey` | 2 | betterdisplay, whatsapp |
| CoreImage.framework | `_kCIInputExtentKey` | 2 | pearcleaner, whatsapp |
| CoreImage.framework | `_kCIInputIntensityKey` | 2 | betterdisplay, whatsapp |
| CoreImage.framework | `_kCIInputSaturationKey` | 2 | betterdisplay, whatsapp |
| CoreImage.framework | `_kCIInputSharpnessKey` | 2 | betterdisplay, vorssaint |
| CoreImage.framework | `_kCIInputTransformKey` | 2 | betterdisplay, whatsapp |
| CoreMIDI.framework | `_MIDIDeviceGetEntity` | 2 | hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIDeviceGetNumberOfEntities` | 2 | hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIEntityGetDestination` | 2 | hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIEntityGetNumberOfDestinations` | 2 | hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIEntityGetNumberOfSources` | 2 | hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIEntityGetSource` | 2 | hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIGetDevice` | 2 | hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIGetNumberOfDevices` | 2 | hammerspoon, lmms |
| CoreMIDI.framework | `_MIDIObjectFindByUniqueID` | 2 | hammerspoon, musescore |
| CoreMIDI.framework | `_kMIDIPropertyOffline` | 2 | hammerspoon, lmms |
| CoreMedia.framework | `_CMMemoryPoolCreate` | 2 | telegram, zed |
| CoreMedia.framework | `_CMMemoryPoolGetAllocator` | 2 | telegram, zed |
| CoreMedia.framework | `_CMMemoryPoolInvalidate` | 2 | telegram, zed |
| CoreMedia.framework | `_CMSimpleQueueDequeue` | 2 | obs, telegram |
| CoreMediaIO.framework | `_CMIODeviceStartStream` | 2 | obs, telegram |
| CoreMediaIO.framework | `_CMIODeviceStopStream` | 2 | obs, telegram |
| CoreMediaIO.framework | `_CMIOStreamCopyBufferQueue` | 2 | obs, telegram |
| MediaPlayer.framework | `_MPNowPlayingInfoPropertyMediaType` | 2 | iina, telegram |
| MediaPlayer.framework | `_OBJC_CLASS_$_MPChangePlaybackPositionCommandEvent` | 2 | iina, telegram |
| MediaPlayer.framework | `_OBJC_CLASS_$_MPRemoteCommand` | 2 | iina, telegram |
| ScreenCaptureKit.framework | `_SCStreamErrorDomain` | 2 | openclaw, vorssaint |
| Speech.framework | `_OBJC_CLASS_$_SFSpeechAudioBufferRecognitionRequest` | 2 | openclaw, raycast |
| Speech.framework | `_OBJC_CLASS_$_SFTranscriptionSegment` | 2 | openclaw, whatsapp |
| VideoToolbox.framework | `_VTCopyVideoEncoderList` | 2 | obs, zoom |
| VideoToolbox.framework | `_kVTDecompressionPropertyKey_RealTime` | 2 | android-studio, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Baseline_4_0` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Baseline_4_2` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Baseline_5_0` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Baseline_5_1` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Baseline_5_2` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_3_0` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_3_1` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_3_2` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_4_0` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_4_1` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_4_2` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_5_1` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_High_5_2` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Main_4_2` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Main_5_1` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTProfileLevel_H264_Main_5_2` | 2 | telegram, zed |
| VideoToolbox.framework | `_kVTVideoEncoderList_CodecType` | 2 | obs, zoom |
| Vision.framework | `_OBJC_CLASS_$_VNDetectHumanRectanglesRequest` | 2 | figma, whatsapp |
| libswiftAVFoundation.dylib | `_$sSo21AVAssetImageGeneratorC12AVFoundationE5image2atSo10CGImageRefaAD_So6CMTimea10actualTimetAI_tYaKF` | 2 | openclaw, vorssaint |
| libswiftAVFoundation.dylib | `_$sSo21AVAssetImageGeneratorC12AVFoundationE5image2atSo10CGImageRefaAD_So6CMTimea10actualTimetAI_tYaKFTu` | 2 | openclaw, vorssaint |
| libswiftVision.dylib | `_$sSo16VNRecognizedTextC6VisionE11boundingBox3forSo22VNRectangleObservationCSgSnySS5IndexVG_tKF` | 2 | raycast, vorssaint |
| AVFAudio.framework | `_AVEncoderBitDepthHintKey` | 1 | shotcut |
| AVFAudio.framework | `_OBJC_CLASS_$_AVAudioConnectionPoint` | 1 | whatsapp |
| AVFAudio.framework | `_OBJC_CLASS_$_AVAudioRoutingArbiter` | 1 | spotify |
| AVFoundation.framework | `_AVCaptureDeviceTypeDeskViewCamera` | 1 | obs |
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
| AVFoundation.framework | `_AVFileTypeAppleM4V` | 1 | localsend |
| AVFoundation.framework | `_AVFileTypeEnhancedAC3` | 1 | localsend |
| AVFoundation.framework | `_AVFileTypeProfileMPEG4AppleHLS` | 1 | whatsapp |
| AVFoundation.framework | `_AVFileTypeSunAU` | 1 | localsend |
| AVFoundation.framework | `_AVMediaCharacteristicAudible` | 1 | localsend |
| AVFoundation.framework | `_AVMetadataCommonKeyArtwork` | 1 | telegram |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataContentIdentifier` | 1 | vorssaint |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyTrackName` | 1 | telegram |
| AVFoundation.framework | `_AVMetadataiTunesMetadataKeyArtist` | 1 | telegram |
| AVFoundation.framework | `_AVMetadataiTunesMetadataKeySongName` | 1 | telegram |
| AVFoundation.framework | `_AVPlayerRateDidChangeNotification` | 1 | cmux |
| AVFoundation.framework | `_AVSampleBufferAudioRendererFlushTimeKey` | 1 | spotify |
| AVFoundation.framework | `_AVVideoCodecH264` | 1 | telegram |
| AVFoundation.framework | `_AVVideoProfileLevelH264High40` | 1 | telegram |
| AVFoundation.framework | `_AVVideoProfileLevelH264MainAutoLevel` | 1 | iterm2 |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetResourceLoadingDataRequest` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetTrackSegment` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAudioCompressedBuffer` | 1 | telegram |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureAudioChannel` | 1 | telegram |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureConnection` | 1 | telegram |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureInput` | 1 | vorssaint |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureMetadataOutput` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureMultiCamSession` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVComposition` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCompositionTrack` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCompositionTrackSegment` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMetadataFaceObject` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMetadataMachineReadableCodeObject` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerItemTrack` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVVideoComposition` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVVideoCompositionCoreAnimationTool` | 1 | whatsapp |
| AVFoundation.framework | `_OBJC_CLASS_$_AVVideoCompositionInstruction` | 1 | whatsapp |
| AVKit.framework | `_OBJC_CLASS_$_AVPictureInPictureControllerContentSource` | 1 | whatsapp |
| AVKit.framework | `_OBJC_CLASS_$_AVPictureInPictureVideoCallViewController` | 1 | whatsapp |
| AVKit.framework | `_OBJC_METACLASS_$_AVPictureInPictureVideoCallViewController` | 1 | whatsapp |
| Accelerate.framework | `_cblas_caxpy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_cdotc_sub$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_cdotu_sub$NEWLAPACK` | 1 | blender |
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
| Accelerate.framework | `_cblas_zgemm$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_zgemv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_zsyrk$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_ccopy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cgelsd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cgesv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cgetrf$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cpotrf$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dcopy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dgelsd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dgemm_` | 1 | gimp |
| Accelerate.framework | `_dgemv_` | 1 | gimp |
| Accelerate.framework | `_dger_` | 1 | gimp |
| Accelerate.framework | `_dgesv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dtrsm_` | 1 | gimp |
| Accelerate.framework | `_dtrsv_` | 1 | gimp |
| Accelerate.framework | `_kvImage_YpCbCrToARGBMatrix_ITU_R_601_4` | 1 | telegram |
| Accelerate.framework | `_kvImage_YpCbCrToARGBMatrix_ITU_R_709_2` | 1 | telegram |
| Accelerate.framework | `_scopy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_sgelsd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_sgesv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_vDSP_DFT_zrop_CreateSetup` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquad` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquad_CreateSetup` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquad_DestroySetup` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquad_SetCoefficientsDouble` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquadm` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquadm_CreateSetup` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquadm_DestroySetup` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquadm_ResetState` | 1 | spotify |
| Accelerate.framework | `_vDSP_biquadm_SetCoefficientsDouble` | 1 | spotify |
| Accelerate.framework | `_vDSP_desamp` | 1 | vorssaint |
| Accelerate.framework | `_vDSP_fft_zrop` | 1 | spotify |
| Accelerate.framework | `_vDSP_vrampmuladd` | 1 | vorssaint |
| Accelerate.framework | `_vDSP_vsmsma` | 1 | spotify |
| Accelerate.framework | `_vDSP_zvma` | 1 | spotify |
| Accelerate.framework | `_vImageAffineWarpCG_ARGB8888` | 1 | telegram |
| Accelerate.framework | `_vImageConvert_16Uto16F` | 1 | iina |
| Accelerate.framework | `_vImageConvert_420Yp8_Cb8_Cr8ToARGB8888` | 1 | telegram |
| Accelerate.framework | `_vImageConvert_AnyToAny` | 1 | telegram |
| Accelerate.framework | `_vImageConvert_YpCbCrToARGB_GenerateConversion` | 1 | telegram |
| Accelerate.framework | `_vImageConverter_CreateWithCGImageFormat` | 1 | telegram |
| Accelerate.framework | `_vImageConverter_MustOperateOutOfPlace` | 1 | telegram |
| Accelerate.framework | `_vImageConvolve_Planar8` | 1 | openclaw |
| Accelerate.framework | `_vImageHorizontalReflect_ARGB8888` | 1 | telegram |
| Accelerate.framework | `_vImagePermuteChannels_ARGB8888` | 1 | telegram |
| Accelerate.framework | `_vImagePremultiplyData_ARGB8888` | 1 | telegram |
| Accelerate.framework | `_vImagePremultiplyData_Planar8` | 1 | telegram |
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
| AudioToolbox.framework | `_CAClockDispose` | 1 | blender |
| AudioToolbox.framework | `_CAClockGetCurrentTime` | 1 | blender |
| AudioToolbox.framework | `_CAClockGetStartTime` | 1 | blender |
| AudioToolbox.framework | `_CAClockNew` | 1 | blender |
| AudioToolbox.framework | `_CAClockSetCurrentTime` | 1 | blender |
| AudioToolbox.framework | `_CAClockSetProperty` | 1 | blender |
| AudioToolbox.framework | `_CAClockStart` | 1 | blender |
| AudioToolbox.framework | `_CAClockStop` | 1 | blender |
| AudioToolbox.framework | `_DisposeMusicEventIterator` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicDeviceMIDIEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorDeleteEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorGetEventInfo` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorHasCurrentEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorHasNextEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorHasPreviousEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorNextEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorPreviousEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorSeek` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicEventIteratorSetEventTime` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicPlayerIsPlaying` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicSequenceDisposeTrack` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicSequenceFileCreateData` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicSequenceFileLoad` | 1 | openttd |
| AudioToolbox.framework | `_MusicSequenceGetSecondsForBeats` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicSequenceGetTempoTrack` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicSequenceGetTrackIndex` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicSequenceNewTrack` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicSequenceSetUserCallback` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicTrackGetSequence` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewAUPresetEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewExtendedNoteEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewExtendedTempoEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewMIDIChannelEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewMIDINoteEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewMIDIRawDataEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewMetaEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewParameterEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicTrackNewUserEvent` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicTrackSetDestMIDIEndpoint` | 1 | hammerspoon |
| AudioToolbox.framework | `_MusicTrackSetProperty` | 1 | hammerspoon |
| AudioToolbox.framework | `_NewMusicEventIterator` | 1 | hammerspoon |
| AudioToolbox.framework | `_OBJC_CLASS_$_AUAudioUnit` | 1 | whatsapp |
| AudioToolbox.framework | `_OBJC_CLASS_$_AUAudioUnitBus` | 1 | whatsapp |
| AudioToolbox.framework | `_OBJC_CLASS_$_AUAudioUnitBusArray` | 1 | whatsapp |
| AudioToolbox.framework | `_OBJC_CLASS_$_AUParameter` | 1 | whatsapp |
| AudioToolbox.framework | `_OBJC_CLASS_$_AUParameterTree` | 1 | whatsapp |
| AudioToolbox.framework | `_OBJC_METACLASS_$_AUAudioUnit` | 1 | whatsapp |
| AudioUnit.framework | `_MusicDeviceMIDIEvent` | 1 | vlc |
| CoreAudio.framework | `_AudioDeviceAddPropertyListener` | 1 | lmms |
| CoreAudio.framework | `_AudioDeviceGetPropertyInfo` | 1 | lmms |
| CoreAudio.framework | `_AudioDeviceRemovePropertyListener` | 1 | lmms |
| CoreAudio.framework | `_AudioStreamGetProperty` | 1 | lmms |
| CoreAudioKit.framework | `_OBJC_CLASS_$_AUGenericView` | 1 | audacity |
| CoreAudioKit.framework | `_OBJC_CLASS_$_AUPannerView` | 1 | audacity |
| CoreImage.framework | `_OBJC_CLASS_$_CIBlendKernel` | 1 | cmux |
| CoreImage.framework | `_OBJC_CLASS_$_CIFaceFeature` | 1 | whatsapp |
| CoreImage.framework | `_OBJC_CLASS_$_CIFeature` | 1 | whatsapp |
| CoreImage.framework | `_OBJC_CLASS_$_CIKernel` | 1 | whatsapp |
| CoreImage.framework | `_OBJC_CLASS_$_CIQRCodeDescriptor` | 1 | whatsapp |
| CoreImage.framework | `_OBJC_CLASS_$_CIQRCodeFeature` | 1 | whatsapp |
| CoreImage.framework | `_kCIContextHighQualityDownsample` | 1 | figma |
| CoreImage.framework | `_kCIContextOutputPremultiplied` | 1 | whatsapp |
| CoreImage.framework | `_kCIContextPriorityRequestLow` | 1 | raycast |
| CoreImage.framework | `_kCIContextWorkingFormat` | 1 | whatsapp |
| CoreImage.framework | `_kCIFormatRGBAf` | 1 | tailscale-app |
| CoreImage.framework | `_kCIFormatRGBAh` | 1 | whatsapp |
| CoreImage.framework | `_kCIImageAutoAdjustRedEye` | 1 | telegram |
| CoreImage.framework | `_kCIImageProperties` | 1 | whatsapp |
| CoreImage.framework | `_kCIImageToneMapHDRtoSDR` | 1 | whatsapp |
| CoreImage.framework | `_kCIInputAmountKey` | 1 | betterdisplay |
| CoreImage.framework | `_kCIInputAngleKey` | 1 | betterdisplay |
| CoreImage.framework | `_kCIInputBrightnessKey` | 1 | betterdisplay |
| CoreImage.framework | `_kCIInputCenterKey` | 1 | vorssaint |
| CoreImage.framework | `_kCIInputContrastKey` | 1 | betterdisplay |
| CoreImage.framework | `_kCIInputTargetImageKey` | 1 | telegram |
| CoreImage.framework | `_kCIInputWidthKey` | 1 | betterdisplay |
| CoreMIDI.framework | `_MIDIEndpointGetEntity` | 1 | lmms |
| CoreMIDI.framework | `_MIDIEntityGetDevice` | 1 | lmms |
| CoreMIDI.framework | `_MIDIObjectGetProperties` | 1 | hammerspoon |
| CoreMIDI.framework | `_MIDIObjectSetIntegerProperty` | 1 | hammerspoon |
| CoreMIDI.framework | `_MIDIObjectSetStringProperty` | 1 | hammerspoon |
| CoreMIDI.framework | `_MIDISourceCreate` | 1 | hammerspoon |
| CoreMIDI.framework | `_kMIDIPropertyModel` | 1 | hammerspoon |
| CoreMIDI.framework | `_kMIDIPropertyPrivate` | 1 | hammerspoon |
| CoreMedia.framework | `_CMAudioDeviceClockCreate` | 1 | telegram |
| CoreMedia.framework | `_CMAudioDeviceClockCreateFromAudioDeviceID` | 1 | telegram |
| CoreMedia.framework | `_CMAudioFormatDescriptionGetChannelLayout` | 1 | vorssaint |
| CoreMedia.framework | `_CMFormatDescriptionCreate` | 1 | obs |
| CoreMedia.framework | `_CMSampleBufferSetDataReady` | 1 | vorssaint |
| CoreMedia.framework | `_CMSimpleQueueCreate` | 1 | obs |
| CoreMedia.framework | `_CMSimpleQueueEnqueue` | 1 | obs |
| CoreMedia.framework | `_CMSimpleQueueGetCapacity` | 1 | obs |
| CoreMedia.framework | `_CMSimpleQueueGetCount` | 1 | obs |
| CoreMedia.framework | `_CMSyncGetTime` | 1 | telegram |
| CoreMedia.framework | `_CMTimebaseAddTimer` | 1 | telegram |
| CoreMedia.framework | `_CMTimebaseGetMaster` | 1 | telegram |
| CoreMedia.framework | `_CMTimebaseGetTimeWithTimeScale` | 1 | spotify |
| CoreMedia.framework | `_CMTimebaseRemoveTimer` | 1 | telegram |
| CoreMedia.framework | `_CMTimebaseSetRateAndAnchorTime` | 1 | telegram |
| CoreMedia.framework | `_CMTimebaseSetTimerNextFireTime` | 1 | telegram |
| CoreMedia.framework | `_kCMFormatDescriptionExtension_ContainsAlphaChannel` | 1 | vorssaint |
| CoreMedia.framework | `_kCMFormatDescriptionTransferFunction_ITU_R_709_2` | 1 | vorssaint |
| CoreMedia.framework | `_kCMSampleAttachmentKey_DependsOnOthers` | 1 | obs |
| CoreMedia.framework | `_kCMSampleBufferAttachmentKey_DroppedFrameReason` | 1 | telegram |
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
| CoreMediaIO.framework | `_CMIOObjectSetPropertyData` | 1 | obs |
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
| MediaPlayer.framework | `_MPNowPlayingInfoPropertyDefaultPlaybackRate` | 1 | iina |
| MediaPlayer.framework | `_MPNowPlayingInfoPropertyPlaybackQueueCount` | 1 | iina |
| MediaPlayer.framework | `_MPNowPlayingInfoPropertyPlaybackQueueIndex` | 1 | iina |
| MediaPlayer.framework | `_OBJC_CLASS_$_MPChangePlaybackRateCommandEvent` | 1 | iina |
| MediaPlayer.framework | `_OBJC_CLASS_$_MPSkipIntervalCommandEvent` | 1 | iina |
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
| VideoToolbox.framework | `_kVTCompressionPropertyKey_ConstantBitRate` | 1 | obs |
| VideoToolbox.framework | `_kVTDecompressionPropertyKey_DeinterlaceMode` | 1 | vlc |
| VideoToolbox.framework | `_kVTDecompressionPropertyKey_FieldMode` | 1 | vlc |
| VideoToolbox.framework | `_kVTDecompressionProperty_DeinterlaceMode_Temporal` | 1 | vlc |
| VideoToolbox.framework | `_kVTDecompressionProperty_FieldMode_DeinterlaceFields` | 1 | vlc |
| VideoToolbox.framework | `_kVTH264EntropyMode_CAVLC` | 1 | zoom |
| VideoToolbox.framework | `_kVTVideoEncoderList_DisplayName` | 1 | obs |
| VideoToolbox.framework | `_kVTVideoEncoderList_EncoderID` | 1 | obs |
| VideoToolbox.framework | `_kVTVideoEncoderList_IsHardwareAccelerated` | 1 | obs |
| VideoToolbox.framework | `_kVTVideoEncoderSpecification_EncoderID` | 1 | obs |
| Vision.framework | `_OBJC_CLASS_$_VNContour` | 1 | whatsapp |
| Vision.framework | `_OBJC_CLASS_$_VNContoursObservation` | 1 | whatsapp |
| Vision.framework | `_OBJC_CLASS_$_VNDetectContoursRequest` | 1 | whatsapp |
| Vision.framework | `_OBJC_CLASS_$_VNGenerateObjectnessBasedSaliencyImageRequest` | 1 | whatsapp |
| Vision.framework | `_OBJC_CLASS_$_VNHumanObservation` | 1 | whatsapp |
| Vision.framework | `_OBJC_CLASS_$_VNPixelBufferObservation` | 1 | figma |
| Vision.framework | `_OBJC_CLASS_$_VNSaliencyImageObservation` | 1 | whatsapp |
| Vision.framework | `_OBJC_CLASS_$_VNTrackObjectRequest` | 1 | whatsapp |
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
| libswiftAVFoundation.dylib | `_$s12AVFoundation22AVPartialAsyncPropertyCAASo7AVAssetCRbzlE033availableMediaCharacteristicsWithG16SelectionOptionsAA07AVAsyncD0CyxSaySo21AVMediaCharacteristicaGGvgZ` | 1 | cmux |
| libswiftAVFoundation.dylib | `_$s12AVFoundation22AVPartialAsyncPropertyCMn` | 1 | raycast |
| libswiftAVFoundation.dylib | `_$sSo12AVPlayerItemC12AVFoundationE5asset28automaticallyLoadedAssetKeysABSo7AVAssetC_SayAC22AVPartialAsyncPropertyCyAGGGtcfC` | 1 | raycast |
| libswiftAVFoundation.dylib | `_$sSo20AVAssetExportSessionC12AVFoundationE6export2to2as9isolationy10Foundation3URLV_So10AVFileTypeaScA_pSgYitYaKF` | 1 | openclaw |
| libswiftAVFoundation.dylib | `_$sSo20AVAssetExportSessionC12AVFoundationE6export2to2as9isolationy10Foundation3URLV_So10AVFileTypeaScA_pSgYitYaKFTu` | 1 | openclaw |
| libswiftAVFoundation.dylib | `_$sSo29AVAsynchronousKeyValueLoadingP12AVFoundationE4load_9isolationqd__AC15AVAsyncPropertyCyxqd__G_ScA_pSgYitYaKlF` | 1 | coteditor |
| libswiftAVFoundation.dylib | `_$sSo29AVAsynchronousKeyValueLoadingP12AVFoundationE4load_9isolationqd__AC15AVAsyncPropertyCyxqd__G_ScA_pSgYitYaKlFTu` | 1 | coteditor |
| libswiftAVFoundation.dylib | `_$sSo29AVAsynchronousKeyValueLoadingP12AVFoundationE4load___9isolationqd___qd_0_qd_1_qd_1_QptAC15AVAsyncPropertyCyxqd__G_AGyxqd_0_GAGyxqd_1_Gqd_1_QpScA_pSgYitYaKRvd_1_r1_lF` | 1 | coteditor |
| libswiftAVFoundation.dylib | `_$sSo29AVAsynchronousKeyValueLoadingP12AVFoundationE4load___9isolationqd___qd_0_qd_1_qd_1_QptAC15AVAsyncPropertyCyxqd__G_AGyxqd_0_GAGyxqd_1_Gqd_1_QpScA_pSgYitYaKRvd_1_r1_lFTu` | 1 | coteditor |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO10formWindow13usingSequence6result06isHalfD0yAC0dF0O_xzSbtAA0A13MutableBufferRzSf7ElementRtzlFZ` | 1 | raycast |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO14WindowSequenceO19hanningDenormalizedyA2EmFWC` | 1 | raycast |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO14WindowSequenceOMa` | 1 | raycast |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO3FFTC5log2n5radix6ofTypeAEy_xGSgSu_AC5RadixOxmtcfc` | 1 | raycast |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO3FFTC7forward5input6outputySo15DSPSplitComplexV_AJztFTj` | 1 | raycast |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO3FFTCMn` | 1 | raycast |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO5RadixO6radix2yA2EmFWC` | 1 | raycast |
| libswiftAccelerate.dylib | `_$s10Accelerate4vDSPO5RadixOMa` | 1 | raycast |
| libswiftAccelerate.dylib | `_$sSryxG10Accelerate0A13MutableBufferABMc` | 1 | raycast |
| libswiftCoreMedia.dylib | `_$sSo17CMSampleBufferRefa9CoreMediaE5FlagsV05audioB25ListAssure16ByteAlignmentAEvgZ` | 1 | raycast |
| libswiftCoreMediaIO.dylib | `_$sSo25CMIOExtensionStreamFormatC11CoreMediaIOE17formatDescription16maxFrameDuration03minjK005validJ9DurationsABSo08CMFormatH3Refa_So6CMTimeaAKSayAKGSgtcfC` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionDevicePropertiesC11CoreMediaIOE13transportTypeSiSgvs` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE13frameDurationSo6CMTimeaSgvs` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE17activeFormatIndexSiSgvg` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE17activeFormatIndexSiSgvs` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE19sinkBufferQueueSizeSiSgvs` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE29sinkBuffersRequiredForStartupSiSgvs` | 1 | obs |
