# Launch Services no mOS

O mOS sabe que tipo cada arquivo tem e que app abre cada tipo e cada esquema de URL, como o `lsd` do macOS.

## Onde fica

| Parte | Arquivo |
|---|---|
| Banco (tipos, apps, padrões, abrir) | `kernel/compat/launchsvc.c`, `kernel/include/launchsvc.h` |
| Property lists em árvore (XML e `bplist00`) | `kernel/compat/plist.c`, `kernel/include/plist.h` |
| Syscall `SYS_LS` (57) e `mos_ls_req_t` | `kernel/include/syscall.h` |
| APIs dos apps: `LS*`, `UTType*`, `NSWorkspace`, chegada de URLs e documentos | `userland/libsystem/openurl.c` |
| Classe `UTType` (UniformTypeIdentifiers) | `userland/libsystem/vscode_media.c` |
| Utilitário de Compactação e DiskImageMounter | `kernel/apps/coreservices.c` |
| Abrir com, Escolher Aplicativo, Obter Informações > Abrir com | `kernel/apps/arquivos.c` |
| Navegador padrão (Ajustes > Mesa e Dock > Janelas) | `kernel/apps/prefs.c` |
| App de teste com tipos de documento | `userland/samples/Descompactador` |

## Como funciona

- **Tipos (UTIs).** Tabela do sistema (`public.*`, `com.apple.*`, ~130 tipos com extensões, MIME e a árvore de conformidade) mais os `UTExportedTypeDeclarations`/`UTImportedTypeDeclarations` dos apps instalados. Extensão desconhecida vira um tipo `dyn.*` que conforma a `public.data`.
- **Apps.** Os do sistema (Horizonte, Pré-Visualização, Editor de Texto, QuickTime, Livros, Calendário, Contatos, Editor de Script, Mail, Utilitário de Compactação, DiskImageMounter...) com identificadores do macOS (`com.apple.TextEdit`, `com.apple.Preview`...). Cada `.app` em `/Aplicativos`, `/Aplicativos/Utilitários`, `/Applications` é lido do `Info.plist`: `CFBundleDocumentTypes` (`LSItemContentTypes`, `CFBundleTypeExtensions`, `CFBundleTypeMIMETypes`, `CFBundleTypeRole`, `LSHandlerRank`, `CFBundleTypeName`, `CFBundleTypeIconFile`) e `CFBundleURLTypes`.
- **Instalar e apagar.** As pastas são vigiadas (a cada ~1,5 s): um app novo é registrado na hora e aparece no Launchpad; um app apagado sai do banco.
- **Quem é o padrão.** Escolha do usuário primeiro; senão o maior `LSHandlerRank` (Owner > Default > Alternate), depois a declaração mais específica (o tipo exato antes de um tipo pai), depois o app do sistema, depois o registrado primeiro. Como no macOS, instalar um descompactador que declara `.zip` como Alternate não tira o Utilitário de Compactação; os `.rar`/`.7z` que só ele abre passam a abrir com ele, com o nome de tipo e o ícone de documento dele.
- **Escolhas do usuário.** Gravadas em `~/Library/Preferences/com.apple.LaunchServices/com.apple.launchservices.secure.plist`, no formato `LSHandlers` do macOS. "Abrir com" de um arquivo só fica em `MOSOpenWith` no mesmo arquivo (no macOS é um atributo estendido).
- **Navegador padrão.** Ajustes > Mesa e Dock > Navegador padrão, ou o app pede (`LSSetDefaultHandlerForURLScheme`, `-[NSWorkspace setDefaultApplicationAtURL:toOpenURLsWithScheme:completionHandler:]`); para http/https o sistema pergunta "Deseja alterar o navegador padrão para “X” ou manter “Y”?". Vale para http, https, `public.html`, `public.xhtml` e `.webloc`.

## Abrir

`ls_open()` decide: pasta → Arquivos; `.app` → abre (ou traz para a frente); documento ou URL → o app padrão (ou o escolhido). Apps do sistema abrem direto. Apps do macOS recebem uma mensagem Mach no serviço `com.mos.abrir-url.<CFBundleIdentifier>`, que o processo principal registra ao terminar de abrir; app fechado é iniciado e a mensagem espera o serviço (até 2 min). Um navegador fechado recebe a URL web no argv.

