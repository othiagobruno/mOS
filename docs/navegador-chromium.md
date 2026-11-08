# Horizonte com o motor Chromium

O **Horizonte** continua sendo o navegador do sistema do mOS, e agora o motor dele é o **Chromium** de código aberto: o
mesmo motor do Google Chrome e do Electron, que já rodam no mOS. Para o usuário há um app só, o Horizonte, no Dock, no
Launchpad, na barra de menus e como navegador padrão. O motor próprio do Horizonte (`kernel/apps/horizonte.c`, simples)
fica de reserva.

Motivo: os apps abrem o login pelo navegador do sistema, e as páginas de login de hoje não abrem num motor simples. Em
7 nov. 2026, o "Sign In to Slack" abria `https://app.slack.com/...` no Horizonte e a página dizia "This browser is no
longer supported". O "Continue with Google" do Claude desktop abria `claude.ai` em branco, com "JavaScript: 3 erro(s)".

## Desenho

Escolha do usuário (7 nov.): o Chromium é o motor do Horizonte, e não um segundo app ao lado dele. Três formas foram consideradas:

| | O que é | Custo |
|---|---|---|
| **Horizonte roda o Chromium** (escolhida) | O Horizonte executa o Chromium e as janelas dele são as do Horizonte. As abas e a barra de endereço são as do Chromium. | Pouco: o Chrome 157 já roda no mOS com HTTPS, abas e atalhos (`docs/porte-chrome.md`) |
| Interface do Horizonte, páginas do Chromium | O Horizonte desenha abas e barra de endereço, e um Chromium sem janela (headless + DevTools Protocol) renderiza e manda os quadros. | Grande: cliente do DevTools no kernel, quadros codificados e decodificados, diálogos e popups refeitos. Mais lento. |
| Chromium como app à parte | `/Aplicativos/Chromium.app` como navegador padrão, com o Horizonte ao lado. | Primeira versão desta sessão; descartada porque ficavam dois apps. |

O Chromium vem dos snapshots oficiais (`gs://chromium-browser-snapshots/Mac/<revisão>/chrome-mac.zip`, licença BSD). O
Google Chrome não pode ser redistribuído dentro da imagem, o Chromium pode. A revisão 1717346 fica 3 commits antes do
ponto de ramificação do Chrome 157 (posição 1717349). É o mesmo código do Chrome 157.0.8103 já portado. O `machocheck`
acusa só 7 símbolos ausentes: classes ObjC e constantes, que o carregador trata como no Chrome.

### Onde mora o motor

`HORIZONTE_MOTOR` = `/Sistema/Biblioteca/Horizonte/Chromium.app` (`kernel/include/launchsvc.h`). Fica fora de `/Aplicativos`,
então o Launchpad e o Launch Services não o veem como app. `ls_app_by_path` também se recusa a registrá-lo, mesmo que o
próprio Chromium consulte o caminho dele.

### Abrir (`kernel/compat/launchsvc.c`, `kernel/apps/horizonte.c`)

- O padrão para http, https, `public.html` e `public.xhtml` continua sendo o Horizonte (`com.mos.Horizonte`).
- `open_in_browser` (o "abrir URL" do Horizonte) chama `horizonte_motor_abrir(url)`:
  - com o motor aberto, a URL vai pelo serviço Mach `com.mos.abrir-url.org.chromium.Chromium` e vira uma aba nova
    (`application:openURLs:`);
  - se o processo ainda está abrindo, a URL espera o serviço (`pend_add`);
  - fechado, o motor é iniciado com a URL na linha de comando.
- Abrir o Horizonte pelo Dock ou pelo Launchpad (`launch` em `horizonte.c`) traz as janelas do motor ou inicia o motor.
  "Encerrar" no Dock pede ao motor que encerre (`horizonte_motor_encerrar`).
- Reserva: sem o motor, ou se ele não iniciar, a URL abre no motor próprio (`horizonte_legado_abrir`). Se o motor iniciado
  com a URL terminar de forma anormal nos primeiros 90 s (`bwatch`), a URL também vai para o motor próprio. O mesmo vale
  se o usuário escolheu outro navegador e ele não abrir. A Ajuda (`about:ajuda`) é sempre do motor próprio.
