# Frente Web e Electron — plano e estado (dono: Fabio)

Escopo (`/home/thiago/OpenBot/Shared/mos-time/TIME.md`, frente 18): WebKit/WKWebView completo, JavaScriptCore e o
runtime Electron/Chromium, com VS Code, Slack, Discord, Chrome, Notion e Figma abrindo e usáveis.

Este arquivo diz o que está pronto (com o teste que prova) e o que falta. Atualizado a cada PR da frente.

## Ponto de partida (15 nov. 2026, levantado na `main` 6a8ed98)

| Área | Onde | Estado medido |
|---|---|---|
| WKWebView / WebKit1 | `userland/frameworks/WebKit/WebKit.m` (2257 linhas) sobre `SYS_WEB` | API ampla ([webkit.md](webkit.md)); 55/55 no QEMU antes do PR #24 |
| Motor web | `kernel/web` (~22,7 mil linhas: HTML, CSS, layout, imagens PNG/JPEG/GIF/BMP, JS "Brisa") | Sem iframe, mídia, canvas/WebGL, SVG, WebP/AVIF, PDF |
| Diálogos JS | `kernel/web/webview.c` | `alert/confirm/prompt` não bloqueiam; `confirm` → false, `prompt` → null |
| JavaScriptCore.framework | — | **Não existe.** Nenhuma API `JSContext`/`JSValue`/`JSGlobalContextCreate` no mOS |
| Chromium/Electron | `docs/porte-chrome.md`, `docs/porte-vscode.md`, `docs/navegador-chromium.md` | Chrome 157 e Chromium navegam com HTTPS; VS Code abriu com terminal; Slack com 32 imports ausentes |
| Teste de host do motor | — | **Não havia.** Só o `tests-webkit` dentro da VM |

## Lacunas e ordem de ataque

Cada item fecha com teste de host (`tools/web_host.sh`) ou, quando depende da VM, com caso novo no
`userland/tests-webkit` para o build final.

1. ✅ **Harness de host do motor** — `tools/web_host.sh` compila `kernel/web` + `kernel/gfx` no Linux e roda
   HTML → layout → pixels e JavaScript (`tools/web_host.c`).
2. ✅ **WebP** — `kernel/web/webp.c` (VP8L, VP8 com filtro de laço, ALPH, VP8X, ANMF: 1º quadro), ligado ao
   `image_decode`; o `webview.c` e o Horizonte não recusam mais `.webp`. Teste: `tools/web_host_webp.c` compara com o
   libwebp (Pillow) **bit a bit** em 13 arquivos (sem perdas, paletas de 2/4/16/200 cores, alfa, q5–q90, método 6,
   animação) e em 1280×853 nos três modos; entradas truncadas e corrompidas não travam. `webp_decode_argb` serve ao
   ImageIO. Animação: só o 1º quadro (o motor não anima GIF nem WebP).
2b. ✅ **Imagens responsivas** — `<picture><source type media srcset>`, `srcset` com descritores `x` e `w`,
   `data-srcset`; `type` sem decodificador (AVIF, JPEG XL) cai para o próximo candidato (`img_pick_src`, `layout.c`).
3. ✅ **SVG** — `kernel/web/svg.c` (parser XML próprio, rasterizador de cobertura com 8 subamostras): `path` com
   todos os comandos, formas básicas, `transform`, `viewBox`/`preserveAspectRatio`, `<svg>` aninhado, não-zero e
   par-ímpar, traço (junções, tampas, `miterlimit`, tracejado), gradientes linear/radial (foco, units, transform,
   spread, `href`), `<style>` com seletores simples, `currentColor` (herda a cor CSS da página no inline),
   `<use>`/`<symbol>` (inclusive sprites definidos em outro `<svg>` da página), `clipPath`, opacidade de grupo em
   camada, `<text>`/`<tspan>`, `<image>` com `data:`. Ligado a `<svg>` inline (elemento substituído), `<img src=.svg>`,
   `<picture type=image/svg+xml>` e `data:`; o vetor é redesenhado no tamanho exibido. Teste: `tools/web_host_svg.c`
   (10 casos; ASan/UBSan limpos com `XCFLAGS`).
   **Falta no SVG:** `<mask>`, `<pattern>`, `<marker>`, filtros, `<textPath>`, texto girado (só translação/escala),
   SMIL/animação e os filhos do `<svg>` inline no DOM do JavaScript (o motor guarda o SVG como fonte).
