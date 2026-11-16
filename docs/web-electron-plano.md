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
3. **SVG**: `<svg>` inline e `.svg` em `<img>`/CSS — parser, caminhos (`path d`, formas básicas, `transform`,
   `viewBox`), preenchimento com regra não-zero/par-ímpar, traço com junções e tampas, gradientes linear/radial,
   opacidade, `<use>`, `<text>` simples; rasterizador com antisserrilhado próprio.
4. **Iframes**: `<iframe src/srcdoc>` como caixa substituída com documento, rolagem e reino JS próprios;
   `contentWindow`, `contentDocument` (mesma origem), `postMessage` entre janelas, eventos `load`.
5. **Diálogos JS bloqueantes**: o script para em `alert/confirm/prompt` até o `completionHandler` do app
   (pilha própria do motor por vista, retomada quando o app responde).
6. **Canvas 2D** (`<canvas>`, `getContext('2d')`, caminhos, texto, imagens, `getImageData`/`toDataURL`).
7. **WebGL 1/2** sobre o OpenGL do mOS (frente do Otavio) com reserva por software.
8. **`<video>`/`<audio>`**: o motor publica o elemento; o `WebKit.framework` decodifica com AVFoundation/
   VideoToolbox (frente da Isabela) e devolve quadros e relógio ao motor; controles nativos desenhados pela vista.
9. **AVIF** (AV1 intra + `av1C`/`ispe`/`pixi`/`alpha`) — junto da Isabela, que é dona do AV1.
10. **PDF**: `createPDFWithConfiguration`/impressão (gerar PDF da pintura) e exibir PDF na vista (sobre o PDFKit/CG
    da Mariana).
11. **JavaScriptCore.framework**: o motor Brisa compilado para o usuário, com a API C (`JSGlobalContextCreate`,
    `JSEvaluateScript`, `JSValue*`, `JSObject*`, `JSString*`, classes) e a API ObjC (`JSContext`, `JSValue`,
    `JSManagedValue`, `JSVirtualMachine`, `JSExport`).
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
- 15 nov.: WebP e imagens responsivas (`tools/web_host.sh`: casos `webp_formatos`, `webp_lixo`, `img_srcset_picture`, `img_webp_na_pagina`).