- `.webloc` abre a URL de dentro com o app do esquema, como no Finder. Antes, o Horizonte mostrava o plist.
- A escolha do usuário continua valendo: Ajustes > Mesa e Dock > Navegador padrão, ou o app pedindo
  (`LSSetDefaultHandlerForURLScheme`). Ela fica em
  `~/Library/Preferences/com.apple.LaunchServices/com.apple.launchservices.secure.plist`, no formato LSHandlers do macOS.

### Um app só na interface

- Dock: `apps_shortcut_for` (`registry.c`) diz que as janelas do processo do motor são do Horizonte. O Dock não cria um
  segundo ícone, e a bolinha de "aberto" do Horizonte acende com elas (`dock_proxy_windows` em `desktop.c`).
- Barra de menus: o proxy do processo se chama "Horizonte" (`gapp_for` em `guisys.c`). Os itens do menu do app vêm do
  Chromium ("Sobre o Chromium", "Encerrar Chromium").
- Ícone: o do Horizonte, não o `app.icns` do Chromium (`icon_for` em `icons.c`).
- Alertas: "“Horizonte” fechou inesperadamente" (`apps_run_mac`).

### Argumentos do motor (`kernel/apps/registry.c`)

O Chromium no mOS precisa de alguns argumentos. Quando a interface ou o Launch Services abre o motor, `macos_run`
acrescenta estes argumentos antes dos do pedido (a URL):

| Argumento | Por quê |
|---|---|
| `--in-process-gpu --disable-gpu` | Sem GPU no mOS: o compositor desenha por software numa thread do navegador. O processo de GPU sozinho sai com "errors during initialization". |
| `--no-first-run` | A janela de primeira execução é um diálogo Cocoa que o AppKit do mOS ainda não desenha, e o navegador fica esperando por ela. |
| `--no-default-browser-check` | Sem a faixa "o Chromium não é o navegador padrão". |
| `--disable-component-update` | Cada pacote do atualizador de componentes subia um processo de descompactação na partida, com o framework de 307 MiB mapeado. O CRLSet chegando no meio da 1ª navegação dava ERR_CERT_VERIFIER_CHANGED, justo na página de login. |
| `--disable-features=SpareRendererForSitePerProcess` | Sem o renderer reserva, que fica parado à espera da próxima navegação. Numa VM de 2 GiB, com o Slack aberto, a memória livre chegava a 30 MiB. |
| `--no-sandbox` | O caminho testado no porte do Chrome. Custa a faixa amarela "sinalização de linha de comando não suportada". |

`/Sistema/Biblioteca/Preferencias/com.mos.argumentos.cfg` troca estes argumentos ou acrescenta os de outros apps, com
uma linha `<CFBundleIdentifier>=<argumentos>` por app. `<id>=` sem nada desliga os do sistema. O `stage-electron.ps1
-ArgumentosApp` grava esse arquivo. Os processos que o próprio app inicia (helpers, `posix_spawn`) não recebem nada.

### Na imagem (`tools/stage-chromium.ps1`)

O sistema de arquivos da VM fica todo na RAM, então só vai o necessário (348,5 MiB):

- o executável, o `Chromium Framework` (305,6 MiB; o snapshot já é só x86_64), os 8 helpers (executável e Info.plist),
  `chrome_crashpad_handler`, `libaperitif`/`libvulkan`/`libvk_swiftshader` e os dados de componentes de `Libraries`;
- `icudtl.dat`, `resources.pak`, `chrome_100/200_percent.pak`, o snapshot do V8 e as traduções en e pt_BR (as outras 220 ficam de fora);
- ficam de fora `Assets.car`, o manifesto de políticas, `scripting.sdef` e `install.sh`.

```powershell
.\tools\stage-chromium.ps1                       # baixa (se faltar), confere o SHA-256 e monta build\chromium\Chromium.app
.\build.ps1 -OutDir build-cr
.\tools\stage-chromium.ps1 -OutDir build-cr -AbrirURL https://example.com/ -Diagnostico   # imagem de teste com o motor
#   com WHPX o relógio do kernel corre na partida e o AtrasoMs do init não vale: o open sai logo no boot
.\tools\vscode-teste.ps1 -Electron -App build\apps\x\slack-x64\Slack\Slack.app -Nome Slack -MotorHorizonte build\chromium\Chromium.app ...
```

## Correções no caminho