4. ✅ **Iframes** (parte 1) — `kernel/web/frame.c`: `<iframe src>` (http, file, data:, about:blank) e `srcdoc`,
   documento com CSS externo, imagens e página JS próprios; pintura dentro da caixa (300×150 padrão, borda UA),
   fundo transparente sem cor declarada; `parent`/`top`/`frameElement`, `contentWindow`/`contentDocument` (mesma
   origem; about:/srcdoc herdam a origem), `postMessage` nos dois sentidos com `targetOrigin`, `event.source`/`origin`,
   evento `load` no `<iframe>`, `src`/`srcdoc` trocados pelo JS, links navegam o iframe (`_top`/`_blank` vão ao
   hospedeiro), rodinha rola o iframe, aninhamento até 4 níveis. Ligado ao WKWebView e ao Horizonte. `doc_hit_atom`
   (declarado e nunca implementado) agora existe. Teste: `tools/web_host_iframe.c` (3 casos).
   ✅ **Parte 2: formulários no iframe** — cada documento de iframe tem a sua faixa de ids de controle
   (`ctrl_base`), então foco, digitação e cliques dos controles do iframe não se misturam com os do pai; eventos
   `input`/`change`/`keydown`/`submit` vão para a página JS do iframe; o envio (GET e POST, e `form.submit()` do
   filho) navega só o iframe; `preventDefault` no clique do botão cancela o envio. A montagem do envio
   (`form_build`, `frame.c`) é a mesma do documento principal. Teste: `iframe_formularios`.
   **Falta nos iframes:** cursor de link sobre o iframe, `sandbox`/`allow`, `window.frames[i]`/`length`, `history`
   do iframe e o `WKFrameInfo` dos delegates.
5. ✅ **Diálogos JS bloqueantes** — `kernel/web/costack.c` (corrotina com troca `sysv_abi`, a mesma no kernel e
   no host), `js_exec_suspend`/`js_exec_resume` (`js_core.c`: estado de execução do motor guardado, pilhas
   suspensas varridas pelo coletor, inclusive dentro de async/gerador), `MOS_WEB_DIALOG`/`MOS_WEB_EVALDONE`
   (`webview.c`) e o WKUIDelegate/WebUIDelegate no `WebKit.m`. Os iframes herdam o gancho do pai. Teste de host:
   `tools/web_host_dialog.c` (4 casos: confirm/prompt/alert em sequência com outra página rodando JS e o coletor
   no meio, página fechada, async e geradores). As corrotinas do motor (`js_gen.c`) passaram a rodar também no
   host (`sysv_abi`), então geradores e async agora têm teste. Na VM: caso 14b do `tests-webkit` (build final).
5b. ✅ **ArrayBuffer, typed arrays, DataView, TextEncoder/TextDecoder** no motor (`kernel/web/js_typed.c`): os 11
   tipos (inclusive Uint8Clamped e BigInt64/BigUint64), construtores por tamanho/buffer/vetor/iterável, `from`/`of`,
   `set`, `subarray` (mesma memória), `slice`, `fill`, `map`/`filter`/`sort` do mesmo tipo, iteradores, métodos de
   leitura do Array, índices inteiros no núcleo (`js_core.c`), DataView com as duas ordens de bytes, UTF-8 do WHATWG
   no TextDecoder (`fatal`, BOM). Base do JavaScriptCore (`JSObjectMakeTypedArrayWithBytesNoCopy`) e do canvas.
   Teste: `tools/web_host_typed.c` (33 expressões).
