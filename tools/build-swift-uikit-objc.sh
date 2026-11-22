#!/bin/bash
# mOS - porte para Linux de tools/build-swift-uikit-objc.ps1 (docs/uikit-catalyst.md): o overlay Swift do UIKit do
# Mac Catalyst (userland/sdk/swift-objc/UIKit/*.swift) sobre o módulo clang UIKit (userland/sdk/swift-objc/sdk/UIKit),
# com interop ObjC e library evolution, como o módulo "UIKit" da Apple.
# Saída: build/swift/objc/overlays/UIKit.swiftmodule (o objeto em overlays/uikit, fora do link estático dos testes) e
# /System/iOSSupport/usr/lib/swift/libswiftUIKit.dylib na raiz das dylibs (build/swift/objc/dylib).
# Depois do UIKit, o overlay do PhotosUI (userland/sdk/swift-objc/PhotosUI, módulo clang sdk/PhotosUI):
# /System/iOSSupport/usr/lib/swift/libswiftPhotosUI.dylib (docs/photosui.md).
# Antes: tools/build-swift-foundation-objc.sh (Foundation) e tools/build-frameworks-objc.sh (UIKit ObjC).
#   tools/build-swift-uikit-objc.sh                  (MOS_ARCH=arm64: fatia arm64 em build/swift/objc-arm64)
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
. "$root/tools/zig-link.sh"   # zig_link: no macOS, refaz com o clang do Xcode quando o linker do zig cai
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
case "${MOS_ARCH:-x86_64}" in
    x86_64) zt=x86_64-macos; st=x86_64-apple-macos26; objc="$root/build/swift/objc" ;;
    arm64) zt=aarch64-macos; st=arm64-apple-macos26; objc="$root/build/swift/objc-arm64" ;;   # docs/runtime-arm64-plano.md
    *) echo "MOS_ARCH=$MOS_ARCH: use x86_64 ou arm64" >&2; exit 1 ;;
esac
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
cp -a "$sdk/sdk/UIKit" "$sdk/sdk/Foundation" "$sdk/sdk/PhotosUI" "$inc/"
mc="$objc/modcache-uikit"
rm -rf "$mc"
lib="$objc/dylib/usr/lib/swift"
dir="$objc/dylib/System/iOSSupport/usr/lib/swift"
mkdir -p "$dir"
# ligação em dois níveis com o runtime real (como em build-swift-foundation-objc.sh); o resto (frameworks ObjC,
# libobjc, libSystem) fica em busca plana
base=()
for b in libswiftCore libswift_Concurrency libswiftDarwin libswiftObjectiveC libswiftCoreFoundation libswiftCoreGraphics \
         libswiftDispatch libswiftos libswiftFoundation; do
    [ -f "$lib/$b.dylib" ] && base+=("$lib/$b.dylib")
done
# overlay NOME: compila userland/sdk/swift-objc/NOME/*.swift e liga /System/iOSSupport/usr/lib/swift/libswiftNOME.dylib
overlay() {
    local name=$1 low
    shift
    low=$(echo "$name" | tr '[:upper:]' '[:lower:]')
    mkdir -p "$out/$low"
    local files
    mapfile -t files < <(find "$sdk/$name" -maxdepth 1 -name '*.swift' | sort)
    local a=(-c -target "$st" -resource-dir "$res" -module-name "$name" -parse-as-library -swift-version 5 -O
       -enable-library-evolution -I "$objc" -I "$out" -module-cache-path "$mc"
       -I "$src/stdlib/public/SwiftShims/swift/shims"
       -Xcc "-isystem$inc" -Xcc -nostdlibinc -Xcc -Wno-nullability-completeness -Xcc -DMOS_SWIFT_OBJC=1
       -import-underlying-module -module-link-name "swift$name" -autolink-force-load
       -emit-module-path "$out/$name.swiftmodule" -o "$out/$low/$name.o" "${files[@]}")
    local x
    for x in "${a[@]}"; do
        if [[ "$x" == *[[:space:]\"]* ]]; then x=${x//\\/\\\\}; printf '"%s"\n' "${x//\"/\\\"}"; else printf '%s\n' "$x"; fi
    done > "$out/$name.rsp"
    "$fe" -frontend "@$out/$name.rsp" > "$out/$name.log" 2>&1
    local rc=$? nerr
    nerr=$(grep -cE ':[0-9]+:[0-9]+: error: |<unknown>:0: error: ' "$out/$name.log")
    if [ $rc -ne 0 ] || [ "$nerr" -gt 0 ]; then
        grep -E ':[0-9]+:[0-9]+: error: |<unknown>:0: error: ' "$out/$name.log" | head -20 | sed 's/^/  /'
        echo "$name (Swift): FALHOU ($nerr erros, saída $rc)"; return 1
    fi
    echo "$name (Swift): ok ($(( $(stat -c %s "$out/$low/$name.o") / 1024 )) KB, ${#files[@]} fontes)"
    if ! zig_link "$zig" c++ -target "$zt" -shared -o "$dir/libswift$name.dylib" \
            -Wl,-install_name,/System/iOSSupport/usr/lib/swift/libswift$name.dylib -Wl,-undefined,dynamic_lookup \
            "$out/$low/$name.o" "$@" "${base[@]}"; then
        echo "libswift$name: link FALHOU"; return 1
    fi
    echo "libswift$name: ok ($(( $(stat -c %s "$dir/libswift$name.dylib") / 1024 )) KB) -> /System/iOSSupport/usr/lib/swift/libswift$name.dylib"
}
overlay UIKit || exit 1
overlay PhotosUI "$dir/libswiftUIKit.dylib" || exit 1
