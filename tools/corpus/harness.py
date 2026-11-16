#!/usr/bin/env python3
"""Baixa o corpus de apps macOS (manifesto), confere o hash e roda machocheck --disco.

Binários ficam em build/corpus (gitignored). Não contorna DRM.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = Path(__file__).resolve().parent / "manifest.json"
CACHE = ROOT / "build" / "corpus"
ZIG_URL = "https://ziglang.org/download/0.17.1/zig-x86_64-linux-0.17.1.tar.xz"
ZIG_SHA = "58959f01285c05a74276f09327b551d42ed6006bf325a78dc661a024e314987b"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(dest.suffix + ".part")
    req = urllib.request.Request(url, headers={"User-Agent": "mOS-corpus-harness"})
    with urllib.request.urlopen(req, timeout=120) as r, tmp.open("wb") as out:
        shutil.copyfileobj(r, out)
    tmp.replace(dest)


def load_manifest() -> dict:
    return json.loads(MANIFEST.read_text(encoding="utf-8"))


def ensure_zig() -> Path:
    found = shutil.which("zig")
    if found:
        return Path(found)
    tools = ROOT / ".tools"
    zig = tools / "zig-x86_64-linux-0.17.1" / "zig"
    if zig.is_file():
        return zig
    archive = tools / "zig-x86_64-linux-0.17.1.tar.xz"
    print("baixando zig 0.17.1", flush=True)
    download(ZIG_URL, archive)
    digest = sha256_file(archive)
    if digest != ZIG_SHA:
        raise SystemExit(f"zig sha256 {digest} != {ZIG_SHA}")
    with tarfile.open(archive, "r:xz") as tf:
        tf.extractall(tools)
    if not zig.is_file():
        raise SystemExit("zig extraído, mas o binário não apareceu")
    return zig


def build_libsystem(zig: Path) -> tuple[Path, Path]:
    env = os.environ.copy()
    env["MOS_ZIG"] = str(zig)
    subprocess.check_call(["bash", str(ROOT / "tools" / "build.sh")], cwd=ROOT, env=env)
    lib = ROOT / "build" / "libSystem.mos"
    disco = ROOT / "build" / "libunwind-x86"
    if not lib.is_file():
        raise SystemExit("build.sh não deixou build/libSystem.mos")
    return lib, disco


def compile_machocheck() -> Path:
    out = ROOT / "build" / "machocheck"
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.check_call(
        ["gcc", "-O2", "-Wall", "-Wextra", "-o", str(out), str(ROOT / "tools" / "machocheck.c")]
    )
    return out


def sete_zip() -> str:
    """7z do sistema, ou o 7zz oficial (7-zip.org, LGPL) em ~/.tools/7zip quando o Ubuntu não tem p7zip."""
    for nome in ("7z", "7zz"):
        achado = shutil.which(nome)
        if achado:
            return achado
    local = Path.home() / ".tools" / "7zip" / "7zz"
    if local.is_file():
        return str(local)
    raise SystemExit("falta o 7z: instale p7zip-full ou ponha o 7zz em ~/.tools/7zip")


def extrai_7z(archive: Path, dest: Path) -> None:
    """O 7z do Ubuntu devolve 2 quando o DMG tem um alias para /Applications.

    Esse aviso não apaga o .app. Outro status, ou um 2 com erro diferente, continua falha.
    """
    proc = subprocess.run(
        [sete_zip(), "x", "-y", f"-o{dest}", str(archive)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    )
    if proc.returncode == 0:
        return
    linhas = [ln.strip() for ln in proc.stderr.splitlines() if ln.strip()]
    # alias para /Applications no dmg, e os links Versions/Current dos .framework que o 7zz 25 recusa seguir
    # ("Dangerous link via another link was ignored"): os binários reais ficam em Versions/A e são medidos
    so_alias = bool(linhas) and all(
        ln.startswith("ERROR: Dangerous ") and "link" in ln and "ignored" in ln
        for ln in linhas
    )
    tem_arquivo = any(p.is_file() for p in dest.rglob("*"))
    if proc.returncode == 2 and so_alias and tem_arquivo:
        print(f"7z: {len(linhas)} alias(es) fora do pacote em {archive.name}; o app foi extraído", flush=True)
        return
    raise SystemExit(f"7z saiu {proc.returncode} em {archive.name}\n{proc.stderr[-500:]}")


def extract(archive: Path, dest: Path) -> None:
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    name = archive.name.lower()
    if name.endswith(".baixado"):
        name += detecta_formato(archive)
    if name.endswith(".pkg") or name.endswith(".mpkg"):
        extrai_pkg(archive, dest)
        return
    if name.endswith(".zip") or name.endswith(".dmg") or name.endswith(".tar.bz2") or name.endswith(".tbz"):
        extrai_7z(archive, dest)
        return
    if name.endswith(".tar.gz") or name.endswith(".tgz"):
        with tarfile.open(archive, "r:gz") as tf:
            tf.extractall(dest)
        return
    if name.endswith(".tar.xz"):
        with tarfile.open(archive, "r:xz") as tf:
            tf.extractall(dest)
        return
    raise SystemExit(f"arquivo não suportado: {archive.name}")


def extrai_pkg(archive: Path, dest: Path) -> None:
    """Pacote do instalador: xar com um ou mais Payload (cpio com gzip). Cada Payload vira uma pasta ao lado dele.

    O instalador do mOS (kernel/apps/pkgextract.c) é outro caminho; aqui só interessa chegar ao .app para medir.
    Payload em pbzx (pacotes da Apple) não é aberto pelo 7z e fica sem .app, o que a varredura registra.
    """
    extrai_7z(archive, dest)
    for _volta in range(3):
        pendentes = [p for p in dest.rglob("*") if p.is_file() and (p.name in ("Payload", "Payload~") or p.suffix == ".cpio")]
        if not pendentes:
            break
        for payload in pendentes:
            alvo = payload.parent / (payload.name.rstrip("~") + ".d")
            alvo.mkdir(exist_ok=True)
            proc = subprocess.run([sete_zip(), "x", "-y", f"-o{alvo}", str(payload)],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
            payload.unlink()
            if proc.returncode not in (0, 2):
                print(f"7z: {payload.relative_to(dest)} não abriu ({proc.returncode})", flush=True)


def find_app(root: Path) -> Path | None:
    apps = [p for p in root.rglob("*.app") if (p / "Contents").is_dir()]
    if not apps:
        return None

    def peso(p: Path) -> int:
        total = 0
        for dirpath, _dirs, files in os.walk(p):
            for name in files:
                try:
                    total += os.path.getsize(os.path.join(dirpath, name))
                except OSError:
                    pass
            if total > 400_000_000:
                break
        return total

    apps.sort(key=lambda p: (-peso(p), len(p.parts), str(p)))
    return apps[0]


def nome_do_arquivo(app: dict, url: str) -> str:
    """Nome local do download. URLs como .../stable ou download?plataforma=... não terminam em extensão."""
    base = url.rstrip("/").split("/")[-1].split("?")[0] or app["id"]
    if not any(base.lower().endswith(x) for x in (".dmg", ".zip", ".pkg", ".mpkg", ".tar.gz", ".tgz", ".tar.xz", ".tar.bz2", ".tbz")):
        base = app["id"] + "." + {"pkg": "pkg"}.get(app.get("tipo", ""), "baixado")
    return base


def detecta_formato(archive: Path) -> str:
    """Extensão de um download sem extensão conhecida, pelo cabeçalho (zip, xar, gzip, xz, bzip2; o resto é dmg)."""
    with archive.open("rb") as f:
        cab = f.read(8)
    if cab[:2] == b"PK":
        return ".zip"
    if cab[:4] == b"xar!":
        return ".pkg"
    if cab[:2] == b"\x1f\x8b":
        return ".tar.gz"
    if cab[:6] == b"\xfd7zXZ\x00":
        return ".tar.xz"
    if cab[:3] == b"BZh":
        return ".tar.bz2"
    return ".dmg"


def baixa_um(app: dict) -> tuple[Path, str]:
    """Só o download e o hash (rede, pouca RAM): fica fora da trava de trabalho pesado. Devolve (arquivo, sha256)."""
    dest_dir = CACHE / app["id"]
    dest_dir.mkdir(parents=True, exist_ok=True)
    archive = dest_dir / nome_do_arquivo(app, app["url"])
    if app.get("sha256") is None:
        # cask com sha256 no_check: o primeiro download fixa o hash; scan --fixar grava no manifesto
        if not archive.is_file():
            print(f"baixando {app['id']} (hash a fixar)", flush=True)
            download(app["url"], archive)
        app["sha256"] = sha256_file(archive)
        app["sha256_fixado_em"] = time.strftime("%Y-%m-%d")
    digest = ""
    for tentativa in range(1, 4):
        if not archive.is_file() or sha256_file(archive) != app["sha256"]:
            if archive.exists():
                archive.unlink()
            print(f"baixando {app['id']} (tentativa {tentativa})", flush=True)
            download(app["url"], archive)
        digest = sha256_file(archive)
        if digest == app["sha256"]:
            break
    if digest != app["sha256"]:
        raise SystemExit(f"{app['id']}: sha256 {digest} != {app['sha256']} depois de 3 tentativas")
    return archive, digest


def fetch_one(app: dict) -> Path:
    dest_dir = CACHE / app["id"]
    archive, digest = baixa_um(app)
    extracted = dest_dir / "extracted"
    marker = dest_dir / "extracted.ok"
    if not marker.is_file() or marker.read_text(encoding="utf-8") != digest:
        extract(archive, extracted)
        marker.write_text(digest, encoding="utf-8")
    return extracted


def imagens_no_log(texto: str) -> int:
    for line in texto.splitlines():
        if line.startswith("Imagens Mach-O"):
            return int(line.split(":")[1].split("(")[0].strip())
    return 0


def roda_machocheck(mc: Path, lib: Path, discos: list[Path], bundle: Path, report: Path, arch: str) -> str:
    cmd = [str(mc), "--arch", arch]
    if any((Path(d) / "Frameworks").is_dir() for d in discos):
        cmd.append("--disco-todo")      # frameworks do mOS (WebKit, CryptoKit, UIKit...) entram na resolução
    for d in discos:
        cmd += ["--disco", str(d)]
    cmd += [str(lib), str(bundle), str(report)]
    proc = subprocess.run(cmd, text=True, capture_output=True)
    if proc.returncode != 0:
        raise SystemExit(f"machocheck saiu {proc.returncode}\n{proc.stderr[-500:]}")
    return proc.stdout + proc.stderr


def run_one(mc: Path, lib: Path, disco, app: dict, lib_arm64: Path | None = None, discos_arm64=None) -> Path:
    """Mede a fatia x86_64; um app só-arm64 (0 imagens x86_64) é medido na fatia arm64 com o libSystem arm64."""
    discos = disco if isinstance(disco, list) else [disco]
    extracted = fetch_one(app)
    bundle = find_app(extracted)
    if bundle is None:
        raise SystemExit(f"{app['id']}: nenhum .app no arquivo")
    report = CACHE / app["id"] / "relatorio.txt"
    log = CACHE / app["id"] / "machocheck.txt"
    texto = roda_machocheck(mc, lib, discos, bundle, report, "x86_64")
    arch = "x86_64"
    if imagens_no_log(texto) == 0 and lib_arm64 is not None and lib_arm64.is_file():
        texto = roda_machocheck(mc, lib_arm64, discos_arm64 or [], bundle, report, "arm64")
        arch = "arm64"
    log.write_text(f"# arch {arch} bundle {bundle.relative_to(extracted)}\n" + texto, encoding="utf-8")
    return log


def parse_summary(text: str) -> dict:
    distinct = None
    origins = {}
    for line in text.splitlines():
        if line.startswith("Total:"):
            # Total: N imports, M sem implementacao (K simbolos distintos)
            if "(" in line and "simbolos" in line:
                chunk = line.split("(")[1].split(" ")[0]
                distinct = int(chunk)
        if line.startswith("Ausentes por origem"):
            # sistema N, libc++ N, Swift N, unwind N
            body = line.split(":", 1)[1]
            for part in body.split(","):
                bits = part.strip().split()
                if len(bits) >= 2:
                    origins[bits[0]] = int(bits[-1])
    if distinct is None:
        raise SystemExit("saída do machocheck sem a linha Total")
    return {"distintos": distinct, "origens": origins}


def cmd_baixar(args: argparse.Namespace) -> None:
    """Baixa e confere (ou fixa) o hash, sem extrair: o varrer-tudo.sh roda isto fora da trava."""
    man = load_manifest()
    want = set(args.ids)
    for app in [a for a in man["apps"] if a["id"] in want]:
        medido = CACHE / app["id"] / "medido.sha256"
        if app.get("sha256") and medido.is_file() and medido.read_text().strip() == app["sha256"]:
            continue
        try:
            baixa_um(app)
        except (SystemExit, OSError, urllib.error.URLError, ValueError) as e:
            print("FALHA download", app["id"], str(e)[:200], flush=True)   # o scan registra a falha em falhas.json
        if args.fixar:
            atual = load_manifest()                  # relê: o scan de outro app pode ter gravado no meio
            atual["apps"] = [app if a["id"] == app["id"] else a for a in atual["apps"]]
            grava_manifesto(atual)


def cmd_libsystem_ci(args: argparse.Namespace) -> None:
    """Baixa o libSystem da main que o CI compilou (artefato libsystem-main do workflow 'corpus machocheck').

    Só rede: o build fica no GitHub, não no PC do time. Grava em build/ci-lib e diz de qual commit veio.
    """
    destino = Path(args.dir)
    runs = json.loads(subprocess.check_output(
        ["gh", "run", "list", "--workflow", "compat-corpus.yml", "--branch", "main", "--event", "push",
         "--status", "success", "--limit", "10", "--json", "databaseId,headSha"], cwd=ROOT, text=True))
    for r in runs:
        tmp = destino.with_name(destino.name + ".novo")
        if tmp.exists():
            shutil.rmtree(tmp)
        p = subprocess.run(["gh", "run", "download", str(r["databaseId"]), "-n", "libsystem-main", "-D", str(tmp)],
                           cwd=ROOT, text=True, capture_output=True)
        if p.returncode == 0 and (tmp / "libSystem.mos").is_file():
            if destino.exists():
                shutil.rmtree(destino)
            tmp.replace(destino)
            print(f"libsystem-ci: main {r['headSha'][:10]} (run {r['databaseId']}) em {destino}")
            return
        shutil.rmtree(tmp, ignore_errors=True)
    raise SystemExit("libsystem-ci: nenhum run verde da main com o artefato libsystem-main")


def cmd_fetch(args: argparse.Namespace) -> None:
    man = load_manifest()
    apps = man["apps"]
    if args.ids:
        want = set(args.ids)
        apps = [a for a in apps if a["id"] in want]
    for app in apps:
        fetch_one(app)
        print("ok", app["id"], flush=True)


def grava_manifesto(man: dict) -> None:
    MANIFEST.write_text(json.dumps(man, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def limpa_binarios(app_id: str) -> None:
    """Apaga o download e a extração; ficam machocheck.txt e relatorio.txt (o que o relatório de lacunas lê)."""
    d = CACHE / app_id
    for p in d.iterdir() if d.is_dir() else []:
        if p.name in ("machocheck.txt", "relatorio.txt", "medido.sha256"):
            continue
        if p.is_dir():
            shutil.rmtree(p, ignore_errors=True)
        else:
            p.unlink()


def cmd_scan(args: argparse.Namespace) -> None:
    man = load_manifest()
    apps = man["apps"]
    if args.ids:
        want = set(args.ids)
        apps = [a for a in apps if a["id"] in want]
    elif args.gate:
        want = set(man["gate_ids"])
        apps = [a for a in apps if a["id"] in want]
    if args.lib:
        # libSystem já compilado (REGRAS do time: o build do projeto só roda na fila final)
        lib = Path(args.lib)
        discos = [Path(d) for d in (args.disco or [])]
        if not lib.is_file():
            raise SystemExit(f"--lib {lib} não existe")
    else:
        zig = ensure_zig()
        lib, d0 = build_libsystem(zig)
        discos = [d0]
    lib_a = Path(args.lib_arm64) if getattr(args, "lib_arm64", None) else None
    discos_a = [Path(d) for d in (getattr(args, "disco_arm64", None) or [])]
    mc = compile_machocheck()
    summary = {}
    arq_falhas = CACHE / "falhas.json"
    falhas = json.loads(arq_falhas.read_text(encoding="utf-8")) if arq_falhas.is_file() else {}
    for app in apps:
        falhas.pop(app["id"], None)
        medido = CACHE / app["id"] / "medido.sha256"
        if getattr(args, "continuar", False) and app.get("sha256") and medido.is_file() \
                and medido.read_text().strip() == app["sha256"] and (CACHE / app["id"] / "machocheck.txt").is_file():
            log = CACHE / app["id"] / "machocheck.txt"
        else:
            try:
                log = run_one(mc, lib, discos, app, lib_a, discos_a)
                medido.write_text(app["sha256"] + "\n")
            except (SystemExit, OSError, urllib.error.URLError, tarfile.TarError, ValueError) as e:
                if not getattr(args, "tolerante", False):
                    raise
                falhas[app["id"]] = " | ".join(str(e).splitlines())[-400:] if str(e) else type(e).__name__
                print("FALHA", app["id"], falhas[app["id"]], flush=True)
                if getattr(args, "limpar", False):
                    limpa_binarios(app["id"])
                continue
            finally:
                if getattr(args, "fixar", False):
                    grava_manifesto(man)
        texto = log.read_text(encoding="utf-8")
        summary[app["id"]] = parse_summary(texto)
        summary[app["id"]]["arch"] = texto.split()[2] if texto.startswith("# arch") else "x86_64"
        summary[app["id"]]["imagens"] = imagens_no_log(texto)
        print(app["id"], summary[app["id"]], flush=True)
        if getattr(args, "limpar", False):
            limpa_binarios(app["id"])
    out = Path(args.summary) if args.summary else CACHE / "summary.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    arq_falhas.write_text(json.dumps(falhas, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"medidos {len(summary)}, falhas {len(falhas)}", flush=True)


def framework_de(lib: str) -> str:
    if ".framework/" in lib:
        return lib.split(".framework/")[0].rsplit("/", 1)[-1]
    base = lib.rsplit("/", 1)[-1]
    return base or lib


def cmd_gaps(args: argparse.Namespace) -> None:
    man = load_manifest()
    por_sim: dict[str, set[str]] = {}
    por_origem: dict[str, dict[str, set[str]]] = {}
    por_fw: dict[str, dict[str, set[str]]] = {}
    contagem = {}
    usados = []
    for app in man["apps"]:
        log = CACHE / app["id"] / "machocheck.txt"
        rep = CACHE / app["id"] / "relatorio.txt"
        if not log.is_file() or not rep.is_file() or not medida_atual(app):
            continue
        texto = log.read_text(encoding="utf-8", errors="replace")
        imagens = 0
        for line in texto.splitlines():
            if line.startswith("Imagens Mach-O"):
                imagens = int(line.split(":")[1].split("(")[0].strip())
        if imagens == 0:
            continue
        info = parse_summary(texto)
        contagem[app["id"]] = info
        usados.append(app["id"])
        cat = ""
        for line in rep.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("# ausentes "):
                cat = line.split()[2].rstrip(":")
                por_origem.setdefault(cat, {})
                continue
            if not line or "\t" not in line or not cat:
                continue
            lib, sym = line.split("\t", 1)
            por_sim.setdefault(sym, set()).add(app["id"])
            por_origem[cat].setdefault(sym, set()).add(app["id"])
            fw = framework_de(lib)
            por_fw.setdefault(fw, {}).setdefault(sym, set()).add(app["id"])
    def rank(mapa, n=20):
        return sorted(mapa.items(), key=lambda kv: (-len(kv[1]), kv[0]))[:n]
    linhas = [
        "# Lacunas de compatibilidade no corpus macOS",
        "",
        "Gerado por `python3 tools/corpus/harness.py gaps` com as medidas atuais de `tools/corpus/varrer-tudo.sh`.",
        f"libSystem medido: {origem_libsystem()}.",
        "O disco é o da varredura: libunwind, libc++, libswift*, libz, libcurl e, com o artefato do CI, os frameworks do",
        "mOS. Os baldes libc++ e Swift só contam o que nenhuma dessas exporta.",
        "Binários não estão no git. URLs e sha256 ficam em `tools/corpus/manifest.json`.",
        "",
        f"Apps com pelo menos uma imagem Mach-O x86_64: **{len(usados)}**.",
        "",
        "## Símbolos que faltam em mais apps",
        "",
        "| símbolo | apps |",
        "|---|---:|",
    ]
    for sym, ids in rank(por_sim, 40):
        linhas.append(f"| `{sym}` | {len(ids)} |")
    linhas += ["", "## Por origem", ""]
    for cat in ("sistema", "libc++", "Swift", "unwind"):
        linhas += [f"### {cat}", "", "| símbolo | apps |", "|---|---:|"]
        bucket = por_origem.get(cat, {})
        if not bucket:
            linhas.append("| — | 0 |")
        for sym, ids in rank(bucket, 15):
            linhas.append(f"| `{sym}` | {len(ids)} |")
        linhas.append("")
    linhas += ["## Por framework", ""]
    fw_rank = sorted(por_fw.items(), key=lambda kv: (-sum(len(v) for v in kv[1].values()), kv[0]))
    for fw, mapa in fw_rank[:25]:
        linhas.append(f"### {fw}")
        linhas.append("")
        linhas.append("| símbolo | apps |")
        linhas.append("|---|---:|")
        for sym, ids in rank(mapa, 8):
            linhas.append(f"| `{sym}` | {len(ids)} |")
        linhas.append("")
    linhas.append("## Contagem distinta por app")
    linhas.append("")
    linhas.append("| app | distintos | sistema | libc++ | Swift | unwind |")
    linhas.append("|---|---:|---:|---:|---:|---:|")
    for app_id, info in sorted(contagem.items(), key=lambda kv: -kv[1]["distintos"]):
        o = info["origens"]
        linhas.append(
            f"| {app_id} | {info['distintos']} | {o.get('sistema', 0)} | {o.get('libc++', 0)} | {o.get('Swift', 0)} | {o.get('unwind', 0)} |"
        )
    linhas.append("")
    out = Path(args.out)
    out.write_text("\n".join(linhas), encoding="utf-8")
    base = {
        "nota": "Símbolos distintos ausentes no subconjunto fixo. O gate falha se algum destes subir.",
        "apps": {i: contagem[i] for i in man["gate_ids"]},
    }
    (Path(__file__).resolve().parent / "baseline.json").write_text(
        json.dumps(base, indent=2) + "\n", encoding="utf-8"
    )
    print("gaps", out, "apps", len(usados))


# ------------------------------------------------------------------ lacunas por dono (TIME.md)
DONOS = {
    "leonardo": "Leonardo (runtime C/C++)", "fernanda": "Fernanda (Swift runtime, Dispatch)",
    "eduardo": "Eduardo (Foundation/CoreFoundation)", "beatriz": "Beatriz (AppKit)",
    "ricardo": "Ricardo (SwiftUI, Combine, UIKit)", "mariana": "Mariana (CG, Core Text, ImageIO, PDFKit)",
    "otavio": "Otavio (OpenGL, Metal, Core Animation)", "isabela": "Isabela (mídia)", "fabio": "Fabio (WebKit/Electron)",
    "natalia": "Natalia (serviços e segurança)", "patricia": "Patricia (rede para apps)", "renata": "Renata (BSD/Mach/XPC)",
    "vinicius": "Vinicius (arquivos e volumes)", "larissa": "Larissa (plataforma: IOKit, USB, áudio HW)",
    "sergio": "Sergio (runtime arm64)", "thais": "Thais (GPU e telas)", "camila": "Camila (desktop e window server)",
    "felipe": "Felipe (Wi-Fi, Bluetooth, Ethernet)",
    "igor": "Igor (sem dono claro: triagem)",
    "plugin": "Plugins (flat lookup: o símbolo vem do app que carrega o plugin, não do mOS)",
}
FW_DONO = {
    "Foundation": "eduardo", "CoreFoundation": "eduardo", "CoreData": "eduardo", "libswiftFoundation.dylib": "eduardo",
    "AppKit": "beatriz", "Carbon": "beatriz", "ApplicationServices": "beatriz", "HIToolbox": "beatriz", "Cocoa": "beatriz",
    "QuickLook": "beatriz", "QuickLookUI": "beatriz", "QuickLookThumbnailing": "beatriz", "UniformTypeIdentifiers": "natalia",
    "SwiftUI": "ricardo", "Combine": "ricardo", "UIKit": "ricardo", "PhotosUI": "ricardo", "libswiftCombine.dylib": "ricardo",
    "SwiftUICore": "ricardo", "Observation": "fernanda", "libswiftObservation.dylib": "fernanda",
    "CoreGraphics": "mariana", "CoreText": "mariana", "ImageIO": "mariana", "PDFKit": "mariana", "ColorSync": "mariana",
    "libswiftCoreGraphics.dylib": "mariana", "Quartz": "mariana",
    "OpenGL": "otavio", "Metal": "otavio", "MetalKit": "otavio", "QuartzCore": "otavio", "IOSurface": "otavio",
    "CoreVideo": "otavio", "MetalPerformanceShaders": "otavio", "GLKit": "otavio", "libswiftMetal.dylib": "otavio",
    "AVFoundation": "isabela", "CoreMedia": "isabela", "VideoToolbox": "isabela", "AudioToolbox": "isabela",
    "CoreAudio": "isabela", "AVKit": "isabela", "ScreenCaptureKit": "isabela", "CoreImage": "isabela", "CoreMIDI": "isabela",
    "AudioUnit": "isabela", "MediaPlayer": "isabela", "Accelerate": "isabela", "vImage": "isabela", "vecLib": "isabela",
    "libswiftAVFoundation.dylib": "isabela", "CoreAudioKit": "isabela", "Speech": "isabela", "Vision": "isabela",
    "WebKit": "fabio", "JavaScriptCore": "fabio",
    "Security": "natalia", "CryptoKit": "natalia", "LocalAuthentication": "natalia", "UserNotifications": "natalia",
    "ServiceManagement": "natalia", "CoreServices": "natalia", "LaunchServices": "natalia", "CloudKit": "natalia",
    "AuthenticationServices": "natalia", "StoreKit": "natalia", "CoreSpotlight": "natalia", "OSAKit": "natalia",
    "ScriptingBridge": "natalia", "AppleScriptObjC": "natalia", "SecurityInterface": "natalia", "libcommonCrypto.dylib": "natalia",
    "CoreLocation": "natalia", "Contacts": "natalia", "EventKit": "natalia", "Intents": "natalia", "Sparkle": "natalia",
    "Network": "patricia", "CFNetwork": "patricia", "SystemConfiguration": "patricia", "libcurl.4.dylib": "patricia",
    "libswiftNetwork.dylib": "patricia", "NetworkExtension": "patricia", "GSS": "patricia", "Kerberos": "patricia",
    "libresolv.9.dylib": "patricia", "MultipeerConnectivity": "patricia",
    "DiskArbitration": "vinicius", "libcompression.dylib": "vinicius", "libarchive.2.dylib": "vinicius",
    "IOKit": "larissa", "IOBluetooth": "felipe", "CoreBluetooth": "felipe", "CoreWLAN": "felipe", "Hypervisor": "larissa",
    "libswiftCore.dylib": "fernanda", "libswift_Concurrency.dylib": "fernanda", "libswiftDispatch.dylib": "fernanda",
    "libswiftDarwin.dylib": "fernanda", "libswiftos.dylib": "fernanda", "libswiftObjectiveC.dylib": "fernanda",
    "libobjc.A.dylib": "fernanda",
    "libc++.1.dylib": "leonardo", "libc++abi.dylib": "leonardo", "libunwind.dylib": "leonardo",
    "OpenCL": "otavio", "AddressBook": "natalia", "OpenDirectory": "natalia", "DeviceCheck": "natalia",
    "SystemExtensions": "natalia", "OpenAL": "isabela", "CoreMediaIO": "isabela", "PIP": "isabela", "AVFAudio": "isabela",
    "SafariServices": "fabio", "ForceFeedback": "larissa", "GameController": "larissa", "CoreDisplay": "thais",
    "DisplayServices": "thais", "SkyLight": "camila", "OSD": "camila", "CoreGraphicsServices": "camila",
    # frameworks Swift da Apple que não são runtime (pedido da Fernanda): família SwiftUI com o Ricardo
    "Charts": "ricardo", "WidgetKit": "ricardo", "_StoreKit_SwiftUI": "ricardo", "Symbols": "ricardo",
    "DeveloperToolsSupport": "ricardo", "CoreTransferable": "ricardo", "_AppIntents_SwiftUI": "ricardo",
    "AppIntents": "natalia", "VisionKit": "isabela", "Accessibility": "beatriz",
    "libswift_StringProcessing.dylib": "fernanda", "libswiftSynchronization.dylib": "fernanda",
}
# imports flat/self: o símbolo é resolvido no processo que carrega a imagem (plugin). Só os do sistema têm dono.
PREFIXO_FLAT = (("eduardo", ("_CF", "_NS")), ("mariana", ("_CG", "_CT")), ("leonardo", ("__ZNSt3__", "___cxa_")),
                ("fernanda", ("_swift_", "_dispatch_", "_objc_")))
PREFIXO_LIBSYSTEM = (
    ("renata", ("_mach_", "_task_", "_thread_", "_host_", "_vm_", "_kevent", "_proc_", "_sysctl", "_xpc_", "_launch_",
                "_bootstrap_", "_sandbox_", "_ptrace", "_posix_spawn", "_audit", "_csops", "_notify_", "_os_log",
                "_os_activity", "_os_signpost", "_os_unfair", "_semaphore_", "_mig_", "_NDR_", "_kqueue", "_pid_",
                "_responsibility", "_libproc", "_getattrlist", "_dyld_", "__dyld", "_NSGetExecutablePath",
                "_semctl", "_semget", "_semop", "_shmget", "_shmat", "_shmdt", "_shmctl", "_msgget", "_msgsnd", "_msgrcv",
                "_msgctl", "_sem_open", "_sem_close", "_sem_unlink", "_shm_open", "_shm_unlink", "_kdebug", "_syscall")),
    ("fernanda", ("_dispatch_", "__dispatch")),
    ("vinicius", ("_copyfile", "_fsctl", "_fsetattrlist", "_setattrlist", "_getxattr", "_setxattr", "_listxattr",
                  "_removexattr", "_fgetxattr", "_fsetxattr", "_acl_", "_clonefile", "_fclonefileat", "_FSEvent",
                  "_statfs", "_getfsstat", "_fs_", "_searchfs", "_exchangedata", "_getdirentriesattr")),
    ("patricia", ("_dns_", "_DNSService", "_res_", "_nw_", "_getaddrinfo_async", "_SCDynamic", "_TXTRecord")),
    ("natalia", ("_CC_", "_CCCrypt", "_CCHmac", "_CCKey", "_CCRandom", "_SecRandom", "_CCDigest")),
)
SIM_DONO = (
    ("leonardo", ("__ZNSt3__", "__ZTVNSt3__", "__ZTINSt3__", "__ZTSNSt3__", "___cxa_", "__Unwind_", "_unw_", "___gxx")),
    ("ricardo", ("_$s7SwiftUI", "_$s7Combine", "_$s5UIKit")),
    ("eduardo", ("_$s10Foundation", "_$s20FoundationEssentials")),
    ("fernanda", ("_$s", "_swift_", "__swift", "_$S")),
)


# símbolo que vale mais que a biblioteca de onde o app o importa (ex.: SCPreferences pedido via IOKit em SDK antigo)
SIM_ANTES = (("patricia", ("_SCPreferences", "_SCDynamicStore", "_SCNetwork", "_kSCProp", "_kSCDynamicStore", "_kSCEnt")),
             # Core Image mora no QuartzCore no macOS, mas é a frente de mídia (Otavio)
             ("isabela", ("_OBJC_CLASS_$_CI", "_OBJC_METACLASS_$_CI", "_kCI")),
             # NSOpenGL* fica no AppKit, mas é OpenGL (Beatriz)
             ("otavio", ("_NSOpenGL", "_OBJC_CLASS_$_NSOpenGL", "_OBJC_METACLASS_$_NSOpenGL")))


def dono_de(lib: str, sym: str, origem: str) -> str:
    for dono, pre in SIM_ANTES:
        if sym.startswith(pre):
            return dono
    fw = framework_de(lib)
    if fw.endswith(".framework"):
        fw = fw[: -len(".framework")]          # o relatório do machocheck grava "AppKit.framework"
    if "flat/self" in lib or fw in ("(flat/self)", "self)"):
        for dono, pre in PREFIXO_FLAT:
            if sym.startswith(pre):
                return dono
        return "plugin"
    if fw in FW_DONO:
        return FW_DONO[fw]
    if fw.startswith("libswift"):
        alvo = fw[len("libswift"):].split(".")[0]
        if alvo in FW_DONO:
            return FW_DONO[alvo]
    for dono, pre in SIM_DONO:
        if sym.startswith(pre):
            return dono
    if fw in ("libSystem.B.dylib", "libSystem.dylib") or fw.startswith("libsystem_"):
        for dono, pre in PREFIXO_LIBSYSTEM:
            if sym.startswith(pre):
                return dono
        return "leonardo"
    if origem == "libc++" or origem == "unwind":
        return "leonardo"
    if origem == "Swift":
        return "fernanda"
    if fw.endswith(".dylib"):
        # bibliotecas abertas que o macOS traz em /usr/lib (ncurses, expat, bz2, xslt, cups, iconv, sqlite, Tcl/Tk)
        return "leonardo"
    if fw in ("Tcl", "Tk"):
        return "leonardo"
    return "igor"


def le_relatorios(man: dict):
    """[(app_id, arch, [(lib, sym, origem)])] dos apps já medidos."""
    out = []
    for app in man["apps"]:
        rep = CACHE / app["id"] / "relatorio.txt"
        log = CACHE / app["id"] / "machocheck.txt"
        if not rep.is_file() or not log.is_file() or not medida_atual(app):
            continue
        texto = log.read_text(encoding="utf-8", errors="replace")
        if imagens_no_log(texto) == 0:
            continue
        arch = texto.split()[2] if texto.startswith("# arch") else "x86_64"
        cat, itens = "", []
        for line in rep.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("# ausentes "):
                cat = line.split()[2].rstrip(":")
                continue
            if "\t" in line and cat:
                lib, sym = line.split("\t", 1)
                itens.append((lib, sym, cat))
        out.append((app, arch, itens))
    return out


def medida_atual(app: dict) -> bool:
    """Medido com o hash atual do manifesto e com o libSystem atual (varrer-tudo.sh apaga medido.sha256 quando ele muda)."""
    m = CACHE / app["id"] / "medido.sha256"
    return bool(app.get("sha256")) and m.is_file() and m.read_text().strip() == app["sha256"]


def origem_libsystem() -> str:
    ci = ROOT / "build" / "ci-lib" / "commit.txt"
    if ci.is_file():
        return f"main {ci.read_text().strip()[:10]} (artefato libsystem-main do CI)"
    local = ROOT / "build" / "libSystem.origem"
    return local.read_text().strip() if local.is_file() else "desconhecido"


def cmd_donos(args: argparse.Namespace) -> None:
    man = load_manifest()
    dados = le_relatorios(man)
    por_dono: dict[str, dict[tuple[str, str], set[str]]] = {}
    por_app: dict[str, dict[str, int]] = {}
    for app, _arch, itens in dados:
        cont: dict[str, set[str]] = {}
        for lib, sym, origem in itens:
            d = dono_de(lib, sym, origem)
            por_dono.setdefault(d, {}).setdefault((framework_de(lib), sym), set()).add(app["id"])
            cont.setdefault(d, set()).add(sym)
        por_app[app["id"]] = {d: len(v) for d, v in cont.items()}
    falhas_p = CACHE / "falhas.json"
    falhas = json.loads(falhas_p.read_text(encoding="utf-8")) if falhas_p.is_file() else {}
    medidos = {a["id"] for a, _, _ in dados}
    pendentes = [a["id"] for a in man["apps"] if a["id"] not in medidos and a["id"] not in falhas]
    ordem = sorted(por_dono, key=lambda d: -sum(len(v) for v in por_dono[d].values()))
    out_dir = Path(args.dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for velho in out_dir.glob("*.md"):
        velho.unlink()                         # dono que zerou não fica com um arquivo antigo
    L = ["# Lacunas do corpus por dono", "",
         "Gerado por `python3 tools/corpus/harness.py donos` a partir dos relatórios do `machocheck` de cada app",
         "(`tools/corpus/varrer-tudo.sh`, libSystem já compilado, sem rodar o build). Cada símbolo ausente vai para a frente",
         "do `TIME.md` dona da biblioteca que o app importa (tabela `FW_DONO` e prefixos em `tools/corpus/harness.py`).",
         "A medida é estática: um símbolo resolvido não quer dizer que a função funciona, e um app com 0 ausentes pode não abrir.",
         "",
         f"libSystem medido: {origem_libsystem()}.",
         "",
         f"Corpus: **{len(man['apps'])}** apps no manifesto; **{len(dados)}** medidos com Mach-O; **{len(falhas)}** falharam na"
         f" medição (lista no fim); **{len(pendentes)}** ainda não medidos.", "",
         "| dono | símbolos distintos | apps afetados | arquivo |", "|---|---:|---:|---|"]
    for d in ordem:
        apps_d = set().union(*por_dono[d].values())
        L.append(f"| {DONOS.get(d, d)} | {len(por_dono[d])} | {len(apps_d)} | [lacunas/{d}.md](lacunas/{d}.md) |")
    L += ["", "## Por app", "", "| app | arch | total | " + " | ".join(ordem) + " |",
          "|---|---|---:|" + "---:|" * len(ordem)]
    arch_de = {a["id"]: arch for a, arch, _ in dados}
    for app_id, c in sorted(por_app.items(), key=lambda kv: -sum(kv[1].values())):
        L.append(f"| {app_id} | {arch_de[app_id]} | {sum(c.values())} | " + " | ".join(str(c.get(d, 0)) for d in ordem) + " |")
    if falhas:
        L += ["", "## Falharam na medição", "", "| app | motivo |", "|---|---|"]
        for k, v in sorted(falhas.items()):
            L.append(f"| {k} | {v.replace('|', '/')[:200]} |")
    if pendentes:
        L += ["", "## Ainda não medidos", "", ", ".join(pendentes)]
    (out_dir.parent / "lacunas-por-dono.md").write_text("\n".join(L) + "\n", encoding="utf-8")
    for d in ordem:
        mapa = por_dono[d]
        R = [f"# Lacunas de {DONOS.get(d, d)}", "",
             f"Símbolos que os apps do corpus importam e o mOS não exporta, na frente de {DONOS.get(d, d)} (`TIME.md`).",
             "Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.", "",
             f"**{len(mapa)}** símbolos distintos.", "", "| biblioteca | símbolo | apps | exemplos |", "|---|---|---:|---|"]
        for (fw, sym), ids in sorted(mapa.items(), key=lambda kv: (-len(kv[1]), kv[0])):
            R.append(f"| {fw} | `{sym}` | {len(ids)} | {', '.join(sorted(ids)[:4])} |")
        (out_dir / f"{d}.md").write_text("\n".join(R) + "\n", encoding="utf-8")
    print(f"donos: {len(dados)} apps, {len(ordem)} donos, {out_dir.parent / 'lacunas-por-dono.md'}")




def contas_que_subiram(baseline: dict, atual: dict, ids: list[str]) -> list[str]:
    """Apps cujo número de símbolos distintos ausentes passou da baseline."""
    subiu = []
    for app_id in ids:
        base = int(baseline["apps"][app_id]["distintos"])
        cur = int(atual[app_id]["distintos"])
        if cur > base:
            subiu.append(app_id)
    return subiu


def cmd_gate(args: argparse.Namespace) -> None:
    man = load_manifest()
    baseline_path = Path(__file__).resolve().parent / "baseline.json"
    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    summary_path = CACHE / "gate-summary.json"
    ns = argparse.Namespace(ids=None, gate=True, summary=str(summary_path), lib=args.lib, disco=args.disco)
    cmd_scan(ns)
    got = json.loads(summary_path.read_text(encoding="utf-8"))
    subiu = contas_que_subiram(baseline, got, man["gate_ids"])
    for app_id in man["gate_ids"]:
        base = baseline["apps"][app_id]["distintos"]
        cur = got[app_id]["distintos"]
        print(f"{app_id}: baseline {base}, agora {cur}")
    if subiu:
        for app_id in subiu:
            print(f"FALHA: {app_id} subiu de {baseline['apps'][app_id]['distintos']} para {got[app_id]['distintos']} símbolos distintos ausentes")
        raise SystemExit(1)
    print("gate ok")


def testa_alias_do_dmg() -> None:
    """O Rectangle.dmg faz o 7z sair 2 por um alias para /Applications. O .app tem de ficar."""
    work = CACHE / "alias-hosttest"
    if work.exists():
        shutil.rmtree(work)
    src = work / "src"
    mac = src / "Rectangle.app" / "Contents" / "MacOS"
    mac.mkdir(parents=True)
    (mac / "Rectangle").write_text("binario", encoding="utf-8")
    os.symlink("/Applications", src / "Applications")
    archive = work / "Rectangle.zip"
    subprocess.check_call(["zip", "-ry", str(archive), "."], cwd=src, stdout=subprocess.DEVNULL)
    dest = work / "out"
    extract(archive, dest)
    got = (dest / "Rectangle.app" / "Contents" / "MacOS" / "Rectangle").read_text(encoding="utf-8")
    if got != "binario":
        raise SystemExit("o 7z aceitou o alias e perdeu o executável")
    ruim = work / "vazio.dmg"
    ruim.write_bytes(b"nao e um dmg")
    try:
        extract(ruim, work / "vazio-out")
    except SystemExit:
        pass
    else:
        raise SystemExit("um arquivo inválido passou na extração")
    print("hosttest extração: alias do Rectangle aceito, lixo recusado")


def cmd_hosttest(args: argparse.Namespace) -> None:
    testa_alias_do_dmg()
    amostra = (
        "Total: 3 imports, 1 sem implementacao (1 simbolos distintos)\n"
        "Ausentes por origem (simbolos distintos): sistema 1, libc++ 0, Swift 0, unwind 0\n"
    )
    got = parse_summary(amostra)
    if got["distintos"] != 1 or got["origens"].get("sistema") != 1:
        raise SystemExit(f"parse_summary não leu a conta real: {got}")
    base = {"apps": {"a": {"distintos": 2}, "b": {"distintos": 5}}}
    if contas_que_subiram(base, {"a": {"distintos": 2}, "b": {"distintos": 4}}, ["a", "b"]):
        raise SystemExit("o gate acusou subida quando a conta caiu ou ficou igual")
    ruim = contas_que_subiram(base, {"a": {"distintos": 3}, "b": {"distintos": 5}}, ["a", "b"])
    if ruim != ["a"]:
        raise SystemExit(f"o gate não apontou só o app que piorou: {ruim}")

    for lib_, sym_, orig_, esperado in (
        ("/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit", "_NSBeep", "sistema", "beatriz"),
        ("/usr/lib/libSystem.B.dylib", "_dispatch_after", "sistema", "fernanda"),
        ("/usr/lib/libSystem.B.dylib", "_mach_port_allocate", "sistema", "renata"),
        ("/usr/lib/libSystem.B.dylib", "_copyfile", "sistema", "vinicius"),
        ("libSystem.B.dylib", "_semget", "sistema", "renata"),
        ("/usr/lib/libSystem.B.dylib", "_strlcpy", "sistema", "leonardo"),
        ("/usr/lib/libc++.1.dylib", "__ZNSt3__14coutE", "libc++", "leonardo"),
        ("/usr/lib/swift/libswiftCore.dylib", "_swift_retain", "Swift", "fernanda"),
        ("/usr/lib/swift/libswiftFoundation.dylib", "_$s10Foundation4DataV", "Swift", "eduardo"),
        ("/System/Library/Frameworks/SwiftUI.framework/Versions/A/SwiftUI", "_$s7SwiftUI4ViewMp", "Swift", "ricardo"),
        ("/System/Library/Frameworks/Metal.framework/Versions/A/Metal", "_MTLCreateSystemDefaultDevice", "sistema", "otavio"),
        ("/System/Library/PrivateFrameworks/Algo.framework/Algo", "_Algo", "sistema", "igor"),
        ("AppKit.framework", "_NSBeep", "sistema", "beatriz"),
        ("OpenCL.framework", "_clGetPlatformIDs", "sistema", "otavio"),
        ("CoreWLAN.framework", "_OBJC_CLASS_$_CWInterface", "sistema", "felipe"),
        ("IOKit.framework", "_SCPreferencesCreate", "sistema", "patricia"),
        ("QuartzCore.framework", "_OBJC_CLASS_$_CIContext", "sistema", "isabela"),
        ("QuartzCore.framework", "_kCIInputImageKey", "sistema", "isabela"),
        ("QuartzCore.framework", "_OBJC_CLASS_$_CALayer", "sistema", "otavio"),
        ("AppKit.framework", "_NSOpenGLGetVersion", "sistema", "otavio"),
        ("Charts.framework", "_$s6Charts9ChartViewV", "Swift", "ricardo"),
        ("AppIntents.framework", "_$s10AppIntents0A6IntentP", "Swift", "natalia"),
        ("IOKit.framework", "_IOServiceGetMatchingService", "sistema", "larissa"),
        ("(flat/self)", "_PyBool_FromLong", "sistema", "plugin"),
        ("(flat/self)", "_CFBundleGetBundleWithIdentifier", "sistema", "eduardo"),
        ("libncurses.5.4.dylib", "_initscr", "sistema", "leonardo"),
    ):
        if dono_de(lib_, sym_, orig_) != esperado:
            raise SystemExit(f"dono_de({sym_}) = {dono_de(lib_, sym_, orig_)}, esperado {esperado}")
    print("hosttest donos: mapa de frentes ok")

    lib = Path(args.lib)
    disco = Path(args.disco)
    if not lib.is_file() or not disco.is_dir():
        raise SystemExit(f"falta libSystem ({lib}) ou libunwind ({disco}) do build.sh")
    zig = ensure_zig()
    mc = compile_machocheck()
    work = CACHE / "hosttest"
    if work.exists():
        shutil.rmtree(work)
    macos = work / "HostTest.app" / "Contents" / "MacOS"
    macos.mkdir(parents=True)
    src = work / "ausente.c"
    src.write_text(
        "extern void mos_corpus_ausente(void);\n"
        "void mos_corpus_entry(void) { mos_corpus_ausente(); }\n",
        encoding="utf-8",
    )
    dylib = macos / "HostTest"
    subprocess.check_call(
        [str(zig), "cc", "-target", "x86_64-macos", "-dynamiclib", "-undefined", "dynamic_lookup",
         "-o", str(dylib), str(src)]
    )
    report = work / "relatorio.txt"
    log = work / "machocheck.txt"
    proc = subprocess.run(
        [str(mc), "--disco", str(disco), str(lib), str(work / "HostTest.app"), str(report)],
        text=True, capture_output=True,
    )
    log.write_text(proc.stdout + proc.stderr, encoding="utf-8")
    if proc.returncode != 0:
        raise SystemExit(f"machocheck no hosttest saiu {proc.returncode}\n{proc.stderr[-400:]}")
    texto = report.read_text(encoding="utf-8")
    if "mos_corpus_ausente" not in texto:
        raise SystemExit("o símbolo inventado não apareceu como ausente; o machocheck não mediu o binário")
    if "_malloc" in texto or "\tmalloc\n" in texto:
        raise SystemExit("malloc saiu como ausente; a tabela do libSystem não entrou na conta")
    info = parse_summary(log.read_text(encoding="utf-8"))
    if info["distintos"] < 1:
        raise SystemExit("conta distinta zerou num binário que importa símbolo inexistente")
    print(f"hosttest ok: {info['distintos']} símbolo(s) distinto(s) ausente(s), mos_corpus_ausente medido")


def main() -> None:
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="cmd", required=True)
    lc = sub.add_parser("libsystem-ci")
    lc.add_argument("--dir", default=str(ROOT / "build" / "ci-lib"))
    lc.set_defaults(func=cmd_libsystem_ci)
    bx = sub.add_parser("baixar")
    bx.add_argument("--fixar", action="store_true")
    bx.add_argument("ids", nargs="+")
    bx.set_defaults(func=cmd_baixar)
    f = sub.add_parser("fetch")
    f.add_argument("ids", nargs="*")
    f.set_defaults(func=cmd_fetch)
    s = sub.add_parser("scan")
    s.add_argument("--gate", action="store_true")
    s.add_argument("--summary")
    s.add_argument("--lib", help="libSystem.mos já compilado (não roda tools/build.sh)")
    s.add_argument("--disco", action="append", help="libc++/libswift*/libunwind/libz/libcurl do build (repetível)")
    s.add_argument("--lib-arm64", help="libSystem-arm64.mos para apps sem fatia x86_64")
    s.add_argument("--disco-arm64", action="append")
    s.add_argument("--fixar", action="store_true", help="grava no manifesto o sha256 dos casks no_check")
    s.add_argument("--limpar", action="store_true", help="apaga download e extração depois de medir")
    s.add_argument("--continuar", action="store_true", help="pula o app já medido com o mesmo sha256")
    s.add_argument("--tolerante", action="store_true", help="registra a falha do app em falhas.json e segue")
    s.add_argument("ids", nargs="*")
    s.set_defaults(func=cmd_scan)
    g = sub.add_parser("gate")
    g.add_argument("--lib")
    g.add_argument("--disco", action="append")
    g.set_defaults(func=cmd_gate)
    gaps = sub.add_parser("gaps")
    gaps.add_argument("--out", default=str(ROOT / "docs" / "compat-gaps.md"))
    gaps.set_defaults(func=cmd_gaps)
    dn = sub.add_parser("donos")
    dn.add_argument("--dir", default=str(ROOT / "docs" / "lacunas"))
    dn.set_defaults(func=cmd_donos)
    h = sub.add_parser("hosttest")
    h.add_argument("--lib", required=True)
    h.add_argument("--disco", required=True)
    h.set_defaults(func=cmd_hosttest)
    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
