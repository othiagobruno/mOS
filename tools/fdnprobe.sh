#!/bin/bash
# mOS — teste do Foundation Swift real na VM (docs/swift-runtime-real.md). Compila tools/fdnprobe/main.swift contra
# build/swift/objc (tools/build-libswift-real.sh + tools/build-swift-foundation-objc.sh), põe o binário como serviço do
# launchd numa cópia da esp de um build e gera uma imagem de teste (overlay sem o assistente, stderr no serial).
#   tools/fdnprobe.sh <OutDir do tools/build.sh> <pasta de saída>
# Depois, no serial: "FDN-OK n/m" e "RX-OK n/m" (o Regex fica num binário à parte), e um "FDN-FAIL <item>" ou
# "RX-FAIL <item>" por item que falhou. Exemplo de QEMU no fim.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
src=${1:?uso: tools/fdnprobe.sh <OutDir do build> <saída>}
out=${2:?uso: tools/fdnprobe.sh <OutDir do build> <saída>}
src=$(cd "$src" && pwd)
mkdir -p "$out"
out=$(cd "$out" && pwd)
zig=${MOS_ZIG:-$(command -v zig)}
fe=${SWIFTC:-$root/.tools/swift-6.4.2/usr/bin/swift-frontend}
objc="$root/build/swift/objc"
dy="$objc/dylib/usr/lib/swift"
for f in "$src/esp/EFI/BOOT/BOOTX64.EFI" "$src/mkimg" "$dy/libswiftFoundation.dylib" "$fe"; do
    [ -e "$f" ] || { echo "fdnprobe: falta $f" >&2; exit 1; }
done
res=/tmp/mos-swift-resdir
if [ ! -d "$res/shims" ]; then
    tc=$(cd "$(dirname "$fe")/../lib/swift" && pwd)
    mkdir -p "$res/shims"
    [ -d "$tc/clang" ] && ln -sfn "$tc/clang" "$res/clang"
    find "$tc/shims" -maxdepth 1 -type f ! -name 'module.modulemap' -exec cp {} "$res/shims/" \;
fi

echo "==> fdnprobe (Swift)"
rm -rf "$out/mc"
"$fe" -frontend -c -target x86_64-apple-macos26 -resource-dir "$res" -module-name fdnprobe -swift-version 5 -O \
    -I "$objc" -I "$objc/overlays" -I "$root/.tools/swift-src/stdlib/public/SwiftShims/swift/shims" \
    -module-cache-path "$out/mc" -Xcc "-isystem$objc/sdk/include" -Xcc -nostdlibinc -Xcc -DMOS_SWIFT_OBJC=1 \
    -Xcc -Wno-nullability-completeness "$root/tools/fdnprobe/main.swift" -o "$out/fdnprobe.o"
"$zig" cc -target x86_64-macos -o "$out/fdnprobe" "$out/fdnprobe.o" \
    "$dy/libswiftCore.dylib" "$dy/libswiftFoundation.dylib" "$dy/libswift_StringProcessing.dylib" \
    "$dy/libswift_Concurrency.dylib" "$dy/libswiftDarwin.dylib" "$dy/libswiftObjectiveC.dylib" \
    "$dy/libswiftCoreFoundation.dylib" "$dy/libswiftDispatch.dylib" -Wl,-undefined,dynamic_lookup

echo "==> rxprobe (Regex do _StringProcessing)"
"$fe" -frontend -c -target x86_64-apple-macos26 -resource-dir "$res" -module-name rxprobe -swift-version 5 -O \
    -I "$objc" -I "$objc/overlays" -I "$root/.tools/swift-src/stdlib/public/SwiftShims/swift/shims" \
    -module-cache-path "$out/mc" -Xcc "-isystem$objc/sdk/include" -Xcc -nostdlibinc -Xcc -DMOS_SWIFT_OBJC=1 \
    -Xcc -Wno-nullability-completeness "$root/tools/fdnprobe/rx/main.swift" -o "$out/rxprobe.o"
"$zig" cc -target x86_64-macos -o "$out/rxprobe" "$out/rxprobe.o" \
    "$dy/libswiftCore.dylib" "$dy/libswift_StringProcessing.dylib" -Wl,-undefined,dynamic_lookup

echo "==> esp de teste"
esp="$out/esp"
rm -rf "$esp"
cp -a "$src/esp" "$esp"
rm -rf "$esp/mOS/Aplicativos"/*
cp -a "$objc/dylib/." "$esp/mOS/"
mkdir -p "$esp/mOS/Library/mOS-teste" "$esp/mOS/Library/LaunchDaemons"
for prog in fdnprobe rxprobe; do
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
  grep -a 'FDN-\\|RX-' $out/serial.log
EOF
