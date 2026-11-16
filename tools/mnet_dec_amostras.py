#!/usr/bin/env python3
"""Gera as amostras comprimidas do teste de host do mnet (tools/mnet_host_test.c) a partir de /tmp/mnet_dec_src.
gzip, zlib e deflate cru pelo zlib do Python; br pela libbrotlienc do host (ctypes)."""
import ctypes, gzip, zlib
d = open('/tmp/mnet_dec_src', 'rb').read()
g = gzip.compress(d)
open('/tmp/mnet_dec.gz', 'wb').write(g)
open('/tmp/mnet_dec2.gz', 'wb').write(g + g)
open('/tmp/mnet_dec.zlib', 'wb').write(zlib.compress(d))
c = zlib.compressobj(6, zlib.DEFLATED, -15)
open('/tmp/mnet_dec.raw', 'wb').write(c.compress(d) + c.flush())
L = ctypes.CDLL('libbrotlienc.so.1')
n = ctypes.c_size_t(len(d) + 1024)
o = ctypes.create_string_buffer(n.value)
assert L.BrotliEncoderCompress(9, 22, 0, ctypes.c_size_t(len(d)), d, ctypes.byref(n), o)
open('/tmp/mnet_dec.br', 'wb').write(o.raw[:n.value])
