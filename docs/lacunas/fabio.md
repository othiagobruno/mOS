# Lacunas de Fabio (WebKit/Electron)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Fabio (WebKit/Electron) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**33** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| WebKit.framework | `_OBJC_CLASS_$_WebView` | 11 | coteditor, iina, iterm2, keepingyouawake |
| WebKit.framework | `_OBJC_CLASS_$_WKUserContentController` | 9 | alt-tab, coteditor, iina, iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKUserScript` | 9 | alt-tab, coteditor, iina, iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WebPreferences` | 8 | coteditor, iina, keepingyouawake, monitorcontrol |
| WebKit.framework | `_OBJC_CLASS_$_WKContentRuleListStore` | 6 | alt-tab, coteditor, iina, iterm2 |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSContext` | 2 | iina, iterm2 |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSValue` | 2 | iina, iterm2 |
| JavaScriptCore.framework | `_JSContextGetGlobalObject` | 1 | macdown |
| JavaScriptCore.framework | `_JSEvaluateScript` | 1 | macdown |
| JavaScriptCore.framework | `_JSGlobalContextCreate` | 1 | macdown |
| JavaScriptCore.framework | `_JSGlobalContextRelease` | 1 | macdown |
| JavaScriptCore.framework | `_JSObjectGetProperty` | 1 | macdown |
| JavaScriptCore.framework | `_JSObjectMakeTypedArrayWithBytesNoCopy` | 1 | iina |
| JavaScriptCore.framework | `_JSStringCreateWithCFString` | 1 | macdown |
| JavaScriptCore.framework | `_JSStringCreateWithUTF8CString` | 1 | macdown |
| JavaScriptCore.framework | `_JSStringGetLength` | 1 | macdown |
| JavaScriptCore.framework | `_JSStringGetUTF8CString` | 1 | macdown |
| JavaScriptCore.framework | `_JSStringRelease` | 1 | macdown |
| JavaScriptCore.framework | `_JSValueCreateJSONString` | 1 | macdown |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSManagedValue` | 1 | iina |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariApplication` | 1 | iina |
| SafariServices.framework | `_OBJC_CLASS_$_SFSafariExtensionHandler` | 1 | iina |
| SafariServices.framework | `_OBJC_METACLASS_$_SFSafariExtensionHandler` | 1 | iina |
| WebKit.framework | `_OBJC_CLASS_$_DOMNode` | 1 | macdown |
| WebKit.framework | `_OBJC_CLASS_$_WKBackForwardListItem` | 1 | iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKContentWorld` | 1 | iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKFrameInfo` | 1 | iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKPreferences` | 1 | iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKProcessPool` | 1 | iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKSnapshotConfiguration` | 1 | iterm2 |
| WebKit.framework | `_WebActionNavigationTypeKey` | 1 | macdown |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKF` | 1 | iterm2 |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKFTu` | 1 | iterm2 |
