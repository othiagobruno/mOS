#!/bin/sh
# Build parcial no Linux: curl 8.23.0 + mbed TLS 3.6.4, verificação ligada.
# Não é o build.ps1 da imagem e não é a VM.
set -e
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
work=${TMPDIR:-/tmp}/mos-curl-tls
mkdir -p "$work"
cd "$work"
if [ ! -f mbedtls-3.6.4/library/libmbedtls.a ]; then
  if [ ! -f mbedtls-3.6.4.tar.bz2 ]; then
    curl -fsSL -o mbedtls-3.6.4.tar.bz2 \
      https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.4/mbedtls-3.6.4.tar.bz2
  fi
  tar -xjf mbedtls-3.6.4.tar.bz2
  make -C mbedtls-3.6.4 -j"$(nproc)" lib
fi
mkdir -p prefix/lib prefix/include
cp -a mbedtls-3.6.4/include/mbedtls mbedtls-3.6.4/include/psa prefix/include/
cp mbedtls-3.6.4/library/libmbed*.a prefix/lib/
if [ ! -x curl-8.23.0/src/curl ]; then
  if [ ! -f curl-8.23.0.tar.gz ]; then
    curl -fsSL -o curl-8.23.0.tar.gz https://curl.se/download/curl-8.23.0.tar.gz
  fi
  tar -xzf curl-8.23.0.tar.gz
  cd curl-8.23.0
  ./configure --with-mbedtls="$work/prefix" --without-libpsl --disable-ldap --disable-ldaps \
    --without-gnutls --without-wolfssl --disable-manual >/tmp/mos-curl-configure.log
  make -j"$(nproc)" >/tmp/mos-curl-make.log
  cd "$work"
fi
sh "$root/tools/tls_local_certs.sh" "$work/certs"
openssl s_server -accept 127.0.0.1:18447 -cert "$work/certs/srv.pem" -key "$work/certs/srv.key" -www \
  >/tmp/mos-curl-s_server.log 2>&1 &
pid=$!
trap 'kill $pid 2>/dev/null || true' EXIT
i=0
while [ "$i" -lt 50 ]; do
  if python3 -c 'import socket; socket.create_connection(("127.0.0.1",18447),1).close()' 2>/dev/null; then
    break
  fi
  i=$((i + 1))
  sleep 0.05
done
echo "curl: $(./curl-8.23.0/src/curl --version | head -1)"
./curl-8.23.0/src/curl -fsS --cacert "$work/certs/ca.pem" --connect-timeout 5 https://localhost:18447/ -o "$work/ok.html"
echo "ok: handshake com a AC de teste"
if ./curl-8.23.0/src/curl -fsS --connect-timeout 5 https://localhost:18447/ -o /dev/null; then
  echo "FALHA: aceitou certificado sem a AC"
  exit 1
fi
echo "ok: sem a AC de teste o certificado é recusado"
echo "curl-tls-host: ok"
