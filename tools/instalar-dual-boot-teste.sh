#!/bin/bash
# Teste de host do instalador dual boot (tools/instalar-dual-boot.py), sem root e sem tocar em disco nenhum:
# a ESP é uma pasta em /tmp com o conteúdo do Ubuntu, findmnt/lsblk/efibootmgr/update-grub são stubs no PATH
# (o efibootmgr guarda a NVRAM num arquivo), e a origem é uma árvore com manifesto no formato do mkimg.
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d /tmp/mos-dualboot-XXXX); trap 'rm -rf "$T"' EXIT
ESP=$T/esp; ORIG=$T/orig; SYS=$T/sys; BIN=$T/bin; NV=$T/nvram
mkdir -p "$ESP/EFI/BOOT" "$ESP/EFI/ubuntu" "$ORIG" "$SYS" "$BIN"
printf 'shim do ubuntu' > "$ESP/EFI/BOOT/BOOTX64.EFI"; printf 'shim' > "$ESP/EFI/ubuntu/shimx64.efi"; printf 'cfg' > "$ESP/EFI/ubuntu/grub.cfg"
printf 'BootOrder: 0000,0002,0001\nBoot0000* Ubuntu\tHD(1,GPT,x)/\\EFI\\UBUNTU\\SHIMX64.EFI\nBoot0001  OpenCore\tVenHw(x)\nBoot0002  Windows Boot Manager\tVenHw(x)\n' > "$NV"
cat > "$BIN/findmnt" <<S
#!/bin/sh
echo '{"filesystems":[{"source":"/dev/sda1","fstype":"vfat","target":"$ESP"}]}'
S
cat > "$BIN/lsblk" <<'S'
#!/bin/sh
echo '{"blockdevices":[{"name":"sda1","pkname":"sda","partn":1,"parttype":"c12a7328-f81f-11d2-ba4b-00a0c93ec93b","uuid":"DE1A-023D","type":"part"}]}'
S
cat > "$BIN/update-grub" <<S
#!/bin/sh
echo rodou >> "$T/update-grub.log"
S
cat > "$BIN/efibootmgr" <<S
#!/usr/bin/env python3
import sys, re
nv = "$NV"
L = open(nv).read().splitlines()
a = sys.argv[1:]
def grava(): open(nv, "w").write("\n".join(L) + "\n")
if "--create" in a:
    nums = [int(re.match(r"Boot([0-9A-F]{4})", l).group(1), 16) for l in L if l.startswith("Boot") and not l.startswith("BootOrder") and not l.startswith("BootNext")]
    n = "%04X" % (max(nums) + 1)
    lab = a[a.index("--label") + 1]; lo = a[a.index("--loader") + 1]
    L.append("Boot%s* %s\tHD(%s,GPT,y)/%s" % (n, lab, a[a.index("--part") + 1], lo))
    L[0] = "BootOrder: " + n + "," + L[0].split(": ")[1]
    grava()
elif "--bootorder" in a:
    L[0] = "BootOrder: " + a[a.index("--bootorder") + 1]; grava()
elif "--bootnext" in a:
    L.insert(1, "BootNext: " + a[a.index("--bootnext") + 1]); grava()
elif "--delete-bootnum" in a:
    n = a[a.index("--bootnum") + 1]
    L = [l for l in L if not l.startswith("Boot" + n)]
    L[0] = "BootOrder: " + ",".join(x for x in L[0].split(": ")[1].split(",") if x != n); grava()
