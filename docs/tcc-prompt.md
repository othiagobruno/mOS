# Diálogo de permissão do TCC

A Interface mostra um diálogo do próprio mOS quando um aplicativo pede uma permissão do TCC: gravar a tela, acessibilidade, câmera, microfone, contatos, agenda, lembretes, fotos, pastas (Mesa, Documentos, Transferências, volumes), disco inteiro, monitoramento de teclado, controle do computador, Bluetooth, localização, rede local, reconhecimento de fala, controlar outro app (Apple Events) e enviar notificações. O registro é o de `tcc.c`.

## Quando aparece

O libsystem grava `pending` em `NSHomeDirectory()/Library/Application Support/com.mos.tcc/decisions.txt`. Hoje `NSHomeDirectory` é `/Users/Visitante` (`foundation.c`), então o arquivo é:

`/Users/Visitante/Library/Application Support/com.mos.tcc/decisions.txt`

`cx_tcc_state` devolve 3 nesse estado. O window server relê o arquivo cerca de quatro vezes por segundo. Se a linha é de um serviço conhecido (tabela `tcc_prompt_texts` em `tcc_prompt_core.inc`) e está `pending`, abre o diálogo. Um pedido de cada vez, na ordem do arquivo. Apple Events levam o app alvo no nome: `kTCCServiceAppleEvents+com.apple.finder` ("“A” gostaria de controlar “Finder”"). Serviço desconhecido não abre diálogo e continua negado.

Na tela de início de sessão e no assistente de instalação o diálogo não abre. Um que já estava aberto fecha sem decidir; o `pending` continua no arquivo.

## O que os botões fazem

Negar (esquerda, Esc) grava `denied`. Permitir (direita, na cor de destaque, Enter) grava `granted`. Enter só vale depois de um quarto de segundo, para uma tecla repetida não aceitar o diálogo seguinte.

`cx_tcc_decide` mora no libsystem. O window server não liga essa biblioteca, então o clique não chama o símbolo. Ele regrava o arquivo na mesma gramática de `tcc_decide`, que é o miolo de `cx_tcc_decide` (`tcc.c`): uma linha `bundle serviço granted|denied|pending` por registro, os outros registros preservados. O teste do host chama `tcc_decide` e confere que o texto é o mesmo que `tcc_prompt_set` + `tcc_prompt_format` produziriam — é isso que o window server escreve.

`denied` não pede de novo. `granted` também não. A escolha está no arquivo, então outra consulta e outro processo que leiam o mesmo caminho enxergam o mesmo estado.

## Pedido com bloco de conclusão

`cx_tcc_request_async` (`tcc.c`) é o que os `requestAccess…` usam: `AVCaptureDevice requestAccessForMediaType:` (câmera e microfone), `CNContactStore requestAccessForEntityType:`, `PHPhotoLibrary requestAuthorization:` e `UNUserNotificationCenter requestAuthorizationWithOptions:`. Com escolha já gravada o bloco volta na hora; sem ela, grava `pending` e acompanha o arquivo numa fila global (a cada 0,2 s) até o usuário responder. Um `tccutil reset` no meio conta como "não".

## tccutil

`/usr/bin/tccutil reset SERVIÇO [BUNDLE_ID]` (`userland/cli-macos/tccutil.c`): `All`, `Camera`, `Microphone`, `AddressBook`, `Photos`, `ScreenCapture`, `Accessibility`, `AppleEvents` (com os pares por alvo), `Notifications`... ou o nome `kTCCService...` inteiro. Saída igual à da Apple.

## O que muda para o aplicativo

`CGPreflightScreenCaptureAccess` é `cx_tcc_preflight("kTCCServiceScreenCapture")`. `AXIsProcessTrusted` é `cx_tcc_preflight("kTCCServiceAccessibility")`. Os dois passam a 1 só quando a linha correspondente está `granted`. Sem gravação de tela a captura não devolve pixels. Sem acessibilidade a árvore continua fechada, `CGEventTapCreate` devolve nulo e `CGEventPost` não entra na fila.

## Arquivos

- `kernel/ui/tcc_prompt_core.inc` — qual pedido abre o diálogo e qual botão é conceder ou negar. Sem arquivo e sem desenho.
- `kernel/ui/tcc_prompt.c` — leitura, vidro, botões e gravação.
- `kernel/ui/desktop.c` — o loop chama o tick, o desenho e o mouse/teclado. O resto do desktop não mudou.
- `tools/tccprompttest.c` — pending, diálogo, `tcc_decide`, estado relido, textos dos serviços (134/134 no gcc).
- `tools/tccutiltest.sh` — `tccutil reset` por serviço, por app, `All`, AppleEvents (8/8).

O desenho não foi executado (build proibido até o fim da frente). O que os testes cobrem é a política, os textos e o arquivo.