- **Aba "Página sem resposta" para sempre** (`userland/libsystem/dispatch.c`). Um renderer que caía antes do rendezvous
  Mach nunca era dado como morto. O `MachPortRendezvousServerMac` do Chromium vigia cada filho com
  `dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, pid, DISPATCH_PROC_EXIT, ...)`. Na saída, ele solta as portas que o filho
  não buscou, e o canal Mojo acusa o erro. No mOS essa fonte era NULL. Agora existe: uma thread confere `kill(pid, 0)` a cada
  50 ms e dispara uma vez (`dispatch_source_get_data` = `DISPATCH_PROC_EXIT`).
- **`dlopen` de imagem grande com outro app aberto** (`kernel/compat/macho.c`). O `dlopen` montava cada imagem nova num
  `kcalloc` contíguo do tamanho dela e depois copiava para o processo. Com o Slack aberto, o Chromium caía na partida
  (`kmalloc: sem memória para 322191360 bytes`, código 134). Imagens a partir de 16 MiB agora são montadas direto nas
  páginas do processo, como o spawn já fazia (CR3 do processo durante o `image_fix`).
- **Memória: o `__TEXT` compartilhado entre links** (`macho.c`, `tcache`). Medido na VM de 2 GiB: com o Slack aberto e os
  348 MiB do Chromium no VFS, sobravam 433 MiB. O `dlopen` do framework (307 MiB) passava, mas os helpers precisavam de
  outra cópia de 307 MiB e não subiam ("sem memória para o processo", 41 MiB livres). O `icache` só compartilha imagens entre
  processos de mesmo link, isto é, com os mesmos endereços. O processo principal do Chromium carrega o framework por
  `dlopen`, num endereço diferente do dos helpers. E o processo principal de um app Electron roda outro executável que os
  helpers. Só que o `__TEXT` (290 dos 307 MiB do Chromium Framework, 203 dos 215 MiB do Electron Framework) não muda com o
  endereço quando nenhum fixup cai nele. A primeira montagem direta de uma imagem grande com chained fixups guarda as
  páginas do `__TEXT` em cópia-na-escrita. As montagens seguintes do mesmo arquivo, em qualquer endereço, mapeiam essas
  páginas (`map_fresh`), e o `image_fix` pula a cópia dele. Salvaguardas:
  - só entra imagem em que nenhuma página do `__TEXT` tem chained fixups (`text_range` confere a tabela de inícios);
  - a entrada é conferida por amostragem com o arquivo (17 páginas);
  - quem escreve no código (tracepoints, o próprio app) ganha uma cópia privada, porque a página é COW;
  - a entrada sai quando nenhum processo usa mais as páginas (`tcache_trim`, junto do `macho_icache_trim`).

  Resultado: o Slack sozinho deixava 433 MiB livres e agora deixa 841 MiB, porque os helpers dele passaram a usar o
  `__TEXT` do processo principal.
- **O Chromium caía ao fechar o diálogo "Abrir Slack?"** (`userland/libsystem/appkit_extra.c`). O `slack://` chegava ao
  Slack, mas o navegador encerrava com `FATAL:ui/views/widget/widget.cc:283 Check failed: !on_call_stack_` (código 134). Os
  diálogos de aba fecham com `ViewsNSWindowCloseAnimator`, um `NSAnimation` não bloqueante. O `startAnimation` do mOS
  terminava na hora, então o `-close` da janela acontecia dentro do clique e destruía o widget que ainda tratava o evento.
  Agora, nos modos não bloqueantes, o `animationDidEnd:` vem pela fila principal depois da duração (no máximo 1 s), e
  `stopAnimation` cancela o fim pendente. O modo bloqueante (o padrão) continua terminando antes de voltar, como no AppKit.
- **Memória livre no despejo de threads** (`process.c`): a linha `despejo: threads em N ms` agora traz
  `(memória livre X de Y MiB)`, para medir a VM nos testes com `despejar-threads`.
- **O motor caía ao abrir uma URL com ele já aberto** (`userland/libsystem/appkit_extra.c`). O `open https://app.slack.com/`
  chegava ao motor e virava uma aba nova, mas uns 90 s depois o processo principal recebia SIGSEGV, e o mOS mostrava
  '"Horizonte" fechou inesperadamente'. Logo antes, o log mostrava `-[NSBitmapImageRep initWithData:] ainda não
  implementado`, `ERROR:ui/gfx/image/image_mac.mm:116] Unable to decode PNG at 1.` e
  `+[NSImage imageWithSize:flipped:drawingHandler:] ainda não implementado`. Com a URL já no histórico e o favicon no banco,
  o Chromium monta o `NSImage` dos menus (Histórico, Janela) a partir do PNG. Se o PNG não decodifica, ele usa uma imagem de
  erro. As duas vinham nil, e o `CHECK(image_)` do `ImageRepCocoa` derrubava o processo. Agora o
  `initWithData:`/`+imageRepWithData:` decodifica PNG, JPEG, GIF e BMP pelo `GUI_DECODE_IMAGE` (RGBA pré-multiplicado),
  `bitmapImageRepByRetaggingWithColorSpace:` devolve o próprio rep, e `imageWithSize:flipped:drawingHandler:` devolve
  uma imagem do tamanho pedido, sem desenho (o mOS não tem `lockFocus` em bitmap).

