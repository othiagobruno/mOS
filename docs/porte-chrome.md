# Porte do Google Chrome para o mOS

Objetivo: rodar o **Google Chrome para macOS (Intel x64)**, sem modificações, como `/Aplicativos/Google Chrome.app`.
Versão medida: Chrome 157.0.8103.39 (`googlechrome.dmg` universal, 270 MiB, de `dl.google.com`).

O Chrome é Chromium puro: o mesmo motor do Electron que o VS Code usa. Quase tudo o que falta para ele é o que
falta para o VS Code (`docs/porte-vscode.md`), então os dois portes andam juntos.

## Onde estamos (6 nov. 2026)

| Etapa | Estado |
|---|---|
| Abrir o `.dmg` do Chrome (chunks LZMA/ULMO) | ✅ `kernel/fs/xz.c`, descompressor xz/LZMA2 próprio; o volume HFS+ "Google Chrome" monta com 394 arquivos e 0 erros (723 MiB) |
| Ler o bundle (`Info.plist`, executável universal x86_64+arm64) | ✅ o carregador escolhe a fatia x86_64 |
| Executável `Google Chrome` carrega e roda | ✅ só 1 import ausente (`abort_report_np`) |
| `dlopen` do `Google Chrome Framework` (264 MiB na fatia x86_64) | ❌ **bloqueio atual**: o `dlopen` do libSystem devolve NULL |
| Framework: 2.708 imports, 1.992 ausentes | ❌ |
| Helpers (GPU, Renderer), IPC Mach, janela | ❌ (mesmo roteiro do VS Code, itens 5 a 7) |

### Validação (6 nov. 2026)

- **Descompressor xz contra a liblzma 5.8.3** (arquivos gerados pelo `bsdtar -J` do Windows): 7 de 7 casos idênticos byte a byte,
  com níveis 0, 6 e 9, multibloco (`threads=4`), streams concatenados, dados aleatórios (chunks sem compressão) e 22 MiB
  de binário. Entrada truncada ou corrompida devolve erro sem travar (testado com UBSan).
- **Arquivos extraídos contra a assinatura de código do Google**: as 159.836 páginas das 20 imagens Mach-O
  (fatias x86_64 e arm64) batem com os hashes SHA-256 do CodeDirectory.
- **Dentro do mOS (QEMU, 2 GiB)**: no boot, `dfs: montado "Google Chrome" (HFS+, ULMO)`; o volume aparece em Locais no
  Arquivos, com botão de ejetar, e mostra o `Google Chrome.app`. O DMG (269 MiB) passa do limite de 256 MiB de
  `persist.c` para arquivos carregados no boot; o teste usou um kernel com `-DMAX_LOAD=(512ull<<20)`.

### Dentro do mOS (6 nov. 2026, 17h30)

- `tools/stage-chrome.ps1` monta o Chrome em `/Aplicativos/Google Chrome.app` numa imagem de teste: só a fatia x86_64
  (309,5 MiB no total, com o framework de 264 MiB), recursos do Chromium e as traduções en/pt_BR.
- O Chrome aparece no Launchpad e na Busca com o ícone dele. Ao clicar, o processo sobe, para no `dlopen` e o mOS mostra
  "\"Google Chrome\" fechou inesperadamente. O app encerrou com o código 134 (abort)" (antes ele sumia sem aviso).
- Funções que só o Chrome pede (269, das 1.893 ausentes; as outras 1.624 o VS Code também pede e as sessões dele cobrem):
  `userland/libsystem/chrome.c` resolve a maior parte. CoreVideo e os `CF*GetTypeID` ficaram com as sessões donas
  (corevideo.c, cfcollections.c). Com tudo junto, o framework caiu de 1.992 para 1.504 imports ausentes. Quando o recurso não existe no mOS (Keychain, DiskArbitration, MIDI,
  Bonjour, assinatura de código...), a função devolve o erro documentado de "indisponível", nunca 0 (= sucesso).
  Sobram 21 classes ObjC (o carregador as liga a `NSObject`), 3 constantes que podem ser nulas e 3 que são de outras
  sessões: `CFDictionaryApplyFunction` (cfcollections.c), `IOSurfaceGetBytesPerElementOfPlane` e `dispatch_data_create`.

### Com o dlopen (7 nov. 2026, madrugada)

Dentro do mOS (QEMU, 3 GiB), aberto pelo Launchpad:

