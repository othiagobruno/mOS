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
6. **Canvas 2D** (`<canvas>`, `getContext('2d')`, caminhos, texto, imagens, `getImageData`/`toDataURL`).
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
