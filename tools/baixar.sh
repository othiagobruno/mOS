# mOS — download com SHA-256 conferido, para os scripts de build em sh/bash.
# Uso (depois de ". tools/baixar.sh"):
#   baixar URL DESTINO SHA256
# Se DESTINO já existe com o hash certo, não baixa (cache). Senão baixa para DESTINO.partial,
# confere o hash e só então renomeia. Hash errado: apaga o .partial e sai com erro.
# O transporte é o primeiro que existir: curl, wget, python3 (urllib). O Linux de build pode
# não ter curl.

# imprime o SHA-256 de um arquivo (sha256sum, shasum ou python3)
sha256_de() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    else
        python3 -I -c 'import hashlib,sys
h = hashlib.sha256()
with open(sys.argv[1], "rb") as f:
    for b in iter(lambda: f.read(1 << 20), b""):
        h.update(b)
print(h.hexdigest())' "$1"
    fi
}

# baixa URL em ARQ (sem conferir hash); 3 tentativas, segue redirecionamento, falha em HTTP >= 400
baixar_url() {
    _u=$1
    _a=$2
    if command -v curl >/dev/null 2>&1; then
        curl -fL --retry 3 -o "$_a" "$_u"
    elif command -v wget >/dev/null 2>&1; then
        wget -q --tries=3 -O "$_a" "$_u"
    elif command -v python3 >/dev/null 2>&1; then
        python3 -I -c 'import shutil,sys,time,urllib.request
u, a = sys.argv[1], sys.argv[2]
for t in range(3):
    try:
        req = urllib.request.Request(u, headers={"User-Agent": "mOS-build"})
        with urllib.request.urlopen(req, timeout=60) as r, open(a, "wb") as f:
            shutil.copyfileobj(r, f, 1 << 20)
        sys.exit(0)
    except Exception as e:
        print("baixar: %s (tentativa %d): %s" % (u, t + 1, e), file=sys.stderr)
        time.sleep(2)
sys.exit(1)' "$_u" "$_a"
    else
        echo "baixar: sem curl, wget nem python3" >&2
        return 1
    fi
}

baixar() {
    _url=$1
    _dest=$2
    _sha=$3
    if [ -f "$_dest" ] && [ "$(sha256_de "$_dest")" = "$_sha" ]; then
        return 0
    fi
    mkdir -p "$(dirname "$_dest")"
    rm -f "$_dest.partial"
    echo "baixando $(basename "$_dest")" >&2
    if ! baixar_url "$_url" "$_dest.partial"; then
        rm -f "$_dest.partial"
        echo "baixar: falhou $_url" >&2
        return 1
    fi
    _got=$(sha256_de "$_dest.partial")
    if [ "$_got" != "$_sha" ]; then
        rm -f "$_dest.partial"
        echo "baixar: SHA-256 de $(basename "$_dest") é $_got, esperado $_sha" >&2
        return 1
    fi
    mv -f "$_dest.partial" "$_dest"
}