## Estado (7 nov. 2026)

Testado na VM com WHPX e 2 núcleos; a memória de cada rodada vai no título.

Rodada 8, desenho atual com o Slack, 2304 MB (`vscode-teste.ps1 -Electron … -MotorHorizonte build\chromium\Chromium.app`):

| Teste | Resultado |
|---|---|
| Clique em "Sign In to Slack" | ✅ "ls: https://app.slack.com abre no Horizonte (motor Chromium iniciado com a URL)". A página real de login, `app.slack.com/ssb/signin?ssb_vid=…` ("Insira seu e-mail para entrar", Google e Apple), renderiza no Horizonte. A barra de menus diz "Horizonte", e o Dock mostra o Horizonte com a bolinha. O teste para aí, porque o login é do usuário. |
| Volta: `slack://open` digitado na barra | ✅ O motor mostra "Abrir Slack?". "Abrir Slack" leva a URL ao Slack ("ls: slack://open entregue a Slack" e "[abrir-url] recebida slack://open"), que vem para a frente. O motor segue aberto. |
| Clique no Horizonte no Dock | ✅ A janela do motor volta para a frente. |
| `open https://app.slack.com/` no Terminal, com o motor aberto | ✅ "ls: https://app.slack.com entregue ao Horizonte (motor Chromium)", "[abrir-url] recebida" no motor, e uma aba nova. ❌ Uns 90 s depois, o motor caiu: '"Horizonte" fechou inesperadamente' (código -11). A causa era o favicon, já corrigida (ver "O motor caía ao abrir uma URL com ele já aberto"). |
| Memória livre | Slack sozinho: 1081 MiB. Com o motor aberto: 552 MiB. Na página de login: 264 a 305 MiB. Com a segunda aba: 231 MiB. |

Rodada 9, a correção do favicon, só o motor, 2048 MB:

| Teste | Resultado |
|---|---|
| Dois `open https://app.slack.com/` na partida | ✅ O primeiro inicia o motor com a URL. O segundo chega com o motor ainda subindo, espera na fila do Launch Services ("ls: documento entregue a org.chromium.Chromium") e vira a segunda aba. |
| Outro `open` pelo Terminal com o favicon já no banco, depois o menu Histórico | ✅ Uma aba nova, e o menu lista a página em "Visitados recentemente". Nenhum `Unable to decode PNG`, e o processo principal segue vivo. |
| Memória | ❌ Três abas da página inicial do Slack (`slack.com/intl/pt-br/`) não cabem em 2048 MB. A memória livre chegou a 170 MiB, 283 pedidos de `kmalloc` de 8 a 12 MiB falharam, e dois renderers caíram com SIGSEGV ("falta de página … pte 0"). A aba mostra "Ah, não! … Código de erro: 11", e o processo principal segue. |
| Harness (`machotest`, programa de teste) | ✅ `initWithData:` devolve o rep 4×4 com o RGBA certo. O retag devolve o mesmo rep, e `imageRepWithData:` funciona. Dados que não são imagem dão nil. `imageWithSize:flipped:drawingHandler:` devolve 16×16. |

Rodada 7, desenho atual sem o Slack, 2048 MB:

| Teste | Resultado |
|---|---|
| Partida | "ls: 40 apps, … navegador padrão Horizonte" e "ls: motor do Horizonte: Chromium (/Sistema/Biblioteca/Horizonte/Chromium.app)". Nenhum app "Chromium" no Launch Services nem no Launchpad. |
| `open https://example.com/` | ✅ "ls: https://example.com abre no Horizonte (motor Chromium iniciado com a URL)". A página renderiza, já com árabe e chinês (fontes novas do sistema). A barra de menus diz "Horizonte", com os menus do motor, e o Dock mostra só o ícone do Horizonte. |
| Esquema externo: `descompactador://teste` digitado na barra | ✅ O motor mostra "Abrir Descompactador?". "Abrir Descompactador" leva a URL ao app ("[abrir-url] recebida descompactador://teste"), e o Horizonte segue aberto, sem o `CHECK` de antes (correção do `NSAnimation`). |

