/*
 * mOS — UIKit.framework do Mac Catalyst (/System/iOSSupport/...), as constantes exportadas: nomes de notificação,
 * chaves, identificadores e valores numéricos com os tipos e valores do SDK da Apple (o WhatsApp e outros apps Catalyst
 * as importam). Ver UIKitCore.m e docs/uikit-catalyst.md.
 */
#import <Foundation/Foundation.h>

#define SN(N) NSString *const N = @#N;
#define S(N, V) NSString *const N = @V;

typedef struct { CGFloat top, left, bottom, right; } mos_UIEdgeInsets;
typedef struct { CGFloat top, leading, bottom, trailing; } mos_NSDirectionalEdgeInsets;
typedef struct { CGFloat horizontal, vertical; } mos_UIOffset;

/* ---- números */
const mos_UIEdgeInsets UIEdgeInsetsZero = {0, 0, 0, 0};
const mos_NSDirectionalEdgeInsets NSDirectionalEdgeInsetsZero = {0, 0, 0, 0};
const mos_UIOffset UIOffsetZero = {0, 0};
const CGSize UILayoutFittingCompressedSize = {0, 0};
const CGSize UILayoutFittingExpandedSize = {10000000, 10000000};
const CGSize UICollectionViewFlowLayoutAutomaticSize = {1.7976931348623157e308, 1.7976931348623157e308};
const CGFloat UITableViewAutomaticDimension = -1;
const CGFloat UIViewNoIntrinsicMetric = -1;
const CGFloat UIScrollViewDecelerationRateNormal = 0.998;
const CGFloat UIScrollViewDecelerationRateFast = 0.99;
const CGFloat UIWindowLevelNormal = 0;
const CGFloat UIWindowLevelStatusBar = 1000;
const CGFloat UIWindowLevelAlert = 2000;
const CGFloat UINavigationControllerHideShowBarDuration = 0.33;
const NSUInteger UIBackgroundTaskInvalid = 0;
/* UIUserInterfaceIdiom: Mac é 5 (Catalyst "Optimized for Mac"). Apps importam o símbolo. */
const NSInteger UIUserInterfaceIdiomUnspecified = -1, UIUserInterfaceIdiomPhone = 0, UIUserInterfaceIdiomPad = 1,
                UIUserInterfaceIdiomTV = 2, UIUserInterfaceIdiomCarPlay = 3, UIUserInterfaceIdiomMac = 5,
                UIUserInterfaceIdiomVision = 6;

const CGFloat UIFontWeightUltraLight = -0.8, UIFontWeightThin = -0.6, UIFontWeightLight = -0.4, UIFontWeightRegular = 0,
              UIFontWeightMedium = 0.23, UIFontWeightSemibold = 0.3, UIFontWeightBold = 0.4, UIFontWeightHeavy = 0.56,
              UIFontWeightBlack = 0.62;

/* UIAccessibilityTraits (uint64_t) */
const uint64_t UIAccessibilityTraitNone = 0, UIAccessibilityTraitButton = 1ull << 0, UIAccessibilityTraitLink = 1ull << 1,
               UIAccessibilityTraitImage = 1ull << 2, UIAccessibilityTraitSelected = 1ull << 3,
               UIAccessibilityTraitPlaysSound = 1ull << 4, UIAccessibilityTraitKeyboardKey = 1ull << 5,
               UIAccessibilityTraitStaticText = 1ull << 6, UIAccessibilityTraitSummaryElement = 1ull << 7,
               UIAccessibilityTraitNotEnabled = 1ull << 8, UIAccessibilityTraitUpdatesFrequently = 1ull << 9,
               UIAccessibilityTraitSearchField = 1ull << 10, UIAccessibilityTraitStartsMediaSession = 1ull << 11,
               UIAccessibilityTraitAdjustable = 1ull << 12, UIAccessibilityTraitAllowsDirectInteraction = 1ull << 13,
               UIAccessibilityTraitCausesPageTurn = 1ull << 14, UIAccessibilityTraitTabBar = 1ull << 15,
               UIAccessibilityTraitHeader = 1ull << 16;
/* UIAccessibilityNotifications (uint32_t) */
const uint32_t UIAccessibilityScreenChangedNotification = 1000, UIAccessibilityLayoutChangedNotification = 1001,
               UIAccessibilityAnnouncementNotification = 1008, UIAccessibilityPageScrolledNotification = 1009;

