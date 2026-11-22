# mOS — zig_link <zig> <cc|c++> -target <alvo> -shared -o <saída> [args...]: liga uma dylib Mach-O com o zig e, só no
# macOS, se o zig falhar, repete o mesmo link com o clang do Xcode. O linker Mach-O do zig 0.17.1 (o do Homebrew e o
# tarball oficial) cai com SIGSEGV quando roda no macOS ao ligar os overlays Swift grandes (libswiftFoundation, AppKit,
# NaturalLanguage, SwiftUI...); no Linux nada muda. Se os dois falharem, a saída é apagada: um .dylib vazio seria lido
# pelos links seguintes como TBD ("failed to parse TBD file: NotLibStub").
# Uso: . tools/zig-link.sh; zig_link "$ZIG" cc -target x86_64-macos -shared -o out.dylib -Wl,... objs...
zig_link() {
    local zig=$1 drv=$2; shift 2
    "$zig" "$drv" "$@" && return 0
    local rc=$? out="" tgt="" a=() prev=""
    if [ "$(uname -s)" != Darwin ] || ! xcrun --find clang >/dev/null 2>&1; then return $rc; fi
    for x in "$@"; do
        if [ "$prev" = -target ]; then tgt=$x; prev=""; continue; fi
        if [ "$prev" = -o ]; then out=$x; fi
        prev=$x
        [ "$x" = -target ] && continue
        a+=("$x")
    done
    local arch=x86_64
    case "$tgt" in aarch64-*|arm64-*) arch=arm64 ;; esac
    local cl=clang
    [ "$drv" = c++ ] && cl=clang++
    echo "zig_link: o zig falhou ($rc) em ${out:-?}; repetindo com o $cl do Xcode" >&2
    if xcrun "$cl" -arch "$arch" -mmacosx-version-min=11 -w -Wl,-not_for_dyld_shared_cache "${a[@]}"; then return 0; fi
    rc=$?
    [ -n "$out" ] && rm -f "$out"
    return $rc
}
