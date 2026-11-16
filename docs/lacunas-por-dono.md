# Lacunas do corpus por dono

Gerado por `python3 tools/corpus/harness.py donos` a partir dos relatórios do `machocheck` de cada app
(`tools/corpus/varrer-tudo.sh`, libSystem já compilado, sem rodar o build). Cada símbolo ausente vai para a frente
do `TIME.md` dona da biblioteca que o app importa (tabela `FW_DONO` e prefixos em `tools/corpus/harness.py`).
A medida é estática: um símbolo resolvido não quer dizer que a função funciona, e um app com 0 ausentes pode não abrir.

libSystem medido: main ce6dc05c (merge #392), copiado de build do Ricardo 2026-11-16_01:41.

Corpus: **300** apps no manifesto; **182** medidos com Mach-O; **2** falharam na medição (lista no fim); **116** ainda não medidos.

| dono | símbolos distintos | apps afetados | arquivo |
|---|---:|---:|---|
| Eduardo (Foundation/CoreFoundation) | 3760 | 149 | [lacunas/eduardo.md](lacunas/eduardo.md) |
| Ricardo (SwiftUI, Combine, UIKit) | 4997 | 42 | [lacunas/ricardo.md](lacunas/ricardo.md) |
| Natalia (serviços e segurança) | 1992 | 136 | [lacunas/natalia.md](lacunas/natalia.md) |
| Leonardo (runtime C/C++) | 1614 | 118 | [lacunas/leonardo.md](lacunas/leonardo.md) |
| Beatriz (AppKit) | 755 | 127 | [lacunas/beatriz.md](lacunas/beatriz.md) |
| Isabela (mídia) | 779 | 78 | [lacunas/isabela.md](lacunas/isabela.md) |
| Fernanda (Swift runtime, Dispatch) | 551 | 65 | [lacunas/fernanda.md](lacunas/fernanda.md) |
| Patricia (rede para apps) | 545 | 78 | [lacunas/patricia.md](lacunas/patricia.md) |
| Plugins (flat lookup: o símbolo vem do app que carrega o plugin, não do mOS) | 661 | 15 | [lacunas/plugin.md](lacunas/plugin.md) |
| Igor (sem dono claro: triagem) | 547 | 48 | [lacunas/igor.md](lacunas/igor.md) |
| Camila (desktop e window server) | 180 | 94 | [lacunas/camila.md](lacunas/camila.md) |
| Fabio (WebKit/Electron) | 139 | 65 | [lacunas/fabio.md](lacunas/fabio.md) |
| Larissa (plataforma: IOKit, USB, áudio HW) | 170 | 59 | [lacunas/larissa.md](lacunas/larissa.md) |
| Mariana (CG, Core Text, ImageIO, PDFKit) | 166 | 82 | [lacunas/mariana.md](lacunas/mariana.md) |
| Otavio (OpenGL, Metal, Core Animation) | 205 | 38 | [lacunas/otavio.md](lacunas/otavio.md) |
| Renata (BSD/Mach/XPC) | 36 | 37 | [lacunas/renata.md](lacunas/renata.md) |
| Vinicius (arquivos e volumes) | 42 | 12 | [lacunas/vinicius.md](lacunas/vinicius.md) |
| Felipe (Wi-Fi, Bluetooth, Ethernet) | 15 | 6 | [lacunas/felipe.md](lacunas/felipe.md) |
| Thais (GPU e telas) | 11 | 3 | [lacunas/thais.md](lacunas/thais.md) |

## Por app

| app | arch | total | eduardo | ricardo | natalia | leonardo | beatriz | isabela | fernanda | patricia | plugin | igor | camila | fabio | larissa | mariana | otavio | renata | vinicius | felipe | thais |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| whatsapp | x86_64 | 2611 | 943 | 1097 | 336 | 4 | 5 | 103 | 38 | 3 | 0 | 56 | 0 | 19 | 0 | 7 | 0 | 0 | 0 | 0 | 0 |
| cmux | x86_64 | 2134 | 745 | 1002 | 142 | 5 | 32 | 13 | 89 | 69 | 0 | 2 | 3 | 21 | 0 | 10 | 0 | 1 | 0 | 0 | 0 |
| vorssaint | arm64 | 2072 | 298 | 1561 | 37 | 4 | 23 | 50 | 56 | 1 | 0 | 1 | 10 | 0 | 10 | 20 | 0 | 0 | 1 | 0 | 0 |
| openclaw | x86_64 | 2037 | 684 | 803 | 66 | 188 | 48 | 48 | 115 | 19 | 0 | 34 | 14 | 11 | 1 | 4 | 0 | 2 | 0 | 0 | 0 |
| codexbar | x86_64 | 1899 | 642 | 892 | 265 | 0 | 47 | 0 | 40 | 1 | 0 | 0 | 1 | 10 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| coteditor | x86_64 | 1878 | 673 | 851 | 145 | 0 | 120 | 23 | 58 | 0 | 0 | 0 | 0 | 5 | 0 | 3 | 0 | 0 | 0 | 0 | 0 |
| raycast | x86_64 | 1863 | 995 | 289 | 261 | 3 | 48 | 26 | 153 | 46 | 0 | 6 | 19 | 3 | 6 | 7 | 1 | 0 | 0 | 0 | 0 |
| protonvpn | x86_64 | 1486 | 675 | 471 | 182 | 7 | 8 | 7 | 47 | 73 | 0 | 1 | 0 | 4 | 0 | 4 | 0 | 4 | 0 | 3 | 0 |
| orbstack | x86_64 | 1424 | 416 | 480 | 319 | 9 | 12 | 0 | 41 | 70 | 0 | 27 | 0 | 4 | 35 | 4 | 0 | 6 | 1 | 0 | 0 |
| tailscale-app | x86_64 | 1191 | 291 | 555 | 295 | 0 | 4 | 2 | 11 | 27 | 0 | 6 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| thaw | x86_64 | 1188 | 282 | 655 | 122 | 0 | 6 | 2 | 90 | 0 | 0 | 0 | 22 | 5 | 0 | 2 | 0 | 2 | 0 | 0 | 0 |
| utm | x86_64 | 964 | 356 | 365 | 42 | 46 | 1 | 1 | 17 | 44 | 1 | 24 | 2 | 0 | 25 | 6 | 14 | 3 | 17 | 0 | 0 |
| betterdisplay | x86_64 | 916 | 309 | 282 | 178 | 0 | 32 | 21 | 8 | 0 | 0 | 6 | 34 | 4 | 9 | 22 | 0 | 0 | 0 | 0 | 11 |
| tableplus | x86_64 | 907 | 590 | 14 | 151 | 32 | 12 | 0 | 3 | 75 | 0 | 15 | 0 | 11 | 0 | 4 | 0 | 0 | 0 | 0 | 0 |
| windows-app | x86_64 | 884 | 351 | 408 | 27 | 6 | 28 | 25 | 2 | 18 | 0 | 15 | 3 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| iterm2 | x86_64 | 848 | 601 | 65 | 71 | 29 | 38 | 7 | 10 | 0 | 0 | 0 | 2 | 14 | 2 | 8 | 0 | 1 | 0 | 0 | 0 |
| hammerspoon | x86_64 | 836 | 351 | 0 | 132 | 9 | 144 | 82 | 0 | 38 | 9 | 6 | 21 | 7 | 25 | 1 | 0 | 0 | 0 | 11 | 0 |
| clipy | x86_64 | 823 | 411 | 302 | 73 | 7 | 10 | 2 | 11 | 0 | 0 | 0 | 1 | 5 | 0 | 1 | 0 | 0 | 0 | 0 | 0 |
| xcodes-app | x86_64 | 822 | 343 | 379 | 77 | 2 | 3 | 0 | 13 | 0 | 0 | 0 | 0 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| applite | x86_64 | 821 | 331 | 425 | 13 | 4 | 0 | 9 | 27 | 3 | 0 | 4 | 0 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| chatgpt | x86_64 | 768 | 623 | 0 | 25 | 0 | 0 | 0 | 111 | 0 | 0 | 0 | 0 | 9 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| maccy | x86_64 | 765 | 184 | 312 | 127 | 0 | 59 | 2 | 75 | 0 | 0 | 0 | 1 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| markedit | x86_64 | 762 | 327 | 190 | 143 | 0 | 16 | 0 | 77 | 0 | 0 | 2 | 0 | 5 | 0 | 2 | 0 | 0 | 0 | 0 | 0 |
| pearcleaner | x86_64 | 754 | 288 | 424 | 9 | 0 | 0 | 5 | 13 | 0 | 0 | 11 | 0 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| calibre | x86_64 | 740 | 31 | 0 | 13 | 258 | 45 | 119 | 0 | 17 | 0 | 236 | 1 | 0 | 3 | 7 | 7 | 3 | 0 | 0 | 0 |
| ghostty | x86_64 | 663 | 211 | 234 | 183 | 1 | 4 | 0 | 16 | 0 | 0 | 0 | 5 | 5 | 0 | 4 | 0 | 0 | 0 | 0 | 0 |
| sequel-ace | x86_64 | 620 | 419 | 133 | 11 | 21 | 16 | 0 | 8 | 9 | 0 | 0 | 0 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| zoom | x86_64 | 615 | 310 | 0 | 59 | 17 | 42 | 27 | 1 | 26 | 0 | 0 | 51 | 29 | 14 | 11 | 27 | 0 | 0 | 1 | 0 |
| dockdoor | x86_64 | 612 | 211 | 369 | 6 | 0 | 4 | 0 | 10 | 0 | 0 | 0 | 7 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| aldente | x86_64 | 549 | 267 | 228 | 26 | 3 | 1 | 0 | 9 | 0 | 0 | 0 | 1 | 4 | 10 | 0 | 0 | 0 | 0 | 0 | 0 |
| puremac | x86_64 | 548 | 199 | 328 | 11 | 0 | 0 | 0 | 10 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| jordanbaird-ice | x86_64 | 547 | 102 | 403 | 3 | 0 | 4 | 0 | 10 | 0 | 0 | 0 | 16 | 4 | 0 | 5 | 0 | 0 | 0 | 0 | 0 |
| adobe-acrobat-reader | x86_64 | 540 | 80 | 0 | 173 | 58 | 130 | 4 | 1 | 39 | 0 | 17 | 3 | 6 | 2 | 22 | 4 | 0 | 0 | 1 | 0 |
| iina | x86_64 | 535 | 337 | 34 | 19 | 11 | 25 | 49 | 10 | 30 | 0 | 0 | 3 | 12 | 0 | 1 | 3 | 0 | 0 | 0 | 1 |
| 1password | x86_64 | 500 | 199 | 253 | 30 | 1 | 4 | 0 | 10 | 2 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 |
| finetune | x86_64 | 492 | 128 | 279 | 5 | 0 | 47 | 6 | 19 | 0 | 0 | 0 | 0 | 4 | 4 | 0 | 0 | 0 | 0 | 0 | 0 |
| inkscape | x86_64 | 470 | 0 | 0 | 3 | 461 | 1 | 0 | 0 | 3 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| microsoft-teams | x86_64 | 461 | 82 | 84 | 128 | 18 | 14 | 13 | 9 | 11 | 0 | 4 | 7 | 64 | 26 | 0 | 1 | 0 | 0 | 0 | 0 |
| deluge | x86_64 | 452 | 4 | 0 | 141 | 237 | 15 | 25 | 0 | 5 | 2 | 0 | 19 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 |
| krita | x86_64 | 449 | 46 | 0 | 23 | 259 | 59 | 12 | 0 | 6 | 0 | 0 | 18 | 0 | 13 | 8 | 3 | 2 | 0 | 0 | 0 |
| blender | x86_64 | 433 | 3 | 0 | 1 | 303 | 6 | 104 | 0 | 3 | 0 | 0 | 2 | 0 | 1 | 4 | 6 | 0 | 0 | 0 | 0 |
| cloudflare-warp | x86_64 | 408 | 283 | 0 | 24 | 19 | 0 | 0 | 11 | 44 | 0 | 6 | 0 | 21 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| swiftbar | x86_64 | 377 | 208 | 143 | 11 | 0 | 1 | 0 | 10 | 0 | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| linearmouse | x86_64 | 375 | 165 | 159 | 0 | 1 | 2 | 0 | 9 | 0 | 0 | 1 | 7 | 4 | 27 | 0 | 0 | 0 | 0 | 0 | 0 |
| telegram | x86_64 | 372 | 128 | 0 | 14 | 31 | 15 | 131 | 1 | 0 | 0 | 11 | 2 | 17 | 0 | 11 | 11 | 0 | 0 | 0 | 0 |
| qlmarkdown | x86_64 | 365 | 229 | 3 | 112 | 1 | 5 | 0 | 10 | 0 | 0 | 0 | 0 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| shotcut | x86_64 | 363 | 34 | 0 | 17 | 20 | 18 | 136 | 0 | 12 | 0 | 0 | 18 | 0 | 13 | 4 | 89 | 2 | 0 | 0 | 0 |
| stats | x86_64 | 361 | 253 | 62 | 1 | 12 | 1 | 0 | 0 | 12 | 0 | 0 | 0 | 0 | 10 | 0 | 0 | 0 | 8 | 2 | 0 |
| meld | x86_64 | 354 | 0 | 0 | 3 | 346 | 1 | 0 | 0 | 3 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| sourcetree | x86_64 | 350 | 132 | 17 | 17 | 11 | 18 | 0 | 2 | 0 | 148 | 2 | 0 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| kitty | x86_64 | 339 | 3 | 0 | 1 | 309 | 0 | 0 | 0 | 3 | 0 | 0 | 15 | 0 | 3 | 3 | 0 | 2 | 0 | 0 | 0 |
| shottr | x86_64 | 325 | 213 | 6 | 26 | 0 | 52 | 14 | 4 | 0 | 0 | 3 | 3 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 |
| claude | x86_64 | 314 | 261 | 0 | 26 | 0 | 0 | 7 | 18 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| virtualbox | x86_64 | 304 | 32 | 0 | 26 | 11 | 55 | 0 | 0 | 18 | 99 | 38 | 4 | 0 | 10 | 4 | 0 | 4 | 3 | 0 | 0 |
| obs | x86_64 | 301 | 63 | 0 | 17 | 11 | 18 | 71 | 3 | 12 | 81 | 0 | 5 | 5 | 3 | 6 | 4 | 2 | 0 | 0 | 0 |
| kicad | x86_64 | 297 | 16 | 0 | 24 | 126 | 79 | 0 | 0 | 5 | 16 | 0 | 4 | 1 | 0 | 7 | 19 | 0 | 0 | 0 | 0 |
| gemini | x86_64 | 285 | 148 | 0 | 34 | 31 | 22 | 24 | 2 | 1 | 0 | 2 | 2 | 5 | 2 | 9 | 3 | 0 | 0 | 0 | 0 |
| wireshark-app | x86_64 | 272 | 30 | 0 | 20 | 49 | 45 | 94 | 0 | 18 | 0 | 0 | 1 | 4 | 0 | 4 | 5 | 2 | 0 | 0 | 0 |
| localsend | x86_64 | 265 | 105 | 38 | 67 | 8 | 0 | 23 | 2 | 0 | 0 | 22 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| alt-tab | x86_64 | 255 | 192 | 0 | 8 | 9 | 12 | 0 | 2 | 0 | 0 | 0 | 27 | 3 | 0 | 2 | 0 | 0 | 0 | 0 | 0 |
| lm-studio | x86_64 | 246 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 246 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| emacs-app | x86_64 | 236 | 4 | 0 | 3 | 17 | 6 | 0 | 1 | 3 | 198 | 0 | 0 | 0 | 0 | 2 | 0 | 2 | 0 | 0 | 0 |
| basictex | x86_64 | 230 | 0 | 0 | 0 | 230 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| xquartz | x86_64 | 227 | 1 | 0 | 1 | 170 | 5 | 0 | 0 | 0 | 0 | 0 | 8 | 5 | 1 | 0 | 33 | 3 | 0 | 0 | 0 |
| bitwarden | x86_64 | 223 | 115 | 0 | 60 | 21 | 9 | 0 | 6 | 0 | 4 | 0 | 1 | 1 | 0 | 1 | 3 | 2 | 0 | 0 | 0 |
| zotero | x86_64 | 211 | 80 | 0 | 22 | 19 | 42 | 15 | 3 | 3 | 0 | 1 | 3 | 14 | 0 | 8 | 1 | 0 | 0 | 0 | 0 |
| lmms | x86_64 | 206 | 31 | 0 | 36 | 25 | 55 | 28 | 0 | 3 | 0 | 0 | 19 | 0 | 1 | 4 | 2 | 2 | 0 | 0 | 0 |
| rustdesk | x86_64 | 202 | 110 | 0 | 0 | 25 | 19 | 15 | 0 | 1 | 0 | 0 | 23 | 0 | 0 | 5 | 4 | 0 | 0 | 0 | 0 |
| rectangle | x86_64 | 199 | 103 | 73 | 3 | 0 | 2 | 0 | 10 | 0 | 0 | 0 | 0 | 5 | 0 | 3 | 0 | 0 | 0 | 0 | 0 |
| warp | x86_64 | 197 | 174 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 19 | 0 | 0 | 0 | 0 | 0 |
| android-studio | x86_64 | 192 | 18 | 0 | 35 | 39 | 20 | 11 | 0 | 9 | 0 | 29 | 17 | 0 | 2 | 2 | 0 | 10 | 0 | 0 | 0 |
| the-unarchiver | x86_64 | 188 | 133 | 0 | 8 | 21 | 11 | 1 | 2 | 1 | 0 | 0 | 2 | 4 | 1 | 1 | 3 | 0 | 0 | 0 | 0 |
| monitorcontrol | x86_64 | 184 | 107 | 0 | 0 | 26 | 13 | 9 | 0 | 0 | 0 | 0 | 10 | 4 | 7 | 2 | 3 | 0 | 0 | 0 | 3 |
| pgadmin4 | x86_64 | 181 | 4 | 0 | 6 | 103 | 45 | 0 | 0 | 23 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| libreoffice | x86_64 | 180 | 3 | 0 | 28 | 117 | 22 | 1 | 0 | 4 | 1 | 0 | 1 | 0 | 1 | 2 | 0 | 0 | 0 | 0 | 0 |
| opencode-desktop | x86_64 | 176 | 0 | 0 | 0 | 176 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| openscad | x86_64 | 175 | 27 | 0 | 19 | 11 | 91 | 0 | 2 | 3 | 0 | 0 | 1 | 1 | 2 | 3 | 15 | 0 | 0 | 0 | 0 |
| moonlight | x86_64 | 174 | 31 | 0 | 18 | 54 | 18 | 2 | 0 | 14 | 0 | 0 | 13 | 0 | 15 | 6 | 1 | 2 | 0 | 0 | 0 |
| dotnet-sdk | x86_64 | 171 | 18 | 0 | 124 | 0 | 0 | 0 | 0 | 29 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| mos | x86_64 | 156 | 107 | 0 | 0 | 24 | 14 | 0 | 0 | 0 | 0 | 0 | 3 | 4 | 0 | 1 | 3 | 0 | 0 | 0 | 0 |
| thunderbird | x86_64 | 152 | 21 | 0 | 48 | 20 | 30 | 16 | 0 | 3 | 0 | 0 | 3 | 0 | 2 | 8 | 1 | 0 | 0 | 0 | 0 |
| webtorrent | x86_64 | 152 | 11 | 0 | 54 | 27 | 39 | 4 | 2 | 0 | 0 | 8 | 1 | 0 | 0 | 6 | 0 | 0 | 0 | 0 | 0 |
| karabiner-elements | x86_64 | 152 | 86 | 45 | 0 | 0 | 0 | 0 | 3 | 0 | 0 | 0 | 0 | 6 | 12 | 0 | 0 | 0 | 0 | 0 | 0 |
| openmtp | x86_64 | 148 | 8 | 0 | 54 | 37 | 33 | 4 | 0 | 0 | 0 | 8 | 1 | 0 | 1 | 2 | 0 | 0 | 0 | 0 | 0 |
| pencil | x86_64 | 141 | 9 | 0 | 54 | 30 | 33 | 4 | 0 | 0 | 0 | 8 | 1 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 |
| vnote | x86_64 | 139 | 30 | 0 | 18 | 12 | 47 | 0 | 0 | 14 | 0 | 8 | 1 | 0 | 0 | 7 | 0 | 2 | 0 | 0 | 0 |
| musescore | x86_64 | 136 | 32 | 0 | 22 | 7 | 45 | 6 | 0 | 14 | 0 | 0 | 1 | 0 | 0 | 7 | 0 | 2 | 0 | 0 | 0 |
| hex-fiend | x86_64 | 126 | 57 | 0 | 6 | 47 | 6 | 0 | 2 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 1 | 6 | 0 | 0 |
| intellij-idea | x86_64 | 126 | 13 | 0 | 26 | 4 | 20 | 7 | 0 | 6 | 0 | 29 | 17 | 0 | 2 | 2 | 0 | 0 | 0 | 0 | 0 |
| audacity | x86_64 | 124 | 40 | 0 | 19 | 15 | 17 | 9 | 0 | 14 | 0 | 0 | 1 | 0 | 0 | 7 | 0 | 2 | 0 | 0 | 0 |
| cyberduck | x86_64 | 123 | 17 | 0 | 24 | 25 | 4 | 5 | 2 | 18 | 0 | 23 | 0 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| jetbrains-toolbox | x86_64 | 122 | 12 | 0 | 26 | 2 | 19 | 7 | 0 | 6 | 0 | 29 | 17 | 0 | 2 | 2 | 0 | 0 | 0 | 0 | 0 |
| vlc | x86_64 | 120 | 14 | 0 | 32 | 18 | 21 | 19 | 2 | 0 | 0 | 0 | 4 | 1 | 3 | 0 | 3 | 3 | 0 | 0 | 0 |
| firefox | x86_64 | 119 | 22 | 0 | 13 | 19 | 30 | 19 | 0 | 3 | 0 | 0 | 3 | 0 | 2 | 7 | 1 | 0 | 0 | 0 | 0 |
| floorp | x86_64 | 118 | 21 | 0 | 13 | 18 | 30 | 19 | 0 | 3 | 0 | 0 | 3 | 0 | 2 | 8 | 1 | 0 | 0 | 0 | 0 |
| zen | x86_64 | 118 | 21 | 0 | 13 | 18 | 30 | 19 | 0 | 3 | 0 | 0 | 3 | 0 | 2 | 8 | 1 | 0 | 0 | 0 | 0 |
| mullvad-browser | x86_64 | 117 | 21 | 0 | 11 | 18 | 35 | 16 | 0 | 3 | 0 | 0 | 3 | 0 | 1 | 8 | 1 | 0 | 0 | 0 | 0 |
| librewolf | x86_64 | 117 | 22 | 0 | 12 | 18 | 30 | 19 | 0 | 3 | 0 | 0 | 3 | 0 | 2 | 7 | 1 | 0 | 0 | 0 | 0 |
| tor-browser | x86_64 | 115 | 21 | 0 | 11 | 17 | 35 | 15 | 0 | 3 | 0 | 0 | 3 | 0 | 1 | 8 | 1 | 0 | 0 | 0 | 0 |
| visual-studio-code | x86_64 | 110 | 27 | 0 | 82 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| db-browser-for-sqlite | x86_64 | 110 | 27 | 0 | 14 | 6 | 52 | 0 | 0 | 3 | 0 | 0 | 1 | 0 | 1 | 4 | 0 | 2 | 0 | 0 | 0 |
| gimp | x86_64 | 109 | 0 | 0 | 0 | 93 | 1 | 10 | 0 | 3 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| prusaslicer | x86_64 | 108 | 15 | 0 | 14 | 14 | 51 | 0 | 0 | 0 | 0 | 0 | 4 | 1 | 0 | 4 | 0 | 0 | 3 | 2 | 0 |
| raspberry-pi-imager | x86_64 | 108 | 28 | 0 | 21 | 3 | 18 | 0 | 0 | 14 | 0 | 0 | 1 | 0 | 4 | 6 | 0 | 2 | 11 | 0 | 0 |
| temurin | x86_64 | 106 | 12 | 0 | 21 | 3 | 15 | 7 | 0 | 6 | 0 | 29 | 12 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 |
| github | x86_64 | 106 | 21 | 0 | 59 | 8 | 1 | 0 | 0 | 17 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| dbeaver-community | x86_64 | 105 | 12 | 0 | 21 | 2 | 15 | 7 | 0 | 6 | 0 | 29 | 12 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 |
| keepassxc | x86_64 | 103 | 27 | 0 | 10 | 12 | 18 | 0 | 0 | 14 | 0 | 13 | 1 | 0 | 0 | 6 | 0 | 2 | 0 | 0 | 0 |
| dolphin | x86_64 | 97 | 29 | 0 | 14 | 9 | 18 | 0 | 0 | 1 | 0 | 0 | 2 | 0 | 18 | 4 | 0 | 2 | 0 | 0 | 0 |
| drawio | x86_64 | 95 | 68 | 24 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| alfred | x86_64 | 88 | 10 | 0 | 53 | 0 | 16 | 3 | 0 | 0 | 0 | 1 | 3 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 |
| docker-desktop | x86_64 | 87 | 37 | 0 | 12 | 8 | 0 | 0 | 0 | 0 | 0 | 15 | 0 | 0 | 14 | 0 | 0 | 0 | 1 | 0 | 0 |
| qbittorrent | x86_64 | 80 | 25 | 0 | 14 | 2 | 18 | 0 | 0 | 12 | 0 | 0 | 1 | 0 | 0 | 6 | 0 | 2 | 0 | 0 | 0 |
| obsidian | x86_64 | 77 | 63 | 0 | 5 | 0 | 1 | 0 | 6 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| spotify | x86_64 | 77 | 6 | 0 | 4 | 1 | 1 | 37 | 0 | 21 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 6 | 0 | 0 |
| figma | x86_64 | 67 | 42 | 0 | 1 | 13 | 0 | 9 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 |
| cursor | x86_64 | 64 | 64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tic80 | x86_64 | 58 | 2 | 0 | 1 | 24 | 0 | 0 | 0 | 0 | 0 | 0 | 17 | 0 | 13 | 0 | 1 | 0 | 0 | 0 | 0 |
| unity-hub | x86_64 | 57 | 0 | 0 | 15 | 2 | 17 | 0 | 0 | 22 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 |
| keka | x86_64 | 49 | 6 | 0 | 7 | 23 | 2 | 0 | 2 | 0 | 0 | 3 | 0 | 5 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| handbrake-app | x86_64 | 49 | 5 | 0 | 1 | 0 | 6 | 21 | 0 | 0 | 0 | 0 | 0 | 5 | 2 | 0 | 9 | 0 | 0 | 0 | 0 |
| mongodb-compass | x86_64 | 48 | 0 | 0 | 1 | 2 | 0 | 0 | 0 | 2 | 43 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| mounty | x86_64 | 47 | 3 | 0 | 42 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 |
| git-credential-manager | x86_64 | 46 | 3 | 0 | 21 | 0 | 5 | 0 | 0 | 17 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| zed | x86_64 | 45 | 0 | 0 | 1 | 0 | 7 | 23 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 14 | 0 | 0 | 0 | 0 | 0 |
| microsoft-edge | x86_64 | 44 | 1 | 0 | 7 | 12 | 17 | 0 | 0 | 3 | 0 | 0 | 1 | 0 | 0 | 2 | 0 | 1 | 0 | 0 | 0 |
| comfy | x86_64 | 43 | 0 | 0 | 0 | 5 | 0 | 0 | 0 | 0 | 38 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| openrct2 | x86_64 | 42 | 4 | 0 | 1 | 6 | 0 | 0 | 0 | 0 | 0 | 0 | 17 | 0 | 13 | 0 | 1 | 0 | 0 | 0 | 0 |
| love | x86_64 | 40 | 2 | 0 | 1 | 4 | 0 | 0 | 2 | 0 | 0 | 0 | 17 | 0 | 13 | 0 | 1 | 0 | 0 | 0 | 0 |
| openlogi | x86_64 | 40 | 1 | 0 | 1 | 0 | 7 | 1 | 0 | 0 | 0 | 0 | 2 | 5 | 19 | 4 | 0 | 0 | 0 | 0 | 0 |
| flycut | x86_64 | 38 | 4 | 0 | 21 | 2 | 9 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| clash-verge-rev | x86_64 | 38 | 2 | 0 | 2 | 0 | 10 | 0 | 0 | 16 | 0 | 0 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| fuse-t | x86_64 | 37 | 0 | 0 | 0 | 1 | 0 | 0 | 21 | 0 | 0 | 15 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| transmission | x86_64 | 36 | 6 | 0 | 5 | 3 | 18 | 0 | 2 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| godot | x86_64 | 36 | 3 | 0 | 0 | 3 | 14 | 2 | 0 | 0 | 0 | 0 | 2 | 0 | 12 | 0 | 0 | 0 | 0 | 0 | 0 |
| pixelorama | x86_64 | 36 | 3 | 0 | 0 | 3 | 14 | 2 | 0 | 0 | 0 | 0 | 2 | 0 | 12 | 0 | 0 | 0 | 0 | 0 | 0 |
| lulu | x86_64 | 36 | 1 | 0 | 4 | 0 | 2 | 0 | 0 | 11 | 0 | 18 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| hiddenbar | x86_64 | 35 | 34 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| multipass | x86_64 | 35 | 31 | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| bruno | x86_64 | 33 | 2 | 0 | 2 | 18 | 3 | 0 | 0 | 0 | 0 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| anydesk | x86_64 | 33 | 2 | 0 | 8 | 2 | 2 | 3 | 0 | 3 | 0 | 3 | 9 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| ollama-app | x86_64 | 32 | 0 | 0 | 2 | 2 | 0 | 28 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| openttd | x86_64 | 30 | 5 | 0 | 0 | 2 | 0 | 19 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 |
| handy | x86_64 | 29 | 2 | 0 | 2 | 0 | 10 | 7 | 0 | 0 | 0 | 0 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| sublime-text | x86_64 | 28 | 4 | 0 | 2 | 13 | 4 | 0 | 0 | 3 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 1 | 0 | 0 | 0 |
| macdown | x86_64 | 27 | 5 | 0 | 3 | 2 | 1 | 0 | 2 | 0 | 0 | 0 | 0 | 14 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| steam | x86_64 | 23 | 5 | 0 | 2 | 10 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 | 0 | 0 | 0 |
| cc-switch | x86_64 | 21 | 1 | 0 | 2 | 0 | 10 | 0 | 0 | 0 | 0 | 0 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| vagrant | x86_64 | 18 | 0 | 0 | 0 | 10 | 0 | 0 | 0 | 0 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| postman | x86_64 | 15 | 1 | 0 | 2 | 1 | 3 | 0 | 0 | 0 | 0 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| brave-browser | x86_64 | 14 | 0 | 0 | 4 | 2 | 1 | 0 | 0 | 5 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| tigervnc | x86_64 | 14 | 2 | 0 | 0 | 1 | 7 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 3 | 0 | 0 | 0 | 0 | 0 | 0 |
| headlamp | x86_64 | 14 | 1 | 0 | 2 | 0 | 3 | 0 | 0 | 0 | 0 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| google-chrome | x86_64 | 13 | 0 | 0 | 1 | 11 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| appcleaner | x86_64 | 12 | 3 | 0 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5 | 0 | 0 | 0 | 0 | 1 | 0 | 0 |
| scroll-reverser | x86_64 | 12 | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 5 | 4 | 0 | 0 | 0 | 0 | 0 | 0 |
| alacritty | x86_64 | 11 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 9 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 |
| antigravity | x86_64 | 8 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| insomnia | x86_64 | 7 | 0 | 0 | 0 | 1 | 0 | 3 | 0 | 0 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| t3-code | x86_64 | 7 | 0 | 0 | 0 | 0 | 0 | 7 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| notunes | x86_64 | 7 | 7 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| background-music | x86_64 | 6 | 2 | 0 | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| keepingyouawake | x86_64 | 5 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| macfuse | x86_64 | 2 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| wezterm | x86_64 | 2 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| signal | x86_64 | 2 | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| blackhole-2ch | x86_64 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| caffeine | x86_64 | 1 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| blackhole-16ch | x86_64 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| helium-browser | x86_64 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| element | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| finicky | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| vscodium | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| dbgate | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| session-manager-plugin | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| discord | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| slack | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| notion | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| google-drive | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| podman-desktop | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| balenaetcher | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| ungoogled-chromium | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

## Falharam na medição

| app | motivo |
|---|---|
| mactex | mactex: sem espaço em disco para extrair (6546 MiB; precisa de 19 GiB livres, há 16 GiB) |
| mactex-no-gui | sem espaço em disco para baixar e extrair (6546 MB; precisa de 19 GiB livres, há 14 GiB) |

## Ainda não medidos

rancher, freecad, google-gemini, wechat, microsoft-excel, microsoft-word, onedrive, mysqlworkbench, microsoft-office, pycharm, vivaldi, dropbox, anki, freelens, redis-insight, openvpn-connect, microsoft-outlook, syncthing-app, termius, grandperspective, proxyman, gcc-arm-embedded, microsoft-powerpoint, skim, github-copilot-for-xcode, maestro, tunnelblick, crossover, keycastr, upscayl, onyx, fluidvoice, mac-mouse-fix, homebrew-app, macs-fan-control, another-redis-desktop-manager, rstudio, teamviewer, nordvpn, kiro, bettertouchtool, typora, wispr-flow, duckduckgo, affinity, burp-suite, quarto, bambu-studio, fork, tabby, prismlauncher, r-app, wave, android-file-transfer, block-goose, wailbrew, telegram-desktop, easydict, espanso, itsycal, caskhub, datagrip, linear, antigravity-ide, devin-desktop, lens, jellyfin, nextcloud, arduino-ide, realvnc-connect-viewer, lunar, macwhisper, opera, onlyoffice, stremio, middleclick, superwhisper, amethyst, voiceink, beekeeper-studio, cherry-studio, dbx, opensuperwhisper, orion, istat-menus, supacode, capcut, bartender, unnaturalscrollwheels, daisydisk, displaylink, keyclu, coconutbattery, anythingllm, epic-games, thebrowsercompany-dia, syntax-highlight, battery, ubersicht, squirrel-app, cleanshot, cryptomator, feishu, sf-symbols, macpacker, qgis, loop, bbedit, marta, libndi, macdown-3000, orcaslicer, microsoft-auto-update, sfm, quicklook-video, imageoptim