print("\n".join(L))
S
chmod +x "$BIN"/*
export PATH="$BIN:$PATH"
python3 - "$ORIG" <<'P'
import hashlib, os, sys
r = sys.argv[1]
fs = {"/EFI/BOOT/BOOTX64.EFI": b"kernel do mOS" * 100, "/mOS/Sistema/Biblioteca/libSystem.mos": os.urandom(300000),
      "/mOS/Aplicativos/Notas.app/Contents/Info.plist": b"<plist/>", "/mOS/Sistema/Biblioteca/chave-atualizacao.pub": b"ab" * 32 + b"\n"}
for p, d in fs.items():
    os.makedirs(os.path.dirname(r + p), exist_ok=True); open(r + p, "wb").write(d)
ks = sorted(fs)
m = "mOS-MANIFESTO-1 %d %d 26.11-teste\n" % (len(ks), sum(len(fs[k]) for k in ks))
for k in ks: m += "%s %d %s\n" % (hashlib.sha256(fs[k]).hexdigest(), len(fs[k]), k)
m += "ASSINATURA-SHA256 %s\n" % hashlib.sha256(m.encode()).hexdigest()
os.makedirs(r + "/mOS/Sistema/Instalacao", exist_ok=True); open(r + "/mOS/Sistema/Instalacao/manifesto.txt", "w").write(m)
os.makedirs(r + "/mOS/Sistema/Biblioteca", exist_ok=True); open(r + "/mOS/Sistema/Biblioteca/estado-sistema", "w").write("LIVE\n")
P
I="python3 $R/tools/instalar-dual-boot.py --esp $ESP --origem $ORIG --raiz-sistema $SYS"
ok=0; n=0
passo() { n=$((n+1)); if eval "$2"; then ok=$((ok+1)); else echo "FALHA: $1"; fi; }
ubuntu() { (cd "$ESP" && sha256sum EFI/BOOT/BOOTX64.EFI EFI/ubuntu/shimx64.efi EFI/ubuntu/grub.cfg); }
U0=$(ubuntu)
passo "simulação não grava" "$I >/dev/null && [ ! -e $ESP/mOS ] && [ ! -e $ESP/EFI/mOS ] && [ ! -e $SYS/var ]"
passo "confirmação errada recusa sem gravar" "! $I --gravar --confirmo sda2 2>/dev/null && [ ! -e $ESP/mOS ]"
cp "$ORIG/mOS/Sistema/Biblioteca/libSystem.mos" "$T/lib.bak"; printf x >> "$ORIG/mOS/Sistema/Biblioteca/libSystem.mos"
passo "origem corrompida recusa sem gravar" "! $I --gravar --confirmo sda1 2>/dev/null && [ ! -e $ESP/mOS ]"
cp "$T/lib.bak" "$ORIG/mOS/Sistema/Biblioteca/libSystem.mos"
passo "instala" "$I --gravar --confirmo sda1 --proximo-boot > $T/inst.log"
passo "carregador em \\EFI\\mOS" "cmp -s $ESP/EFI/mOS/BOOTX64.EFI $ORIG/EFI/BOOT/BOOTX64.EFI && [ ! -e $ESP/EFI/mOS/MOSNEW.TMP ]"
passo "arquivos do Ubuntu intactos" "[ \"\$(ubuntu)\" = \"\$U0\" ]"
passo "estado PRIMEIRO_BOOT e manifesto instalado" "grep -qx PRIMEIRO_BOOT $ESP/mOS/Sistema/Biblioteca/estado-sistema && cmp -s $ESP/mOS/Sistema/Atualizacao/instalado.txt $ORIG/mOS/Sistema/Instalacao/manifesto.txt"
passo "preferências de idioma e teclado" "grep -qx teclado=abnt2 $ESP/mOS/Sistema/Biblioteca/Preferencias/com.mos.sistema.cfg"
passo "o manifesto de instalação da mídia não vai para o sistema" "[ ! -e $ESP/mOS/Sistema/Instalacao ]"
passo "Ubuntu continua primeiro, mOS no fim, BootNext" "grep -qx 'BootOrder: 0000,0002,0001,0003' $NV && grep -qx 'BootNext: 0003' $NV && grep -q 'Boot0003\* mOS' $NV"
passo "entrada do GRUB com a UUID da ESP" "grep -q 'search --no-floppy --fs-uuid --set=root DE1A-023D' $SYS/etc/grub.d/42_mos && [ -x $SYS/etc/grub.d/42_mos ] && [ -s $T/update-grub.log ]"
passo "cópia de segurança da NVRAM" "ls $SYS/var/backups/mos-dual-boot/*/efibootmgr-v.txt >/dev/null"
passo "reinstalar sem --atualizar recusa" "! $I --gravar --confirmo sda1 2>/dev/null"
mkdir -p "$ESP/mOS/Users/thiago"; echo doc > "$ESP/mOS/Users/thiago/nota.txt"; echo NORMAL > "$ESP/mOS/Sistema/Biblioteca/estado-sistema"
printf 'kernel novo' > "$ORIG/EFI/BOOT/BOOTX64.EFI"
python3 - "$ORIG" <<'P'
import hashlib, sys
r = sys.argv[1]; p = r + "/mOS/Sistema/Instalacao/manifesto.txt"
L = open(p).read().splitlines()[:-1]
d = open(r + "/EFI/BOOT/BOOTX64.EFI", "rb").read()
L = [("%s %d %s" % (hashlib.sha256(d).hexdigest(), len(d), "/EFI/BOOT/BOOTX64.EFI")) if l.endswith(" /EFI/BOOT/BOOTX64.EFI") else l for l in L]
tot = 0
for l in L[1:]: tot += int(l.split(" ")[1])
h = L[0].split(" "); h[2] = str(tot); L[0] = " ".join(h)
m = "\n".join(L) + "\n"; m += "ASSINATURA-SHA256 %s\n" % hashlib.sha256(m.encode()).hexdigest()
open(p, "w").write(m)
P
passo "atualizar troca o kernel e mantém contas e estado" "$I --gravar --confirmo sda1 --atualizar >/dev/null && grep -qx 'kernel novo' $ESP/EFI/mOS/BOOTX64.EFI && [ -f $ESP/mOS/Users/thiago/nota.txt ] && grep -qx NORMAL $ESP/mOS/Sistema/Biblioteca/estado-sistema && [ \$(grep -c 'Boot....\* mOS' $NV) = 1 ]"
passo "Ubuntu intacto depois de atualizar" "[ \"\$(ubuntu)\" = \"\$U0\" ]"
passo "desinstalar simula" "$I --desinstalar >/dev/null && [ -e $ESP/mOS ]"
passo "desinstalar" "$I --desinstalar --gravar --confirmo sda1 >/dev/null && [ ! -e $ESP/mOS ] && [ ! -e $ESP/EFI/mOS ] && [ ! -e $SYS/etc/grub.d/42_mos ] && ! grep -q mOS $NV && grep -q 'BootOrder: 0000,0002,0001' $NV"
passo "ESP volta a ter só o Ubuntu" "[ \"\$(cd $ESP && find . -type f | sort | tr '\n' ' ')\" = './EFI/BOOT/BOOTX64.EFI ./EFI/ubuntu/grub.cfg ./EFI/ubuntu/shimx64.efi ' ] && [ \"\$(ubuntu)\" = \"\$U0\" ]"
mkdir -p "$ESP/mOS"; echo alheio > "$ESP/mOS/x"
passo "não apaga \\mOS que não é do mOS" "! $I --desinstalar --gravar --confirmo sda1 2>/dev/null && [ -f $ESP/mOS/x ]"
rm -rf "$ESP/mOS"
# imagem de verdade do mkimg (GPT + FAT32 + manifesto assinado), lida pelo mtools sem montar e sem root
cc -O2 -o "$T/mkimg" "$R/tools/mkimg.c" 2>/dev/null
mkdir -p "$T/stg/EFI/BOOT" "$T/stg/mOS/Aplicativos/Calculadora Científica.app/Contents" "$T/stg/mOS/Sistema/Biblioteca"
head -c 200000 /dev/urandom > "$T/stg/EFI/BOOT/BOOTX64.EFI"
echo '<plist/>' > "$T/stg/mOS/Aplicativos/Calculadora Científica.app/Contents/Info.plist"
head -c 70000 /dev/urandom > "$T/stg/mOS/Sistema/Biblioteca/libSystem.mos"
"$T/mkimg" "$T/mOS.img" "$T/stg" 64 >/dev/null
J="python3 $R/tools/instalar-dual-boot.py --esp $ESP --imagem $T/mOS.img --raiz-sistema $SYS"
passo "imagem do mkimg: simulação confere a origem" "MOS_TMP=$T $J | grep -q 'origem conferida' && [ ! -e $ESP/mOS ]"
passo "imagem do mkimg: instala com acento no nome" "MOS_TMP=$T $J --gravar --confirmo sda1 --sem-nvram --sem-grub >/dev/null && cmp -s $ESP/EFI/mOS/BOOTX64.EFI $T/stg/EFI/BOOT/BOOTX64.EFI && [ -f '$ESP/mOS/Aplicativos/Calculadora Científica.app/Contents/Info.plist' ] && [ \"\$(ubuntu)\" = \"\$U0\" ]"
passo "imagem do mkimg: nada sobra no temporário" "[ -z \"\$(ls -d $T/mos-origem-* 2>/dev/null)\" ]"
passo "arquivo que não é imagem recusa" "head -c 4096 /dev/zero > $T/lixo.img && ! $J --imagem $T/lixo.img 2>/dev/null"
echo "instalar-dual-boot: $ok/$n ok"
[ $ok = $n ]
