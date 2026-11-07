# mOS — diagnóstico do curl do Homebrew: DNS, HTTP sem TLS e HTTPS, cada um com limite de tempo.
d() { local nome=$1; shift; echo "diag: == $nome"; timeout 40 "$@" > /tmp-diag.txt 2>&1; local rc=$?
      while IFS= read -r l; do echo "diag:   $l"; done < /tmp-diag.txt; echo "diag: rc=$rc"; }
d "tar czf + /bin/sh"  tar czf /tmp-d.tgz /usr/local/bash-teste.sh
d "tar tzf"            tar tzf /tmp-d.tgz
d "http sem TLS"       curl -sv -o /dev/null http://example.com/
d "https example.com"  curl -sv -o /dev/null https://example.com/
d "https brew API"     curl -fsS -o /tmp-hello.json https://formulae.brew.sh/api/formula/hello.json
d "ver json"           head -c 120 /tmp-hello.json
echo "diag: fim"