/* ---- UIApplication */
SN(UIApplicationDidFinishLaunchingNotification) SN(UIApplicationDidBecomeActiveNotification)
SN(UIApplicationWillResignActiveNotification) SN(UIApplicationDidEnterBackgroundNotification)
SN(UIApplicationWillEnterForegroundNotification) SN(UIApplicationWillTerminateNotification)
SN(UIApplicationDidReceiveMemoryWarningNotification) SN(UIApplicationSignificantTimeChangeNotification)
SN(UIApplicationUserDidTakeScreenshotNotification) SN(UIApplicationBackgroundRefreshStatusDidChangeNotification)
SN(UIApplicationProtectedDataDidBecomeAvailable) SN(UIApplicationProtectedDataWillBecomeUnavailable)
S(UIApplicationOpenSettingsURLString, "app-settings:")
S(UIApplicationOpenURLOptionUniversalLinksOnly, "UIApplicationOpenURLOptionUniversalLinksOnly")
S(UIApplicationOpenURLOptionsSourceApplicationKey, "UIApplicationOpenURLOptionsSourceApplicationKey")
S(UIApplicationOpenURLOptionsAnnotationKey, "UIApplicationOpenURLOptionsAnnotationKey")
S(UIApplicationOpenURLOptionsOpenInPlaceKey, "UIApplicationOpenURLOptionsOpenInPlaceKey")
S(UIApplicationOpenURLOptionsEventAttributionKey, "UIApplicationOpenURLOptionsEventAttributionKey")
S(UIApplicationLaunchOptionsURLKey, "UIApplicationLaunchOptionsURLKey")
S(UIApplicationLaunchOptionsSourceApplicationKey, "UIApplicationLaunchOptionsSourceApplicationKey")
S(UIApplicationLaunchOptionsRemoteNotificationKey, "UIApplicationLaunchOptionsRemoteNotificationKey")
S(UIApplicationLaunchOptionsAnnotationKey, "UIApplicationLaunchOptionsAnnotationKey")
S(UIApplicationLaunchOptionsLocationKey, "UIApplicationLaunchOptionsLocationKey")
S(UIApplicationLaunchOptionsNewsstandDownloadsKey, "UIApplicationLaunchOptionsNewsstandDownloadsKey")
S(UIApplicationLaunchOptionsBluetoothCentralsKey, "UIApplicationLaunchOptionsBluetoothCentralsKey")
S(UIApplicationLaunchOptionsBluetoothPeripheralsKey, "UIApplicationLaunchOptionsBluetoothPeripheralsKey")
S(UIApplicationLaunchOptionsShortcutItemKey, "UIApplicationLaunchOptionsShortcutItemKey")
S(UIApplicationLaunchOptionsUserActivityTypeKey, "UIApplicationLaunchOptionsUserActivityTypeKey")
S(UIApplicationLaunchOptionsCloudKitShareMetadataKey, "UIApplicationLaunchOptionsCloudKitShareMetadataKey")

/* ---- cenas e janelas */
SN(UISceneWillConnectNotification) SN(UISceneDidDisconnectNotification) SN(UISceneDidActivateNotification)
SN(UISceneWillDeactivateNotification) SN(UISceneWillEnterForegroundNotification) SN(UISceneDidEnterBackgroundNotification)
S(UIWindowSceneSessionRoleApplication, "UIWindowSceneSessionRoleApplication")
SN(UIWindowDidBecomeVisibleNotification) SN(UIWindowDidBecomeHiddenNotification) SN(UIWindowDidBecomeKeyNotification)
SN(UIWindowDidResignKeyNotification)
SN(UIScreenDidConnectNotification) SN(UIScreenDidDisconnectNotification) SN(UIScreenModeDidChangeNotification)
SN(UIScreenBrightnessDidChangeNotification) SN(UIScreenCapturedDidChangeNotification)
SN(UIDeviceOrientationDidChangeNotification) SN(UIDeviceBatteryStateDidChangeNotification)
SN(UIDeviceBatteryLevelDidChangeNotification) SN(UIDeviceProximityStateDidChangeNotification)

