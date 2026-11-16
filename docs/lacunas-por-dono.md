# Lacunas do corpus por dono

Gerado por `python3 tools/corpus/harness.py donos` a partir dos relatórios do `machocheck` de cada app
(`tools/corpus/varrer-tudo.sh`, libSystem já compilado, sem rodar o build). Cada símbolo ausente vai para a frente
do `TIME.md` dona da biblioteca que o app importa (tabela `FW_DONO` e prefixos em `tools/corpus/harness.py`).
A medida é estática: um símbolo resolvido não quer dizer que a função funciona, e um app com 0 ausentes pode não abrir.

Corpus: **300** apps no manifesto; **25** medidos com Mach-O; **0** falharam na medição (lista no fim); **275** ainda não medidos.

| dono | símbolos distintos | apps afetados | arquivo |
|---|---:|---:|---|
| Leonardo (runtime C/C++) | 711 | 20 | [lacunas/leonardo.md](lacunas/leonardo.md) |
| Eduardo (Foundation/CoreFoundation) | 615 | 20 | [lacunas/eduardo.md](lacunas/eduardo.md) |
| Ricardo (SwiftUI, Combine, UIKit) | 874 | 5 | [lacunas/ricardo.md](lacunas/ricardo.md) |
| Beatriz (AppKit) | 466 | 24 | [lacunas/beatriz.md](lacunas/beatriz.md) |
| Isabela (mídia) | 370 | 14 | [lacunas/isabela.md](lacunas/isabela.md) |
| Natalia (serviços e segurança) | 264 | 20 | [lacunas/natalia.md](lacunas/natalia.md) |
| Mariana (CG, Core Text, ImageIO, PDFKit) | 142 | 18 | [lacunas/mariana.md](lacunas/mariana.md) |
| Fernanda (Swift runtime, Dispatch) | 262 | 11 | [lacunas/fernanda.md](lacunas/fernanda.md) |
| Otavio (OpenGL, Metal, Core Animation) | 136 | 9 | [lacunas/otavio.md](lacunas/otavio.md) |
| Patricia (rede para apps) | 63 | 13 | [lacunas/patricia.md](lacunas/patricia.md) |
| Plugins (flat lookup: o símbolo vem do app que carrega o plugin, não do mOS) | 85 | 2 | [lacunas/plugin.md](lacunas/plugin.md) |
| Renata (BSD/Mach/XPC) | 18 | 14 | [lacunas/renata.md](lacunas/renata.md) |
| Fabio (WebKit/Electron) | 33 | 12 | [lacunas/fabio.md](lacunas/fabio.md) |
| Larissa (plataforma: IOKit, USB, áudio HW) | 33 | 11 | [lacunas/larissa.md](lacunas/larissa.md) |
| Vinicius (arquivos e volumes) | 20 | 3 | [lacunas/vinicius.md](lacunas/vinicius.md) |
| Camila (desktop e window server) | 18 | 2 | [lacunas/camila.md](lacunas/camila.md) |
| Thais (GPU e telas) | 3 | 2 | [lacunas/thais.md](lacunas/thais.md) |
| Felipe (Wi-Fi, Bluetooth, Ethernet) | 2 | 1 | [lacunas/felipe.md](lacunas/felipe.md) |

## Por app

