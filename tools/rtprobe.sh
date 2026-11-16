#!/bin/bash
# mOS — probe do runtime Swift na VM (docs/swift-runtime-frente.md): Concurrency sobre a libdispatch, Dispatch,
# Observation, Synchronization, os, System e Regex/RegexBuilder. Compila tools/rtprobe/main.swift contra build/swift/objc,
# põe o binário como serviço do launchd numa cópia da esp de um build e gera uma imagem de teste.
#   tools/rtprobe.sh <OutDir do tools/build.sh> <pasta de saída>
# No serial: "RT-OK n/m" e um "RT-FAIL <item>" por falha.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
src=${1:?uso: tools/rtprobe.sh <OutDir do build> <saída>}
out=${2:?uso: tools/rtprobe.sh <OutDir do build> <saída>}
src=$(cd "$src" && pwd)
mkdir -p "$out"
out=$(cd "$out" && pwd)
zig=${MOS_ZIG:-$(command -v zig)}
fe=${SWIFTC:-$root/.tools/swift-6.4.2/usr/bin/swift-frontend}
objc="$root/build/swift/objc"
dy="$objc/dylib/usr/lib/swift"
for f in "$src/esp/EFI/BOOT/BOOTX64.EFI" "$src/mkimg" "$dy/libswiftSynchronization.dylib" "$dy/libswiftRegexBuilder.dylib" "$fe"; do
    [ -e "$f" ] || { echo "rtprobe: falta $f" >&2; exit 1; }
done
res=/tmp/mos-swift-resdir
if [ ! -d "$res/shims" ]; then
    tc=$(cd "$(dirname "$fe")/../lib/swift" && pwd)
    mkdir -p "$res/shims"
    [ -d "$tc/clang" ] && ln -sfn "$tc/clang" "$res/clang"
    find "$tc/shims" -maxdepth 1 -type f ! -name 'module.modulemap' -exec cp {} "$res/shims/" \;
fi

echo "==> rtprobe (Swift)"
rm -rf "$out/mc"
"$fe" -frontend -c -target x86_64-apple-macos26 -resource-dir "$res" -module-name rtprobe -swift-version 5 -O \
    -parse-as-library -I "$objc" -I "$objc/overlays" -I "$root/.tools/swift-src/stdlib/public/SwiftShims/swift/shims" \
    -module-cache-path "$out/mc" -Xcc "-isystem$objc/sdk/include" -Xcc -nostdlibinc -Xcc -DMOS_SWIFT_OBJC=1 \
    -Xcc -Wno-nullability-completeness "$root/tools/rtprobe/main.swift" -o "$out/rtprobe.o"
libs=()
for l in libswiftCore libswift_Concurrency libswiftDispatch libswiftDarwin libswiftObjectiveC libswiftos libswiftObservation \
         libswiftSynchronization libswift_StringProcessing libswift_RegexParser libswiftRegexBuilder; do libs+=("$dy/$l.dylib"); done
"$zig" cc -target x86_64-macos -o "$out/rtprobe" "$out/rtprobe.o" "${libs[@]}" \
    "$objc/dylib/System/Library/Frameworks/System.framework/Versions/A/System" -Wl,-undefined,dynamic_lookup

echo "==> esp de teste"
esp="$out/esp"
rm -rf "$esp"
cp -a "$src/esp" "$esp"
rm -rf "$esp/mOS/Aplicativos"/*
cp -a "$objc/dylib/." "$esp/mOS/"
mkdir -p "$esp/mOS/Library/mOS-teste" "$esp/mOS/Library/LaunchDaemons"
for prog in rtprobe; do
    cp "$out/$prog" "$esp/mOS/Library/mOS-teste/$prog"
    cat > "$esp/mOS/Library/LaunchDaemons/com.mos.teste.$prog.plist" << PL
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
	<string>com.mos.teste.$prog</string>
	<key>Program</key>
	<string>/Library/mOS-teste/$prog</string>
	<key>RunAtLoad</key>
	<true/>
</dict>
</plist>
PL
done
printf '1\n' > "$esp/mOS/Sistema/Biblioteca/stderr-serial"

# overlay de teste (tools/overlay-teste.ps1): só o marcador que pula o assistente do Live
ovt="$out/overlay-teste"
rm -rf "$ovt"
mkdir -p "$ovt/mOS/Sistema/Biblioteca/Preferencias"
printf 'imagem de teste: o live abre na sessao do Visitante, sem o assistente\n' \
    > "$ovt/mOS/Sistema/Biblioteca/Preferencias/com.mos.teste.sem-assistente"

bytes=$(du -sb "$esp" | cut -f1)
mib=$(( (bytes * 5 / 4 + 1048575) / 1048576 ))
[ "$mib" -lt 128 ] && mib=128
mib=$(( (mib + 15) / 16 * 16 ))
"$src/mkimg" "$out/mOS.img" "$esp" "$mib" --test-overlay "$ovt"
echo "OK: $out/mOS.img (${mib} MiB)"
cat << EOF
Para rodar (OVMF do Ubuntu; o QEMU não desliga sozinho; TMPDIR fora do /tmp, que recebe a sobreposição do
snapshot; com 2 núcleos o boot travou nesta máquina, com 1 passou):
  cp /usr/share/OVMF/OVMF_VARS_4M.fd $out/vars.fd
  TMPDIR=$out timeout 240 qemu-system-x86_64 -accel kvm -machine q35 -m 512 -cpu host -smp 1 \\
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \\
    -drive if=pflash,format=raw,file=$out/vars.fd -drive file=$out/mOS.img,format=raw,if=ide,snapshot=on \\
    -display none -serial file:$out/serial.log -nic none -no-reboot
  grep -a 'RT-' $out/serial.log
EOF