/* ---- teclado e texto */
SN(UIKeyboardWillShowNotification) SN(UIKeyboardDidShowNotification) SN(UIKeyboardWillHideNotification)
SN(UIKeyboardDidHideNotification) SN(UIKeyboardWillChangeFrameNotification) SN(UIKeyboardDidChangeFrameNotification)
S(UIKeyboardFrameBeginUserInfoKey, "UIKeyboardFrameBeginUserInfoKey") S(UIKeyboardFrameEndUserInfoKey, "UIKeyboardFrameEndUserInfoKey")
S(UIKeyboardAnimationDurationUserInfoKey, "UIKeyboardAnimationDurationUserInfoKey")
S(UIKeyboardAnimationCurveUserInfoKey, "UIKeyboardAnimationCurveUserInfoKey")
S(UIKeyboardIsLocalUserInfoKey, "UIKeyboardIsLocalUserInfoKey")
S(UIKeyInputUpArrow, "UIKeyInputUpArrow") S(UIKeyInputDownArrow, "UIKeyInputDownArrow")
S(UIKeyInputLeftArrow, "UIKeyInputLeftArrow") S(UIKeyInputRightArrow, "UIKeyInputRightArrow")
S(UIKeyInputEscape, "UIKeyInputEscape") S(UIKeyInputDelete, "\b") S(UIKeyInputEnd, "UIKeyInputEnd")
SN(UITextFieldTextDidBeginEditingNotification) SN(UITextFieldTextDidEndEditingNotification) SN(UITextFieldTextDidChangeNotification)
SN(UITextViewTextDidBeginEditingNotification) SN(UITextViewTextDidEndEditingNotification) SN(UITextViewTextDidChangeNotification)
SN(UITextInputCurrentInputModeDidChangeNotification)
S(UITrackingRunLoopMode, "UITrackingRunLoopMode")
S(UITextContentTypeName, "name") S(UITextContentTypeNamePrefix, "honorific-prefix") S(UITextContentTypeGivenName, "given-name")
S(UITextContentTypeMiddleName, "additional-name") S(UITextContentTypeFamilyName, "family-name")
S(UITextContentTypeNameSuffix, "honorific-suffix") S(UITextContentTypeFullStreetAddress, "street-address")
S(UITextContentTypeStreetAddressLine1, "address-line1") S(UITextContentTypeStreetAddressLine2, "address-line2")
S(UITextContentTypeAddressCity, "address-level2") S(UITextContentTypeAddressState, "address-level1")
S(UITextContentTypeCountryName, "country-name") S(UITextContentTypePostalCode, "postal-code")
S(UITextContentTypeTelephoneNumber, "tel") S(UITextContentTypeEmailAddress, "email") S(UITextContentTypeURL, "url")
S(UITextContentTypeCreditCardNumber, "cc-number") S(UITextContentTypeUsername, "username")
S(UITextContentTypePassword, "password") S(UITextContentTypeNewPassword, "new-password")
S(UITextContentTypeOneTimeCode, "one-time-code") S(UITextContentTypeDateTime, "date-time")

/* ---- fontes */
S(UIFontTextStyleLargeTitle, "UICTFontTextStyleTitle0") S(UIFontTextStyleTitle1, "UICTFontTextStyleTitle1")
S(UIFontTextStyleTitle2, "UICTFontTextStyleTitle2") S(UIFontTextStyleTitle3, "UICTFontTextStyleTitle3")
S(UIFontTextStyleHeadline, "UICTFontTextStyleHeadline") S(UIFontTextStyleSubheadline, "UICTFontTextStyleSubhead")
S(UIFontTextStyleBody, "UICTFontTextStyleBody") S(UIFontTextStyleCallout, "UICTFontTextStyleCallout")
S(UIFontTextStyleFootnote, "UICTFontTextStyleFootnote") S(UIFontTextStyleCaption1, "UICTFontTextStyleCaption1")
S(UIFontTextStyleCaption2, "UICTFontTextStyleCaption2")
S(UIFontDescriptorFamilyAttribute, "NSFontFamilyAttribute") S(UIFontDescriptorNameAttribute, "NSFontNameAttribute")
S(UIFontDescriptorSizeAttribute, "NSFontSizeAttribute") S(UIFontDescriptorTraitsAttribute, "NSCTFontTraitsAttribute")
S(UIFontDescriptorFeatureSettingsAttribute, "NSCTFontFeatureSettingsAttribute")
S(UIFontWeightTrait, "NSCTFontWeightTrait") S(UIFontSymbolicTrait, "NSCTFontSymbolicTrait")
S(UIFontDescriptorSystemDesignRounded, "NSCTFontUIFontDesignRounded")
S(UIFontFeatureTypeIdentifierKey, "CTFeatureTypeIdentifier") S(UIFontFeatureSelectorIdentifierKey, "CTFeatureSelectorIdentifier")
S(UIFontFeatureTypeIdentifierKey_ForNewSwiftAPI, "CTFeatureTypeIdentifier")
S(UIFontFeatureSelectorIdentifierKey_ForNewSwiftAPI, "CTFeatureSelectorIdentifier")
S(UIContentSizeCategoryUnspecified, "_UICTContentSizeCategoryUnspecified")
S(UIContentSizeCategoryExtraSmall, "UICTContentSizeCategoryXS") S(UIContentSizeCategorySmall, "UICTContentSizeCategoryS")
S(UIContentSizeCategoryMedium, "UICTContentSizeCategoryM") S(UIContentSizeCategoryLarge, "UICTContentSizeCategoryL")
S(UIContentSizeCategoryExtraLarge, "UICTContentSizeCategoryXL") S(UIContentSizeCategoryExtraExtraLarge, "UICTContentSizeCategoryXXL")
S(UIContentSizeCategoryExtraExtraExtraLarge, "UICTContentSizeCategoryXXXL")
S(UIContentSizeCategoryAccessibilityMedium, "UICTContentSizeCategoryAccessibilityM")
S(UIContentSizeCategoryAccessibilityLarge, "UICTContentSizeCategoryAccessibilityL")
S(UIContentSizeCategoryAccessibilityExtraLarge, "UICTContentSizeCategoryAccessibilityXL")
S(UIContentSizeCategoryAccessibilityExtraExtraLarge, "UICTContentSizeCategoryAccessibilityXXL")
S(UIContentSizeCategoryAccessibilityExtraExtraExtraLarge, "UICTContentSizeCategoryAccessibilityXXXL")
SN(UIContentSizeCategoryDidChangeNotification) S(UIContentSizeCategoryNewValueKey, "UIContentSizeCategoryNewValueKey")

