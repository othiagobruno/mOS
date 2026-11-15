#!/bin/bash
# mOS - porte para Linux de tools/build-swift-foundation-objc.ps1 (fase 5, marco B de docs/swift-objc-runtime.md).
# O swift-foundation (Apache 2.0) na variante com interop ObjC: o FoundationEssentials sai como módulo Foundation
# (nomes mangled da Apple, $s10Foundation3URLV...) com library evolution, junto dos fontes de
# userland/sdk/swift-objc/Foundation. Também: Synchronization, swift-collections 1.7.3 e _FoundationCShims (C).
# Saída: build/swift/objc/overlays, ao lado dos overlays de tools/build-libswift-real.sh (que precisa ter rodado).
#   tools/build-swift-foundation-objc.sh [Módulo ...]
# Fontes (não vão para o git): MOS_SF_DEPS ou .tools/openswiftui/deps; se faltarem, baixa (SHA-256 conferido) o
# swift-foundation, o swift-collections 1.7.3 e o swift-experimental-string-processing 6.4.2.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
only=" $* "
want() { [ "$only" = "  " ] || [[ "$only" == *" $1 "* ]]; }

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
deps=${MOS_SF_DEPS:-$root/.tools/openswiftui/deps}
# Fontes fixadas por commit e SHA-256. O swift-foundation é um commit do main: a tag swift-6.4.2-RELEASE não tem
# CharacterSet nem _FoundationRefs no FoundationEssentials, e o userland/sdk/swift-objc/Foundation conta com os dois.
SF_REV=d53ce9ce2a1552ddbf8c5b63e8f1124c2c1c6751
SF_SHA=560216230a38075e59a33ef634bf0c38ca77f54667cd679ca757e76720f61f94
SC_SHA=c21cef95b650bc5d852b8d030df8cd42d123df7c5aa9f0f0b762f15867d2fc1b
SP_SHA=8452bfdeac7f1afb9fe91faeba34f09ad46eb8e3085d345d3839f9ad4b69032b
sfroot=${MOS_SF_SRC:-$deps/swift-foundation-$SF_REV}
scroot="$deps/swift-collections-1.7.3"
sproot="$deps/swift-experimental-string-processing-swift-6.4.2-RELEASE"
OO_REV=b030e44d6471cf1d760137e5da6805cf57aa885b
OO_SHA=d8b4f7805aa611f73ecbc81447c5f5ca381b74feae0b57bd5f7def2a760b50d5
ooroot="$deps/OpenObservation-$OO_REV"
if [ ! -d "$sfroot/Sources" ] || [ ! -d "$scroot/Sources" ] || [ ! -d "$sproot/Sources" ] || [ ! -d "$ooroot/Sources" ]; then
    # shellcheck source=tools/baixar.sh
    . "$root/tools/baixar.sh"
    mkdir -p "$deps/dl"
    pega() {   # URL ARQUIVO SHA PASTA
        [ -d "$4/Sources" ] && return 0
        baixar "$1" "$deps/dl/$2" "$3" && tar xzf "$deps/dl/$2" -C "$deps"
    }
    pega "https://github.com/swiftlang/swift-foundation/archive/$SF_REV.tar.gz" "swift-foundation-$SF_REV.tar.gz" "$SF_SHA" "$sfroot"
    pega https://github.com/apple/swift-collections/archive/refs/tags/1.7.3.tar.gz swift-collections-1.7.3.tar.gz "$SC_SHA" "$scroot"
    pega https://github.com/swiftlang/swift-experimental-string-processing/archive/refs/tags/swift-6.4.2-RELEASE.tar.gz \
        swift-experimental-string-processing-6.4.2.tar.gz "$SP_SHA" "$sproot"
    pega "https://github.com/OpenSwiftUIProject/OpenObservation/archive/$OO_REV.tar.gz" "OpenObservation-$OO_REV.tar.gz" "$OO_SHA" "$ooroot"
