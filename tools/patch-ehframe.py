#!/usr/bin/env python3
# Mesma gravação de tools/patch-ehframe.ps1: cx_ls_ehframe[2..3] e aliases unw_* globais.
import sys


def main(path):
    b = bytearray(open(path, "rb").read())
    def u16(o):
        return int.from_bytes(b[o:o + 2], "little")
    def u32(o):
        return int.from_bytes(b[o:o + 4], "little")
    def u64(o):
        return int.from_bytes(b[o:o + 8], "little")
    shoff = u64(40)
    shentsize = u16(58)
    shnum = u16(60)
    shstr = u16(62)
    str_off = u64(shoff + shstr * shentsize + 24)
    eh_addr = eh_size = 0
    for i in range(shnum):
        sh = shoff + i * shentsize
        no = str_off + u32(sh)
        e = no
        while b[e]:
            e += 1
        if bytes(b[no:e]) == b".eh_frame":
            eh_addr = u64(sh + 16)
            eh_size = u64(sh + 32)
    magic = (0x4D4F5345485F4652).to_bytes(8, "little") + (0x4152344D34365F41).to_bytes(8, "little")
    hit = -1
    k = 0
    while k <= len(b) - 32:
        if b[k:k + 16] == magic:
            hit = k
            break
        k += 8
    if hit < 0 or not eh_size:
        sys.stderr.write("%s: .eh_frame ou marcador cx_ls_ehframe nao encontrado\n" % path)
        return 1
    b[hit + 16:hit + 24] = eh_addr.to_bytes(8, "little")
    b[hit + 24:hit + 32] = eh_size.to_bytes(8, "little")
    nprom = 0
    for i in range(shnum):
        sh = shoff + i * shentsize
        if u32(sh + 4) != 2:
            continue
        off = u64(sh + 24)
        size = u64(sh + 32)
        ent = u64(sh + 56)
        stroff = u64(shoff + u32(sh + 40) * shentsize + 24)
        k = 1
        while k < size // ent:
            s = off + k * ent
            if (b[s + 4] >> 4) != 2 or u16(s + 6) == 0:
                k += 1
                continue
            no = stroff + u32(s)
            e = no
            while b[e]:
                e += 1
            nm = bytes(b[no:e])
            if nm.startswith((b"unw_", b"_Unwind_", b"__unw_")):
                b[s + 4] = (1 << 4) | (b[s + 4] & 15)
                nprom += 1
            k += 1
    open(path, "wb").write(b)
    print("eh_frame: +0x%x (%d bytes), %d alias(es) global(is)" % (eh_addr, eh_size, nprom))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