/* ---- texto atribuído (no Catalyst essas chaves vêm do UIKit) */
S(NSAttachmentAttributeName, "NSAttachment") S(NSStrokeWidthAttributeName, "NSStrokeWidth")
S(NSStrokeColorAttributeName, "NSStrokeColor") S(NSKernAttributeName, "NSKern")
S(NSBaselineOffsetAttributeName, "NSBaselineOffset") S(NSLigatureAttributeName, "NSLigature")
S(NSObliquenessAttributeName, "NSObliqueness") S(NSStrikethroughColorAttributeName, "NSStrikethroughColor")
S(NSTrackingAttributeName, "NSTracking")
S(NSCharacterEncodingDocumentOption, "CharacterEncoding") S(NSDocumentTypeDocumentOption, "DocumentType")
S(NSRTFTextDocumentType, "NSRTF")
SN(NSTextStorageWillProcessEditingNotification) SN(NSTextStorageDidProcessEditingNotification)

/* ---- acessibilidade */
SN(UIAccessibilityAnnouncementDidFinishNotification) SN(UIAccessibilityElementFocusedNotification)
SN(UIAccessibilityVoiceOverStatusDidChangeNotification) SN(UIAccessibilityReduceMotionStatusDidChangeNotification)
SN(UIAccessibilityReduceTransparencyStatusDidChangeNotification) SN(UIAccessibilityBoldTextStatusDidChangeNotification)
SN(UIAccessibilityDarkerSystemColorsStatusDidChangeNotification) SN(UIAccessibilitySwitchControlStatusDidChangeNotification)
SN(UIAccessibilityVideoAutoplayStatusDidChangeNotification)
S(UIAccessibilityAnnouncementKeyStringValue, "UIAccessibilityAnnouncementKeyStringValue")
S(UIAccessibilityAnnouncementKeyWasSuccessful, "UIAccessibilityAnnouncementKeyWasSuccessful")
S(UIAccessibilityFocusedElementKey, "UIAccessibilityFocusedElementKey")
S(UIAccessibilityUnfocusedElementKey, "UIAccessibilityUnfocusedElementKey")
S(UIAccessibilityNotificationVoiceOverIdentifier, "UIAccessibilityNotificationVoiceOverIdentifier")
S(UIAccessibilitySpeechAttributeLanguage, "accessibilitySpeechLanguage")
S(UIAccessibilitySpeechAttributeIPANotation, "accessibilitySpeechIPANotation")
S(UIAccessibilitySpeechAttributeSpellOut, "accessibilitySpeechSpellOut")
S(UIAccessibilitySpeechAttributeQueueAnnouncement, "accessibilitySpeechQueueAnnouncement")

