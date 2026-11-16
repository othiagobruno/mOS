# Lacunas de Fabio (WebKit/Electron)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Fabio (WebKit/Electron) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**66** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| WebKit.framework | `_OBJC_CLASS_$_WKUserScript` | 30 | alt-tab, appcleaner, betterdisplay, chatgpt |
| WebKit.framework | `_OBJC_CLASS_$_WebView` | 30 | appcleaner, betterdisplay, brave-browser, chatgpt |
| WebKit.framework | `_OBJC_CLASS_$_WKUserContentController` | 25 | alt-tab, appcleaner, betterdisplay, chatgpt |
| WebKit.framework | `_OBJC_CLASS_$_WebPreferences` | 24 | appcleaner, betterdisplay, chatgpt, clipy |
| WebKit.framework | `_OBJC_CLASS_$_WKContentRuleListStore` | 16 | alt-tab, appcleaner, chatgpt, clipy |
| WebKit.framework | `_OBJC_CLASS_$_WKContentWorld` | 6 | cmux, drawio, iterm2, obsidian |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSContext` | 5 | cmux, iina, iterm2, raycast |
| WebKit.framework | `_OBJC_CLASS_$_WKPreferences` | 5 | cmux, hammerspoon, iterm2, whatsapp |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSValue` | 4 | cmux, iina, iterm2, raycast |
| WebKit.framework | `_OBJC_CLASS_$_WKSnapshotConfiguration` | 4 | cmux, iterm2, whatsapp, zoom |
| WebKit.framework | `_WKErrorDomain` | 4 | cmux, drawio, whatsapp, zoom |
| WebKit.framework | `_OBJC_CLASS_$_WKBackForwardListItem` | 3 | cmux, iterm2, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKFrameInfo` | 3 | cmux, iterm2, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKProcessPool` | 3 | hammerspoon, iterm2, zoom |
| JavaScriptCore.framework | `_JSEvaluateScript` | 2 | macdown, zoom |
| JavaScriptCore.framework | `_JSGlobalContextCreate` | 2 | macdown, zoom |
| JavaScriptCore.framework | `_JSGlobalContextRelease` | 2 | macdown, zoom |
| JavaScriptCore.framework | `_JSStringCreateWithUTF8CString` | 2 | macdown, zoom |
| JavaScriptCore.framework | `_JSStringGetUTF8CString` | 2 | macdown, zoom |
| JavaScriptCore.framework | `_JSStringRelease` | 2 | macdown, zoom |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariApplication` | 2 | bitwarden, iina |
| WebKit.framework | `_OBJC_CLASS_$_WKHTTPCookieStore` | 2 | cmux, whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKNavigation` | 2 | chatgpt, whatsapp |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKF` | 2 | cmux, iterm2 |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKFTu` | 2 | cmux, iterm2 |
| JavaScriptCore.framework | `_JSContextGetGlobalObject` | 1 | macdown |
| JavaScriptCore.framework | `_JSContextGetGroup` | 1 | cmux |
| JavaScriptCore.framework | `_JSObjectCallAsFunction` | 1 | zoom |
| JavaScriptCore.framework | `_JSObjectGetProperty` | 1 | macdown |
| JavaScriptCore.framework | `_JSObjectMakeTypedArrayWithBytesNoCopy` | 1 | iina |
| JavaScriptCore.framework | `_JSStringCreateWithCFString` | 1 | macdown |
| JavaScriptCore.framework | `_JSStringGetLength` | 1 | macdown |
| JavaScriptCore.framework | `_JSStringGetMaximumUTF8CStringSize` | 1 | zoom |
| JavaScriptCore.framework | `_JSValueCreateJSONString` | 1 | macdown |
| JavaScriptCore.framework | `_JSValueMakeString` | 1 | zoom |
| JavaScriptCore.framework | `_JSValueToObject` | 1 | zoom |
| JavaScriptCore.framework | `_JSValueToStringCopy` | 1 | zoom |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSManagedValue` | 1 | iina |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariExtensionHandler` | 1 | iina |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariViewController` | 1 | whatsapp |
| SafariServices.framework | `_OBJC_CLASS_$_SSReadingList` | 1 | whatsapp |
| SafariServices.framework | `_OBJC_METACLASS_$_SFSafariExtensionHandler` | 1 | iina |
| WebKit.framework | `_OBJC_CLASS_$_DOMNode` | 1 | macdown |
| WebKit.framework | `_OBJC_CLASS_$_WKContentRuleList` | 1 | whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKFindConfiguration` | 1 | sequel-ace |
| WebKit.framework | `_OBJC_CLASS_$_WKNavigationResponse` | 1 | whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKPDFConfiguration` | 1 | cmux |
| WebKit.framework | `_OBJC_CLASS_$_WKWebpagePreferences` | 1 | whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WKWebsiteDataRecord` | 1 | whatsapp |
| WebKit.framework | `_OBJC_CLASS_$_WebArchive` | 1 | chatgpt |
| WebKit.framework | `_OBJC_CLASS_$_WebResource` | 1 | chatgpt |
| WebKit.framework | `_OBJC_METACLASS_$_WKUserContentController` | 1 | hammerspoon |
| WebKit.framework | `_OBJC_METACLASS_$_WebArchive` | 1 | chatgpt |
| WebKit.framework | `_WKWebsiteDataTypeFetchCache` | 1 | zoom |
| WebKit.framework | `_WKWebsiteDataTypeIndexedDBDatabases` | 1 | zoom |
| WebKit.framework | `_WKWebsiteDataTypeLocalStorage` | 1 | zoom |
| WebKit.framework | `_WKWebsiteDataTypeServiceWorkerRegistrations` | 1 | zoom |
| WebKit.framework | `_WKWebsiteDataTypeSessionStorage` | 1 | zoom |
| WebKit.framework | `_WKWebsiteDataTypeWebSQLDatabases` | 1 | zoom |
| WebKit.framework | `_WebActionElementKey` | 1 | zoom |
| WebKit.framework | `_WebActionNavigationTypeKey` | 1 | macdown |
| WebKit.framework | `_WebKitErrorDomain` | 1 | zoom |
| libswiftWebKit.dylib | `_$sSo18WKWebsiteDataStoreC6WebKitE19proxyConfigurationsSay7Network18ProxyConfigurationVGvs` | 1 | cmux |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE3pdf13configuration10Foundation4DataVSo18WKPDFConfigurationC_tYaKF` | 1 | cmux |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE3pdf13configuration10Foundation4DataVSo18WKPDFConfigurationC_tYaKFTu` | 1 | cmux |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE4find_13configuration17completionHandlerySS_So19WKFindConfigurationCySo0I6ResultCctF` | 1 | sequel-ace |
