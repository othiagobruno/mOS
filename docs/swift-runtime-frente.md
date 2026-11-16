# Frente Swift runtime, stdlib, Concurrency e Dispatch (Fernanda)

Dona: Fernanda (`fernanda/*`). Líder de Swift: Rodrigo. Este arquivo diz o que está pronto, com qual teste, e o que falta.

## Levantamento (main `97c611b`, 2026-11-15)

Medição: `llvm-nm` do toolchain 6.4.2 nas dylibs reais de `build/swift/objc/dylib/usr/lib/swift`
(saída de `tools/build-libswift-real.sh`) contra os `void nome(void) {}` de `userland/libsystem/sys_zerados.c`.

| Grupo em `sys_zerados.c` | Stubs | Já exportados por libswift real |
|---|---:|---:|
| stdlib (`$ss…`, `$sS…`) | 2840 | 2712 (`libswiftCore`, `libswift_Concurrency`) |
| `swift_*` (runtime C) | 117 | 117 |
| Dispatch | 52 | 48 |
| ObjectiveC, Darwin, CoreFoundation | 23 | 23 |
| `$sSo…` (extensões de tipos ObjC) | 467 | 75 |

As 19 libswift reais importam só nomes que o `libSystem.mos` define com corpo (nenhum cai em `sys_zerados.c`);
`libswiftDispatch` importa ainda os 17 nomes de `Combine` (frente do Ricardo).

### Por que o stub coberto é bug, não só sobra
`resolve()` em `kernel/compat/macho.c`: com ordinal de dylib, a dylib real ganha; na busca plana (`-2`) e
quando o nome não está na dylib do ordinal, o `libSystem` vem antes das outras imagens. O stub vazio escondia
o runtime de verdade e o app chamava um `ret`.

## Plano (um PR por item, merge na main a cada turno)

1. ✅ **Tirar de `sys_zerados.c` os 3044 nomes que a libswift real exporta.** Fica `$ss5ErrorMp`
   (referência de dados em `fdn_meta.S` e `ck_p256_swift.c`; ver Pendências de outros).
   Teste: `gcc -fsyntax-only` do arquivo; recontagem por `tools/gerar-stubs-md.py` (5009 → 1929 linhas de stub).
