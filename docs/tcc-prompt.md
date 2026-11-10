# Diálogo de gravação de tela e acessibilidade

A Interface mostra um diálogo do próprio mOS quando um aplicativo pede para gravar a tela ou usar a acessibilidade. O registro continua o do Sistemas, em `tcc.c`. Este lado não muda esse contrato.

## Quando aparece

O libsystem grava `pending` em `NSHomeDirectory()/Library/Application Support/com.mos.tcc/decisions.txt`. Hoje `NSHomeDirectory` é `/Users/Visitante` (`foundation.c`), então o arquivo é:

`/Users/Visitante/Library/Application Support/com.mos.tcc/decisions.txt`

`cx_tcc_state` devolve 3 nesse estado. O window server relê o arquivo cerca de quatro vezes por segundo. Se a linha é `kTCCServiceScreenCapture` ou `kTCCServiceAccessibility` e está `pending`, abre o diálogo. Um pedido de cada vez, na ordem do arquivo. `userNotifications` não usa este diálogo.

Na tela de início de sessão e no assistente de instalação o diálogo não abre. Um que já estava aberto fecha sem decidir; o `pending` continua no arquivo.

## O que os botões fazem

Negar (esquerda, Esc) grava `denied`. Permitir (direita, na cor de destaque, Enter) grava `granted`. Enter só vale depois de um quarto de segundo, para uma tecla repetida não aceitar o diálogo seguinte.

`cx_tcc_decide` mora no libsystem. O window server não liga essa biblioteca, então o clique não chama o símbolo. Ele regrava o arquivo na mesma gramática de `tcc_decide`, que é o miolo de `cx_tcc_decide` (`tcc.c`): uma linha `bundle serviço granted|denied|pending` por registro, os outros registros preservados. O teste do host chama `tcc_decide` e confere que o texto é o mesmo que `tcc_prompt_set` + `tcc_prompt_format` produziriam — é isso que o window server escreve.

`denied` não pede de novo. `granted` também não. A escolha está no arquivo, então outra consulta e outro processo que leiam o mesmo caminho enxergam o mesmo estado.

## O que muda para o aplicativo

`CGPreflightScreenCaptureAccess` é `cx_tcc_preflight("kTCCServiceScreenCapture")`. `AXIsProcessTrusted` é `cx_tcc_preflight("kTCCServiceAccessibility")`. Os dois passam a 1 só quando a linha correspondente está `granted`. Sem isso, a captura não devolve pixels e a árvore de acessibilidade continua fechada. Este diálogo não altera `CGEventPost`.

## Arquivos

- `kernel/ui/tcc_prompt_core.inc` — qual pedido abre o diálogo e qual botão é conceder ou negar. Sem arquivo e sem desenho.
- `kernel/ui/tcc_prompt.c` — leitura, vidro, botões e gravação.
- `kernel/ui/desktop.c` — o loop chama o tick, o desenho e o mouse/teclado. O resto do desktop não mudou.
- `tools/tccprompttest.c` — pending, diálogo, `tcc_decide`, estado relido.

O desenho não foi executado neste ambiente: não há Zig nem QEMU aqui. O que o teste cobre é a política e o arquivo.
