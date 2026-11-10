#!/bin/bash
# Compila o runtime Swift real (Mach-O x86_64) para build/swift/objc/dylib/usr/lib/swift.
# Não é o stub de tools/gen-libswift-stubs.c. tools/build.sh e o CI chamam este arquivo.
#
# Precisa de:
#   zig 0.17.1 ELF (MOS_ZIG ou .tools/zig-*/zig)
#   swift-frontend 6.4.2 (SWIFTC, ou .tools/swift-6.4.2/usr/bin/swift-frontend)
#   fonte swift-6.4.2-RELEASE (SWIFT_SRC ou .tools/swift-src)
# Se o dylib real já existe, para. MOS_SWIFT_REBUILD=1 refaz.
# Sem toolchain: avisa e sai 0 (o stage completa com stub). Com toolchain e erro de compilação: sai 1.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
dest="$root/build/swift/objc/dylib/usr/lib/swift"
core="$dest/libswiftCore.dylib"
if [ -z "${MOS_SWIFT_REBUILD:-}" ] && [ -f "$core" ] && ! grep -a -q 'STUB mOS libswift' "$core"; then
    echo "libswift real já está em $dest (MOS_SWIFT_REBUILD=1 para refazer)"
    exit 0
fi

zig=${MOS_ZIG:-}
if [ -z "$zig" ]; then
    for z in "$root"/.tools/zig-*/zig; do
        if [ -x "$z" ]; then zig=$z; break; fi
    done
fi
fe=${SWIFTC:-}
if [ -z "$fe" ]; then
    for c in "$root"/.tools/swift-6.4.2/usr/bin/swift-frontend \
             "$root"/.tools/swift/*/usr/bin/swift-frontend \
             /tmp/swift642/swift-*-ubuntu*/usr/bin/swift-frontend; do
        if [ -x "$c" ]; then fe=$c; break; fi
    done
fi
src=${SWIFT_SRC:-$root/.tools/swift-src}
if [ -z "$zig" ] || [ ! -x "$zig" ] || [ -z "$fe" ] || [ ! -x "$fe" ] || [ ! -f "$src/stdlib/public/core/CMakeLists.txt" ]; then
    echo "aviso: sem zig, swift-frontend 6.4.2 ou fonte swift-6.4.2. As libswift* que faltarem continuam stub." >&2
    echo "aviso: zig=${zig:-ausente} swift-frontend=${fe:-ausente} src=$src" >&2
    exit 0
fi

tc=$(CDPATH= cd -- "$(dirname "$fe")/../lib/swift" && pwd)
objc="$root/build/swift/objc"
out="$objc/overlays"
inc="$objc/sdk/include"
sdk="$root/userland/sdk/swift-objc"
shims="$src/stdlib/public/SwiftShims/swift/shims"
res=/tmp/mos-swift-resdir
mkdir -p "$objc/Swift.swiftmodule" "$out" "$objc/modcache" "$objc/overlays-src" "$objc/concurrency-src" "$objc/runtime" "$dest" "$src/gen/stubs" "$src/include/swift/Runtime"

echo "build-libswift-real: $fe"
rm -rf "$res"
mkdir -p "$res/shims"
if [ -d "$tc/clang" ]; then ln -s "$tc/clang" "$res/clang"; fi
find "$tc/shims" -maxdepth 1 -type f ! -name 'module.modulemap' -exec cp {} "$res/shims/" \;

