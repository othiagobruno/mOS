# Lacunas de Fabio (WebKit/Electron)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Fabio (WebKit/Electron) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**74** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| WebKit.framework | `_OBJC_CLASS_$_WKUserScript` | 37 | alt-tab, appcleaner, betterdisplay, chatgpt |
| WebKit.framework | `_OBJC_CLASS_$_WebView` | 37 | appcleaner, betterdisplay, brave-browser, chatgpt |
| WebKit.framework | `_OBJC_CLASS_$_WKUserContentController` | 32 | alt-tab, appcleaner, betterdisplay, chatgpt |
| WebKit.framework | `_OBJC_CLASS_$_WebPreferences` | 30 | appcleaner, betterdisplay, chatgpt, clipy |
| WebKit.framework | `_OBJC_CLASS_$_WKContentRuleListStore` | 19 | alt-tab, appcleaner, chatgpt, clipy |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSContext` | 8 | cmux, iina, iterm2, karabiner-elements |
| WebKit.framework | `_OBJC_CLASS_$_WKContentWorld` | 7 | cmux, drawio, iterm2, obsidian |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSValue` | 6 | cmux, iina, iterm2, karabiner-elements |
| WebKit.framework | `_OBJC_CLASS_$_WKSnapshotConfiguration` | 6 | cmux, iterm2, openclaw, telegram |
| WebKit.framework | `_OBJC_CLASS_$_WKPreferences` | 5 | cmux, hammerspoon, iterm2, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKBackForwardListItem` | 4 | cmux, iterm2, telegram, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKProcessPool` | 4 | hammerspoon, iterm2, telegram, zoom |
| WebKit.framework | `_WKErrorDomain` | 4 | cmux, drawio, whatsapp, zoom |
| WebKit.framework | `_OBJC_CLASS_$_WKFrameInfo` | 3 | cmux, iterm2, whatsapp |
| JavaScriptCore.framework | `_JSContextGetGroup` | 2 | cmux, telegram |
| JavaScriptCore.framework | `_JSEvaluateScript` | 2 | macdown, zoom |
| JavaScriptCore.framework | `_JSGlobalContextCreate` | 2 | macdown, zoom |
| JavaScriptCore.framework | `_JSGlobalContextRelease` | 2 | macdown, zoom |
| JavaScriptCore.framework | `_JSStringCreateWithUTF8CString` | 2 | macdown, zoom |
| JavaScriptCore.framework | `_JSStringGetUTF8CString` | 2 | macdown, zoom |
| JavaScriptCore.framework | `_JSStringRelease` | 2 | macdown, zoom |
| JavaScriptCore.framework | `_JSValueToObject` | 2 | telegram, zoom |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariApplication` | 2 | bitwarden, iina |
| WebKit.framework | `_OBJC_CLASS_$_WKHTTPCookieStore` | 2 | cmux, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKNavigation` | 2 | chatgpt, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WebArchive` | 2 | chatgpt, keka |
| WebKit.framework | `_WKWebsiteDataTypeServiceWorkerRegistrations` | 2 | openclaw, zoom |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKF` | 2 | cmux, iterm2 |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKFTu` | 2 | cmux, iterm2 |
| JavaScriptCore.framework | `_JSContextGetGlobalObject` | 1 | macdown |
| JavaScriptCore.framework | `_JSObjectCallAsFunction` | 1 | zoom |
| JavaScriptCore.framework | `_JSObjectGetProperty` | 1 | macdown |
| JavaScriptCore.framework | `_JSObjectGetTypedArrayByteLength` | 1 | telegram |
| JavaScriptCore.framework | `_JSObjectGetTypedArrayBytesPtr` | 1 | telegram |
| JavaScriptCore.framework | `_JSObjectMakeTypedArrayWithBytesNoCopy` | 1 | iina |
| JavaScriptCore.framework | `_JSStringCreateWithCFString` | 1 | macdown |
| JavaScriptCore.framework | `_JSStringGetLength` | 1 | macdown |
| JavaScriptCore.framework | `_JSStringGetMaximumUTF8CStringSize` | 1 | zoom |
| JavaScriptCore.framework | `_JSValueCreateJSONString` | 1 | macdown |
| JavaScriptCore.framework | `_JSValueGetTypedArrayType` | 1 | telegram |
| JavaScriptCore.framework | `_JSValueMakeString` | 1 | zoom |
| JavaScriptCore.framework | `_JSValueToStringCopy` | 1 | zoom |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSManagedValue` | 1 | iina |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSVirtualMachine` | 1 | telegram |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariExtensionHandler` | 1 | iina |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariViewController` | 1 | whatsapp |
| SafariServices.framework | `_OBJC_CLASS_$_SSReadingList` | 1 | whatsapp |
| SafariServices.framework | `_OBJC_METACLASS_$_SFSafariExtensionHandler` | 1 | iina |
| WebKit.framework | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKF` | 1 | openclaw |
| WebKit.framework | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKFTu` | 1 | openclaw |
| WebKit.framework | `_OBJC_CLASS_$_DOMNode` | 1 | macdown |
| WebKit.framework | `_OBJC_CLASS_$_WKContentRuleList` | 1 | whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKFindConfiguration` | 1 | sequel-ace |
| WebKit.framework | `_OBJC_CLASS_$_WKNavigationResponse` | 1 | whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKPDFConfiguration` | 1 | cmux |
| WebKit.framework | `_OBJC_CLASS_$_WKWebpagePreferences` | 1 | whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKWebsiteDataRecord` | 1 | whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WebResource` | 1 | chatgpt |
| WebKit.framework | `_OBJC_METACLASS_$_WKUserContentController` | 1 | hammerspoon |
| WebKit.framework | `_OBJC_METACLASS_$_WebArchive` | 1 | chatgpt |
| WebKit.framework | `_WKWebsiteDataTypeDiskCache` | 1 | telegram |
| WebKit.framework | `_WKWebsiteDataTypeFetchCache` | 1 | zoom |
| WebKit.framework | `_WKWebsiteDataTypeIndexedDBDatabases` | 1 | zoom |
| WebKit.framework | `_WKWebsiteDataTypeLocalStorage` | 1 | zoom |
| WebKit.framework | `_WKWebsiteDataTypeMemoryCache` | 1 | telegram |
| WebKit.framework | `_WKWebsiteDataTypeSessionStorage` | 1 | zoom |
| WebKit.framework | `_WKWebsiteDataTypeWebSQLDatabases` | 1 | zoom |
| WebKit.framework | `_WebActionElementKey` | 1 | zoom |
| WebKit.framework | `_WebActionNavigationTypeKey` | 1 | macdown |
| WebKit.framework | `_WebKitErrorDomain` | 1 | zoom |
| libswiftWebKit.dylib | `_$sSo18WKWebsiteDataStoreC6WebKitE19proxyConfigurationsSay7Network18ProxyConfigurationVGvs` | 1 | cmux |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE3pdf13configuration10Foundation4DataVSo18WKPDFConfigurationC_tYaKF` | 1 | cmux |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE3pdf13configuration10Foundation4DataVSo18WKPDFConfigurationC_tYaKFTu` | 1 | cmux |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE4find_13configuration17completionHandlerySS_So19WKFindConfigurationCySo0I6ResultCctF` | 1 | sequel-ace |
