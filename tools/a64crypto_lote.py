#!/usr/bin/env python3
"""Lote do a64fuzz (crypto anunciado) para tools/a64host --fuzz e o monitor QEMU.

Grupos: AES, SHA1/SHA256, SHA512/EOR3/BCAX/RAX1/XAR, PMULL 64. Sem SM3/SM4.
"""
import random, struct, sys
random.seed(0xA64C)

def word(mask, val):
    return (val | (random.getrandbits(32) & ~mask)) & 0xFFFFFFFF

# (nome, máscara, valor, filtro que deixa só a forma anunciada)
def keep_aes(w):
    return 4 <= ((w >> 12) & 31) <= 7
def keep_sha3(w):
    return ((w >> 12) & 7) <= 6 and ((w >> 15) & 1) == 0
def keep_sha2(w):
    return ((w >> 12) & 31) <= 2
def keep_c(w):
    op = (w >> 21) & 7
    if (w >> 24) & 0xFF != 0xCE:
        return False
    if op in (0, 1):
        return ((w >> 15) & 1) == 0
    if op == 3:
        return ((w >> 15) & 1) == 1 and ((w >> 14) & 1) == 0 and ((w >> 12) & 3) == 0
    if op == 4:
        return True
    if op == 6:
        return ((w >> 10) & 0x7FF) == 0x020
    return False
def keep_pmull(w):
    return ((w >> 22) & 3) == 3

GROUPS = [
    ("aes", 0xFF3E0C00, 0x4E280800, keep_aes),
    ("sha3", 0xFF208C00, 0x5E000000, keep_sha3),
    ("sha2", 0xFF3E0C00, 0x5E280800, keep_sha2),
    ("crypto4", 0xFF808000, 0xCE000000, keep_c),
    ("pmull", 0xBF20FC00, 0x0E20E000, keep_pmull),
]

def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "/tmp/crypto-anunciado.bin"
    qout = sys.argv[2] if len(sys.argv) > 2 else ""
    cases = bytearray()
    n = 0
    for name, mask, val, keep in GROUPS:
        got = 0
        while got < 32:
            w = word(mask, val)
            if not keep(w):
                continue
            st = bytearray(800)
            struct.pack_into("<Q", st, 0, random.getrandbits(64))
            struct.pack_into("<Q", st, 280, random.getrandbits(64))
            struct.pack_into("<Q", st, 288, random.getrandbits(64))
            struct.pack_into("<Q", st, 296, random.getrandbits(64))
            struct.pack_into("<Q", st, 304, random.getrandbits(64))
            cases += st
            cases += struct.pack("<QII", random.getrandbits(64) | 1, 1, 0)
            cases += struct.pack("<16I", w, *([0] * 15))
            got += 1
            n += 1
        print(name, got, file=sys.stderr)
    path = qout.encode()[:223]
    hdr = struct.pack("<QIIIIQ", 0x3130425A46343641, n, 0, 880, 832, 100000) + path + b"\x00" * (224 - len(path))
    open(out, "wb").write(hdr + cases)
    print(n, out)

if __name__ == "__main__":
    main()
