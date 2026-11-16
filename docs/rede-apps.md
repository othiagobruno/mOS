# Rede de alto nível para apps (frente da Patricia)

Dona: Patricia (`patricia/*`). Escopo: CFNetwork e NSURLSession completos (HTTP/1.1, HTTP/2, WebSocket,
cookies, cache, proxy, background), Network.framework (C `nw_*` e o módulo Swift `Network`), libcurl, TLS 1.3
completo, Bonjour/mDNS (`dns_sd.h`) e multicast UDP.

Fora do escopo (outros donos): drivers de placa (Felipe), `NSURL`/`NSURLComponents`/`NSData` (Eduardo),
runtime Swift (Fernanda), Keychain/SecTrust além do que o TLS precisa (Natalia).

## Levantamento (main `97c611b`, 15 nov. 2026)

| Peça | Onde estava | Estado encontrado |
|---|---|---|
| Motor TLS | `kernel/net/tls.c` + `crypto.c` + `x509.c`, syscalls `MOS_NET_TLS_*` | Cliente TLS 1.3/1.2, ALPN fixo `http/1.1`, sem servidor, sem retomada (ticket ignorado), sem certificado de cliente, sem ALPN negociável |
| SecureTransport | `userland/libsystem/securetransport.c` | Cliente sobre o motor do kernel; `SSLSetALPNProtocols` ignorado; lado servidor devolve NULL |
| NSURLSession | `userland/libsystem/foundation_more.c` (`http_do`) | Um pedido `Connection: close`, lê até o fim; **sem chunked**, sem redirect, sem keep-alive, sem cookies, sem cache, sem proxy, sem gzip, sem HTTP/2, sem WebSocket, configuração única compartilhada |
| libcurl | `userland/libsystem/libcurl_easy.c` | easy + URL parser, HTTP/1.1 com chunked e redirect; sem multi, sem share, sem cookies, sem HTTP/2, sem proxy |
| CFStream | `cfstream.c` | Só arquivo/memória sobre `NSInputStream`; par de socket em `wa_net.c` |
| CFHost / reachability | `cfhost.c`, `wa_net.c` | Resolução real; reachability com estado fixo |
| Network.framework C | `sys_zerados.c` | 25 `nw_*` vazios |
| Network Swift | `sys_zerados.c` | 154 símbolos vazios (`NWConnection`, `NWListener`, `NWBrowser`, `NWPathMonitor`...) |
| Bonjour | — | `DNSService*` inexistente |
| Multicast UDP | `kernel/net/userudp.c`, `sockets_udp.inc` | Saída passa; `IP_ADD_MEMBERSHIP` aceito e ignorado; entrada depende da placa; sem IGMP |
| Proxy | `sys_zerados.c` | `kCFNetworkProxies*` são ponteiros nulos; sem `CFNetworkCopySystemProxySettings` real, PAC devolve nulo |

## Arquitetura

Um núcleo C portátil, `mnet` (`userland/libsystem/mnet_*.c` + `mnet.h`), sem dependência de ObjC, compilado
igual no libSystem (freestanding) e no host (gcc) para os testes. Todas as APIs públicas (NSURLSession, CFNetwork,
libcurl, `nw_*`, `dns_sd`) são cascas finas sobre ele.

```mermaid
flowchart TD
  NSURLSession --> core[mnet: pedido/resposta, pool, redirect, auth]
  CFNetwork[CFHTTPMessage / CFReadStream HTTP] --> core
  libcurl --> core
  core --> h1[HTTP/1.1]
  core --> h2[HTTP/2 + HPACK]
  core --> ws[WebSocket]
  core --> jar[cookies RFC 6265]
  core --> cache[cache RFC 9111]
  core --> proxy[proxy HTTP/HTTPS/SOCKS5 + PAC]
  h1 --> tr[transporte: TCP + TLS]
  h2 --> tr
  ws --> tr
  nw[Network.framework nw_* e Swift] --> tr
  tr --> tls[motor TLS do kernel, ALPN h2/http1.1]
  dnssd[dns_sd / NWBrowser] --> mdns[mDNS/DNS-SD multicast 224.0.0.251:5353]
```

## Plano (ordem de entrega, um PR por etapa)

1. **Núcleo HTTP portátil** — parser incremental de resposta HTTP/1.1 (status, cabeçalhos dobrados, chunked com
   trailers, Content-Length, fim por fechamento, 1xx/100-continue), serialização de pedido, cookies RFC 6265
   (Set-Cookie, domínio/caminho, Expires/Max-Age, Secure, HttpOnly, SameSite, sufixo público básico, arquivo
   persistente), HPACK RFC 7541 (tabelas estática e dinâmica, Huffman). Testes de host.
