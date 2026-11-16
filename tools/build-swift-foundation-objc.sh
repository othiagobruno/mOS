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
# swift-system (Apache 2.0) com o nome do módulo da Apple (System): o Package.swift só muda o nome (SystemPackage)
SY_VER=1.9.0
SY_SHA=e164360588c9b4a3b1388650e86d7e0f9b61129f303181eae7f85eb15843d8f3
syroot="$deps/swift-system-$SY_VER"
if [ ! -d "$sfroot/Sources" ] || [ ! -d "$scroot/Sources" ] || [ ! -d "$sproot/Sources" ] || [ ! -d "$ooroot/Sources" ] ||
   [ ! -d "$syroot/Sources" ]; then
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
    pega "https://github.com/apple/swift-system/archive/refs/tags/$SY_VER.tar.gz" "swift-system-$SY_VER.tar.gz" "$SY_SHA" "$syroot"
fi
if [ ! -x "${zig:-/}" ] || [ ! -x "${fe:-/}" ] || [ ! -d "$sfroot/Sources" ] || [ ! -d "$scroot/Sources" ]; then
    echo "build-swift-foundation-objc: falta zig, swift-frontend 6.4.2, swift-foundation ou swift-collections 1.7.3" >&2
    echo "  zig=${zig:-ausente} swift-frontend=${fe:-ausente} deps=$deps" >&2
    exit 1
fi
case "${MOS_ARCH:-x86_64}" in
    x86_64) zt=x86_64-macos; st=x86_64-apple-macos26; objc="$root/build/swift/objc" ;;
    arm64) zt=aarch64-macos; st=arm64-apple-macos26; objc="$root/build/swift/objc-arm64" ;;   # docs/runtime-arm64-plano.md
    *) echo "MOS_ARCH=$MOS_ARCH: use x86_64 ou arm64" >&2; exit 1 ;;
