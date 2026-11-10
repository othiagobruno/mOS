# IPv6 no mOS

Estado em 7 nov. 2026. O código está em `kernel/net/ipv6.c` (IPv6, NDP, SLAAC, ICMPv6, UDP sobre IPv6, laço local), `kernel/net/tcp.c` (TCP com endereços de 16 bytes e abertura passiva), `kernel/net/dns.c` (AAAA), `kernel/net/usersock.c` (syscalls) e `userland/libsystem/sockets_inet.inc` (`AF_INET6` dos apps).

## Endereços

Toda a pilha guarda endereços de 16 bytes em ordem de rede. O IPv4 entra como **IPv4 mapeado** (`::ffff:a.b.c.d`, RFC 4291 2.5.5.2). O TCP e os sockets escolhem a família pelo endereço (`ip6_is_v4mapped`). Assim um mesmo `tcb` serve aos dois protocolos, e o socket `AF_INET6` dual-stack sai de graça.

## Kernel

| Parte | O que faz |
|---|---|
| Interface | Ao ativar a placa ou quando o link volta, `ipv6_start()` gera o link-local `fe80::` + EUI-64 do MAC e manda RS para `ff02::2` (até 4 vezes, a cada 4 s). |
| NDP (RFC 4861) | Cache de vizinhos com 16 entradas e fila de 8 pacotes à espera de resolução. NS vai para o grupo de nó solicitado. Responde NS para os nossos endereços com NA (S/O), aprende com NA e com a opção SLLA de NS e RA. |
| RA / SLAAC (RFC 4862) | O primeiro prefixo /64 com A=1 vira `prefixo + EUI-64`. O roteador padrão é a origem do RA e vale pelo tempo de vida do roteador. O RDNSS (RFC 8106) define o DNS v6. O DAD é otimista: o endereço vale na hora. |
| ICMPv6 | Responde eco (128 → 129). As respostas de eco dos apps (`MOS_NET_ICMP_*`) entram num anel de 16 por identificador, e o mesmo anel recebe as respostas do ICMPv4. |
| IPv6 | Cabeçalhos de extensão salto a salto, opções de destino e roteamento sem segmentos são pulados. Fragmentos são descartados. Hop limit 64 (255 no NDP). |
| UDP sobre IPv6 | `udp6_send` e entrada com soma obrigatória. A entrega usa a mesma tabela de portas do IPv4 (`udp_deliver`). |
| DNS | Cache por (nome, tipo): A (1) e AAAA (28). O servidor é o do DHCP. Numa rede só IPv6, é o do RA, por UDP sobre IPv6. `localhost` vira `::1` e literais voltam direto. |
| TCP | `tcp_connect6`, `tcp_listen`/`tcp_accept` (SYN → SYN_RCVD → SYN+ACK com retransmissão → ESTAB → fila do accept, com backlog). Os ouvintes têm ids a partir de `TCP_LISTEN_BASE`. Num ouvinte, `TCP_L_V4`/`TCP_L_V6` dizem quais famílias ele aceita. MSS 1440 no IPv6. |
| Laço local | Pacotes para `127.x`, `::1` ou para o próprio endereço não vão à placa: entram numa fila de 96 e voltam à entrada por `net_loop_drain`. Essa função roda no `net_poll` e em toda syscall de rede dos apps, junto de um `tcp_tick` para os ACKs, então o TCP local não espera o laço da área de trabalho. Um `connect` local sem ouvinte devolve `ECONNREFUSED` na hora, como antes. |
| Placa | O e1000 liga `RCTL_MPE` (todo o multicast) e o r8169 põe `0xFF` em MAR0..7, porque o mOS não fala MLD. O filtro é feito em software (`33:33:…` no Ethernet e grupos nossos no IPv6). |

Syscalls novas no fim do enum de `SYS_NET` (`kernel/include/syscall.h`): `MOS_NET_RESOLVE6`, `CONNECT6`, `LISTEN`, `ACCEPT`, `ADDRS`, `INFO6`, `ICMP_SEND`, `ICMP_RECV`. O `MOS_NET_CONNECT` antigo (IPv4) passa pelo mesmo caminho.

## libSystem

