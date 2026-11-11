#!/usr/bin/env python3
"""Placar da meta de compatibilidade (docs/meta-compat.md): mede N0 e N1 dos 12 apps.

  python3 -I tools/meta/placar.py --build DIR [--cache DIR] [--saida docs/meta-placar.md]
                                  [--apps id,id] [--n2 DIR] [--so-baixar]

--build  saída do tools/build.sh (libSystem.mos, esp/, dmgtest, machocheck).
--cache  pacotes oficiais (padrão: <build>/apps-dl, o mesmo do stage-apps-oficiais.sh).
--n2     pasta com <id>/n2.json gravados pelo tools/meta/n2-vm.ps1 (VM). Sem ela, N2 = não medido.

N0 Instala: pacote oficial com SHA-256 conferido, listado no tools/stage-apps-oficiais.sh e
   presente em esp/mOS/Aplicativos como Mach-O real (o que o verificar-imagem-apps.py exige).
N1 Liga: machocheck do bundle inteiro contra o libSystem.mos do build e os dylibs reais do esp
   (libc++, Swift, ICU, libz...; dylib com o marcador de stub não entra) dá 0 ausentes e
   nenhum import do app cai num stub vazio de userland/libsystem/sys_zerados.c ou
   swiftui_imports.c.
Os pacotes e hashes vêm do tools/stage-apps-oficiais.sh. Os três apps que ele ainda não lista
(Antigravity, Postico, Setapp) têm URL e SHA-256 aqui, medidos no download oficial.
"""
import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
STAGE = os.path.join(ROOT, "tools", "stage-apps-oficiais.sh")
STUB_MARK = b"STUB mOS libswift"
STUB_FONTES = ("userland/libsystem/sys_zerados.c", "userland/libsystem/swiftui_imports.c")

# id, nome, pacote no cache, tipo, caminho do .app (ou do binário) dentro do extraído,
# executável principal relativo ao .app, caminho em esp/mOS/Aplicativos
APPS = [
    ("codex-cli", "Codex CLI 0.167.0", "codex-cli.tar.gz", "tar", None, None, "Codex CLI/codex"),
    ("chatgpt", "Codex desktop / ChatGPT 26.1103.60933", "ChatGPT.zip", "zip", "ChatGPT.app", "Contents/MacOS/ChatGPT", "ChatGPT.app/Contents/MacOS/ChatGPT"),
    ("cursor", "Cursor 3.29.15", "Cursor.zip", "zip", "Cursor.app", "Contents/MacOS/Cursor", "Cursor.app/Contents/MacOS/Cursor"),
    ("antigravity", "Antigravity 2.31.0", "Antigravity.dmg", "dmg", "Antigravity.app", None, "Antigravity.app"),
    ("vscode", "VS Code 1.145.3", "VSCode.zip", "zip", "Visual Studio Code.app", "Contents/MacOS/Code", "Visual Studio Code.app/Contents/MacOS/Code"),
    ("slack", "Slack 4.53.287", "Slack.dmg", "dmg", "Slack.app", "Contents/MacOS/Slack", "Slack.app/Contents/MacOS/Slack"),
    ("grok", "Grok Bot 0.79.4", "Grok.dmg", "dmg", "Grok Bot.app", "Contents/MacOS/Grok Bot", "Grok Bot.app/Contents/MacOS/Grok Bot"),
    ("claude", "Claude Desktop 4.23118.1", "Claude.zip", "zip", "Claude.app", "Contents/MacOS/Claude", "Claude.app/Contents/MacOS/Claude"),
    ("postico", "Postico 2.3.5", "Postico.dmg", "dmg", "Postico 2.app", None, "Postico 2.app"),
    ("whatsapp", "WhatsApp 26.45.13", "WhatsApp.zip", "zip", "WhatsApp.app", "Contents/MacOS/WhatsApp", "WhatsApp.app/Contents/MacOS/WhatsApp"),
    ("openbot", "OpenBot 0.66.0", "OpenBot.zip", "zip", "OpenBot.app", "Contents/MacOS/OpenBot", "OpenBot.app/Contents/MacOS/OpenBot"),
    ("setapp", "Setapp 4.0.0", "Setapp.zip", "zip", "Setapp.app", None, "Setapp.app"),
]