# --- gyb da stdlib ---
for g in "$src"/stdlib/public/core/*.gyb; do
    base=$(basename "$g" .gyb)
    extra=
    case "$base" in
        IntegerTypes.swift|FloatingPointTypes.swift|SIMD*) extra="-D CMAKE_SIZEOF_VOID_P=8" ;;
    esac
    # shellcheck disable=SC2086
    python3 "$src/utils/gyb.py" $extra -o "$src/gen/$base" "$g"
done
python3 "$src/utils/gyb.py" -o "$src/gen/stubs/SwiftNativeNSXXXBase.mm" \
    "$src/stdlib/public/stubs/SwiftNativeNSXXXBase.mm.gyb"

cat > "$src/include/swift/Runtime/CMakeConfig.h" << 'EOF'
#ifndef SWIFT_RUNTIME_CMAKECONFIG_H
#define SWIFT_RUNTIME_CMAKECONFIG_H
#define SWIFT_VERSION_MAJOR "6"
#define SWIFT_VERSION_MINOR "4"
#endif
EOF

python3 - "$src" << 'PY'
import pathlib, re, sys
src = pathlib.Path(sys.argv[1])
core = src / "stdlib/public/core"
files = []
for raw in (core / "CMakeLists.txt").read_text(errors="replace").splitlines():
    m = re.match(r"^(EMBEDDED|NORMAL)\s+(\S+)", raw.strip())
    if not m:
        continue
    f = m.group(2)
    if "${" in f or "linker-support" in f:
        continue
    files.append(str(src / "gen" / f[:-4]) if f.endswith(".gyb") else str(core / f))
files.append(str(core / "SIMDVector.swift"))
seen, out = set(), []
for f in files:
    if f not in seen:
        seen.add(f)
        out.append(f)
missing = [f for f in out if not pathlib.Path(f).is_file()]
if missing:
    sys.stderr.write("fontes da stdlib ausentes: %s\n" % missing[:8])
    sys.exit(1)
pathlib.Path("/tmp/mos-swift-files.txt").write_text("\n".join(out) + "\n")
avail = []
for line in (src / "utils/availability-macros.def").read_text().splitlines():
    if line and not line.startswith("#"):
        avail += ["-define-availability", line.split(":", 1)[0] + ":macOS 10.15"]
pathlib.Path("/tmp/mos-swift-avail.txt").write_text("\n".join(avail) + "\n")
print("stdlib: %d fontes" % len(out))
PY

mapfile -t FILES < /tmp/mos-swift-files.txt
mapfile -t AVAIL < /tmp/mos-swift-avail.txt
feat="Macros FreestandingMacros Extern BitwiseCopyable ValueGenerics AddressableParameters AddressableTypes AllowUnsafeAttribute SuppressedAssociatedTypesWithDefaults Reparenting Lifetimes BorrowAndMutateAccessors BorrowInout LifetimeDependence"
fargs=()
for x in $feat; do fargs+=(-enable-experimental-feature "$x"); done

echo "==> stdlib"
if [ -n "${MOS_SWIFT_SKIP_CORE:-}" ] && [ -f "$objc/swiftCore.o" ]; then
    echo "stdlib: reusa $objc/swiftCore.o"
else
"$fe" -frontend -c -target x86_64-apple-macos26 -module-name Swift -parse-stdlib -parse-as-library \
    -nostdimport -swift-version 5 -O -enforce-exclusivity=unchecked \
    -I "$shims" -disable-objc-attr-requires-foundation-module \
    -Xcc "-I$root/userland/sdk/objc/include" \
    -strict-memory-safety -Xcc -Wno-nullability-completeness -Xcc -DSWIFT_OBJC_INTEROP=1 \
    -D SWIFT_ENABLE_REFLECTION -D SWIFT_RUNTIME_OS_VERSIONING -D SWIFT_STDLIB_ENABLE_VECTOR_TYPES \
    -D SWIFT_STDLIB_HAS_COMMANDLINE -D SWIFT_STDLIB_HAS_STDIN \
    -enable-library-evolution "${fargs[@]}" "${AVAIL[@]}" \
    -emit-module-path "$objc/Swift.swiftmodule/x86_64-apple-macos.swiftmodule" \
    -o "$objc/swiftCore.o" "${FILES[@]}"
fi

echo "==> runtime C++"
if [ -n "${MOS_SWIFT_SKIP_CORE:-}" ] && ls "$objc/runtime/"*.o >/dev/null 2>&1; then
    echo "runtime: reusa objetos"
else
python3 - "$root" "$zig" "$src" << 'PY'
import subprocess, sys, pathlib
root, zig, src = map(pathlib.Path, sys.argv[1:4])
rt = src / "stdlib/public/runtime"
out = root / "build/swift/objc/runtime"
skip = {"ImageInspectionELF","ImageInspectionCOFF","ImageInspectionStatic","ImageInspectionWasm","WindowsUnicode","Backtrace","BacktraceUtils","CrashHandlerMacOS","CrashHandlerLinux","CrashHandlerWindows","SwiftRT-COFF","SwiftRT-ELF-WASM","Leaks"}
files = [p for p in sorted(rt.glob("*.cpp")) if p.stem not in skip]
files += [p for p in sorted(rt.glob("*.mm")) if p.stem != "Leaks"]
files.append(src / "stdlib/public/CompatibilityOverride/CompatibilityOverride.cpp")
files += sorted((src / "lib/Demangling").glob("*.cpp"))
files += sorted((src / "stdlib/public/LLVMSupport").glob("*.cpp"))
files += [p for p in sorted((src / "lib/Threading").glob("*.cpp")) if p.stem in ("Pthreads", "ThreadSanitizer")]
for p in sorted((src / "stdlib/public/stubs").glob("*.cpp")):
    if any(s in p.stem for s in ("OptionalParsing", "Android", "Windows")):
        continue
    files.append(p)
files += sorted((src / "stdlib/public/stubs").glob("*.mm"))
gen = src / "gen/stubs/SwiftNativeNSXXXBase.mm"
if gen.is_file():
    files.append(gen)
files += sorted((src / "stdlib/public/stubs/Unicode").glob("*.cpp"))
defs = ["-DswiftCore_EXPORTS","-DSWIFT_RUNTIME","-DSWIFT_OBJC_INTEROP=1","-DSWIFT_THREADING_PTHREADS","-DSWIFT_LIBRARY_EVOLUTION=1","-DSWIFT_TARGET_LIBRARY_NAME=swiftRuntimeCore","-DSWIFT_STDLIB_HAS_TYPE_PRINTING=1","-DSWIFT_ENABLE_REFLECTION=1","-DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1","-DSWIFT_RUNTIME_ENABLE_LEAK_CHECKER=0","-DSWIFT_STDLIB_HAS_DLADDR","-DSWIFT_STDLIB_OVERRIDABLE_RETAIN_RELEASE=1","-DSWIFT_STDLIB_HAS_STDIN","-DSWIFT_STDLIB_HAS_ENVIRON","-DSWIFT_STDLIB_HAS_LOCALE","-DSWIFT_STDLIB_ENABLE_UNICODE_DATA=1","-DSWIFT_INLINE_NAMESPACE=__runtime","-DSWIFT_RUNTIME_OS_VERSIONING=0","-DNDEBUG"]
inc = ["-I"+str(src/"include"),"-I"+str(src/"stdlib/include"),"-I"+str(src/"stdlib/public/SwiftShims"),"-I"+str(src/"stdlib/public"),"-I"+str(rt),"-I"+str(root/"userland/sdk/objc/include")]
fl = ["-target","x86_64-macos","-std=c++20","-O2","-fno-exceptions","-fno-rtti","-ffunction-sections","-fno-sanitize=all","-w","-c"]
bad = 0
for f in files:
    o = out / (f.parent.name + "_" + f.stem + ".o")
    lang = ["-x","objective-c++","-fobjc-runtime=macosx-14.0"] if f.suffix == ".mm" else []
    r = subprocess.run([str(zig),"c++",*lang,*fl,*defs,*inc,str(f),"-o",str(o)], capture_output=True, text=True)
    if r.returncode:
        bad += 1
        err = [ln for ln in ((r.stderr or "") + (r.stdout or "")).splitlines() if "error:" in ln][:2]
        print(f.name + ": " + (err[0][:200] if err else "falhou"))
print("runtime: %d arquivos, %d erros" % (len(files), bad))
sys.exit(1 if bad else 0)
PY
fi

link_one() {
    name=$1
    shift
    "$zig" c++ -target x86_64-macos -shared -o "$dest/$name.dylib" \
        -Wl,-install_name,/usr/lib/swift/$name.dylib -Wl,-undefined,dynamic_lookup "$@"
    if grep -a -q 'STUB mOS libswift' "$dest/$name.dylib"; then
        echo "$name ainda tem a marca de stub" >&2
        exit 1
    fi
    echo "$name: $(wc -c < "$dest/$name.dylib") bytes"
}

echo "==> libswiftCore"
# shellcheck disable=SC2046
link_one libswiftCore "$objc/swiftCore.o" $(find "$objc/runtime" -name '*.o')
if ! grep -a -q '_swift_allocObject' "$dest/libswiftCore.dylib"; then
    echo "libswiftCore sem _swift_allocObject" >&2
    exit 1
fi

echo "==> cabeçalhos do overlay"
rm -rf "$inc"
mkdir -p "$(dirname "$inc")"
cp -a "$root/.tools/zig-x86_64-linux-0.17.1/lib/libc/include/any-darwin-any" "$inc" 2>/dev/null || \
    cp -a "$(dirname "$zig")/lib/libc/include/any-darwin-any" "$inc"
python3 - "$inc" << 'PY'
import os, sys
root = sys.argv[1]
for dp, dns, fns in os.walk(root):
    for fn in fns:
        p = os.path.join(dp, fn)
        try:
            b = open(p, encoding="utf-8", errors="surrogateescape").read()
        except Exception:
            continue
        if "__has_feature(modules)" in b:
            open(p, "w", encoding="utf-8", errors="surrogateescape").write(b.replace("__has_feature(modules)", "0"))
PY
cp -a "$root/userland/sdk/openswiftui/darwin/." "$inc/" || true
cp "$sdk/sdk/module.modulemap" "$inc/module.modulemap"
mkdir -p "$inc/os" "$inc/Metal"
cp "$sdk/sdk/os/log.h" "$inc/os/log.h"
cp -a "$sdk/sdk/Foundation" "$inc/"
cp -a "$sdk/sdk/UniformTypeIdentifiers" "$inc/"
cp -a "$root/userland/sdk/openswiftui/importer/CoreFoundation" "$inc/"
cp -a "$root/userland/sdk/include/CoreGraphics" "$inc/"
cp -a "$root/userland/sdk/include/Metal/." "$inc/Metal/"
python3 - "$inc/dispatch/workloop.h" << 'PY'
import pathlib, sys
p = pathlib.Path(sys.argv[1])
t = p.read_text()
if "mOS: workgroup" in t:
    sys.exit(0)
decl = "dispatch_workloop_set_os_workgroup(dispatch_workloop_t workloop,"
i = t.find(decl)
s = t.rfind("API_AVAILABLE", 0, i)
e = t.find(";", i) + 1
if i < 0 or s < 0 or e <= 0:
    sys.exit(0)
p.write_text(t[:s] + "#if defined(__OS_WORKGROUP_OBJECT__) /* mOS: workgroup */\n" + t[s:e] + "\n#endif" + t[e:])
PY
python3 - "$root" << 'PY'
from pathlib import Path
root = Path("/agent/repos/mOS") if False else Path(__import__("sys").argv[1])
md = (root / "userland/sdk/openswiftui/swift/MOSDarwin/MOSDarwin.swift").read_text()
(root / "build/swift/objc/overlays-src/Darwin.swift").write_text(
    md.replace("@_exported import MOSDarwin", "@_exported import Darwin").replace("MOSDarwin.", "Darwin."))
