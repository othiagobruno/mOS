# Lacunas de Isabela (mídia)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Isabela (mídia) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**370** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreAudio.framework | `_AudioConvertHostTimeToNanos` | 5 | firefox, iina, obs, shotcut |
| CoreImage.framework | `_OBJC_CLASS_$_CIColor` | 5 | iina, iterm2, krita, obs |
| CoreImage.framework | `_OBJC_CLASS_$_CIContext` | 5 | iina, iterm2, krita, obs |
| CoreImage.framework | `_kCIInputImageKey` | 5 | iina, iterm2, krita, obs |
| CoreAudio.framework | `_AudioGetCurrentHostTime` | 4 | firefox, iina, musescore, thunderbird |
| CoreImage.framework | `_kCIAttributeClass` | 4 | iina, krita, obs, shotcut |
| CoreImage.framework | `_kCIAttributeDefault` | 4 | iina, krita, obs, shotcut |
| CoreImage.framework | `_kCIAttributeSliderMax` | 4 | iina, krita, obs, shotcut |
| CoreImage.framework | `_kCIAttributeSliderMin` | 4 | iina, krita, obs, shotcut |
| CoreImage.framework | `_kCIAttributeType` | 4 | iina, krita, obs, shotcut |
| CoreImage.framework | `_kCICategoryGenerator` | 4 | iina, krita, obs, shotcut |
| CoreImage.framework | `_kCIFormatARGB8` | 4 | iina, krita, obs, shotcut |
| CoreImage.framework | `_kCIOutputImageKey` | 4 | iina, krita, obs, shotcut |
| VideoToolbox.framework | `_VTRegisterProfessionalVideoWorkflowVideoDecoders` | 4 | iina, krita, obs, shotcut |
| AVFoundation.framework | `_AVCaptureSessionPresetLow` | 3 | firefox, obs, thunderbird |
| CoreMIDI.framework | `_MIDIInputPortCreateWithBlock` | 3 | firefox, musescore, thunderbird |
| CoreMIDI.framework | `_MIDIPortDispose` | 3 | firefox, musescore, thunderbird |
| CoreMedia.framework | `_CMSampleBufferGetOutputSampleTimingInfoArray` | 3 | iina, obs, shotcut |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMutableMetadataItem` | 2 | iterm2, shotcut |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerLayer` | 2 | libreoffice, shotcut |
| Accelerate.framework | `_vDSP_create_fftsetupD` | 2 | iina, shotcut |
| Accelerate.framework | `_vDSP_ctozD` | 2 | iina, shotcut |
| Accelerate.framework | `_vDSP_destroy_fftsetupD` | 2 | iina, shotcut |
| Accelerate.framework | `_vDSP_fft_zriptD` | 2 | iina, shotcut |
| Accelerate.framework | `_vDSP_maxv` | 2 | firefox, shotcut |
| Accelerate.framework | `_vDSP_measqv` | 2 | firefox, shotcut |
| Accelerate.framework | `_vDSP_vclrD` | 2 | iina, shotcut |
| Accelerate.framework | `_vDSP_vdpsp` | 2 | iina, shotcut |
| Accelerate.framework | `_vDSP_vsdivD` | 2 | iina, shotcut |
| Accelerate.framework | `_vDSP_vsmsa` | 2 | firefox, shotcut |
| Accelerate.framework | `_vDSP_vspdp` | 2 | iina, shotcut |
| Accelerate.framework | `_vDSP_ztocD` | 2 | iina, shotcut |
| Accelerate.framework | `_vDSP_zvmagsD` | 2 | iina, shotcut |
| Accelerate.framework | `_vvatan2` | 2 | iina, shotcut |
| Accelerate.framework | `_vvatan2f` | 2 | iina, shotcut |
| Accelerate.framework | `_vvexp` | 2 | iina, shotcut |
| Accelerate.framework | `_vvlog` | 2 | iina, shotcut |
| Accelerate.framework | `_vvlogf` | 2 | iina, shotcut |
| Accelerate.framework | `_vvsincos` | 2 | iina, shotcut |
| Accelerate.framework | `_vvsincosf` | 2 | iina, shotcut |
| Accelerate.framework | `_vvsqrt` | 2 | iina, shotcut |
| AudioToolbox.framework | `_AudioFileStreamClose` | 2 | firefox, thunderbird |
| AudioToolbox.framework | `_AudioFileStreamGetProperty` | 2 | firefox, thunderbird |
| AudioToolbox.framework | `_AudioFileStreamGetPropertyInfo` | 2 | firefox, thunderbird |
| AudioToolbox.framework | `_AudioFileStreamOpen` | 2 | firefox, thunderbird |
| AudioToolbox.framework | `_AudioFileStreamParseBytes` | 2 | firefox, thunderbird |
| CoreAudio.framework | `_AudioDeviceDuck` | 2 | firefox, thunderbird |
| CoreMIDI.framework | `_MIDIEndpointDispose` | 2 | firefox, thunderbird |
| CoreMIDI.framework | `_MIDIReceived` | 2 | firefox, thunderbird |
| CoreMedia.framework | `_CMSampleBufferGetOutputDuration` | 2 | firefox, thunderbird |
| CoreMedia.framework | `_CMTimebaseCreateWithMasterClock` | 2 | firefox, thunderbird |
| VideoToolbox.framework | `_kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder` | 2 | firefox, thunderbird |
| AVFAudio.framework | `_AVEncoderBitDepthHintKey` | 1 | shotcut |
| AVFoundation.framework | `_AVCaptureDeviceTypeDeskViewCamera` | 1 | obs |
| AVFoundation.framework | `_AVCaptureSessionPreset320x240` | 1 | obs |
| AVFoundation.framework | `_AVCaptureSessionPreset3840x2160` | 1 | obs |
| AVFoundation.framework | `_AVCaptureSessionPreset960x540` | 1 | obs |
| AVFoundation.framework | `_AVCaptureSessionPresetMedium` | 1 | obs |
| AVFoundation.framework | `_AVErrorDeviceKey` | 1 | monitorcontrol |
| AVFoundation.framework | `_AVErrorFileSizeKey` | 1 | monitorcontrol |
| AVFoundation.framework | `_AVErrorMediaSubTypeKey` | 1 | monitorcontrol |
| AVFoundation.framework | `_AVErrorMediaTypeKey` | 1 | monitorcontrol |
| AVFoundation.framework | `_AVErrorPIDKey` | 1 | monitorcontrol |
| AVFoundation.framework | `_AVErrorRecordingSuccessfullyFinishedKey` | 1 | monitorcontrol |
| AVFoundation.framework | `_AVErrorTimeKey` | 1 | monitorcontrol |
| AVFoundation.framework | `_AVFileTypeMPEGLayer3` | 1 | shotcut |
| AVFoundation.framework | `_AVFileTypeWAVE` | 1 | shotcut |
| AVFoundation.framework | `_AVMediaCharacteristicLegible` | 1 | shotcut |
| AVFoundation.framework | `_AVMediaTypeSubtitle` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyAlbumAndTrack` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyAuthor` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyDescription` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyGenre` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyPerformer` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadata3GPUserDataKeyTitle` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonIdentifierAlbumName` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonIdentifierArtist` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonIdentifierArtwork` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonIdentifierAuthor` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonIdentifierCopyrights` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonIdentifierCreationDate` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonIdentifierDescription` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonIdentifierLanguage` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonIdentifierPublisher` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonIdentifierTitle` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonIdentifierType` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonKeyAlbumName` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonKeyArtist` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonKeyAuthor` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonKeyCopyrights` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonKeyCreationDate` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonKeyDescription` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonKeyLanguage` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonKeyPublisher` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonKeyTitle` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataCommonKeyType` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataISOUserDataKeyDate` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataAlbumTitle` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataAttachedPicture` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataComments` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataComposer` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataContentType` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataCopyright` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataDate` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataFileType` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataLanguage` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataLeadPerformer` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataLength` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataOfficialAudioSourceWebpage` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataPublisher` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataTitleDescription` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierID3MetadataTrackNumber` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataAlbum` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataArtist` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataArtwork` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataAuthor` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataCameraFrameReadoutTime` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataComment` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataComposer` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataCopyright` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataCreationDate` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataDescription` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataGenre` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataPerformer` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataPublisher` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataTitle` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifierQuickTimeMetadataVideoOrientation` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataAlbum` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataAlbumArtist` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataArtist` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataAuthor` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataComposer` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataCopyright` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataCoverArt` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataDescription` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataPerformer` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataPublisher` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataReleaseDate` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataSongName` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataTrackNumber` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataUserComment` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataIdentifieriTunesMetadataUserGenre` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataKeySpaceID3` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataKeySpaceQuickTimeUserData` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataKeySpaceiTunes` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyAlbum` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyArtist` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyAuthor` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyComment` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyCopyright` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyCreationDate` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyDescription` | 1 | shotcut |
| AVFoundation.framework | `_AVMetadataQuickTimeUserDataKeyGenre` | 1 | shotcut |
| AVFoundation.framework | `_AVSampleBufferAudioRendererWasFlushedAutomaticallyNotification` | 1 | iina |
| AVFoundation.framework | `_AVVideoProfileLevelH264MainAutoLevel` | 1 | iterm2 |
| AVFoundation.framework | `_OBJC_CLASS_$_AVAssetWriterInputPixelBufferAdaptor` | 1 | iterm2 |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureDeviceFormat` | 1 | obs |
| AVFoundation.framework | `_OBJC_CLASS_$_AVCaptureStillImageOutput` | 1 | shotcut |
| AVFoundation.framework | `_OBJC_CLASS_$_AVMetadataItem` | 1 | iterm2 |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerItemLegibleOutput` | 1 | shotcut |
| AVFoundation.framework | `_OBJC_CLASS_$_AVPlayerItemVideoOutput` | 1 | shotcut |
| Accelerate.framework | `_cblas_caxpy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_cdotc_sub$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_cdotu_sub$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cblas_cgemm$NEWLAPACK` | 1 | blender |
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
| Accelerate.framework | `_cgesdd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cgesv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cgetrf$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cheevd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_cpotrf$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dcopy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dgeev$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dgelsd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dgemm_` | 1 | gimp |
| Accelerate.framework | `_dgemv_` | 1 | gimp |
| Accelerate.framework | `_dgeqrf$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dger_` | 1 | gimp |
| Accelerate.framework | `_dgesdd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dgesv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dgetrf$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dorgqr$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dpotrf$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dsyevd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_dtrsm_` | 1 | gimp |
| Accelerate.framework | `_dtrsv_` | 1 | gimp |
| Accelerate.framework | `_scopy$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_sgeev$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_sgelsd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_sgesdd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_sgesv$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_sgetrf$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_spotrf$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_ssyevd$NEWLAPACK` | 1 | blender |
| Accelerate.framework | `_vDSP_dotpr` | 1 | iina |
| Accelerate.framework | `_vImageConvert_16Uto16F` | 1 | iina |
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
| AudioToolbox.framework | `_AUGraphAddNode` | 1 | vlc |
| AudioToolbox.framework | `_AUGraphConnectNodeInput` | 1 | vlc |
| AudioToolbox.framework | `_AUGraphInitialize` | 1 | vlc |
| AudioToolbox.framework | `_AUGraphNodeInfo` | 1 | vlc |
| AudioToolbox.framework | `_AUGraphOpen` | 1 | vlc |
| AudioToolbox.framework | `_AUGraphStart` | 1 | vlc |
| AudioToolbox.framework | `_AUGraphStop` | 1 | vlc |
| AudioToolbox.framework | `_CAClockDispose` | 1 | blender |
| AudioToolbox.framework | `_CAClockGetCurrentTime` | 1 | blender |
| AudioToolbox.framework | `_CAClockGetStartTime` | 1 | blender |
| AudioToolbox.framework | `_CAClockNew` | 1 | blender |
| AudioToolbox.framework | `_CAClockSetCurrentTime` | 1 | blender |
| AudioToolbox.framework | `_CAClockSetProperty` | 1 | blender |
| AudioToolbox.framework | `_CAClockStart` | 1 | blender |
| AudioToolbox.framework | `_CAClockStop` | 1 | blender |
| AudioToolbox.framework | `_DisposeAUGraph` | 1 | vlc |
| AudioToolbox.framework | `_NewAUGraph` | 1 | vlc |
| AudioUnit.framework | `_MusicDeviceMIDIEvent` | 1 | vlc |
| CoreAudio.framework | `_AudioConvertNanosToHostTime` | 1 | shotcut |
| CoreAudio.framework | `_AudioDeviceAddIOProc` | 1 | shotcut |
| CoreAudio.framework | `_AudioDeviceGetProperty` | 1 | shotcut |
| CoreAudio.framework | `_AudioDeviceRemoveIOProc` | 1 | shotcut |
| CoreAudio.framework | `_AudioDeviceSetProperty` | 1 | shotcut |
| CoreAudio.framework | `_AudioHardwareGetProperty` | 1 | shotcut |
| CoreAudio.framework | `_AudioHardwareGetPropertyInfo` | 1 | shotcut |
| CoreMIDI.framework | `_MIDIObjectFindByUniqueID` | 1 | musescore |
| CoreMIDI.framework | `_MIDIPortDisconnectSource` | 1 | musescore |
| CoreMIDI.framework | `_kMIDIPropertyName` | 1 | musescore |
| CoreMedia.framework | `_CMFormatDescriptionCreate` | 1 | obs |
| CoreMedia.framework | `_CMSimpleQueueCreate` | 1 | obs |
| CoreMedia.framework | `_CMSimpleQueueDequeue` | 1 | obs |
| CoreMedia.framework | `_CMSimpleQueueEnqueue` | 1 | obs |
| CoreMedia.framework | `_CMSimpleQueueGetCapacity` | 1 | obs |
| CoreMedia.framework | `_CMSimpleQueueGetCount` | 1 | obs |
| CoreMedia.framework | `_CMTimeMakeFromDictionary` | 1 | monitorcontrol |
| CoreMedia.framework | `_CMTimeRangeGetUnion` | 1 | monitorcontrol |
| CoreMedia.framework | `_kCMSampleAttachmentKey_DependsOnOthers` | 1 | obs |
| CoreMediaIO.framework | `_CMIODeviceStartStream` | 1 | obs |
| CoreMediaIO.framework | `_CMIODeviceStopStream` | 1 | obs |
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
| CoreMediaIO.framework | `_CMIOObjectCreate` | 1 | obs |
| CoreMediaIO.framework | `_CMIOObjectGetPropertyDataSize` | 1 | obs |
| CoreMediaIO.framework | `_CMIOObjectSetPropertyData` | 1 | obs |
| CoreMediaIO.framework | `_CMIOObjectsPublishedAndDied` | 1 | obs |
| CoreMediaIO.framework | `_CMIOSampleBufferCreateForImageBuffer` | 1 | obs |
| CoreMediaIO.framework | `_CMIOStreamClockCreate` | 1 | obs |
| CoreMediaIO.framework | `_CMIOStreamClockInvalidate` | 1 | obs |
| CoreMediaIO.framework | `_CMIOStreamClockPostTimingEvent` | 1 | obs |
| CoreMediaIO.framework | `_CMIOStreamCopyBufferQueue` | 1 | obs |
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
| MediaPlayer.framework | `_MPNowPlayingInfoPropertyMediaType` | 1 | iina |
| MediaPlayer.framework | `_MPNowPlayingInfoPropertyPlaybackQueueCount` | 1 | iina |
| MediaPlayer.framework | `_MPNowPlayingInfoPropertyPlaybackQueueIndex` | 1 | iina |
| MediaPlayer.framework | `_OBJC_CLASS_$_MPChangePlaybackPositionCommandEvent` | 1 | iina |
| MediaPlayer.framework | `_OBJC_CLASS_$_MPChangePlaybackRateCommandEvent` | 1 | iina |
| MediaPlayer.framework | `_OBJC_CLASS_$_MPRemoteCommand` | 1 | iina |
| MediaPlayer.framework | `_OBJC_CLASS_$_MPSkipIntervalCommandEvent` | 1 | iina |
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
| VideoToolbox.framework | `_VTCopyVideoEncoderList` | 1 | obs |
| VideoToolbox.framework | `_kVTCompressionPropertyKey_ConstantBitRate` | 1 | obs |
| VideoToolbox.framework | `_kVTDecompressionPropertyKey_DeinterlaceMode` | 1 | vlc |
| VideoToolbox.framework | `_kVTDecompressionPropertyKey_FieldMode` | 1 | vlc |
| VideoToolbox.framework | `_kVTDecompressionProperty_DeinterlaceMode_Temporal` | 1 | vlc |
| VideoToolbox.framework | `_kVTDecompressionProperty_FieldMode_DeinterlaceFields` | 1 | vlc |
| VideoToolbox.framework | `_kVTVideoEncoderList_CodecType` | 1 | obs |
| VideoToolbox.framework | `_kVTVideoEncoderList_DisplayName` | 1 | obs |
| VideoToolbox.framework | `_kVTVideoEncoderList_EncoderID` | 1 | obs |
| VideoToolbox.framework | `_kVTVideoEncoderList_IsHardwareAccelerated` | 1 | obs |
| VideoToolbox.framework | `_kVTVideoEncoderSpecification_EncoderID` | 1 | obs |
| libswiftAVFoundation.dylib | `_$sSo29AVAsynchronousKeyValueLoadingP12AVFoundationE4load_9isolationqd__AC15AVAsyncPropertyCyxqd__G_ScA_pSgYitYaKlF` | 1 | coteditor |
| libswiftAVFoundation.dylib | `_$sSo29AVAsynchronousKeyValueLoadingP12AVFoundationE4load_9isolationqd__AC15AVAsyncPropertyCyxqd__G_ScA_pSgYitYaKlFTu` | 1 | coteditor |
| libswiftAVFoundation.dylib | `_$sSo29AVAsynchronousKeyValueLoadingP12AVFoundationE4load___9isolationqd___qd_0_qd_1_qd_1_QptAC15AVAsyncPropertyCyxqd__G_AGyxqd_0_GAGyxqd_1_Gqd_1_QpScA_pSgYitYaKRvd_1_r1_lF` | 1 | coteditor |
| libswiftAVFoundation.dylib | `_$sSo29AVAsynchronousKeyValueLoadingP12AVFoundationE4load___9isolationqd___qd_0_qd_1_qd_1_QptAC15AVAsyncPropertyCyxqd__G_AGyxqd_0_GAGyxqd_1_Gqd_1_QpScA_pSgYitYaKRvd_1_r1_lFTu` | 1 | coteditor |
| libswiftCoreMediaIO.dylib | `_$sSo25CMIOExtensionStreamFormatC11CoreMediaIOE17formatDescription16maxFrameDuration03minjK005validJ9DurationsABSo08CMFormatH3Refa_So6CMTimeaAKSayAKGSgtcfC` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionDevicePropertiesC11CoreMediaIOE13transportTypeSiSgvs` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE13frameDurationSo6CMTimeaSgvs` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE17activeFormatIndexSiSgvg` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE17activeFormatIndexSiSgvs` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE19sinkBufferQueueSizeSiSgvs` | 1 | obs |
| libswiftCoreMediaIO.dylib | `_$sSo29CMIOExtensionStreamPropertiesC11CoreMediaIOE29sinkBuffersRequiredForStartupSiSgvs` | 1 | obs |
