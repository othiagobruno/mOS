# O que ainda falta: Grok Bot, WhatsApp, Cursor, Antigravity, Postico e Codex desktop no mOS (8 nov. 2026)

Meta: os seis apps 100% funcionais no mOS. Este arquivo lista só o que falta. Os detalhes do que já foi feito estão em
[compat-agentes.md](compat-agentes.md) e [swift-objc-runtime.md](swift-objc-runtime.md).

## Situação por app

| App | Onde chega hoje (VM, WHPX) | O que falta |
|---|---|---|
| **Codex desktop** | Abre a tela "Get started". O clique em "Sign in with ChatGPT" chega ao app, o app-server faz `account/login/start` e `auth.openai.com` abre no Horizonte. Estável em 3 de 3 rodadas com o `thread_suspend` corrigido. | Ver a seção Codex abaixo |
| **Cursor** | Tela de login. O clique em "Log In" abre `cursor.com` no Horizonte. | Ver a seção Cursor abaixo |
| **Antigravity** | Tela "Welcome", com o `language_server` (Go) rodando. O clique em "Continue with Google" inicia o OAuth. | Ver a seção Antigravity abaixo |
| **Grok Bot** | Tela de login (pt-BR). O helper "Computer Use" liga todos os símbolos. Clique sintético e captura de tela funcionam (Calculadora aberta pelo Dock). | Ver a seção Grok Bot abaixo |
| **Postico** | O SwiftUI.framework com ObjC existe (b16b648): 0 símbolos de SwiftUI ausentes. No harness chega à janela principal. | ~30 classes do AppKit; não testado na VM (com a sessão do SwiftUI) |
| **WhatsApp** | Faltam 136 símbolos de SwiftUI (eram 555). | Os 136 símbolos e depois a partida (com a sessão do SwiftUI) |

## Pendências em comum

1. **Página de login no navegador.** O motor próprio do Horizonte mostra `auth.openai.com`, `accounts.google.com` e
   `cursor.com` em branco. Testar com o motor Chromium (`-MotorHorizonte build\chromium\Chromium.app`, ver
   [navegador-chromium.md](navegador-chromium.md)), que deve renderizar essas páginas.
2. **Volta do login ao app.** Depois do login, o navegador abre a URL de retorno (`cursor://`, `antigravity://`, o
   localhost do Codex). Isso precisa chegar ao app (serviço `com.mos.abrir-url.<id>`, ver
   [launch-services.md](launch-services.md)). Ainda não foi exercitado de ponta a ponta.
3. **Login de verdade.** É feito pelo usuário, com as contas dele. Nada além de abrir a página foi testado.
4. **Estabilidade:**
   - #GP intermitente no renderer do Codex em `Codex Framework+0x21bf566` (o renderer reinicia sozinho);
   - reinícios raros da VM (cerca de 1 em 7 rodadas antes do 0ffff3c; nenhum visto depois);
   - o WHPX não mostra o estado da CPU antes do reset.
5. **Regressão do hit-test (7653c60).** Verificado no Codex, no Cursor e no Antigravity. Falta o VS Code, o Slack, o
   Claude desktop e o Grok Bot.

## Codex desktop

- O Sparkle (atualizador) diz "error in retrieving update information". O `NSXMLDocument` agora lê o appcast. Falta ver,
  com o diagnóstico `NSURLSession[delegate]`, qual URL e qual status ele recebe.
- O `-[ASWebAuthenticationSessionWebBrowserSessionManager sharedManager]` devolve nil. Ver se algum fluxo de login usa
  isso.
- O botão "Sign in another way" (chave de API) não foi testado.

## Cursor