| app | arch | total | leonardo | eduardo | ricardo | beatriz | isabela | natalia | mariana | fernanda | otavio | patricia | plugin | renata | fabio | larissa | vinicius | camila | thais | felipe |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| coteditor | x86_64 | 1447 | 0 | 265 | 792 | 139 | 4 | 65 | 4 | 173 | 0 | 0 | 0 | 0 | 5 | 0 | 0 | 0 | 0 | 0 |
| krita | x86_64 | 517 | 262 | 50 | 0 | 87 | 12 | 22 | 44 | 0 | 11 | 6 | 0 | 10 | 0 | 13 | 0 | 0 | 0 | 0 |
| inkscape | x86_64 | 507 | 482 | 0 | 0 | 6 | 0 | 4 | 10 | 0 | 0 | 3 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 |
| shotcut | x86_64 | 454 | 24 | 58 | 0 | 39 | 136 | 19 | 47 | 0 | 95 | 12 | 0 | 11 | 0 | 13 | 0 | 0 | 0 | 0 |
| blender | x86_64 | 445 | 310 | 3 | 0 | 8 | 104 | 1 | 8 | 0 | 6 | 3 | 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 |
| iterm2 | x86_64 | 367 | 29 | 99 | 11 | 76 | 7 | 55 | 19 | 54 | 0 | 0 | 0 | 1 | 14 | 2 | 0 | 0 | 0 | 0 |
| obs | x86_64 | 342 | 12 | 60 | 0 | 38 | 71 | 17 | 25 | 3 | 5 | 12 | 81 | 10 | 5 | 3 | 0 | 0 | 0 | 0 |
| iina | x86_64 | 268 | 14 | 44 | 34 | 38 | 49 | 11 | 6 | 1 | 12 | 30 | 0 | 4 | 12 | 0 | 12 | 0 | 1 | 0 |
| libreoffice | x86_64 | 238 | 121 | 6 | 0 | 41 | 1 | 29 | 23 | 0 | 0 | 4 | 4 | 8 | 0 | 1 | 0 | 0 | 0 | 0 |
| musescore | x86_64 | 213 | 8 | 57 | 0 | 63 | 6 | 24 | 25 | 0 | 6 | 14 | 0 | 10 | 0 | 0 | 0 | 0 | 0 | 0 |
| thunderbird | x86_64 | 212 | 23 | 25 | 0 | 51 | 16 | 49 | 38 | 0 | 2 | 3 | 0 | 3 | 0 | 2 | 0 | 0 | 0 | 0 |
| monitorcontrol | x86_64 | 211 | 35 | 108 | 0 | 19 | 9 | 0 | 20 | 5 | 0 | 0 | 0 | 0 | 4 | 7 | 0 | 1 | 3 | 0 |
| mos | x86_64 | 182 | 33 | 108 | 0 | 19 | 0 | 0 | 13 | 5 | 0 | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 |
| firefox | x86_64 | 177 | 22 | 26 | 0 | 51 | 19 | 14 | 35 | 0 | 2 | 3 | 0 | 3 | 0 | 2 | 0 | 0 | 0 | 0 |
| vlc | x86_64 | 168 | 22 | 15 | 0 | 40 | 14 | 32 | 0 | 2 | 32 | 0 | 0 | 7 | 1 | 3 | 0 | 0 | 0 | 0 |
| gimp | x86_64 | 148 | 98 | 0 | 0 | 5 | 10 | 1 | 10 | 0 | 0 | 3 | 0 | 9 | 0 | 0 | 12 | 0 | 0 | 0 |
| qbittorrent | x86_64 | 131 | 2 | 50 | 0 | 36 | 0 | 16 | 13 | 0 | 0 | 12 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 |
| stats | x86_64 | 117 | 12 | 9 | 35 | 1 | 0 | 1 | 0 | 27 | 0 | 12 | 0 | 0 | 0 | 10 | 8 | 0 | 0 | 2 |
| rectangle | x86_64 | 109 | 0 | 9 | 73 | 18 | 0 | 1 | 3 | 0 | 0 | 0 | 0 | 0 | 5 | 0 | 0 | 0 | 0 | 0 |
| alt-tab | x86_64 | 81 | 9 | 14 | 0 | 19 | 0 | 4 | 13 | 2 | 0 | 0 | 0 | 0 | 3 | 0 | 0 | 17 | 0 | 0 |
| transmission | x86_64 | 50 | 3 | 6 | 0 | 32 | 0 | 5 | 0 | 2 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 |
| macdown | x86_64 | 32 | 2 | 5 | 0 | 6 | 0 | 3 | 0 | 2 | 0 | 0 | 0 | 0 | 14 | 0 | 0 | 0 | 0 | 0 |
| keepingyouawake | x86_64 | 11 | 0 | 0 | 0 | 7 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 |
| hiddenbar | x86_64 | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| element | x86_64 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

