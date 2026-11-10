# WebKit que os apps-alvo exigem (S15)

Electron usa Chromium. WebKit só entra quando um binário do bundle liga
`WebKit.framework` ou importa uma classe `WK*` / `WebView` / `WebArchive`. A leitura é a de
`tools/machocheck.c`: `LC_LOAD_DYLIB`, opcodes de bind e a tabela de imports de
`LC_DYLD_CHAINED_FIXUPS`, mais os seletores de `__objc_methname` nessas imagens.
Ferramenta: `tools/webkit_api_scan.py`.

Build e data da medição: 10 nov. 2026. Os binários não entram no repositório.

| App | Build medido | Liga WebKit? | Classes importadas de WebKit.framework | O que o mOS faz |
|---|---|---|---|---|
| Open Bot | 0.66.0 x64 | não (Electron) | — | nada a acrescentar |
| UsageBar | 0.8.4 (universal) | só o Sparkle embutido | `WKWebView`, `WKWebViewConfiguration`, `WKUserContentController`, `WKUserScript`, `WebView`, `WebPreferences` | já coberto; ver Sparkle abaixo |
| Usage for Claude & ChatGPT | 4.0, Mac App Store `id6755173244` | sem binário público | — | não dá para afirmar uma API. O site descreve login numa janela embutida; isso não é evidência de símbolo |
| Setapp | 4.0.0 (165), universal | sim, o executável | `WKWebView`, `WKWebViewConfiguration`, `WKUserContentController`, `WKUserScript` | classes já existiam. O executável também implementa o delegate de histórico (abaixo) |
| WhatsApp | 2.26.45.13, universal | sim (`WebKit` e o WebKit de iOSSupport) | `WKWebView`, `WKWebViewConfiguration`, `WKWebpagePreferences`, `WKPreferences`, `WKWebsiteDataStore`, `WKWebsiteDataRecord`, `WKHTTPCookieStore`, `WKNavigation`, `WKNavigationResponse`, `WKBackForwardListItem`, `WKUserContentController`, `WKUserScript`, `WKContentWorld`, `WKContentRuleList`, `WKContentRuleListStore`, `WKFrameInfo`, `WKSnapshotConfiguration`, `WebView`, `WebPreferences` | todas essas classes já existiam no framework |
| Grok Bot | 0.79.3 x64 | não (Electron) | — | nada a acrescentar |
| Cursor | 3.29.15 x64 | só `libmsalruntime_x64.dylib` (login Microsoft) | `WKWebView`, `WKWebViewConfiguration`, `WKWebsiteDataStore` | classes já existiam. O editor em si é Chromium |
| ChatGPT / Codex desktop | ChatGPT 26.1103.60933 x64 | Sparkle e `CUALockScreenGuardian` | no guardian: `WKWebView`, `WKNavigation`, **`WebArchive`**, **`WebResource`** | `WebArchive`/`WebResource` não existiam e são import forte: o helper não carregava. Implementados |
| Codex CLI | 0.167.0 x64 | não (binário Rust) | — | nada a acrescentar |
| Slack | 4.53.287 x64 | não | — | nada a acrescentar. Há uma string solta sobre "WebKit APIs on iOS" |
| VS Code | 1.145.2 x64 | só `libOneAuthInterop.dylib` | `WKWebView`, `WKWebViewConfiguration`, `WKWebsiteDataStore` | igual ao MSAL do Cursor |
| Claude | 4.23118.1 universal | só `libmsalruntime_arm64.dylib` | `WKWebView`, `WKWebViewConfiguration`, `WKWebsiteDataStore` | igual ao MSAL |
| Postico | 2.3.5 (9817), universal | sim, o executável | `WKWebView`, `WKWebViewConfiguration` | já coberto. O Sparkle do mesmo bundle traz `WebView` |

`WebMessageInfoReportingInfo`, visto no WhatsApp, não vem do WebKit.framework.

## O que foi implementado por causa dessa tabela

Funcional:

- `WebResource` e `WebArchive`. O plist usa as chaves da Apple (`WebMainResource`, `WebSubresources`,
  `WebSubframeArchives`, `WebResourceData`, `WebResourceURL`, `WebResourceMIMEType`,
  `WebResourceTextEncodingName`, `WebResourceFrameName`). `initWithData:` lê XML e bplist pelo
  `NSPropertyListSerialization` do mOS. `data` grava plist XML (formato 100), não o binário idêntico
  ao que o WebKit da Apple emitiria. `initWithData:` de lixo devolve `nil`.
- `webView:shouldGoToBackForwardListItem:willUseInstantBack:completionHandler:`, chamado por
  `goToBackForwardListItem:`. O terceiro argumento vai `NO` (não há instant back). Se o delegate
  recusa, a navegação não acontece. WhatsApp, Setapp, o MSAL (Cursor, VS Code, Claude) e o
  `CUALockScreenGuardian` implementam esse método.
- `WebFrame loadData:MIMEType:textEncodingName:baseURL:completionHandler:`. O Sparkle, dentro de
  UsageBar, Postico, Setapp, WhatsApp e ChatGPT, referencia esse seletor. Carrega como o
  `loadData` que já existia e chama o bloco com erro `nil`.

## Stub

Nada novo foi deixado como stub. As classes de fachada do libSystem (`vscode_fwclasses.c`) continuam
existindo para quem não carrega o framework; não foram alteradas.

## Limitação (o seletor está no binário; o framework não chama)

O motor não emite o evento, então chamar o delegate seria inventar uma resposta:

- `webView:didReceiveAuthenticationChallenge:completionHandler:` e
  `webView:authenticationChallenge:shouldAllowDeprecatedTLS:` (Setapp, WhatsApp, MSAL, guardian)
- `webView:navigationAction:didBecomeDownload:` e `webView:navigationResponse:didBecomeDownload:`
  (os mesmos). Não há classe `WKDownload`; ninguém a importa forte
- `webView:runOpenPanelWithParameters:initiatedByFrame:completionHandler:` (WhatsApp, MSAL)
- `webView:requestMediaCapturePermissionForOrigin:initiatedByFrame:type:decisionHandler:` e
  `webView:requestDeviceOrientationAndMotionPermissionForOrigin:initiatedByFrame:decisionHandler:` (WhatsApp, MSAL)
- `webView:requestGeolocationPermissionForOrigin:initiatedByFrame:decisionHandler:` e
  `webView:requestFullScreenPermissionForOrigin:initiatedByFrame:decisionHandler:` (WhatsApp)
- `webView:willSubmitForm:submissionHandler:` (Setapp, WhatsApp)
- os delegates de menu de contexto, pré-visualização e lockdown mode do WhatsApp
- os callbacks de `WebView` do Sparkle que o framework ainda não dispara (moldura, arraste, ícone,
  redirect de cliente, `windowScriptObjectAvailable:`). O `WebView` em si já existe e carrega HTML

Não há DRM nesse caminho. Nada disso foi contornado.