2. **TLS 1.3 completo no motor** — ALPN configurável e lido de volta (`h2`, `http/1.1`), retomada por ticket
   (PSK, `psk_dhe_ke`), certificado de cliente, lado servidor (TLS 1.3, para `NWListener`/SecureTransport servidor),
   novas syscalls `MOS_NET_TLS_CONFIG`/`MOS_NET_TLS_INFO`; SecureTransport com ALPN real.
3. **HTTP/2** — quadros, fluxos, controle de fluxo, SETTINGS/PING/GOAWAY/RST, multiplexação no pool.
4. **NSURLSession novo** (sai de `foundation_more.c`, combinado com o Eduardo) — configurações reais
   (default/ephemeral/background), fila de delegate, data/upload/download/stream/WebSocket tasks, redirect com
   delegate, autenticação (Basic, Digest, servidor, cliente), cookies (`NSHTTPCookieStorage`/`NSHTTPCookie`),
   cache (`NSURLCache`), métricas, progresso, cancel/suspend/resume com resumeData, gzip/deflate/br, async Swift.
5. **WebSocket** — RFC 6455 (handshake, máscara, fragmentação, ping/pong, close), `permessage-deflate`,
   `NSURLSessionWebSocketTask`, `nw_ws_*`.
6. **Proxy** — `CFNetworkCopySystemProxySettings`, `kCFNetworkProxies*` reais, CONNECT, SOCKS5, PAC.
7. **Network.framework** — `nw_*` C completo (connection, listener, browser, path monitor, parameters,
   protocol options TCP/UDP/TLS/WS, framer) sobre o transporte; depois o módulo Swift (154) com a Fernanda.
8. **libcurl** — easy/multi/share sobre o núcleo (HTTP/2, cookies, proxy).
9. **CFNetwork restante** — `CFHTTPMessage`, `CFReadStreamCreateForHTTPRequest`, par de socket com TLS
   (`kCFStreamPropertySSLSettings`), `CFHost` assíncrono.
10. **Multicast e Bonjour** — `IP_ADD_MEMBERSHIP`/IGMPv2 no kernel (filtro da placa com o Felipe), mDNS
    (RFC 6762) e DNS-SD (RFC 6763): `DNSServiceRegister/Browse/Resolve/GetAddrInfo/QueryRecord`, `NSNetService`,
    `NWBrowser`.

## Estado

| Etapa | Estado | Teste |
|---|---|---|
| 1. Núcleo HTTP portátil | pronto (PR desta etapa): `mnet_util.c`, `mnet_h1.c`, `mnet_cookie.c`, `mnet_hpack.c` | `tools/mnet_host_test.c`: 638/638 no gcc com ASan/UBSan; os 12 blocos do Apêndice C da RFC 7541 decodificam e o codificador gera os mesmos bytes; `zig cc` freestanding x86_64 e arm64 compila sem aviso |
| 2–10 | não iniciado | — |

### Etapa 1 — o que está no código

- **HTTP/1.1** (`mnet_h1.c`): parser incremental em qualquer fatiamento (testado de 1 em 1 byte), 1xx descartado com
  evento, 101 marca upgrade, HEAD/204/304 sem corpo, chunked com extensões e trailers, Content-Length repetido igual
  aceito e diferente recusado, fim por fechamento, linha dobrada (obs-fold), CR solto e nome com espaço recusados,
  limite de 256 KiB na cabeça, keep-alive por versão e `Connection`, bytes da próxima resposta devolvidos (pipeline).
  O pedido corta CR/LF dentro de campos (sem injeção) e não manda `Content-Length: 0` em GET/HEAD.
- **Cookies** (`mnet_cookie.c`): RFC 6265 com as regras de 6265bis (SameSite, `__Secure-`/`__Host-`, Secure só de
  origem segura e não sobrescrito por origem insegura, CTL recusa o cookie, nome vazio). Public Suffix List inteira
  embutida (`mnet_psl.inc`, 10119 regras, MPL-2.0, IDN em punycode, curinga e exceção). Limites 3000 no total e 180
  por domínio, com despejo do menos usado. Arquivo Netscape do curl com `#HttpOnly_`.
- **Datas**: o algoritmo do RFC 6265 §5.1.1 (IMF-fixdate, RFC 850, asctime e variações) e a formatação IMF.
- **HPACK** (`mnet_hpack.c`): tabelas geradas do texto da RFC (`mnet_hpack_tab.inc`), Huffman canônico com checagem
  de EOS e de enchimento, tabela dinâmica em anel, atualização de tamanho (menor e final, §4.2), campos sensíveis como
  "nunca indexar".

Ainda não ligado a nenhuma API pública: isso entra nas etapas 2–4.