No app (`openurl.c`), `file://...` é documento: primeiro o tratador `aevt`/`odoc` que o app tiver registrado (no macOS ele substitui o do AppKit), com o parâmetro direto `'----'` como lista (`'list'`) de `typeFileURL` (`'furl'`: `descriptorAtIndex:`, `fileURLValue`, `stringValue`, `data`); senão `-application:openURLs:`, `-application:openFiles:`, `-application:openFile:` do delegate ou o `NSDocumentController`. As outras URLs viram o evento `GURL` (Electron: `open-url`) ou `-application:openURLs:`.

Pedidos de apps pela syscall (abrir, mudar o padrão com pergunta) vão para uma fila atendida no loop principal do kernel, porque mexem na interface.

## APIs cobertas

- C: `LSCopyDefaultHandlerForURLScheme`, `LSCopyAllHandlersForURLScheme`, `LSSetDefaultHandlerForURLScheme`, `LSCopyDefaultRoleHandlerForContentType`, `LSCopyAllRoleHandlersForContentType`, `LSSetDefaultRoleHandlerForContentType`, `LSCopyDefaultApplicationURLForURL`, `LSCopyApplicationURLsForURL`, `LSCopyDefaultApplicationURLForContentType`, `LSCopyApplicationURLsForBundleIdentifier`, `LSOpenCFURLRef`, `LSOpenFromURLSpec`, `LSRegisterURL`, `LSGetApplicationForURL`, `LSCanURLAcceptURL`, `UTTypeCreatePreferredIdentifierForTag`, `UTTypeCreateAllIdentifiersForTag`, `UTTypeCopyPreferredTagWithClass`, `UTTypeCopyDescription`, `UTTypeConformsTo`, `UTTypeEqual`, `UTTypeIsDynamic`, `UTTypeIsDeclared`.
- `NSWorkspace`: `openURL:` e variantes, `openURLs:withApplicationAtURL:...`, `openFile:` e variantes, `openApplicationAtURL:...`, `launchApplication:`, `launchAppWithBundleIdentifier:...`, `URLForApplicationToOpenURL:`, `URLsForApplicationsToOpenURL:`, `URLForApplicationToOpenContentType:`, `URLsForApplicationsToOpenContentType:`, `URLForApplicationWithBundleIdentifier:`, `URLsForApplicationsWithBundleIdentifier:`, `setDefaultApplicationAtURL:toOpenURLsWithScheme:/toOpenContentType:/toOpenContentTypeOfFileAtURL:/toOpenFileAtURL:completionHandler:`, `typeOfFile:error:`, `localizedDescriptionForType:`, `type:conformsToType:`, `preferredFilenameExtensionForType:`, `filenameExtension:isValidForType:`, `selectFile:inFileViewerRootedAtPath:`, `activateFileViewerSelectingURLs:`.
- `UTType`: `typeWithIdentifier:`, `typeWithFilenameExtension:(conformingToType:)`, `typeWithMIMEType:(conformingToType:)`, `typeWithTag:tagClass:conformingToType:`, `identifier`, `preferredFilenameExtension`, `preferredMIMEType`, `localizedDescription`, `conformsToType:`, `isSubtypeOfType:`, `isSupertypeOfType:`, `supertypes`, `tags`, `isDynamic`, `isDeclared`, e ~60 constantes (`UTTypeZIP`, `UTTypePDF`, `UTTypeMovie`...).

## Testar

Três apps de exemplo em `userland/samples` fazem o papel de apps baixados:

- **Descompactador** (extrator): declara `.zip` (Alternate), `.rar` (Owner), `.7z` (extensão), um tipo exportado `.dcz` e o esquema `descompactador:`. Ao receber um documento grava `~/Downloads/recebido.txt`; o botão "Tornar padrão para .zip" chama `LSSetDefaultRoleHandlerForContentType`.
- **Navegador**: declara `http`/`https` e `public.html`. "Definir como navegador padrão" chama `LSSetDefaultHandlerForURLScheme` (o sistema pergunta); links chegam pelo argv ou pelo evento `GURL`.
- **Reprodutor** (player): declara `public.movie` (Default) e `public.audio` (Alternate), com ícone do app e de documento em `.icns` do formato antigo (`il32`/`l8mk`, sem PNG).