/* ---- menus (UIMenuIdentifier) */
S(UIMenuFile, "com.apple.menu.file") S(UIMenuEdit, "com.apple.menu.edit") S(UIMenuView, "com.apple.menu.view")
S(UIMenuHelp, "com.apple.menu.help") S(UIMenuFormat, "com.apple.menu.format") S(UIMenuFind, "com.apple.menu.find")
S(UIMenuReplace, "com.apple.menu.replace") S(UIMenuShare, "com.apple.menu.share") S(UIMenuStandardEdit, "com.apple.menu.standard-edit")
S(UIMenuSpelling, "com.apple.menu.spelling") S(UIMenuSpellingPanel, "com.apple.menu.spelling-panel")
S(UIMenuSpellingOptions, "com.apple.menu.spelling-options") S(UIMenuSubstitutionOptions, "com.apple.menu.substitutions-options")
S(UIMenuSpeech, "com.apple.menu.speech") S(UIMenuSidebar, "com.apple.menu.sidebar") S(UIMenuToolbar, "com.apple.menu.toolbar")
S(UIMenuServices, "com.apple.menu.services") S(UIMenuNewScene, "com.apple.menu.new-scene")
S(UIMenuPreferences, "com.apple.menu.preferences")
SN(UIMenuControllerWillShowMenuNotification) SN(UIMenuControllerWillHideMenuNotification)
SN(UIMenuControllerDidHideMenuNotification)

/* ---- atividades, pasteboard, picker, transições, coleções, folhas */
S(UIActivityTypeMail, "com.apple.UIKit.activity.Mail") S(UIActivityTypeMessage, "com.apple.UIKit.activity.Message")
S(UIActivityTypePrint, "com.apple.UIKit.activity.Print") S(UIActivityTypeCopyToPasteboard, "com.apple.UIKit.activity.CopyToPasteboard")
S(UIActivityTypeAssignToContact, "com.apple.UIKit.activity.AssignToContact")
S(UIActivityTypeSaveToCameraRoll, "com.apple.UIKit.activity.SaveToCameraRoll")
S(UIActivityTypeAddToReadingList, "com.apple.UIKit.activity.AddToReadingList")
S(UIActivityTypeOpenInIBooks, "com.apple.UIKit.activity.OpenInIBooks")
S(UIActivityTypePostToFacebook, "com.apple.UIKit.activity.PostToFacebook")
S(UIActivityTypePostToTwitter, "com.apple.UIKit.activity.PostToTwitter")
S(UIActivityTypePostToWeibo, "com.apple.UIKit.activity.PostToWeibo")
S(UIActivityTypePostToTencentWeibo, "com.apple.UIKit.activity.TencentWeibo")
S(UIActivityTypePostToVimeo, "com.apple.UIKit.activity.PostToVimeo")
S(UIActivityTypePostToFlickr, "com.apple.UIKit.activity.PostToFlickr")
S(UIPasteboardOptionExpirationDate, "UIPasteboardOptionExpirationDate")
S(UIPasteboardOptionLocalOnly, "UIPasteboardOptionLocalOnly")
S(UIPasteboardTypeAutomatic, "com.apple.uikit.pasteboard-type.automatic")
NSArray *UIPasteboardTypeListString;   /* NSArray<NSString *> *const na Apple: preenchida no carregamento */
S(UIImagePickerControllerOriginalImage, "UIImagePickerControllerOriginalImage")
S(UIImagePickerControllerEditedImage, "UIImagePickerControllerEditedImage")
S(UIImagePickerControllerImageURL, "UIImagePickerControllerImageURL")
S(UIImagePickerControllerPHAsset, "UIImagePickerControllerPHAsset")
S(UITransitionContextFromViewControllerKey, "UITransitionContextFromViewControllerKey")
S(UITransitionContextToViewControllerKey, "UITransitionContextToViewControllerKey")
S(UITransitionContextFromViewKey, "UITransitionContextFromViewKey")
S(UITransitionContextToViewKey, "UITransitionContextToViewKey")
S(UICollectionElementKindSectionHeader, "UICollectionElementKindSectionHeader")
S(UICollectionElementKindSectionFooter, "UICollectionElementKindSectionFooter")
S(UISheetPresentationControllerDetentIdentifierMedium, "com.apple.UIKit.medium")
S(UISheetPresentationControllerDetentIdentifierLarge, "com.apple.UIKit.large")
S(UIPageViewControllerOptionInterPageSpacingKey, "UIPageViewControllerOptionInterPageSpacingKey")

/* chamada pelo UIKitCore.m (+[UIApplication initialize] e UIApplicationMain): o runtime ObjC já está de pé */
void _mos_uikit_constants_init(void) {
    if (UIPasteboardTypeListString) return;
    UIPasteboardTypeListString = [[NSArray alloc] initWithObjects:@"public.utf8-plain-text", @"public.text", nil];
}
