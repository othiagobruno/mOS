# AppKit — plano da frente (Beatriz)

Dona: Beatriz (`beatriz/*`). Escopo do `TIME.md`, linha 11: todos os controles e views, NSDocument, menus, toolbar,
drag & drop, pasteboard, NSTextView/TextKit 1 e 2, NSTableView/NSOutlineView/NSCollectionView, acessibilidade,
impressão, sheets/painéis, NSOpenPanel/NSSavePanel e Services.

Regra deste doc: cada linha diz o que está pronto **com teste**, o que existe sem teste e o que falta. Nada aqui foi
executado numa VM; o build do projeto só roda no fim (`build-final.sh`).

## Como o AppKit do mOS é feito

- Código em `userland/libsystem/appkit*.c`, em C, sobre o runtime ObjC próprio (`objc.h`: `cx_define_class`,
  `cx_add_method`; o último método registrado com o mesmo seletor vence).
- Ordem de registro: `cx_appkit_init` (`appkit.c`) → `cx_appkit_input_init` (`appkit_input.c`), que chama
  `appkit_extra`, `appkit_postico`, `appkit_apps_ui` e os módulos novos desta frente.
- Desenho: lote de comandos para o window server do kernel (`compat_gui.h`, `GUI_DRAW`). `NSRectFill`, `NSBezierPath`,
  `-[NSString drawAtPoint:withAttributes:]` já desenham.
- Padrão de módulo novo: **núcleo em C puro** (sem ObjC, testável no host com gcc) + **cola ObjC** que só traduz
  mensagens. Os testes de host ficam em `userland/tests-appkit/*_host.c`, com o comando no topo do arquivo.

## Inventário em 15 nov. 2026 (antes desta frente)

Medido no fonte (`grep` de `DEF(`/`M(` por classe). "Raso" = a classe existe mas os métodos são `noop`/constantes.

| Área | Estado encontrado |
|---|---|
| NSApplication, NSWindow, NSView, NSControl, NSButton, NSTextField, NSSlider, NSProgressIndicator, NSBox, NSStackView, NSImageView, NSScrollView | Funcionam no básico (`appkit.c`, 5,4 mil linhas). ~46 métodos ainda são `noop` (ex.: `setLevel:`, `setMinSize:`=`setMaxSize:`, `setFrameOrigin:` da janela, scrollers) |
| NSTableView view-based | Geometria e reutilização com teste (`table_host.c`, 70/70) |
| NSOutlineView, NSSplitView, NSTabView | Subconjunto do Postico (`appkit_postico.c`) |
| NSCollectionView, NSTextStorage, NSLayoutManager | Subconjunto (`appkit_apps_ui.c`) |
| NSPasteboard / NSPasteboardItem | Tipo → valor em memória; texto ligado ao clipboard do kernel |
| NSMenu / NSMenuItem | Barra de menus publicada no window server |
| NSAlert | Modal real e folha (assíncrona no run loop) |
| NSOpenPanel / NSSavePanel | **Cancelavam sempre** (`runModal` = 0) |
| NSDocument / NSDocumentController | Raso (2 e 3 métodos) |
| NSToolbar | Raso (1 método) e itens do Postico |
| NSPopUpButton, NSSegmentedControl, NSCell e família | Classes vazias |
| NSComboBox, NSDatePicker, NSStepper, NSLevelIndicator, NSPathControl, NSBrowser, NSMatrix, NSRuleEditor/NSPredicateEditor, NSFontPanel, NSPrintOperation, NSTextContainer, NSTextLayoutManager (TextKit 2) | **Não existem** |
| Acessibilidade | AX do próprio processo (`appkit_ax.c`) e transporte entre processos (`axremote.c`, de outra frente) |
| Drag & drop | `NSDraggingSession`/`NSDraggingItem` sem métodos |
| Impressão | `NSPrintInfo`/`NSPrintPanel` rasos; sem `NSPrintOperation` |
| Services | Menu de serviços ignorado (`setServicesMenu:` noop) |

Do corpus (`docs/compat-gaps.md`, 61 apps): `NSComboBox` (18 apps), `NSFontPanel` (18), `NSModalPanelRunLoopMode` (20),
`NSAccessibilityUnknownRole` (22), `NSAccessibilityMenuButtonRole` (19), `NSBitsPerPixelFromDepth` (18). OpenGL
(`NSOpenGL*`) é da frente do Otavio.

## Fases

Ordem pelo impacto em apps reais (todo app de documento usa painel, documento, menu e texto).