6. ✅ **Canvas 2D** (`kernel/web/canvas.c` + `canvas_js.c`, rasterizador comum com o SVG em `raster.c`): `<canvas>`
   como elemento substituído (300×150 padrão, reserva não pintada, width/height recriam o bitmap), contexto 2D
   completo (estado com save/restore/reset, transformações e `DOMMatrix`, caminhos com arc/arcTo/ellipse/roundRect,
   `Path2D` com texto SVG e `addPath`, fill/stroke/clip com nonzero/evenodd, `isPointInPath`/`isPointInStroke`,
   tracejado, tampas e junções, gradientes linear/radial de dois círculos/cônico, padrões com repetição e
   `setTransform`, 26 operações de composição (Porter-Duff e modos de mistura), `globalAlpha`, sombra com desfoque
   gaussiano, `drawImage` de `<img>`/`<canvas>` com suavização e filtro de área na redução, texto com fonte CSS,
   alinhamento, linha de base, `maxWidth`, `letterSpacing` e transformação, `strokeText`, `measureText` completo,
   `ImageData`/`getImageData`/`putImageData`, `toDataURL` em PNG com deflate próprio, `DOMException`).
   Teste: `tools/web_host_canvas.c` (9 casos, 105 verificações; ASan/UBSan limpos). Falta: `OffscreenCanvas` e
   `filter`.
6b. ✅ **HTMLImageElement no JS** (`js_dom.c`, `layout.c`): `new Image()`, imagem pedida pelo `src`/`srcset` mesmo
   fora do documento, eventos load/error para cada elemento (propriedade, `addEventListener` e atributo), `complete`,
   `naturalWidth/Height`, `width/height` (exibido, atributo ou natural), `currentSrc`, `srcset`/`sizes`/
   `crossOrigin`/`decoding`/`loading`, `decode()`, `src=""` como imagem quebrada, e o `load` da janela esperando as
   imagens do documento. Teste: `tools/web_host_img.c` (4 casos, 19 verificações).
6c. ✅ **Blob, File, FileReader, URL e URLSearchParams** (`web_blob.c`, `web_url.c`; fora do prefixo `js_` para não
   entrar no JavaScriptCore.framework, que não tem DOM): Blob com partes de texto/ArrayBuffer/views/Blob, `slice`,
   `text`/`arrayBuffer`/`bytes`; File com nome e `lastModified`; FileReader com os quatro modos, codificação, eventos
   em tarefa e `abort`; `URL.createObjectURL`/`revokeObjectURL` com `blob:` valendo em `<img>` e `fetch`;
   `Response.arrayBuffer()`/`blob()`; `canvas.toBlob`; `createImageBitmap` (canvas, `<img>`, Blob, ImageData,
   ImageBitmap; recorte, `resizeWidth/Height`, `flipY`) e `ImageBitmap.close`. URL do WHATWG: esquemas especiais e
   opacos, credenciais, IPv4 (hexa/octal), IPv6 comprimido, punycode, `.`/`..`, conjuntos de escape, resolução
   relativa, `origin`, setters, `canParse`/`parse`; URLSearchParams completo e ligado ao `URL.searchParams`.
   Teste: `tools/web_host_blob.c`.
6d. ✅ **Fetch, XMLHttpRequest e DOMParser** (`web_fetch.c`; cliente HTTP do kernel com método e cabeçalhos livres e
   cabeçalhos da resposta em `http_request`): Headers (guarda, combinação, `getSetCookie`, Set-Cookie oculto),
   Request e Response completos com corpo binário, `clone`, `Response.json/error/redirect`, FormData (de `<form>`,
   multipart de ida e volta), fetch com http(s), data:, blob:, file:, redirect follow/error/manual, AbortController/
   AbortSignal (`abort`, `timeout`, `any`), XMLHttpRequest com eventos de progresso, `upload`, `responseType`
   (json, arraybuffer, blob, document), cabeçalhos, síncrono, `abort` e `timeout`; DOMParser.
   Teste: `tools/web_host_fetch.c` (6 casos). Falta: `Response.body`/ReadableStream (Streams).
