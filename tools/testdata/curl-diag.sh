# mOS — diagnóstico do curl do Homebrew: DNS, HTTP sem TLS e HTTPS, cada um com limite de tempo.
d() { local nome=$1; shift; echo "diag: == $nome"; timeout -s KILL 40 "$@" > /tmp-diag.txt 2>&1; local rc=$?
      while IFS= read -r l; do echo "diag:   $l"; done < /tmp-diag.txt; echo "diag: rc=$rc"; }
d "dns pelo ruby"      ruby -rsocket -e 'p Addrinfo.getaddrinfo("example.com", 80, nil, :STREAM).map(&:ip_address)'
d "tcp pelo ruby"      ruby -rsocket -e 's = TCPSocket.new("example.com", 80); s.write("HEAD / HTTP/1.0\r\nHost: example.com\r\n\r\n"); puts s.readpartial(200)'
d "curl --resolve"     curl -m 30 -sv --resolve example.com:80:172.66.147.243 -o /dev/null http://example.com/
d "curl http"          curl -m 30 -sv -o /dev/null http://example.com/
d "curl https"         curl -m 30 -sv -o /dev/null https://example.com/
d "https brew API"     curl -m 30 -fsS -o /tmp-hello.json https://formulae.brew.sh/api/formula/hello.json
d "ver json"           head -c 120 /tmp-hello.json
echo "diag: fim"
