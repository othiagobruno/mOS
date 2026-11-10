# Interface — I23 (notificações) e I19 (selo do Dock)

Medido neste branch. O kernel não foi ligado nem iniciado. Os testes abaixo rodam no host, no mesmo modelo que a barra de status.

## I23 — UNUserNotificationCenter

Funciona no modelo (`userland/libsystem/un_center.c`) e nos métodos (`userland/libsystem/usernotifications.c`):

- `addNotificationRequest:withCompletionHandler:` com conteúdo (título, subtítulo, corpo) e gatilho imediato (`trigger` nil ou intervalo ≤ 0) ou `UNTimeIntervalNotificationTrigger`.
- `removePendingNotificationRequestsWithIdentifiers:`, `removeAllPendingNotificationRequests`, `removeDeliveredNotificationsWithIdentifiers:`, `removeAllDeliveredNotifications`.
- `setDelegate:`. Sem delegate, ou sem `willPresent`, a apresentação é banner e lista. `willPresent` pode recusar o banner. A opção antiga de alerta também abre banner.
- O clique no banner chama `didReceive` com `com.apple.UNNotificationDefaultActionIdentifier`.
- Autorização continua no TCC, serviço `userNotifications` (`cx_un_decide`). Sem autorização o pedido não abre banner e o completion recebe um `NSError`.

O banner do desktop (`kernel/ui/desktop.c`, `notify_tagged`) recebe o aviso por `SYS_DESKTOP` / `MOS_DESK_BANNER` (número 63). O clique na área de descanso do cartão entra na fila (`deskbridge.c`) e o laço do app busca com `MOS_DESK_TAKE`.

Stub ou limite:

- Não há APNs nem `UNNotificationSettings`. `userInfo` devolve um dicionário vazio. A descrição do `NSError` é o texto genérico já existente ("Erro").
- Não Perturbe (`g_notify_dnd`) não publica o cartão. O som do kernel toca em todo aviso publicado; o bit de som da apresentação não é separado.
- Há 24 pedidos. Um pedido que se repete ocupa o mesmo lugar: continua pendente e entregue, e cada disparo chama `willPresent` de novo. Não é um histórico.
- O completion do delegate é síncrono. Se o app não chamar o bloco, o padrão é banner e lista.
- O retângulo do clique é o cartão parado. Nos primeiros 300 ms e nos últimos 300 ms o cartão desliza e o clique pode não coincidir com o desenho.
- O diálogo do TCC (I20) não é desta frente.

## I19 — NSDockTile

Funciona no app (`userland/libsystem/appkit_docktile.c`, cabeçalho em `userland/sdk/include/AppKit/AppKit.h`):

- `setBadgeLabel:`, `badgeLabel`, `contentView`, `display`, `showsApplicationBadge` (nasce ligado), `size` (128×128) e `owner`.
- `-[NSApplication dockTile]`.
- `setBadgeLabel:` publica o texto por `MOS_DESK_BADGE`. O kernel grava no app proxy e, se existir, no atalho de `/Aplicativos` (`compat_gui_set_badge_label`). Número positivo vira o selo numérico (teto 999). Texto vira selo com esse texto (até 7 caracteres). Vazio ou zero tira o selo. `showsApplicationBadge` NO esconde o selo e guarda o texto.
- A classe `NSDockTile` é exportada. Seletor que o tile não tem cai no método ausente do runtime e não aborta. `cx_docktile_load_path` lê `NSPrincipalClass` e `CFBundleExecutable` do `Info.plist`, faz `dlopen` e chama `setDockTile:` se a classe responder. Caminho vazio, arquivo ausente ou `dlopen` falho devolvem 0.

K19 não está em `main` nem neste checkout. Não há syscall nem desenhador que pinte um `NSView` dentro do ícone do Dock. `contentView` fica guardado no tile; `display` só republica o selo. Isso é o que falta do lado do kernel para o tile completo.

O binário `CodexDockTilePlugin` do ChatGPT não foi baixado nem executado, e não houve login. O teste de host chama os mesmos seletores que o plugin precisa, mais um seletor desconhecido, e o carregador de um caminho ausente devolve 0.

## Testes de host

```
gcc -std=c11 -Wall -Wextra -DCX_UN_HOST -Iuserland/libsystem \
    userland/libsystem/un_center.c userland/tests-appkit/un_host.c \
    -o /tmp/un-host && /tmp/un-host

gcc -std=c11 -Wall -Wextra -DCX_DOCK_HOST -Iuserland/libsystem -Ikernel/ui \
    userland/libsystem/appkit_docktile.c userland/tests-appkit/docktile_host.c \
    -o /tmp/docktile-host && /tmp/docktile-host
```

No host, os dois comandos passaram: `un-host: 35/35 ok` e `docktile-host: 37/37 ok`. O banner na tela e o selo no Dock não foram vistos: o kernel não foi compilado nem iniciado.
