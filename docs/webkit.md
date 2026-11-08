# WebKit.framework no mOS (WKWebView sobre o motor web do sistema)

O `WebKit.framework` do mOS (`userland/frameworks/WebKit/WebKit.m`) implementa a API do WebKit da Apple sobre o motor
web que já existia no kernel, o do Horizonte (`kernel/web`: HTML, CSS, layout, imagens e o JavaScript "Brisa"). Ele é
exposto aos apps pela syscall `SYS_WEB` (59, `kernel/web/webview.c`). O framework vai em toda imagem
(`build.ps1` o compila para `/System/Library/Frameworks/WebKit.framework/Versions/A/WebKit`).

## Arquitetura

```
app (WKWebView, delegates, KVO)          userland/frameworks/WebKit/WebKit.m
   │  SYS_WEB: NEW, LOAD, SIZE, PUMP, PAINT, INPUT, EVAL, ...   (kernel/include/syscall.h, MOS_WEB_*)
   ▼
vista do motor ("aba sem janela")         kernel/web/webview.c
   │  doc_parse / css / doc_layout / doc_paint / js_page_*     (kernel/web/*.c, o mesmo código do Horizonte)
   ▼
surface_t BGRA do tamanho da vista  ──PAINT──▶  buffer do app  ──cx_gfx_image_px──▶  janela (drawRect:)
```

- **Vistas por processo.** Cada `WKWebView` pede uma vista (`MOS_WEB_NEW`). A vista é do processo, e
  `webview_user_exit` (chamado em `process.c`) a libera quando ele sai.
- **Sem bloqueio.** Um `NSTimer` de 1/60 s chama `MOS_WEB_PUMP`, que avança a rede (`http_poll`), as folhas
  externas, as imagens e os temporizadores e scripts da página. Ele devolve os eventos acumulados (`MOS_WEB_EV_*`),
  que viram chamadas de delegate e notificações KVO.
- **Pintura.** `MOS_WEB_PAINT` refaz o quadro só quando algo mudou e copia o surface para o buffer do app. O
  `drawRect:` compõe esse buffer na posição em pixels da vista (`cx_gfx_view_xform` + `cx_gfx_image_px`,
  `appkit.c`). No modo transparente (`drawsBackground = NO`), o kernel desfaz a pré-multiplicação.
- **Entrada.** `mouseDown:`/`Up:`/`Dragged:`/`Moved:`, `scrollWheel:` e `keyDown:` viram `MOS_WEB_INPUT`. A vista
  tem um `ui_t` próprio, e os controles de formulário são os widgets do kernel (`ui_textfield`, `ui_button`,
  `ui_checkbox`...), pintados pelo gancho `web_paint_control_hook`. O `<select>` abre uma lista desenhada pela
  própria vista, porque o `ui_popup` precisa de uma janela do window server.
- **Pilha.** As threads de usuário têm 32 KiB de pilha de kernel, e o motor JS aceita até 600 KiB de recursão. Por
  isso o despacho da `SYS_WEB` roda numa pilha própria de 1,5 MiB (`on_big_stack`). A trava grande garante que só um
  núcleo está no motor por vez. A entrada usa `sysv_abi`, porque o kernel compila com a ABI da Microsoft.
- **Concorrência.** Toda syscall roda com a trava grande, e o laço do desktop também (ele só a solta no `hlt`).
  Então o estado global do motor (o `J` do JavaScript, as folhas, o heap) nunca é tocado por dois núcleos ao mesmo
  tempo.

## O que a API cobre

