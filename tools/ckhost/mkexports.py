#!/usr/bin/env python3
"""Monta uma dylib Mach-O x86_64 só com o export trie.

O machocheck (tools/machocheck.c) trata como implementado o símbolo que aparece no
export trie de qualquer Mach-O da pasta varrida — a mesma regra de
docs/compat-apps.md para as dylibs Swift. Os nomes vêm do `nm` do CryptoKit.o
compilado de userland/frameworks/CryptoKit/CryptoKit.swift; esta dylib não é uma
segunda implementação e não substitui o binário do tools/build-frameworks-objc.ps1.
"""
import struct
import sys


def uleb(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def uleb_size(n):
    return len(uleb(n))


class Node:
    def __init__(self):
        self.children = {}
        self.terminal = False

    def insert(self, name):
        node = self
        for ch in name:
            node = node.children.setdefault(ch, Node())
        node.terminal = True


def build(names):
    root = Node()
    for name in names:
        if not name:
            raise SystemExit("nome vazio")
        root.insert(name)
    nodes = []

    def collect(node):
        node.idx = len(nodes)
        nodes.append(node)
        for ch in sorted(node.children):
            collect(node.children[ch])

    collect(root)
    offs = [0] * len(nodes)
    for _ in range(16):
        sizes = []
        for node in nodes:
            payload = b"\x00\x00" if node.terminal else b""
            size = uleb_size(len(payload)) + len(payload) + 1
            for ch in sorted(node.children):
                size += len(ch) + 1 + uleb_size(offs[node.children[ch].idx])
            sizes.append(size)
        new, at = [], 0
        for size in sizes:
            new.append(at)
            at += size
        if new == offs:
            break
        offs = new
    else:
        raise SystemExit("trie não convergiu")
    blob = bytearray()
    for node in nodes:
        payload = b"\x00\x00" if node.terminal else b""
        blob += uleb(len(payload)) + payload
        kids = sorted(node.children)
        blob.append(len(kids))
        for ch in kids:
            blob += ch.encode("latin1") + b"\x00" + uleb(offs[node.children[ch].idx])
    return bytes(blob)


def parse_back(trie):
    names = []

    def walk(node, pref, depth=0):
        if depth > 512 or node >= len(trie):
            raise SystemExit("trie inválida")
        i, shift, tsz = node, 0, 0
        while True:
            c = trie[i]
            i += 1
            tsz |= (c & 0x7F) << shift
            if c < 128:
                break
            shift += 7
        if tsz:
            names.append(pref)
            i += tsz
        nch = trie[i]
        i += 1
        for _ in range(nch):
            e = i
            while trie[e]:
                e += 1
            edge = trie[i:e].decode("latin1")
            i = e + 1
            shift, child = 0, 0
            while True:
                c = trie[i]
                i += 1
                child |= (c & 0x7F) << shift
                if c < 128:
                    break
                shift += 7
            walk(child, pref + edge, depth + 1)

    walk(0, "")
    return names


def macho(trie):
    # cabeçalho + LC_DYLD_EXPORTS_TRIE; o trie fica logo em seguida
    dataoff = 32 + 16
    hdr = struct.pack("<IIIIIIII", 0xFEEDFACF, 0x01000007, 3, 6, 1, 16, 4, 0)
    cmd = struct.pack("<IIII", 0x80000033, 16, dataoff, len(trie))
    return hdr + cmd + trie


def main():
    names = [ln.strip() for ln in sys.stdin if ln.strip()]
    trie = build(names)
    got = set(parse_back(trie))
    missing = [n for n in names if n not in got]
    if missing:
        raise SystemExit("trie perdeu %d nomes, o primeiro é %s" % (len(missing), missing[0]))
    out = sys.argv[1]
    with open(out, "wb") as f:
        f.write(macho(trie))
    print("dylib %s: %d símbolos, trie %d bytes" % (out, len(names), len(trie)))


if __name__ == "__main__":
    main()
