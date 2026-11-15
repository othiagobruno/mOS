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
2. **Observation, `_StringProcessing`, `RegexBuilder` reais.** Fonte `stdlib/public/{Observation,StringProcessing,RegexParser,RegexBuilder}`
   do swift 6.4.2 (Apache 2.0), na mesma cadeia de `build-libswift-real.sh`. Tirar os 9 + 21 + 18 stubs e os
   nomes `_StringProcessing`/`RegexBuilder` que estão no grupo stdlib.
3. **`libswiftos` próprio** (`userland/sdk/swift-objc/os`): `Logger`, `OSSignposter`, `OSSignpostID`,
   `OSSignpostIntervalState`, `OSSignpostError`, `OSAllocatedUnfairLock`, `os_log`, `os_signpost` sobre o `os_log_*` do libSystem. 27 stubs.
4. **System** (`FilePath`, `FileDescriptor`, `Errno`, `FilePermissions`): implementação própria em Swift,
   dylib `/System/Library/Frameworks/System.framework/System`. 31 stubs.
5. **libdispatch C**: `dispatch_apply`, `dispatch_data_create_map`, `dispatch_queue_attr_make_initially_inactive`,
   `__pthread_workqueue_setkill`, `pthread_get_qos_class_np` e vizinhos; auditoria de `dispatch*.c` contra a API pública
   (fontes, `dispatch_io`, `dispatch_data`, workloop, `dispatch_block_*`), com teste de host em `userland/tests-dispatch`.
6. **Overlay Dispatch**: `DispatchDataIterator` (3 stubs).
7. **Concurrency sobre o Dispatch**: conferir que o executor global do `libswift_Concurrency` real usa
   `dispatch_async_swift_job`/fila global com QoS, `Task.sleep` (`dispatch_after`), `MainActor` na fila principal.
8. **Unicode** de `swiftrt.c` (`_swift_stdlib_getNormData`, composição): conferir se a libswiftCore real traz
   as tabelas; se sim, o fallback em C sai.

## Fora da frente (avisado ao dono)
- Extensões Foundation sobre tipos da stdlib (`$sSS10FoundationE…`, bridging `NSNumber`, `Duration.TimeFormatStyle`):
  Eduardo. `$ss5ErrorMp` apontado por `fdn_meta.S` é o endereço do stub do libSystem, não o descritor do `libswiftCore`;
  a conformidade a `Error` registrada ali nunca casa com o protocolo real.
- `DispatchData: DataProtocol` (`$s8Dispatch0A4DataV10Foundation0B8ProtocolADMc`) é definido no módulo Foundation: Eduardo.
- CloudKit sobre stdlib (`CKErrorCode`, `CKRecordValueProtocol`): Natalia. Network/`nw_*`: Patricia. libc/libm `*l`: Leonardo.

## Testes
Sem `tools/build.sh` até a frente fechar (REGRAS.md). Cada item tem teste de host próprio, citado acima quando pronto.
