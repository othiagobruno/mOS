# Disco de apps (build/apps.img)

A imagem do sistema (`build/mOS.img`, `tools/build.sh`) leva só o sistema e os apps nativos do mOS. O sistema
ocupa uns 190 MB e a imagem fica perto de 240 MiB. Os apps de terceiros usados nos testes (WhatsApp, ChatGPT, Claude,
Cursor, Visual Studio Code, Setapp, Slack, OpenBot, Grok Bot, Antigravity, Postico e o Codex CLI) somam uns 6,7 GB.
Eles ficam num disco de dados separado, feito só quando pedido.

## Gerar

    tools/build-apps-img.sh            # depois do tools/build.sh (usa build/mkimg e build/BOOTX64.EFI)

O script baixa os apps com o hash conferido (`tools/stage-apps-oficiais.sh`, downloads em `build/apps-dl`), monta
cada um numa árvore à parte, confere os executáveis (`tools/verificar-imagem-apps.py --apps`) e grava `build/apps.img`:
GPT com uma partição de dados (tipo Microsoft basic data, não ESP) e FAT32 rotulado `MOS-APPS`, com os apps em
`/Aplicativos`. O `mkimg --dados MOS-APPS` não põe `\mOS\`, manifesto nem chave, então o kernel nunca o toma por disco de
sistema.

## Usar na VM

O kernel monta todo disco extra em `/Volumes/<rótulo>` (`kernel/dev/storage.c`, `storage_mount_all`). O disco de apps
aparece em `/Volumes/MOS-APPS/Aplicativos`. O kernel vê NVMe, SATA (AHCI) e USB; virtio não. No QEMU, junte ao comando
de boot de sempre:

    -drive file=build/apps.img,format=raw,if=none,id=apps -device nvme,drive=apps,serial=apps

Os testes que abriam `/Aplicativos/<App>.app` da imagem principal passam a abrir `/Volumes/MOS-APPS/Aplicativos/<App>.app`.
Quem prepara a própria imagem com `tools/stage-image.sh --app ...` (testes de um app só) não muda nada.

## Instalador

O dual boot (`tools/instalar-dual-boot.py`) copia o `\mOS` do `build/mOS.img`. Agora isso é só o sistema; os apps de
terceiros não vão para a ESP.
