# Postico e WhatsApp no mOS: o que ainda falta (8 nov. 2026)

Continuação da fase 6 de [swift-objc-runtime.md](swift-objc-runtime.md). O SwiftUI.framework está no commit `b16b648`.
O trabalho de AppKit descrito abaixo **ainda não foi commitado**.

## Onde parou

### Postico 2.3.5 (harness, raiz `build/postico-root`)

Na última execução concluída, o Postico:
- carrega 33 imagens e passa pelo alerta do Sparkle, que fecha com Return;
- cria o `PGEWindowController` e abre a janela principal "Window", de 950×500;
- carrega o nib da lista de favoritos (`FavoritesListViewController.nib`), com o `FavoritesOutlineView` dentro do scroll.

Depois parava em `$sSo6CGRectV6AppKitE4fill5usingySo22NSCompositingOperationV_tF`, porque o nome do símbolo tinha `_` a mais no `__asm__`. O nome já foi corrigido, junto com o NSPageController e o `NSGraphicsContext.compositingOperation`, mas essa versão **não chegou a rodar**: a última execução foi interrompida.

Para rodar, com os scripts do scratchpad desta sessão (copie para `tools/` se for continuar):
- `libsys.ps1` monta um libSystem particular em `scratchpad\libSystem.mos`. Ele não toca no `build\libSystem.mos` e contorna o `KERN_INVALID_ARGUMENT` do `vscode_libc.c` de outra sessão.
- `pg.ps1 -Secs 140 -Events 'f:1500;key:ret;f:3000'` roda o `mt.exe`, que é o machotest com o watchdog mostrando mais da pilha.
- `it.ps1` faz os dois passos acima e mostra as últimas mensagens.

### Mudanças locais não commitadas (deste trabalho)

- `userland/libsystem/appkit_postico.c` (novo): classes que faltavam, como NSToolbarItem, NSSplitView e NSOutlineView. O NSOutlineView usa um adaptador que achata a árvore em linhas para o NSTableView do mOS.
  - Também tem NSTableRowView/HeaderView, as células (NSTextFieldCell etc.), NSSearchField, NSTokenField, NSTabView/Item/Controller, NSPageController, NSGridView, NSDocument, NSGradient, NSColorPanel, NSRulerView, NSTextTable e NSFilePromise*.
  - E ainda constantes, `NSSetFocusRingStyle` e as funções do overlay Swift do AppKit (`CGRect.fill(using:)`, `CGRect.frame(withWidth:using:)`, `NSSound.beep()`).
  - Remendos: `NSImage(systemSymbolName:)` devolve uma imagem vazia de 16×16, `NSColor(named:)` devolve uma cor neutra, e foram acrescentados `NSButton(image:target:action:)`, `-[NSView enclosingScrollView]`, `menu`, o `imageView` do NSTableCellView e `-[NSDictionary __swift_objectForKeyedSubscript:]`.
- `userland/libsystem/appkit_input.c`: chamada a `cx_appkit_postico_init()` no fim do `cx_appkit_input_init`.
- `userland/libsystem/appkit.c`: posta `NSApplicationDidFinishRestoringWindowsNotification` na partida. É ela que liga a flag que faz o Postico abrir a janela de conexões.
- `userland/libsystem/appkit_nib.inc`:
  - O nome padrão do nib de uma classe Swift passa a ser sem o módulo (`_TtC7Postico27FavoritesListViewController` → `FavoritesListViewController`).
  - Um NSClipView decodificado à parte não rouba mais do NSScrollView a view de documento.

Na hora de commitar, o `appkit.c` e o `appkit_nib.inc` podem ter trechos de outras sessões: use o índice temporário com `git apply --cached` só nos trechos deste trabalho.

## O que falta no Postico (até a janela de conexões aceitar entrada)