# Fora do stage-apps-oficiais.sh. SHA-256 medido no download oficial de 10 nov. 2026.
EXTRAS = {
    "Antigravity.dmg": ("https://storage.googleapis.com/antigravity-public/antigravity-hub/2.31.0-6379060755038208/darwin-x64/Antigravity.dmg",
                        "f9d2699fd1307a44eae396f7463fc118f04554dc84d4ae56d95c487f1f050c56"),
    # para onde aponta releases.eggerapps.at/postico2/download (302): Postico 2.3.5 (9817).
    # O 9820 da mesma página é "Postico 2.3 dev", não o release.
    "Postico.dmg": ("https://downloads.eggerapps.at/postico/postico-9817.dmg",
                    "34ec7730de0796c008e0caf9f366629a3043c5813c79f749e47d22084afb6e0f"),
    "Setapp.zip": ("https://dl.devmate.com/com.setapp.DesktopClient/Setapp.zip",
                   "a124004ba0c6555e8bb05904a65e78b7d32a8eaa069b304f8e9a78e4e859735a"),
}

MAGIC = (b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca", b"\xfe\xed\xfa\xcf")


def sh256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def pacotes_do_stage():
    s = open(STAGE, encoding="utf-8").read()
    out = {}
    for m in re.finditer(r'fetch "([^"]+)" \\\s*\n\s*"\$DL/([^"]+)" ([0-9a-f]{64})', s):
        out[m.group(2)] = (m.group(1), m.group(3))
    return out


def baixar(url, dest):
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=600) as r, open(dest + ".partial", "wb") as f:
        shutil.copyfileobj(r, f, 1 << 20)
    os.replace(dest + ".partial", dest)


def extrair_zip(z, dest):
    """Como o ditto: links simbólicos do zip (Versions/Current) viram links de verdade."""
    with zipfile.ZipFile(z) as zf:
        for info in zf.infolist():
            nome = info.filename
            if nome.startswith("/") or ".." in nome.split("/"):
                continue
            alvo = os.path.join(dest, nome)
            modo = info.external_attr >> 16
            if stat.S_ISLNK(modo):
                os.makedirs(os.path.dirname(alvo), exist_ok=True)
                link = zf.read(info).decode("utf-8", "replace")
                if os.path.lexists(alvo):
                    os.remove(alvo)
                os.symlink(link, alvo)
            elif nome.endswith("/"):
                os.makedirs(alvo, exist_ok=True)
            else:
                os.makedirs(os.path.dirname(alvo), exist_ok=True)
                with zf.open(info) as src, open(alvo, "wb") as dst:
                    shutil.copyfileobj(src, dst, 1 << 20)


def dmgtest(build):
    exe = os.path.join(build, "dmgtest")
    if os.access(exe, os.X_OK):
        return exe
    host = os.path.join(build, "dmghost.c")
    with open(host, "w") as f:
        f.write("#include <stdint.h>\n"
                "typedef struct { int year, month, day, hour, minute, second, weekday; } datetime_t;\n"
                "void rtc_now(datetime_t *dt) { if (dt) { dt->year=2026; dt->month=1; dt->day=1; } }\n"
                "void rtc_now_utc(datetime_t *dt) { rtc_now(dt); }\n"
                "uint32_t rand32(void) { return 1; }\n")
    fs = ["apfs", "blkdev", "bzip2", "dfs", "dmg", "hfsplus", "hfsplus_wr", "pcache", "inflate", "lzfse", "xz", "fat"]
    cmd = ["cc", "-O2", "-DMOS_HOST_TEST", "-I" + os.path.join(ROOT, "kernel/include"), "-I" + os.path.join(ROOT, "tools"),
           os.path.join(ROOT, "tools/dmgtest.c"), host] + [os.path.join(ROOT, "kernel/fs", n + ".c") for n in fs] + ["-o", exe]
    subprocess.run(cmd, check=True)
    return exe