fi
if [ ! -x "${zig:-/}" ] || [ ! -x "${fe:-/}" ] || [ ! -d "$sfroot/Sources" ] || [ ! -d "$scroot/Sources" ]; then
    echo "build-swift-foundation-objc: falta zig, swift-frontend 6.4.2, swift-foundation ou swift-collections 1.7.3" >&2
    echo "  zig=${zig:-ausente} swift-frontend=${fe:-ausente} deps=$deps" >&2
    exit 1
fi
objc="$root/build/swift/objc"
out="$objc/overlays"
inc="$objc/sdk/include"
shims="$src/stdlib/public/SwiftShims/swift/shims"
if [ ! -d "$out/Darwin.swiftmodule" ] && [ ! -f "$out/Darwin.swiftmodule" ]; then
    echo "build-swift-foundation-objc: faltam os overlays (tools/build-libswift-real.sh)" >&2
    exit 1
fi
# o mesmo resource dir de tools/build-libswift-real.sh: o do toolchain Linux sem shims/module.modulemap
res=/tmp/mos-swift-resdir
if [ ! -d "$res/shims" ]; then
    tc=$(cd "$(dirname "$fe")/../lib/swift" && pwd)
    mkdir -p "$res/shims"
    [ -d "$tc/clang" ] && ln -sfn "$tc/clang" "$res/clang"
    find "$tc/shims" -maxdepth 1 -type f ! -name 'module.modulemap' -exec cp {} "$res/shims/" \;
fi
fails=0

