# mOS — escada da auto-hospedagem (docs/auto-hospedagem.md, fase 1): até onde o Zig chega dentro do mOS.
# Roda no boot pelo stage-zig.ps1; cada linha vai para o log serial com o prefixo "zig:".
Z=/usr/local/zig/zig
S=/mos-src
MAX=$(cat $S/degrau-max 2>/dev/null || echo 6)
MIN=$(cat $S/degrau-min 2>/dev/null || echo 1)
export HOME=/tmp-zig ZIG_GLOBAL_CACHE_DIR=/tmp-zig/cache ZIG_LOCAL_CACHE_DIR=/tmp-zig/cache
mkdir -p /tmp-zig/cache /tmp-zig/out
cd /tmp-zig
log() { echo "zig: $*"; }
mostra() { local k=0; while IFS= read -r l; do k=$((k+1)); [ $k -le 40 ] && echo "zig:   ${l:0:200}"; done < "$1"; }
t0() { T0=$(date +%s); }
dt() { echo $(( $(date +%s) - T0 ))s; }
degrau() {   # degrau <n> <nome> <comando...>
    local n=$1 nome=$2; shift 2
    [ "$n" -gt "$MAX" ] && return 0
    [ "$n" -lt "$MIN" ] && return 0
    log "degrau $n ($nome): inicio"
    t0
    "$@" > /tmp-zig/out/d$n.txt 2>&1
    local rc=$?
    log "degrau $n ($nome): rc=$rc em $(dt)"
    mostra /tmp-zig/out/d$n.txt
    [ $rc -eq 0 ] || { log "parou no degrau $n"; log "fim"; exit 0; }
}

log "inicio (max=$MAX)"
degrau 1 version $Z version

printf '#include <stdio.h>\nint main(void){printf("ola do zig no mOS\\n");return 0;}\n' > hello.c
d2() { $Z cc -target x86_64-macos hello.c -o hello && ./hello; }
degrau 2 hello d2

d3() { $Z cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -fpie -mno-red-zone \
          -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header -I$S/userland/libsystem/kshim \
          -c $S/userland/libsystem/libc.c -o libc.o && ls -l libc.o; }
degrau 3 "libSystem libc.c" d3

d4() {
    cd $S || return 1
    local L=/usr/local/zig/lib/libunwind UW=/tmp-zig/uw f x
    mkdir -p $UW
    local fl="--target=x86_64-apple-macos11-elf -O2 -g0 -fPIC -mno-red-zone -fno-stack-protector -funwind-tables -D_LIBUNWIND_IS_NATIVE_ONLY -DNDEBUG -w -I$L/include -isystem /usr/local/zig/lib/include -isystem /usr/local/zig/lib/libc/include/any-darwin-any"
    for f in libunwind.cpp UnwindLevel1.c UnwindLevel1-gcc-ext.c UnwindRegistersSave.S UnwindRegistersRestore.S; do
        case $f in *.cpp) x="-std=c++17 -fno-exceptions -fno-rtti -nostdinc++";; *.S) x="-U__APPLE__ -U__MACH__";; *) x="-std=c11";; esac
        $Z clang $fl $x -c $L/src/$f -o $UW/$f.o || { cd /tmp-zig; return 1; }
        echo "uw $f: $(sha256sum < $UW/$f.o | cut -c1-16) host $(sha256sum < $S/$f.o.host | cut -c1-16)"
    done
    $Z cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -static -no-pie \
        -fpie -mno-red-zone -w -Iuserland/libsystem/kshim -Wl,--image-base=0x400000100000 -Wl,-e,cx_start -Wl,--no-gc-sections \
        $(cat libsystem-fontes.txt) $UW/libunwind.cpp.o $UW/UnwindLevel1.c.o $UW/UnwindLevel1-gcc-ext.c.o \
        $UW/UnwindRegistersSave.S.o $UW/UnwindRegistersRestore.S.o -o /tmp-zig/libSystem.mos
    local rc=$?
    cd /tmp-zig
    [ $rc -eq 0 ] || return $rc
    echo "mOS : $(sha256sum < libSystem.mos)"
    echo "host: $(sha256sum < $S/libSystem-ref.mos)"
    [ "$(sha256sum < libSystem.mos)" = "$(sha256sum < $S/libSystem-ref.mos)" ] && echo "IGUAL byte a byte" || echo "DIFERENTE"
}
degrau 4 "libSystem inteiro" d4

