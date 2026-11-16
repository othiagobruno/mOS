# Foundation e CoreFoundation — plano da frente (Eduardo)

Dono: Eduardo (`eduardo/*`). Frente 10 de `TIME.md`: os símbolos Swift do Foundation que ainda são stub e todos os
`NS*`/`CF*` que faltam. Este arquivo diz o que está pronto (com teste de host), o que está em andamento e o que falta.
Atualizado a cada PR da frente.

## Ferramentas

- `tools/fdn-inventario.py SWIFT_DEMANGLE > docs/foundation-stubs.md`: inventário dos stubs da frente em
  `userland/libsystem/sys_zerados.c`, com o nome demanglado. O `swift-demangle` vem de qualquer toolchain Swift
  (o de Linux 6.0.3 serve; fica fora do repositório).
- O mesmo toolchain confere a ordem dos requisitos de protocolo (`-emit-ir` de uma conformidade de teste) antes de
  montar uma tabela de testemunhas à mão. É o que faltou aos `Mc` "stub" de `docs/foundation-swift-187.md`.
- Testes de host por arquivo em `tools/fdn_*_test.c`, com o comando no topo de cada um. Nada aqui roda o build.

## Onde fica cada parte

| Parte | Arquivos |
|---|---|
| Overlay Swift (tipos-valor, metadata, testemunhas) | `fdn_tipos.c`, `fdn_meta.S`, `fdn_url.c`, `fdn_urlcomp.c`, `fdn_local.c`, `fdn_cset.c`, `fdn_valores.c`, `foundation_swift_data.c`, `fi_*.c/inc` |
| Constantes `NS*`/`kCF*` e `__CFStringMakeConstantString` | `fdn_consts.c` (este plano), `darwin.c`, `vscode_consts.c` |
| Classes ObjC da Foundation | `foundation.c`, `foundation_extra.c`, `foundation_more.c`, `kvo.c`, `plist.c`, `prefs.c`, `nsurl.c`, `string_fdn.c`, `fmt.c` |
| CoreFoundation em C | `cf_public.c`, `cf_rest.c`, `cfcollections.c`, `cfdata.c`, `cfencoding.c`, `cfrunloop.c`, `cfnotify.c` |
| ICU | `/usr/lib/libicucore.A.dylib` no disco (`tools/build-icu.ps1`, ICU4C 74.2). A Foundation chama por `dlsym` quando está carregada |

## Estado de partida (main `97c611b`, 15 nov.)

- **954** stubs da frente em `sys_zerados.c` (lista em [foundation-stubs.md](foundation-stubs.md)):
  949 Swift com módulo Foundation, 5 de `CoreFoundation._CFObject`. Mais 32 constantes C que eram ponteiro nulo
  ou função vazia (resolvidas no primeiro PR, abaixo).
- Por tipo de símbolo: 133 descritores de conformidade, 68 descritores de tipo, 56 acessores de metadata,
  52 casos de enum, 30 thunks de despacho, 59 estáticos, o resto funções e propriedades.
- Os maiores grupos: `PredicateExpressions` 98, `Data` 71, `Calendar` 61, `JSONEncoder` 40 + `JSONDecoder` 30,
  `IndexSet` 32, `URLResourceValues` 30, `DateComponents` 29, `URL` 26, `URLRequest` 25, erros bridged 18+11+10.
- Lado ObjC: as classes existem pelo nome, mas várias só têm o caminho que um app já pediu. Exemplo:
  `+[NSLocale localeWithLocaleIdentifier:]` ignora o identificador e devolve o locale atual.

## Caminho do lado Swift (depois do PR #232)

O `libswiftFoundation.dylib` real (swift-foundation, Apache 2.0, `tools/build-swift-foundation-objc.sh`) mais os fontes
de `userland/sdk/swift-objc/Foundation` dão os nomes mangled da Apple; o carregador os prefere aos stubs de
`sys_zerados.c`. Então a API Swift que falta entra como fonte Swift nessa pasta (não como C/assembly à mão), e um stub
só sai de `sys_zerados.c` quando o dylib exporta o nome. A medida por app é a dos "stubs alcançados" de
`tools/meta/placar.py`. O lado ObjC (`NS*` em C no libSystem) continua sendo o que esse Swift chama por baixo.
Compilar o dylib é trabalho pesado: só dentro de `flock /tmp/mos-pesado.lock` (`mos-regras-de-ram.md`).

## Fora da frente (combinado com o dono)

- `kCFNetwork*`, `kCFStream*` (proxy, SOCKS, socket nativo), `NSURLSession`/`URLSession` async, WebSocket: Patricia.
- Bookmarks e aliases (`CFURLCreateBookmarkData`, resolução), xattrs reais e FSEvents: Vinicius. `NSFileManager` e
  `URLResourceValues` usam o que ele expor.
- Símbolos de outros módulos que só citam Foundation no tipo (UIKit, Combine, CloudKit): Ricardo e Natalia.
- `swift_*`, stdlib, Dispatch: Fernanda. XPC C (`xpc_*`): Renata; `NSXPCConnection` em cima dele é desta frente.

## Ordem de trabalho

Cada item sai com teste de host e PR próprio. Ao fechar um, o inventário é regerado.

1. ✅ Constantes C (`NSLocale*`, `NSURL*Key`, `NSStringTransform*`, `kCFURL*`, `kCFErrorLocalizedFailureReasonKey`,
   `NSMallocException`, `NSPresentationIntentAttributeName`, `NSStreamFileCurrentOffsetKey`) e
   `__CFStringMakeConstantString` com internação. Teste: `tools/fdn_consts_test.c` (14/14).
