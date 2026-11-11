#!/bin/bash
# Equivalente Linux de tools/build-swift-combine-objc.ps1: o OpenCombine (MIT) compilado como o
# módulo "Combine", com interop ObjC e library evolution, para os nomes mangled serem os da Apple
# ($s7Combine...). Sai como /System/Library/Frameworks/Combine.framework/Versions/A/Combine em
# build/swift/objc/dylib, que o carregador lê do disco antes do libSystem.
#
# Antes: tools/build-libswift-real.sh (Swift.swiftmodule, Dispatch e o resource dir em /tmp/mos-swift-resdir).
# Fonte: OpenCombine 0.14.0 (tag do GitHub, SHA-256 conferido) em .tools/openswiftui/deps/OpenCombine-0.14.0.
#   tools/build-swift-combine.sh
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
zig=${MOS_ZIG:-}
if [ -z "$zig" ]; then for z in "$root"/.tools/zig-*/zig; do [ -x "$z" ] && { zig=$z; break; }; done; fi
fe=${SWIFTC:-$root/.tools/swift-6.4.2/usr/bin/swift-frontend}
objc="$root/build/swift/objc"
ov="$objc/overlays"
inc="$objc/sdk/include"
res=/tmp/mos-swift-resdir
OCVER=0.14.0
OCSHA=161bb7907052a5a4e65356fdc3883ab1df463f1891cc8412166608d88a8d356a
ocdir="$root/.tools/openswiftui/deps/OpenCombine-$OCVER"
src="$ocdir/Sources"
if [ ! -d "$src/OpenCombine" ]; then
    zip="$root/.tools/dl/OpenCombine-$OCVER.zip"
    mkdir -p "$root/.tools/dl" "$root/.tools/openswiftui/deps"
    # curl, wget ou python3 (tools/baixar.sh): a máquina de build pode não ter curl
    . "$root/tools/baixar.sh"
    baixar "https://github.com/OpenCombine/OpenCombine/archive/refs/tags/$OCVER.zip" "$zip" "$OCSHA" \
        || { echo "build-swift-combine: download ou SHA-256 do OpenCombine falhou" >&2; exit 1; }
    (cd "$root/.tools/openswiftui/deps" && unzip -q -o "$zip")
fi
shims="$root/.tools/swift-src/stdlib/public/SwiftShims/swift/shims"
for need in "$zig" "$fe"; do [ -x "$need" ] || { echo "build-swift-combine: falta $need" >&2; exit 1; }; done
[ -d "$src/OpenCombine" ] || { echo "build-swift-combine: falta o OpenCombine em $src" >&2; exit 1; }
[ -d "$objc/Swift.swiftmodule" ] && [ -f "$ov/Dispatch.swiftmodule" ] || { echo "build-swift-combine: rode tools/build-libswift-real.sh antes" >&2; exit 1; }
[ -d "$res" ] || { echo "build-swift-combine: falta $res (criado pelo build-libswift-real.sh)" >&2; exit 1; }
# o SwiftPM gera o modulemap do alvo C; aqui ele é escrito ao lado do cabeçalho
hdir="$objc/combine-helpers"
mkdir -p "$hdir"
cp "$src/COpenCombineHelpers/include/COpenCombineHelpers.h" "$hdir/"
printf 'module COpenCombineHelpers {\n    header "COpenCombineHelpers.h"\n    export *\n}\n' > "$hdir/module.modulemap"
prep="$objc/combine-src"
tmp="$objc/combine-obj"
rm -rf "$prep"
mkdir -p "$prep" "$tmp"

# C++: CombineIdentifier e a trava; o os_unfair_lock do libSystem do mOS não é usado, vai pthread.
sed 's|#ifdef __APPLE__|#if 0 /* mOS: os_unfair_lock é no-op no libSystem; usa pthread */|' \
    "$src/COpenCombineHelpers/COpenCombineHelpers.cpp" > "$tmp/COpenCombineHelpers.cpp"
"$zig" c++ -target x86_64-macos -O2 -c -std=c++17 -fno-rtti -I"$src/COpenCombineHelpers/include" \
    "$tmp/COpenCombineHelpers.cpp" -o "$ov/Combine_COpenCombineHelpers.o"

# Fonte Swift preparado: OpenCombine. -> Combine., layout fixo dos tipos @inlinable como no Combine
# da Apple e Optional/Result.Publisher fora do invólucro OCombine (o nome mangled muda).
python3 - "$src/OpenCombine" "$prep" << 'PY'
import pathlib, re, sys
src, prep = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])

def remove_ocombine(t, name):
    l = t.split("\n")
    a = b = oc = ta = e = -1
    for i, x in enumerate(l):
        if a < 0:
            if re.match(r"^\s*public struct OCombine \{", x): a = i
        elif b < 0:
            if re.match(r"^\s*public struct Publisher\b", x): b = i
        elif oc < 0:
            if re.match(r"^\s*public var ocombine\b", x): oc = i
        elif ta < 0:
            if "public typealias Publisher = OCombine.Publisher" in x: ta = i
        elif e < 0:
            if x.startswith("#endif"): e = i
    if e < 0:
        raise SystemExit("remove_ocombine: formato inesperado em " + name)
    while a > 0 and re.match(r"^\s*///", l[a - 1]): a -= 1
    c = oc
    while not re.match(r"^\s{4}\}\s*$", l[c]): c -= 1
    return "\n".join(l[:a] + l[b:c] + l[ta + 1:e] + l[e + 1:])