# build_mod NOME LINGUAGEM EVOLUÇÃO(1/0) -- extras... -- fontes...
build_mod() {
    local name=$1 lang=$2 evo=$3
    shift 3
    want "$name" || return 0
    local extra=() files=()
    [ "$1" = -- ] && shift
    while [ $# -gt 0 ] && [ "$1" != -- ]; do extra+=("$1"); shift; done
    [ "${1:-}" = -- ] && shift
    files=("$@")
    local a=(-c -target x86_64-apple-macos26 -resource-dir "$res" -module-name "$name" -parse-as-library
             -swift-version "$lang" -O -I "$objc" -I "$out" -I "$shims" -module-cache-path "$objc/modcache-foundation"
             -Xcc "-isystem$inc" -Xcc -nostdlibinc -Xcc -DMOS_SWIFT_OBJC=1 -Xcc -Wno-nullability-completeness
             -module-link-name "swift$name" -emit-module-path "$out/$name.swiftmodule" -o "$out/$name.o")
    [ "$evo" = 1 ] && a=(-enable-library-evolution "${a[@]}")
    a+=("${extra[@]}" "${files[@]}")
    # um argumento por linha; com espaço ou aspas vai entre aspas (o .rsp do swift-frontend separa por espaço)
    local x
    for x in "${a[@]}"; do
        if [[ "$x" == *[[:space:]\"]* ]]; then x=${x//\\/\\\\}; printf '"%s"\n' "${x//\"/\\\"}"; else printf '%s\n' "$x"; fi
    done > "$out/$name.rsp"
    "$fe" -frontend "@$out/$name.rsp" > "$out/$name.log" 2>&1
    local rc=$?
    local nerr
    nerr=$(grep -cE ':[0-9]+:[0-9]+: error: |<unknown>:0: error: ' "$out/$name.log")
    if [ $rc -ne 0 ] || [ "$nerr" -gt 0 ]; then
        grep -E ':[0-9]+:[0-9]+: error: |<unknown>:0: error: ' "$out/$name.log" | head -20 | sed 's/^/  /'
        echo "$name: FALHOU ($nerr erros, saída $rc)"
        fails=$((fails + 1))
        return 1
    fi
    echo "$name: ok ($(( $(stat -c %s "$out/$name.o") / 1024 )) KB, ${#files[@]} fontes)"
}

# ---- Synchronization (Atomic/Mutex) do mOS
mapfile -t f < <(find "$root/userland/sdk/openswiftui/swift/Synchronization" -name '*.swift' | sort)
build_mod Synchronization 5 1 -- -parse-stdlib -enable-experimental-feature RawLayout -- "${f[@]}"

# ---- swift-collections 1.7.3
sc="$scroot/Sources"
scflags=(-package-name SwiftCollections -enable-upcoming-feature MemberImportVisibility)
for ft in BuiltinModule Lifetimes InoutLifetimeDependence AddressableParameters AddressableTypes SuppressedAssociatedTypesWithDefaults; do
    scflags+=(-enable-experimental-feature "$ft")
done
for kv in 5.0=10.14.4 5.1=10.15 5.6=12.3 5.7=13.0 5.8=13.3 5.9=14.0 5.10=14.4 6.0=15.0 6.1=15.4 6.2=26.0 6.3=26.4 6.4=27.0; do
    scflags+=(-enable-experimental-feature "AvailabilityMacro=SwiftStdlib ${kv%%=*}: macOS ${kv#*=}, iOS 99, watchOS 99, tvOS 99, visionOS 99")
done
for m in InternalCollectionsUtilities:InternalCollectionsUtilities:6 SpanPreview:SpanPreview:6 BasicContainers:BasicContainers:6 \
         DequeModule:DequeModule:6 OrderedCollections:OrderedCollections:6 _RopeModule:RopeModule:5; do
    IFS=: read -r mod dir lang <<< "$m"
    [ -d "$sc/$dir" ] || continue
    mapfile -t f < <(find "$sc/$dir" -name '*.swift' | sort)
    build_mod "$mod" "$lang" 1 -- "${scflags[@]}" -- "${f[@]}"
done

# ---- _StringProcessing (Regex), que o FoundationEssentials usa: swift-experimental-string-processing 6.4.2
# (Apache 2.0), como em tools/build-swift-overlays-objc.ps1: _RegexParser + _CUnicode (C) + _StringProcessing
spr="$sproot/Sources"; [ -d "$spr" ] || spr=
if [ -n "$spr" ]; then
    spav=()
    for kv in 5.7=13.0 5.8=13.3 5.9=14.0 5.10=14.4 6.0=15.0 6.1=15.4 6.2=26.0 6.3=26.4 6.4=27.0; do
        spav+=(-define-availability "SwiftStdlib ${kv%%=*}:macOS ${kv#*=}, iOS 99, watchOS 99, tvOS 99")
    done
    mapfile -t f < <(find "$spr/_RegexParser" -name '*.swift' | sort)
    # quem implementa o Regex não pode importar o _StringProcessing implícito (o da build anterior na pasta)
    build_mod _RegexParser 5 1 -- -autolink-force-load -disable-implicit-string-processing-module-import "${spav[@]}" -- "${f[@]}"
    if want _StringProcessing; then
        rm -f "$out"/_CUnicode_*.o
        for c in "$spr"/_CUnicode/*.c; do
            "$zig" cc -target x86_64-macos -O2 -c "-I$spr/_CUnicode/include" -std=gnu11 "$c" -o "$out/_CUnicode_$(basename "$c" .c).o" || fails=$((fails + 1))
        done
    fi
    mapfile -t f < <(find "$spr/_StringProcessing" -name '*.swift' | sort)
    build_mod _StringProcessing 5 1 -- -autolink-force-load -disable-implicit-string-processing-module-import "${spav[@]}" -Xcc "-I$spr/_CUnicode/include" -- "${f[@]}"
else
    echo "aviso: sem swift-experimental-string-processing em $deps; o Foundation não acha Regex" >&2
fi

# ---- swift-foundation
sfs="$sfroot/Sources"
# cópia do _FoundationCShims: o platform_shims.h usa os kOSThermalPressureLevel* de <libkern/OSThermalNotification.h>.
# Aqui esse cabeçalho já entrou pelo módulo Darwin (que o _FoundationCShims não importa): a guarda impede a
# reinclusão e o enum fica invisível. Os valores são os do cabeçalho público (macOS 0..4; os outros 0, 10..50).
csh="$objc/foundation-src/_FoundationCShims"
rm -rf "$csh"
mkdir -p "$(dirname "$csh")"
cp -a "$sfs/_FoundationCShims" "$csh"
python3 - "$csh/include/platform_shims.h" << 'PY'
import re, sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
i = s.find('#if TARGET_OS_OSX || TARGET_OS_MACCATALYST', s.find('OSThermalNotification.h'))
e = s.find('#endif', s.find('#else', i))
osx = {'Nominal': 0, 'Moderate': 1, 'Heavy': 2, 'Trapping': 3, 'Sleeping': 4}
other = {'Nominal': 0, 'Light': 10, 'Moderate': 20, 'Heavy': 30, 'Trapping': 40, 'Sleeping': 50}
j = s.find('#else', i)
fix = lambda t, m: re.sub(r'= kOSThermalPressureLevel(\w+)', lambda x: '= %d' % m[x.group(1)], t)
s = s[:i] + fix(s[i:j], osx) + fix(s[j:e], other) + s[e:]
open(p, 'w', encoding='utf-8').write(s)
PY
if want _FoundationCShims; then
    mapfile -t cf < <(find "$csh" -maxdepth 1 -name '*.c' | sort)
    okc=${#cf[@]}
    rm -f "$out"/_FoundationCShims_*.o
    for c in "${cf[@]}"; do
        "$zig" cc -target x86_64-macos -O2 -c "-I$csh/include" -std=gnu11 "$c" -o "$out/_FoundationCShims_$(basename "$c" .c).o" || okc=0
    done
    if [ "$okc" -gt 0 ]; then
        echo '_FoundationCShims (C): ok'
    else
        fails=$((fails + 1))
    fi
fi
sfflags=(-package-name SwiftFoundation -I "$csh/include" -enable-upcoming-feature InferSendableFromCaptures
         -enable-upcoming-feature MemberImportVisibility)
for v in 6.0.2 6.1 6.2 6.3 6.4 6.4.2 6.5; do
    sfflags+=(-enable-experimental-feature "AvailabilityMacro=FoundationPreview ${v}:macOS 26, iOS 26, tvOS 26, watchOS 26, visionOS 26")
done
for ft in VariadicGenerics Lifetimes AddressableTypes AllowUnsafeAttribute BuiltinModule AccessLevelOnImport ImportMacroAliases; do
    sfflags+=(-enable-experimental-feature "$ft")
done
# _FoundationRefs só existe no main do swift-foundation; interno, sem library evolution
if [ -d "$sfs/_FoundationRefs" ]; then
    mapfile -t f < <(find "$sfs/_FoundationRefs" -name '*.swift' | sort)
    build_mod _FoundationRefs 5 0 -- "${sfflags[@]}" -- "${f[@]}"
fi

# Cópia preparada do FoundationEssentials (mesmas trocas do .ps1, ver os comentários lá)
prep="$objc/foundation-src/FoundationEssentials"
if want Foundation; then
    rm -rf "$prep"
    python3 - "$sfs/FoundationEssentials" "$prep" << 'PY'
import os, re, sys
base, prep = sys.argv[1], sys.argv[2]
aliases = {'NSData.ReadingOptions': 'NSDataReadingOptions', 'NSData.WritingOptions': 'NSDataWritingOptions',
           'NSData.Base64EncodingOptions': 'NSDataBase64EncodingOptions',
           'NSData.Base64DecodingOptions': 'NSDataBase64DecodingOptions',
           'NSString.CompareOptions': 'NSStringCompareOptions',
           'NSURL.BookmarkResolutionOptions': 'NSURLBookmarkResolutionOptions',
           'NSURL.BookmarkCreationOptions': 'NSURLBookmarkCreationOptions'}
err_blocks = {'CocoaError.swift': {1, 9}, 'ErrorCodes.swift': {1, 2, 3}, 'ErrorCodes+POSIX.swift': {1}}
patched = 0
for dp, dn, fn in os.walk(base):
    rel = os.path.relpath(dp, base)
    parts = rel.split(os.sep)
    if 'ProgressManager' in parts or 'NotificationCenter' in parts:
        continue
    for n in sorted(fn):
        if not n.endswith('.swift'):
            continue
        src = os.path.join(dp, n)
        dst = os.path.normpath(os.path.join(prep, rel, n))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        lines = open(src, encoding='utf-8').read().split('\n')
        touched = False
        for i, l in enumerate(lines):
            m = re.match(r'^\s*public typealias \w+ = (NS\w+\.\w+)\s*$', l)
            if not m or m.group(1) not in aliases:
                continue
            lines[i] = l.replace(m.group(1), aliases[m.group(1)])
            touched = True
            j = i - 1
            while j >= 0 and not re.match(r'^\s*#\s*(if|else|elseif)\b', lines[j]):
                j -= 1
            if j < 0 or 'MOS_NS_OPTIONS' in lines[j]:
                continue
            mi = re.match(r'^\s*#\s*if\s+(.+)$', lines[j])
            if mi:
                lines[j] = '#if (%s) || MOS_NS_OPTIONS' % mi.group(1).strip()
                continue
            depth, k = 0, j - 1
            while k >= 0:
                if re.match(r'^\s*#\s*endif\b', lines[k]):
                    depth += 1
                elif re.match(r'^\s*#\s*if\b', lines[k]):
                    if depth == 0:
                        break
                    depth -= 1
                k -= 1
            if k >= 0 and 'MOS_NS_OPTIONS' not in lines[k]:
                mk = re.match(r'^\s*#\s*if\s+(.+)$', lines[k])
                if mk:
                    lines[k] = '#if (%s) && !MOS_NS_OPTIONS' % mk.group(1).strip()
        if touched:
            patched += 1
        for i, l in enumerate(lines):
            if not re.match(r'^\s*#', l):
                lines[i] = l.replace('FoundationEssentials.', 'Foundation.')
        for i, l in enumerate(lines):
            if re.match(r'^\s*public struct URLResourceKey \{\}\s*$', l):
                lines[i] = '// mOS: URLResourceKey = NSURLResourceKey (ObjC)'
        if os.path.basename(dp) == 'Error' and n in err_blocks:
            nth = 0
            for i, l in enumerate(lines):
                if re.match(r'^\s*#\s*if\s+FOUNDATION_FRAMEWORK\s*$', l):
                    nth += 1
                    if nth in err_blocks[n]:
                        lines[i] = '#if FOUNDATION_FRAMEWORK || MOS_NSERROR'
                lines[i] = (lines[i].replace('return NSCocoaErrorDomain', 'return _mosString(NSCocoaErrorDomain)')
                            .replace('return NSPOSIXErrorDomain', 'return _mosString(NSPOSIXErrorDomain)')
                            .replace('return (self as NSError).localizedDescription',
                                     'return _mosString((self as NSError).localizedDescription)'))
        if n == 'ComparisonResult.swift':
            for i, l in enumerate(lines):
                if re.match(r'^\s*#\s*if\s+!FOUNDATION_FRAMEWORK\s*$', l):
                    lines[i] = '#if !FOUNDATION_FRAMEWORK && !MOS_NS_OPTIONS'
                    break
        lines.insert(0, 'import _MOSNSOptions   // mOS: opções NS_OPTIONS da Apple (tools/build-swift-foundation-objc.sh)')
        open(dst, 'w', encoding='utf-8').write('\n'.join(lines))
print('Foundation: fonte do swift-foundation preparado (%d arquivos com opções NS_OPTIONS)' % patched)
PY
    mapfile -t f < <( { find "$prep" -name '*.swift'; find "$root/userland/sdk/swift-objc/Foundation" -maxdepth 1 -name '*.swift'; } | sort)
    rm -f "$out/FoundationEssentials.swiftmodule" "$out/FoundationEssentials.o"
    build_mod Foundation 5 1 -- "${sfflags[@]}" -import-underlying-module -autolink-force-load \
        -D DATA_LEGACY_ABI -D MOS_NS_OPTIONS -D MOS_NSERROR -- "${f[@]}"
fi

# ---- dylibs, como no tools/link-swift-dylibs-objc.ps1. Uma dylib passada ao link vira LC_LOAD_DYLIB: o
# libswiftFoundation carrega o _StringProcessing (Regex) mesmo quando o app não o pede.
dest="$objc/dylib/usr/lib/swift"
mkdir -p "$dest"
link_dylib() {
    local name=$1
    shift
    if "$zig" c++ -target x86_64-macos -shared -o "$dest/$name.dylib" \
        -Wl,-install_name,/usr/lib/swift/$name.dylib -Wl,-undefined,dynamic_lookup "$@"; then
        echo "$name: $(wc -c < "$dest/$name.dylib") bytes"
    else
        echo "$name: FALHOU no link"
        fails=$((fails + 1))
    fi
}
if [ $fails -eq 0 ] && [ -f "$out/_RegexParser.o" ] && [ -f "$out/_StringProcessing.o" ]; then
    link_dylib libswift_RegexParser "$out/_RegexParser.o"
    link_dylib libswift_StringProcessing "$out/_StringProcessing.o" "$out"/_CUnicode_*.o "$dest/libswift_RegexParser.dylib"
fi
# libswiftFoundation: o módulo Foundation e o que ele leva dentro. O carregador (kernel/compat/macho.c) a junta a
# quem pede o Foundation.framework.
if [ $fails -eq 0 ] && [ -f "$out/Foundation.o" ]; then
    objs=()
    for c in "$out"/_FoundationCShims_*.o; do [ -f "$c" ] && objs+=("$c"); done
    for m in Foundation _FoundationRefs Synchronization InternalCollectionsUtilities SpanPreview \
             BasicContainers DequeModule OrderedCollections _RopeModule; do
        [ -f "$out/$m.o" ] && objs+=("$out/$m.o")
    done
    [ -f "$dest/libswift_StringProcessing.dylib" ] && objs+=("$dest/libswift_StringProcessing.dylib" "$dest/libswift_RegexParser.dylib")
    link_dylib libswiftFoundation "${objs[@]}"
fi

# ---- UniformTypeIdentifiers: o struct UTType sobre a classe ObjC do libSystem. Importa o Foundation acima.
if [ $fails -eq 0 ] && { [ -f "$out/Foundation.swiftmodule" ] || [ -d "$out/Foundation.swiftmodule" ]; }; then
    mapfile -t f < <(find "$root/userland/sdk/swift-objc/UniformTypeIdentifiers" -maxdepth 1 -name '*.swift' | sort)
    if build_mod UniformTypeIdentifiers 5 1 -- -import-underlying-module -autolink-force-load -- "${f[@]}" &&
        want UniformTypeIdentifiers; then
        link_dylib libswiftUniformTypeIdentifiers "$out/UniformTypeIdentifiers.o" "$dest/libswiftFoundation.dylib"
    fi
fi

# ---- Observation: o OpenObservation (MIT, porte do Observation da Apple) com o nome do módulo da Apple, em Swift
# puro, como em tools/build-swift-overlays-objc.ps1 (ObservationRegistrar, Observable, withObservationTracking).
# Sem o plugin de macros: o @Observable dos apps já vem expandido no binário.
if [ -d "$ooroot/Sources/OpenObservation" ]; then
    mapfile -t f < <(find "$ooroot/Sources/OpenObservation" -maxdepth 1 -name '*.swift' | sort)
    if build_mod Observation 5 1 -- -autolink-force-load -- "${f[@]}" && want Observation; then
        link_dylib libswiftObservation "$out/Observation.o"
    fi
fi
exit $fails
