# Lacunas de Fabio (WebKit/Electron)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Fabio (WebKit/Electron) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**157** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| WebKit.framework | `_OBJC_CLASS_$_WebView` | 86 | adobe-acrobat-reader, aldente, amethyst, appcleaner |
| WebKit.framework | `_OBJC_CLASS_$_WKUserScript` | 85 | adobe-acrobat-reader, aldente, alt-tab, amethyst |
| WebKit.framework | `_OBJC_CLASS_$_WKUserContentController` | 78 | adobe-acrobat-reader, aldente, alt-tab, amethyst |
| WebKit.framework | `_OBJC_CLASS_$_WebPreferences` | 75 | aldente, amethyst, appcleaner, applite |
| WebKit.framework | `_OBJC_CLASS_$_WKContentRuleListStore` | 46 | alt-tab, amethyst, appcleaner, applite |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSContext` | 17 | amethyst, bettertouchtool, cmux, codexbar |
| WebKit.framework | `_OBJC_CLASS_$_WKContentWorld` | 13 | bettertouchtool, cloudflare-warp, cmux, drawio |
| WebKit.framework | `_OBJC_CLASS_$_WKPreferences` | 13 | bettertouchtool, cloudflare-warp, cmux, duckduckgo |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSValue` | 12 | amethyst, bettertouchtool, cmux, codexbar |
| WebKit.framework | `_OBJC_CLASS_$_WKProcessPool` | 8 | bettertouchtool, cloudflare-warp, duckduckgo, hammerspoon |
| WebKit.framework | `_OBJC_CLASS_$_WKSnapshotConfiguration` | 8 | cloudflare-warp, cmux, duckduckgo, iterm2 |
| JavaScriptCore.framework | `_JSStringRelease` | 6 | bettertouchtool, macdown, macdown-3000, microsoft-teams |
| WebKit.framework | `_OBJC_CLASS_$_WKBackForwardListItem` | 6 | cloudflare-warp, cmux, duckduckgo, iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKWebsiteDataRecord` | 6 | cloudflare-warp, codexbar, duckduckgo, fork |
| WebKit.framework | `_WKWebsiteDataTypeDiskCache` | 6 | bettertouchtool, cloudflare-warp, codexbar, duckduckgo |
| WebKit.framework | `_WKWebsiteDataTypeMemoryCache` | 6 | bettertouchtool, cloudflare-warp, codexbar, duckduckgo |
| JavaScriptCore.framework | `_JSGlobalContextCreate` | 5 | bettertouchtool, macdown, macdown-3000, proxyman |
| JavaScriptCore.framework | `_JSGlobalContextRelease` | 5 | macdown, macdown-3000, microsoft-teams, proxyman |
| JavaScriptCore.framework | `_JSStringCreateWithUTF8CString` | 5 | macdown, macdown-3000, microsoft-teams, proxyman |
| JavaScriptCore.framework | `_JSStringGetUTF8CString` | 5 | macdown, macdown-3000, microsoft-teams, proxyman |
| JavaScriptCore.framework | `_JSValueToObject` | 5 | microsoft-teams, proxyman, telegram, zoom |
| WebKit.framework | `_OBJC_CLASS_$_WKWebpagePreferences` | 5 | bettertouchtool, duckduckgo, proxyman, teamviewer |
| WebKit.framework | `_WKErrorDomain` | 5 | cmux, drawio, duckduckgo, whatsapp |
| JavaScriptCore.framework | `_JSContextGetGroup` | 4 | bettertouchtool, cmux, tableplus, telegram |
| JavaScriptCore.framework | `_JSEvaluateScript` | 4 | macdown, macdown-3000, microsoft-teams, zoom |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSVirtualMachine` | 4 | bettertouchtool, macwhisper, tableplus, telegram |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariApplication` | 4 | bettertouchtool, bitwarden, iina, zotero |
| WebKit.framework | `_OBJC_CLASS_$_WKFrameInfo` | 4 | cmux, duckduckgo, iterm2, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKNavigation` | 4 | amethyst, chatgpt, duckduckgo, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKPDFConfiguration` | 4 | bettertouchtool, cloudflare-warp, cmux, duckduckgo |
| WebKit.framework | `_OBJC_CLASS_$_WebArchive` | 4 | adobe-acrobat-reader, chatgpt, keka, typora |
| WebKit.framework | `_WKWebsiteDataTypeLocalStorage` | 4 | bettertouchtool, cloudflare-warp, duckduckgo, zoom |
| JavaScriptCore.framework | `_JSContextGetGlobalObject` | 3 | macdown, macdown-3000, microsoft-teams |
| JavaScriptCore.framework | `_JSObjectGetProperty` | 3 | macdown, macdown-3000, microsoft-teams |
| JavaScriptCore.framework | `_JSObjectGetTypedArrayBytesPtr` | 3 | microsoft-teams, proxyman, telegram |
| JavaScriptCore.framework | `_JSStringCreateWithCFString` | 3 | bettertouchtool, macdown, macdown-3000 |
| JavaScriptCore.framework | `_JSStringGetLength` | 3 | macdown, macdown-3000, microsoft-teams |
| JavaScriptCore.framework | `_JSStringGetMaximumUTF8CStringSize` | 3 | microsoft-teams, proxyman, zoom |
| JavaScriptCore.framework | `_JSValueGetTypedArrayType` | 3 | microsoft-teams, proxyman, telegram |
| JavaScriptCore.framework | `_JSValueToStringCopy` | 3 | microsoft-teams, proxyman, zoom |
| WebKit.framework | `_OBJC_CLASS_$_WKHTTPCookieStore` | 3 | cmux, duckduckgo, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WebUndefined` | 3 | gemini, the-unarchiver, typora |
| WebKit.framework | `_WKWebsiteDataTypeFetchCache` | 3 | bettertouchtool, cloudflare-warp, zoom |
| WebKit.framework | `_WKWebsiteDataTypeIndexedDBDatabases` | 3 | bettertouchtool, duckduckgo, zoom |
| WebKit.framework | `_WKWebsiteDataTypeOfflineWebApplicationCache` | 3 | bettertouchtool, cloudflare-warp, duckduckgo |
| WebKit.framework | `_WKWebsiteDataTypeServiceWorkerRegistrations` | 3 | bettertouchtool, openclaw, zoom |
| WebKit.framework | `_WebViewProgressStartedNotification` | 3 | adobe-acrobat-reader, gemini, the-unarchiver |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKF` | 3 | cmux, duckduckgo, iterm2 |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKFTu` | 3 | cmux, duckduckgo, iterm2 |
| JavaScriptCore.framework | `_JSObjectCallAsFunction` | 2 | microsoft-teams, zoom |
| JavaScriptCore.framework | `_JSObjectGetArrayBufferBytesPtr` | 2 | microsoft-teams, zotero |
| JavaScriptCore.framework | `_JSObjectGetTypedArrayByteLength` | 2 | proxyman, telegram |
| JavaScriptCore.framework | `_JSObjectMakeArrayBufferWithBytesNoCopy` | 2 | microsoft-teams, zotero |
| JavaScriptCore.framework | `_JSValueCreateJSONString` | 2 | macdown, macdown-3000 |
| JavaScriptCore.framework | `_JSValueMakeString` | 2 | microsoft-teams, zoom |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSManagedValue` | 2 | bettertouchtool, iina |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariExtensionHandler` | 2 | iina, zotero |
| SafariServices.framework | `_OBJC_METACLASS_$_SFSafariExtensionHandler` | 2 | iina, zotero |
| SafariServices.framework | `_SFExtensionMessageKey` | 2 | bettertouchtool, zotero |
| WebKit.framework | `_OBJC_CLASS_$_DOMNode` | 2 | macdown, macdown-3000 |
| WebKit.framework | `_OBJC_CLASS_$_WKContentRuleList` | 2 | duckduckgo, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKFindConfiguration` | 2 | duckduckgo, sequel-ace |
| WebKit.framework | `_OBJC_CLASS_$_WebResource` | 2 | chatgpt, typora |
| WebKit.framework | `_OBJC_METACLASS_$_WKUserContentController` | 2 | duckduckgo, hammerspoon |
| WebKit.framework | `_WKWebsiteDataTypeSessionStorage` | 2 | bettertouchtool, zoom |
| WebKit.framework | `_WKWebsiteDataTypeWebSQLDatabases` | 2 | bettertouchtool, zoom |
| WebKit.framework | `_WebActionNavigationTypeKey` | 2 | macdown, macdown-3000 |
| WebKit.framework | `_WebViewProgressEstimateChangedNotification` | 2 | gemini, the-unarchiver |
| libswiftWebKit.dylib | `_$sSo18WKPDFConfigurationC6WebKitE4rectSo6CGRectVSgvs` | 2 | bettertouchtool, cloudflare-warp |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE06createC11ArchiveData17completionHandleryys6ResultOy10Foundation0G0Vs5Error_pGc_tF` | 2 | cloudflare-warp, duckduckgo |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE3pdf13configuration10Foundation4DataVSo18WKPDFConfigurationC_tYaKF` | 2 | bettertouchtool, cmux |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE3pdf13configuration10Foundation4DataVSo18WKPDFConfigurationC_tYaKFTu` | 2 | bettertouchtool, cmux |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE4find_13configuration17completionHandlerySS_So19WKFindConfigurationCySo0I6ResultCctF` | 2 | duckduckgo, sequel-ace |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE9createPDF13configuration17completionHandlerySo18WKPDFConfigurationC_ys6ResultOy10Foundation4DataVs5Error_pGctF` | 2 | cloudflare-warp, duckduckgo |
| JavaScriptCore.framework | `_JSCheckScriptSyntax` | 1 | proxyman |
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
| JavaScriptCore.framework | `_JSObjectMakeTypedArray` | 1 | proxyman |
| JavaScriptCore.framework | `_JSObjectMakeTypedArrayWithArrayBufferAndOffset` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectMakeTypedArrayWithBytesNoCopy` | 1 | iina |
| JavaScriptCore.framework | `_JSObjectSetProperty` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectSetPropertyAtIndex` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectSetPropertyForKey` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSObjectSetPrototype` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSPropertyDescriptorConfigurableKey` | 1 | proxyman |
| JavaScriptCore.framework | `_JSPropertyDescriptorEnumerableKey` | 1 | proxyman |
| JavaScriptCore.framework | `_JSPropertyDescriptorValueKey` | 1 | proxyman |
| JavaScriptCore.framework | `_JSPropertyDescriptorWritableKey` | 1 | proxyman |
| JavaScriptCore.framework | `_JSStringCreateWithCharacters` | 1 | microsoft-teams |
| JavaScriptCore.framework | `_JSStringGetCharactersPtr` | 1 | microsoft-teams |
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
| JavaScriptCore.framework | `_OBJC_METACLASS_$_JSContext` | 1 | bettertouchtool |
| JavaScriptCore.framework | `_kJSClassDefinitionEmpty` | 1 | microsoft-teams |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariExtension` | 1 | zotero |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariExtensionManager` | 1 | bettertouchtool |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariExtensionViewController` | 1 | zotero |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariTab` | 1 | zotero |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariViewController` | 1 | whatsapp |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariWindow` | 1 | zotero |
| SafariServices.framework | `_OBJC_CLASS_$_SSReadingList` | 1 | whatsapp |
| SafariServices.framework | `_OBJC_METACLASS_$_SFSafariExtensionViewController` | 1 | zotero |
| WebKit.framework | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKF` | 1 | openclaw |
| WebKit.framework | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKFTu` | 1 | openclaw |
| WebKit.framework | `_OBJC_CLASS_$_WKDownload` | 1 | duckduckgo |
| WebKit.framework | `_OBJC_CLASS_$_WKNavigationAction` | 1 | duckduckgo |
| WebKit.framework | `_OBJC_CLASS_$_WKNavigationResponse` | 1 | whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKScriptMessage` | 1 | duckduckgo |
| WebKit.framework | `_OBJC_CLASS_$_WKView` | 1 | ubersicht |
| WebKit.framework | `_OBJC_METACLASS_$_WKProcessPool` | 1 | bettertouchtool |
| WebKit.framework | `_OBJC_METACLASS_$_WKUserScript` | 1 | cloudflare-warp |
| WebKit.framework | `_OBJC_METACLASS_$_WebArchive` | 1 | chatgpt |
| WebKit.framework | `_OBJC_METACLASS_$_WebView` | 1 | bettertouchtool |
| WebKit.framework | `_WKInspectorDetach` | 1 | ubersicht |
| WebKit.framework | `_WKInspectorShowConsole` | 1 | ubersicht |
| WebKit.framework | `_WKPageGetInspector` | 1 | ubersicht |
| WebKit.framework | `_WebActionElementKey` | 1 | zoom |
| WebKit.framework | `_WebActionOriginalURLKey` | 1 | bettertouchtool |
| WebKit.framework | `_WebKitErrorDomain` | 1 | zoom |
| WebKit.framework | `_WebViewProgressFinishedNotification` | 1 | adobe-acrobat-reader |
| libswiftWebKit.dylib | `_$sSo18WKWebsiteDataStoreC6WebKitE19proxyConfigurationsSay7Network18ProxyConfigurationVGvs` | 1 | cmux |
