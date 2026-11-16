# Runtime arm64 completo: plano e estado (frente 16, Sergio)

Meta: um app macOS só-arm64 roda de ponta a ponta no mOS. O processo traduzido é inteiro arm64 (`docs/tradutor-arm64.md`,
"Por que o processo inteiro é arm64"): a "ponte" para os frameworks não é um thunk arm64→x86_64 por função (sem a
assinatura não há conversão correta de variádicos, HFA, `x8`, blocks e callbacks), é **cada biblioteca que o mOS instala
ter fatia arm64 com a ABI da Apple**. O carregador (`kernel/compat/macho.c`, `pick_slice`) escolhe a fatia do processo.

Divisão combinada com a Paula (carregador `macho.c`, `a64/`, `a64rt/`, coordenação ARM64) e a Carolina (ISA e JIT):
o que segue é desta frente.

## Levantamento (15 nov. 2026, `origin/main` 6a8ed98)

O que já existe e não é desta frente refazer: `a64rt` (processo, threads, sinais, `MAP_JIT`), página guest de 16 KiB
(`docs/pagesize-arm64.md`), fat/fat64 e arm64e no carregador, `tools/build-libsystem-arm64.sh` no `tools/build.sh`
(Paula, 7183fc9), testes `tools/a64rt-testes.sh` (machotest) e `tools/a64rt-teste-vm.sh` (VM).

`tools/verificar-imagem-arm64.py --relatorio` na ESP Linux mais recente do clone principal (`build/linux-a64/esp`):
**55 bibliotecas Mach-O do sistema, 53 sem fatia arm64**. Só o libSystem arm64 e o a64rt estavam certos.

| Grupo | Sem fatia arm64 hoje | Onde é gerado no Linux |
|---|---|---|
| `/usr/lib` | `libc++.1.dylib`, `libz.1.dylib`, `libcurl.4.dylib`, `libxml2.2.dylib` (e `libsqlite3.dylib`, `libcompression.dylib`, `libicucore.A.dylib` nem entram no build Linux com as duas fatias) | `build-libcxx.sh`, `build-libz.sh`, `build-sqlite.sh`, `build-libcurl.sh`, `build-libxml2.sh` |
| Swift `/usr/lib/swift` | as 31 `libswift*.dylib` | `build-libswift-real.sh`, `stage-libswift.sh` |
| Frameworks ObjC/Swift | AudioToolbox, CloudKit, Collaboration, Combine, CoreData, CoreImage, CryptoKit, ExtensionFoundation, ExtensionKit, FinderSync, ImageIO, NaturalLanguage, QuickLookUI, SecurityInterface, ServiceManagement, WebKit, UIKit (iOSSupport) | `build-frameworks-objc.sh`, `build-swift-combine.sh`, `build-coreimage.sh`, `build-cryptokit-macho.sh`, WebKit direto no `build.sh` |
| libSystem arm64 | fontes com a convenção Swift do x86 escrita à mão fora do arm64 (`X86_SWIFTCC` em `build-libsystem-arm64.sh`: `swiftui_*.c`, `string_fdn.c`, `foundation_swift_data.c`, `combine_link.c`, `combine_meta.c`, `combine_meta.S`) | — |

Achados no carregador, repassados à Paula (dona do `macho.c`): num universal com arm64 **e** arm64e, `pick_slice` fica
com a última fatia da tabela; deveria preferir arm64 puro.

## Plano

1. **Ferramentas** (feito neste PR): `tools/lipo-mos.py` (o `lipo-mos.ps1` para o Linux: cria, `--info`, `--archs`,
   `--thin`; fatia arm64 alinhada a 16 KiB), `tools/verificar-imagem-arm64.py` (falha se uma biblioteca do sistema
   não tem arm64 ou se uma dependência da fatia arm64 não resolve) e o teste `tools/arm64-imagem-teste.sh`.
2. **`/usr/lib` universal no build Linux**: `-Arch`/`ARCH` arm64 nos scripts `.sh` e lipo com o `lipo-mos.py`;
   libsqlite3, libcompression e ICU com as duas fatias.
3. **Swift arm64 no Linux**: as `libswift*` com fatia arm64 (o `tools/build-swift-arm64.ps1` faz no Windows) e
   Combine/SwiftUI/UIKit.
4. **Frameworks**: fatia arm64 de cada framework da tabela, pelo mesmo script que gera o x86_64.
5. **Convenção Swift arm64 no libSystem**: os fontes de `X86_SWIFTCC` com a versão arm64 (self `x20`, erro `x21`,
   resultado indireto `x8`, retorno `x0..x3`, `d0..d3`) e fora da lista nos dois scripts (`.sh` e `.ps1`).
6. **`tools/build.sh`**: `verificar-imagem-arm64.py` sem `--relatorio` no fim, para a imagem não sair sem as fatias.
7. **Build final** (uma vez, pela fila): ESP com 0 problemas no verificador; `tools/a64rt-teste-vm.sh` com binário arm64
   real.

## Testes

```
tools/arm64-imagem-teste.sh                                   # lipo + verificador, dylibs reais do zig
python3 -I tools/verificar-imagem-arm64.py --relatorio <esp>  # lacunas de uma ESP
```

## Estado

| Item | Estado |
|---|---|
| 1. Ferramentas | pronto, `tools/arm64-imagem-teste.sh`: 11 verificações ok |
| 2. `/usr/lib` universal | a fazer |
| 3. Swift arm64 | a fazer |
| 4. Frameworks arm64 | a fazer |
| 5. swiftcc arm64 no libSystem | a fazer |
| 6. Verificador no build | a fazer (entra quando 2–5 fecharem, senão o build para) |
| 7. Build final | a fazer |