2. Erros bridged: conformidades `Error`, `_BridgedStoredNSError`, `_ErrorCodeProtocol`, `CustomNSError`,
   `LocalizedError` de `CocoaError`, `URLError`, `POSIXError`, com a ordem dos requisitos conferida no toolchain.
3. `Data` completa (71) + `__DataStorage`, `DataProtocol`, `ContiguousBytes`; `IndexSet` (32), `IndexPath` (12).
4. ✅ ICU no build de Linux: `tools/build-icu.sh` (porte do `.ps1`, ICU4C 74.2 com SHA-256 fixado, 2 jobs) gera
   `/usr/lib/libicucore.A.dylib` (nomes sem versão, como a da Apple) e `/usr/share/icu/icudt74l.dat`; o `tools/build.sh`
   o chama e falha se não compilar. Conferido: 445 fontes, dylib de 5 MiB ligada à `libc++` e ao `libSystem`.
   ✅ Lado ObjC/CF de locale, fuso e calendário (`fdn_icu.c`, `fdn_i18n_core.c`, `fdn_i18n.c`): `NSLocale` com o
   identificador de verdade (partes, nomes localizados, separadores, moeda, aspas, sistema métrico, direção, listas ISO),
   `NSTimeZone` com o tzdata da ICU (desvio e horário de verão por data, abreviação, nomes localizados, transições,
   fuso do sistema por `TZ`, `/etc/localtime` ou `fuso=` em `com.mos.sistema.cfg`), `NSCalendar` com todos os
   calendários da ICU (componentes, montagem, soma com e sem wrap, diferença, faixas, ordinalidade, intervalos,
   fim de semana, busca de próxima data, símbolos) e `NSDateComponents` completo; `CFLocale*`, `CFTimeZone*`,
   `CFCalendar*` (com o descritor de componentes variádico) e `CFGregorianDate`/`CFAbsoluteTime*` sobre os mesmos
   objetos. As cópias antigas em `nsurl.c`, `foundation_extra.c`, `cf_public.c`, `cf_rest.c`, `cli.c`,
   `compat_agentes.c` e `vscode_cf.c` saíram. Teste: `tools/fdn_i18n_test.c` 81/81 contra a ICU do host.
   Falta do lado Swift: `Calendar`/`Locale`/`TimeZone` vêm do `libswiftFoundation` real (item 14).
5. ✅ Formatters (`fdn_fmt_core.c`, `fdn_formatters.c`): `NSDateFormatter` (estilos, formato, template, relativo,
   leniente, século de dois dígitos, símbolos), `NSISO8601DateFormatter` (todas as opções), `NSNumberFormatter`
   (todos os estilos, padrões, símbolos, prefixos, dígitos, arredondamento, multiplicador, mínimo/máximo, decimal),
   `NSByteCountFormatter`, `NSDateComponentsFormatter` (os seis estilos, regras de zero, contagem máxima, frases),
   `NSRelativeDateTimeFormatter`, `NSListFormatter`, `NSDateIntervalFormatter`, `NSPersonNameComponentsFormatter`,
   as classes `NSDateInterval` e `NSPersonNameComponents`, e `CFDateFormatter*`/`CFNumberFormatter*` com as chaves.
   Teste: `tools/fdn_fmt_test.c` 54/54 (textos do macOS). `NSMeasurementFormatter` entra com `NSUnit`/`NSMeasurement`.
6. Codable: `JSONEncoder`/`JSONDecoder` (70) e `PropertyListEncoder`/`Decoder` com os containers keyed/unkeyed/
   single-value e as estratégias de data, dados, chave e ponto flutuante.
7. `URL` (26), `URLComponents`, `URLQueryItem`, `URLRequest` (25), `URLResourceValues` (30), `URLError`.
8. `NSPredicate`/`NSExpression`/`NSComparisonPredicate`/`NSCompoundPredicate` (parser do formato, avaliação, KVC)
   e o `#Predicate` Swift (`PredicateExpressions` 98, `Predicate`, `PredicateBindings`, `PredicateError`).
9. KVO/KVC completos (`NSKeyValueObservation`, `observe(_:options:changeHandler:)`, keyPaths compostos,
   operadores `@sum`/`@avg`/`@count`/`@distinctUnionOfObjects`), `NSUserDefaults` com KVO por chave.
10. `NSString`/`CFString` completos (UTF-16 de verdade, comparação com locale, `rangeOfString:options:`, normalização,
    transformações pelo ICU), `NSScanner` (10), `NSAttributedString`/`AttributedString` e `AttributeScopes`.
11. `NSFileManager` (enumeração, cópia/movimento, atributos, `URLsForDirectory`), `NSFileHandle` (9), `NSTask`/`NSPipe`,
    `NSProgress` (5), `NSRunLoop` (8), `NSTimer` (5), `NSNotificationCenter` Swift (13, inclusive `notifications(named:)`).
12. `NSKeyedArchiver`/`NSKeyedUnarchiver`, `NSCoder` Swift (5), `NSSecureCoding`.
13. `NSXPCConnection`/`NSXPCListener`/`NSXPCInterface` sobre o `xpc_*` da Renata.
14. O resto do inventário (pontes `_ObjectiveCBridgeable` de `NSArray`/`NSDictionary`/`NSSet`/`NSNumber`/`NSString`,
    `_NSRange`, `KeyPathComparator`, `SortOrder`, `AsyncLineSequence`, `NSUndoManager`) até `foundation-stubs.md` zerar.
15. Build final uma vez pela fila (`build-final.sh`), corrigir o que quebrar.
