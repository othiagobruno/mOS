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
| NSStepper, NSComboBox, NSLevelIndicator, NSPathControl | Desenho, mouse e target/action (`appkit_public.c`, com teste de host) — o inventário de 15 nov. errou ao listá-las como ausentes |
| NSDatePicker, NSBrowser, NSMatrix, NSRuleEditor/NSPredicateEditor, NSFontPanel, NSPrintOperation, NSTextContainer, NSTextLayoutManager (TextKit 2) | **Não existem** |
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
| 2 | Constantes e símbolos que o corpus pede | `userland/libsystem/appkit_consts.c`: 579 NSString públicas que não existiam em nenhum arquivo (`NSModalPanelRunLoopMode`, `NSEventTrackingRunLoopMode`, papéis/subpapéis/atributos/ações/notificações AX, tipos e nomes de pasteboard, notificações de app/janela/view/menu/texto/tabela/outline/toolbar/workspace, chaves de impressão, de fonte, de documento, de binding, nomes de imagem, aparência). `NSBitsPerPixelFromDepth` já existia (`appkit_apps_ui.c`) | **Feito**: compila em x86_64 e arm64; checado contra duplicatas em todo o libSystem. Valores das chaves de metadado de documento (Title, Author…) e de Touch Bar usam o próprio nome, sem conferência contra um Mac |
| 3a | Menu flutuante e controles com menu | `appkit_menupop.c` (+ núcleo `appkit_menupop_core.c`, teste `menupop_host.c` 32/32): `popUpMenuPositioningItem:atLocation:inView:`, `popUpContextMenu:withEvent:forView:`, clique direito com `menuForEvent:`/`menu`, submenus em cascata, teclado, validação (`autoenablesItems`, `validateMenuItem:`, `validateUserInterfaceItem:`) pela cadeia de responders, delegate do NSMenu. `appkit_controls.c` (+ `appkit_segment_core.c`, teste `controls_host.c` 19/19): NSPopUpButton (pop-up e pull-down) e NSSegmentedControl completos. NSComboBox com lista suspensa e fonte de dados. Corrigido: `appkit_public.c` lia `bounds`/`frame` pelo `objc_msgSend` comum (struct de 32 bytes vai pela memória no x86_64) | **Feito** (host + compilação x86_64/arm64; sem VM). Limite: clicar no menu-pai com submenu aberto fecha a cascata |
| 3b | Campos e controles de valor | **Segurança:** o NSSecureTextField mostrava a senha em claro; agora desenha • por caractere, recusa ⌘C/⌘X e o AX devolve vazio. NSTextField ganhou seleção (⇧ setas, ⌘A, 1º clique seleciona tudo, duplo clique), ⌘C/⌘X/⌘V, Home/End, ⌘←/⌘→, ⌥⌫, apagar para frente e clique que posiciona o cursor. `appkit_controls2.c`: NSSearchField (limpar ×, Esc, ação incremental ou no Enter, buscas recentes), NSColorWell + NSColorPanel (paleta, prévia, opacidade, alvo/ação, `changeColor:` pela cadeia, notificação), NSDatePicker (não existia: campo+stepper, campo, calendário; setas, digitação, Tab, mínimo/máximo, delegate de validação). Núcleo de datas `appkit_date_core.c` no teste `controls_host.c` (38/38) | **Feito** (host + compilação; sem VM). O campo de texto não tem teste de host (o código fica no `appkit.c`, preso ao runtime) |
| 3 | Família NSCell + controles que faltam | NSCell/NSActionCell/NSButtonCell/NSTextFieldCell com desenho; NSPopUpButton, NSSegmentedControl, NSComboBox, NSStepper, NSDatePicker, NSLevelIndicator, NSPathControl, NSColorWell, NSSearchField, NSTokenField, NSSecureTextField, NSMatrix, NSBrowser | A fazer |
| 4 | NSDocument / NSDocumentController | `appkit_document.c` (+ núcleo `appkit_doc_core.c`, teste `document_host.c` 32/32): ler e gravar (data, file wrapper, URL, `writeSafelyToURL`), salvar / salvar como / exportar / reverter, painel de salvar com escolha do tipo, contagem de mudanças e tokens, NSUndoManager pelas notificações, autosave no lugar, fechar com Salvar/Cancelar/Não Salvar (botão vermelho e ⌘W passam por `performClose:`), título "nome — Editado", NSDocumentController (novo, abrir com as extensões do `CFBundleDocumentTypes`/UTIs, recentes no NSUserDefaults, revisão dos não salvos no `terminate:`, documento sem título no lançamento). Barra de menus: alvo pela cadeia completa (inclui controlador de janela, documento e NSDocumentController) e validação a cada 0,4 s | **Feito** (host + compilação; sem VM). O NSUndoManager do Eduardo (PR #368) posta as notificações; o documento conta só o grupo de fora (`groupingLevel` 0) e respeita o grupo descartável. Autosave de rascunho sem título não existe |
| 5 | Menus e toolbar completos | `appkit_toolbar.c` (+ núcleo `appkit_toolbar_core.c`, teste `toolbar_host.c` 11/11): layout com mínimo/máximo, flexíveis, centrados e transbordo por prioridade com »; ação sem alvo pela cadeia de responders; validação (`validateToolbarItem:`/`validateUserInterfaceItem:`) junto da barra de menus; itens selecionáveis; itens padrão (separador, barra lateral, imprimir, cores, fontes, personalizar); NSToolbarItemGroup, NSMenuToolbarItem, NSSearchToolbarItem, NSTrackingSeparatorToolbarItem, NSSharingServicePickerToolbarItem (classes novas); personalização e configuração salva no NSUserDefaults; `toggleToolbarShown:`. Menus: validação e cadeia (fase 3a/4) | **Feito toolbar** (host + compilação; sem VM). Faltam ⌃clique como menu de contexto e rótulos sob o ícone (a faixa tem 28 pt); o picker de compartilhamento não tem serviço por trás |
| 6 | Pasteboard, drag & drop e Services | Pasteboard com lista de itens (um por objeto/arquivo), tipos legados canonizados (`appkit_pb_core.c`, teste `pasteboard_host.c` 18/18), `NSFilenamesPboardType` ↔ itens `public.file-url`, NSPasteboardWriting/Reading do app, NSURL/NSString/NSAttributedString/NSImage/NSColor nos dois sentidos, opções de leitura (só arquivos, conformidade de UTI), provedor de dados por item, tipos declarados. `appkit_filepromise.c`: NSFilePromiseProvider/Receiver. O arrasto (sessão, fonte, destino) já existia em `appkit_input.c` | **Feito pasteboard e promessas** (host + compilação; sem VM). Promessa de arquivo só dentro do processo; entre processos o pasteboard leva só texto. Falta Services |
| 7 | Texto | NSTextView/TextKit 1 completo (NSTextStorage, NSLayoutManager, NSTextContainer, seleção, desfazer, busca, régua), TextKit 2 (`NSTextLayoutManager`, `NSTextContentStorage`) | A fazer |
| 8 | Tabelas e coleções | NSTableView cell-based e view-based completo (ordenar, arrastar linhas, edição), NSOutlineView completo, NSCollectionView (flow e compositional) | A fazer |
| 9 | Janelas, sheets e painéis restantes | `beginSheet:`, NSFontPanel, NSColorPanel, níveis, tamanho mínimo/máximo, tabs | A fazer |
| 10 | Impressão | Base nova: desenho do AppKit fora da janela (`appkit_offscreen.c`; o lote GC_* é gravado e reproduzido num CGContext por `appkit_gcreplay.c`, teste `gcreplay_host.c` 15/15): `lockFocus`/`unlockFocus`, `imageWithSize:flipped:drawingHandler:` que desenha, `cacheDisplayInRect:toBitmapImageRep:`, `dataWithPDFInsideRect:`, `graphicsContextWithCGContext:flipped:`/`WithBitmapImageRep:`, `setCurrentContext:`, NSBitmapImageRep para PNG/JPEG/TIFF/GIF/BMP pelo ImageIO. Impressão (`appkit_print.c` + `appkit_print_core.c`, teste `print_host.c` 17/17): NSPrintInfo completo, NSPrinter com a impressora virtual "Salvar como PDF", NSPrintOperation (paginação automática/ajustar/recortar, `knowsPageRange:`/`rectForPage:`, PDF em arquivo/dados/pré-visualização), NSPrintPanel com acessórios, NSPageLayout, Core Printing `PM*` | **Feito** (host + compilação; sem VM). Não há driver de impressora física: imprimir gera PDF. Chamadas CG diretas do app num contexto corrente só ficam em ordem com o que o AppKit desenhou antes de cada `-CGContext` |
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
