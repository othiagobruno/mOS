# Lacunas de Fabio (WebKit/Electron)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Fabio (WebKit/Electron) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**15** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| WebKit.framework | `_OBJC_CLASS_$_WKUserContentController` | 5 | alt-tab, iterm2, keepingyouawake, mos |
| WebKit.framework | `_OBJC_CLASS_$_WKUserScript` | 5 | alt-tab, iterm2, keepingyouawake, mos |
| WebKit.framework | `_OBJC_CLASS_$_WebView` | 5 | iterm2, keepingyouawake, mos, rectangle |
| WebKit.framework | `_OBJC_CLASS_$_WKContentRuleListStore` | 3 | alt-tab, iterm2, rectangle |
| WebKit.framework | `_OBJC_CLASS_$_WebPreferences` | 3 | keepingyouawake, mos, rectangle |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSContext` | 1 | iterm2 |
| JavaScriptCore.framework | `_OBJC_CLASS_$_JSValue` | 1 | iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKBackForwardListItem` | 1 | iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKContentWorld` | 1 | iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKFrameInfo` | 1 | iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKPreferences` | 1 | iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKProcessPool` | 1 | iterm2 |
| WebKit.framework | `_OBJC_CLASS_$_WKSnapshotConfiguration` | 1 | iterm2 |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKF` | 1 | iterm2 |
| libswiftWebKit.dylib | `_$sSo9WKWebViewC6WebKitE18evaluateJavaScript_2in12contentWorldypSgSS_So11WKFrameInfoCSgSo09WKContentJ0CtYaKFTu` | 1 | iterm2 |
