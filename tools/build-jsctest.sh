#!/usr/bin/env bash
# mOS — compila o JavaScriptCore.framework e o teste userland/tests-jsc (JSCTest, linha de comando) para rodar no mOS.
# uso: tools/build-jsctest.sh <zig> <saída>    depois copie JSCTest para a imagem e rode: a saída tem "jsc: N/M ok"
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ZIG="$1"; OUT="$2"
mkdir -p "$OUT/obj"
for f in $(ls "$ROOT"/kernel/web/js_*.c | grep -v /js_dom.c) "$ROOT/kernel/web/util.c" "$ROOT/userland/frameworks/JavaScriptCore/jsc_api.c"; do
    "$ZIG" cc -target x86_64-macos -O2 -std=gnu11 -w -fno-builtin -fno-stack-protector -fvisibility=hidden \
        -I"$ROOT/kernel/include" -I"$ROOT" -c "$f" -o "$OUT/obj/$(basename "$f" .c).o"
done
"$ZIG" cc -target x86_64-macos -O2 -c -x objective-c -I"$ROOT/userland/sdk/include" -Wno-objc-root-class -Wno-objc-method-access \
    "$ROOT/userland/frameworks/JavaScriptCore/JSC.m" -o "$OUT/obj/JSC.o"
"$ZIG" cc -target x86_64-macos -shared "$OUT"/obj/*.o -o "$OUT/JavaScriptCore" \
    -Wl,-install_name,/System/Library/Frameworks/JavaScriptCore.framework/Versions/A/JavaScriptCore -Wl,-undefined,dynamic_lookup
cp "$OUT/JavaScriptCore" "$OUT/JavaScriptCore-link.dylib"   # o zig só aceita a dylib de entrada com extensão
"$ZIG" cc -target x86_64-macos -O1 -x objective-c -I"$ROOT/userland/sdk/include" -Wno-objc-root-class -Wno-objc-method-access \
    "$ROOT/userland/tests-jsc/main.m" -x none "$OUT/JavaScriptCore-link.dylib" -o "$OUT/JSCTest" -Wl,-undefined,dynamic_lookup
echo "jsctest: $OUT/JSCTest (e o framework em $OUT/JavaScriptCore)"
