# Lacunas do corpus por dono

Gerado por `python3 tools/corpus/harness.py donos` a partir dos relatórios do `machocheck` de cada app
(`tools/corpus/varrer-tudo.sh`, libSystem já compilado, sem rodar o build). Cada símbolo ausente vai para a frente
do `TIME.md` dona da biblioteca que o app importa (tabela `FW_DONO` e prefixos em `tools/corpus/harness.py`).
A medida é estática: um símbolo resolvido não quer dizer que a função funciona, e um app com 0 ausentes pode não abrir.

libSystem medido: main ce6dc05c (merge #392), copiado de build do Ricardo 2026-11-16_01:41.

Corpus: **300** apps no manifesto; **10** medidos com Mach-O; **0** falharam na medição (lista no fim); **290** ainda não medidos.

| dono | símbolos distintos | apps afetados | arquivo |
|---|---:|---:|---|
| Eduardo (Foundation/CoreFoundation) | 937 | 7 | [lacunas/eduardo.md](lacunas/eduardo.md) |
| Leonardo (runtime C/C++) | 552 | 7 | [lacunas/leonardo.md](lacunas/leonardo.md) |
| Ricardo (SwiftUI, Combine, UIKit) | 138 | 2 | [lacunas/ricardo.md](lacunas/ricardo.md) |
| Natalia (serviços e segurança) | 120 | 6 | [lacunas/natalia.md](lacunas/natalia.md) |
| Beatriz (AppKit) | 96 | 10 | [lacunas/beatriz.md](lacunas/beatriz.md) |
| Isabela (mídia) | 55 | 4 | [lacunas/isabela.md](lacunas/isabela.md) |
| Camila (desktop e window server) | 39 | 7 | [lacunas/camila.md](lacunas/camila.md) |
| Fabio (WebKit/Electron) | 15 | 6 | [lacunas/fabio.md](lacunas/fabio.md) |
| Fernanda (Swift runtime, Dispatch) | 21 | 4 | [lacunas/fernanda.md](lacunas/fernanda.md) |
| Mariana (CG, Core Text, ImageIO, PDFKit) | 20 | 5 | [lacunas/mariana.md](lacunas/mariana.md) |
| Patricia (rede para apps) | 6 | 3 | [lacunas/patricia.md](lacunas/patricia.md) |
| Larissa (plataforma: IOKit, USB, áudio HW) | 6 | 3 | [lacunas/larissa.md](lacunas/larissa.md) |
| Otavio (OpenGL, Metal, Core Animation) | 7 | 3 | [lacunas/otavio.md](lacunas/otavio.md) |
| Renata (BSD/Mach/XPC) | 6 | 4 | [lacunas/renata.md](lacunas/renata.md) |

## Por app

| app | arch | total | eduardo | leonardo | ricardo | natalia | beatriz | isabela | camila | fabio | fernanda | mariana | patricia | larissa | otavio | renata |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| iterm2 | x86_64 | 848 | 601 | 29 | 65 | 71 | 38 | 7 | 2 | 14 | 10 | 8 | 0 | 2 | 0 | 1 |
| inkscape | x86_64 | 470 | 0 | 461 | 0 | 3 | 1 | 0 | 1 | 0 | 0 | 0 | 3 | 0 | 0 | 1 |
| alt-tab | x86_64 | 255 | 192 | 9 | 0 | 8 | 12 | 0 | 27 | 3 | 2 | 2 | 0 | 0 | 0 | 0 |
| rectangle | x86_64 | 199 | 103 | 0 | 73 | 3 | 2 | 0 | 0 | 5 | 10 | 3 | 0 | 0 | 0 | 0 |
| mos | x86_64 | 156 | 107 | 24 | 0 | 0 | 14 | 0 | 3 | 4 | 0 | 1 | 0 | 0 | 3 | 0 |
| vlc | x86_64 | 120 | 14 | 18 | 0 | 32 | 21 | 19 | 4 | 1 | 2 | 0 | 0 | 3 | 3 | 3 |
| firefox | x86_64 | 119 | 22 | 19 | 0 | 13 | 30 | 19 | 3 | 0 | 0 | 7 | 3 | 2 | 1 | 0 |
| gimp | x86_64 | 109 | 0 | 93 | 0 | 0 | 1 | 10 | 1 | 0 | 0 | 0 | 3 | 0 | 0 | 1 |
| hiddenbar | x86_64 | 35 | 34 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| keepingyouawake | x86_64 | 5 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 | 0 |

## Ainda não medidos

libreoffice, blender, obs, stats, monitorcontrol, iina, transmission, krita, shotcut, thunderbird, coteditor, macdown, element, qbittorrent, musescore, lmms, openscad, kicad, prusaslicer, kitty, alacritty, hammerspoon, hex-fiend, sequel-ace, godot, love, openrct2, openttd, bitwarden, floorp, zen, tor-browser, mullvad-browser, pixelorama, pencil, webtorrent, deluge, clipy, maccy, flycut, finicky, linearmouse, cyberduck, vscodium, audacity, keepassxc, dbgate, insomnia, tic80, dolphin, vnote, docker-desktop, visual-studio-code, google-chrome, ghostty, git-credential-manager, claude, obsidian, orbstack, tailscale-app, postman, warp, android-studio, macfuse, dbeaver-community, dotnet-sdk, blackhole-2ch, cc-switch, session-manager-plugin, raycast, cursor, zed, opencode-desktop, chatgpt, betterdisplay, spotify, discord, ollama-app, temurin, brave-browser, slack, xquartz, basictex, utm, bruno, localsend, antigravity, zoom, cmux, whatsapp, 1password, notion, sublime-text, lm-studio, karabiner-elements, jordanbaird-ice, microsoft-edge, drawio, github, wireshark-app, appcleaner, db-browser-for-sqlite, openclaw, pgadmin4, steam, telegram, microsoft-teams, mactex, keka, vorssaint, google-drive, adobe-acrobat-reader, figma, wezterm, pearcleaner, librewolf, calibre, podman-desktop, qlmarkdown, balenaetcher, intellij-idea, rustdesk, swiftbar, dockdoor, windows-app, caffeine, codexbar, thaw, signal, mongodb-compass, openmtp, tableplus, mounty, rancher, freecad, jetbrains-toolbox, shottr, unity-hub, fuse-t, emacs-app, alfred, mactex-no-gui, raspberry-pi-imager, gemini, multipass, ungoogled-chromium, tigervnc, meld, the-unarchiver, protonvpn, handbrake-app, scroll-reverser, xcodes-app, virtualbox, t3-code, cloudflare-warp, finetune, openlogi, anydesk, google-gemini, wechat, microsoft-excel, lulu, handy, microsoft-word, aldente, onedrive, markedit, mysqlworkbench, headlamp, microsoft-office, zotero, background-music, pycharm, vagrant, vivaldi, puremac, clash-verge-rev, blackhole-16ch, notunes, moonlight, dropbox, comfy, sourcetree, anki, helium-browser, applite, freelens, redis-insight, openvpn-connect, microsoft-outlook, syncthing-app, termius, grandperspective, proxyman, gcc-arm-embedded, microsoft-powerpoint, skim, github-copilot-for-xcode, maestro, tunnelblick, crossover, keycastr, upscayl, onyx, fluidvoice, mac-mouse-fix, homebrew-app, macs-fan-control, another-redis-desktop-manager, rstudio, teamviewer, nordvpn, kiro, bettertouchtool, typora, wispr-flow, duckduckgo, affinity, burp-suite, quarto, bambu-studio, fork, tabby, prismlauncher, r-app, wave, android-file-transfer, block-goose, wailbrew, telegram-desktop, easydict, espanso, itsycal, caskhub, datagrip, linear, antigravity-ide, devin-desktop, lens, jellyfin, nextcloud, arduino-ide, realvnc-connect-viewer, lunar, macwhisper, opera, onlyoffice, stremio, middleclick, superwhisper, amethyst, voiceink, beekeeper-studio, cherry-studio, dbx, opensuperwhisper, orion, istat-menus, supacode, capcut, bartender, unnaturalscrollwheels, daisydisk, displaylink, keyclu, coconutbattery, anythingllm, epic-games, thebrowsercompany-dia, syntax-highlight, battery, ubersicht, squirrel-app, cleanshot, cryptomator, feishu, sf-symbols, macpacker, qgis, loop, bbedit, marta, libndi, macdown-3000, orcaslicer, microsoft-auto-update, sfm, quicklook-video, imageoptim
