#!/bin/bash
# mOS - porte para Linux de tools/build-swift-uikit-objc.ps1 (docs/uikit-catalyst.md): o overlay Swift do UIKit do
# Mac Catalyst (userland/sdk/swift-objc/UIKit/*.swift) sobre o módulo clang UIKit (userland/sdk/swift-objc/sdk/UIKit),
# com interop ObjC e library evolution, como o módulo "UIKit" da Apple.
# Saída: build/swift/objc/overlays/UIKit.swiftmodule (o objeto em overlays/uikit, fora do link estático dos testes) e
# /System/iOSSupport/usr/lib/swift/libswiftUIKit.dylib na raiz das dylibs (build/swift/objc/dylib).
# Antes: tools/build-swift-foundation-objc.sh (Foundation) e tools/build-frameworks-objc.sh (UIKit ObjC).
#   tools/build-swift-uikit-objc.sh
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
zig=${MOS_ZIG:-}
if [ -z "$zig" ]; then for z in "$root"/.tools/zig-*/zig; do [ -x "$z" ] && { zig=$z; break; }; done; fi
[ -z "$zig" ] && zig=$(command -v zig || true)
fe=${SWIFTC:-}
if [ -z "$fe" ]; then
    for c in "$root"/.tools/swift-6.4.2/usr/bin/swift-frontend "$root"/.tools/swift/*/usr/bin/swift-frontend; do
        [ -x "$c" ] && { fe=$c; break; }
    done
fi
src=${SWIFT_SRC:-$root/.tools/swift-src}
objc="$root/build/swift/objc"
out="$objc/overlays"
inc="$objc/sdk/include"
sdk="$root/userland/sdk/swift-objc"
res=/tmp/mos-swift-resdir
if [ -z "$zig" ] || [ -z "$fe" ] || [ ! -x "$fe" ]; then
    echo "build-swift-uikit-objc: falta zig ou swift-frontend 6.4.2" >&2; exit 1
fi
if [ ! -e "$out/Foundation.swiftmodule" ]; then
    echo "build-swift-uikit-objc: falta o Foundation (tools/build-swift-foundation-objc.sh)" >&2; exit 1
fi
if [ ! -d "$res/shims" ]; then
    tc=$(cd "$(dirname "$fe")/../lib/swift" && pwd)
    mkdir -p "$res/shims"
    [ -d "$tc/clang" ] && ln -sfn "$tc/clang" "$res/clang"
    find "$tc/shims" -maxdepth 1 -type f ! -name 'module.modulemap' -exec cp {} "$res/shims/" \;
fi
# o módulo clang UIKit vai junto dos outros cabeçalhos da variante
mkdir -p "$inc" "$out/uikit"
cp -f "$sdk/sdk/module.modulemap" "$inc/module.modulemap"
cp -a "$sdk/sdk/UIKit" "$sdk/sdk/Foundation" "$inc/"
mc="$objc/modcache-uikit"
rm -rf "$mc"
mapfile -t files < <(find "$sdk/UIKit" -maxdepth 1 -name '*.swift' | sort)
a=(-c -target x86_64-apple-macos26 -resource-dir "$res" -module-name UIKit -parse-as-library -swift-version 5 -O
   -enable-library-evolution -I "$objc" -I "$out" -module-cache-path "$mc"
   -I "$src/stdlib/public/SwiftShims/swift/shims"
   -Xcc "-isystem$inc" -Xcc -nostdlibinc -Xcc -Wno-nullability-completeness -Xcc -DMOS_SWIFT_OBJC=1
   -import-underlying-module -module-link-name swiftUIKit -autolink-force-load
   -emit-module-path "$out/UIKit.swiftmodule" -o "$out/uikit/UIKit.o" "${files[@]}")
for x in "${a[@]}"; do
    if [[ "$x" == *[[:space:]\"]* ]]; then x=${x//\\/\\\\}; printf '"%s"\n' "${x//\"/\\\"}"; else printf '%s\n' "$x"; fi
done > "$out/UIKit.rsp"
"$fe" -frontend "@$out/UIKit.rsp" > "$out/UIKit.log" 2>&1
rc=$?
nerr=$(grep -cE ':[0-9]+:[0-9]+: error: |<unknown>:0: error: ' "$out/UIKit.log")
if [ $rc -ne 0 ] || [ "$nerr" -gt 0 ]; then
    grep -E ':[0-9]+:[0-9]+: error: |<unknown>:0: error: ' "$out/UIKit.log" | head -20 | sed 's/^/  /'
    echo "UIKit (Swift): FALHOU ($nerr erros, saída $rc)"; exit 1
fi
echo "UIKit (Swift): ok ($(( $(stat -c %s "$out/uikit/UIKit.o") / 1024 )) KB, ${#files[@]} fontes)"

# ligação em dois níveis com o runtime real (como em build-swift-foundation-objc.sh); o resto (UIKit ObjC, libobjc,
# libSystem) fica em busca plana
lib="$objc/dylib/usr/lib/swift"
base=()
for b in libswiftCore libswift_Concurrency libswiftDarwin libswiftObjectiveC libswiftCoreFoundation libswiftCoreGraphics \
         libswiftDispatch libswiftos libswiftFoundation; do
    [ -f "$lib/$b.dylib" ] && base+=("$lib/$b.dylib")
done
dir="$objc/dylib/System/iOSSupport/usr/lib/swift"
mkdir -p "$dir"
if ! "$zig" c++ -target x86_64-macos -shared -o "$dir/libswiftUIKit.dylib" \
        -Wl,-install_name,/System/iOSSupport/usr/lib/swift/libswiftUIKit.dylib -Wl,-undefined,dynamic_lookup \
        "$out/uikit/UIKit.o" "${base[@]}"; then
    echo "libswiftUIKit: link FALHOU"; exit 1
fi
echo "libswiftUIKit: ok ($(( $(stat -c %s "$dir/libswiftUIKit.dylib") / 1024 )) KB) -> /System/iOSSupport/usr/lib/swift/libswiftUIKit.dylib"