- Git na imagem: o `tools/build.sh` compila o git do fonte (`tools/build-git.sh`, `/usr/bin/git`). Sem ele, o
  `--cursor-extras` usa o bottle: `tools/stage-git.sh` põe o git 2.55.0 do Homebrew (bottle Intel) em `/usr/local/opt/git`, com
  `/usr/bin/git` e `/usr/local/bin/git` como link do mOS. Os ~150 hardlinks de `libexec/git-core` viram `#mOS-link:`:
  a raiz fica com 27 MiB, não 619 MiB. Entram `libpcre2-8.0.dylib` e `libintl.8.dylib` (`@@HOMEBREW_PREFIX@@`).
  `tools/stage-apps-oficiais.sh` baixa `git gettext` (`fetch-bottles.sh`) e passa `--bottles` ao Cursor. Teste de
  host: `tools/test-git-image.sh` (version, init, add, commit, log, status, diff, branch no machotest).
- `/usr/bin/security`: Mach-O de `userland/cli-macos/security.c` (o `tools/build.sh` compila). `find-certificate -a -p`
  devolve as 118 raízes da Mozilla do kernel (o `@vscode/proxy-agent` do Cursor e do VS Code chama exatamente isso);
  `find/add/delete-generic-password` usam o Keychain do mOS. Teste de host: `tools/test-security-cli.sh`.
- Falta provar na VM: o Cursor chamando os dois (painel Source Control com um repositório, proxy-agent sem erro no log).
- O login (usuário).

## Antigravity

- **O OAuth do Google volta com `invalid_scope` ("invalid=[http]").** O escopo chega corrompido ao Google. O pedido não
  passa pelo `ls_open_c`: o diagnóstico `/Sistema/Biblioteca/abrir-url-completo` não registrou nada. O 302 aparece no
  cliente HTTP do kernel (`net/http.c`). Falta achar quem abre a URL (o `language_server` em Go, o `/usr/bin/open` ou
  outro caminho do Electron) e onde o `scope` se perde.
- O `cable_ble_scanner` (Swift, opcional) não liga.

## Grok Bot

- Testar o login (clique em "Fazer login" no kernel novo) e a volta ao app.
- Computer use de ponta a ponta. O helper liga, mas ainda falta:
  - ver o helper subir pelo próprio Grok Bot (depois do login);
  - confirmar que o `SCScreenshotManager` devolve a imagem ao helper.
  
  Acessibilidade (`AXUIElement*`) responde "sem permissão" de propósito: o mOS ainda não tem a árvore de acessibilidade.
- `SCStream` (vídeo contínuo da tela) não existe.

## Postico e WhatsApp (sessão "SwiftUI.framework com interop ObjC")

- Postico: as ~30 classes do AppKit que faltam, até a janela de conexões aceitar entrada; depois o teste na VM.
- WhatsApp: os 136 símbolos de SwiftUI que faltam; depois a partida no harness (`build\wa-run`) e na VM.

## Como testar (lembrete)

- Imagem de teste pós-PR #4: o `mkimg` com `--test-overlay <OutDir>\overlay-teste`. O overlay leva
  `Preferencias\com.mos.teste.sem-assistente` e o `Users`. O `esp` não pode ter `Users`, `Preferencias` nem `*.log`.
- O `build.ps1` apaga o `esp` inteiro e falha nos caminhos longos dos apps Electron. Para trocar só o kernel: compilar
  noutra pasta (`build.ps1 -OutDir build-k2`), copiar `esp\EFI\BOOT\BOOTX64.EFI` e rodar o `mkimg` de novo.
- Cliques: `tools/testdata/cgevent/cgevent-teste.c` como serviço do launchd, com
  `Argumentos=x y xf yf espera RRGGBB`. Ele espera a cor do alvo na tela e clica com `CGEventPost`.
- Diagnósticos (arquivos em `/Sistema/Biblioteca`):
  - `ca-log`: quem recebe o `mouseDown`;
  - `abrir-url-completo`: a URL inteira passada ao Launch Services;
  - `despejar-threads`.
- QEMU: encerrar só o da própria porta. Outras sessões rodam VMs ao mesmo tempo.