esac
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
# cache de módulos próprio e limpo: dois .pcm do mesmo módulo com flags diferentes dão "module X is defined in both"
rm -rf "$objc/modcache-foundation"
# os cabeçalhos do Foundation ObjC e do UTType (o build-libswift-real.sh os copia; aqui de novo, para valer a última versão)
cp -a "$root/userland/sdk/swift-objc/sdk/Foundation" "$root/userland/sdk/swift-objc/sdk/UniformTypeIdentifiers" "$inc/"
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
    local a=(-c -target "$st" -resource-dir "$res" -module-name "$name" -parse-as-library
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

# ---- Synchronization (Atomic, Mutex, _Cell): o módulo do próprio swift-src 6.4.2 (Apache 2.0), com a ABI da Apple,
# em libswiftSynchronization.dylib (os apps importam Mutex/_Cell dela). O Foundation e o SwiftUI ligam nela.
stdav=()
for kv in 5.0=10.14.4 5.1=10.15 5.2=10.15.4 5.3=11.0 5.4=11.3 5.5=12.0 5.6=12.3 5.7=13.0 5.8=13.3 5.9=14.0 5.10=14.4 6.0=15.0 6.1=15.4 6.2=26.0 6.3=26.4 6.4=27.0; do
    stdav+=(-define-availability "SwiftStdlib ${kv%%=*}:macOS ${kv#*=}, iOS 99, watchOS 99, tvOS 99, visionOS 99")
done
sy="$src/stdlib/public/Synchronization"
mkdir -p "$out/syncgen"
for g in AtomicIntegers AtomicStorage; do python3 "$src/utils/gyb.py" -D CMAKE_SIZEOF_VOID_P=8 -o "$out/syncgen/$g.swift" "$sy/Atomics/$g.swift.gyb"; done
f=("$sy"/Atomics/{Atomic,AtomicBool,AtomicFloats,AtomicLazyReference,AtomicMemoryOrderings,AtomicOptional,AtomicPointers,AtomicRepresentable,WordPair}.swift
   "$sy/Cell.swift" "$out/syncgen/AtomicIntegers.swift" "$out/syncgen/AtomicStorage.swift" "$sy/Mutex/DarwinImpl.swift" "$sy/Mutex/Mutex.swift")
build_mod Synchronization 5 1 -- -autolink-force-load -enable-builtin-module -enable-experimental-feature RawLayout \
    -enable-experimental-feature StaticExclusiveOnly -enable-experimental-feature Extern -strict-memory-safety \
    -library-level api "${stdav[@]}" -- "${f[@]}"

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
            "$zig" cc -target "$zt" -O2 -c "-I$spr/_CUnicode/include" -std=gnu11 "$c" -o "$out/_CUnicode_$(basename "$c" .c).o" || fails=$((fails + 1))
        done
    fi
    mapfile -t f < <(find "$spr/_StringProcessing" -name '*.swift' | sort)
    build_mod _StringProcessing 5 1 -- -autolink-force-load -disable-implicit-string-processing-module-import "${spav[@]}" -Xcc "-I$spr/_CUnicode/include" -- "${f[@]}"
    # RegexBuilder (a DSL Regex { ... } dos apps) sobre o _StringProcessing acima
    mapfile -t f < <(find "$spr/RegexBuilder" -name '*.swift' | sort)
    build_mod RegexBuilder 5 1 -- -autolink-force-load "${spav[@]}" -- "${f[@]}"
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
        "$zig" cc -target "$zt" -O2 -c "-I$csh/include" -std=gnu11 "$c" -o "$out/_FoundationCShims_$(basename "$c" .c).o" || okc=0
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
           'NSData.SearchOptions': 'NSDataSearchOptions',
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

# ---- System (FilePath, FileDescriptor, Errno, FilePermissions...): swift-system com o nome System, como o
# System.framework do macOS. As macros de disponibilidade são as do Package.swift dele (a versão -> o macOS).
if [ -d "$syroot/Sources/System" ]; then
    syav=()
    while IFS= read -r ln; do
        v=$(sed -n 's/^ *Available("\([^"]*\)", "\([^"]*\)").*/\1|\2/p' <<< "$ln")
        [ -n "$v" ] && syav+=(-enable-experimental-feature "AvailabilityMacro=System ${v%%|*}:${v#*|}")
    done < "$syroot/Package.swift"
    mapfile -t f < <(find "$syroot/Sources/System" -name '*.swift' | sort)
    f+=("$root/userland/sdk/swift-objc/System/SystemABI.swift")   # nomes da ABI antiga do System do macOS
    # CSystem: o módulo C do swift-system (só cabeçalhos no Darwin; o shims.c é para Linux/BSD)
    build_mod System 5 1 -- -autolink-force-load -I "$syroot/Sources/CSystem/include" -D SYSTEM_PACKAGE_DARWIN "${syav[@]}" -- "${f[@]}"
fi

llvm_nm_u() { "$(dirname "$fe")/llvm-nm" -u "$1" 2>/dev/null | awk '{print $NF}'; }

# ---- dylibs, como no tools/link-swift-dylibs-objc.ps1. Uma dylib passada ao link vira LC_LOAD_DYLIB: o
# libswiftFoundation carrega o _StringProcessing (Regex) mesmo quando o app não o pede.
dest="$objc/dylib/usr/lib/swift"
mkdir -p "$dest"
# As dylibs base do runtime real entram no link: os swift_* e os $s... delas viram ligação em dois níveis (ordinal da
# dylib). Só com -undefined dynamic_lookup eles ficavam em busca plana, e o carregador do mOS acha antes os 209 swift_*
# do libSystem (outro runtime, o do caminho sem ObjC): swift_bridgeObjectRetain dava proteção geral no URL(string:) e
# swift_getGenericMetadata devolvia nulo para Regex<...>. O dynamic_lookup fica para o libSystem e o libobjc.
base_dylibs=()
for b in libswiftCore libswift_Concurrency libswiftDarwin libswiftObjectiveC libswiftCoreFoundation libswiftCoreGraphics \
         libswiftDispatch libswiftos; do
    [ -f "$dest/$b.dylib" ] && base_dylibs+=("$dest/$b.dylib")
done
link_dylib() {
    local name=$1
    shift
    if "$zig" c++ -target "$zt" -shared -o "$dest/$name.dylib" \
        -Wl,-install_name,/usr/lib/swift/$name.dylib -Wl,-undefined,dynamic_lookup "$@" "${base_dylibs[@]}"; then
        echo "$name: $(wc -c < "$dest/$name.dylib") bytes"
    else
        echo "$name: FALHOU no link"
        fails=$((fails + 1))
    fi
}
# libswiftSynchronization: antes das outras (o Foundation liga nela)
if [ $fails -eq 0 ] && [ -f "$out/Synchronization.o" ]; then
    link_dylib libswiftSynchronization "$out/Synchronization.o"
    base_dylibs+=("$dest/libswiftSynchronization.dylib")
fi
if [ $fails -eq 0 ] && [ -f "$out/_RegexParser.o" ] && [ -f "$out/_StringProcessing.o" ]; then
    link_dylib libswift_RegexParser "$out/_RegexParser.o"
    link_dylib libswift_StringProcessing "$out/_StringProcessing.o" "$out"/_CUnicode_*.o "$dest/libswift_RegexParser.dylib"
    [ -f "$out/RegexBuilder.o" ] && link_dylib libswiftRegexBuilder "$out/RegexBuilder.o" "$dest/libswift_StringProcessing.dylib" "$dest/libswift_RegexParser.dylib"
fi
# System.framework: o caminho do macOS 11+ (o app pede /System/Library/Frameworks/System.framework/Versions/A/System)
if [ $fails -eq 0 ] && [ -f "$out/System.o" ]; then
    sysfw="$objc/dylib/System/Library/Frameworks/System.framework/Versions/A"
    mkdir -p "$sysfw"
    if "$zig" c++ -target "$zt" -shared -o "$sysfw/System" \
        -Wl,-install_name,/System/Library/Frameworks/System.framework/Versions/A/System -Wl,-undefined,dynamic_lookup \
        "$out/System.o" "${base_dylibs[@]}"; then
        ln -sfn A "$objc/dylib/System/Library/Frameworks/System.framework/Versions/Current"
        ln -sfn Versions/Current/System "$objc/dylib/System/Library/Frameworks/System.framework/System"
        echo "System.framework: $(wc -c < "$sysfw/System") bytes"
    else
        echo "System.framework: FALHOU no link"; fails=$((fails + 1))
    fi
fi
# libswiftFoundation: o módulo Foundation e o que ele leva dentro. O carregador (kernel/compat/macho.c) a junta a
# quem pede o Foundation.framework.
if [ $fails -eq 0 ] && [ -f "$out/Foundation.o" ]; then
    objs=()
    for c in "$out"/_FoundationCShims_*.o; do [ -f "$c" ] && objs+=("$c"); done
    for m in Foundation _FoundationRefs InternalCollectionsUtilities SpanPreview \
             BasicContainers DequeModule OrderedCollections _RopeModule; do
        [ -f "$out/$m.o" ] && objs+=("$out/$m.o")
    done
    [ -f "$dest/libswift_StringProcessing.dylib" ] && objs+=("$dest/libswift_StringProcessing.dylib" "$dest/libswift_RegexParser.dylib")
    # com o módulo Combine (tools/build-swift-combine.sh) o swift-foundation importa TopLevelEncoder/Decoder dele: o
    # Combine.framework entra como LC_LOAD_DYLIB, senão esses símbolos ficam nulos no carregador
    cmb="$objc/dylib/System/Library/Frameworks/Combine.framework/Versions/A/Combine"
    if grep -q '7Combine15TopLevel' <(llvm_nm_u "$out/Foundation.o"); then
        [ -f "$cmb" ] || { echo "libswiftFoundation: o Foundation.o usa o Combine e falta $cmb" >&2; fails=$((fails + 1)); }
        # o zig reconhece a dylib pelo sufixo; o LC_LOAD_DYLIB sai com o install name do Combine
        ln -sfn "$cmb" "$out/Combine.dylib"
        objs+=("$out/Combine.dylib")
    fi
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

# ---- _System_Foundation: a ponte FilePath <-> URL (FilePath(URL)), que precisa do System e do Foundation
if [ $fails -eq 0 ] && [ -f "$out/System.o" ] && [ -f "$dest/libswiftFoundation.dylib" ]; then
    if build_mod _System_Foundation 5 1 -- -autolink-force-load -- "$root/userland/sdk/swift-objc/System/SystemFoundation.swift" &&
        want _System_Foundation; then
        # o zig só liga entradas com extensão conhecida: uma cópia .dylib do System.framework (o LC_ID_DYLIB dela é o
        # caminho do framework, então o LC_LOAD_DYLIB da ponte sai certo)
        cp -f "$objc/dylib/System/Library/Frameworks/System.framework/Versions/A/System" "$out/System-link.dylib"
        link_dylib libswift_System_Foundation "$out/_System_Foundation.o" "$dest/libswiftFoundation.dylib" "$out/System-link.dylib"
    fi
fi

# ---- Observation: o módulo do próprio swift-src 6.4.2 (Apache 2.0), com a ABI da Apple: ObservationRegistrar, Observable,
# withObservationTracking e o Observations (AsyncSequence do Swift 6.2) que o OpenObservation não tinha. O TLS da
# transação (ThreadLocal.cpp do original usa a camada de threads do runtime) sai de userland/sdk/swift-objc/Observation.
# Sem o plugin de macros: o @Observable dos apps já vem expandido no binário.
obs="$src/stdlib/public/Observation/Sources/Observation"
if [ -d "$obs" ]; then
    f=("$obs"/{ContinuousObservation,Locking,Observable,ObservationRegistrar,ObservationTracking,Observations,ThreadLocal}.swift)
    if build_mod Observation 5 1 -- -autolink-force-load -enable-experimental-feature Macros \
            -enable-experimental-feature ExtensionMacros -library-level api "${stdav[@]}" -- "${f[@]}" && want Observation; then
        "$zig" cc -target x86_64-macos -O2 -c "$root/userland/sdk/swift-objc/Observation/ThreadLocal.c" -o "$out/Observation_tls.o" &&
            link_dylib libswiftObservation "$out/Observation.o" "$out/Observation_tls.o"
    fi
fi
exit $fails
