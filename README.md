# mOS

**mOS** é um kernel x86_64 que dá boot UEFI (`EFI/BOOT/BOOTX64.EFI`, sem GRUB) e um desktop próprio no estilo do Mac. Por cima disso há uma camada para carregar apps de macOS: Mach-O x86_64 direto e Mach-O ARM64 pelo tradutor `a64rt` (`userland/a64` + `userland/a64rt`), que junta interpretador e JIT. O processo traduzido é inteiro ARM64; a fronteira com o kernel é a syscall. As bibliotecas que essa camada oferece incluem libSystem, AppKit, SwiftUI, um runtime para apps Electron e um WebKit (`WKWebView`) em cima do motor do Horizonte.

O desktop (barra de menus, Dock, janelas, tema claro/escuro) é desenhado pelo próprio sistema. A arte é original; a fonte da interface é a Inter (SIL OFL). "Mac" e "macOS" são marcas da Apple Inc. O mOS não é afiliado nem endossado pela Apple e não contém código, ícones, fontes ou imagens da Apple.

Nada abaixo é uma promessa de que um app abre. Cada área separa o que um teste registrou (onde e como), o que é stub e o que não foi executado. Os números saem dos docs da `main`, do código e dos PRs já mergeados. Esta página não reexecutou a suíte.

## Arquitetura

