#!/bin/sh
# Testes de host das libswift* (gcc/cc). Não sobe QEMU e não carrega o VS Code
# como processo: o machocheck só lê o Mach-O.
set -e
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cc=$(command -v gcc || command -v cc)
"$cc" -Wall -Wextra -I "$root/kernel/include" -o /tmp/libswift_image_test "$root/tools/libswift_image_test.c"
"$cc" -std=gnu11 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
    -DMOS_LIBSYS_SYSCALL -I "$root/userland/libsystem" -I "$root" \
    -o /tmp/swiftsect_test "$root/tools/swiftsect_test.c" \
    "$root/userland/libsystem/dyldimg.c" "$root/userland/libsystem/swiftsect.c"
"$cc" -Wall -Wextra -O2 -o /tmp/gen-libswift-stubs "$root/tools/gen-libswift-stubs.c"
"$cc" -O2 -o /tmp/machocheck "$root/tools/machocheck.c"
/tmp/libswift_image_test
/tmp/swiftsect_test

img=$(mktemp -d)
fix=$img/Fixture
/tmp/gen-libswift-stubs "$img" "$root/tools/libswift-stub-exports.txt" --fixture "$fix"
echo 'int mos_dummy(void){return 1;}' > /tmp/mosdummy.c
"$cc" -shared -fPIC -o /tmp/libSystem-empty.mos /tmp/mosdummy.c
python3 - "$img" << 'PY'
import sys, pathlib
root = pathlib.Path(sys.argv[1])
n = 0
for p in root.glob("*.dylib"):
    b = p.read_bytes()
    if b"STUB mOS libswift" not in b or b"\xC3" not in b:
        print("stub sem marca:", p.name)
        raise SystemExit(1)
    n += 1
print("stubs com marca STUB e ret: %d" % n)
if n < 12:
    raise SystemExit(1)
PY
/tmp/machocheck /tmp/libSystem-empty.mos "$img" /tmp/libswift-with.txt >/tmp/libswift-with.out
python3 - "$root/tools/libswift-stub-exports.txt" /tmp/libswift-with.txt /tmp/libswift-with.out << 'PY'
import sys
names=[]
for line in open(sys.argv[1]):
    line=line.strip()
    if line.startswith("_"): names.append(line)
miss=set()
for line in open(sys.argv[2]):
    if "\t" not in line: continue
    miss.add(line.rstrip("\n").split("\t",1)[1])
gone=[n for n in names if n in miss]
text=open(sys.argv[3]).read().splitlines()
fix=[ln for ln in text if ln.rstrip().endswith("/Fixture")]
print("com as dylibs: %d símbolos da lista, %d ainda ausentes" % (len(names), len(gone)))
print(fix[0] if fix else "fixture ausente no resumo")
if gone or not fix:
    print("\n".join(gone[:20]))
    sys.exit(1)
parts=fix[0].split()
imp=int(parts[parts.index("imports")+1])
aus=int(parts[parts.index("ausentes")+1])
if imp != len(names) or aus != 0:
    print("fixture deveria importar %d e faltar 0" % len(names))
    sys.exit(1)
if len(names) < 53:
    print("a lista tem menos de 53 símbolos")
    sys.exit(1)
PY

nod=$(mktemp -d)
cp "$fix" "$nod/Fixture"
/tmp/machocheck /tmp/libSystem-empty.mos "$nod" /tmp/libswift-without.txt >/tmp/libswift-without.out
python3 - "$root/tools/libswift-stub-exports.txt" /tmp/libswift-without.txt /tmp/libswift-without.out << 'PY'
import sys
names=[ln.strip() for ln in open(sys.argv[1]) if ln.startswith("_")]
miss=set()
for line in open(sys.argv[2]):
    if "\t" not in line: continue
    miss.add(line.rstrip("\n").split("\t",1)[1])
lack=[n for n in names if n not in miss]
text=open(sys.argv[3]).read().splitlines()
fix=[ln for ln in text if ln.rstrip().endswith("/Fixture")]
print("sem as dylibs: %d símbolos no relatório de ausentes" % (len(names) - len(lack)))
print(fix[0] if fix else "fixture ausente no resumo")
if lack or not fix:
    print("não ausentes:", " ".join(lack[:10]))
    sys.exit(1)
parts=fix[0].split()
imp=int(parts[parts.index("imports")+1])
aus=int(parts[parts.index("ausentes")+1])
if imp != len(names) or aus != len(names):
    print("sem as dylibs o fixture deveria faltar todos os imports")
    sys.exit(1)
PY

if [ -d "/tmp/vscode-app/Visual Studio Code.app" ]; then
    /tmp/machocheck /tmp/libSystem-empty.mos "/tmp/vscode-app" /tmp/vscode-noswift.txt >/tmp/vscode-noswift.out
    both=$(mktemp -d)
    ln -s "/tmp/vscode-app/Visual Studio Code.app" "$both/Visual Studio Code.app"
    mkdir -p "$both/usr/lib/swift"
    cp "$img"/*.dylib "$both/usr/lib/swift/"
    /tmp/machocheck /tmp/libSystem-empty.mos "$both" /tmp/vscode-with-swift.txt >/tmp/vscode-with-swift.out
    python3 - << 'PY'
def rows(path):
    return [ln for ln in open(path) if ln.startswith("libswift")]
before=rows("/tmp/vscode-noswift.txt")
after=rows("/tmp/vscode-with-swift.txt")
print("VS Code sem as libswift* da imagem: %d linhas libswift ausentes" % len(before))
print("VS Code com as libswift* da imagem: %d linhas libswift ausentes" % len(after))
if len(before) != 53 or after:
    print("".join(after[:10]))
    raise SystemExit(1)
PY
    rm -rf "$both"
fi

stage=$(mktemp -d)
echo REAL > "$stage/libswiftCore.dylib"
sh "$root/tools/stage-libswift.sh" "$stage" >/tmp/stage-libswift.out
python3 - "$stage" << 'PY'
import pathlib, sys
d = pathlib.Path(sys.argv[1])
core = (d / "libswiftCore.dylib").read_bytes()
found = (d / "libswiftFoundation.dylib").read_bytes()
print("stage: Core preservada=%s Foundation stub=%s" % (core == b"REAL\n", b"STUB mOS libswift" in found))
n = len(list(d.glob("libswift*.dylib")))
if core != b"REAL\n" or b"STUB mOS libswift" not in found or n < 12:
    raise SystemExit(1)
print("stage: %d libswift*.dylib no destino" % n)
PY
rm -rf "$stage"

tree=$(mktemp -d)
mkdir -p "$tree/EFI/BOOT" "$tree/mOS/usr/lib/swift"
printf 'EFI' > "$tree/EFI/BOOT/BOOTX64.EFI"
cp "$img"/*.dylib "$tree/mOS/usr/lib/swift/"
"$cc" -O2 -o /tmp/mkimg "$root/tools/mkimg.c"
/tmp/mkimg /tmp/mos-libswift.img "$tree" 32
python3 - << 'PY'
b=open("/tmp/mos-libswift.img","rb").read()
need=b"libswiftCore.dylib"
print("imagem: %d bytes, libswiftCore.dylib %s" % (len(b), "presente" if need in b else "AUSENTE"))
if need not in b: raise SystemExit(1)
PY
rm -rf "$img" "$nod" "$tree"
echo "libswift_check: ok"
