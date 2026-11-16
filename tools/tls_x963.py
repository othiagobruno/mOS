# Converte uma chave EC do openssl (PEM) para o formato X9.63 do SecKeyCopyExternalRepresentation (04 || X || Y || K).
# python3 -I tools/tls_x963.py chave.pem saida.x963   (usado por tools/tls_local_test.c)
import re, subprocess, sys
t = subprocess.run(["openssl", "pkey", "-in", sys.argv[1], "-text", "-noout"], capture_output=True, text=True, check=True).stdout
def bloco(nome):
    m = re.search(nome + r":\s*\n((?:\s+[0-9a-f:]+\n)+)", t)
    return bytes.fromhex(re.sub(r"[\s:]", "", m.group(1)))
priv, pub = bloco("priv"), bloco("pub")
b = (len(pub) - 1) // 2
priv = priv[-b:].rjust(b, b"\0")
open(sys.argv[2], "wb").write(pub + priv)
