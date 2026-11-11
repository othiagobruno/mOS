#!/bin/bash
# mOS — git 2.56.1 (GPLv2, COPYING) compilado do fonte como Mach-O x86_64 para /usr/bin/git.
# O Cursor (e o VS Code) precisam de um git no PATH. O bottle do Homebrew tem 619 MiB e liga
# pcre2/gettext do Homebrew; o Xcode CLT não é baixável. Daqui sai um git sem nada do Homebrew:
#   - libz e libcurl são as nossas (/usr/lib/libz.1.dylib, /usr/lib/libcurl.4.dylib; build-libz.sh, build-libcurl.sh);
#   - sem gettext, perl, python, tcl/tk, expat, OpenSSL (SHA-1 do sha1dc, SHA-256 do bloco interno), Rust;
#   - regex do compat/ do git (não depende do regcomp do libSystem), sem FSMonitor (CoreServices).
#   - iconv (precompose_utf8 do Darwin) vem do libSystem do mOS: o carregador põe /usr/lib/libiconv.2.dylib no
#     libSystem (kernel/compat/macho.c). Para ligar, um libiconv.dylib só de nomes (build/git-x86_64/lib).
# Os builtins não viram git-<cmd> (SKIP_DASHED_BUILT_INS): o "git <cmd>" basta. O que o git ainda chama
# pelo nome com hífen (git-upload-pack, git-receive-pack, git-upload-archive no clone local; git-remote-https)
# vira um executável de poucos KiB que faz exec do binário de verdade (tools/git-hifen.c), porque a imagem
# não guarda hardlink nem symlink e cada cópia do git teria alguns MiB.
# Saída: build/git-x86_64/mos/usr/{bin/git, libexec/git-core/*, share/git-core/templates}, que o build.sh copia
# para a imagem. Fonte: https://mirrors.edge.kernel.org/pub/software/scm/git/git-2.56.1.tar.xz (SHA-256 igual ao
# sha256sums.asc assinado do kernel.org).
# Sem make no host (o Linux de build não tem): baixa e compila o GNU make 4.4.1 com o build.sh dele.
#   MOS_ZIG=/caminho/do/zig bash tools/build-git.sh
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-zig}
TGT=x86_64-macos.11.0
JOBS=$(nproc 2>/dev/null || echo 4)
VER=2.56.1
SHA=f0b0bf89264b9bd2e1c640363e7529643989da76281600473c50745cd34a607f
MKVER=4.4.1
MKSHA=dd16fb1d67bfab79a72f5e8390735c49e3e8e70b4945a15ab1f81ddb78658fb3
DL=$ROOT/.tools/git-dl
OUT=$ROOT/build/git-x86_64
SRC=$OUT/git-$VER
STAGE=$OUT/mos
GIT=$STAGE/usr/bin/git

. "$ROOT/tools/baixar.sh"
baixar "https://mirrors.edge.kernel.org/pub/software/scm/git/git-$VER.tar.xz" "$DL/git-$VER.tar.xz" "$SHA" || exit 1

# libz e libcurl primeiro (a libcurl chama build-libz.sh)
MOS_ZIG="$ZIG" bash "$ROOT/tools/build-libcurl.sh"
ZLIB=$ROOT/build/zlib-x86_64/libz.1.dylib
CURL=$ROOT/build/curl-x86_64/libcurl.4.dylib
ZSRC=$ROOT/.tools/zlib-dl/zlib-1.3.2
CSRC=$ROOT/.tools/curl-dl/curl-8.23.0
for f in "$ZLIB" "$CURL" "$ZSRC/zlib.h" "$CSRC/include/curl/curl.h"; do
    [ -f "$f" ] || { echo "git: falta $f" >&2; exit 1; }
done

if [ -x "$GIT" ] && [ "$GIT" -nt "$0" ] && [ "$GIT" -nt "$CURL" ] && [ "$GIT" -nt "$ROOT/tools/git-hifen.c" ]; then
    echo "OK: $GIT (cache)"
    exit 0
fi