1. **Rodar de novo** com o nome do símbolo corrigido e seguir queda por queda. As próximas prováveis são métodos que devolvem nil e o Swift desembrulha à força.
2. **`makeViewWithIdentifier:owner:` do NSTableView**: feito. As views de `NSTableViewArchivedReusableViewsKey` (protótipo, NSNib ou dados de nib) entram no registro da tabela; cada linha recebe uma cópia, e a view que sai da área visível volta para a fila do identificador.
3. **Views de célula**: a NSTableView/NSOutlineView posiciona e desenha as views (indentação, triângulo e clique de expansão no outline), dá `objectValue` à célula, respeita `tableView:isGroupRow:`/`outlineView:isGroupItem:` e `shouldSelect…`, e responde `rowForView:`, `columnForView:`, `viewAtColumn:row:makeIfNecessary:`. Falta altura variável de linha (`heightOfRow…`).
4. **NSToolbar**: desenha os itens (imagem ou rótulo) numa view hospedeira no topo da janela, com o conteúdo embaixo; itens com view (busca, segmentado) são subviews de verdade. Os itens padrão vêm de `toolbarDefaultItemIdentifiers:`. Sem personalização nem menu de excesso.
5. **NSSplitView**: holdingPriority no redimensionamento, arraste com os limites do delegate, colapso (`canCollapseSubview:`, duplo clique) e espessura pelo `dividerStyle`. Sem autosave das posições.
6. **Entrada:** confirmar cliques e digitação nos campos da tela de conexão. O hit-test respeita `-hitTest:` sobrescrito desde o commit `7653c60`; conferir também as views SwiftUI do NSHostingView. Para cliques na VM: `tools/testdata/cgevent/cgevent-teste.c` (serviço do launchd, `Argumentos=x y xf yf espera RRGGBB`).
7. **No QEMU de verdade** (as imagens de teste precisam do `--test-overlay`, ver `tools/overlay-teste.ps1`):
   - pôr as dylibs Swift e o SwiftUI.framework na imagem (o `build.ps1` ainda não as copia);
   - lembrar que o `build.ps1` apaga o esp inteiro: tire os apps grandes de lá antes.
8. **Pendências menores vistas no log:** `-[NSMenuItem keyEquivalentModifierMask]`/`userKeyEquivalent` já existem. `-[NSWindow tab]`/`setSubtitle:`/`setRepresentedURL:`/`setRestorable:`, `-[NSToolbar setCenteredItemIdentifiers:]`, `-[NSProgressIndicator setDisplayedWhenStopped:]` e `-[NSApplication helpMenu]` guardam o valor (`setControlSize:` já está em `NSControl` e a subclasse herda). `-[NSFileManager isReadableFileAtPath:]`, `+[NSData dataWithContentsOfURL:options:error:]` e `-[NSMutableArray indexOfObject:inSortedRange:options:usingComparator:]` continuam na frente de Foundation, fora do AppKit.
9. **Sparkle:** o alerta "Unable to Check For Updates" aparece em toda partida. Dá para desligar a verificação, por exemplo com `SUEnableAutomaticChecks` nos defaults, ou fazer o Sparkle iniciar.
10. **Do machocheck** (139 símbolos distintos ausentes, nenhum do SwiftUI): o restante do AppKit, CFNetwork (CFHost*), CoreText (CTLine*/CTTypesetter*), `NSHost` e as constantes de URL e locale. As de URL e locale já entraram em `postico_cf.c`.

## O que falta no WhatsApp

1. **136 símbolos de SwiftUI ausentes** (medidos com `build\wa-scan`): API que o OpenSwiftUI ainda não tem. Gere a lista com `machocheck` (`grep '^SwiftUI.framework'`) e implemente em `userland/sdk/swift-objc/SwiftUI/UI` (tipos do módulo OpenSwiftUI) ou em `Core`. Membros de tipos do Core precisam ficar no Core, senão o nome mangled ganha `AAE`.
2. **Foundation Swift:** o machocheck do 2.26.45.13 mediu 204 símbolos de formatação (não há módulo `FoundationInternationalization`). Os 204 são exportados; 17 têm miolo em `fmt_core.inc` (data, ISO8601, NSDecimal, Measurement). Os outros 187 resolvem o dyld e devolvem vazio. Faltam metadata Swift de verdade, markdown, lista e nome de pessoa. Ver [userspace-sistemas.md](userspace-sistemas.md).
3. **Partida no harness:** ver [compat-apps.md](compat-apps.md), seção "WhatsApp: primeira partida no harness". Refazer com as dylibs novas (SwiftUI.framework, CoreGraphics overlay).

