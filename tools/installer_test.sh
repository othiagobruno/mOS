#!/bin/sh
# /usr/sbin/installer e /usr/sbin/pkgutil do mOS, compilados no host, instalando pacotes gerados por tools/mkpkg.py
# num volume de teste (pasta). Os scripts dos pacotes de teste só escrevem dentro desse volume.
set -e
cd "$(dirname "$0")/.."
D=$(mktemp -d /tmp/instest.XXXX)
for f in installer pkgutil; do
    gcc -std=gnu11 -O1 -g -fsanitize=address,undefined -w -Iuserland/pkg userland/cli-macos/$f.c -o "$D/$f"
done
python3 -I - "$D" "$(pwd)/tools" <<'PY'
import sys, os, gzip, subprocess, plistlib
D, tools = sys.argv[1], sys.argv[2]
sys.path.insert(0, tools)
import mkpkg
ok = fails = 0
def check(c, what):
    global ok, fails
    if c: ok += 1
    else: fails += 1; print('FALHA:', what)
def run(*a):
    p = subprocess.run([*a], capture_output=True, text=True, env={**os.environ, 'TMPDIR': D, 'ASAN_OPTIONS': 'detect_leaks=0'})
    return p.returncode, p.stdout + p.stderr
vol = D + '/vol'
os.makedirs(vol)
inst, pku = D + '/installer', D + '/pkgutil'
pre = b'#!/bin/sh\necho "pre $1|$2|$3|$4|$DSTROOT|$DSTVOLUME|$SCRIPT_NAME|$COMMAND_LINE_INSTALL" > "$3/pre.txt"\n[ -f "$2/App.app/Contents/x" ] && echo payload-cedo >> "$3/pre.txt"\nexit 0\n'
post = b'#!/bin/sh\n[ -f "$2/App.app/Contents/x" ] && echo "post ok $SCRIPT_NAME" > "$3/post.txt"\nexit 0\n'
scripts = gzip.compress(mkpkg.cpio_odc([('.', 0o40755, b''), ('./preinstall', 0o100755, pre), ('./postinstall', 0o100755, post)]))
payload_raw = mkpkg.cpio_odc([('.', 0o40755, b''), ('./App.app', 0o40755, b''), ('./App.app/Contents', 0o40755, b''),
                              ('./App.app/Contents/x', 0o100755, b'BIN' * 1000), ('./App.app/Contents/link', 0o120755, b'x')])
