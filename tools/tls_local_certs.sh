#!/bin/sh
# Certificados efêmeros do teste de host. Não entram na imagem nem na lista do sistema.
set -e
d="${1:-/tmp/mos-tls-test}"
rm -rf "$d"
mkdir -p "$d"
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$d/ca.key" -out "$d/ca.pem" -days 30 \
  -subj /CN=mos-test-ca -addext basicConstraints=critical,CA:TRUE \
  -addext keyUsage=critical,keyCertSign,cRLSign >/dev/null 2>&1
openssl req -newkey rsa:2048 -nodes -keyout "$d/srv.key" -out "$d/srv.csr" -subj /CN=localhost >/dev/null 2>&1
printf '%s\n' 'basicConstraints=CA:FALSE' 'keyUsage=digitalSignature,keyEncipherment' \
  'extendedKeyUsage=serverAuth' 'subjectAltName=DNS:localhost' > "$d/ok.ext"
openssl x509 -req -in "$d/srv.csr" -CA "$d/ca.pem" -CAkey "$d/ca.key" -CAcreateserial \
  -out "$d/srv.pem" -days 30 -extfile "$d/ok.ext" >/dev/null 2>&1
printf '%s\n' 'basicConstraints=CA:FALSE' 'keyUsage=digitalSignature,keyEncipherment' \
  'extendedKeyUsage=serverAuth' 'subjectAltName=DNS:wrong.example' > "$d/badname.ext"
openssl x509 -req -in "$d/srv.csr" -CA "$d/ca.pem" -CAkey "$d/ca.key" -set_serial 2 \
  -out "$d/badname.pem" -days 30 -extfile "$d/badname.ext" >/dev/null 2>&1
printf '%s\n' 'basicConstraints=CA:FALSE' 'keyUsage=digitalSignature,keyEncipherment' \
  'extendedKeyUsage=serverAuth' 'subjectAltName=DNS:localhost' > "$d/exp.ext"
cat > "$d/ca.cnf" <<EOF
[ca]
default_ca = CA_default
[CA_default]
database = $d/index.txt
new_certs_dir = $d
certificate = $d/ca.pem
private_key = $d/ca.key
serial = $d/serial
default_md = sha256
policy = policy_loose
default_startdate = 20010101000000Z
default_enddate = 20010102000000Z
copy_extensions = copy
unique_subject = no
[policy_loose]
commonName = optional
countryName = optional
stateOrProvinceName = optional
organizationName = optional
organizationalUnitName = optional
EOF
touch "$d/index.txt"
echo 03 > "$d/serial"
openssl ca -batch -config "$d/ca.cnf" -in "$d/srv.csr" -out "$d/exp.pem" -extfile "$d/exp.ext" >/dev/null 2>&1
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$d/other.key" -out "$d/other.pem" -days 30 \
  -subj /CN=mos-other-ca -addext basicConstraints=critical,CA:TRUE \
  -addext keyUsage=critical,keyCertSign,cRLSign >/dev/null 2>&1
openssl x509 -req -in "$d/srv.csr" -CA "$d/other.pem" -CAkey "$d/other.key" -set_serial 4 \
  -out "$d/unknown.pem" -days 30 -extfile "$d/ok.ext" >/dev/null 2>&1