## Riscos já conhecidos

- **Blocos com contagem de referências** (`objc.c`, `b16b648`): o Codex desktop caiu na partida, porque o par ARC `objc_retain` + `_Block_release` soltava o bloco ainda em uso. A sessão "macOS apps funcionalidade completa" corrigiu: `objc_retain`/`objc_release` de bloco no heap passam a usar `_Block_copy`/`_Block_release`. Qualquer mudança em blocos exige retestar Electron e Chromium.
- **Ponte NSString→String** (`swift_bridge("Swift.String")`): só existe na cópia do SDK que o SwiftUI usa. Levar ao `Foundation.h` da variante ObjC muda a importação de todos os overlays e precisa de reteste.
- **`dladdr` com tabela por imagem** (`swiftsupport.c`): ainda não foi retestado no VS Code nem no Electron.
- **`build/libSystem.mos` compartilhado:** só é refeito quando o `vscode_libc.c` de outra sessão compila.

## Estado em 10 nov. 2026 (Linux, harness de host)

Apps oficiais: Postico 2.3.5 (`postico-9817.dmg` de releases.eggerapps.at, SHA-256
`34ec7730de0796c008e0caf9f366629a3043c5813c79f749e47d22084afb6e0f`) e WhatsApp 2.26.45.13 (o zip de
`tools/stage-apps-oficiais.sh`).

**Runtime Swift real.** `tools/build-libswift-real.sh` ligava a `libswiftCore` com a lista de objetos sem aspas:
com o repo em "Área de trabalho", o link quebrava (`error: /home/thiago/Área: unrecognized file extension`) e a
imagem saía com as 24 `libswift*` stub. Com as listas em array, sai o runtime real: `libswiftCore` (8,8 MB),
`_Concurrency`, `Dispatch`, `Darwin`, `ObjectiveC`, `CoreFoundation`, `CoreGraphics`, `Metal` e os módulos pequenos.
`MOS_SWIFT_SKIP_CORE=1` reaproveita a stdlib e o runtime C++ já compilados.

**Na imagem pelo `tools/build.sh`:** os frameworks ObjC do mOS (o UIKit do Catalyst, CoreData com o overlay Swift
etc., `tools/build-frameworks-objc.sh`), o Combine.framework (OpenCombine 0.14.0 como módulo `Combine`,
`tools/build-swift-combine.sh`) e a libxml2 2.13.8 (`tools/build-libxml2.sh`, `/usr/lib/libxml2.2.dylib`).

**machocheck** (mesma pasta: runtime Swift real, Combine, frameworks, libc++, libz, libsqlite3, libxml2):

| | libSystem antes | libSystem depois |
|---|---|---|
| Postico 2.3.5 | 0 | 0 |
| WhatsApp 2.26.45.13 | 809 | 556 |

Sem os frameworks e o Combine na pasta, o WhatsApp media 1.641. Dos 556: StoreKit 2 em Swift (157), interface
(ImageIO, CoreGraphics, Photos, CoreImage: com o Dev de Interface), classes de Intents, AVFoundation, CloudKit,
PassKit, Vision, WidgetKit em Swift (13) e CryptoKit (48). O machocheck não mede as `libswift*` que não existem
no disco (`libswiftFoundation`, `libswiftUIKit`, `libswiftAVFoundation`...).

**Harness (Postico):** carrega 26 imagens com o runtime real e cai no primeiro `Optional<URL>`:
`__swift_instantiateConcreteTypeFromMangledName` devolve nulo porque `$s10Foundation3URLVMn` é um stub vazio
(`sys_zerados.c`). O próximo marco dos dois apps é o Foundation em Swift de verdade (swift-foundation compilado
como módulo `Foundation`, como no `tools/build-swift-foundation-objc.ps1`, que ainda não tem porte Linux).
O mesmo vale para o overlay do UIKit (`libswiftUIKit`), a StoreKit 2 e o CryptoKit com a stdlib real
(o `tools/build-cryptokit-macho.sh` liga contra um módulo Swift mínimo, que não casa com o runtime real).