def extrair(app, pacote, build, xdir):
    ident, _, arq, tipo = app[:4]
    dest = os.path.join(xdir, ident)
    feito = os.path.join(dest, ".extraido-" + sh256(pacote)[:16])
    if os.path.exists(feito):
        return dest
    shutil.rmtree(dest, ignore_errors=True)
    os.makedirs(dest)
    if tipo == "zip":
        extrair_zip(pacote, dest)
    elif tipo == "tar":
        with tarfile.open(pacote) as t:
            t.extractall(dest, filter="data")
    elif tipo == "dmg":
        r = subprocess.run([dmgtest(build), pacote, dest], capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError("dmgtest falhou: " + (r.stdout + r.stderr)[-400:])
    open(feito, "w").close()
    return dest


def achar_app(dest, nome):
    if nome is None:
        return dest
    for dp, dn, _ in os.walk(dest):
        if nome in dn:
            return os.path.join(dp, nome)
        dn[:] = [d for d in dn if not d.endswith(".app")]
    # nome exato não achado: o primeiro .app do nível mais alto
    for dp, dn, _ in os.walk(dest):
        apps = sorted(d for d in dn if d.endswith(".app"))
        if apps:
            return os.path.join(dp, apps[0])
    return None


def exec_principal(app_dir, rel):
    if os.path.isfile(app_dir):
        return app_dir
    if rel and os.path.isfile(os.path.join(app_dir, rel)):
        return os.path.join(app_dir, rel)
    plist = os.path.join(app_dir, "Contents", "Info.plist")
    try:
        import plistlib
        with open(plist, "rb") as f:
            n = plistlib.load(f).get("CFBundleExecutable")
        p = os.path.join(app_dir, "Contents", "MacOS", n)
        if os.path.isfile(p):
            return p
    except Exception:
        pass
    return None


def arquiteturas(path):
    with open(path, "rb") as f:
        h = f.read(4096)
    be = int.from_bytes(h[:4], "big")
    if be in (0xCAFEBABE, 0xCAFEBABF):
        n = int.from_bytes(h[4:8], "big")
        w = 32 if be == 0xCAFEBABF else 20
        cpus = [int.from_bytes(h[8 + i * w:12 + i * w], "big") for i in range(min(n, 16))]
    elif h[:4] == b"\xcf\xfa\xed\xfe":
        cpus = [int.from_bytes(h[4:8], "little")]
    else:
        cpus = []
    nomes = {0x01000007: "x86_64", 0x0100000C: "arm64"}
    return [nomes.get(c, hex(c)) for c in cpus]


def stubs_vazios():
    nomes = set()
    rx = re.compile(r"^\s*(?:void|int|long)\s*\*?\s*([A-Za-z_$][\w$.]*)\s*(?:\(void\)\s*\{\s*\}|;|=\s*0\s*;)\s*$")
    for rel in STUB_FONTES:
        for linha in open(os.path.join(ROOT, rel), encoding="utf-8"):
            m = rx.match(linha)
            if m:
                nomes.add(m.group(1))
    return nomes


def variante(n):
    """Mesma regra do machocheck/macho.c: sem o "$..." final (não vale para nome Swift, que começa com $)."""
    i = n.find("$", 1)
    return n[:i] if i > 0 and not n.startswith("$") else n


def disco_real(build, esp):
    """Links para os dylibs/frameworks reais do esp; o que tem o marcador de stub fica de fora."""
    d = os.path.join(build, "placar-disco")
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    reais, stubs = [], []
    base = os.path.join(esp, "mOS")
    # a imagem inteira menos os apps: usr/lib, /System/Library/Frameworks, iOSSupport, Sistema/Biblioteca...
    for dp, dn, fn in os.walk(base):
        if dp == base:
            dn[:] = [d for d in dn if d not in ("Aplicativos", "Users")]
        if True:
            for n in fn:
                p = os.path.join(dp, n)
                if os.path.islink(p) or not os.path.isfile(p) or os.path.getsize(p) < 32:
                    continue
                with open(p, "rb") as f:
                    if f.read(4) not in MAGIC:
                        continue
                data = open(p, "rb").read()
                rel = os.path.relpath(p, base)
                if STUB_MARK in data:
                    stubs.append(rel)
                    continue
                reais.append(rel)
                # mesmo caminho relativo: o machocheck classifica pelo nome do arquivo
                # (libc++.1.dylib, libswift*, CryptoKit...)
                alvo = os.path.join(d, rel)
                os.makedirs(os.path.dirname(alvo), exist_ok=True)
                os.symlink(p, alvo)
    return d, reais, stubs


def rodar_machocheck(mc, libsys, discos, app_dir, arch, saida):
    rep = os.path.join(saida, "ausentes.txt")
    res = os.path.join(saida, "resolvidos.txt")
    cmd = [mc, "--arch", arch, "--disco-todo", "--resolvidos", res]
    for d in discos:
        cmd += ["--disco", d]
    cmd += [libsys, app_dir, rep]
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    open(os.path.join(saida, "machocheck.txt"), "w").write(r.stdout + r.stderr)
    out = r.stdout
    m = re.search(r"Total: (\d+) imports, (\d+) sem implementacao \((\d+) simbolos distintos\)", out)
    im = re.search(r"Imagens Mach-O \S+ no bundle: (\d+)", out)
    cat = re.search(r"sistema (\d+), libc\+\+ (\d+), Swift (\d+), unwind (\d+)", out)
    faltam = re.findall(r"\[falta\] (.+)", out)
    if not m or not im:
        raise RuntimeError("saída do machocheck sem total: " + out[-300:] + r.stderr[-300:])
    ausentes = []
    if os.path.exists(rep):
        for linha in open(rep, encoding="utf-8", errors="replace"):
            if linha.startswith("#") or "\t" not in linha:
                continue
            lib, nome = linha.rstrip("\n").split("\t", 1)
            ausentes.append((lib, nome))
    resolvidos = [l.strip() for l in open(res, encoding="utf-8", errors="replace")] if os.path.exists(res) else []
    return {
        "imagens": int(im.group(1)), "imports": int(m.group(1)), "ausentes_imports": int(m.group(2)),
        "ausentes": int(m.group(3)),
        "por_origem": dict(zip(("sistema", "libc++", "Swift", "unwind"), map(int, cat.groups()))) if cat else {},
        "libs_faltando": faltam, "lista_ausentes": ausentes, "resolvidos": resolvidos,
    }


def commit():
    try:
        h = subprocess.run(["git", "-C", ROOT, "rev-parse", "--short", "HEAD"], capture_output=True, text=True).stdout.strip()
        sujo = subprocess.run(["git", "-C", ROOT, "status", "--porcelain", "--untracked-files=no"], capture_output=True, text=True).stdout
        # o tools/build.sh reescreve o caminho do libSystem.mos neste arquivo: não é mudança de fonte
        sujo = [l for l in sujo.splitlines() if l.strip() and not l.endswith("kernel/compat/libsystem_blob.S")]
        return h + ("+mudanças locais" if sujo else "")
    except Exception:
        return "?"


def n2_de(n2dir, ident):
    if not n2dir:
        return None
    p = os.path.join(n2dir, ident, "n2.json")
    if not os.path.exists(p):
        return None
    return json.load(open(p, encoding="utf-8-sig"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", required=True)
    ap.add_argument("--cache")
    ap.add_argument("--saida", default=os.path.join(ROOT, "docs", "meta-placar.md"))
    ap.add_argument("--apps")
    ap.add_argument("--n2")
    ap.add_argument("--so-baixar", action="store_true")
    ap.add_argument("--disco-extra", action="append", default=[],
                    help="mais uma árvore de dylibs reais (ex.: frameworks ObjC do tools/build-frameworks-objc.sh)")
    a = ap.parse_args()
    build = os.path.abspath(a.build)
    cache = os.path.abspath(a.cache or os.path.join(build, "apps-dl"))
    os.makedirs(cache, exist_ok=True)
    esp = os.path.join(build, "esp")
    libsys = os.path.join(build, "libSystem.mos")
    stage = pacotes_do_stage()
    escolhidos = set(a.apps.split(",")) if a.apps else None

    mc = os.path.join(build, "machocheck-placar")
    subprocess.run(["cc", "-O2", "-o", mc, os.path.join(ROOT, "tools", "machocheck.c")], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    stubs = stubs_vazios()
    disco, reais, stubs_dylib = disco_real(build, esp) if os.path.isdir(esp) else (None, [], [])
    extras = [os.path.abspath(x) for x in a.disco_extra]
    fw = os.path.join(build, "frameworks-objc")
    if os.path.isdir(fw) and fw not in extras:
        extras.append(fw)
    discos = ([disco] if disco else []) + extras
    xdir = os.path.join(build, "placar-x")
    rdir = os.path.join(build, "placar")
    os.makedirs(xdir, exist_ok=True)
    os.makedirs(rdir, exist_ok=True)

    linhas = []
    for app in APPS:
        ident, nome, arq, tipo, appnome, execrel, imgrel = app
        if escolhidos and ident not in escolhidos:
            continue
        r = {"id": ident, "app": nome, "pacote": arq}
        pac = os.path.join(cache, arq)
        url, want = stage.get(arq, EXTRAS.get(arq, (None, None)))
        r["no_stage"] = arq in stage
        r["url"] = url
        if not os.path.exists(pac) and url:
            print("baixando", arq, flush=True)
            baixar(url, pac)
        if not os.path.exists(pac):
            r["erro"] = "pacote ausente no cache"
            linhas.append(r)
            continue
        got = sh256(pac)
        r["sha256"] = got
        r["sha_ok"] = (got == want) if want else None
        if a.so_baixar:
            linhas.append(r)
            continue
        # N0
        img = os.path.join(esp, "mOS", "Aplicativos", imgrel)
        no_img = os.path.exists(img)
        if no_img and os.path.isfile(img):
            no_img = open(img, "rb").read(4) in MAGIC
        r["na_imagem"] = no_img
        r["N0"] = bool(r["no_stage"] and r["sha_ok"] and no_img)
        # N1
        try:
            dest = extrair(app, pac, build, xdir)
            app_dir = achar_app(dest, appnome)
            if app_dir is None:
                raise RuntimeError(".app não achado no pacote")
            if tipo == "tar":
                ex = None
                for dp, _, fn in os.walk(dest):
                    for n in fn:
                        if n == "codex" or n.startswith("codex-") and "apple-darwin" in n:
                            ex = os.path.join(dp, n)
                app_dir = os.path.dirname(ex)
            else:
                ex = exec_principal(app_dir, execrel)
            arcs = arquiteturas(ex) if ex else []
            arch = "x86_64" if "x86_64" in arcs else "arm64"
            r["arquiteturas"] = arcs
            r["arch_medida"] = arch
            r["bundle"] = os.path.basename(app_dir)
            sd = os.path.join(rdir, ident)
            os.makedirs(sd, exist_ok=True)
            if not os.path.exists(libsys) or disco is None:
                raise RuntimeError("sem libSystem.mos/esp do build")
            mcres = rodar_machocheck(mc, libsys, discos, app_dir, arch, sd)
            alc = sorted({n for n in mcres["resolvidos"] if n in stubs or variante(n) in stubs})
            open(os.path.join(sd, "stubs-alcancados.txt"), "w").write("\n".join(alc) + ("\n" if alc else ""))
            r.update({k: v for k, v in mcres.items() if k not in ("resolvidos", "lista_ausentes")})
            r["stubs_alcancados"] = len(alc)
            r["exemplo_stubs"] = alc[:5]
            r["exemplo_ausentes"] = [n for _, n in mcres["lista_ausentes"][:5]]
            r["N1"] = mcres["imagens"] > 0 and mcres["ausentes"] == 0 and not alc
        except Exception as e:  # noqa: BLE001 — o placar registra a falha e segue
            r["erro"] = str(e)[:400]
            r["N1"] = False
        n2 = n2_de(a.n2, ident)
        r["N2"] = None if n2 is None else bool(n2.get("ok"))
        r["n2_det"] = n2
        linhas.append(r)
        print(json.dumps({k: r.get(k) for k in ("id", "N0", "N1", "arch_medida", "imagens", "ausentes", "stubs_alcancados", "erro")}, ensure_ascii=False), flush=True)

    meta = {
        "data": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC"),
        "commit": commit(),
        "libsystem_sha256": sh256(libsys) if os.path.exists(libsys) else None,
        "stubs_vazios_fonte": len(stubs),
        "dylibs_reais": len(reais),
        "dylibs_stub_excluidos": stubs_dylib,
        "disco_extra": [os.path.relpath(x, build) if x.startswith(build) else x for x in extras],
    }
    json.dump({"meta": meta, "apps": linhas}, open(os.path.join(rdir, "placar.json"), "w"), ensure_ascii=False, indent=1)
    if not a.so_baixar:
        open(a.saida, "w", encoding="utf-8").write(markdown(meta, linhas))
        print("gravado", a.saida)
    return 0


def sim(v):
    return "não medido" if v is None else ("✓" if v else "✗")


def markdown(meta, linhas):
    o = ["# Placar medido da meta (N0, N1)", "",
         "Gerado por `tools/meta/placar.py`. Não editar à mão: rode o script de novo. Níveis e roteiros em "
         "[meta-compat.md](meta-compat.md).", "",
         "- Data: %s" % meta["data"],
         "- Commit: `%s`" % meta["commit"],
         "- libSystem.mos: SHA-256 `%s`" % meta["libsystem_sha256"],
         "- Dylibs reais no `--disco` (do esp): %d. Excluídos por serem stub marcado: %d%s" % (
             meta["dylibs_reais"], len(meta["dylibs_stub_excluidos"]),
             (" (" + ", ".join("`%s`" % s for s in meta["dylibs_stub_excluidos"][:6]) +
              (", …" if len(meta["dylibs_stub_excluidos"]) > 6 else "") + ")") if meta["dylibs_stub_excluidos"] else ""),
         "- Árvores extras no `--disco`: %s" % (", ".join("`%s`" % x for x in meta["disco_extra"]) or "nenhuma"),
         "- Stubs vazios conhecidos (`sys_zerados.c` + `swiftui_imports.c`): %d nomes" % meta["stubs_vazios_fonte"],
         "", "| App | Pacote (SHA-256) | Fatia medida | N0 Instala | Imagens | Imports | Ausentes (distintos) | Stubs vazios alcançados | N1 Liga | N2 Abre | N3 | N4 |",
         "|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in linhas:
        sha = r.get("sha256", "—")
        shatxt = "`%s…` %s" % (sha[:12], ("conferido" if r.get("no_stage") else "conferido (SHA do placar, fora do stage)") if r.get("sha_ok") else ("sem SHA de referência" if r.get("sha_ok") is None else "**diferente**")) if sha != "—" else "ausente"
        n0 = sim(r.get("N0"))
        if r.get("N0") is False:
            motivo = []
            if not r.get("no_stage"):
                motivo.append("fora do stage")
            if r.get("sha_ok") is False:
                motivo.append("SHA")
            if not r.get("na_imagem"):
                motivo.append("não está no esp")
            n0 += " (" + ", ".join(motivo) + ")"
        if r.get("erro"):
            o.append("| %s | %s | — | %s | — | — | — | — | ✗ (%s) | não medido | não medido | não medido |" % (
                r["app"], shatxt, n0, r["erro"].replace("|", "/")))
            continue
        o.append("| %s | %s | %s | %s | %d | %d | %d | %d | %s | %s | não medido | não medido |" % (
            r["app"], shatxt, r["arch_medida"] + ("" if r["arch_medida"] in r.get("arquiteturas", []) else "?"), n0,
            r["imagens"], r["imports"], r["ausentes"], r["stubs_alcancados"], sim(r["N1"]), sim(r["N2"])))
    o += ["", "## Detalhe por app", ""]
    for r in linhas:
        if r.get("erro"):
            o.append("- **%s**: erro: %s" % (r["app"], r["erro"]))
            continue
        po = r.get("por_origem", {})
        o.append("- **%s** (`%s`, fatias %s): ausentes por origem: sistema %s, libc++ %s, Swift %s, unwind %s. "
                 "Bibliotecas inexistentes no mOS: %s. Exemplos ausentes: %s. Exemplos de stub vazio alcançado: %s." % (
                     r["app"], r["bundle"], "/".join(r.get("arquiteturas", [])) or "?",
                     po.get("sistema"), po.get("libc++"), po.get("Swift"), po.get("unwind"),
                     len(r.get("libs_faltando", [])) and ", ".join("`%s`" % x for x in r["libs_faltando"][:4]) + (" …" if len(r["libs_faltando"]) > 4 else "") or "nenhuma",
                     ", ".join("`%s`" % x for x in r.get("exemplo_ausentes", [])) or "—",
                     ", ".join("`%s`" % x for x in r.get("exemplo_stubs", [])) or "—"))
    o += ["", "Listas completas por app: `<build>/placar/<id>/` (`ausentes.txt`, `stubs-alcancados.txt`, `machocheck.txt`) e "
          "`<build>/placar/placar.json`.", "",
          "Fatia arm64: o machocheck lê o slice arm64 e resolve contra os nomes do `libSystem.mos` x86_64 deste build "
          "(o libSystem arm64 sai do mesmo fonte; ver docs/tradutor-arm64.md). Mede a cobertura de símbolos, não o tradutor.", ""]
    return "\n".join(o)


if __name__ == "__main__":
    sys.exit(main())