Verificado na VM: tipos certos na lista do Arquivos; "Abrir com" e "Obter Informações > Abrir com > Alterar Tudo…"; `.rar` e `.mp4` abrem nos apps instalados e chegam ao `application:openURLs:`; um app copiado para /Aplicativos com o sistema ligado é registrado sozinho; o alerta do navegador padrão, o `open https://...` do Terminal indo para o navegador escolhido e a escolha em Ajustes; Comprimir e expandir de volta.

## Comprimir

Menu de contexto do Arquivos: "Comprimir “X”" (ou "Comprimir N Itens") cria `X.zip` ao lado, com pastas e links simbólicos (modo `0120777`, como o `ditto`), deflate com Huffman fixo + LZ77 (o que não encolhe vai armazenado) e nomes em UTF-8. Expandir um `.zip` com o mesmo nome de algo que já existe cria `X 2`.

## Ícones

`CFBundleIconFile` e `CFBundleTypeIconFile`/`UTTypeIconFile`: entradas PNG do `.icns` e, sem elas, os formatos antigos `it32`/`ih32`/`il32`/`is32` (RLE por canal) com as máscaras `t8mk`/`h8mk`/`l8mk`/`s8mk`, e `ic04`/`ic05`/`icsb` (ARGB). Entradas JPEG 2000 não são lidas.

## Apps baixados de verdade

Testado com os `Info.plist` e `.icns` reais (o registro não precisa do executável) de Google Chrome, Slack, Claude, Codex, Cursor, Grok Bot, WhatsApp (plist binário) e Postico 2: Chrome 17 tipos e 4 esquemas, Cursor 219 tipos, WhatsApp 10 tipos e 4 esquemas, 17 UTIs declaradas pelos apps. O Arquivos mostra os ícones de documento do Cursor nos `.js`/`.json`/`.py`; "Abrir com" de um `.html` lista Claude, Cursor, Google Chrome e os do sistema com os ícones reais; o Chrome aparece em Ajustes > Navegador padrão e pode ser escolhido.

Armadilha achada nos apps reais: o VS Code e o Cursor põem `CFBundleTypeOSTypes` = `****` em todas as entradas, ao lado da extensão e com `LSHandlerRank` Owner. `****` só vale como "qualquer tipo" numa entrada sem extensões nem MIME, e uma declaração genérica nunca passa na frente de uma que cita o tipo — senão o Cursor viraria o app de tudo.

## Arquivos compactados

Como o Utilitário de Compactação do macOS: `.zip`, `.tar`, `.tar.gz`/`.tgz`, `.tar.bz2`/`.tbz`, `.tar.xz`/`.txz`, `.gz`, `.bz2` e `.xz`. O compressor sai e o tar de dentro é expandido na mesma operação (`X.tar.gz` vira a pasta `X`); um `.gz` solto vira o arquivo com o nome guardado no gzip. tar ustar, pax (`path`/`linkpath`) e nomes longos do GNU; pastas, arquivos e links simbólicos. Testado com arquivos gerados pelo `tar` do Windows (bsdtar). `.7z` e `.rar` não são do sistema também no macOS: abrem com o app instalado (Keka, The Unarchiver), que o Launch Services ativa.

## Ainda diferente do macOS

- Entradas JPEG 2000 em `.icns` não são lidas (só aparecem em ícones de antes de 2011; os oito apps reais testados usam PNG). O ícone cai para outra entrada do mesmo `.icns` ou para o genérico.
- Ícones de `.icns` só em JPEG 2000 (sem PNG nem os formatos antigos ao lado) ficam com o ícone genérico; o `iconutil` gera PNG desde o macOS 10.7 e os ícones da época do JPEG 2000 traziam também `it32`/`il32`, que o mOS lê.