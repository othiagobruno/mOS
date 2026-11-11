# Meta: 100% dos 12 apps do macOS no mOS

Marco M1 da meta. Este documento define o que "100%" quer dizer, os níveis de cada app, o roteiro N3 de cada um e o
nível atual. Os números de N0 e N1 não são escritos à mão: saem de `tools/meta/placar.py`, que grava
[meta-placar.md](meta-placar.md). Para atualizar, rode o placar de novo (ver [Como medir](#como-medir)).

## A meta

Os 12 apps abaixo chegam a **N4**: N3 três vezes seguidas, inclusive depois de um reboot, na VM e no PC real.

| # | App | Pacote oficial (versão medida) | Tipo |
|---|---|---|---|
| 1 | Codex CLI | `codex-aarch64-apple-darwin.tar.gz` 0.167.0 | CLI Rust, arm64 |
| 2 | Codex desktop / ChatGPT | `ChatGPT-darwin-arm64-26.1103.60933.zip` | Electron, arm64 |
| 3 | Cursor | `Cursor-darwin-arm64.zip` 3.29.15 | Electron, arm64 |
| 4 | Antigravity | `Antigravity.dmg` 2.31.0 darwin-x64 | Electron + `language_server` (Go) |
| 5 | VS Code | 1.145.3 darwin-arm64 | Electron, arm64 |
| 6 | Slack | `Slack-4.53.287-macOS.dmg` x64 | Electron |
| 7 | Grok Bot | `Grok_Bot_0.79.4_x64.dmg` | Electron |
| 8 | Claude Desktop | 4.23118.1 universal | Electron + addons Swift |
| 9 | Postico | `postico-9817.dmg` (2.3.5) universal | AppKit + Swift |
| 10 | WhatsApp | 2.26.45.13 universal | Catalyst/SwiftUI + WebKit |
| 11 | OpenBot | `OpenBot-0.66.0-x64.zip` | Electron |
| 12 | Setapp | `Setapp.zip` 4.0.0 universal (dl.devmate.com) | AppKit + WebKit, agentes de launchd |

URLs e SHA-256: os 9 primeiros pacotes do `tools/stage-apps-oficiais.sh` mais Antigravity, Postico e Setapp, que só o
placar lista por enquanto (`EXTRAS` em `tools/meta/placar.py`, SHA-256 medido no download oficial de 10 nov. 2026).

**Fora do 100%**, de propósito: FairPlay e apps da Mac App Store; iCloud, APNs, Sign in with Apple e compras; vídeo com DRM.
Quando um roteiro N3 passa por um desses (ex.: notificação push), o passo é marcado como fora e não conta.

## Níveis

| Nível | Nome | Critério | Quem mede |
|---|---|---|---|
| N0 | Instala | O pacote oficial está na imagem com SHA-256 conferido: listado no `tools/stage-apps-oficiais.sh`, o `tools/verificar-imagem-apps.py` acha o executável Mach-O real em `/Aplicativos` | `placar.py` (host) |
| N1 | Liga | `machocheck` do bundle inteiro (executável, frameworks, helpers, `.node`) contra o `libSystem.mos` do build e os dylibs reais (libc++, Swift, ICU, libz…; dylib com o marcador `STUB mOS libswift` não entra) dá **0 ausentes**, e nenhum import do app cai num **stub vazio** de `userland/libsystem/sys_zerados.c` ou `swiftui_imports.c` | `placar.py` (host) |
| N2 | Abre | A janela principal fica de pé por 10 min na VM sem travar: o processo não cai, todas as capturas desde a primeira janela acham a janela com conteúdo desenhado. Captura + log serial guardados | `tools/meta/n2-vm.ps1` (VM) |
| N3 | Funciona | O roteiro do app (abaixo) passa inteiro. O login é do Thiago | Thiago, com o roteiro |
| N4 | Estável | N3 três vezes seguidas, uma delas depois de reboot, na VM e no PC real | Thiago |

Cada nível exige o anterior. Um app em N1 ✗ não é testado em N2 para o placar, mesmo que abra: os stubs vazios
no caminho podem quebrar qualquer coisa mais adiante.

### O que o N1 mede e o que não mede

- É estático: diz se cada import tem implementação no mOS, não se a implementação está certa.
- "Stub vazio alcançado" quer dizer que o app importa o nome e ele só existe no libSystem como corpo vazio
  (`void f(void) {}`) ou ponteiro nulo (`void *k;`) daqueles dois arquivos. Stubs com corpo em outros arquivos não entram.
- A fatia medida é a x86_64 quando o executável tem uma; senão a arm64 (Codex CLI, ChatGPT, Cursor, VS Code).
  No arm64, o `machocheck --arch arm64` lê o slice arm64 e resolve contra os nomes do `libSystem.mos` x86_64 do build.
  O libSystem arm64 sai do mesmo fonte (docs/tradutor-arm64.md), então mede a cobertura de símbolos, não o tradutor.

## Roteiro N3 por app

Cada passo é feito na VM (e depois no PC real) com o app aberto a partir de `/Aplicativos`. Login e contas são do
Thiago; nenhum outro agente faz login. "Passa" = todos os passos sem travar, sem janela em branco, sem erro do app.

1. **Codex CLI**: no Terminal do mOS, `codex --version`; `codex login` pelo código de dispositivo (Thiago conclui no
   navegador); numa pasta com um repositório git, `codex "liste os arquivos e resuma o README"`; o Codex lê arquivos,
   roda um comando com aprovação e responde; `Ctrl+C` sai e o terminal volta normal (termios restaurado).
2. **Codex desktop / ChatGPT**: abre na tela de login; Thiago entra; abre uma conversa nova, manda uma mensagem e recebe
   a resposta em streaming; abre o painel do Codex, escolhe uma pasta local e pede uma alteração pequena; o diff aparece e é aplicado.
3. **Cursor**: abre a janela de boas-vindas; abre uma pasta; edita e salva um arquivo; o terminal integrado roda
   `ls` e `git status`; a busca em arquivos (ripgrep) acha um texto; Thiago entra na conta e o chat do agente responde
   a uma pergunta sobre o arquivo aberto; instala uma extensão do marketplace.
4. **Antigravity**: abre; Thiago entra com a conta Google (pelo navegador); abre uma pasta; o `language_server` sobe
   (sem erro no painel); o agente responde e edita um arquivo; o terminal integrado roda um comando.
5. **VS Code**: abre uma pasta; edita e salva; terminal integrado com o shell; busca em arquivos; Git (painel de
   controle de código mostra a mudança, faz commit local); instala uma extensão do marketplace e ela ativa.
6. **Slack**: tela de boas-vindas; "Sign in" abre o navegador e a volta `slack://` chega ao app; Thiago entra no
   workspace; abre um canal, lê o histórico, manda uma mensagem e uma reação; abre uma thread; anexa uma imagem.
7. **Grok Bot**: abre; Thiago entra; manda uma pergunta e recebe a resposta; abre o histórico; muda uma preferência e
   ela continua depois de fechar e abrir o app.
8. **Claude Desktop**: chega ao "Sign In"; "Continue with Google" abre o navegador e a volta `claude://` chega; Thiago
   entra; nova conversa com resposta em streaming; anexa um arquivo; abre as configurações; conectores MCP locais
   listados (sem exigir computer use).
9. **Postico**: abre a janela de favoritos; cria um favorito para um PostgreSQL local ou de teste do Thiago; conecta;
   lista tabelas; abre uma tabela e edita uma célula; roda uma consulta SQL no editor e vê o resultado; fecha e o favorito continua salvo.
10. **WhatsApp**: abre na tela do QR code; Thiago lê com o celular; a lista de conversas carrega; abre uma conversa,
    manda texto e um emoji; recebe uma mensagem; abre uma imagem recebida. Chamadas de voz/vídeo e notificação push: fora.
11. **OpenBot 0.66**: abre; Thiago entra; a lista de agentes aparece; manda uma mensagem a um agente e recebe a resposta;
    fecha e abre o app e a sessão continua.
12. **Setapp**: abre o cliente; Thiago entra pelo navegador com o código por e-mail; o catálogo carrega (WebKit);
    instala um app gratuito do catálogo e ele aparece em `/Aplicativos`; os agentes `SetappAgent`/`SetappLauncher`
    sobem pelo launchd. Compras: fora.

## Nível atual

A tabela completa, com números, data e commit, está em [meta-placar.md](meta-placar.md). Resumo da última medição:

<!-- resumo: preenchido a partir do placar -->

## Como medir

N0 e N1, no Linux ou no Windows com Python 3 e um `cc`:

```sh
export MOS_ZIG=~/.local/opt/zig-x86_64-linux-0.17.1/zig
tools/build.sh --out build-placar          # libSystem.mos, esp/ e os apps do stage (baixa ou usa build-placar/apps-dl)
python3 -I tools/meta/placar.py --build build-placar     # grava docs/meta-placar.md
```

`--cache DIR` aponta outro cache de pacotes (padrão `<build>/apps-dl`, o do stage). `--apps slack,claude` mede só alguns.
Listas completas por app em `<build>/placar/<id>/` (`ausentes.txt`, `stubs-alcancados.txt`, `machocheck.txt`).

N2, onde houver QEMU (PC do Thiago com WHPX; ver "Como testar" em [pendencias-apps-agentes.md](pendencias-apps-agentes.md)):

```powershell
.\tools\meta\n2-vm.ps1 -Id slack -App "build\apps\x\slack\Slack\Slack.app" -Accel whpx
# um app por vez; depois de todos:
python3 tools\meta\placar.py --build build --n2 build-n2
```

O `n2-vm.ps1` cobre os 8 apps Electron (ChatGPT, Cursor, Antigravity, VS Code, Slack, Grok Bot, Claude, OpenBot) pelo
`tools/vscode-teste.ps1 -Electron`. WhatsApp, Postico, Setapp e o Codex CLI ainda não têm partida automática numa
imagem de teste. O script recusa esses ids em vez de inventar um resultado. Até alguém fazer essa partida, o N2 deles é
manual, com a imagem completa do `build.sh`.

N3 e N4: o Thiago, com o roteiro acima. O resultado entra aqui à mão, com data.
