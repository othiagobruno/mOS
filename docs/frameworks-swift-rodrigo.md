# Frameworks Swift da Apple fora do runtime (dono: Rodrigo, desde 18/11)

WeatherKit, SwiftData (com `_SwiftData_SwiftUI`), FoundationModels, Translation, TipKit e ExtensionFoundation.
As lacunas vieram de `docs/lacunas/fernanda.md` (354 símbolos, 7 frameworks). Implementação própria, a partir da
documentação pública e dos nomes mangled que os apps importam. O código da Apple não foi usado.

Cada framework fica em `userland/frameworks/<Nome>`, é compilado por `tools/build-frameworks-objc.sh` e tem teste em
`userland/tests-swiftobjc/<nome>/main.swift`, que roda no host com `tools/test-frameworks-swift.sh <Nome>`
(machotest).

| framework | apps do corpus | símbolos | estado |
|---|---|---:|---|
| ExtensionFoundation | cmux, fuse-t, qlmarkdown, syntax-highlight | 32/32 | pronto, com teste |
| Translation | macwhisper, markedit | 33 | pronto; sem modelos, toda tradução falha com `TranslationError` |
| FoundationModels | markedit | 40 | pronto; sem modelo, `.unavailable(.deviceNotEligible)` e `GenerationError.assetsUnavailable` |
| SwiftData | bartender, maccy, voiceink | 94 | código escrito; falta `Foundation.SortDescriptor` (Eduardo) para compilar |
| _SwiftData_SwiftUI | voiceink | 5 | falta: `@Query` e `EnvironmentValues.modelContext` sobre o OpenSwiftUI |
| WeatherKit | bartender, raycast | 129 | falta: `Foundation.Measurement<NSUnit*>` (Eduardo); sem serviço de clima, vai lançar erro |
| TipKit | macwhisper | 21/21 | pronto, com teste: datastore em disco, invalidação, MaxDisplayCount, regras, displayFrequency, TipView |

## Comportamento no mOS
- **ExtensionFoundation**: não há pluginkit, então `AppExtensionIdentity.matching(appExtensionPointIDs:)` e
  `AppExtensionPoint.Monitor` leem o disco (`ExtensionFoundation_scan.m`). Eles procuram os `.appex` em
  `Contents/Extensions` e `Contents/PlugIns` do próprio app e dos apps de `/Applications`, `/System/Applications` e
  `~/Applications`. O ponto de extensão vem de `EXAppExtensionAttributes.EXExtensionPointIdentifier` ou de
  `NSExtension.NSExtensionPointIdentifier`. Toda extensão instalada conta como habilitada e aprovada. As sequências
  entregam a lista atual e ficam abertas até a tarefa ser cancelada.
- **SwiftData**: o armazenamento é um arquivo JSON por container, no padrão
  `~/Library/Application Support/default.store`, com gravação atômica. Cada linha guarda o JSON de cada propriedade
  pelo nome e, para as relações, a lista de `PersistentIdentifier`. Já funcionam:
  - relações inversas e regras de exclusão (`cascade`, `nullify`, `deny`);
  - `@Attribute(.unique)` (upsert);
  - `rollback`, autosave do `mainContext` na fila principal, `ModelActor` com fila serial e o filtro por
    `Predicate.evaluate`.

  Não há SQLite do Core Data nem CloudKit. Hoje um container usa só a primeira configuração.

- **TipKit**: o estado de cada dica (vezes mostrada, invalidada e o motivo) fica num JSON. O local padrão é
  `~/Library/Application Support/<bundle id>/TipKit/tips.json`, ou o que vier em `Tips.configure`. Nenhuma dica
  aparece antes de `Tips.configure()`. `Tips.Rule` aceita uma condição em closure; o macro `#Rule` com
  `@Parameter` e eventos ainda não existe.

## Frameworks sobre o SwiftUI (`swiftui-needs`)
Um framework com o arquivo `swiftui-needs` é compilado numa passada do `tools/build.sh` que roda depois do
`build-swiftui-objc.py`. Ele usa as opções do `OpenSwiftUI.rsp` filtradas e liga ao SwiftUI.framework. O fonte usa
`import OpenSwiftUI`, porque o ABI name do módulo é `SwiftUI`. Com `-module-alias SwiftUI=OpenSwiftUI`, os tipos do
módulo OpenSwiftUI, como o `Button`, sairiam com o nome mangled `11OpenSwiftUI` e não ligariam. O `.ps1` não tem
essa passada: o build do Windows só compila o CryptoKit por `build-frameworks-objc.ps1`.

## Correções de infraestrutura feitas junto
- `tools/build-frameworks-objc.sh`: o código Swift de um framework agora liga em dois níveis com `libswiftCore`,
  `libswift_Concurrency` e o `libswift<X>` de cada módulo importado. Antes os `swift_task_*` caíam nos stubs do
  libSystem (`openbot_swift.c`), e todo `await` numa função do framework saltava para lixo.
- Reportado à Paula: `kernel/compat/macho.c` (`run_rebase`) não aplica o slide em ponteiros com o bit 63 ligado no
  x86_64. Esses ponteiros aparecem nas Strings dos arrays estáticos do Swift com `-O`. Até a correção, os testes daqui
  compilam com `-Onone`.
