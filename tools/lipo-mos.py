#!/usr/bin/env python3
"""Junta Mach-O finos de arquiteturas diferentes num binário universal (o "lipo -create").

Mesmo formato do tools/lipo-mos.ps1, para o build Linux (tools/build.sh): as bibliotecas que o mOS instala servem aos
processos x86_64 e aos arm64 do tradutor, e o carregador (kernel/compat/macho.c, pick_slice) escolhe a fatia da
arquitetura do processo. Alinhamento das fatias: 2^14 no arm64 (página de 16 KiB) e 2^12 no x86_64, como o lipo da Apple.

  tools/lipo-mos.py -o saida entrada1 entrada2 ...   # cria (entradas finas ou universais; fatias repetidas são erro)
  tools/lipo-mos.py --info arquivo                    # lista as fatias
  tools/lipo-mos.py --archs arquivo                   # só os nomes, separados por espaço
  tools/lipo-mos.py --thin arm64 -o saida arquivo      # extrai uma fatia
"""
import os
import struct
import sys

FAT_MAGIC, FAT_MAGIC_64 = 0xCAFEBABE, 0xCAFEBABF
MH_MAGIC_64 = 0xFEEDFACF
CPU_X86_64, CPU_ARM64 = 0x01000007, 0x0100000C
NAMES = {CPU_X86_64: "x86_64", CPU_ARM64: "arm64"}


def arch_name(cpu, sub=0):
    if cpu == CPU_ARM64 and (sub & 0xFF) == 2:
        return "arm64e"
    return NAMES.get(cpu, "cpu 0x%X" % cpu)


def slices(data):
    """[(cpu, sub, align, bytes)] de um Mach-O fino de 64 bits ou de um universal (32 ou 64 bits)."""
    if len(data) < 8:
        raise ValueError("curto demais para Mach-O")
    magic = struct.unpack_from(">I", data, 0)[0]
    if magic in (FAT_MAGIC, FAT_MAGIC_64):
        b64 = magic == FAT_MAGIC_64
        n = struct.unpack_from(">I", data, 4)[0]
        if n == 0 or n > 32:
            raise ValueError("universal com %d fatias" % n)
        out = []
        for i in range(n):
            if b64:
                cpu, sub, off, size, align = struct.unpack_from(">IIQQI", data, 8 + 32 * i)
            else:
                cpu, sub, off, size, align = struct.unpack_from(">IIIII", data, 8 + 20 * i)
            if off + size > len(data):
                raise ValueError("fatia %s fora do arquivo" % arch_name(cpu, sub))
            out.append((cpu, sub, align, data[off:off + size]))
        return out
    if struct.unpack_from("<I", data, 0)[0] == MH_MAGIC_64:
        cpu, sub = struct.unpack_from("<II", data, 4)
        return [(cpu, sub, 14 if cpu == CPU_ARM64 else 12, data)]
    raise ValueError("não é Mach-O de 64 bits")


def build(parts):
    """bytes do universal (FAT_MAGIC de 32 bits, como o lipo para arquivos < 4 GiB)."""
    seen = set()
    for cpu, sub, _, _ in parts:
        key = (cpu, sub & 0xFF if cpu == CPU_ARM64 else 0)
        if key in seen:
            raise ValueError("duas fatias %s" % arch_name(cpu, sub))
        seen.add(key)
    off = 8 + 20 * len(parts)
    lay = []
    for cpu, sub, _, b in parts:
        align = 14 if cpu == CPU_ARM64 else 12
        a = 1 << align
        off = (off + a - 1) // a * a
        lay.append((cpu, sub, align, off, b))
        off += len(b)
    if off >= 1 << 32:
        raise ValueError("universal passa de 4 GiB")
    buf = bytearray(struct.pack(">II", FAT_MAGIC, len(lay)))
    for cpu, sub, align, o, b in lay:
        buf += struct.pack(">IIIII", cpu, sub & 0xFFFFFFFF, o, len(b), align)
    for _, _, _, o, b in lay:
        buf += b"\0" * (o - len(buf))
        buf += b
    return bytes(buf)


def create(out, inputs):
    parts = []
    for p in inputs:
        with open(p, "rb") as f:
            parts += slices(f.read())
    data = build(parts)
    d = os.path.dirname(out)
    if d:
        os.makedirs(d, exist_ok=True)
    tmp = out + ".lipo-tmp"
    with open(tmp, "wb") as f:
        f.write(data)
    os.chmod(tmp, 0o755)
    os.replace(tmp, out)
    return [arch_name(c, s) for c, s, _, _ in parts]


def main(argv):
    out = thin = None
    mode = "create"
    files = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ("-o", "--out"):
            out = argv[i + 1]
            i += 2
            continue
        if a == "--thin":
            thin = argv[i + 1]
            mode = "thin"
            i += 2
            continue
        if a in ("--info", "--archs"):
            mode = a[2:]
        else:
            files.append(a)
        i += 1
    try:
        if mode in ("info", "archs"):
            if len(files) != 1:
                raise ValueError("--%s pede um arquivo" % mode)
            with open(files[0], "rb") as f:
                ss = slices(f.read())
            if mode == "archs":
                print(" ".join(arch_name(c, s) for c, s, _, _ in ss))
            else:
                for c, s, al, b in ss:
                    print("%-7s %d bytes (alinhado a 2^%d)" % (arch_name(c, s), len(b), al))
            return 0
        if mode == "thin":
            if not out or len(files) != 1:
                raise ValueError("uso: --thin ARCH -o saida arquivo")
            with open(files[0], "rb") as f:
                ss = [x for x in slices(f.read()) if arch_name(x[0], x[1]) == thin]
            if not ss:
                raise ValueError("%s não tem fatia %s" % (files[0], thin))
            with open(out, "wb") as f:
                f.write(ss[0][3])
            return 0
        if not out or not files:
            raise ValueError("uso: lipo-mos.py -o saida entrada1 entrada2 ...")
        print("%s: universal (%s)" % (out, " + ".join(create(out, files))))
        return 0
    except (OSError, ValueError, struct.error) as e:
        print("lipo-mos: %s" % e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