# tar por python3 (tarfile): xz e bzip2 sem depender dos programas do host
extrair() {
    python3 -I -c 'import sys, tarfile
with tarfile.open(sys.argv[1]) as t:
    if hasattr(tarfile, "data_filter"):
        t.extractall(sys.argv[2], filter="data")
    else:
        for m in t.getmembers():
            if m.name.startswith("/") or ".." in m.name.split("/"):
                sys.exit("tar: caminho suspeito: " + m.name)
        t.extractall(sys.argv[2])' "$1" "$2"
}

# ---- make do host
MAKE=$(command -v make || command -v gmake || true)
if [ -z "$MAKE" ]; then
    MKDIR=$ROOT/.tools/make-$MKVER
    MAKE=$MKDIR/make
    if [ ! -x "$MAKE" ]; then
        baixar "https://ftp.gnu.org/gnu/make/make-$MKVER.tar.gz" "$ROOT/.tools/make-dl/make-$MKVER.tar.gz" "$MKSHA" || exit 1
        rm -rf "$MKDIR"
        extrair "$ROOT/.tools/make-dl/make-$MKVER.tar.gz" "$ROOT/.tools"
        echo "==> GNU make $MKVER (host)"
        # o caminho da raiz tem espaço: o configure roda de um diretório sem espaço no nome
        MKTMP=$(mktemp -d)
        cp -a "$MKDIR/." "$MKTMP/"
        (cd "$MKTMP" && CC="$ZIG cc" ./configure --disable-nls --without-guile --disable-dependency-tracking -q >/dev/null && sh build.sh >/dev/null)
        [ -x "$MKTMP/make" ] || { echo "git: GNU make não compilou" >&2; rm -rf "$MKTMP"; exit 1; }
        cp -f "$MKTMP/make" "$MAKE"
        rm -rf "$MKTMP"
    fi
fi
echo "make: $MAKE"

# ---- fonte do git em build/git-x86_64 (o Makefile do git compila dentro da árvore). A raiz do repositório
# costuma ter espaço no caminho ("Área de trabalho") e o Makefile não aceita espaço nos -I/-L nem nos programas:
# esses passam por links num diretório temporário (só links e três scripts; nada pesado vai para /tmp).
# O version_gen do git põe "$(shell pwd)" entre aspas duas vezes; com espaço, quebra. Na nossa cópia vira ".".
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
rm -rf "$STAGE" "$SRC" "$OUT/lib" "$OUT/inc" "$OUT/inst"
mkdir -p "$OUT/lib" "$OUT/inc" "$STAGE"
extrair "$DL/git-$VER.tar.xz" "$OUT"
[ -f "$SRC/git.c" ] || { echo "git: extrair o fonte falhou" >&2; exit 1; }
sed -i 's|$(call version_gen,"$(shell pwd)",|$(call version_gen,.,|' "$SRC/Makefile"
ln -s "$OUT" "$W/o"
cp "$MAKE" "$W/make"
G=$W/o/git-$VER

# cabeçalhos e bibliotecas do -I/-l: as nossas; o install name continua /usr/lib/...
cp -f "$ZSRC/zlib.h" "$ZSRC/zconf.h" "$OUT/inc/"
cp -a "$CSRC/include/curl" "$OUT/inc/curl"
cp -f "$ZLIB" "$OUT/lib/libz.dylib"
cp -f "$CURL" "$OUT/lib/libcurl.dylib"
printf 'void *iconv_open(void) { return 0; }\nint iconv(void) { return -1; }\nint iconv_close(void) { return 0; }\n' > "$OUT/iconv-nomes.c"
"$ZIG" cc -target "$TGT" -shared -o "$OUT/lib/libiconv.dylib" "$OUT/iconv-nomes.c" \
    -Wl,-install_name,/usr/lib/libiconv.2.dylib -Wl,-compatibility_version,7.0.0 -Wl,-current_version,7.0.0
printf '#!/bin/sh\nexec "%s" cc -target %s "$@"\n' "$ZIG" "$TGT" > "$W/zigcc"
printf '#!/bin/sh\nexec "%s" ar "$@"\n' "$ZIG" > "$W/zigar"
printf '#!/bin/sh\nexec "%s" ranlib "$@"\n' "$ZIG" > "$W/zigranlib"
chmod +x "$W/zigcc" "$W/zigar" "$W/zigranlib"