cg = (root / "userland/sdk/openswiftui/swift/CoreGraphics/CGFloat.swift").read_text()
(root / "build/swift/objc/overlays-src/CGFloat.swift").write_text(
    cg.replace("@_exported import MOSLibm", "@_exported import CoreGraphics\nimport Darwin"))
PY

ov() {
    name=$1
    shift
    echo "==> overlay $name"
    # shellcheck disable=SC2068
    "$fe" -frontend -c -target x86_64-apple-macos26 -resource-dir "$res" \
        -module-name "$name" -parse-as-library -swift-version 5 -O \
        -enable-library-evolution -I "$objc" -I "$out" -module-cache-path "$objc/modcache" \
        -I "$shims" -disable-implicit-concurrency-module-import -disable-implicit-string-processing-module-import \
        -Xcc "-isystem$inc" -Xcc -Wno-nullability-completeness -Xcc -DMOS_SWIFT_OBJC=1 \
        -emit-module-path "$out/$name.swiftmodule" -o "$out/$name.o" \
        -module-link-name "swift$name" -autolink-force-load "$@"
}

ov Darwin -import-underlying-module "$objc/overlays-src/Darwin.swift" "$sdk"/Darwin/*.swift
ov ObjectiveC -import-underlying-module "$sdk"/ObjectiveC/*.swift
ov CoreFoundation -import-underlying-module "$sdk"/CoreFoundation/*.swift
ov CoreGraphics -import-underlying-module "$objc/overlays-src/CGFloat.swift" "$sdk"/CoreGraphics/*.swift
ov os -import-underlying-module "$sdk"/os/*.swift
ov Dispatch -import-underlying-module "$sdk"/Dispatch/*.swift
"$zig" cc -target x86_64-macos -O2 -c "$sdk/Metal/mos_mtl_modify.c" -o "$out/mos_mtl_modify.o"
ov Metal -import-underlying-module "$sdk/Metal/MTLBuffer.swift"
for m in _Builtin_float simd XPC QuartzCore IOKit OSLog Spatial CoreAudio CoreImage CoreMIDI QuickLookUI; do
    printf '%s\n' "// módulo vazio: só o _swift_FORCE_LOAD" > "$objc/overlays-src/$m.swift"
    ov "$m" "$objc/overlays-src/$m.swift"
done

echo "==> _Concurrency"
ccdir="$src/stdlib/public/Concurrency"
okc=0
for f in ../CompatibilityOverride/CompatibilityOverride.cpp Actor.cpp AsyncLet.cpp Clock.cpp ConcurrencyHooks.cpp \
    GlobalExecutor.cpp EmbeddedSupport.cpp Error.cpp ExecutorBridge.cpp ExecutorChecks.cpp Task.cpp TaskAlloc.cpp \
    TaskStatus.cpp TaskGroup.cpp TaskLocal.cpp ThreadingError.cpp TracingSignpost.cpp AsyncStream.cpp ExecutorImpl.cpp \
    DispatchGlobalExecutor.cpp CFExecutor.cpp; do
    base=$(basename "$f" .cpp)
    dir=$out
    if [ "$base" = ThreadingError ]; then dir=$out/dylib-only; mkdir -p "$dir"; fi
    "$zig" c++ -target x86_64-macos -std=c++20 -O2 -fno-exceptions -fno-rtti -ffunction-sections -fno-sanitize=all \
        -fswift-async-fp=always -w -c \
        -Dswift_Concurrency_EXPORTS -DSWIFT_RUNTIME -DSWIFT_OBJC_INTEROP=1 -DSWIFT_THREADING_PTHREADS \
        -DSWIFT_LIBRARY_EVOLUTION=1 -DSWIFT_TARGET_LIBRARY_NAME=swift_Concurrency -DSWIFT_STDLIB_HAS_TYPE_PRINTING=1 \
        -DSWIFT_ENABLE_REFLECTION=1 -DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1 -DSWIFT_STDLIB_HAS_DLADDR \
        -DSWIFT_STDLIB_HAS_STDIN -DSWIFT_STDLIB_HAS_ENVIRON -DSWIFT_STDLIB_HAS_LOCALE \
        -DSWIFT_INLINE_NAMESPACE=__runtime -DSWIFT_RUNTIME_OS_VERSIONING=0 -DSWIFT_CONCURRENCY_USES_DISPATCH=1 \
        -D__STDC_WANT_LIB_EXT1__=1 -DNDEBUG \
        -I"$src/include" -I"$src/stdlib/include" -I"$src/stdlib/public/SwiftShims" -I"$src/stdlib/public" -I"$ccdir" \
        -I"$root/userland/sdk/objc/include" \
        "$ccdir/$f" -o "$dir/_Concurrency_$base.o"
    okc=$((okc + 1))
done
"$zig" c++ -target x86_64-macos -std=c++20 -O2 -fno-exceptions -fno-rtti -ffunction-sections -fno-sanitize=all \
    -fswift-async-fp=always -w -c \
    -Dswift_Concurrency_EXPORTS -DSWIFT_RUNTIME -DSWIFT_OBJC_INTEROP=1 -DSWIFT_THREADING_PTHREADS \
    -DSWIFT_LIBRARY_EVOLUTION=1 -DSWIFT_TARGET_LIBRARY_NAME=swift_Concurrency -DNDEBUG \
    -DSWIFT_CONCURRENCY_USES_DISPATCH=1 \
    -I"$src/include" -I"$src/stdlib/include" -I"$src/stdlib/public/SwiftShims" -I"$src/stdlib/public" -I"$ccdir" \
    -I"$root/userland/sdk/objc/include" \
    "$sdk/Concurrency/mos_executor.cpp" -o "$out/_Concurrency_mos_executor.o"
for g in Task+init.swift.gyb TaskGroup+addTask.swift.gyb Task+immediate.swift.gyb; do
    python3 "$src/utils/gyb.py" -o "$objc/concurrency-src/${g%.gyb}" "$ccdir/$g"
done
python3 - "$ccdir" "$objc/concurrency-src" << 'PY'
import re, pathlib, sys
cc, gen = map(pathlib.Path, sys.argv[1:3])
m = re.search(r"set\(SWIFT_RUNTIME_CONCURRENCY_SWIFT_SOURCES(.*?)\)", (cc / "CMakeLists.txt").read_text(), re.S)
files = [str(cc / t) for t in m.group(1).split() if t.endswith(".swift")]
for extra in ("DispatchExecutor.swift", "CFExecutor.swift", "ExecutorImpl.swift"):
    files.append(str(cc / extra))
files += [str(p) for p in gen.glob("*.swift")]
src = cc.parents[2]
avail = []
for line in (src / "utils/availability-macros.def").read_text().splitlines():
    if line and not line.startswith("#"):
        avail += ["-define-availability", line.strip()]
pathlib.Path("/tmp/mos-conc-files.txt").write_text("\n".join(files) + "\n")
pathlib.Path("/tmp/mos-conc-avail.txt").write_text("\n".join(avail) + "\n")
print("concurrency: %d fontes" % len(files))
PY
mapfile -t CFILES < /tmp/mos-conc-files.txt
mapfile -t CAVAIL < /tmp/mos-conc-avail.txt
cfargs=()
for x in IsolatedAny AllowUnsafeAttribute Extern Macros FreestandingMacros BitwiseCopyable Lifetimes LifetimeDependence \
    SuppressedAssociatedTypesWithDefaults AddressableParameters AddressableTypes ValueGenerics; do
    cfargs+=(-enable-experimental-feature "$x")
done
"$fe" -frontend -c -target x86_64-apple-macos26 -resource-dir "$res" -module-name _Concurrency \
    -D SWIFT_CONCURRENCY_USES_DISPATCH -parse-stdlib -parse-as-library -swift-version 5 -O \
    -enable-library-evolution -strict-memory-safety -enforce-exclusivity=unchecked -swift-async-frame-pointer=always \
    -disable-implicit-concurrency-module-import -disable-implicit-string-processing-module-import \
    -I "$objc" -I "$out" -I "$shims" -I "$ccdir/InternalShims" -module-cache-path "$objc/modcache" \
    -Xcc "-isystem$inc" -Xcc -DMOS_SWIFT_OBJC=1 -Xcc -Wno-nullability-completeness \
    -module-link-name swift_Concurrency -autolink-force-load "${cfargs[@]}" "${CAVAIL[@]}" \
    -emit-module-path "$out/_Concurrency.swiftmodule" -o "$out/_Concurrency.o" "${CFILES[@]}"

echo "==> dylibs"
link_one libswiftDarwin "$out/Darwin.o" "$out/_Builtin_float.o"
link_one libswiftObjectiveC "$out/ObjectiveC.o"
link_one libswiftCoreFoundation "$out/CoreFoundation.o"
link_one libswiftCoreGraphics "$out/CoreGraphics.o"
link_one libswiftDispatch "$out/Dispatch.o"
link_one libswiftMetal "$out/Metal.o" "$out/mos_mtl_modify.o"
# shellcheck disable=SC2046
link_one libswift_Concurrency "$out/_Concurrency.o" $(find "$out" -name '_Concurrency_*.o')
for m in XPC QuartzCore IOKit OSLog Spatial CoreAudio CoreImage CoreMIDI QuickLookUI simd; do
    link_one "libswift$m" "$out/$m.o"
done
echo "libswift real em $dest"
if ! grep -a -q '_objc_addLoadImageFunc' "$dest/libswiftCore.dylib"; then
    echo "libswiftCore não importa _objc_addLoadImageFunc" >&2
    exit 1
fi

# Símbolos que o WhatsApp 2.26.45.13 importa e esta cadeia não define.
# Cada um é `ret; int3`. O relatório do machotest marca "Stub".
gaps="$root/tools/wa-swift-stubs.txt"
if [ -f "$gaps" ]; then
    echo "==> stubs que a stdlib não emitiu"
    python3 - "$dest" "$gaps" "$objc" "$zig" << 'PY'
import pathlib, subprocess, sys
dest, gaps, objc, zig = map(pathlib.Path, sys.argv[1:5])
nm = None
root = dest.parents[6]
for c in (root / ".tools/swift-6.4.2/usr/bin/llvm-nm",):
    if c.is_file():
        nm = c
        break
if nm is None:
    import shutil
    w = shutil.which("llvm-nm")
    if not w:
        sys.stderr.write("sem llvm-nm; stubs do WhatsApp não entram\n")
        sys.exit(0)
    nm = pathlib.Path(w)
have = set()
for p in dest.glob("*.dylib"):
    r = subprocess.run([str(nm), "-gU", str(p)], capture_output=True)
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            have.add(parts[-1].decode())
want = [ln.strip() for ln in gaps.read_text().splitlines() if ln.strip() and not ln.startswith("#")]
miss = [n for n in want if n not in have]
if not miss:
    print("stubs: a cadeia já exporta a lista")
    sys.exit(0)
asm = objc / "wa-stubs.S"
lines = [".text"]
for n in miss:
    lines.append(".globl " + n)
    lines.append(n + ":")
    lines.append("    ret")
    lines.append("    int3")
asm.write_text("\n".join(lines) + "\n")
o = objc / "wa-stubs.o"
r = subprocess.run([str(zig), "cc", "-target", "x86_64-macos", "-c", str(asm), "-o", str(o)], capture_output=True, text=True)
if r.returncode:
    sys.stderr.write(r.stderr[:500] + "\n")
    sys.exit(1)
core = dest / "libswiftCore.dylib"
objs = [str(objc / "swiftCore.o"), str(o)]
objs += [str(p) for p in sorted((objc / "runtime").glob("*.o"))]
r = subprocess.run([str(zig), "c++", "-target", "x86_64-macos", "-shared", "-o", str(core),
                    "-Wl,-install_name,/usr/lib/swift/libswiftCore.dylib", "-Wl,-undefined,dynamic_lookup",
                    *objs], capture_output=True, text=True)
if r.returncode:
    sys.stderr.write(r.stderr[:800] + "\n")
    sys.exit(1)
print("stubs ligados em libswiftCore: %d" % len(miss))
PY
fi