Até a rodada 6, o Chromium ainda era um app em `/Aplicativos`; o motor e o Launch Services são os mesmos no desenho atual:

| Teste | Resultado |
|---|---|
| `open https://example.com/` (init → `/usr/bin/open` → `SYS_LS`) | ✅ O kernel inicia o Chromium com os argumentos do sistema, e a página renderiza (latim e cirílico; árabe e CJK sem fonte, pendência conhecida). |
| Slack + Chromium, clique em "Sign In to Slack" | ✅ "ls: https://app.slack.com abre no …". A página real de login, `app.slack.com/ssb/signin?ssb_vid=…`, renderiza no Chromium: "Insira seu e-mail para entrar", Google, banner de cookies. O teste para aí, porque o login é do usuário. |
| Volta `slack://`: `slack://open` digitado na barra do Chromium | ✅ O Chromium mostra o próprio diálogo "Abrir Slack?", com o nome que o Launch Services informou. "Abrir Slack" chama `openURL:`, e o log mostra "ls: slack://open entregue a Slack" e "[abrir-url] recebida slack://open" no Slack, que vem para a frente. O Chromium caiu logo depois; a correção do `NSAnimation` acima trata isso. |
| Memória | Slack sozinho: 433 MiB livres sem o `tcache` e 841 MiB com ele. Com o Slack e o Chromium na página de login, a memória livre caiu para 29 a 62 MiB, e o kernel passou a negar alocações. Numa das rodadas, o renderer e o processo principal do Chromium receberam SIGSEGV, e o `bwatch` reabriu a URL no motor próprio. Em 2048 MB os dois juntos não cabem; a rodada 8 usou 2304 MB. |

Pendências:
- `Chromium Framework+0x838290a` (base `0x400001020000` nos helpers, `0x400100000000` no processo principal) é onde o
  relatório de qualquer `CHECK` ou `LOG(FATAL)` do motor cai. É um laço que lê o primeiro elemento de um contêiner com
  início nulo (`movq (%rax),%rsi` com `rax = 0`), provavelmente porque o `backtrace()` do libSystem devolve 0 quadros. A
  mensagem do `CHECK` nunca chega ao log. A pista são as linhas `objc[mOS]: … ainda não implementado` logo antes, como
  na queda do favicon. Um renderer ainda cai às vezes na partida por esse caminho, de causa desconhecida; com a fonte
  PROC, o navegador repõe o renderer e a aba segue.
- Memória: o Slack com o motor precisa de 2304 MB (em 2048 MB a rodada 6 caiu). Páginas pesadas em várias abas esgotam
  2048 MB mesmo sem o Slack. Nesses casos, `kmalloc` de 8 a 12 MiB falham, e os renderers caem com falta de página em
  endereço sem PTE. Falta ver se alguma alocação que falha deixa uma região dada como mapeada sem páginas.
- `-[NSImage addRepresentation:]`, `-[NSImage representations]` e `-[NSBitmapImageRep initWithCGImage:]` ainda devolvem nil,
  então os ícones que o motor converte para `NSImage` (menus, Dock) ficam sem desenho.
- O `AtrasoMs` do init não vale com WHPX, porque o relógio do kernel corre na partida: os `open` agendados com
  `-AbrirAposMs` saem logo no boot. Para abrir uma URL com o motor já aberto, use o Terminal da VM (`qmp-passos.ps1`).