bom = open(tools + '/../tools/testdata_bom', 'rb').read() if os.path.exists(tools + '/testdata_bom') else None
# 1) pacote de componente, gzip
mkpkg.build_xar([('PackageInfo', mkpkg.pkginfo('com.mos.teste.app', '1.2', '/Applications')), ('Payload', gzip.compress(payload_raw)), ('Scripts', scripts)], D + '/a.pkg')
rc, out = run(inst, '-pkg', D + '/a.pkg', '-target', vol, '-verbose')
check(rc == 0 and 'The install was successful' in out, 'instalar a.pkg: ' + out)
check(open(vol + '/Applications/App.app/Contents/x', 'rb').read() == b'BIN' * 1000, 'conteúdo instalado')
check(os.stat(vol + '/Applications/App.app/Contents/x').st_mode & 0o777 == 0o755, 'modo do cpio')
check(int(os.stat(vol + '/Applications/App.app/Contents/x').st_mtime) == 1700000000, 'data do cpio')
check(os.readlink(vol + '/Applications/App.app/Contents/link') == 'x', 'link simbólico')
p = open(vol + '/pre.txt').read()
check(p.startswith('pre ' + D + '/a.pkg|' + vol + '/Applications|' + vol + '|' + vol + '|' + vol + '/Applications|' + vol + '|preinstall|1'), 'argumentos e ambiente do preinstall: ' + p)
check('payload-cedo' not in p, 'preinstall roda antes da carga')
check(open(vol + '/post.txt').read() == 'post ok postinstall\n', 'postinstall depois da carga')
r = plistlib.load(open(vol + '/var/db/receipts/com.mos.teste.app.plist', 'rb'))
check(r['PackageIdentifier'] == 'com.mos.teste.app' and r['PackageVersion'] == '1.2' and r['InstallPrefixPath'] == 'Applications', 'recibo: %r' % r)
rc, out = run(pku, '--volume', vol, '--pkgs')
check(rc == 0 and out.strip() == 'com.mos.teste.app', 'pkgutil --pkgs: ' + out)
rc, out = run(pku, '--volume', vol, '--pkg-info', 'com.mos.teste.app')
check('version: 1.2' in out and 'location: Applications' in out, 'pkgutil --pkg-info: ' + out)
rc, out = run(pku, '--volume', vol, '--pkgs=^com\\.mos\\..*app$')
check(out.strip() == 'com.mos.teste.app', 'pkgutil --pkgs=regex')
# 2) product archive com Distribution, dois componentes, pbzx; escolha desligada por -applyChoiceChangesXML
dist = b'''<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2"><title>Produto de Teste</title>
<pkg-ref id="com.mos.teste.um"/><pkg-ref id="com.mos.teste.dois"/>
<choices-outline><line choice="um"/><line choice="dois"/></choices-outline>
<choice id="um" title="Um"><pkg-ref id="com.mos.teste.um"/></choice>
<choice id="dois" title="Dois"><pkg-ref id="com.mos.teste.dois"/></choice>
<pkg-ref id="com.mos.teste.um" version="3.0" onConclusion="none">#um.pkg</pkg-ref>
<pkg-ref id="com.mos.teste.dois" version="3.0">#dois.pkg</pkg-ref>
</installer-gui-script>
'''
p1 = mkpkg.cpio_odc([('.', 0o40755, b''), ('./um.txt', 0o100644, b'um' * 5000)])
p2 = mkpkg.cpio_odc([('.', 0o40755, b''), ('./dois.txt', 0o100644, b'dois')])
mkpkg.build_xar([('Distribution', dist), ('um.pkg/PackageInfo', mkpkg.pkginfo('com.mos.teste.um', '3.0', '/opt/teste', scripts=False)),
                 ('um.pkg/Payload', mkpkg.pbzx(p1)), ('dois.pkg/PackageInfo', mkpkg.pkginfo('com.mos.teste.dois', '3.0', '/opt/teste', scripts=False)),
                 ('dois.pkg/Payload', gzip.compress(p2))], D + '/b.pkg')