| Área | Como |
|---|---|
| Carregar | `loadRequest:` (GET e POST), `loadHTMLString:baseURL:`, `loadData:MIMEType:characterEncodingName:baseURL:`, `loadFileURL:allowingReadAccessToURL:`, `loadSimulatedRequest:…`, `about:`, `file:`, `data:`, `http(s):` |
| Navegação | `goBack`/`goForward`/`goToBackForwardListItem:`/`reload`/`stopLoading`, `canGoBack`/`canGoForward`, `WKBackForwardList` (o histórico guarda o HTML de `loadHTMLString`/`loadData`: voltar mostra a página de novo) |
| `WKNavigationDelegate` | `decidePolicyForNavigationAction` (as duas formas; toda navegação passa por ela: links com `WKNavigationTypeLinkActivated`, formulários, voltar/avançar, recarregar e scripts), `decidePolicyForNavigationResponse`, `didStartProvisional…`, `didReceiveServerRedirect…`, `didCommit…`, `didFinish…`, `didFailProvisional…`/`didFail…` com `NSURLErrorDomain` |
| `WKUIDelegate` | `createWebViewWithConfiguration:…` (links `target=_blank` e ⌘-clique), `runJavaScriptAlertPanel…`, `…ConfirmPanel…`, `…TextInputPanel…`, `webViewDidClose:` |
| JavaScript | `evaluateJavaScript:` (NSString, NSNumber, NSNull, NSDictionary, NSArray, NSDate; `WKErrorJavaScriptExceptionOccurred`/`…ResultTypeIsUnsupported`), `callAsyncJavaScript:arguments:…` (promessas), sempre assíncronos |
| Mensagens | `window.webkit.messageHandlers.<nome>.postMessage()` → `userContentController:didReceiveScriptMessage:`; `addScriptMessageHandlerWithReply:` (a promessa do JS resolve ou rejeita com a resposta) |
| Scripts | `WKUserScript` `AtDocumentStart` (antes dos scripts da página) e `AtDocumentEnd` (depois dos inline, antes do `load`) |
| Esquemas próprios | `setURLSchemeHandler:forURLScheme:` e `WKURLSchemeTask` para o documento principal (o mesmo nome reservado que o WebKit recusa lança exceção) |
| KVO | `title`, `URL`, `loading`, `estimatedProgress`, `canGoBack`, `canGoForward`, `hasOnlySecureContent` |
| Outros | `findString:withConfiguration:`, `takeSnapshotWithConfiguration:`, `pageZoom`, `magnification`, `drawsBackground`, `customUserAgent`, `WKWebsiteDataStore`, `WKHTTPCookieStore` (sobre o `NSHTTPCookieStorage`), `WKContentRuleListStore` (valida e guarda) |
| WebKit1 | `WebView` (subclasse do `WKWebView`), `WebFrame`, `WebDataSource`, `WebPreferences`, `stringByEvaluatingJavaScriptFromString:`, `frameLoadDelegate` e `policyDelegate` |

As classes de fachada `WKWebView`/`WKWebViewConfiguration`/`WKWebsiteDataStore` do libSystem
(`vscode_fwclasses.c`, para apps que só referenciam o WebKit) continuam existindo. Elas ficam marcadas como stub
(`cx_mark_stub_class`), e a busca por nome (`NSClassFromString`, nibs) prefere a do framework quando ele carrega.

## Limites conhecidos

- **Diálogos.** `alert`/`confirm`/`prompt` não param o script esperando o app. O delegate recebe a mensagem;
  `confirm` devolve `false` e `prompt` devolve `null` à página, como o WebKit sem `WKUIDelegate`.
- **O que o motor ainda não suporta.** Iframes, mídia (`<video>`/`<audio>`), WebGL/canvas 3D, SVG e WebP/AVIF,
  impressão e PDF ficam de fora (o motor do Horizonte não os tem). Os completions de PDF e impressão devolvem erro.
- **Um mundo de conteúdo.** `WKContentWorld` é aceito, mas tudo roda no mundo da página.
- **Esquemas próprios.** Servem a navegação principal. Sub-recursos (imagens, folhas e scripts em `app://`) ainda
  não passam pelo `WKURLSchemeHandler`.
- **Resposta depois do commit.** `decidePolicyForNavigationResponse` é chamado depois que o documento chegou. Um
  "cancelar" volta à página anterior.
- **Bloqueio de conteúdo.** As regras de conteúdo são validadas e guardadas, mas não bloqueiam nada.

## Teste

`userland/tests-webkit/main.m` é um app AppKit com um WKWebView numa janela. Ele cobre:

- navegação e ordem dos delegates;
- KVO;
- `messageHandlers`, com e sem resposta;
- `alert`;
- os tipos do `evaluateJavaScript` e a exceção;
- `callAsyncJavaScript`;
- os `WKUserScript`;
- pixels pintados;
- `findString`;
- um clique de verdade num link (`MOS_WEB_INPUT`);
- voltar a uma página de `loadHTMLString`;
- política cancelando;
- `WKURLSchemeHandler`;
- `loadData` `text/plain`;
- falha provisória;
- a `WebView` legada;
- `NSClassFromString`.

```powershell
.\build.ps1 -OutDir build-wk
.\tools\build-webkittest.ps1 -OutDir build-wk
.\run.ps1 -Headless -Image build-wk\mOS.img -Serial build-wk\serial.log    # procure "webkit:" no log
```