## Ainda não medidos

lmms, openscad, kicad, prusaslicer, kitty, alacritty, hammerspoon, hex-fiend, sequel-ace, godot, love, openrct2, openttd, bitwarden, floorp, zen, tor-browser, mullvad-browser, pixelorama, pencil, webtorrent, deluge, clipy, maccy, flycut, finicky, linearmouse, cyberduck, vscodium, audacity, keepassxc, dbgate, insomnia, tic80, dolphin, vnote, docker-desktop, visual-studio-code, google-chrome, ghostty, git-credential-manager, claude, obsidian, orbstack, tailscale-app, postman, warp, android-studio, macfuse, dbeaver-community, dotnet-sdk, blackhole-2ch, cc-switch, session-manager-plugin, raycast, cursor, zed, opencode-desktop, chatgpt, betterdisplay, spotify, discord, ollama-app, temurin, brave-browser, slack, xquartz, basictex, utm, bruno, localsend, antigravity, zoom, cmux, whatsapp, 1password, notion, sublime-text, lm-studio, karabiner-elements, jordanbaird-ice, microsoft-edge, drawio, github, wireshark-app, appcleaner, db-browser-for-sqlite, openclaw, pgadmin4, steam, telegram, microsoft-teams, mactex, keka, vorssaint, google-drive, adobe-acrobat-reader, figma, wezterm, pearcleaner, librewolf, calibre, podman-desktop, qlmarkdown, balenaetcher, intellij-idea, rustdesk, swiftbar, dockdoor, windows-app, caffeine, codexbar, thaw, signal, mongodb-compass, openmtp, tableplus, mounty, rancher, freecad, jetbrains-toolbox, shottr, unity-hub, fuse-t, emacs-app, alfred, mactex-no-gui, raspberry-pi-imager, gemini, multipass, ungoogled-chromium, tigervnc, meld, the-unarchiver, protonvpn, handbrake-app, scroll-reverser, xcodes-app, virtualbox, t3-code, cloudflare-warp, finetune, openlogi, anydesk, google-gemini, wechat, microsoft-excel, lulu, handy, microsoft-word, aldente, onedrive, markedit, mysqlworkbench, headlamp, microsoft-office, zotero, background-music, pycharm, vagrant, vivaldi, puremac, clash-verge-rev, blackhole-16ch, notunes, moonlight, dropbox, comfy, sourcetree, anki, helium-browser, applite, freelens, redis-insight, openvpn-connect, microsoft-outlook, syncthing-app, termius, grandperspective, proxyman, gcc-arm-embedded, microsoft-powerpoint, skim, github-copilot-for-xcode, maestro, tunnelblick, crossover, keycastr, upscayl, onyx, fluidvoice, mac-mouse-fix, homebrew-app, macs-fan-control, another-redis-desktop-manager, rstudio, teamviewer, nordvpn, kiro, bettertouchtool, typora, wispr-flow, duckduckgo, affinity, burp-suite, quarto, bambu-studio, fork, tabby, prismlauncher, r-app, wave, android-file-transfer, block-goose, wailbrew, telegram-desktop, easydict, espanso, itsycal, caskhub, datagrip, linear, antigravity-ide, devin-desktop, lens, jellyfin, nextcloud, arduino-ide, realvnc-connect-viewer, lunar, macwhisper, opera, onlyoffice, stremio, middleclick, superwhisper, amethyst, voiceink, beekeeper-studio, cherry-studio, dbx, opensuperwhisper, orion, istat-menus, supacode, capcut, bartender, unnaturalscrollwheels, daisydisk, displaylink, keyclu, coconutbattery, anythingllm, epic-games, thebrowsercompany-dia, syntax-highlight, battery, ubersicht, squirrel-app, cleanshot, cryptomator, feishu, sf-symbols, macpacker, qgis, loop, bbedit, marta, libndi, macdown-3000, orcaslicer, microsoft-auto-update, sfm, quicklook-video, imageoptim