6e. ✅ **structuredClone, MessageChannel e crypto** (`web_plat.c`, `web_crypto.c`): clone estruturado com ciclos, Map,
   Set, Date, RegExp, buffers e views, erros, Blob/File, ImageData e `transfer` (também no `postMessage` entre
   janelas); MessageChannel/MessagePort; `getRandomValues`/`randomUUID`; `crypto.subtle` sobre a criptografia do
   kernel: digest SHA-1/256/384/512, HMAC, AES-GCM, AES-CTR, PBKDF2, HKDF, ECDSA e ECDH P-256/P-384, Ed25519,
   X25519, verificação RSASSA-PKCS1-v1_5 e RSA-PSS, formatos raw/jwk/spki/pkcs8, wrapKey/unwrapKey.
   Teste: `tools/web_host_plat.c` (vetores FIPS 180, RFC 2202/4231/6070/5869/8032, NIST SP 800-38A e OpenSSL).
   Falta: geração e assinatura RSA, AES-CBC e AES-KW (o kernel só tem a cifra direta do AES).
6f. ✅ **Observadores** (`web_obs.c`): MutationObserver (childList com um registro por innerHTML/textContent,
   attributes com oldValue e attributeFilter, characterData, subtree, microtarefa, takeRecords, disconnect),
   ResizeObserver (content-box/border-box, só quando muda) e IntersectionObserver (viewport ou elemento raiz,
   rootMargin, limiares, rolagem). Teste: `tools/web_host_obs.c`. Falta: registro transitório de subárvores
   removidas e recorte por overflow dos ancestrais no IntersectionObserver.
6g. ✅ **Web Workers e BroadcastChannel** (`web_worker.c`): cada Worker é uma página JS própria sem DOM
   (DedicatedWorkerGlobalScope: self, postMessage, onmessage, close, importScripts, name, WorkerLocation), rodada no
   tick da dona; mensagens por clone estruturado; blob:/data:/file:/http; workers clássicos e de módulo; erros e 404
   viram evento error na dona; terminate; workers aninhados; BroadcastChannel entre páginas e workers da mesma
   origem. Teste: `tools/web_host_worker.c`. Falta: SharedWorker e ServiceWorker.
6h. ✅ **Streams** (`web_stream.js` embutido por `tools/gen_stream_js.py` em `web_stream_js.h`, ligado por
   `web_stream.c`, carregado na primeira vez que a página usa os nomes): ReadableStream (padrão e bytes, leitores
   padrão e BYOB, tee, pipeTo/pipeThrough com AbortSignal, iteração assíncrona, from), WritableStream com
   contrapressão, TransformStream, estratégias, TextEncoderStream/TextDecoderStream, Response/Request.body (inclusive
   corpo em stream no construtor, no fetch e no clone) e Blob.stream(). No núcleo JS: IteratorClose no for-of e no
   for await, e surrogates soltos que se juntam ao concatenar. Teste: `tools/web_host_stream.c`.
   Falta: CompressionStream/DecompressionStream.
6i. ✅ **SVG, segunda parte** (`svg.c`): `<pattern>` (unidades, viewBox, patternTransform, href), `<mask>`
   (luminância e alpha, região, unidades), filtros (feGaussianBlur, feOffset, feFlood, feColorMatrix,
   feComponentTransfer, feComposite com arithmetic, feMerge, feBlend, feMorphology, feDropShadow; região e
   primitiveUnits), `<marker>` (start/mid/end, orient auto e auto-start-reverse, markerUnits, viewBox, refX/refY),
   texto por glifo com a matriz inteira (girado, rotate, gradiente e padrão no texto) e `<textPath>`; conteúdo de
   marcador, padrão e máscara herda dos próprios ancestrais. Teste: casos svg_padrao_mascara, svg_filtros e
   svg_marcadores_texto em `tools/web_host_svg.c`. Falta: feTurbulence, feImage, feTile, feConvolveMatrix,
   feDisplacementMap e iluminação; sub-região de primitivo; recorte do marcador.
