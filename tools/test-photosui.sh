#!/bin/bash
# Linux: teste do PhotosUI do Catalyst (asserções em userland/tests-swiftobjc/photosui/main.swift). Liga o teste como um
# binário da Apple contra as dylibs de build/swift/objc/dylib (libswiftCore real, Foundation, UIKit, PhotosUI, Photos) e
# roda no build/machotest numa raiz própria: cópia por hard link da árvore de dylibs (nada é escrito nela) com três PNG
# em /Users/Visitante/Imagens e o kTCCServicePhotos concedido ao app de teste. Placar: "photosui: N/N ok".
# Antes: tools/build-frameworks-objc.sh (UIKit, Photos, PhotosUI) e tools/build-swift-uikit-objc.sh.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
zig=${MOS_ZIG:-$(command -v zig || true)}
fe=${SWIFTC:-$root/.tools/swift-6.4.2/usr/bin/swift-frontend}
objc="$root/build/swift/objc"
dy="$objc/dylib"
sw="$dy/usr/lib/swift"
ios="$dy/System/iOSSupport"
fw="$ios/System/Library/Frameworks/UIKit.framework/Versions/A/UIKit"
pfw="$ios/System/Library/Frameworks/PhotosUI.framework/Versions/A/PhotosUI"
t="$root/build/swiftobjctest/photosui"
res=/tmp/mos-swift-resdir
for f in "$root/build/machotest" "$root/build/libSystem.mos" "$fe" "$sw/libswiftCore.dylib" "$fw" "$pfw" \
         "$ios/usr/lib/swift/libswiftUIKit.dylib" "$ios/usr/lib/swift/libswiftPhotosUI.dylib" \
         "$dy/System/Library/Frameworks/Photos.framework/Versions/A/Photos"; do
    [ -e "$f" ] || { echo "test-photosui: falta $f" >&2; exit 2; }
done
mkdir -p "$t"
rm -f "$t/PhotosUITest"
rm -rf "$t/mc"
"$fe" -frontend -c -target x86_64-apple-macos26 -resource-dir "$res" -module-name PhotosUITest -swift-version 5 -O \
    -I "$objc" -I "$objc/overlays" -I "$root/.tools/swift-src/stdlib/public/SwiftShims/swift/shims" \
    -module-cache-path "$t/mc" -Xcc "-isystem$objc/sdk/include" -Xcc -nostdlibinc -Xcc -DMOS_SWIFT_OBJC=1 \
    -Xcc -Wno-nullability-completeness "$root/userland/tests-swiftobjc/photosui/main.swift" -o "$t/PhotosUITest.o" \
    > "$t/compilacao.txt" 2>&1 || { grep -m20 ' error: ' "$t/compilacao.txt" >&2; echo "compilação FALHOU" >&2; exit 1; }
ln -sfn "$fw" "$t/UIKit-link.dylib"
ln -sfn "$pfw" "$t/PhotosUI-link.dylib"
ln -sfn "$dy/System/Library/Frameworks/Photos.framework/Versions/A/Photos" "$t/Photos-link.dylib"
libs=()
for b in libswiftCore libswift_Concurrency libswiftDarwin libswiftObjectiveC libswiftCoreFoundation libswiftCoreGraphics \
         libswiftDispatch libswiftFoundation libswift_StringProcessing; do
    [ -f "$sw/$b.dylib" ] && libs+=("$sw/$b.dylib")
done
"$zig" cc -target x86_64-macos -o "$t/PhotosUITest" "$t/PhotosUITest.o" "$t/UIKit-link.dylib" "$t/PhotosUI-link.dylib" \
    "$t/Photos-link.dylib" "$ios/usr/lib/swift/libswiftUIKit.dylib" "$ios/usr/lib/swift/libswiftPhotosUI.dylib" "${libs[@]}" \
    -Wl,-undefined,dynamic_lookup >> "$t/compilacao.txt" 2>&1 || { tail -20 "$t/compilacao.txt" >&2; echo "link FALHOU" >&2; exit 1; }
# raiz do teste: hard links da árvore (sem copiar bytes) + biblioteca de fotos + decisão do TCC
r="$t/root"
rm -rf "$r"
cp -al "$dy" "$r"
mkdir -p "$r/Users/Visitante/Imagens" "$r/Users/Visitante/Library/Application Support/com.mos.tcc" "$r/tmp"
python3 -I - "$r/Users/Visitante/Imagens" <<'PY'
import struct, sys, zlib, os
def png(path, w, h, rgb):
    raw = b''.join(b'\x00' + bytes(rgb) * w for _ in range(h))
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + \
        chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
    open(path, 'wb').write(data)
d = sys.argv[1]
png(os.path.join(d, 'a.png'), 8, 6, (200, 30, 30))
png(os.path.join(d, 'b.png'), 4, 4, (30, 200, 30))
png(os.path.join(d, 'c.png'), 6, 9, (30, 30, 200))
PY
printf 'com.mos.app kTCCServicePhotos granted\n' > "$r/Users/Visitante/Library/Application Support/com.mos.tcc/decisions.txt"
export MACHOTEST_WATCHDOG="${MACHOTEST_WATCHDOG:-20000}"
"$root/build/machotest" "$root/build/libSystem.mos" "$r" "$t/PhotosUITest" > "$t/saida.txt" 2>&1
code=$?
grep -E 'photosui|  ok   |FALHA|exceção|não implementado|macho_load' "$t/saida.txt" | head -60
rm -rf "$r"
if grep -E -q 'photosui: ([0-9]+)/\1 ok' "$t/saida.txt"; then exit 0; fi
echo "photosui FALHOU (machotest saiu $code); saída em $t/saida.txt" >&2
exit 1
