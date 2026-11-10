# L22 — apps darwin-arm64 no QEMU

Base: `origin/main` `1bbde5d`. Branch de teste `cursor/l22-apps-arm64-qemu-5120`. Sem feature flag. Sem disco físico. Sem segredo no relatório.

PRs **#7**, **#10**, **#11**, **#15**, **#34** e **#55** já estão mergeados nessa `main`. Nada foi trazido de outra branch. `tools/build.sh` e `tools/stage-image.sh` (PR #34) estão no fonte. O harness Linux do PR #55 não entrou: a imagem não chegou a existir.

## Build

`tools/build.sh --out /tmp/mos-linux-build` com zig ELF 0.17.1 para em `userland/libsystem/appkit_extra.c`. O parâmetro `unsigned id` de `cx_status_kernel_click` esconde o tipo `id`:

```
appkit_extra.c:236:7: error: expected ';' after expression
    id it = (id)cx_sb_by_token((int)id);
appkit_extra.c:236:8: error: use of undeclared identifier 'it'
```

O `mkimg` não rodou. O QEMU com OVMF e `-snapshot` não subiu. Não há log serial de app. O texto do compilador está em `docs/l22-build-erro.txt`.

Dono da lacuna: **Interface** (AppKit). Kernel (tradutor, JIT, `macho.c`, `process.c`) e Sistemas (frameworks e símbolos) não foram editados.

## Pacotes

Oficiais, gratuitos, darwin-arm64 (WhatsApp é universal; o slice arm64 tem `cryptid` 0). Nenhum `Contents/SC_Info`. Nenhum `LC_ENCRYPTION_INFO` com `cryptid` diferente de 0. FairPlay não foi a trava e não foi contornado.

| App | Versão | SHA-256 do pacote | SHA-256 do executável |
|---|---|---|---|
| Claude | 4.23118.1 | `a1b69ab28237a34b9894c7fe370492107e3e3b6a6cccb98a258bde27ce98a575` | `52bf315ebffe859840b86a795a930b05e622a7904c2c26124117260654cc3a0e` (universal) |
| Cursor | 3.29.15 | `f78da77be414e5acaf1ea4ffbad81fe4b1ac5c3df64a059346150ba7680d262d` | `b6ea307fca2b7179c6180ce5e72db7ae120595e579672f6d12545bf75ee28821` |
| Slack | 4.53.287 (build 453000287) | `4d4b794cfca076d438122693889179ef6c59066a239413b76df9b51c6dd14082` | `ffed933e6d34f612a4749d375aec2c55358c8bc6ceeb9b9e802d3211cbbc1a92` |
| Grok Bot | 0.79.4 | `fdbe280ff9bf23ad95efb518141a8892fc39f677bef5d6540a73010993ad2ac4` | `824a7706ad7dc3a8ae4e43dc0d64f7156adb9191069fa08d06e2dd6a0963de59` |
| Codex (ChatGPT desktop) | 26.1103.60933 (build 25894) | `1d671e7784e0e379f7293ebcfeac826c2a297524ea806cd55e994068d019e388` | `75d42141ee272cd9a3905b04a2009318fa89730becfd8e8af840f5d994f907ad` |
| VS Code | 1.145.3 | `c81d07705484b6017f34f2b1c106d2817b38e35cbd98adc9dec5c562aa945167` | `82146e63b1572a74e1fb77324c9c99d281de6f232a4844b5066bca2fb45726a3` |
| WhatsApp | 26.45.13 (build 1108933973) | `29c12f980448668107782f6f1b52fe28f60e8a6729032177f6002eedd917cb3f` | `0a24874e339f61a17a731085f37ded1eb08b21a59dc00b7c68428432b6326502` (universal) |
| Codex CLI | 0.167.0 (`rust-v0.167.0`, `codex-aarch64-apple-darwin.tar.gz`) | `4b22a2a971be2791c3e31f912f3e77a88ebd64a65812a7289ccba65dd6425fe9` | `030316ecfbb41f4e7946e584b0063ef333b7a36cd312ba349f2f6869e39b62cc` |

## Testado

Nada. Nenhum app abriu no QEMU.

## Stub

Nada neste ensaio. O stage nem o boot chegaram a classificar símbolo ou janela.

## Não testado

Os oito apps. O mesmo erro de compilação corta todos antes de `tools/stage-image.sh`. Dono: **Interface**, `cx_status_kernel_click` em `userland/libsystem/appkit_extra.c`.