6j. ✅ **SVG inline no DOM** (`html.c`, `layout.c`, `js_dom.c`): os filhos do `<svg>` viram nós (XML, com a caixa dos
   nomes e atributos), com `namespaceURI`, `tagName` como no navegador, `createElementNS` e `innerHTML` em XML;
   qualquer mudança pelo JS re-serializa a subárvore e redesenha. Teste: caso svg_dom_js em `tools/web_host_svg.c`.
7. **WebGL 1/2** sobre o OpenGL do mOS (frente do Otavio) com reserva por software.
8. **`<video>`/`<audio>`**: o motor publica o elemento; o `WebKit.framework` decodifica com AVFoundation/
   VideoToolbox (frente da Isabela) e devolve quadros e relógio ao motor; controles nativos desenhados pela vista.
9. **AVIF** (AV1 intra + `av1C`/`ispe`/`pixi`/`alpha`) — junto da Isabela, que é dona do AV1.
10. **PDF**: `createPDFWithConfiguration`/impressão (gerar PDF da pintura) e exibir PDF na vista (sobre o PDFKit/CG
    da Mariana).
11. **JavaScriptCore.framework** — ✅ **API C** (`userland/frameworks/JavaScriptCore/jsc_api.c`): o motor do mOS
    (`kernel/web/js_*.c` sem o DOM, mais `util.c`) no processo do app, atrás de uma trava recursiva; grupos e
    contextos (cada contexto é um reino, inclusive com classe do objeto global), `JSEvaluateScript` (com `this`
    próprio e microtarefas), `JSCheckScriptSyntax`, `JSGarbageCollect`, todo o `JSValue*` (tipos, igualdades,
    `instanceof`, JSON, conversões, `Protect`), `JSString*` em UTF-16, `JSObject*` (propriedades por nome, índice e
    chave, protótipo, chamada e construção, `MakeFunction`/`WithCallback`/`Constructor`/`Array`/`Date`/`Error`/
    `RegExp`, nomes de propriedades), `JSClass` com valores e funções estáticas e os callbacks (`initialize`,
    `finalize`, `hasProperty`, `get/set/deleteProperty`, `getPropertyNames`, `callAsFunction`,
    `callAsConstructor`), e typed arrays (inclusive `...WithBytesNoCopy` com desalocador). `JSValueRef` é o valor
    do motor com XOR (o número 0 não vira NULL) e o coletor varre a pilha da thread inteira, como o do JSC.
    Teste de host: `tools/jsc_host.sh` (34 verificações, inclusive o que o MacDown e o IINA chamam); o framework
    liga para x86_64-macos e entra no `build.sh`/`build.ps1`.
    ✅ **API ObjC** (`JSC.m`): `JSVirtualMachine` (referências gerenciadas), `JSContext` (subscritos,
    `evaluateScript`, `exception`/`exceptionHandler`, `globalObject`, `name`, `currentContext`/`Callee`/`This`/
    `Arguments`, `contextWithJSGlobalContextRef:`), `JSValue` (todos os `valueWith…`, `to…`, `is…`, `isEqual…`,
    `isInstanceOf:`, `call`/`construct`/`invokeMethod`, propriedades por chave e índice, `defineProperty`),
    `JSManagedValue`; conversões NSString/NSNumber/NSDate/NSArray/NSDictionary/NSNull nos dois sentidos, blocos
    ObjC como funções JS (pela assinatura do bloco), `JSExport` com métodos, propriedades (de objeto e numéricas,
    somente leitura respeitada) e `JSExportAs`. Chamadas nativas pelo protótipo de 6 inteiros + 8 FP (vale na
    System V e no AAPCS64); structs por valor e mais de 6 inteiros lançam TypeError. Compila e liga para
    x86_64-macos; teste na VM: `userland/tests-jsc` (`tools/build-jsctest.sh`, 12 verificações, roda no build final).
    **Falta no JSC:** `hasInstance` e `convertToType` dos `JSClass`, structs (CGRect/NSRange) nas conversões ObjC,
    o slice arm64 e o `JSScript`/módulos.