# config.mak entra depois do config.mak.uname: tira o que o Darwin liga e o mOS não tem
cat > "$G/config.mak" <<'EOF'
# gerado por tools/build-git.sh
FSMONITOR_DAEMON_BACKEND =
FSMONITOR_OS_SETTINGS =
BASIC_LDFLAGS := $(filter-out -framework CoreServices,$(BASIC_LDFLAGS))
USE_ENHANCED_BASIC_REGULAR_EXPRESSIONS =
APPLE_COMMON_CRYPTO =
EOF

GITMAKE=(
    uname_S=Darwin uname_M=x86_64 uname_O=Darwin uname_R=20.6.0 uname_V=mOS uname_P=i386
    CC="$W/zigcc" AR="$W/zigar" RANLIB="$W/zigranlib"
    CFLAGS="-O2 -g0 -I$W/o/inc"
    LDFLAGS="-L$W/o/lib -Wl,-S"
    CURL_CFLAGS= CURL_LDFLAGS=-lcurl
    prefix=/usr gitexecdir=/usr/libexec/git-core template_dir=/usr/share/git-core/templates
    sysconfdir=/etc ETC_GITCONFIG=/etc/gitconfig ETC_GITATTRIBUTES=/etc/gitattributes
    SHELL_PATH=/bin/sh
    NO_GETTEXT=YesPlease NO_PERL=YesPlease NO_PYTHON=YesPlease NO_TCLTK=YesPlease NO_OPENSSL=YesPlease
    NO_EXPAT=YesPlease NO_RUST=YesPlease NO_REGEX=YesPlease NO_APPLE_COMMON_CRYPTO=YesPlease
    NO_INSTALL_HARDLINKS=YesPlease SKIP_DASHED_BUILT_INS=YesPlease INSTALL_SYMLINKS=
    NO_GITWEB=YesPlease
)
echo "==> git $VER ($JOBS jobs)"
(cd "$G" && "$W/make" -j"$JOBS" "${GITMAKE[@]}" all) >"$OUT/make.log" 2>&1 || { tail -40 "$OUT/make.log" >&2; exit 1; }
(cd "$G" && "$W/make" "${GITMAKE[@]}" DESTDIR="$W/o/inst" install) >>"$OUT/make.log" 2>&1 || { tail -40 "$OUT/make.log" >&2; exit 1; }
INST=$OUT/inst

# ---- árvore mínima para a imagem
mkdir -p "$STAGE/usr/bin" "$STAGE/usr/libexec/git-core" "$STAGE/usr/share/git-core"
cp -f "$INST/usr/bin/git" "$GIT"
cp -f "$INST/usr/libexec/git-core/git-remote-http" "$STAGE/usr/libexec/git-core/git-remote-http"
# templates: description e info/exclude; os hooks *.sample são só exemplo
mkdir -p "$STAGE/usr/share/git-core/templates/info"
cp -f "$INST/usr/share/git-core/templates/description" "$STAGE/usr/share/git-core/templates/description"
cp -f "$INST/usr/share/git-core/templates/info/exclude" "$STAGE/usr/share/git-core/templates/info/exclude"
mkdir -p "$STAGE/usr/share/git-core/licenca"
cp -f "$SRC/COPYING" "$STAGE/usr/share/git-core/licenca/COPYING"
# nomes com hífen que o git procura em gitexecdir: exec do binário de verdade, argv[0] mantido
hifen() {
    "$ZIG" cc -target "$TGT" -O2 -g0 -Wl,-S -DALVO="\"$2\"" "$ROOT/tools/git-hifen.c" -o "$STAGE/usr/libexec/git-core/$1"
}
for n in git-upload-pack git-receive-pack git-upload-archive; do
    hifen "$n" /usr/bin/git
done
hifen git-remote-https /usr/libexec/git-core/git-remote-http
rm -rf "$INST"
echo "OK: git $VER em $STAGE ($(du -sk "$STAGE" | awk '{printf "%.1f", $1 / 1024}') MiB)"
