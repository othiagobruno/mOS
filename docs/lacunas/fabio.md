# Lacunas de Fabio (WebKit/Electron)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Fabio (WebKit/Electron) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**139** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| WebKit.framework | `_OBJC_CLASS_$_WebView` | 54 | adobe-acrobat-reader, aldente, appcleaner, applite |
| WebKit.framework | `_OBJC_CLASS_$_WKUserScript` | 53 | adobe-acrobat-reader, aldente, alt-tab, appcleaner |
| WebKit.framework | `_OBJC_CLASS_$_WKUserContentController` | 48 | adobe-acrobat-reader, aldente, alt-tab, appcleaner |
| WebKit.framework | `_OBJC_CLASS_$_WebPreferences` | 44 | aldente, appcleaner, applite, betterdisplay |
| WebKit.framework | `_OBJC_CLASS_$_WKContentRuleListStore` | 29 | alt-tab, appcleaner, applite, chatgpt |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSContext` | 12 | cmux, codexbar, iina, iterm2 |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSValue` | 9 | cmux, codexbar, iina, iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKContentWorld` | 9 | cloudflare-warp, cmux, drawio, iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKPreferences` | 8 | cloudflare-warp, cmux, hammerspoon, iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKSnapshotConfiguration` | 7 | cloudflare-warp, cmux, iterm2, openclaw |
| WebKit.framework | `_OBJC_CLASS_$_WKBackForwardListItem` | 5 | cloudflare-warp, cmux, iterm2, telegram |
| WebKit.framework | `_OBJC_CLASS_$_WKProcessPool` | 5 | cloudflare-warp, hammerspoon, iterm2, telegram |
| JavaScriptCore.framework | `_JSValueToObject` | 4 | microsoft-teams, telegram, zoom, zotero |
| WebKit.framework | `_WKErrorDomain` | 4 | cmux, drawio, whatsapp, zoom |
| JavaScriptCore.framework | `_JSContextGetGroup` | 3 | cmux, tableplus, telegram |
| JavaScriptCore.framework | `_JSEvaluateScript` | 3 | macdown, microsoft-teams, zoom |
| JavaScriptCore.framework | `_JSGlobalContextRelease` | 3 | macdown, microsoft-teams, zoom |
| JavaScriptCore.framework | `_JSStringCreateWithUTF8CString` | 3 | macdown, microsoft-teams, zoom |
| JavaScriptCore.framework | `_JSStringGetUTF8CString` | 3 | macdown, microsoft-teams, zoom |
| JavaScriptCore.framework | `_JSStringRelease` | 3 | macdown, microsoft-teams, zoom |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariApplication` | 3 | bitwarden, iina, zotero |
| WebKit.framework | `_OBJC_CLASS_$_WKFrameInfo` | 3 | cmux, iterm2, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKWebsiteDataRecord` | 3 | cloudflare-warp, codexbar, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WebArchive` | 3 | adobe-acrobat-reader, chatgpt, keka |
| WebKit.framework | `_WKWebsiteDataTypeDiskCache` | 3 | cloudflare-warp, codexbar, telegram |
| WebKit.framework | `_WKWebsiteDataTypeMemoryCache` | 3 | cloudflare-warp, codexbar, telegram |
| WebKit.framework | `_WebViewProgressStartedNotification` | 3 | adobe-acrobat-reader, gemini, the-unarchiver |
| JavaScriptCore.framework | `_JSContextGetGlobalObject` | 2 | macdown, microsoft-teams |
| JavaScriptCore.framework | `_JSGlobalContextCreate` | 2 | macdown, zoom |
| JavaScriptCore.framework | `_JSObjectCallAsFunction` | 2 | microsoft-teams, zoom |
| JavaScriptCore.framework | `_JSObjectGetArrayBufferBytesPtr` | 2 | microsoft-teams, zotero |
| JavaScriptCore.framework | `_JSObjectGetProperty` | 2 | macdown, microsoft-teams |
| JavaScriptCore.framework | `_JSObjectGetTypedArrayBytesPtr` | 2 | microsoft-teams, telegram |
| JavaScriptCore.framework | `_JSObjectMakeArrayBufferWithBytesNoCopy` | 2 | microsoft-teams, zotero |
| JavaScriptCore.framework | `_JSStringGetLength` | 2 | macdown, microsoft-teams |
| JavaScriptCore.framework | `_JSStringGetMaximumUTF8CStringSize` | 2 | microsoft-teams, zoom |
| JavaScriptCore.framework | `_JSValueGetTypedArrayType` | 2 | microsoft-teams, telegram |
| JavaScriptCore.framework | `_JSValueMakeString` | 2 | microsoft-teams, zoom |
| JavaScriptCore.framework | `_JSValueToStringCopy` | 2 | microsoft-teams, zoom |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSVirtualMachine` | 2 | tableplus, telegram |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariExtensionHandler` | 2 | iina, zotero |
| SafariServices.framework | `_OBJC_METACLASS_$_SFSafariExtensionHandler` | 2 | iina, zotero |
| WebKit.framework | `_OBJC_CLASS_$_WKHTTPCookieStore` | 2 | cmux, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKNavigation` | 2 | chatgpt, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKPDFConfiguration` | 2 | cloudflare-warp, cmux |
| WebKit.framework | `_OBJC_CLASS_$_WebUndefined` | 2 | gemini, the-unarchiver |
| WebKit.framework | `_WKWebsiteDataTypeFetchCache` | 2 | cloudflare-warp, zoom |
| WebKit.framework | `_WKWebsiteDataTypeLocalStorage` | 2 | cloudflare-warp, zoom |
| WebKit.framework | `_WKWebsiteDataTypeServiceWorkerRegistrations` | 2 | openclaw, zoom |
| WebKit.framework | `_WebViewProgressEstimateChangedNotification` | 2 | gemini, the-unarchiver |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKF` | 2 | cmux, iterm2 |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKFTu` | 2 | cmux, iterm2 |
| JavaScriptCore.framework | `_JSClassCreate` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSClassRelease` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSContextGetGlobalContext` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSContextGroupSetExecutionTimeLimit` | 1 | tableplus |
| JavaScriptCore.framework | `_JSGlobalContextCreateInGroup` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSGlobalContextRetain` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectCallAsConstructor` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectDeleteProperty` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectGetArrayBufferByteLength` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectGetPrivate` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectGetPropertyAtIndex` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectGetPropertyForKey` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectGetPrototype` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectGetTypedArrayBuffer` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectGetTypedArrayByteLength` | 1 | telegram |
| JavaScriptCore.framework | `_JSObjectGetTypedArrayByteOffset` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectGetTypedArrayLength` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectHasProperty` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectHasPropertyForKey` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectIsConstructor` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectIsFunction` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectMake` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectMakeArray` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectMakeConstructor` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectMakeError` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectMakeFunctionWithCallback` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectMakeTypedArrayWithArrayBufferAndOffset` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectMakeTypedArrayWithBytesNoCopy` | 1 | iina |
| JavaScriptCore.framework | `_JSObjectSetProperty` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectSetPropertyAtIndex` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectSetPropertyForKey` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectSetPrototype` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSStringCreateWithCFString` | 1 | macdown |
| JavaScriptCore.framework | `_JSStringCreateWithCharacters` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSStringGetCharactersPtr` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueCreateJSONString` | 1 | macdown |
| JavaScriptCore.framework | `_JSValueGetType` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueIsArray` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueIsInstanceOfConstructor` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueIsStrictEqual` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueIsString` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueIsUndefined` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueMakeBoolean` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueMakeNull` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueMakeNumber` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueMakeSymbol` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueMakeUndefined` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueProtect` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueToBoolean` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueToNumber` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSValueUnprotect` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSManagedValue` | 1 | iina |
| JavaScriptCore.framework | `_kJSClassDefinitionEmpty` | 1 | microsoft-teams |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariExtension` | 1 | zotero |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariExtensionViewController` | 1 | zotero |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariTab` | 1 | zotero |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariViewController` | 1 | whatsapp |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariWindow` | 1 | zotero |
| SafariServices.framework | `_OBJC_CLASS_$_SSReadingList` | 1 | whatsapp |
| SafariServices.framework | `_OBJC_METACLASS_$_SFSafariExtensionViewController` | 1 | zotero |
| SafariServices.framework | `_SFExtensionMessageKey` | 1 | zotero |
| WebKit.framework | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKF` | 1 | openclaw |
| WebKit.framework | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKFTu` | 1 | openclaw |
| WebKit.framework | `_OBJC_CLASS_$_DOMNode` | 1 | macdown |
| WebKit.framework | `_OBJC_CLASS_$_WKContentRuleList` | 1 | whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKFindConfiguration` | 1 | sequel-ace |
| WebKit.framework | `_OBJC_CLASS_$_WKNavigationResponse` | 1 | whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKWebpagePreferences` | 1 | whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WebResource` | 1 | chatgpt |
| WebKit.framework | `_OBJC_METACLASS_$_WKUserContentController` | 1 | hammerspoon |
| WebKit.framework | `_OBJC_METACLASS_$_WKUserScript` | 1 | cloudflare-warp |
| WebKit.framework | `_OBJC_METACLASS_$_WebArchive` | 1 | chatgpt |
| WebKit.framework | `_WKWebsiteDataTypeIndexedDBDatabases` | 1 | zoom |
| WebKit.framework | `_WKWebsiteDataTypeOfflineWebApplicationCache` | 1 | cloudflare-warp |
| WebKit.framework | `_WKWebsiteDataTypeSessionStorage` | 1 | zoom |
| WebKit.framework | `_WKWebsiteDataTypeWebSQLDatabases` | 1 | zoom |
| WebKit.framework | `_WebActionElementKey` | 1 | zoom |
| WebKit.framework | `_WebActionNavigationTypeKey` | 1 | macdown |
| WebKit.framework | `_WebKitErrorDomain` | 1 | zoom |
| WebKit.framework | `_WebViewProgressFinishedNotification` | 1 | adobe-acrobat-reader |
| libswiftWebKit.dylib | `_$sSo18WKPDFConfigurationC6WebKitE4rectSo6CGRectVSgvs` | 1 | cloudflare-warp |
| libswiftWebKit.dylib | `_$sSo18WKWebsiteDataStoreC6WebKitE19proxyConfigurationsSay7Network18ProxyConfigurationVGvs` | 1 | cmux |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE06createC11ArchiveData17completionHandleryys6ResultOy10Foundation0G0Vs5Error_pGc_tF` | 1 | cloudflare-warp |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE3pdf13configuration10Foundation4DataVSo18WKPDFConfigurationC_tYaKF` | 1 | cmux |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE3pdf13configuration10Foundation4DataVSo18WKPDFConfigurationC_tYaKFTu` | 1 | cmux |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE4find_13configuration17completionHandlerySS_So19WKFindConfigurationCySo0I6ResultCctF` | 1 | sequel-ace |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE9createPDF13configuration17completionHandlerySo18WKPDFConfigurationC_ys6ResultOy10Foundation4DataVs5Error_pGctF` | 1 | cloudflare-warp |
