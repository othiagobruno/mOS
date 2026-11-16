# Frente SwiftUI, Combine, UIKit/Catalyst e PhotosUI (Ricardo)

Dono: Ricardo (`ricardo/*`). Liderança de Swift: Rodrigo. Este arquivo diz o que está pronto, com teste, e o que falta.

## Levantamento (15 nov. 2026, main `187fef7`)

O caminho que importa é o do **Linux** (`tools/build.sh`, o que o `build-final.sh` roda). Nele:

| Peça | Fonte | No Linux hoje |
|---|---|---|
| UIKit ObjC do Catalyst | `userland/frameworks/UIKit/*.m` | entra (`build-frameworks-objc.sh`) |
| `libswiftUIKit` (overlay Swift) | `userland/sdk/swift-objc/UIKit/*.swift` | **não entra**: só existia `build-swift-uikit-objc.ps1`; os 278 `$s5UIKit…` caem nos stubs de `sys_zerados.c` |
| `SwiftUI.framework` (OpenSwiftUI com interop ObjC) | `tools/build-swiftui-objc.ps1`, `userland/sdk/swift-objc/SwiftUI` | **não entra**: sem porte `.sh`, e os 5 repositórios do OpenSwiftUIProject sem commit fixado. Os apps caem no SwiftUI de C do libSystem (`swiftui_*.c`, ABI própria, não é o layout de metadata do Swift) e nos 46 corpos vazios de `swiftui_imports.c` |
| Combine.framework (OpenCombine como `Combine`) | `tools/build-swift-combine.sh` | entra |
| Combine no Foundation (publishers de Timer, NotificationCenter, URLSession, KVO; `Scheduler` de RunLoop/DispatchQueue/OperationQueue; `TopLevelDecoder` do JSONDecoder) | — | **falta**: 8 `…7Combine…` em `sys_zerados.c` |
| PhotosUI (`PHPickerViewController`, `PHPickerConfiguration`, `PHPickerFilter`, `PHPickerResult`) | — | **falta**: 27 `$s8PhotosUI…` stub, sem framework |

## Plano (ordem de entrega, um PR por etapa)

1. **libswiftUIKit no Linux** — `tools/build-swift-uikit-objc.sh` (porte do `.ps1`, ligação em dois níveis com o libswiftCore real) chamado pelo `build.sh` depois do Foundation. Tirar de `sys_zerados.c` os `$s5UIKit…` que o overlay exporta com o mesmo nome; conferir com `llvm-nm`.
2. **PhotosUI.framework** — classes ObjC (`PHPickerViewController` sobre o `NSOpenPanel` do mOS, filtro por UTType, `NSItemProvider` com o arquivo escolhido, delegate `picker:didFinishPicking:`) + overlay Swift `PhotosUI` com os nomes da Apple (`PHPickerConfiguration`, `PHPickerFilter`, `PHPickerResult`). Teste de host. Tirar os 27 stubs.
3. **Combine × Foundation** — `Timer.publish`, `NotificationCenter.publisher(for:)`, `URLSession.dataTaskPublisher`, `NSObject.publisher(for:)` (KVO), `RunLoop`/`DispatchQueue`/`OperationQueue: Scheduler`, `JSONDecoder: TopLevelDecoder`; no módulo Foundation (nomes mangled da Apple), combinado com o Eduardo. Tirar os 8 stubs.
4. **SwiftUI.framework no Linux** — `tools/build-swiftui-objc.sh`: OpenSwiftUI + Compute + OpenRenderBox + OpenAttributeGraphShims + OpenObservation fixados por commit e SHA-256, mesmas etapas do `.ps1` (Deps, Core, UI, Link), ligação em dois níveis. Depois, os 46 de `swiftui_imports.c` resolvem no framework e o arquivo sai.
5. **SwiftUI completo** — o que o OpenSwiftUI upstream deixa vazio e os apps usam: layout (stacks, grids, `Layout`, `GeometryReader`), navegação (`NavigationStack`/`SplitView`), listas e `ScrollView`, animação e transições, estado (`@State`, `@Binding`, `@Observable`, `@Environment`), gestos e foco, `NSHostingView`/`UIHostingController`, representables. Teste por área.
6. **UIKit Catalyst completo** — o que `docs/uikit-catalyst.md` lista como falta: desenho de `transform` e imagem na janela (com a Mariana/Beatriz no CG/AppKit), `popoverPresentationController`, `UIImage` de assets, `UITextView`, menus de contexto, drag & drop, `UIDocumentPickerViewController`.
7. **Apps** — WhatsApp (Catalyst + SwiftUI), Postico e Claude no harness; placar com `tools/meta/placar.py`; build final uma vez pela fila.

## Estado

- [x] 1 libswiftUIKit no Linux (15 nov.): `tools/build-swift-uikit-objc.sh` no `build.sh`; ligação em dois níveis (0 `_swift_*` em busca plana); `tools/test-uikit-swift.sh` **28/28** no `build/machotest`; os 278 `$s5UIKit…` saíram de `sys_zerados.c` (o overlay exporta os 278 com o mesmo nome)
- [ ] 2 PhotosUI
- [ ] 3 Combine × Foundation
- [ ] 4 SwiftUI.framework no Linux
- [ ] 5 SwiftUI completo
- [ ] 6 UIKit Catalyst completo
- [ ] 7 Apps e build final
