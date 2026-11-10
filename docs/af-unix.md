# AF_UNIX entre processos

Syscall `SYS_UNIX` (61). `rdi` é a operação (`MOS_UNIX_*` em `kernel/include/syscall.h`). O fd devolvido é do processo (`FD_UNIX`). `read`/`write` usam `SYS_READ`/`SYS_WRITE`. `fstat`: `is_dir` 6, `size` = bytes prontos (ou conexões na fila), `mtime` 1 se o próximo `read` for EOF. `stat` do caminho continua arquivo comum. `open` do caminho devolve `ENXIO`.

## ABI para consumidores

Chamadas: `SOCKET` `BIND` `LISTEN` `ACCEPT` `CONNECT` `SHUTDOWN` `SOCKNAME` `PEERNAME` `PAIR` `SENDMSG` `RECVMSG` `GETSOCKOPT`. Domínio 1, protocolo 0. Tipo 1 = stream, 2 = dgram. Bits de não-bloqueio: `0x800`, `0x10000000`, `0x40000000`. `CLOEXEC`: `0x80000`, `0x20000000`, `0x80000000`.

Caminho: `sockaddr_un` do Darwin (`sun_len`, `sun_family` 1, `sun_path` de no máximo 103 caracteres e um zero). Relativo vira absoluto com a pasta atual. Sem nome abstrato. A pasta pai tem de existir (`ENOENT`). `bind` cria o arquivo; nome ocupado ou arquivo já no caminho = `EADDRINUSE`. `close` sem `unlink` deixa o arquivo: `connect`/`sendto` = `ECONNREFUSED`. `unlink` tira o nome: aí é `ENOENT`. `rename` acompanha o nome.

`socketpair` (`PAIR`) devolve duas pontas já ligadas, herdadas no `fork`. Outro domínio continua nos pipes antigos.

`SCM_RIGHTS` (`cmsg_level` `0xffff`, `cmsg_type` `0x01`): até 16 fds por `sendmsg`. Chegam só no `recvmsg`. `MSG_PEEK` não entrega e não consome. `read` descarta recado só de descritores. Um `recv` de stream não junta dois `send`.

`getsockopt` (`mos_unix_sockopt_t`): `SOL_LOCAL` (0) ou `SOL_SOCKET` (`0xffff`). `LOCAL_PEERCRED` (76 bytes, uid 501, gid 20), `LOCAL_PEERPID` e `LOCAL_PEEREPID`. No dgram sem `connect`, o pid é o de quem enviou o último datagrama. `recvfrom` devolve o caminho do `bind` do remetente.

AX (`axtransport.h`, não editado aqui): cada processo faz `bind` em `/tmp/mos-ax/<pid>` com `SOCK_DGRAM`. `send(pid)` = `sendto` nesse caminho. `listen` = `recvfrom` + `LOCAL_PEERPID`. Uma mensagem AX cabe em 640 bytes.

Limites: 128 sockets, 64 nomes, backlog 32, stream 16 KiB, datagrama 8192 e fila de 32, 16 direitos, controle até 512 bytes, 8 iovecs. Uma espera bloqueada por sentido; a segunda recebe `EAGAIN`. Fila de `accept` cheia e não-bloqueante: `EAGAIN` (não há `EINPROGRESS`). `POLLOUT` aparece sempre pronto; buffer cheio bloqueia ou devolve `EAGAIN` na escrita.

Stub: `SO_ERROR` fica 0. `SO_SNDBUF`/`SO_RCVBUF` só informam o teto. `setsockopt` não altera nada. `getpeereid` do libSystem continua 501/20 para qualquer fd. `MSG_WAITALL` não junta leituras. O tradutor ARM invalida os iovecs que cabem no retorno (até 8), o endereço e o controle.