12. **Mundos de conteúdo** (`WKContentWorld` isolado), esquemas próprios para sub-recursos, regras de conteúdo
    que bloqueiam, `WKDownload`, desafio de autenticação e painel de arquivo.
13. **Electron/Chromium**: renderer e helpers de Cursor, Slack, Discord, Notion, Figma, Claude e ChatGPT abrindo;
    lacunas de libSystem medidas pelo `machocheck` repassadas aos donos (Leonardo, Eduardo, Beatriz, Natalia).

14. ✅ **Lacunas do corpus (docs/lacunas/fabio.md)** — DOM ObjC do WebKit1 (`DOMNode`, `DOMElement`,
    `DOMHTMLElement`, `DOMDocument`, `DOMHTMLDocument`, `DOMCharacterData`, `DOMText`, `DOMComment`, `DOMNodeList`,
    `DOMHTMLCollection`; `WebFrame.DOMDocument` deixou de ser nil) sobre o JS da vista, as constantes
    `WebAction*Key`, e o `SafariServices.framework` (`SFSafariApplication`, `SFSafariExtensionHandler`,
    `SFSafariExtensionManager`, `SFSafariExtensionState`): sem Safari no mOS, `openWindowWithURL:` abre no
    navegador padrão e o resto responde como um Mac sem o Safari. Das 33 lacunas, 11 eram classes que já existiam
    no WebKit.framework (o machocheck do corpus não o carregava; avisado ao Igor). Falta: o `evaluateJavaScript`
    async do overlay Swift (`libswiftWebKit`).

## Coordenação

- Mariana (ImageIO): o decodificador WebP serve também ao `CGImageSource`.
- Isabela (mídia): pipeline de `<video>`/`<audio>` e AV1 para AVIF.
- Otavio (OpenGL/Metal): contexto GL para WebGL.
- Igor (corpus): reexecutar os apps Electron depois de cada fechamento.

## Diário

- 15 nov.: harness de host do motor (`tools/web_host.sh`, 5/5).
- 16 nov.: JavaScriptCore, API ObjC (compila e liga; `tests-jsc` para a VM).
- 16 nov.: JavaScriptCore, API C (`tools/jsc_host.sh` 34/34; `web_host` 248/248).
- 16 nov.: typed arrays (`typed_arrays`): 248/248.
- 16 nov.: diálogos bloqueantes (`dialogo_bloqueante`, `dialogo_pagina_fechada`, `dialogo_em_async`, `geradores_e_async`): 215/215.
- 16 nov.: formulários em iframes (`iframe_formularios`): 199/199.
- 16 nov.: iframes parte 1 (`iframe_src_pintura_js`, `iframe_clique_rodinha_src`, `iframe_aninhado_e_limites`): 188/188.
- 15 nov.: SVG (`svg_formas`, `svg_path_comandos`, `svg_regras_e_traco`, `svg_viewbox_transform`, `svg_gradientes`,
  `svg_estilo_use_clip`, `svg_texto_imagem`, `svg_na_pagina`, `svg_sprite_da_pagina`, `svg_lixo`): 166/166.
- 15 nov.: WebP e imagens responsivas (`tools/web_host.sh`: casos `webp_formatos`, `webp_lixo`, `img_srcset_picture`, `img_webp_na_pagina`).