2. **Observation, `_StringProcessing`, `RegexBuilder`.** Observation (OpenObservation) e `_StringProcessing` já saem de
   `tools/build-swift-foundation-objc.sh` (PR #232, Rodrigo); os 53 stubs que essas dylibs e o `libswiftos` cobrem saíram de
   `sys_zerados.c`. `RegexBuilder` (swift-experimental-string-processing 6.4.2, mesma fonte) entrou no mesmo script
   como `libswiftRegexBuilder.dylib`. Teste: os 23 nomes RegexBuilder/_StringProcessing que eram stub saem de
   `RegexBuilder.o` (`llvm-nm`).
3. **`os`** (`userland/sdk/swift-objc/os/os.swift`): `Logger` guarda o `OSLog` do subsistema/categoria e escreve no log
   unificado pelo `_os_log_impl` (antes descartava info/debug/padrão e ignorava a categoria); `OSSignposter`,
   `OSSignpostIntervalState`, `OSSignpostError`, `checkForErrorAndConsumeState`, `os_signpost` e
   `os_signpost_type_t.event/begin/end`; em C, `os_signpost_id_generate` e `os_signpost_id_make_with_pointer` (`oslog.c`).
   Sem Instruments, `os_signpost_enabled` é falso (como o macOS fora de um trace).
   Teste: overlay compilado com o swift-frontend 6.4.2; os 16 nomes da ABI conferidos no `os.o` (`llvm-nm`).
4. **System**: o swift-system 1.9.0 (Apache 2.0, SHA-256 fixado) compilado com o nome `System`, em
   `/System/Library/Frameworks/System.framework/Versions/A/System`, e `_System_Foundation`
   (`userland/sdk/swift-objc/System/SystemFoundation.swift`: `FilePath(URL)`, `URL(FilePath)`) em
   `libswift_System_Foundation.dylib`. O `_read(into:retryOnInterrupt:)` com `throws` da ABI do macOS 11-14 sai de
   `userland/sdk/swift-objc/System/SystemABI.swift` (o swift-system novo tirou o `throws`).
   Teste: os 31 nomes que eram stub saem de `System.o`/`_System_Foundation.o` (`llvm-nm`), com o swift-frontend 6.4.2.
   Não conferido: em qual dylib o macOS põe o `_System_Foundation` (não há app no
   corpus local que o importe); o carregador procura o nome em todas as imagens carregadas.
5. ✅ **libdispatch reescrita** (`userland/libsystem/dispatch.c`, `dispatch_source.c`, `dispatch_data.c`, `dispatch_io.c`,
   `dispatch_plat.h`). Antes, todo `dispatch_async` rodava na hora, na thread de quem chamava. Agora:
   pool de threads por QoS (6 classes, normal e overcommit, cresce quando o trabalho bloqueia), filas serial,
   concorrente e workloop com barreira, hierarquia de destinos, suspend/resume/activate (filas inativas), `dispatch_sync`
   com caminho rápido e entrega na thread principal para filas presas a ela, `dispatch_async_and_wait`, `dispatch_after`
   e timers numa thread de prazos, `dispatch_apply` paralelo, grupos com prazo e notify, semáforos no futex, `dispatch_once`
   no futex, specifics pela cadeia de destinos, `dispatch_assert_queue*` de verdade (aborta como o macOS), finalizador na
   fila de destino, relógio de parede em `dispatch_time`/`dispatch_walltime`, `dispatch_async_swift_job`.
   Fontes: timer (com contagem de disparos perdidos), DATA_ADD/OR/REPLACE, READ, WRITE, PROC (saída), SIGNAL, VNODE
   (delete, write, extend, attrib, link, rename, revoke), MACH_RECV; MACH_SEND e MEMORYPRESSURE são aceitas e nunca disparam
   (o kernel não gera esses eventos). Dados: create/map/concat/subrange/apply/copy_region e `dispatch_data_empty`.
   `dispatch_io`: canal com fila serial, marca alta, barreira, `close` com STOP, cleanup no fim.
   Teste: `sh tools/dispatch_hosttest.sh` (Linux, 41 de 41, cinco rodadas seguidas) e `tools/runtime-block-task-teste.sh`.
   `userland/tests-dispatch` (Mach-O) não rodou: precisa da imagem (build proibido até o fim da frente).
6. **Overlay Dispatch**: `DispatchDataIterator` e `DispatchData.makeIterator()`; `dispatchPrecondition` e
   `_dispatchPreconditionTest` passam a chamar `dispatch_assert_queue*` (antes devolviam sempre verdadeiro).
   Teste: overlay compilado; os 3 nomes do iterador no `Dispatch.o`.
   Também: fonte MEMORYPRESSURE lê `SYS_VMCTL`/`MOS_VM_STATS` (nível do kernel, a cada ~1 s); QoS por thread
   (`pthread_get_qos_class_np`, `qos_class_self`) e workers do pool com a QoS da raiz; `pthread_introspection_hook_install`
   chamado na criação, início e fim da thread.
7. **Concurrency sobre o Dispatch**: conferir que o executor global do `libswift_Concurrency` real usa
   `dispatch_async_swift_job`/fila global com QoS, `Task.sleep` (`dispatch_after`), `MainActor` na fila principal.
8. **Unicode** de `swiftrt.c` (`_swift_stdlib_getNormData`, composição): conferir se a libswiftCore real traz
   as tabelas; se sim, o fallback em C sai.

## Decisão: os `swift_*` do libSystem
O libSystem exporta ~209 `swift_*` de um runtime próprio (o caminho sem ObjC, usado pelo código C de SwiftUI/OpenBot do
próprio libSystem, que é ELF estático e não alcança a dylib). Ficam exportados: os binários da Apple ligam em dois
níveis e acham o `libswiftCore` pelo ordinal, e desde o PR #232 toda dylib Swift do mOS também. Só a busca plana cai neles.

## Fora da frente (avisado ao dono)
- Extensões Foundation sobre tipos da stdlib (`$sSS10FoundationE…`, bridging `NSNumber`, `Duration.TimeFormatStyle`):
  Eduardo. `$ss5ErrorMp` apontado por `fdn_meta.S` é o endereço do stub do libSystem, não o descritor do `libswiftCore`;
  a conformidade a `Error` registrada ali nunca casa com o protocolo real.
- `DispatchData: DataProtocol` (`$s8Dispatch0A4DataV10Foundation0B8ProtocolADMc`) é definido no módulo Foundation: Eduardo.
- CloudKit sobre stdlib (`CKErrorCode`, `CKRecordValueProtocol`): Natalia. Network/`nw_*`: Patricia. libc/libm `*l`: Leonardo.

## Testes
Sem `tools/build.sh` até a frente fechar (REGRAS.md). Cada item tem teste de host próprio, citado acima quando pronto.
