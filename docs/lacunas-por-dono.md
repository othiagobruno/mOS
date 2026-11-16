# Lacunas do corpus por dono

Gerado por `python3 tools/corpus/harness.py donos` a partir dos relatórios do `machocheck` de cada app
(`tools/corpus/varrer-tudo.sh`, libSystem já compilado, sem rodar o build). Cada símbolo ausente vai para a frente
do `TIME.md` dona da biblioteca que o app importa (tabela `FW_DONO` e prefixos em `tools/corpus/harness.py`).
A medida é estática: um símbolo resolvido não quer dizer que a função funciona, e um app com 0 ausentes pode não abrir.

libSystem medido: main ce6dc05c (merge #392), copiado de build do Ricardo 2026-11-16_01:41.

Corpus: **300** apps no manifesto; **126** medidos com Mach-O; **0** falharam na medição (lista no fim); **174** ainda não medidos.

| dono | símbolos distintos | apps afetados | arquivo |
|---|---:|---:|---|
| Eduardo (Foundation/CoreFoundation) | 3480 | 105 | [lacunas/eduardo.md](lacunas/eduardo.md) |
| Ricardo (SwiftUI, Combine, UIKit) | 4540 | 27 | [lacunas/ricardo.md](lacunas/ricardo.md) |
| Natalia (serviços e segurança) | 1663 | 95 | [lacunas/natalia.md](lacunas/natalia.md) |
| Leonardo (runtime C/C++) | 1530 | 89 | [lacunas/leonardo.md](lacunas/leonardo.md) |
| Beatriz (AppKit) | 653 | 89 | [lacunas/beatriz.md](lacunas/beatriz.md) |
| Isabela (mídia) | 724 | 54 | [lacunas/isabela.md](lacunas/isabela.md) |
| Fernanda (Swift runtime, Dispatch) | 451 | 43 | [lacunas/fernanda.md](lacunas/fernanda.md) |
| Patricia (rede para apps) | 398 | 54 | [lacunas/patricia.md](lacunas/patricia.md) |
| Camila (desktop e window server) | 176 | 68 | [lacunas/camila.md](lacunas/camila.md) |
| Igor (sem dono claro: triagem) | 230 | 26 | [lacunas/igor.md](lacunas/igor.md) |
| Plugins (flat lookup: o símbolo vem do app que carrega o plugin, não do mOS) | 301 | 9 | [lacunas/plugin.md](lacunas/plugin.md) |
| Mariana (CG, Core Text, ImageIO, PDFKit) | 138 | 62 | [lacunas/mariana.md](lacunas/mariana.md) |
| Larissa (plataforma: IOKit, USB, áudio HW) | 135 | 40 | [lacunas/larissa.md](lacunas/larissa.md) |
| Fabio (WebKit/Electron) | 74 | 45 | [lacunas/fabio.md](lacunas/fabio.md) |
| Otavio (OpenGL, Metal, Core Animation) | 193 | 29 | [lacunas/otavio.md](lacunas/otavio.md) |
| Renata (BSD/Mach/XPC) | 32 | 30 | [lacunas/renata.md](lacunas/renata.md) |
| Vinicius (arquivos e volumes) | 37 | 9 | [lacunas/vinicius.md](lacunas/vinicius.md) |
| Felipe (Wi-Fi, Bluetooth, Ethernet) | 13 | 4 | [lacunas/felipe.md](lacunas/felipe.md) |
| Thais (GPU e telas) | 11 | 3 | [lacunas/thais.md](lacunas/thais.md) |

## Por app

| app | arch | total | eduardo | ricardo | natalia | leonardo | beatriz | isabela | fernanda | patricia | camila | igor | plugin | mariana | larissa | fabio | otavio | renata | vinicius | felipe | thais |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| whatsapp | x86_64 | 2611 | 943 | 1097 | 336 | 4 | 5 | 103 | 38 | 3 | 0 | 56 | 0 | 7 | 0 | 19 | 0 | 0 | 0 | 0 | 0 |
| cmux | x86_64 | 2134 | 745 | 1002 | 142 | 5 | 32 | 13 | 89 | 69 | 3 | 2 | 0 | 10 | 0 | 21 | 0 | 1 | 0 | 0 | 0 |
| vorssaint | arm64 | 2072 | 298 | 1561 | 37 | 4 | 23 | 50 | 56 | 1 | 10 | 1 | 0 | 20 | 10 | 0 | 0 | 0 | 1 | 0 | 0 |
| openclaw | x86_64 | 2037 | 684 | 803 | 66 | 188 | 48 | 48 | 115 | 19 | 14 | 34 | 0 | 4 | 1 | 11 | 0 | 2 | 0 | 0 | 0 |
| coteditor | x86_64 | 1878 | 673 | 851 | 145 | 0 | 120 | 23 | 58 | 0 | 0 | 0 | 0 | 3 | 0 | 5 | 0 | 0 | 0 | 0 | 0 |
| raycast | x86_64 | 1863 | 995 | 289 | 261 | 3 | 48 | 26 | 153 | 46 | 19 | 6 | 0 | 7 | 6 | 3 | 1 | 0 | 0 | 0 | 0 |
| orbstack | x86_64 | 1424 | 416 | 480 | 319 | 9 | 12 | 0 | 41 | 70 | 0 | 27 | 0 | 4 | 35 | 4 | 0 | 6 | 1 | 0 | 0 |
| tailscale-app | x86_64 | 1191 | 291 | 555 | 295 | 0 | 4 | 2 | 11 | 27 | 0 | 6 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| utm | x86_64 | 964 | 356 | 365 | 42 | 46 | 1 | 1 | 17 | 44 | 2 | 24 | 1 | 6 | 25 | 0 | 14 | 3 | 17 | 0 | 0 |
| betterdisplay | x86_64 | 916 | 309 | 282 | 178 | 0 | 32 | 21 | 8 | 0 | 34 | 6 | 0 | 22 | 9 | 4 | 0 | 0 | 0 | 0 | 11 |
| iterm2 | x86_64 | 848 | 601 | 65 | 71 | 29 | 38 | 7 | 10 | 0 | 2 | 0 | 0 | 8 | 2 | 14 | 0 | 1 | 0 | 0 | 0 |
| hammerspoon | x86_64 | 836 | 351 | 0 | 132 | 9 | 144 | 82 | 0 | 38 | 21 | 6 | 9 | 1 | 25 | 7 | 0 | 0 | 0 | 11 | 0 |
| clipy | x86_64 | 823 | 411 | 302 | 73 | 7 | 10 | 2 | 11 | 0 | 1 | 0 | 0 | 1 | 0 | 5 | 0 | 0 | 0 | 0 | 0 |
| chatgpt | x86_64 | 768 | 623 | 0 | 25 | 0 | 0 | 0 | 111 | 0 | 0 | 0 | 0 | 0 | 0 | 9 | 0 | 0 | 0 | 0 | 0 |
| maccy | x86_64 | 765 | 184 | 312 | 127 | 0 | 59 | 2 | 75 | 0 | 1 | 0 | 0 | 0 | 0 | 5 | 0 | 0 | 0 | 0 | 0 |
| pearcleaner | x86_64 | 754 | 288 | 424 | 9 | 0 | 0 | 5 | 13 | 0 | 0 | 11 | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 |
| ghostty | x86_64 | 663 | 211 | 234 | 183 | 1 | 4 | 0 | 16 | 0 | 5 | 0 | 0 | 4 | 0 | 5 | 0 | 0 | 0 | 0 | 0 |
| sequel-ace | x86_64 | 620 | 419 | 133 | 11 | 21 | 16 | 0 | 8 | 9 | 0 | 0 | 0 | 0 | 0 | 3 | 0 | 0 | 0 | 0 | 0 |
| zoom | x86_64 | 615 | 310 | 0 | 59 | 17 | 42 | 27 | 1 | 26 | 51 | 0 | 0 | 11 | 14 | 29 | 27 | 0 | 0 | 1 | 0 |
| jordanbaird-ice | x86_64 | 547 | 102 | 403 | 3 | 0 | 4 | 0 | 10 | 0 | 16 | 0 | 0 | 5 | 0 | 4 | 0 | 0 | 0 | 0 | 0 |
| iina | x86_64 | 535 | 337 | 34 | 19 | 11 | 25 | 49 | 10 | 30 | 3 | 0 | 0 | 1 | 0 | 12 | 3 | 0 | 0 | 0 | 1 |
| 1password | x86_64 | 500 | 199 | 253 | 30 | 1 | 4 | 0 | 10 | 2 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| inkscape | x86_64 | 470 | 0 | 0 | 3 | 461 | 1 | 0 | 0 | 3 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| deluge | x86_64 | 452 | 4 | 0 | 141 | 237 | 15 | 25 | 0 | 5 | 19 | 0 | 2 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| krita | x86_64 | 449 | 46 | 0 | 23 | 259 | 59 | 12 | 0 | 6 | 18 | 0 | 0 | 8 | 13 | 0 | 3 | 2 | 0 | 0 | 0 |
| blender | x86_64 | 433 | 3 | 0 | 1 | 303 | 6 | 104 | 0 | 3 | 2 | 0 | 0 | 4 | 1 | 0 | 6 | 0 | 0 | 0 | 0 |
| swiftbar | x86_64 | 377 | 208 | 143 | 11 | 0 | 1 | 0 | 10 | 0 | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 |
| linearmouse | x86_64 | 375 | 165 | 159 | 0 | 1 | 2 | 0 | 9 | 0 | 7 | 1 | 0 | 0 | 27 | 4 | 0 | 0 | 0 | 0 | 0 |
| telegram | x86_64 | 372 | 128 | 0 | 14 | 31 | 15 | 131 | 1 | 0 | 2 | 11 | 0 | 11 | 0 | 17 | 11 | 0 | 0 | 0 | 0 |
| qlmarkdown | x86_64 | 365 | 229 | 3 | 112 | 1 | 5 | 0 | 10 | 0 | 0 | 0 | 0 | 0 | 0 | 5 | 0 | 0 | 0 | 0 | 0 |
| shotcut | x86_64 | 363 | 34 | 0 | 17 | 20 | 18 | 136 | 0 | 12 | 18 | 0 | 0 | 4 | 13 | 0 | 89 | 2 | 0 | 0 | 0 |
| stats | x86_64 | 361 | 253 | 62 | 1 | 12 | 1 | 0 | 0 | 12 | 0 | 0 | 0 | 0 | 10 | 0 | 0 | 0 | 8 | 2 | 0 |
| kitty | x86_64 | 339 | 3 | 0 | 1 | 309 | 0 | 0 | 0 | 3 | 15 | 0 | 0 | 3 | 3 | 0 | 0 | 2 | 0 | 0 | 0 |
| claude | x86_64 | 314 | 261 | 0 | 26 | 0 | 0 | 7 | 18 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| obs | x86_64 | 301 | 63 | 0 | 17 | 11 | 18 | 71 | 3 | 12 | 5 | 0 | 81 | 6 | 3 | 5 | 4 | 2 | 0 | 0 | 0 |
| kicad | x86_64 | 297 | 16 | 0 | 24 | 126 | 79 | 0 | 0 | 5 | 4 | 0 | 16 | 7 | 0 | 1 | 19 | 0 | 0 | 0 | 0 |
| wireshark-app | x86_64 | 272 | 30 | 0 | 20 | 49 | 45 | 94 | 0 | 18 | 1 | 0 | 0 | 4 | 0 | 4 | 5 | 2 | 0 | 0 | 0 |
| localsend | x86_64 | 265 | 105 | 38 | 67 | 8 | 0 | 23 | 2 | 0 | 0 | 22 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| alt-tab | x86_64 | 255 | 192 | 0 | 8 | 9 | 12 | 0 | 2 | 0 | 27 | 0 | 0 | 2 | 0 | 3 | 0 | 0 | 0 | 0 | 0 |
| lm-studio | x86_64 | 246 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 246 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| basictex | x86_64 | 230 | 0 | 0 | 0 | 230 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| xquartz | x86_64 | 227 | 1 | 0 | 1 | 170 | 5 | 0 | 0 | 0 | 8 | 0 | 0 | 0 | 1 | 5 | 33 | 3 | 0 | 0 | 0 |
| bitwarden | x86_64 | 223 | 115 | 0 | 60 | 21 | 9 | 0 | 6 | 0 | 1 | 0 | 4 | 1 | 0 | 1 | 3 | 2 | 0 | 0 | 0 |
| lmms | x86_64 | 206 | 31 | 0 | 36 | 25 | 55 | 28 | 0 | 3 | 19 | 0 | 0 | 4 | 1 | 0 | 2 | 2 | 0 | 0 | 0 |
| rustdesk | x86_64 | 202 | 110 | 0 | 0 | 25 | 19 | 15 | 0 | 1 | 23 | 0 | 0 | 5 | 0 | 0 | 4 | 0 | 0 | 0 | 0 |
| rectangle | x86_64 | 199 | 103 | 73 | 3 | 0 | 2 | 0 | 10 | 0 | 0 | 0 | 0 | 3 | 0 | 5 | 0 | 0 | 0 | 0 | 0 |
| warp | x86_64 | 197 | 174 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 2 | 0 | 0 | 19 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| android-studio | x86_64 | 192 | 18 | 0 | 35 | 39 | 20 | 11 | 0 | 9 | 17 | 29 | 0 | 2 | 2 | 0 | 0 | 10 | 0 | 0 | 0 |
| monitorcontrol | x86_64 | 184 | 107 | 0 | 0 | 26 | 13 | 9 | 0 | 0 | 10 | 0 | 0 | 2 | 7 | 4 | 3 | 0 | 0 | 0 | 3 |
| pgadmin4 | x86_64 | 181 | 4 | 0 | 6 | 103 | 45 | 0 | 0 | 23 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| libreoffice | x86_64 | 180 | 3 | 0 | 28 | 117 | 22 | 1 | 0 | 4 | 1 | 0 | 1 | 2 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| opencode-desktop | x86_64 | 176 | 0 | 0 | 0 | 176 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| openscad | x86_64 | 175 | 27 | 0 | 19 | 11 | 91 | 0 | 2 | 3 | 1 | 0 | 0 | 3 | 2 | 1 | 15 | 0 | 0 | 0 | 0 |
| dotnet-sdk | x86_64 | 171 | 18 | 0 | 124 | 0 | 0 | 0 | 0 | 29 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| mos | x86_64 | 156 | 107 | 0 | 0 | 24 | 14 | 0 | 0 | 0 | 3 | 0 | 0 | 1 | 0 | 4 | 3 | 0 | 0 | 0 | 0 |
| thunderbird | x86_64 | 152 | 21 | 0 | 48 | 20 | 30 | 16 | 0 | 3 | 3 | 0 | 0 | 8 | 2 | 0 | 1 | 0 | 0 | 0 | 0 |
| webtorrent | x86_64 | 152 | 11 | 0 | 54 | 27 | 39 | 4 | 2 | 0 | 1 | 8 | 0 | 6 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| karabiner-elements | x86_64 | 152 | 86 | 45 | 0 | 0 | 0 | 0 | 3 | 0 | 0 | 0 | 0 | 0 | 12 | 6 | 0 | 0 | 0 | 0 | 0 |
| pencil | x86_64 | 141 | 9 | 0 | 54 | 30 | 33 | 4 | 0 | 0 | 1 | 8 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| vnote | x86_64 | 139 | 30 | 0 | 18 | 12 | 47 | 0 | 0 | 14 | 1 | 8 | 0 | 7 | 0 | 0 | 0 | 2 | 0 | 0 | 0 |
| musescore | x86_64 | 136 | 32 | 0 | 22 | 7 | 45 | 6 | 0 | 14 | 1 | 0 | 0 | 7 | 0 | 0 | 0 | 2 | 0 | 0 | 0 |
| hex-fiend | x86_64 | 126 | 57 | 0 | 6 | 47 | 6 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 1 | 6 | 0 | 0 |
| audacity | x86_64 | 124 | 40 | 0 | 19 | 15 | 17 | 9 | 0 | 14 | 1 | 0 | 0 | 7 | 0 | 0 | 0 | 2 | 0 | 0 | 0 |
| cyberduck | x86_64 | 123 | 17 | 0 | 24 | 25 | 4 | 5 | 2 | 18 | 0 | 23 | 0 | 0 | 0 | 5 | 0 | 0 | 0 | 0 | 0 |
| vlc | x86_64 | 120 | 14 | 0 | 32 | 18 | 21 | 19 | 2 | 0 | 4 | 0 | 0 | 0 | 3 | 1 | 3 | 3 | 0 | 0 | 0 |
| firefox | x86_64 | 119 | 22 | 0 | 13 | 19 | 30 | 19 | 0 | 3 | 3 | 0 | 0 | 7 | 2 | 0 | 1 | 0 | 0 | 0 | 0 |
| floorp | x86_64 | 118 | 21 | 0 | 13 | 18 | 30 | 19 | 0 | 3 | 3 | 0 | 0 | 8 | 2 | 0 | 1 | 0 | 0 | 0 | 0 |
| zen | x86_64 | 118 | 21 | 0 | 13 | 18 | 30 | 19 | 0 | 3 | 3 | 0 | 0 | 8 | 2 | 0 | 1 | 0 | 0 | 0 | 0 |
| mullvad-browser | x86_64 | 117 | 21 | 0 | 11 | 18 | 35 | 16 | 0 | 3 | 3 | 0 | 0 | 8 | 1 | 0 | 1 | 0 | 0 | 0 | 0 |
| librewolf | x86_64 | 117 | 22 | 0 | 12 | 18 | 30 | 19 | 0 | 3 | 3 | 0 | 0 | 7 | 2 | 0 | 1 | 0 | 0 | 0 | 0 |
| tor-browser | x86_64 | 115 | 21 | 0 | 11 | 17 | 35 | 15 | 0 | 3 | 3 | 0 | 0 | 8 | 1 | 0 | 1 | 0 | 0 | 0 | 0 |
| visual-studio-code | x86_64 | 110 | 27 | 0 | 82 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| db-browser-for-sqlite | x86_64 | 110 | 27 | 0 | 14 | 6 | 52 | 0 | 0 | 3 | 1 | 0 | 0 | 4 | 1 | 0 | 0 | 2 | 0 | 0 | 0 |
| gimp | x86_64 | 109 | 0 | 0 | 0 | 93 | 1 | 10 | 0 | 3 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| prusaslicer | x86_64 | 108 | 15 | 0 | 14 | 14 | 51 | 0 | 0 | 0 | 4 | 0 | 0 | 4 | 0 | 1 | 0 | 0 | 3 | 2 | 0 |
| temurin | x86_64 | 106 | 12 | 0 | 21 | 3 | 15 | 7 | 0 | 6 | 12 | 29 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| github | x86_64 | 106 | 21 | 0 | 59 | 8 | 1 | 0 | 0 | 17 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| dbeaver-community | x86_64 | 105 | 12 | 0 | 21 | 2 | 15 | 7 | 0 | 6 | 12 | 29 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| keepassxc | x86_64 | 103 | 27 | 0 | 10 | 12 | 18 | 0 | 0 | 14 | 1 | 13 | 0 | 6 | 0 | 0 | 0 | 2 | 0 | 0 | 0 |
| dolphin | x86_64 | 97 | 29 | 0 | 14 | 9 | 18 | 0 | 0 | 1 | 2 | 0 | 0 | 4 | 18 | 0 | 0 | 2 | 0 | 0 | 0 |
| drawio | x86_64 | 95 | 68 | 24 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 | 0 | 0 | 0 | 0 | 0 |
| docker-desktop | x86_64 | 87 | 37 | 0 | 12 | 8 | 0 | 0 | 0 | 0 | 0 | 15 | 0 | 0 | 14 | 0 | 0 | 0 | 1 | 0 | 0 |
| qbittorrent | x86_64 | 80 | 25 | 0 | 14 | 2 | 18 | 0 | 0 | 12 | 1 | 0 | 0 | 6 | 0 | 0 | 0 | 2 | 0 | 0 | 0 |
| obsidian | x86_64 | 77 | 63 | 0 | 5 | 0 | 1 | 0 | 6 | 0 | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 |
| spotify | x86_64 | 77 | 6 | 0 | 4 | 1 | 1 | 37 | 0 | 21 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 6 | 0 | 0 |
| figma | x86_64 | 67 | 42 | 0 | 1 | 13 | 0 | 9 | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| cursor | x86_64 | 64 | 64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| tic80 | x86_64 | 58 | 2 | 0 | 1 | 24 | 0 | 0 | 0 | 0 | 17 | 0 | 0 | 0 | 13 | 0 | 1 | 0 | 0 | 0 | 0 |
| keka | x86_64 | 49 | 6 | 0 | 7 | 23 | 2 | 0 | 2 | 0 | 0 | 3 | 0 | 0 | 1 | 5 | 0 | 0 | 0 | 0 | 0 |
| git-credential-manager | x86_64 | 46 | 3 | 0 | 21 | 0 | 5 | 0 | 0 | 17 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| zed | x86_64 | 45 | 0 | 0 | 1 | 0 | 7 | 23 | 0 | 0 | 0 | 0 | 0 | 14 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| microsoft-edge | x86_64 | 44 | 1 | 0 | 7 | 12 | 17 | 0 | 0 | 3 | 1 | 0 | 0 | 2 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| openrct2 | x86_64 | 42 | 4 | 0 | 1 | 6 | 0 | 0 | 0 | 0 | 17 | 0 | 0 | 0 | 13 | 0 | 1 | 0 | 0 | 0 | 0 |
| love | x86_64 | 40 | 2 | 0 | 1 | 4 | 0 | 0 | 2 | 0 | 17 | 0 | 0 | 0 | 13 | 0 | 1 | 0 | 0 | 0 | 0 |
| flycut | x86_64 | 38 | 4 | 0 | 21 | 2 | 9 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| transmission | x86_64 | 36 | 6 | 0 | 5 | 3 | 18 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 |
| godot | x86_64 | 36 | 3 | 0 | 0 | 3 | 14 | 2 | 0 | 0 | 2 | 0 | 0 | 0 | 12 | 0 | 0 | 0 | 0 | 0 | 0 |
| pixelorama | x86_64 | 36 | 3 | 0 | 0 | 3 | 14 | 2 | 0 | 0 | 2 | 0 | 0 | 0 | 12 | 0 | 0 | 0 | 0 | 0 | 0 |
| hiddenbar | x86_64 | 35 | 34 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| bruno | x86_64 | 33 | 2 | 0 | 2 | 18 | 3 | 0 | 0 | 0 | 0 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| ollama-app | x86_64 | 32 | 0 | 0 | 2 | 2 | 0 | 28 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| openttd | x86_64 | 30 | 5 | 0 | 0 | 2 | 0 | 19 | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| sublime-text | x86_64 | 28 | 4 | 0 | 2 | 13 | 4 | 0 | 0 | 3 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| macdown | x86_64 | 27 | 5 | 0 | 3 | 2 | 1 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 14 | 0 | 0 | 0 | 0 | 0 |
| steam | x86_64 | 23 | 5 | 0 | 2 | 10 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 | 0 | 0 | 0 |
| cc-switch | x86_64 | 21 | 1 | 0 | 2 | 0 | 10 | 0 | 0 | 0 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| postman | x86_64 | 15 | 1 | 0 | 2 | 1 | 3 | 0 | 0 | 0 | 0 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| brave-browser | x86_64 | 14 | 0 | 0 | 4 | 2 | 1 | 0 | 0 | 5 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 1 | 0 | 0 | 0 |
| google-chrome | x86_64 | 13 | 0 | 0 | 1 | 11 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| appcleaner | x86_64 | 12 | 3 | 0 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5 | 0 | 0 | 1 | 0 | 0 |
| alacritty | x86_64 | 11 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 9 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| antigravity | x86_64 | 8 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| insomnia | x86_64 | 7 | 0 | 0 | 0 | 1 | 0 | 3 | 0 | 0 | 0 | 0 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| keepingyouawake | x86_64 | 5 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 |
| macfuse | x86_64 | 2 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| wezterm | x86_64 | 2 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| blackhole-2ch | x86_64 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| element | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| finicky | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| vscodium | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| dbgate | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| session-manager-plugin | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| discord | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| slack | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| notion | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| google-drive | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

## Ainda não medidos

microsoft-teams, mactex, adobe-acrobat-reader, calibre, podman-desktop, balenaetcher, intellij-idea, dockdoor, windows-app, caffeine, codexbar, thaw, signal, mongodb-compass, openmtp, tableplus, mounty, rancher, freecad, jetbrains-toolbox, shottr, unity-hub, fuse-t, emacs-app, alfred, mactex-no-gui, raspberry-pi-imager, gemini, multipass, ungoogled-chromium, tigervnc, meld, the-unarchiver, protonvpn, handbrake-app, scroll-reverser, xcodes-app, virtualbox, t3-code, cloudflare-warp, finetune, openlogi, anydesk, google-gemini, wechat, microsoft-excel, lulu, handy, microsoft-word, aldente, onedrive, markedit, mysqlworkbench, headlamp, microsoft-office, zotero, background-music, pycharm, vagrant, vivaldi, puremac, clash-verge-rev, blackhole-16ch, notunes, moonlight, dropbox, comfy, sourcetree, anki, helium-browser, applite, freelens, redis-insight, openvpn-connect, microsoft-outlook, syncthing-app, termius, grandperspective, proxyman, gcc-arm-embedded, microsoft-powerpoint, skim, github-copilot-for-xcode, maestro, tunnelblick, crossover, keycastr, upscayl, onyx, fluidvoice, mac-mouse-fix, homebrew-app, macs-fan-control, another-redis-desktop-manager, rstudio, teamviewer, nordvpn, kiro, bettertouchtool, typora, wispr-flow, duckduckgo, affinity, burp-suite, quarto, bambu-studio, fork, tabby, prismlauncher, r-app, wave, android-file-transfer, block-goose, wailbrew, telegram-desktop, easydict, espanso, itsycal, caskhub, datagrip, linear, antigravity-ide, devin-desktop, lens, jellyfin, nextcloud, arduino-ide, realvnc-connect-viewer, lunar, macwhisper, opera, onlyoffice, stremio, middleclick, superwhisper, amethyst, voiceink, beekeeper-studio, cherry-studio, dbx, opensuperwhisper, orion, istat-menus, supacode, capcut, bartender, unnaturalscrollwheels, daisydisk, displaylink, keyclu, coconutbattery, anythingllm, epic-games, thebrowsercompany-dia, syntax-highlight, battery, ubersicht, squirrel-app, cleanshot, cryptomator, feishu, sf-symbols, macpacker, qgis, loop, bbedit, marta, libndi, macdown-3000, orcaslicer, microsoft-auto-update, sfm, quicklook-video, imageoptim
