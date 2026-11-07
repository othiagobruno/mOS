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

## libswiftUIKit completa, entrada e controles (7 nov. 2026)

- **Terceira leva do overlay** (`Configurations.swift`, `DataSources.swift`): `UIButton.Configuration` (+ `UIButton.configuration`
  e `init(configuration:primaryAction:)`), `UIListContentConfiguration`, `UIBackgroundConfiguration`,
  `UICellConfigurationState` (com a ponte para a classe ObjC), `UICellAccessory`, `UICollectionLayoutListConfiguration`,
  `UIListSeparatorConfiguration`, `NSCollectionLayoutSection.list`, os acessores de configuração das células,
  `UITableViewDiffableDataSource`/`UICollectionViewDiffableDataSource` (respondem como `UITableViewDataSource`),
  `UICollectionView.CellRegistration`/`SupplementaryRegistration` + `dequeueConfigured…`, `UIPointerEffect`/`UIPointerShape`/
  `UIPointerStyle`, `AttributeScopes.UIKitAttributes`, `UIFocusSystem.focusSystem(for:)`, `UITextDirection.layout`,
  `UILayoutPriority`. No Foundation, a ponte `IndexPath ↔ NSIndexPath`.
- **WhatsApp: libswiftUIKit 0 ausentes**; total **3.413**. Teste `uikit` **26/26**.
- **Entrada** (`UIKitEvents.m`): a NSView hospedeira traduz mouse em `UITouch`/`UIEvent` na view do `hitTest:`;
  `touchesBegan/Moved/Ended` sobem pela cadeia de resposta (view → view controller → superview → janela → `UIApplication`
  → delegate); gestos de toque, arrastar e toque longo acompanham o toque; o teclado vai para o primeiro respondedor
  (`insertText:`/`deleteBackward`).
- **Controles** (`UIKitControls.m`): `UIControl` com alvos/ações e `UIAction` (TouchDown, TouchUpInside,
  PrimaryActionTriggered); `UIButton` desenha o título; `UITextField` edita pelo teclado (delegate, EditingChanged,
  `UITextFieldTextDidChangeNotification`); `UISwitch` alterna e manda ValueChanged.
- **libSystem:** `CGContextSetAlpha` no CoreGraphics (alfa global do contexto nas cores de preenchimento e traço); a
  `UIView` o usa para `alpha < 1` (ainda sem multiplicar pelo alfa dos ancestrais).
- Teste `catalyst` **15/15** (cliques, gesto, cadeia de resposta, teclado, interruptor).

## Layout, rolagem, tabelas, coleções e navegação (7 nov. 2026)

- **`UIKitViews.m`** (novo): `UIScrollView` (`contentOffset` = origem dos bounds, limitado a `contentSize` +
  `contentInset`; roda do mouse pela NSView hospedeira e arrasto do toque; `scrollViewDidScroll:`), `UITableView`
  (alturas por linha/delegate, cria só as células na área visível e solta as que saem, seleção por toque com
  `didSelectRowAtIndexPath:`, `scrollToRowAtIndexPath:`), `UITableViewCell` (estilos com `textLabel`/`detailTextLabel`),
  `UICollectionView` com `UICollectionViewFlowLayout` (itens em linhas que quebram pela largura; só rolagem vertical) e
  `UICollectionViewListCell`, `UINavigationController` (pilha, `UINavigationBar` com título e "‹ Voltar" que faz pop,
  `navigationItem`/`navigationController`), `UILayoutGuide` (área segura, margens).
- **Auto Layout:** as âncoras do UIKit reaproveitam as classes de restrição do AppKit do mOS (`cx_layout_anchor`,
  `cx_layout_constraints`, `cx_layout_constraint_info` no `appkit.c`); a `UIWindow` resolve as restrições ativas das
  views com `translatesAutoresizingMaskIntoConstraints = NO` por propagação de igualdades (bordas, centro, largura,
  altura, multiplicador/constante, limites `>=`/`<=` constantes) antes do `layoutSubviews`. Sem prioridades nem
  desigualdades entre views.
- **Overlay Swift:** os data sources diffable respondem pelos seletores do protocolo ObjC
  (`numberOfSectionsInCollectionView:`, `collectionView:cellForItemAtIndexPath:`…), então a `UICollectionView`/`UITableView`
  do mOS desenha as células do `cellProvider`.
- **Foundation do libSystem:** `-[NSMutableDictionary setDictionary:]` e `+[NSSet setWithObject:]` (faltavam: o
  conjunto de toques chegava vazio e o cache de células da tabela nunca ficava guardado).
- Testes: `catalyst` **23/23** (Auto Layout por âncoras, tabela com só as células visíveis, roda do mouse e limite da
  rolagem, seleção por toque, push/pop com a barra), `uikit` **28/28** (coleção real alimentada pelo diffable).
- Ainda não: modais (`present`), animações (os blocos rodam na hora), alfa dos ancestrais, prioridades no Auto Layout,
  layout composicional de verdade (vira lista).