| Etapa | Estado |
|---|---|
| `dlopen` do framework (264 MiB) | ✅ 13 símbolos ausentes (de 1.992 no começo) |
| ICU (`icudtl.dat`) | ✅ depois de `NSBundle` achar `Resources/` numa pasta `Versions/<v>` (foundation.c) |
| Crashpad | ✅ `fork` + `chrome_crashpad_handler` sobem; `shutdown` em socketpair não dá mais EBADF (sockets.c) |
| Singleton do perfil | ✅ `stat` devolve o modo pedido em `mkdir`/`chmod` (0700 do `mkdtemp`; libc.c) |
| `base/mac/mac_util.mm` | ✅ sysctls que faltavam (darwin.c) |
| **Próximo bloqueio** | ❌ cai em `__cxa_guard_release` (libc++abi embutida no Chrome), no caminho de *landing pad*: algo está desenrolando a pilha, e o `_Unwind_RaiseException` do mOS devolve "fim da pilha" |

Para achar a causa de cada queda: `llvm-objdump -d --start-address=... ` (LLVM em `C:\Program Files\LLVM\bin`) no
framework de `build-chrome\esp\...`; o endereço do log menos `0x400100000000` é o endereço no arquivo.

Como o Chrome inicia: o executável `Contents/MacOS/Google Chrome` (0,3 MiB) só monta o caminho
`../Frameworks/Google Chrome Framework.framework/Versions/<versão>/Google Chrome Framework`, chama `dlopen`,
procura `ChromeMain` com `dlsym` e pula para lá. Sem `dlopen` em tempo de execução ele para nesse ponto:

```
dlopen .../Google Chrome Framework: dlopen não é suportado no mOS.
Abort trap: 6
```

## Próximo passo: `dlopen` em tempo de execução

Combinado com o porte do VS Code (6 nov. 2026): o `dlopen`/`dlsym` em tempo de execução é feito lá, em
`kernel/compat/macho.c`, porque o VS Code precisa da mesma coisa (módulos `.node`, `MH_BUNDLE`). Quando existir, o teste aqui
é carregar o `Google Chrome Framework` e chegar ao `ChromeMain`. Flags que o Chromium deve precisar no mOS por enquanto:
`--no-sandbox` e, sem IPC Mach entre processos, `--single-process`/`--in-process-gpu` (em teste no porte do VS Code).

Desenho proposto:

1. O `link_t` de `kernel/compat/macho.c` passa a viver junto do processo (hoje é liberado depois do spawn).
2. Uma syscall `SYS_DLOPEN(caminho)` carrega a imagem e as dependências dela depois da última imagem,
   liga os símbolos contra as imagens já carregadas + libSystem e devolve um handle com os inicializadores e as
   seções ObjC.
3. No libSystem, `dlopen` registra as seções ObjC, roda os inicializadores e devolve o handle;
   `dlsym` percorre a export trie da imagem; `_dyld_image_count`/`_dyld_get_image_header` passam a ver as imagens novas.

Memória: só a fatia x86_64 do framework já tem 264 MiB, e hoje `\mOS\` inteiro vai para a RAM no boot. O script de
preparação deve gravar só a fatia x86_64 (afinar os binários universais), e o boot precisa de `-Memory 3072` ou mais.

## Como reproduzir

```powershell
# extrair o .app com o leitor de DMG do próprio mOS (no Windows)
zig cc -target x86_64-windows-gnu -O2 -DMOS_HOST_TEST -Ikernel/include -Itools tools/dmgtest.c `
  kernel/fs/{apfs,blkdev,bzip2,dfs,dmg,hfsplus,inflate,lzfse,xz,fat}.c <stub com rtc_now/rand32> -o build/dmgtest.exe
.\build\dmgtest.exe googlechrome.dmg saida      # use um caminho sem espaços ou relativo

# inventário do que falta
.\build\machocheck.exe build\libSystem.mos "saida\Google Chrome\Google Chrome.app" faltando.txt

# rodar o executável no carregador do mOS, sem QEMU
$env:MACHOTEST_TOLERANT = 1; $env:MACHOTEST_STUB_LOG = 1
$env:MACHOTEST_PATH = '/Google Chrome.app/Contents/MacOS/Google Chrome'
.\build\machotest.exe build\libSystem.mos "saida\Google Chrome" "saida\Google Chrome\Google Chrome.app\Contents\MacOS\Google Chrome"
```
