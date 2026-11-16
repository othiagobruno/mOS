#!/usr/bin/env python3
"""mOS — gera .pkg de teste no formato do macOS (xar: TOC XML em zlib, checksums SHA-1, heap; cpio odc; gzip e pbzx).
Usado por tools/installer_test.sh. Uso como módulo: build_xar(members, path)."""
import gzip, hashlib, lzma, struct, zlib, os, sys

def cpio_odc(entries):
    """entries: lista de (nome, modo, dados)"""
    out = b''
    ino = 1
    for name, mode, data in entries + [('TRAILER!!!', 0, b'')]:
        nm = name.encode() + b'\0'
        out += b'070707' + b'%06o' % 0 + b'%06o' % ino + b'%06o' % mode + b'%06o' % 501 + b'%06o' % 20 + b'%06o' % 1 + \
               b'%06o' % 0 + b'%011o' % 1700000000 + b'%06o' % len(nm) + b'%011o' % len(data) + nm + data
        ino += 1
    return out

def pbzx(raw, chunk=4096):
    out = b'pbzx' + struct.pack('>Q', chunk)
    for i in range(0, len(raw), chunk):
        c = raw[i:i + chunk]
        x = lzma.compress(c, format=lzma.FORMAT_XZ)
        out += struct.pack('>QQ', len(c), len(x)) + x
    return out

def build_xar(members, path, corrupt=False):
    """members: lista de (caminho, bytes) ; pastas implícitas"""
    tree = {}
    for p, d in members:
        parts = p.split('/')
        node = tree
        for q in parts[:-1]:
            node = node.setdefault(q, {})
        node[parts[-1]] = d
    heap = bytearray(b'\0' * 20)       # checksum do TOC no começo do heap
    fid = [1]
    def emit(node, indent):
        x = ''
        for name in sorted(node):
            v = node[name]
            i = fid[0]; fid[0] += 1
            if isinstance(v, dict):
                x += f'{indent}<file id="{i}"><name>{name}</name><type>directory</type>\n' + emit(v, indent + ' ') + f'{indent}</file>\n'
            else:
                comp = zlib.compress(v)
                off = len(heap)
                heap.extend(comp)
                x += (f'{indent}<file id="{i}"><data><length>{len(comp)}</length><offset>{off}</offset><size>{len(v)}</size>'
                      f'<encoding style="application/x-gzip"/>'
                      f'<extracted-checksum style="sha1">{hashlib.sha1(v).hexdigest()}</extracted-checksum>'
                      f'<archived-checksum style="sha1">{hashlib.sha1(comp).hexdigest()}</archived-checksum></data>'
                      f'<name>{name}</name><type>file</type><mode>0644</mode></file>\n')
        return x
    files = emit(tree, '  ')
    toc = ('<?xml version="1.0" encoding="UTF-8"?>\n<xar>\n <toc>\n  <checksum style="sha1"><offset>0</offset><size>20</size></checksum>\n'
           '  <creation-time>2026-11-15T12:00:00</creation-time>\n' + files + ' </toc>\n</xar>\n').encode()
    tc = zlib.compress(toc)
    heap[0:20] = hashlib.sha1(tc).digest()
    if corrupt:
        heap[-1] ^= 0xff
    hdr = b'xar!' + struct.pack('>HHQQI', 28, 1, len(tc), len(toc), 1)
    with open(path, 'wb') as f:
        f.write(hdr + tc + bytes(heap))

def pkginfo(ident, version, loc, scripts=True, extra=''):
    s = '<scripts><preinstall file="./preinstall"/><postinstall file="./postinstall"/></scripts>' if scripts else ''
    return (f'<?xml version="1.0" encoding="utf-8"?>\n<pkg-info format-version="2" identifier="{ident}" version="{version}" '
            f'install-location="{loc}" auth="root"><payload installKBytes="4" numberOfFiles="3"/>{s}{extra}</pkg-info>\n').encode()

if __name__ == '__main__':
    sys.exit(0)