- A faixa "--no-sandbox". O sandbox do mOS são stubs (`darwin.c`). Falta testar o Chromium sem `--no-sandbox`.
- ~~A memória do VFS: o arquivo do framework fica na RAM além da cópia montada.~~ Resolvido em 7 nov. (noite): os
  arquivos de 16 MiB para cima vêm do disco em buffers de `kmalloc_pages` (alinhados à página e liberados página a
  página pelas contagens de `pref[]`), e o `tcache` mapeia o `__TEXT` direto dessas páginas (`tcache_borrow`, "mapeado
  das páginas do arquivo, sem cópia"). O `vfs_append` nunca escreve num buffer desses no lugar. Economia: 198 MiB com o
  Electron Framework e 248 MiB com o Google Chrome Framework. O Claude com o Chrome passou a caber em 2816 MB (de 543 a
  773 MiB livres na página de login do Google; antes, 165 MiB e o serviço de rede do Chrome caía). Detalhes em
  [compat-apps.md](compat-apps.md), na seção do Google Chrome. `write`, `pwrite` e `ftruncate` também passaram a gravar
  no buffer do arquivo (`proc/process.c`), sem copiar o arquivo inteiro a cada escrita, e o mmap de arquivo usa as
  páginas desses buffers (`mm/lazy.c`). Na outra medição do mesmo fluxo, a memória livre com o Chrome na página do
  Google subiu de 321–343 MiB para 567–596 MiB, e as falhas de `kmalloc` caíram de 316 para zero.
- Memória, o que veio depois e o que falta:
  - Feito: o `fat.c` põe em páginas alinhadas os arquivos a partir de 256 KiB (antes 16 MiB), e o `lazy.c` mapeia
    direto o `icudtl.dat`, os `.pak` e o snapshot do V8 em cada processo que os usa.
  - Feito: o libSystem relocado (3,3 MiB) é montado uma vez, num buffer de `kmalloc_pages`, e entra em cada processo
    em cópia na escrita (`map_libsystem`/`ls_build` em `macho.c`, "libSystem (… KiB) montado uma vez e compartilhado
    entre os processos"). Antes cada processo novo pedia um `kcalloc` contíguo de 3,2 MiB e ficava com uma cópia
    própria, e com a VM fragmentada isso já impediu um renderer de subir. Os 2,2 MiB só de leitura ficam
    compartilhados, e as páginas de dados e bss viram do processo na primeira escrita. Na rodada de 2816 MB com as duas
    mudanças, a memória livre só com o Claude subiu de 1079–1081 para 1096–1098 MiB, e com o Chrome na página do Google
    ficou em 548–635 MiB, sem falha de `kmalloc`. Um diagnóstico temporário (já retirado) conferiu a imagem página a
    página a cada processo novo, e ela nunca mudou.
  - Resolvido em 8 nov.: numa das rodadas com o libSystem compartilhado, o processo principal do Chrome caiu no
    `CHECK` do `ScopedFD` (`scoped_file.cc:45`, "Bad file descriptor") 8 s depois de abrir o login do Google. Logo
    antes, o `fileport_makeport` falhou porque o kernel já não tinha o fd (`wrapped_platform_handle.cc:105`). A causa
    era o `dup()` do libSystem: ele procurava um número livre (`SYS_FDFLAGS` dando EBADF) e só depois chamava `dup2`.
    Nesse intervalo, outra thread podia abrir um arquivo e receber o mesmo número, e o `dup2` trocava o fd dela em
    silêncio. Os dois donos acabavam fechando o mesmo fd. O `base::File::Duplicate` do Chromium usa `dup`, e o processo
    principal tem dezenas de threads. Agora o `SYS_DUP2` aceita `MOS_DUP_ANY`: o kernel escolhe o menor fd livre e
    copia na mesma syscall (`dup()` e `fcntl(F_DUPFD)`, que agora respeita o mínimo pedido). O `kqueue()` e o
    `cx_kqueue_dup` tinham a mesma corrida na tabela `kqs[]` e passaram a tomar a vaga com CAS.
    Teste de estresse (`duptest`, Mach-O, uma thread com 20000 `dup` e três abrindo, lendo e fechando outro arquivo, numa
    VM de 4 núcleos): o padrão antigo deu 1871 + 5382 leituras do arquivo errado e 2460 + 4432 `close` com EBADF; o
    `dup()` novo, nenhuma. O `posixteste`, o `dispatchteste` (30/30), o `sinaisteste` e o `smpteste` continuam
    passando.
  - Os buffers de arquivo continuam contíguos. Com a escrita no lugar, só o crescimento de um arquivo grande pede um
    bloco novo, e só a cada dobra. Trocar por listas de páginas exigiria uma área virtual do kernel com páginas de
    4 KiB, porque todo leitor de `vnode->data` supõe um buffer contínuo. Nas rodadas em 2816 MB nenhum crescimento
    falhou depois da escrita no lugar.
- `--disable-component-update` deixa o CRLSet e os outros componentes sem atualização. O repositório de certificados raiz é o embutido no Chromium.
- Os itens do menu do app ainda dizem "Chromium" (vêm dos `.pak` de tradução do Chromium).