| Camada | Onde | O que é |
|---|---|---|
| Boot UEFI | `kernel/boot` | GOP, ACPI, cópia de `\mOS\` para a RAM, `ExitBootServices` |
| Núcleo | `kernel/arch`, `kernel/mm`, `kernel/proc` | GDT/IDT, paginação, processos em ring 3, syscalls `int 0x80`, ELF64, LAPIC/SMP |
| Drivers e rede | `kernel/dev`, `kernel/net`, `kernel/wifi`, `kernel/bt` | Vídeo, entrada, áudio, USB, AHCI, NVMe, TCP/IP, TLS, 802.11, Bluetooth |
| Desktop | `kernel/gfx`, `kernel/ui`, `kernel/apps` | Compositor, window server, apps do próprio sistema |
| Arquivos | `kernel/fs` | FAT, GPT, HFS+/APFS só leitura em `.dmg`, Launch Services |
| Compatibilidade | `kernel/compat`, `userland/libsystem`, `userland/frameworks` | Carregador Mach-O, libSystem, AppKit, frameworks |
| Tradutor ARM64 | `userland/a64`, `userland/a64rt` | Interpretador + JIT; página de convidado de 16 KiB |
| Swift / SwiftUI / WebKit | `userland/sdk`, `userland/frameworks` | Runtime Swift, OpenSwiftUI, `WebKit.framework` |
| Apps de fábrica | `root/mOS`, `root-live/mOS` | Árvore copiada para a imagem; o Live não entra no payload de instalação |
| Imagem e testes de host | `tools/`, `docs/` | `mkimg`, `build.ps1`, `machocheck`, harnesses |

O inventário dos apps do próprio desktop está em [docs/apps-sistema.md](docs/apps-sistema.md). O contrato do tradutor está em [docs/tradutor-arm64.md](docs/tradutor-arm64.md).

## Compilar

O fluxo que gera a imagem completa é o do Windows. Ele precisa do [Zig](https://ziglang.org/download/) 0.17.1 como `zig cc` (executável PE/MZ em `.tools\zig-*\zig.exe`, ou `MOS_ZIG`). Um binário de macOS dentro de `.tools\zig-*` é ignorado de propósito. Para abrir no QEMU, o script procura `qemu-system-x86_64` em `C:\Program Files\qemu`.

```powershell
.\build.ps1          # build\mOS.img (GPT + FAT32) e build\esp\
.\build.ps1 -Run     # compila e abre no QEMU
.\build.ps1 -Tests   # inclui o autoteste de userland; o log serial imprime bench: e as linhas do teste
.\build.ps1 -UnitTests   # kernel com -DMOS_UNIT_TESTS; o serial imprime unit:
```

O kernel é ligado com `zig cc -target x86_64-uefi-msvc` (`-O2`, `-ffreestanding`, `-fno-stack-protector`, `-mno-red-zone`, `-fshort-wchar`, `-nostdlib`, `-Ikernel/include`, os `.c` e `.S` de `kernel/`). O `mkimg` no Windows sai como `build\mkimg.exe` (`zig cc -target x86_64-windows-gnu -lbcrypt`). O script também monta o `libSystem.mos` antes do kernel.

### Linux

Não há, na `main`, um script que monte o `mOS.img` inteiro. O alvo do kernel é o mesmo do `build.ps1`. O `tools/mkimg.c` é C de host; o ensaio documentado em [docs/instalacao-runtime.md](docs/instalacao-runtime.md) (PR #6, árvore de 3 arquivos, 32 MiB, sem a imagem do desktop) é:

```bash
gcc -O2 -o /tmp/mkimg tools/mkimg.c
/tmp/mkimg /tmp/mos-fw.img /tmp/mos-fw-tree 32
```

O que já está na `main` e rodou em Linux são harnesses de host. Eles não sobem o kernel:

| Harness | Comando (está no fonte ou no PR) | Resultado registrado |
|---|---|---|
| Monitor exclusivo do tradutor | `clang -O2 -pthread` em `tools/a64xmon_host.c` (PR #33) | `a64xmon: ok` |
| Invalidação do JIT e pilha do host | `clang` em `tools/a64hostcheck.c` (PR #7) | `a64hostcheck: ok` |
| CTLine / CFHost | `clang -DCT_HOST` com `tools/ctline_host_stubs.c` (PR #32) | saída 0; não é o libSystem freestanding |
| Computer use | `gcc` de `userland/tests-appkit/computeruse_host.c` (PR #20) | `computer-use: 30/30 ok` |
| Tabela / toolbar do AppKit | `gcc -DCX_TV_HOST` (PR #30) | `appkit-table: 70/70 ok` |
| Barra de status | `gcc -DCX_SB_HOST` (PR #5) | `appkit-status: 38/38 ok` |
| `dlclose` / `remove_image` | comando no topo de `tools/dyld_remove_test.c` (PR #19) | `dyld-remove: 11/11 ok` |
| AIR | comando no topo de `tools/airlower_test.c` (PR #31) | 0 falhas no diferencial contra o fonte; sem `.metallib` de app |
| Preferências, TCC, formato | `tools/prefstest.c`, `tcctest.c`, `fmttest.c` e os vizinhos (PR #8) | 72/72, 18/18, 94/94 no gcc 13 |

O build e o stage da imagem no Linux estão no PR #34, aberto e não mergeado. Ver [Em andamento](#em-andamento).

## Rodar no QEMU com OVMF

No Windows, `.\run.ps1` (também chamado por `.\build.ps1 -Run`) sobe `qemu-system-x86_64` com `-machine q35`, `-cpu max`, `-smp 4`, `-m 1024` e o firmware somente leitura em `C:\Program Files\qemu\share\edk2-x86_64-code.fd` (`-drive if=pflash`). A imagem padrão é `build/mOS.img` no IDE. `-Headless` tira a janela, liga o QMP em `tcp:127.0.0.1:4444` e grava o serial. `-Accel whpx` usa o hipervisor do Windows e tira VMX, SVM e AVX do CPUID do convidado (o OVMF e o Bun do Claude Code quebram com essa combinação). Secure Boot fica desligado: o EFI não é assinado.

O ensaio de `MAP_SHARED` e NX que entrou na `main` pelo PR #27 não usa esse `mOS.img`. O comando que produziu o log, com ESP vvfat descartável e OVMF 4M, é:

```bash
qemu-system-x86_64 -machine q35 -m 1024 -cpu max -smp 1 \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=/tmp/ovmf-vars.fd \
  -drive file=fat:rw:/tmp/esp-mm,format=raw,if=ide,index=0 \
  -vga std -display none -serial file:/tmp/mmshared-serial.log \
  -nic none -no-reboot
