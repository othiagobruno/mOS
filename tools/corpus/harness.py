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


def extract(archive: Path, dest: Path) -> None:
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    name = archive.name.lower()
    if name.endswith(".pkg"):
        raise SystemExit(f"{archive.name}: pacote .pkg fora do corpus")
    if name.endswith(".zip") or name.endswith(".dmg") or name.endswith(".tar.bz2") or name.endswith(".tbz"):
        subprocess.check_call(["7z", "x", "-y", f"-o{dest}", str(archive)], stdout=subprocess.DEVNULL)
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


def find_app(root: Path) -> Path | None:
    apps = [p for p in root.rglob("*.app") if p.is_dir()]
    if not apps:
        return None
    apps.sort(key=lambda p: (len(p.parts), str(p)))
    return apps[0]


def fetch_one(app: dict) -> Path:
    dest_dir = CACHE / app["id"]
    dest_dir.mkdir(parents=True, exist_ok=True)
    filename = app["url"].rstrip("/").split("/")[-1].split("?")[0]
    archive = dest_dir / filename
    if not archive.is_file() or sha256_file(archive) != app["sha256"]:
        print(f"baixando {app['id']}", flush=True)
        download(app["url"], archive)
    digest = sha256_file(archive)
    if digest != app["sha256"]:
        raise SystemExit(f"{app['id']}: sha256 {digest} != {app['sha256']}")
    extracted = dest_dir / "extracted"
    marker = dest_dir / "extracted.ok"
    if not marker.is_file():
        extract(archive, extracted)
        marker.write_text(digest, encoding="utf-8")
    return extracted


def run_one(mc: Path, lib: Path, disco: Path, app: dict) -> Path:
    extracted = fetch_one(app)
    bundle = find_app(extracted)
    if bundle is None:
        raise SystemExit(f"{app['id']}: nenhum .app no arquivo")
    report = CACHE / app["id"] / "relatorio.txt"
    log = CACHE / app["id"] / "machocheck.txt"
    cmd = [str(mc), "--disco", str(disco), str(lib), str(bundle), str(report)]
    proc = subprocess.run(cmd, text=True, capture_output=True)
    log.write_text(proc.stdout + proc.stderr, encoding="utf-8")
    if proc.returncode != 0:
        raise SystemExit(f"{app['id']}: machocheck saiu {proc.returncode}\n{proc.stderr[-500:]}")
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


def cmd_fetch(args: argparse.Namespace) -> None:
    man = load_manifest()
    apps = man["apps"]
    if args.ids:
        want = set(args.ids)
        apps = [a for a in apps if a["id"] in want]
    for app in apps:
        fetch_one(app)
        print("ok", app["id"], flush=True)


def cmd_scan(args: argparse.Namespace) -> None:
    man = load_manifest()
    apps = man["apps"]
    if args.ids:
        want = set(args.ids)
        apps = [a for a in apps if a["id"] in want]
    elif args.gate:
        want = set(man["gate_ids"])
        apps = [a for a in apps if a["id"] in want]
    zig = ensure_zig()
    lib, disco = build_libsystem(zig)
    mc = compile_machocheck()
    summary = {}
    for app in apps:
        log = run_one(mc, lib, disco, app)
        summary[app["id"]] = parse_summary(log.read_text(encoding="utf-8"))
        print(app["id"], summary[app["id"]], flush=True)
    out = Path(args.summary) if args.summary else CACHE / "summary.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")


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
        if not log.is_file() or not rep.is_file():
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
        "Gerado por `python3 tools/corpus/harness.py gaps` depois de `machocheck --disco build/libunwind-x86`.",
        "O disco aponta a libunwind do `tools/build.sh`. Não há `libc++.1.dylib` nem `libswift*` nessa árvore,",
        "então os baldes libc++ e Swift continuam os imports que o bundle pede e o libSystem não exporta.",
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


def cmd_gate(_: argparse.Namespace) -> None:
    man = load_manifest()
    baseline_path = Path(__file__).resolve().parent / "baseline.json"
    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    summary_path = CACHE / "gate-summary.json"
    ns = argparse.Namespace(ids=None, gate=True, summary=str(summary_path))
    cmd_scan(ns)
    got = json.loads(summary_path.read_text(encoding="utf-8"))
    failed = False
    for app_id in man["gate_ids"]:
        base = baseline["apps"][app_id]["distintos"]
        cur = got[app_id]["distintos"]
        print(f"{app_id}: baseline {base}, agora {cur}")
        if cur > base:
            print(f"FALHA: {app_id} subiu de {base} para {cur} símbolos distintos ausentes")
            failed = True
    if failed:
        raise SystemExit(1)
    print("gate ok")


def main() -> None:
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="cmd", required=True)
    f = sub.add_parser("fetch")
    f.add_argument("ids", nargs="*")
    f.set_defaults(func=cmd_fetch)
    s = sub.add_parser("scan")
    s.add_argument("--gate", action="store_true")
    s.add_argument("--summary")
    s.add_argument("ids", nargs="*")
    s.set_defaults(func=cmd_scan)
    g = sub.add_parser("gate")
    g.set_defaults(func=cmd_gate)
    gaps = sub.add_parser("gaps")
    gaps.add_argument("--out", default=str(ROOT / "docs" / "compat-gaps.md"))
    gaps.set_defaults(func=cmd_gaps)
    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