d5() {   # mesmas flags e mesma ordem de fontes do build.ps1; embute o build/libSystem.mos do host
    cd $S || return 1
    $Z cc -target x86_64-uefi-msvc -O2 -g0 -s -Wl,/Brepro -ffreestanding -fno-stack-protector -fno-stack-check -mno-stack-arg-probe \
        -mno-red-zone -fshort-wchar -fno-sanitize=all -fno-strict-aliasing -nostdlib -Ikernel/include -I. -std=gnu11 \
        -w $(cat kernel-fontes.txt) -o /tmp-zig/BOOTX64.EFI
    local rc=$?
    cd /tmp-zig
    [ $rc -eq 0 ] || return $rc
    echo "mOS : $(sha256sum < BOOTX64.EFI)"
    echo "host: $(sha256sum < $S/BOOTX64-host.EFI)"
    [ "$(sha256sum < BOOTX64.EFI)" = "$(sha256sum < $S/BOOTX64-host.EFI)" ] && echo "IGUAL byte a byte" || echo "DIFERENTE"
}
degrau 5 kernel d5

d6() {   # cada objeto do libSystem contra o hash do host (libsystem-objs.txt): mostra só os que diferem
    cd $S || return 1
    mkdir -p /tmp-zig/obj
    local f h o got dif=0 tot=0
    while read -r f h; do
        o=/tmp-zig/obj/${f##*/}.o
        $Z cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
            -fpie -mno-red-zone -w -Iuserland/libsystem/kshim -c $f -o $o 2>/dev/null
        got=$(sha256sum < $o 2>/dev/null | cut -d' ' -f1)
        tot=$((tot+1))
        [ "$got" = "$h" ] || { dif=$((dif+1)); echo "difere: $f ($(wc -c < $o 2>/dev/null) bytes)"; }
    done < libsystem-objs.txt
    cd /tmp-zig
    echo "objetos: $tot, diferentes: $dif"
}
degrau 6 "objetos do libSystem" d6

d7() {   # um objeto pequeno em base64 no log, para comparar byte a byte com o do host
    cd $S || return 1
    $Z cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
        -fpie -mno-red-zone -w -Iuserland/libsystem/kshim -c userland/libsystem/cfencoding.c -o /tmp-zig/cfenc.o || return 1
    cd /tmp-zig
    base64 -w 76 cfenc.o > /tmp-zig/cfenc.b64
}
degrau 7 "base64 de um objeto" d7
[ -f /tmp-zig/cfenc.b64 ] && while IFS= read -r l; do echo "zig:b64 $l"; done < /tmp-zig/cfenc.b64

d8() {   # onde o conteúdo some: clang direto, zig cc (com cache) e o objeto que ficou no cache
    cd $S || return 1
    local fl="-target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-builtin -nostdlib -w -Iuserland/libsystem/kshim"
    $Z clang $fl -c userland/libsystem/cfencoding.c -o /tmp-zig/d8-clang.o; echo "clang rc=$?"
    $Z cc $fl -c userland/libsystem/cfencoding.c -o /tmp-zig/d8-cc.o; echo "cc rc=$?"
    for o in /tmp-zig/d8-clang.o /tmp-zig/d8-cc.o $(find /tmp-zig/cache -name 'cfencoding*.o' 2>/dev/null | head -3); do
        echo "$o $(wc -c < $o) bytes: $(od -A n -t x1 -N 16 $o | tr -s ' ')"
    done
    printf 'abcdefgh' > /tmp-zig/d8-w.bin; echo "write: $(od -A n -t x1 -N 8 /tmp-zig/d8-w.bin | tr -s ' ')"
    cd /tmp-zig
}
degrau 8 "onde o conteúdo some" d8

log "chegou ao topo"
log "fim"