```

O QEMU não desliga sozinho; nesse PR um `timeout 90` encerrou com 124. O serial registrou `mmshared: TODOS OS TESTES OK` e `ok  mmshared: MAP_SHARED, msync, munmap parcial e NX`. A receita que montou `/tmp/esp-mm` não está num script da `main`. O programa é `userland/mmshared.c`; o `userland/teste.c` o dispara quando o binário está na imagem (`.\build.ps1 -Tests`).

## Gravar no pendrive

**Imagem:** grave `build\mOS.img` com o Rufus em modo DD ou com o balenaEtcher. Isso apaga o pendrive.

**Arquivos:** FAT32 (GPT ou MBR) e copie `build\esp\` (`EFI` e `mOS`) para a raiz.

No firmware, escolha o pendrive em modo UEFI e deixe o Secure Boot desligado.

### Primeiro boot

O pendrive abre o assistente antes do login (idioma pt-BR/English, teclado ABNT2/US) e oferece:

- **Live:** a conta fica só na RAM.
- **Live com persistência no pendrive:** só depois de consentimento; grava na partição do pendrive. Não há criptografia.
- **Instalar:** pergunta o disco, se usa o disco inteiro, o espaço livre ou uma partição já vazia, e se formata. Antes da primeira gravação é preciso digitar o identificador do alvo (`sata0`, `sata0p2`). A cópia vem de um payload com manifesto SHA-256 e o `BOOTX64.EFI` é o último arquivo. Não há redimensionamento nem ESP compartilhada.

Se o firmware não entregar um caminho de boot que o mOS consiga amarrar ao disco, a sessão fica só na RAM e a instalação fica bloqueada. Sem RDRAND/RDSEED o mOS não cria senha. A senha da conta não cifra o disco.

## Atalhos

| Atalho | Ação |
|---|---|
| Tecla Windows + Espaço, ou Ctrl + Espaço | Busca |
| ⌘Q / ⌘W / ⌘M | Encerrar app / fechar janela / minimizar |
| ⌘Tab | Alternar apps |
| ⌘, | Preferências |
| ⌥⌘Esc | Forçar encerrar |

## Compatibilidade com apps de macOS

"Ausente" no `machocheck` (`tools/machocheck.c`) é um import que não está no `libSystem.mos` nem no próprio bundle, e não é weak. Os binários dos apps não estão no repositório. FairPlay e qualquer outro DRM ficam como limitação: o instalador recusa um bundle com `Contents/SC_Info` (`ls_install_app` devolve -2, PR #6) e não há descriptografia.

O WhatsApp **não** é Electron. É Swift + SwiftUI + UIKit Catalyst. Está na seção de nativos.

### Electron e cascas parecidas

Nenhum destes foi aberto de ponta a ponta nesta atualização do README. Onde uma sessão anterior registrou uma tela, a data e o doc estão na linha.

| App | O que foi medido | O que um teste executou | Stub ou fora do ensaio | Não testado |
|---|---|---|---|---|
| **VS Code** 1.145.2, Electron 43.8.3 | [docs/checklist-compat.md](docs/checklist-compat.md): 2166 → 690 imports sem implementação no bundle. Com 0 ausentes nessa medição: Electron Framework, os 4 Code Helper, libffmpeg, Squirrel, Mantle, ReactiveObjC e os `.node` pty, spdlog, sqlite3, keymapping, policy-watcher, fs-copyfile, watchdog, os-proxy-resolver e kerberos. CryptoKit, no 1.145.1 (PR #29): 46 imports / 39 símbolos distintos passaram a 46/0. Não é um total novo do bundle | [docs/auto-hospedagem.md](docs/auto-hospedagem.md), 8 nov., QEMU com WHPX: o editor com terminal integrado; dentro da VM o zig 0.17.1 gerou libSystem e kernel byte a byte iguais aos do Windows | Bibliotecas opcionais do Copilot (MSAL/OneAuth). Sem GPU: o compositor é CPU | O PR #29 não deu boot. As sessões de 10 nov. não reabriram o editor |
| **Cursor** 3.29.9 medido; cask citado 3.29.15. Electron 43.8.4, fork do VS Code, fuses `101100011` | [docs/compat-apps.md](docs/compat-apps.md), 6 nov.: 2378 → 2089 ausentes (libc++ 279, Swift 237 só no `cursor-update-supervisor`, Security 84). Varredura WebKit de 10 nov. ([docs/webkit-apis-alvo.md](docs/webkit-apis-alvo.md)): o editor não liga WebKit; `libmsalruntime_x64.dylib` do 3.29.15 importa `WKWebView` | Não há sessão posterior que abra o editor | Mesmo caminho de renderer/IPC do VS Code. `NSDockTile` não é o que este app pede | Login, git de 619 MiB na imagem e `/usr/bin/security` continuam na lista de [docs/pendencias-apps-agentes.md](docs/pendencias-apps-agentes.md) |
| **Slack** 4.53.280 | compat-apps, 7 nov.: 1709 → 1534 → **32** ausentes. O doc atribui esses 32 a libc++ (28), libcompression (3) e libz (1), dylibs reais que o `machocheck` não enxerga | Não reexecutado nos PRs de 10 nov. | Integridade de asar (fuses `010011001`) depende de `ElectronAsarIntegrity` no `NSBundle` | O app inteiro não foi reaberto. A varredura WebKit do 4.53.287 não achou `WebKit.framework` |
| **Claude Desktop** 4.23118.0, Electron 44.9.3 | compat-apps, 6 nov.: 4279 → 4040 ausentes. Os extras grandes são Swift (1256) e SwiftUI (547) em `swift_addon.node`, `computer_use.node` e no simulador de iOS | Não reexecutado em 10 nov. | Fuses `010011011`: asar com hash e cookies no Keychain | Login e o helper de computer use de verdade. A varredura WebKit do 4.23118.1 só viu WebKit em `libmsalruntime_arm64.dylib` |
| **Grok Bot** 0.79.2 medido; cask 0.79.3. Electron 42.11.16, fuses `010011011` | compat-apps, 6 nov.: 2562 → 2393 ausentes. Swift 541 no helper Computer Use | [docs/pendencias-apps-agentes.md](docs/pendencias-apps-agentes.md), 8 nov.: tela de login em pt-BR; clique sintético e captura com a Calculadora aberta pelo Dock. O helper "liga todos os símbolos" nessa nota | Asar com integridade; cookie "Safe Storage" no Keychain, sem cifra de disco | O PR #20 não fez login. O doc de 8 nov. ainda lista o helper subindo a partir do próprio Grok, depois do login |
| **ChatGPT desktop** (sucessor do Codex) 26.1103.60933 | Só o diretório central e o `Info.plist` do zip oficial, por HTTP range, em 8 nov. ([docs/compat-apps.md](docs/compat-apps.md)). Casca Electron (`app.asar`, `owl-electron-app.json`) sobre `Codex Framework.framework` 157.0.8103.39 (`BrowserCrApplication`). Não há `Electron Framework.framework`. O Codex medido em 6 nov. (26.623.141536) era Chromium 149: 2481 → 2231 ausentes | Nenhum. O zip não foi instalado | `NSDockTile` do `CodexDockTilePlugin` não existe no libSystem. Renderer do Chromium 157 | Login, Sparkle, helpers GPU/Renderer e o plugin de autorização do Computer Use |
| **OpenBot** 0.66.0, Electron 44.9.3, fuses `010011001` | [docs/compat-open-usage.md](docs/compat-open-usage.md), 10 nov., `machocheck` no host contra um `libSystem.mos` de 6977 exports. Cada arquivo uma vez: **16495** imports, **1344** ausentes, **844** símbolos distintos. Electron Framework, helpers, libffmpeg, Mantle e ReactiveObjC fecham com 0. Os 1344 estão em sete imagens (`cua-driver`, Sunshine, voz/ONNX, `koffi.node`, Squirrel). `LC_ENCRYPTION_INFO` com cryptid ≠ 0: 0 nos slices lidos. Não é pacote da Mac App Store | O `stage-electron.ps1` nesse Linux **não** produziu imagem (`mkimg FAILED`; o prefixo `\\?\` do Windows) | Quase todos os ausentes estão em pastas que o `stage-electron.ps1` nem copia | O app não rodou. O slice ARM64 do `cua-driver` não entrou no `machocheck` |

### Codex CLI

Binário Rust oficial 0.167.0 (`codex-x86_64-apple-darwin`), não Electron.

- **Executado no QEMU** (texto ainda em [docs/compat-apps.md](docs/compat-apps.md), sessão anterior ao PR #16; imagem com `tools/stage-codex-cli.ps1`, 1,5 GB): `codex --version` imprime `codex-cli 0.167.0` e sai 0; `codex login status` imprime `Not logged in`. `codex login --device-auth` negocia TLS 1.3 com `auth.openai.com` e mostra o texto do device code. Ninguém completou o login.
- **`machocheck` refeito** (PR #16, 10 nov., host, libSystem com 6975 exports): **630 imports, 0 símbolos distintos ausentes**, 13/13 bibliotecas `[ok]`. Esse PR não subiu o QEMU de novo.
- **Stub no mesmo doc:** `FSEventStream*` sobe e não entrega evento; PAC de proxy devolve nulo; `IOPMAssertion*` aceita porque o sistema não dorme sozinho; `copyfile_state_get`, `acl_get_*`, `fclonefileat`, `fsctl` e `fsetattrlist` devolvem erro. `sandbox-exec` executa o comando e ignora a política.
- **Não testado no PR #16:** o ponto do login dentro da VM. No harness `machotest` (Windows) o processo para em `failed to create UnixStream` porque esse harness não emula `socketpair`; na VM o par é um pipe do kernel.

O Claude Code CLI (Bun, não o app Electron) tem registro próprio no mesmo doc, 7 nov.: `claude --version` imprime `2.1.311 (Claude Code)` e sai 0 em WHPX e em TCG; `claude --help` sai 0 só no TCG. O `machocheck` daquela sessão: 356 ausentes. O modo interativo e o login não foram testados. Sob WHPX o `--help` cai em AVX (`#UD`); o `run.ps1 -Accel whpx` tira o AVX do CPUID.

