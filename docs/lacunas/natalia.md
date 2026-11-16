# Lacunas de Natalia (serviços e segurança)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Natalia (serviços e segurança) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**264** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| Security.framework | `_kSecAttrServer` | 6 | iina, musescore, obs, qbittorrent |
| CoreServices.framework | `_GetIconRef` | 5 | krita, musescore, obs, qbittorrent |
| CoreServices.framework | `_ReleaseIconRef` | 5 | krita, musescore, obs, qbittorrent |
| CoreServices.framework | `_UCCreateCollator` | 5 | inkscape, musescore, obs, qbittorrent |
| CoreServices.framework | `_kUTTagClassNSPboardType` | 5 | firefox, musescore, obs, qbittorrent |
| CoreServices.framework | `_kUTTypeFileURL` | 5 | firefox, krita, musescore, shotcut |
| Security.framework | `_kSecAttrProtocol` | 5 | musescore, obs, qbittorrent, shotcut |
| CoreServices.framework | `_FSCopyObjectSync` | 4 | iterm2, macdown, transmission, vlc |
| CoreServices.framework | `_FSGetCatalogInfo` | 4 | iterm2, macdown, transmission, vlc |
| CoreServices.framework | `_FSPathMakeRefWithOptions` | 4 | iterm2, macdown, transmission, vlc |
| CoreServices.framework | `_LocaleRefFromLocaleString` | 4 | musescore, obs, qbittorrent, shotcut |
| CoreServices.framework | `_UCCompareText` | 4 | musescore, obs, qbittorrent, shotcut |
| CoreServices.framework | `_UCDisposeCollator` | 4 | musescore, obs, qbittorrent, shotcut |
| CoreServices.framework | `_UCGetCollationKey` | 4 | inkscape, musescore, obs, shotcut |
| Security.framework | `_SSLSetDiffieHellmanParams` | 4 | musescore, qbittorrent, shotcut, vlc |
| Security.framework | `_SecIdentityCreate` | 4 | iina, krita, obs, shotcut |
| Security.framework | `_kSecAttrProtocolFTPProxy` | 4 | musescore, obs, qbittorrent, shotcut |
| Security.framework | `_kSecAttrProtocolHTTPProxy` | 4 | musescore, obs, qbittorrent, shotcut |
| Security.framework | `_kSecAttrProtocolHTTPSProxy` | 4 | musescore, obs, qbittorrent, shotcut |
| Security.framework | `_kSecPolicyClient` | 4 | krita, musescore, qbittorrent, shotcut |
| CoreServices.framework | `_UCCompareCollationKeys` | 3 | musescore, obs, shotcut |
| CoreServices.framework | `_kMDItemTextContent` | 3 | gimp, inkscape, libreoffice |
| CoreServices.framework | `_kUTTypeTIFF` | 3 | krita, libreoffice, musescore |
| Security.framework | `_kSecImportExportKeychain` | 3 | musescore, qbittorrent, shotcut |
| Security.framework | `_kSecImportItemCertChain` | 3 | musescore, qbittorrent, shotcut |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV11applicationACvgZ` | 3 | alt-tab, coteditor, iterm2 |
| AddressBook.framework | `_kABAddressProperty` | 2 | libreoffice, thunderbird |
| AddressBook.framework | `_kABEmailProperty` | 2 | libreoffice, thunderbird |
| AddressBook.framework | `_kABFirstNameProperty` | 2 | libreoffice, thunderbird |
| AddressBook.framework | `_kABGroupNameProperty` | 2 | libreoffice, thunderbird |
| AddressBook.framework | `_kABLastNameProperty` | 2 | libreoffice, thunderbird |
| AddressBook.framework | `_kABModificationDateProperty` | 2 | libreoffice, thunderbird |
| AddressBook.framework | `_kABOrganizationProperty` | 2 | libreoffice, thunderbird |
| AddressBook.framework | `_kABPhoneProperty` | 2 | libreoffice, thunderbird |
| AuthenticationServices.framework | `_OBJC_CLASS_$_ASWebAuthenticationSessionRequest` | 2 | firefox, thunderbird |
| CoreServices.framework | `_FSCompareFSRefs` | 2 | firefox, thunderbird |
| CoreServices.framework | `_FSFindFolder` | 2 | firefox, thunderbird |
| CoreServices.framework | `_FSRefMakePath` | 2 | firefox, thunderbird |
| CoreServices.framework | `_LSCopyApplicationForMIMEType` | 2 | firefox, thunderbird |
| CoreServices.framework | `_LSCopyItemAttribute` | 2 | firefox, thunderbird |
| CoreServices.framework | `_LSGetApplicationForInfo` | 2 | firefox, thunderbird |
| CoreServices.framework | `_LSOpenFromRefSpec` | 2 | firefox, thunderbird |
| CoreServices.framework | `_kLSItemDisplayName` | 2 | firefox, thunderbird |
| CoreServices.framework | `_kMDItemAuthors` | 2 | krita, libreoffice |
| CoreServices.framework | `_kMDItemTitle` | 2 | krita, libreoffice |
| CoreServices.framework | `_kUTTypeHTML` | 2 | firefox, transmission |
| CryptoKit.framework | `_$s9CryptoKit12HashFunctionP6update13bufferPointerySW_tFTj` | 2 | iina, iterm2 |
| CryptoKit.framework | `_$s9CryptoKit12HashFunctionP8finalize6DigestQzyFTj` | 2 | iina, iterm2 |
| CryptoKit.framework | `_$s9CryptoKit12HashFunctionPxycfCTj` | 2 | iina, iterm2 |
| Security.framework | `_SecCertificateCopyPublicKey` | 2 | firefox, thunderbird |
| Security.framework | `_kSecAttrPort` | 2 | iina, vlc |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers10UTTagClassV17filenameExtensionACvgZ` | 2 | coteditor, iina |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV12propertyListACvgZ` | 2 | alt-tab, coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV14unixExecutableACvgZ` | 2 | coteditor, iterm2 |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV4jsonACvgZ` | 2 | coteditor, rectangle |
| AddressBook.framework | `_ABCopyArrayOfAllGroups` | 1 | libreoffice |
| AddressBook.framework | `_ABCopyArrayOfAllPeople` | 1 | libreoffice |
| AddressBook.framework | `_ABCopyArrayOfPropertiesForRecordType` | 1 | libreoffice |
| AddressBook.framework | `_ABCopyLocalizedPropertyOrLabel` | 1 | libreoffice |
| AddressBook.framework | `_ABGetSharedAddressBook` | 1 | libreoffice |
| AddressBook.framework | `_ABGroupCopyArrayOfAllMembers` | 1 | libreoffice |
| AddressBook.framework | `_ABMultiValueCopyLabelAtIndex` | 1 | libreoffice |
| AddressBook.framework | `_ABMultiValueCopyValueAtIndex` | 1 | libreoffice |
| AddressBook.framework | `_ABMultiValueCount` | 1 | libreoffice |
| AddressBook.framework | `_ABMultiValuePropertyType` | 1 | libreoffice |
| AddressBook.framework | `_ABRecordCopyValue` | 1 | libreoffice |
| AddressBook.framework | `_ABTypeOfProperty` | 1 | libreoffice |
| AddressBook.framework | `_OBJC_CLASS_$_ABAddressBook` | 1 | thunderbird |
| AddressBook.framework | `_OBJC_CLASS_$_ABGroup` | 1 | thunderbird |
| AddressBook.framework | `_kABAddressCityKey` | 1 | thunderbird |
| AddressBook.framework | `_kABAddressCountryKey` | 1 | thunderbird |
| AddressBook.framework | `_kABAddressHomeLabel` | 1 | thunderbird |
| AddressBook.framework | `_kABAddressStateKey` | 1 | thunderbird |
| AddressBook.framework | `_kABAddressStreetKey` | 1 | thunderbird |
| AddressBook.framework | `_kABAddressWorkLabel` | 1 | thunderbird |
| AddressBook.framework | `_kABAddressZIPKey` | 1 | thunderbird |
| AddressBook.framework | `_kABAnniversaryLabel` | 1 | thunderbird |
| AddressBook.framework | `_kABBirthdayProperty` | 1 | thunderbird |
| AddressBook.framework | `_kABDatabaseChangedExternallyNotification` | 1 | thunderbird |
| AddressBook.framework | `_kABDeletedRecords` | 1 | thunderbird |
| AddressBook.framework | `_kABDepartmentProperty` | 1 | thunderbird |
| AddressBook.framework | `_kABHomePageProperty` | 1 | thunderbird |
| AddressBook.framework | `_kABInsertedRecords` | 1 | thunderbird |
| AddressBook.framework | `_kABInstantMessageProperty` | 1 | thunderbird |
| AddressBook.framework | `_kABJobTitleProperty` | 1 | thunderbird |
| AddressBook.framework | `_kABNicknameProperty` | 1 | thunderbird |
| AddressBook.framework | `_kABNoteProperty` | 1 | thunderbird |
| AddressBook.framework | `_kABOtherDatesProperty` | 1 | thunderbird |
| AddressBook.framework | `_kABPersonFlags` | 1 | thunderbird |
| AddressBook.framework | `_kABPersonRecordType` | 1 | libreoffice |
| AddressBook.framework | `_kABPhoneHomeLabel` | 1 | thunderbird |
| AddressBook.framework | `_kABPhoneMobileLabel` | 1 | thunderbird |
| AddressBook.framework | `_kABPhonePagerLabel` | 1 | thunderbird |
| AddressBook.framework | `_kABPhoneWorkFAXLabel` | 1 | thunderbird |
| AddressBook.framework | `_kABPhoneWorkLabel` | 1 | thunderbird |
| AddressBook.framework | `_kABTitleProperty` | 1 | libreoffice |
| AddressBook.framework | `_kABUIDProperty` | 1 | libreoffice |
| AddressBook.framework | `_kABUpdatedRecords` | 1 | thunderbird |
| CoreServices.framework | `_AEStreamClose` | 1 | vlc |
| CoreServices.framework | `_AEStreamCreateEvent` | 1 | vlc |
| CoreServices.framework | `_AEStreamWriteKeyDesc` | 1 | vlc |
| CoreServices.framework | `_CSBackupSetItemExcluded` | 1 | transmission |
| CoreServices.framework | `_CSDiskSpaceGetRecoveryEstimate` | 1 | stats |
| CoreServices.framework | `_CreateTextEncoding` | 1 | iterm2 |
| CoreServices.framework | `_DisposeHandle` | 1 | vlc |
| CoreServices.framework | `_FSCopyAliasInfo` | 1 | vlc |
| CoreServices.framework | `_FSGetVolumeParms` | 1 | vlc |
| CoreServices.framework | `_FSNewAlias` | 1 | vlc |
| CoreServices.framework | `_FSPathMakeRef` | 1 | vlc |
| CoreServices.framework | `_Gestalt` | 1 | vlc |
| CoreServices.framework | `_GetHandleSize` | 1 | vlc |
| CoreServices.framework | `_GetMacOSStatusCommentString` | 1 | vlc |
| CoreServices.framework | `_HLock` | 1 | vlc |
| CoreServices.framework | `_HUnlock` | 1 | vlc |
| CoreServices.framework | `_LSGetApplicationForItem` | 1 | thunderbird |
| CoreServices.framework | `_LSSharedFileListItemCopyDisplayName` | 1 | alt-tab |
| CoreServices.framework | `_LSSharedFileListItemResolve` | 1 | blender |
| CoreServices.framework | `_PtrToHand` | 1 | vlc |
| CoreServices.framework | `_TECConvertText` | 1 | iterm2 |
| CoreServices.framework | `_TECCreateConverter` | 1 | iterm2 |
| CoreServices.framework | `_UCCompareTextDefault` | 1 | inkscape |
| CoreServices.framework | `_UTGetOSTypeFromString` | 1 | krita |
| CoreServices.framework | `_kMDItemBitsPerSample` | 1 | krita |
| CoreServices.framework | `_kMDItemColorSpace` | 1 | krita |
| CoreServices.framework | `_kMDItemCreator` | 1 | krita |
| CoreServices.framework | `_kMDItemDescription` | 1 | libreoffice |
| CoreServices.framework | `_kMDItemHasAlphaChannel` | 1 | krita |
| CoreServices.framework | `_kMDItemHeadline` | 1 | krita |
| CoreServices.framework | `_kMDItemKeywords` | 1 | libreoffice |
| CoreServices.framework | `_kMDItemLayerNames` | 1 | krita |
| CoreServices.framework | `_kMDItemPixelCount` | 1 | krita |
| CoreServices.framework | `_kMDItemPixelHeight` | 1 | krita |
| CoreServices.framework | `_kMDItemPixelWidth` | 1 | krita |
| CoreServices.framework | `_kMDItemProfileName` | 1 | krita |
| CoreServices.framework | `_kMDItemResolutionHeightDPI` | 1 | krita |
| CoreServices.framework | `_kMDItemResolutionWidthDPI` | 1 | krita |
| CoreServices.framework | `_kUTTagClassOSType` | 1 | krita |
| CoreServices.framework | `_kUTTypeDirectory` | 1 | musescore |
| CoreServices.framework | `_kUTTypeUTF8PlainText` | 1 | iterm2 |
| CoreServices.framework | `_kUTTypeVCard` | 1 | musescore |
| CryptoKit.framework | `_$s9CryptoKit03ChaC4PolyO4open_5using10Foundation4DataVAC9SealedBoxV_AA12SymmetricKeyVtKFZ` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit03ChaC4PolyO4seal_5using5nonceAC9SealedBoxVx_AA12SymmetricKeyVAC5NonceVSgtK10Foundation12DataProtocolRzlFZ` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit03ChaC4PolyO5NonceVMa` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit03ChaC4PolyO5NonceVMn` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit0aB5ErrorO22incorrectParameterSizeyA2CmFWC` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit0aB5ErrorOMa` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit0aB5ErrorOs0C0AAMc` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit10Curve25519O7SigningO10PrivateKeyV06publicF0AE06PublicF0Vvg` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit10Curve25519O7SigningO10PrivateKeyV17rawRepresentationAGx_tKc10Foundation15ContiguousBytesRzlufC` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit10Curve25519O7SigningO10PrivateKeyV9signature3for10Foundation4DataVx_tKAJ0J8ProtocolRzlF` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit10Curve25519O7SigningO10PrivateKeyVMa` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit10Curve25519O7SigningO9PublicKeyV16isValidSignature_3forSbx_q_t10Foundation12DataProtocolRzAjKR_r0_lF` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit10Curve25519O7SigningO9PublicKeyV17rawRepresentation10Foundation4DataVvg` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit10Curve25519O7SigningO9PublicKeyV17rawRepresentationAGx_tKc10Foundation15ContiguousBytesRzlufC` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit10Curve25519O7SigningO9PublicKeyVMa` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit12SHA256DigestV10Foundation15ContiguousBytesAAMc` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit12SHA256DigestVMa` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit12SHA256DigestVMn` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit12SHA256DigestVSTAAMc` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit12SymmetricKeyV15withUnsafeBytesyxxSWKXEKlF` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit12SymmetricKeyV4dataACx_tc10Foundation15ContiguousBytesRzlufC` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit12SymmetricKeyVMa` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit24HashedAuthenticationCodeVMn` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit24HashedAuthenticationCodeVyxG10Foundation15ContiguousBytesAAMc` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit24HashedAuthenticationCodeVyxGSTAAMc` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit3AESO3GCMO4open_5using14authenticating10Foundation4DataVAE9SealedBoxV_AA12SymmetricKeyVxtKAI0I8ProtocolRzlFZ` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit3AESO3GCMO4seal_5using5nonce14authenticatingAE9SealedBoxVx_AA12SymmetricKeyVAE5NonceVSgq_tK10Foundation12DataProtocolRzAqRR_r0_lFZ` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit3AESO3GCMO5NonceVMa` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit3AESO3GCMO5NonceVMn` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit3AESO3GCMO9SealedBoxV8combined10Foundation4DataVSgvg` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit3AESO3GCMO9SealedBoxV8combinedAG10Foundation4DataV_tcfC` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit3AESO3GCMO9SealedBoxVMa` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit4HKDFV9deriveKey05inputE8Material4info15outputByteCountAA09SymmetricE0VAI_qd__Sit10Foundation12DataProtocolRd__lFZ` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit4HMACV18authenticationCode3for5usingAA020HashedAuthenticationE0VyxGqd___AA12SymmetricKeyVt10Foundation12DataProtocolRd__lFZ` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit6SHA256VAA12HashFunctionAAMc` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit6SHA256VMa` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit6SHA256VMn` | 1 | iterm2 |
| CryptoKit.framework | `_$s9CryptoKit8InsecureO3MD5VAA12HashFunctionAAMc` | 1 | iina |
| CryptoKit.framework | `_$s9CryptoKit8InsecureO3MD5VMa` | 1 | iina |
| CryptoKit.framework | `_$s9CryptoKit8InsecureO9MD5DigestVMa` | 1 | iina |
| CryptoKit.framework | `_$s9CryptoKit8InsecureO9MD5DigestVSTAAMc` | 1 | iina |
| DeviceCheck.framework | `_OBJC_CLASS_$_DCAppAttestService` | 1 | iterm2 |
| OSAKit.framework | `_OBJC_CLASS_$_OSAScript` | 1 | coteditor |
| OpenDirectory.framework | `_kODAttributeTypeUserShell` | 1 | iterm2 |
| Security.framework | `_SSLSetAllowsAnyRoot` | 1 | vlc |
| Security.framework | `_SSLSetAllowsExpiredCerts` | 1 | vlc |
| Security.framework | `_SSLSetAllowsExpiredRoots` | 1 | vlc |
| Security.framework | `_SSLSetEnableCertVerify` | 1 | vlc |
| Security.framework | `_SSLSetProtocolVersionEnabled` | 1 | vlc |
| Security.framework | `_SecACLGetTypeID` | 1 | musescore |
| Security.framework | `_SecCertificateCopyLongDescription` | 1 | iterm2 |
| Security.framework | `_SecCertificateCopyNormalizedSubjectSequence` | 1 | iterm2 |
| Security.framework | `_SecKeychainItemCopyAttributesAndData` | 1 | vlc |
| Security.framework | `_SecKeychainItemFreeAttributesAndData` | 1 | vlc |
| Security.framework | `_SecKeychainSearchCopyNext` | 1 | vlc |
| Security.framework | `_SecKeychainSearchCreateFromAttributes` | 1 | vlc |
| Security.framework | `_SecTrustGetResult` | 1 | vlc |
| Security.framework | `_SecTrustedApplicationGetTypeID` | 1 | musescore |
| Security.framework | `_kSecAttrPath` | 1 | vlc |
| Security.framework | `_kSecAttrSubject` | 1 | iterm2 |
| Security.framework | `_kSecCodeInfoStatus` | 1 | firefox |
| StoreKit.framework | `_$s8StoreKit0aB5ErrorO07networkC0yAC10Foundation8URLErrorVcACmFWC` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit0aB5ErrorO13userCancelledyA2CmFWC` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit0aB5ErrorOMa` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit0aB5ErrorOMn` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit0aB5ErrorOs0C0AAMc` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit11TransactionV6finishyyYaF` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit11TransactionV6finishyyYaFTu` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit11TransactionVMa` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit11TransactionVMn` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit18VerificationResultO0C5ErrorOMn` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit18VerificationResultO0C5ErrorOyx_Gs0E0AAMc` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit18VerificationResultOMn` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV11descriptionSSvg` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV11displayNameSSvg` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV14PurchaseOptionV8quantityyAESiFZ` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV14PurchaseOptionVMa` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV14PurchaseOptionVMn` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV14PurchaseOptionVSHAAMc` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV14PurchaseOptionVSQAAMc` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV14PurchaseResultO13userCancelledyA2EmFWC` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV14PurchaseResultO7pendingyA2EmFWC` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV14PurchaseResultO7successyAeA012VerificationE0OyAA11TransactionVGcAEmFWC` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV14PurchaseResultOMa` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV16SubscriptionInfoV12RenewalStateV10subscribedAGvgZ` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV16SubscriptionInfoV12RenewalStateV13inGracePeriodAGvgZ` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV16SubscriptionInfoV12RenewalStateVMa` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV16SubscriptionInfoV12RenewalStateVMn` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV16SubscriptionInfoV12RenewalStateVSQAAMc` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV16SubscriptionInfoV6StatusV5stateAE12RenewalStateVvg` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV16SubscriptionInfoV6StatusVMa` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV16SubscriptionInfoV6StatusVMn` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV16priceFormatStyleSo9NSDecimala10FoundationE0eF0V8CurrencyVvg` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductV5priceSo9NSDecimalavg` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductVMa` | 1 | coteditor |
| StoreKit.framework | `_$s8StoreKit7ProductVMn` | 1 | coteditor |
| SystemExtensions.framework | `_OBJC_CLASS_$_OSSystemExtensionManager` | 1 | obs |
| SystemExtensions.framework | `_OBJC_CLASS_$_OSSystemExtensionRequest` | 1 | obs |
| libswiftUniformTypeIdentifiers.dylib | `_$s10Foundation3URLV22UniformTypeIdentifiersE22appendingPathComponent_12conformingToACSS_AD6UTTypeVtF` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers10UTTagClassVSHAAMc` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers10UTTagClassVSQAAMc` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV10javaScriptACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV10perlScriptACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV10resolvableACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV10rubyScriptACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV11appleScriptACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV11shellScriptACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV11swiftSourceACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV12pythonScriptACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV15osaScriptBundleACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV16tabSeparatedTextACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV18audiovisualContentACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV20mpeg2TransportStreamACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV2eeoiySbAC_ACtFZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV3urlACvgZ` | 1 | alt-tab |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV3xmlACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV4tagsSDyAA10UTTagClassVSaySSGGvg` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV4textACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV4yamlACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV6bundleACvgZ` | 1 | iterm2 |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV7archiveACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV9osaScriptACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeV9phpScriptACvgZ` | 1 | coteditor |
| libswiftUniformTypeIdentifiers.dylib | `_$s22UniformTypeIdentifiers6UTTypeVSQAAMc` | 1 | coteditor |