| # | Fase | Entrega | Estado |
|---|---|---|---|
| 1 | Painéis de arquivo | NSOpenPanel/NSSavePanel reais: navegação, barra lateral, filtros por extensão e UTType, pacotes, ocultos, seleção múltipla, digitar para achar, ir para pasta, nova pasta, nome e extensão no salvar, confirmar substituição, delegate (`panel:shouldEnableURL:`, `panel:validateURL:error:`, `panel:didChangeToDirectoryURL:`, `panelSelectionDidChange:`), `runModal`, `beginWithCompletionHandler:`, folha | **Feito neste PR**, núcleo com teste de host (`panel_host.c`) |
| 2 | Constantes e símbolos que o corpus pede | `NSModalPanelRunLoopMode`, papéis AX que faltam, `NSBitsPerPixelFromDepth` e vizinhos | A fazer |
| 3 | Família NSCell + controles que faltam | NSCell/NSActionCell/NSButtonCell/NSTextFieldCell com desenho; NSPopUpButton, NSSegmentedControl, NSComboBox, NSStepper, NSDatePicker, NSLevelIndicator, NSPathControl, NSColorWell, NSSearchField, NSTokenField, NSSecureTextField, NSMatrix, NSBrowser | A fazer |
| 4 | NSDocument / NSDocumentController | Abrir, salvar, salvar como, reverter, autosave, documentos recentes persistidos, `isDocumentEdited`, título, menu Arquivo padrão, NSUndoManager ligado | A fazer |
| 5 | Menus e toolbar completos | Validação (`validateMenuItem:`/`NSUserInterfaceValidations`), teclas equivalentes pela cadeia de responders, menus de contexto, NSToolbar completo (itens padrão, flexível, personalização, `NSToolbarItemGroup`) | A fazer |
| 6 | Pasteboard, drag & drop e Services | Tipos múltiplos com conversão, `NSFilePromiseProvider`, sessão de arrasto real com imagem, destino (`NSDraggingDestination`), Services (`NSServicesMenu`, `validRequestorForSendType:returnType:`) | A fazer |
| 7 | Texto | NSTextView/TextKit 1 completo (NSTextStorage, NSLayoutManager, NSTextContainer, seleção, desfazer, busca, régua), TextKit 2 (`NSTextLayoutManager`, `NSTextContentStorage`) | A fazer |
| 8 | Tabelas e coleções | NSTableView cell-based e view-based completo (ordenar, arrastar linhas, edição), NSOutlineView completo, NSCollectionView (flow e compositional) | A fazer |
| 9 | Janelas, sheets e painéis restantes | `beginSheet:`, NSFontPanel, NSColorPanel, níveis, tamanho mínimo/máximo, tabs | A fazer |
| 10 | Impressão | NSPrintOperation, NSPrintInfo completo, painel de impressão, saída em PDF (com a Mariana, Core Graphics PDF) | A fazer |
| 11 | Acessibilidade | Protocolo NSAccessibility completo nos controles desta frente (papéis, ações, notificações) | A fazer |

Cruzamentos: Mariana (CGContext/PDF/Core Text), Eduardo (Foundation: NSURL, NSFileManager, NSUndoManager),
Natalia (Launch Services e UTType, TCC), Ricardo (SwiftUI usa NSHostingView), Camila (window server).

## Fase 1 — NSOpenPanel / NSSavePanel

Arquivos: `userland/libsystem/appkit_panel_core.{c,h}` (núcleo), `userland/libsystem/appkit_panel.c` (cola e UI),
`userland/tests-appkit/panel_host.c` (teste).

O núcleo não toca o disco: a cola lê a pasta (`NSFileManager` + `stat`) e entrega as entradas. Ele decide:

- caminho: normalização (`.`, `..`, `//`, `~`), pai, componentes;
- tipos permitidos: extensões e UTTypes (`public.image`, `public.text`, `com.adobe.pdf`, … expandidos para extensões),
  `public.folder`/`public.directory`, `com.apple.application-bundle`;
- pacotes (`.app`, `.bundle`, `.framework`, `.rtfd`, …) como arquivo, salvo `treatsFilePackagesAsDirectories`;
- ocultos (nome com `.`), ordenação Finder (sem caixa, números pelo valor, acentos latinos dobrados), por nome, data,
  tamanho ou tipo, crescente ou decrescente;
- habilitado por linha (abrir: arquivo só se `canChooseFiles` e tipo aceito; pasta escolhível se
  `canChooseDirectories`; salvar: só pastas);
- seleção: clique, ⇧ (faixa), ⌘ (alternar), setas com e sem ⇧, Home/End, digitar para achar (1 s);
- salvar: validação do nome, extensão acrescentada quando o tipo exige, detecção de "já existe" sem caixa,
  nome para "nova pasta" sem colisão.

Teclas na UI: ↑ ↓, ⇧↑ ⇧↓, ⌘↑ (pasta de cima), ⌘↓ / Enter (abrir), Esc (cancelar), ⌘⇧G ou `/` `~` (ir para a pasta),
⌘⇧H (Início), ⌘⇧D (Mesa), ⌘⇧N (nova pasta), ⌘⇧. (mostrar ocultos).

Limites: não há visualização em colunas nem ícones de arquivo (lista com marcador de pasta); tags do Finder não
existem no mOS.