rc, out = run(inst, '-pkginfo', '-pkg', D + '/b.pkg')
check(out.split() == ['com.mos.teste.um', 'com.mos.teste.dois'], '-pkginfo: ' + out)
rc, out = run(inst, '-showChoicesXML', '-pkg', D + '/b.pkg')
check('<string>um</string>' in out and '<string>dois</string>' in out, '-showChoicesXML')
ch = D + '/ch.plist'
plistlib.dump([{'choiceIdentifier': 'dois', 'choiceAttribute': 'selected', 'attributeSetting': 0}], open(ch, 'wb'))
rc, out = run(inst, '-pkg', D + '/b.pkg', '-target', vol, '-applyChoiceChangesXML', ch)
check(rc == 0 and 'Package name is Produto de Teste' in out, 'instalar b.pkg: ' + out)
check(open(vol + '/opt/teste/um.txt', 'rb').read() == b'um' * 5000, 'pbzx instalado')
check(not os.path.exists(vol + '/opt/teste/dois.txt'), 'escolha desligada não instala')
rc, out = run(pku, '--volume', vol, '--pkgs')
check(out.split() == ['com.mos.teste.app', 'com.mos.teste.um'], 'recibos depois do produto: ' + out)
# 3) recusas: script que falha, caminho com .., checksum
bad_pre = gzip.compress(mkpkg.cpio_odc([('./preinstall', 0o100755, b'#!/bin/sh\nexit 3\n')]))
mkpkg.build_xar([('PackageInfo', mkpkg.pkginfo('com.mos.teste.falha', '1', '/falha')), ('Payload', gzip.compress(mkpkg.cpio_odc([('./f', 0o100644, b'x')]))), ('Scripts', bad_pre)], D + '/c.pkg')
rc, out = run(inst, '-pkg', D + '/c.pkg', '-target', vol)
check(rc == 1 and 'The install failed' in out and 'preinstall' in out, 'preinstall com erro cancela: ' + out)
check(not os.path.exists(vol + '/falha/f') and not os.path.exists(vol + '/var/db/receipts/com.mos.teste.falha.plist'), 'nada instalado depois da falha')
mkpkg.build_xar([('PackageInfo', mkpkg.pkginfo('com.mos.teste.mau', '1', '/mau', scripts=False)), ('Payload', gzip.compress(mkpkg.cpio_odc([('./../../fora', 0o100644, b'x')])))], D + '/d.pkg')
rc, out = run(inst, '-pkg', D + '/d.pkg', '-target', vol)
check(rc == 1 and not os.path.exists(D + '/fora'), 'caminho com .. recusado: ' + out)
mkpkg.build_xar([('PackageInfo', mkpkg.pkginfo('com.mos.teste.x', '1', '/x', scripts=False)), ('Payload', gzip.compress(p2))], D + '/e.pkg', corrupt=True)
rc, out = run(inst, '-pkg', D + '/e.pkg', '-target', vol)
check(rc == 1 and 'checksum' in out, 'checksum errado recusado: ' + out)
rc, out = run(inst, '-pkg', D + '/nao-existe.pkg', '-target', vol)
check(rc == 1, 'pacote que não existe')
# 4) pkgutil --forget, --expand, --payload-files
rc, out = run(pku, '--volume', vol, '--forget', 'com.mos.teste.um')
check(rc == 0 and not os.path.exists(vol + '/var/db/receipts/com.mos.teste.um.plist'), '--forget')
rc, out = run(pku, '--expand', D + '/b.pkg', D + '/exp')
check(rc == 0 and os.path.exists(D + '/exp/Distribution') and os.path.exists(D + '/exp/um.pkg/Payload'), '--expand')
rc, out = run(pku, '--payload-files', D + '/b.pkg')
check('./um.txt' in out and './dois.txt' in out, '--payload-files: ' + out)
rc, out = run(pku, '--check-signature', D + '/a.pkg')
check(rc == 1 and 'no signature' in out, '--check-signature sem assinatura')
# 5) formato antigo: pasta .pkg com Info.plist, Archive.pax.gz e scripts em Resources (preflight/postflight)
b = D + '/Velho.pkg/Contents'
os.makedirs(b + '/Resources')
plistlib.dump({'CFBundleIdentifier': 'com.mos.teste.velho', 'CFBundleShortVersionString': '0.9', 'IFPkgFlagDefaultLocation': '/Applications'}, open(b + '/Info.plist', 'wb'))
open(b + '/Archive.pax.gz', 'wb').write(gzip.compress(mkpkg.cpio_odc([('.', 0o40755, b''), ('./Velho.app', 0o40755, b''), ('./Velho.app/v', 0o100644, b'velho')])))
for nm in ('preflight', 'postflight'):
    open(b + '/Resources/' + nm, 'wb').write(b'#!/bin/sh\necho %s >> "$3/flight.txt"\n' % nm.encode())
    os.chmod(b + '/Resources/' + nm, 0o755)
rc, out = run(inst, '-pkg', D + '/Velho.pkg', '-target', vol)
check(rc == 0 and open(vol + '/Applications/Velho.app/v').read() == 'velho', 'pacote em pasta: ' + out)
check(open(vol + '/flight.txt').read() == 'preflight\npostflight\n', 'preflight e postflight')
check(os.path.exists(vol + '/var/db/receipts/com.mos.teste.velho.plist'), 'recibo do pacote em pasta')
print('installer: %d verificações, %d falha(s)' % (ok + fails, fails))
sys.exit(1 if fails else 0)
PY
r=$?
rm -rf "$D"
exit $r
