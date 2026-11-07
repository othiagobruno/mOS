# UIKit do Mac Catalyst no mOS

Apps Catalyst (o WhatsApp para Mac é um) ligam o UIKit pelo caminho do Catalyst,
`/System/iOSSupport/System/Library/Frameworks/UIKit.framework/Versions/A/UIKit`, e a camada Swift dele
(`/System/iOSSupport/usr/lib/swift/libswiftUIKit.dylib`). O mOS monta esse framework sobre o AppKit que já tem.

## Onde está

- `userland/frameworks/UIKit/`, compilado por `tools/build-frameworks-objc.ps1` (o arquivo `install-path` dá o caminho do
  Catalyst e uma cópia em `UIKit.framework/UIKit`, porque o VFS não tem links simbólicos).
  - `UIKitConstants.m`: as ~290 constantes que os apps importam (notificações, chaves, identificadores de menu,
    estilos e pesos de fonte, traços de acessibilidade, `UIEdgeInsetsZero`...), com os tipos e valores do SDK da Apple.
  - `UIKitClasses.m`: ~200 classes com a hierarquia da Apple (`UIButton : UIControl : UIView : UIResponder`,
    `UIImagePickerController : UINavigationController`...), para subclasses e casts dos apps. A maioria ainda só tem o
    comportamento herdado.
  - `UIKitCore.m`: o que leva um app até a tela:
    - `UIApplicationMain` → `UIApplication` + delegate (`application:didFinishLaunchingWithOptions:`);
    - cena: `application:configurationForConnectingSceneSession:options:` ou o `UIApplicationSceneManifest` do Info.plist →
      `UIWindowScene` + delegate da cena (`scene:willConnectToSession:options:`, `sceneDidBecomeActive:`);
    - `UIWindow.makeKeyAndVisible` → uma `NSWindow` do AppKit do mOS, cujo conteúdo é uma NSView hospedeira que desenha a
      árvore de `UIView` (fundo, `-drawRect:` de subclasses e o texto do `UILabel`);
    - `UIView` (hierarquia, frame/bounds/center, conversão de coordenadas, `hitTest:`), `UIViewController` (view
      preguiçosa, `viewDidLoad`, filhos), `UIColor` (cores fixas e as do sistema no tema claro), `UIFont`
      (tamanhos dos estilos de texto do Catalyst), `UIImage` sobre `CGImage`, `UIScreen`, `UIDevice` (idioma Mac),
      `UITraitCollection`;
    - o laço de eventos é o `NSApplication` do mOS.
  - `UIKitFunctions.m`: contexto de imagem (`UIGraphicsBeginImageContextWithOptions`...), consultas de acessibilidade
    (tudo desligado), categorias de tamanho de texto, `CGRectFromString`, `NSStringFromCGAffineTransform`.
- Teste: `tools/test-catalyst.ps1` compila `userland/tests-catalyst/main.m` (app com delegate de cena) contra o framework
  e roda no harness: **9/9**, e o quadro sai com os retângulos das views e o texto do `UILabel`.

## Estado (7 nov. 2026)

- WhatsApp: os 571 símbolos do UIKit.framework resolvem (ausentes distintos 4.265 → **3.694**; importações sem
  implementação 24.990 → 7.030).
- Ainda não: eventos de mouse/teclado entregues às `UIView` (o `hitTest:` existe, falta ligá-lo aos eventos da
  NSView hospedeira), Auto Layout, animações (os blocos rodam na hora), alpha parcial (falta `CGContextSetAlpha` no CG
  do libSystem), controles (`UIButton`, `UITextField`, `UITableView`, `UICollectionView`... são só a hierarquia),
  navegação e apresentação modal, `UIImage` de arquivos/assets, PNG/JPEG de saída.
- Próximo: a camada Swift (`libswiftUIKit`, 382 símbolos no WhatsApp), que precisa de um módulo clang UIKit com os
  nomes da Apple para o overlay compilar.

## libswiftUIKit (overlay Swift)

- Módulo clang `UIKit` (`userland/sdk/swift-objc/sdk/UIKit/UIKit.h` + entrada no `module.modulemap`), no formato do SDK
  da Apple: typedefs com wrapper Swift (`UIWindowLevel`, `UIContentSizeCategory`, `UIAccessibilityTraits`,
  `UIMenuIdentifier`, `UIActionIdentifier`), os `NS_ENUM`/`NS_OPTIONS` de menus, botões, eventos de controle e teclas,
  e as classes que o overlay estende. Os nomes mangled levam o nome C deles (`So13UIWindowLevela`, `So0A6EventsV`...).
- Overlay `userland/sdk/swift-objc/UIKit/*.swift`, compilado por `tools/build-swift-uikit-objc.ps1` como módulo `UIKit`
  (interop, library evolution) e ligado em `/System/iOSSupport/usr/lib/swift/libswiftUIKit.dylib`. O `UIKit.o` fica em
  `overlays/uikit`, fora do link estático dos outros testes.
  - `UIKit.swift`: `IndexPath(row:section:)`/`item`, `UIContentSizeCategory` (comparação, `isAccessibilityCategory`),
    `_UIKitNumericRawRepresentable` e `UIWindowLevel` (aritmética), `UIAccessibilityTraits: OptionSet`,
    `NSDiffableDataSourceSnapshot` e `NSDiffableDataSourceSectionSnapshot` (Swift puro).
  - `Menus.swift`: os inits Swift de `UIAction`, `UIMenu`, `UICommand`, `UIKeyCommand`, `UIBarButtonItem`,
    `UIButton(type:primaryAction:)`; `UIControl.enumerateEventHandlers`; `UISheetPresentationController.preferredCornerRadius`;
    `UIFocusEnvironment.contains`; `UIConfigurationColorTransformer`/`UIConfigurationTextAttributesTransformer`; os
    protocolos `UIContentConfiguration`/`UIContentView`/`UIConfigurationState`.
- No framework, `UIKitMenus.m` dá estado de verdade a menus, ações, comandos, itens de barra e `UIButton`, e o
  `UIControl` guarda alvos+ações e `UIAction` (`sendActionsForControlEvents:` dispara).
- Teste: `tools/test-uikit-swift.ps1` (`userland/tests-swiftobjc/uikit`) → **16/16**.
- WhatsApp: libswiftUIKit 278 → 232 → **206** distintos (282 importações); total 3.694 → **3.622**. Falta:
  `UIButton.Configuration` (35), as configurações de lista/célula/fundo (`UIListContentConfiguration`,
  `UIBackgroundConfiguration`, `UICellAccessory`, `UICellConfigurationState`), os data sources "diffable" (classes
  genéricas), as registrations de célula, `UIPointerStyle`/`UIPointerEffect` e `AttributeScopes.UIKitAttributes`.