### Nativos

| App | Medido | Executado | Stub | Não testado |
|---|---|---|---|---|
| **Postico** 2.3.5 | [docs/postico-whatsapp-pendencias.md](docs/postico-whatsapp-pendencias.md), 8 nov.: **139** símbolos distintos ausentes no `machocheck`, nenhum de SwiftUI. A lista ainda cita `CFHost*` e `CTLine*` | Harness daquela nota: 33 imagens, alerta do Sparkle, janela principal 950×500, nib da lista de favoritos. A execução seguinte, com o nome de símbolo corrigido, foi interrompida e não há log novo | PR #30 (host, sem o binário): célula, toolbar de 28 pt e divisória. `setCenteredItemIdentifiers:` guarda a lista e o layout continua à esquerda. PR #32: oito símbolos de `CTLine`/`CTTypesetter` e sete de `CFHost` mais `NSHost` existem no fonte; o teste de host saiu 0. Limites no próprio PR: sem grafema de ICU, tabulação fixa, avanço substituto sem a fonte do kernel, `CFGetTypeID` de CFHost continua 1 | O `machocheck` **não** foi refeito depois do PR #32, então o 139 não foi atualizado. Não houve VM com o binário nesses PRs |
| **WhatsApp** 2.26.44.21 no inventário; 2.26.45.13 nas contagens de formato | compat-apps, 7 nov.: ausentes do bundle **3105** com a `libc++.1.dylib` do LLVM no diretório varrido (a série começa em 8922). Formatação (PR #8, executável principal do 2.26.45.13): **204** símbolos medidos, **0** desses 204 ausentes depois de `fi_swift.c`. Desses, **17** têm miolo (data, ISO8601, `NSDecimal`, `Measurement`); **187** resolvem e devolvem string vazia. `libswiftFoundation.dylib`: 1057 imports, 884 ainda sem implementação | Harness Windows de 7 nov. (`MACHOTEST_TOLERANT`, raiz `build\wa-run`): carrega 33 imagens e cria banco SQLite. Não é a janela de conversa | SwiftUI: a nota de 8 nov. ainda lista **136** símbolos ausentes. O PR #12 acrescentou `NSHostingView` (List, ScrollView, TabView, popover, vidro de fallback) sem refazer esse `machocheck` | QEMU com o app. O PR #8 diz explicitamente que o `machocheck` não significa que o WhatsApp abre |
| **Setapp** | A página de download não publica o framework ([docs/compat-apps.md](docs/compat-apps.md)). A varredura WebKit de 10 nov. leu um universal **4.0.0 (165)** cujo executável importa `WKWebView` e implementa o delegate de histórico. Não há total de `machocheck` do cliente | Nenhum | Helpers `SetappAgent`, `SetappLauncher` e `SetappUpdater` aparecem em relatos de terceiros, não num ensaio daqui. Não é app da Mac App Store | O cliente não foi iniciado. Login no navegador não foi feito |

### Usage for Claude & ChatGPT (App Store)

Ficha [id 6755173244](https://apps.apple.com/us/app/usage-for-claude-chatgpt/id6755173244), "version 4", 31,4 MB, nativo (iPhone, iPad, Mac, Watch, visionOS). Não foi baixado: não há binário público para `machocheck` ([docs/webkit-apis-alvo.md](docs/webkit-apis-alvo.md) e [docs/compat-apps.md](docs/compat-apps.md)).

A Mac App Store assina esses apps com FairPlay. Isso é uma limitação documentada. O mOS não a contorna: não há descriptografia, o instalador recusa `Contents/SC_Info`, e o StoreKit local não fala com a Apple nem valida recibo (`cx_storekit_verify_receipt` devolve -1). Uma frase de "login embutido" no site não é evidência de símbolo.

O Usagebar de usagebar.com (US$ 9, Stripe) é outro produto. Também não há pacote gratuito no repositório ([docs/compat-open-usage.md](docs/compat-open-usage.md)). A varredura WebKit cita um "UsageBar 0.8.4" só por causa do Sparkle embutido; isso não é o app da App Store e não tem contagem de bundle nesta página.

## Computer use

As APIs usadas pelos helpers do Grok Bot, do Claude Desktop e do ChatGPT estão em `userland/libsystem/cgevent.c`, `screencapturekit.c` e no window server (`DEV_SCREEN_CAPTURE`, `DEV_WINDOW_LIST`). A permissão é o TCC local (`userland/libsystem/tcc.c`): sem `kTCCServiceScreenCapture` a captura devolve imagem nula e a lista de janelas vem vazia; sem `kTCCServiceAccessibility` o AX devolve -25211.

**Executado no host, não no app** (PR #20, `gcc` de `computeruse_host.c`, dois processos e memória compartilhada): `computer-use: 30/30 ok`. O processo de amostra pinta uma janela; o outro lê pixels, a geometria, posta um clique e lê o texto por AX. O `tools/qemu-test.ps1` não rodou nesse PR. O `tools/tcctest.c` do PR #8 deu 18/18 para o arquivo de decisões, também no host.

**Implementado nesse caminho, sem o helper real:**

- `CGDisplayCreateImage`, `CGDisplayCreateImageForRect`, `CGWindowListCreateImage` e `SCScreenshotManager` leem o retângulo do compositor.
- `CGWindowListCopyWindowInfo` preenche número, pid, dono, nome, bounds, layer e onscreen, da frente para o fundo.
- `CGEventPost` entra na fila global. `CGEventPostToPid` ignora o pid.
- AX do **próprio** processo, com a concessão: role, title, value, position, size, children, `AXPress`, `AXValue`.

**Stub:**

- AX de **outro** processo: o elemento é criado e a leitura devolve -25204. Não há transporte entre processos.
- `SCStream` e `CGDisplayStream`: não há vídeo contínuo.
- `SCShareableContent` com janela âncora não filtra a lista.
- `AXObserver` e atributo parametrizado: -25208 com permissão.
- Microfone e câmera (PR #31): o portão de TCC existe; sem concessão a API erra e não captura. `AVCaptureSession` fica `isRunning` sem quadro. Não houve VM nem hardware.

**Não testado:** computer use com login no Grok Bot, no Claude Desktop ou no ChatGPT. A nota de 8 nov. do Grok (clique sintético, Calculadora) é outra sessão e não foi repetida no PR #20.

## Testes

Como repetir o que já está scriptado. Os resultados são os últimos publicados; esta página não os reexecutou.

| Suíte | Como rodar | Último resultado publicado | Origem |
|---|---|---|---|
| `a64fuzz` | No Windows, `build\a64fuzz.exe` (o harness `tools/a64test.c` é Windows). Flags em [docs/tradutor-arm64.md](docs/tradutor-arm64.md): `--grupo`, `--casos`, `--k`, `--semente`, `--jit` | Placar de `tools/a64placar.ps1` em **2026-11-08 16:22**, semente 20261115, 2000 casos/grupo, com JIT, oráculo QEMU 11.1 `-cpu max`. INT: 65764/65764 execuções batem, 0 divergências, 1 "devia ser UDF". FP: 15545/15545, 0 divergências. SIMD: 72357 batem de 72359 execuções, 0 divergências, 2 na coluna FMA?. Sequências K=8: 27900 casos, 2 divergentes, 0 JIT diferente do interpretador | O arquivo do placar na `main`. O PR #28 (10 nov., QEMU 11.1.0 e LLVM 23.1.5) reclassificou essas três sobras: o UDF de `br.reg` é um `STXR` no rascunho com monitor vazio (o ARM deixa indefinido); as duas sequências são `CMPBR`, recurso que o tradutor não anuncia e o QEMU executa; os dois FMA? batem com o QEMU 11.1 do Linux. O JIT não foi reexecutado nesse PR e o placar do arquivo não foi regerado |
| Monitor exclusivo | `tools/a64xmon_host.c` no host | `a64xmon: ok` (ABA, `STXR`, litígio store-buffer, JIT com `STR` entre `LDXR` e `STXR`) | PR #33. A troca de contexto em `kernel/proc/sched.c` só passou em `clang -fsyntax-only`. `tools/a64tests/int_xmon.S` não rodou no harness QEMU |
| `mmshared` | Comando OVMF do PR #27, acima; ou `.\build.ps1 -Tests` quando o binário está na imagem | `mmshared: TODOS OS TESTES OK` (escrita visível no outro mapeamento, `msync` parcial, `munmap` parcial, `MAP_SHARED\|MAP_ANON`, NX entregue como SIGBUS, W^X do JIT) | PR #27. Paginação para disco não existe ([docs/mm-shared.md](docs/mm-shared.md)) |
| `machocheck` / `machotest` | Host: `gcc -O2 -o machocheck tools/machocheck.c` e `./machocheck libSystem.mos <app> relatorio.txt`. No Windows o `porte-vscode.md` usa `zig cc -target x86_64-windows-gnu`. `tools/machotest.c` é harness Windows (`ExitProcess`); não sobe o kernel | Codex CLI: 630 imports, 0 ausentes (PR #16). OpenBot: 1344 ausentes com cada arquivo uma vez (doc de 10 nov.). VS Code: 690 no checklist, e CryptoKit 39 → 0 no PR #29 | Cada linha da tabela de apps cita o doc. O `machotest` não emula porta Mach nem `socketpair` |
| Launch Services (`unit:` / LS-TEST) | `.\build.ps1 -UnitTests` e boot. Os casos estão em `kernel/fs/unit_tests.c` e `kernel/fs/ls_open_tests.c`, só com `-DMOS_UNIT_TESTS` | `unit: RESUMO: 146 verificações ok, 0 FALHA(S) — APROVADO` | PR #19. Imagem com `libSystem.mos` fictício de 64 bytes, OVMF, IDE, `-snapshot`. Não é o desktop completo. No host, `dyld-remove: 11/11 ok` |
| WebKit | `.\build.ps1 -OutDir build-wk`, `.\tools\build-webkittest.ps1 -OutDir build-wk`, `.\run.ps1 -Headless -Image build-wk\mOS.img`. Procurar `webkit:` no serial | [docs/falta-compat.md](docs/falta-compat.md) registra **55/55** no QEMU. O `userland/tests-webkit/main.m` da `main` tem casos a mais (recusa de histórico, `loadData`, `WebArchive`) acrescentados no PR #24, e esse PR **não** rodou a VM | O 55/55 é anterior a esses casos. O fonte imprime `webkit: N/M ok`; o N atual não foi medido de novo |
| Rede / TLS / XPC | `tools/build-nettest.ps1`, `build-tlstest-app.ps1`, `build-nettest6.ps1`, `build-xpctest.ps1`, `build-udptest.ps1` | TCP 16/16, TLS 16/16, IPv6 11/11, XPC 24/24 no `machotest`, UDP 34/34 no QEMU | [docs/compat-apps.md](docs/compat-apps.md), [docs/ipv6.md](docs/ipv6.md), [docs/falta-compat.md](docs/falta-compat.md). O checklist ainda diz "falta UDP" numa linha antiga; o `falta-compat.md` é o que traz o 34/34 |
| Tradutor na VM | `userland/tests-a64rt`, `userland/tests-a64libsys` | 21/21 nos dois motores (QEMU WHPX, 4 CPUs, e `machotest`); `tests-a64libsys` 36/36 | [docs/tradutor-arm64.md](docs/tradutor-arm64.md), 8 nov. App SwiftUI ARM64 de ponta a ponta continua de fora |
| Áudio | `tools/build-audiotest.ps1` (VM); `tools/audiodec_test.c` (host, precisa de ffmpeg) | 42/42 no QEMU é resultado antigo e **não** foi reexecutado. MP3 (minimp3) e ALAC 16 bits passaram no host; o ALAC bate bit a bit com o ffmpeg no seno de teste do doc | [docs/falta-compat.md](docs/falta-compat.md), [docs/midia-licencas.md](docs/midia-licencas.md). AAC e Opus são reconhecidos e não viram PCM |

## Limitações conhecidas

- Hardware real: som, USB, Bluetooth, SATA, NVMe, rede e a instalação foram ensaiados no QEMU/OVMF, não numa máquina física. Wi-Fi real depende de driver por chipset (`kernel/wifi/`); o que existe foi exercitado com rádio simulado. WPA3 não entra. Bluetooth LE/clássico não rodou num adaptador: o QEMU não emula um.
- Sem GPU. O desenho pesado usa todos os núcleos (`par_for` em `kernel/arch/smp.c`) e SSE2. O número ~2,3× em 4 núcleos é o que o README anterior atribuía ao log `bench:` de `.\build.ps1 -Tests`; não foi remediado aqui.
- Página presente no x86 é legível. Não há `PROT_NONE` na PTE. Não há swap ([docs/mm-shared.md](docs/mm-shared.md)).
- Volumes `.dmg` são só leitura. FAT/HFS+ de outros discos aparecem em `/Volumes` só leitura até um consentimento por volume. Partição EFI, disco do sistema e HFS+ com journal pendente continuam recusados.
- Keychain e preferências gravam arquivo sem criptografia. Quem lê o disco lê a senha.
- TLS do kernel: 1.3 e 1.2, ECDHE, AES-GCM e ChaCha20-Poly1305, cadeia até as raízes da Mozilla. OCSP só se vier grampeado. Sem CBC e sem RSA estático. O SecureTransport dos apps é cliente, sem ALPN e sem certificado de cliente.
- Navegador Horizonte: o motor próprio não tem iframe, vídeo, WebGL, SVG nem WebP. O WebKit dos apps usa esse motor, com os mesmos tetos, mais diálogo JS que não bloqueia e regras de conteúdo que são guardadas e não bloqueiam ([docs/webkit.md](docs/webkit.md)).
- Assinatura de código: tudo é "não assinado" (`errSecCSUnsigned`). `sandbox_apply` responde sucesso e não aplica a política.
- Metal: os 17 opcodes AIR foram baixados e conferidos no host contra bitcode gerado pelo clang, não contra um `.metallib` de app. Textura sem metadado deixa o `thread_position_in_grid` em 0 ([docs/metallib-air.md](docs/metallib-air.md)).
- iCloud, APNs, Sign in with Apple e recibo da App Store dependem de servidor da Apple. Não há cliente disso.

## Roadmap curto

O que os docs da `main` ainda deixam aberto, em seguida do que já tem teste:

1. Refazer o `machocheck` do Postico depois de CTLine/CFHost e o do VS Code como um total único depois do CryptoKit. Os dois deltas estão no fonte; o placar do bundle não.
2. Rodar na VM os casos novos do WebKit (`WebArchive`, histórico, `loadData`) e o `int_xmon.S` no harness do tradutor.
3. AX entre processos e `SCStream`. Sem isso o computer use não sai do processo de teste.
4. `NSDockTile` para o plugin de Dock do ChatGPT desktop, e o renderer (Electron no Cursor/Grok/Slack/Claude; Chromium 157 na casca do ChatGPT).
5. Paginação para disco, no desenho de [docs/mm-shared.md](docs/mm-shared.md).
6. AAC e Opus continuam sem decodificador. FAAD2 foi recusado por ser GPL; fdk-aac e libopus não foram incluídos.
7. FairPlay permanece fora. Não é item de implementação.

## Em andamento

PRs abertos, não mergeados. O conteúdo deles não está na `main` e não entra nas tabelas acima.

- **[#34](https://github.com/othiagobruno/mOS/pull/34)** — build e stage no Linux (`tools/build.sh`, `tools/stage-image.sh`). Ainda em review.

Os PRs #26, #29, #30, #31, #32 e #33 já foram mergeados em 10 nov. 2026. O estado deles está nas seções de compatibilidade e de testes, não aqui.

## Licença

MIT (`LICENSE`). Fonte Inter: SIL OFL 1.1.
