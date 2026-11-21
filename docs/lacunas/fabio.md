# Lacunas de Fabio (WebKit/Electron)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Fabio (WebKit/Electron) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**190** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| WebKit.framework | `_OBJC_CLASS_$_WKUserScript` | 106 | adobe-acrobat-reader, affinity, aldente, alt-tab |
| WebKit.framework | `_OBJC_CLASS_$_WebView` | 102 | adobe-acrobat-reader, affinity, aldente, amethyst |
| WebKit.framework | `_OBJC_CLASS_$_WKUserContentController` | 96 | adobe-acrobat-reader, affinity, aldente, alt-tab |
| WebKit.framework | `_OBJC_CLASS_$_WebPreferences` | 87 | aldente, amethyst, appcleaner, applite |
| WebKit.framework | `_OBJC_CLASS_$_WKContentRuleListStore` | 54 | alt-tab, amethyst, appcleaner, applite |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSContext` | 19 | amethyst, bettertouchtool, cmux, codexbar |
| WebKit.framework | `_OBJC_CLASS_$_WKContentWorld` | 18 | bettertouchtool, cloudflare-warp, cmux, drawio |
| WebKit.framework | `_OBJC_CLASS_$_WKPreferences` | 18 | bettertouchtool, cloudflare-warp, cmux, duckduckgo |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSValue` | 13 | amethyst, bettertouchtool, cmux, codexbar |
| WebKit.framework | `_OBJC_CLASS_$_WKProcessPool` | 12 | bettertouchtool, cloudflare-warp, duckduckgo, hammerspoon |
| WebKit.framework | `_OBJC_CLASS_$_WKSnapshotConfiguration` | 12 | cloudflare-warp, cmux, duckduckgo, iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKWebpagePreferences` | 12 | bettertouchtool, duckduckgo, github-copilot-for-xcode, microsoft-excel |
| JavaScriptCore.framework | `_JSGlobalContextRelease` | 11 | capcut, macdown, macdown-3000, microsoft-excel |
| JavaScriptCore.framework | `_JSStringGetUTF8CString` | 11 | capcut, macdown, macdown-3000, microsoft-excel |
| JavaScriptCore.framework | `_JSStringRelease` | 11 | bettertouchtool, capcut, macdown, macdown-3000 |
| JavaScriptCore.framework | `_JSValueToObject` | 11 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| WebKit.framework | `_WKWebsiteDataTypeDiskCache` | 11 | bettertouchtool, cloudflare-warp, codexbar, duckduckgo |
| WebKit.framework | `_WKWebsiteDataTypeMemoryCache` | 11 | bettertouchtool, cloudflare-warp, codexbar, duckduckgo |
| JavaScriptCore.framework | `_JSStringCreateWithUTF8CString` | 10 | capcut, macdown, macdown-3000, microsoft-excel |
| JavaScriptCore.framework | `_JSEvaluateScript` | 9 | capcut, macdown, macdown-3000, microsoft-excel |
| JavaScriptCore.framework | `_JSStringGetMaximumUTF8CStringSize` | 9 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| WebKit.framework | `_WKWebsiteDataTypeLocalStorage` | 9 | bettertouchtool, cloudflare-warp, duckduckgo, microsoft-excel |
| JavaScriptCore.framework | `_JSContextGetGlobalObject` | 8 | capcut, macdown, macdown-3000, microsoft-excel |
| JavaScriptCore.framework | `_JSObjectGetProperty` | 8 | capcut, macdown, macdown-3000, microsoft-excel |
| JavaScriptCore.framework | `_JSObjectGetTypedArrayBytesPtr` | 8 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueGetTypedArrayType` | 8 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueToStringCopy` | 8 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| WebKit.framework | `_WKWebsiteDataTypeFetchCache` | 8 | bettertouchtool, cloudflare-warp, microsoft-excel, microsoft-outlook |
| WebKit.framework | `_WKWebsiteDataTypeIndexedDBDatabases` | 8 | bettertouchtool, duckduckgo, microsoft-excel, microsoft-outlook |
| WebKit.framework | `_WKWebsiteDataTypeOfflineWebApplicationCache` | 8 | bettertouchtool, cloudflare-warp, duckduckgo, microsoft-excel |
| JavaScriptCore.framework | `_JSObjectCallAsFunction` | 7 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectGetArrayBufferBytesPtr` | 7 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectMakeArrayBufferWithBytesNoCopy` | 7 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueIsArray` | 7 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueMakeString` | 7 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| WebKit.framework | `_WKErrorDomain` | 7 | cmux, drawio, duckduckgo, google-drive |
| WebKit.framework | `_WKWebsiteDataTypeSessionStorage` | 7 | bettertouchtool, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| WebKit.framework | `_WKWebsiteDataTypeWebSQLDatabases` | 7 | bettertouchtool, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSClassCreate` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSContextGetGlobalContext` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSGlobalContextCreateInGroup` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSGlobalContextRetain` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectCallAsConstructor` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectGetArrayBufferByteLength` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectGetPrivate` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectGetPropertyAtIndex` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectGetPrototype` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectGetTypedArrayBuffer` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectGetTypedArrayByteOffset` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectGetTypedArrayLength` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectHasProperty` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectIsFunction` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectMake` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectMakeArray` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectMakeError` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectMakeTypedArrayWithArrayBufferAndOffset` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectSetProperty` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectSetPropertyAtIndex` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectSetPrototype` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSStringCreateWithCharacters` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueGetType` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueIsInstanceOfConstructor` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueIsStrictEqual` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueIsString` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueIsUndefined` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueMakeBoolean` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueMakeNull` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueMakeNumber` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueMakeUndefined` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueProtect` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueToBoolean` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueToNumber` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueUnprotect` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_kJSClassDefinitionEmpty` | 6 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| WebKit.framework | `_OBJC_CLASS_$_WKBackForwardListItem` | 6 | cloudflare-warp, cmux, duckduckgo, iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKWebsiteDataRecord` | 6 | cloudflare-warp, codexbar, duckduckgo, fork |
| WebKit.framework | `_WebActionNavigationTypeKey` | 6 | macdown, macdown-3000, microsoft-excel, microsoft-outlook |
| JavaScriptCore.framework | `_JSClassRelease` | 5 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-teams |
| JavaScriptCore.framework | `_JSContextGroupRelease` | 5 | capcut, microsoft-excel, microsoft-office, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSGlobalContextCreate` | 5 | bettertouchtool, macdown, macdown-3000, proxyman |
| JavaScriptCore.framework | `_JSGlobalContextSetName` | 5 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectCopyPropertyNames` | 5 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectGetPropertyForKey` | 5 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-teams |
| JavaScriptCore.framework | `_JSObjectHasPropertyForKey` | 5 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-teams |
| JavaScriptCore.framework | `_JSObjectIsConstructor` | 5 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-teams |
| JavaScriptCore.framework | `_JSObjectMakeConstructor` | 5 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-teams |
| JavaScriptCore.framework | `_JSObjectMakeFunctionWithCallback` | 5 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-teams |
| JavaScriptCore.framework | `_JSObjectSetPrivate` | 5 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSObjectSetPropertyForKey` | 5 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-teams |
| JavaScriptCore.framework | `_JSPropertyNameAccumulatorAddName` | 5 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSPropertyNameArrayGetCount` | 5 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSPropertyNameArrayGetNameAtIndex` | 5 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSPropertyNameArrayRelease` | 5 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSStringIsEqual` | 5 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSStringRetain` | 5 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueIsEqual` | 5 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueIsObjectOfClass` | 5 | capcut, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| JavaScriptCore.framework | `_JSValueMakeSymbol` | 5 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-teams |
| WebKit.framework | `_OBJC_CLASS_$_WKPDFConfiguration` | 5 | bettertouchtool, cloudflare-warp, cmux, coconutbattery |
| JavaScriptCore.framework | `_JSContextGetGroup` | 4 | bettertouchtool, cmux, tableplus, telegram |
| JavaScriptCore.framework | `_JSStringCreateWithCFString` | 4 | bettertouchtool, macdown, macdown-3000, onlyoffice |
| JavaScriptCore.framework | `_JSStringGetLength` | 4 | capcut, macdown, macdown-3000, microsoft-teams |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSVirtualMachine` | 4 | bettertouchtool, macwhisper, tableplus, telegram |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariApplication` | 4 | bettertouchtool, bitwarden, iina, zotero |
| WebKit.framework | `_OBJC_CLASS_$_WKFrameInfo` | 4 | cmux, duckduckgo, iterm2, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKNavigation` | 4 | amethyst, chatgpt, duckduckgo, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WebArchive` | 4 | adobe-acrobat-reader, chatgpt, keka, typora |
| WebKit.framework | `_WKWebsiteDataTypeServiceWorkerRegistrations` | 4 | bettertouchtool, mysqlworkbench, openclaw, zoom |
| WebKit.framework | `_WebElementLinkURLKey` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| JavaScriptCore.framework | `_JSValueCreateJSONString` | 3 | macdown, macdown-3000, onlyoffice |
| WebKit.framework | `_OBJC_CLASS_$_DOMNode` | 3 | macdown, macdown-3000, microsoft-outlook |
| WebKit.framework | `_OBJC_CLASS_$_WKFindConfiguration` | 3 | duckduckgo, microsoft-outlook, sequel-ace |
| WebKit.framework | `_OBJC_CLASS_$_WKHTTPCookieStore` | 3 | cmux, duckduckgo, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WebResource` | 3 | chatgpt, microsoft-outlook, typora |
| WebKit.framework | `_OBJC_CLASS_$_WebUndefined` | 3 | gemini, the-unarchiver, typora |
| WebKit.framework | `_WebViewProgressStartedNotification` | 3 | adobe-acrobat-reader, gemini, the-unarchiver |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKF` | 3 | cmux, duckduckgo, iterm2 |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKFTu` | 3 | cmux, duckduckgo, iterm2 |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE4find_13configuration17completionHandlerySS_So19WKFindConfigurationCySo0I6ResultCctF` | 3 | duckduckgo, microsoft-outlook, sequel-ace |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE9createPDF13configuration17completionHandlerySo18WKPDFConfigurationC_ys6ResultOy10Foundation4DataVs5Error_pGctF` | 3 | cloudflare-warp, coconutbattery, duckduckgo |
| JavaScriptCore.framework | `_JSObjectDeleteProperty` | 2 | capcut, microsoft-teams |
| JavaScriptCore.framework | `_JSObjectGetTypedArrayByteLength` | 2 | proxyman, telegram |
| JavaScriptCore.framework | `_JSStringGetCharactersPtr` | 2 | capcut, microsoft-teams |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSManagedValue` | 2 | bettertouchtool, iina |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariExtensionHandler` | 2 | iina, zotero |
| SafariServices.framework | `_OBJC_METACLASS_$_SFSafariExtensionHandler` | 2 | iina, zotero |
| SafariServices.framework | `_SFExtensionMessageKey` | 2 | bettertouchtool, zotero |
| WebKit.framework | `_OBJC_CLASS_$_WKContentRuleList` | 2 | duckduckgo, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WebDownload` | 2 | mactex, mactex-no-gui |
| WebKit.framework | `_OBJC_METACLASS_$_WKUserContentController` | 2 | duckduckgo, hammerspoon |
| WebKit.framework | `_OBJC_METACLASS_$_WebView` | 2 | bettertouchtool, microsoft-outlook |
| WebKit.framework | `_WebActionElementKey` | 2 | microsoft-outlook, zoom |
| WebKit.framework | `_WebViewProgressEstimateChangedNotification` | 2 | gemini, the-unarchiver |
| libswiftWebKit.dylib | `_$sSo18WKPDFConfigurationC6WebKitE4rectSo6CGRectVSgvs` | 2 | bettertouchtool, cloudflare-warp |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE06createC11ArchiveData17completionHandleryys6ResultOy10Foundation0G0Vs5Error_pGc_tF` | 2 | cloudflare-warp, duckduckgo |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE3pdf13configuration10Foundation4DataVSo18WKPDFConfigurationC_tYaKF` | 2 | bettertouchtool, cmux |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE3pdf13configuration10Foundation4DataVSo18WKPDFConfigurationC_tYaKFTu` | 2 | bettertouchtool, cmux |
| JavaScriptCore.framework | `_JSCheckScriptSyntax` | 1 | proxyman |
| JavaScriptCore.framework | `_JSContextGroupCreate` | 1 | capcut |
| JavaScriptCore.framework | `_JSContextGroupSetExecutionTimeLimit` | 1 | tableplus |
| JavaScriptCore.framework | `_JSGarbageCollect` | 1 | capcut |
| JavaScriptCore.framework | `_JSObjectMakeDate` | 1 | capcut |
| JavaScriptCore.framework | `_JSObjectMakeTypedArray` | 1 | proxyman |
| JavaScriptCore.framework | `_JSObjectMakeTypedArrayWithBytesNoCopy` | 1 | iina |
| JavaScriptCore.framework | `_JSPropertyDescriptorConfigurableKey` | 1 | proxyman |
| JavaScriptCore.framework | `_JSPropertyDescriptorEnumerableKey` | 1 | proxyman |
| JavaScriptCore.framework | `_JSPropertyDescriptorValueKey` | 1 | proxyman |
| JavaScriptCore.framework | `_JSPropertyDescriptorWritableKey` | 1 | proxyman |
| JavaScriptCore.framework | `_JSValueIsBoolean` | 1 | capcut |
| JavaScriptCore.framework | `_JSValueIsDate` | 1 | capcut |
| JavaScriptCore.framework | `_JSValueIsNull` | 1 | capcut |
| JavaScriptCore.framework | `_JSValueIsNumber` | 1 | capcut |
| JavaScriptCore.framework | `_JSValueIsObject` | 1 | capcut |
| JavaScriptCore.framework | `_JSValueMakeFromJSONString` | 1 | onlyoffice |
| JavaScriptCore.framework | `_OBJC_METACLASS_$_JSContext` | 1 | bettertouchtool |
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
| WebKit.framework | `_OBJC_CLASS_$_DOMElement` | 1 | microsoft-outlook |
| WebKit.framework | `_OBJC_CLASS_$_DOMHTMLDivElement` | 1 | microsoft-outlook |
| WebKit.framework | `_OBJC_CLASS_$_DOMHTMLPreElement` | 1 | microsoft-outlook |
| WebKit.framework | `_OBJC_CLASS_$_DOMHTMLTableCellElement` | 1 | microsoft-outlook |
| WebKit.framework | `_OBJC_CLASS_$_DOMHTMLTableElement` | 1 | microsoft-outlook |
| WebKit.framework | `_OBJC_CLASS_$_DOMHTMLTableRowElement` | 1 | microsoft-outlook |
| WebKit.framework | `_OBJC_CLASS_$_DOMHTMLTableSectionElement` | 1 | microsoft-outlook |
| WebKit.framework | `_OBJC_CLASS_$_DOMText` | 1 | microsoft-outlook |
| WebKit.framework | `_OBJC_CLASS_$_WKDownload` | 1 | duckduckgo |
| WebKit.framework | `_OBJC_CLASS_$_WKNavigationAction` | 1 | duckduckgo |
| WebKit.framework | `_OBJC_CLASS_$_WKNavigationResponse` | 1 | whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKScriptMessage` | 1 | duckduckgo |
| WebKit.framework | `_OBJC_CLASS_$_WKView` | 1 | ubersicht |
| WebKit.framework | `_OBJC_METACLASS_$_WKProcessPool` | 1 | bettertouchtool |
| WebKit.framework | `_OBJC_METACLASS_$_WKUserScript` | 1 | cloudflare-warp |
| WebKit.framework | `_OBJC_METACLASS_$_WebArchive` | 1 | chatgpt |
| WebKit.framework | `_WKInspectorDetach` | 1 | ubersicht |
| WebKit.framework | `_WKInspectorShowConsole` | 1 | ubersicht |
| WebKit.framework | `_WKPageGetInspector` | 1 | ubersicht |
| WebKit.framework | `_WebActionOriginalURLKey` | 1 | bettertouchtool |
| WebKit.framework | `_WebArchivePboardType` | 1 | microsoft-outlook |
| WebKit.framework | `_WebElementDOMNodeKey` | 1 | microsoft-outlook |
| WebKit.framework | `_WebKitErrorDomain` | 1 | zoom |
| WebKit.framework | `_WebViewProgressFinishedNotification` | 1 | adobe-acrobat-reader |
| libswiftWebKit.dylib | `_$sSo18WKWebsiteDataStoreC6WebKitE19proxyConfigurationsSay7Network18ProxyConfigurationVGvs` | 1 | cmux |