- `socket(AF_INET6, SOCK_STREAM)` funciona, e `AF_INET` também ganhou servidor. `bind`/`listen`/`accept` funcionam, com porta 0 efêmera (`getsockname` devolve a porta escolhida). O `accept` herda o `O_NONBLOCK` e o poll/kqueue acorda no ouvinte quando há conexão pronta.
- O socket é dual-stack como no macOS (`net.inet6.ip6.v6only = 0`): um ouvinte em `::` aceita IPv4, e o par aparece como `::ffff:a.b.c.d`. `IPV6_V6ONLY` (nível 41, opção 27) desliga isso.
- `getsockname`/`getpeername`/`recvfrom`/`accept` devolvem `sockaddr_in` ou `sockaddr_in6` (28 bytes) conforme a família do socket.
- Ping como no Darwin, sem root: `socket(AF_INET6, SOCK_DGRAM, IPPROTO_ICMPV6)` e `socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP)`. O app monta o eco e o kernel soma. No IPv4 o `recvfrom` entrega o cabeçalho IP antes do ICMP, como o Darwin.
- `getaddrinfo`:
  - `AF_UNSPEC`: IPv4 primeiro, depois IPv6. O AAAA só é pedido quando há endereço IPv6 global **e** roteador padrão. É o `AI_ADDRCONFIG`, que no macOS vem por padrão sem dicas.
  - `AF_INET6`: AAAA. Com `AI_V4MAPPED`, devolve os A mapeados quando não há AAAA, e sempre com `AI_ALL`.
  - Literais `[::1]` e `fe80::1%en0` são aceitos (os colchetes e a zona são tirados).
  - `getnameinfo` e `inet_ntop` formatam IPv6 (o mapeado sai `::ffff:10.0.2.15`).
- Os apps Chromium (Chrome, Slack, VS Code…) não mudam de caminho. O teste de IPv6 do Chromium abre um socket **UDP** `AF_INET6` para `2001:4860:4860::8888`; o mOS ainda não tem UDP para os apps, então o teste falha e o Chromium pede só registros A, como antes. E mesmo para quem pede `AF_UNSPEC`, o A vem antes do AAAA: um AAAA inalcançável (por exemplo o user-net do QEMU num host sem IPv6 de saída) nunca é tentado primeiro.

## Teste

`userland/tests-net6/main.c` (compila igual no macOS):

```
.\build.ps1 -OutDir build-net6
.\tools\build-nettest6.ps1 -OutDir build-net6          # [-Nome example.com] [-Alvo fec0::2] [-Porta 8086]
powershell -File tools\testdata\http6-servidor.ps1     # HTTP em [::1]:8086 no host
.\run.ps1 -Headless -Image build-net6/mOS.img -Serial build-net6/serial.log -Port 4466
```

O teste cobre:
- TCP no laço local: `[::1]`, `127.0.0.1` e dual-stack com e sem `IPV6_V6ONLY`.
- `ECONNREFUSED` local.
- ping6 para `::1` e para o gateway.
- `getaddrinfo` com AAAA, `localhost` em `AF_INET6` e a ordem em `AF_UNSPEC`.
- GET HTTP por TCP sobre IPv6 em `[fec0::2]:8086`. O user-net do QEMU leva essa conexão ao `::1` do host.

O user-net do QEMU já traz IPv6 (`ipv6=on` é o padrão): prefixo `fec0::/64` por RA, gateway `fec0::2` e DNS `fec0::3`.

Resultado (7 nov. 2026, QEMU user-net):
- `net6test: 11/11 ok`. O SLAAC deu `fec0::5054:ff:fe12:3456/64` com roteador `fe80::2` e DNS `fec0::3`. O AAAA de `example.com` voltou (`2606:4700:10::…`), e o GET em `[fec0::2]:8086` respondeu `HTTP/1.0 200 OK` do servidor do host.
- O teste IPv4 antigo (`tools/build-nettest.ps1`) passou com 16/16.
- Com o IPv6 configurado na rede, o Chrome (`stage-chrome.ps1 -AutoAbrir`) abriu `https://example.com` e o Slack (`stage-electron.ps1 -Autostart`) mostrou a tela de entrada, nos dois casos sem erro de rede no log.

## Falta

- UDP para os apps (v4 e v6), o que também ligaria o teste de IPv6 do Chromium.
- DHCPv6, endereços temporários (RFC 4941), vários prefixos, MLD, DAD com espera, fragmentação e PMTU.
- Escopo/zona nos endereços link-local dos apps (o `sin6_scope_id` é ignorado; há uma interface só).