n = 0
for f in sorted(src.rglob("*.swift")):
    t = f.read_text(encoding="utf-8")
    t = t.replace("OpenCombine.", "Combine.")
    t = t.replace("\nimport COpenCombineHelpers", "\n@_implementationOnly import COpenCombineHelpers")
    t = re.sub(r"(?m)^(\s*)public struct (AnyPublisher|AnySubscriber|Demand|Stride)\b", r"\1@frozen public struct \2", t)
    t = re.sub(r"(?m)^(\s*)@usableFromInline(\r?\n\s*)internal (final )?class ", r"\1@usableFromInline\2@_fixed_layout internal \3class ", t)
    t = t.replace(".OCombine.Publisher", ".Publisher").replace("extension Optional.OCombine {", "extension Optional {").replace("extension Result.OCombine {", "extension Result {")
    if f.name in ("Optional.Publisher.swift", "Result.Publisher.swift"):
        t = remove_ocombine(t, f.name)
    d = prep / f.relative_to(src)
    d.parent.mkdir(parents=True, exist_ok=True)
    d.write_text(t, encoding="utf-8")
    n += 1
print("combine-src: %d fontes" % n)
PY
mapfile -t files < <(find "$prep" -name '*.swift' | sort)
# O que o OpenCombine não tem (CombineLatest, Merge...): userland/sdk/swift-objc/Combine
for f in "$root"/userland/sdk/swift-objc/Combine/*.swift; do [ -f "$f" ] && files+=("$f"); done

"$fe" -frontend -c -target x86_64-apple-macos26 -resource-dir "$res" -module-name Combine -parse-as-library \
    -swift-version 5 -O -enable-library-evolution -I "$objc" -I "$ov" -I "$hdir" \
    -I "$shims" -module-cache-path "$objc/modcache" \
    -Xcc "-isystem$inc" -Xcc -DMOS_SWIFT_OBJC=1 -Xcc -Wno-nullability-completeness -Xcc "-I$hdir" \
    -emit-module-path "$ov/Combine.swiftmodule" -o "$ov/Combine.o" "${files[@]}" > "$tmp/Combine.log" 2>&1 || {
    grep -E ':[0-9]+:[0-9]+: error: |<unknown>:0: error: ' "$tmp/Combine.log" | head -25
    echo "Combine: FALHOU" >&2
    exit 1
}
echo "Combine: ok ($(( $(wc -c < "$ov/Combine.o") / 1024 )) KB, ${#files[@]} fontes)"

dir="$objc/dylib/System/Library/Frameworks/Combine.framework/Versions/A"
mkdir -p "$dir"
"$zig" c++ -target x86_64-macos -shared -o "$dir/Combine" \
    -Wl,-install_name,/System/Library/Frameworks/Combine.framework/Versions/A/Combine -Wl,-undefined,dynamic_lookup \
    "$ov/Combine.o" "$ov/Combine_COpenCombineHelpers.o"
echo "Combine.framework: ok ($(( $(wc -c < "$dir/Combine") / 1024 )) KB)"

# O Dispatch da Apple importa o Combine e traz DispatchQueue: Scheduler (SchedulerTimeType, schedule(after:...)):
# o Setapp e o WhatsApp importam esses nomes de libswiftDispatch. Recompila o overlay Dispatch com
# userland/sdk/swift-objc/Dispatch/Combine e religa a dylib (o .ps1 faz o mesmo com build-swift-overlays-objc.ps1).
# O Dispatch.swiftmodule de $ov fica o sem Combine: o próximo build do Combine importa o Dispatch, e o
# módulo novo (que importa o Combine) faria um ciclo.
sdk="$root/userland/sdk/swift-objc"
dest="$objc/dylib/usr/lib/swift"
if [ -f "$dest/libswiftDispatch.dylib" ]; then
    mkdir -p "$tmp/dispatch"
    "$fe" -frontend -c -target x86_64-apple-macos26 -resource-dir "$res" \
        -module-name Dispatch -parse-as-library -swift-version 5 -O \
        -enable-library-evolution -I "$objc" -I "$ov" -module-cache-path "$objc/modcache" \
        -I "$shims" -disable-implicit-concurrency-module-import -disable-implicit-string-processing-module-import \
        -Xcc "-isystem$inc" -Xcc -Wno-nullability-completeness -Xcc -DMOS_SWIFT_OBJC=1 \
        -emit-module-path "$tmp/dispatch/Dispatch.swiftmodule" -o "$tmp/dispatch/Dispatch.o" \
        -module-link-name swiftDispatch -autolink-force-load -import-underlying-module \
        "$sdk"/Dispatch/*.swift "$sdk"/Dispatch/Combine/*.swift > "$tmp/Dispatch.log" 2>&1 || {
        grep -E ':[0-9]+:[0-9]+: error: |<unknown>:0: error: ' "$tmp/Dispatch.log" | head -25
        echo "Dispatch+Combine: FALHOU" >&2
        exit 1
    }
    "$zig" c++ -target x86_64-macos -shared -o "$tmp/dispatch/libswiftDispatch.dylib" \
        -Wl,-install_name,/usr/lib/swift/libswiftDispatch.dylib -Wl,-undefined,dynamic_lookup "$tmp/dispatch/Dispatch.o"
    grep -a -q '_$sSo17OS_dispatch_queueC8DispatchE17SchedulerTimeTypeVMn' "$tmp/dispatch/libswiftDispatch.dylib" || {
        echo "Dispatch+Combine: libswiftDispatch sem DispatchQueue.SchedulerTimeType" >&2
        exit 1
    }
    mv -f "$tmp/dispatch/libswiftDispatch.dylib" "$dest/libswiftDispatch.dylib"
    echo "libswiftDispatch: religada com DispatchQueue: Scheduler"
fi
