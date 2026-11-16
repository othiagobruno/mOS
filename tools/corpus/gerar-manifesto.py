#!/usr/bin/env python3
"""Amplia tools/corpus/manifest.json para os N apps de macOS fora da App Store mais instalados.

Fonte do ranking: as instalações de 365 dias do Homebrew Cask (formulae.brew.sh/api/analytics/cask-install/365d.json).
Fonte do artefato: formulae.brew.sh/api/cask.json (url e sha256 publicados pelo cask). Todo cask é distribuído fora da
Mac App Store; os apps da MAS têm FairPlay e não entram no corpus.

Regras:
- As entradas que já estão no manifesto ficam como estão (url e sha256 fixados por uma sessão anterior).
- Só entram casks com artefato `app` ou `pkg`; casks deprecados ou desativados e variantes `nome@canal` ficam de fora.
- url: a variação Intel do cask quando existe (o mOS roda x86_64 direto); url_arm64: a padrão do cask quando é outra.
- sha256 "no_check" no cask vira null: `harness.py scan --fixar` baixa, calcula e grava o hash no manifesto.

Uso: gerar-manifesto.py [--total 300] [--cask cask.json] [--analytics 365d.json]
"""
from __future__ import annotations

import argparse
import json
import urllib.request
from pathlib import Path

AQUI = Path(__file__).resolve().parent
MANIFEST = AQUI / "manifest.json"
CASK_URL = "https://formulae.brew.sh/api/cask.json"
AN_URL = "https://formulae.brew.sh/api/analytics/cask-install/365d.json"
# Nomes de variação do Homebrew sem prefixo arm64_ são da fatia Intel, do mais novo para o mais antigo.
INTEL = ("tahoe", "sequoia", "sonoma", "ventura", "monterey", "big_sur", "catalina", "mojave", "high_sierra", "sierra", "el_capitan")


def baixa_json(url_ou_arquivo: str | None, padrao: str):
    if url_ou_arquivo and Path(url_ou_arquivo).is_file():
        return json.loads(Path(url_ou_arquivo).read_text(encoding="utf-8"))
    req = urllib.request.Request(url_ou_arquivo or padrao, headers={"User-Agent": "mOS-corpus-harness"})
    with urllib.request.urlopen(req, timeout=120) as r:
        return json.loads(r.read().decode("utf-8"))


def tipo(cask: dict) -> str | None:
    chaves = {k for art in cask.get("artifacts", []) for k in art}
    if "app" in chaves:
        return "app"
    if "pkg" in chaves:
        return "pkg"
    return None


def urls(cask: dict) -> tuple[str, str | None, str | None, str | None]:
    """(url, sha256, url_arm64, sha256_arm64): a Intel primeiro, a padrão do cask como arm64 quando difere."""
    padrao_url, padrao_sha = cask["url"], cask.get("sha256")
    var = cask.get("variations") or {}
    intel = None
    for nome in INTEL:
        v = var.get(nome)
        if v and v.get("url"):
            intel = (v["url"], v.get("sha256", padrao_sha))
            break
    sha = lambda s: None if not s or s == "no_check" else s
    if intel and intel[0] != padrao_url:
        return intel[0], sha(intel[1]), padrao_url, sha(padrao_sha)
    return padrao_url, sha(padrao_sha), None, None


def escolhe(casks: dict, ranking: list, existentes: set[str], faltam: int) -> list[dict]:
    novos = []
    vistos = set(existentes)
    for item in ranking:
        if len(novos) >= faltam:
            break
        token = item["cask"]
        c = casks.get(token)
        if not c or "@" in token or c.get("deprecated") or c.get("disabled"):
            continue
        k = tipo(c)
        if not k or token in vistos:
            continue
        vistos.add(token)
        url, sha, url_a, sha_a = urls(c)
        e = {
            "id": token,
            "nome": (c.get("name") or [token])[0],
            "licenca": "não declarada no cask; download público do autor, usado só para medir (não redistribuído)",
            "url": url,
            "sha256": sha,
            "cask": token,
            "versao": c.get("version"),
            "tipo": k,
            "homepage": c.get("homepage"),
            "rank_brew_365d": int(item["number"]),
            "instalacoes_365d": int(str(item["count"]).replace(",", "")),
        }
        if url_a:
            e["url_arm64"] = url_a
            e["sha256_arm64"] = sha_a
        novos.append(e)
    return novos


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--total", type=int, default=300)
    p.add_argument("--cask")
    p.add_argument("--analytics")
    a = p.parse_args()
    man = json.loads(MANIFEST.read_text(encoding="utf-8"))
    casks = {c["token"]: c for c in baixa_json(a.cask, CASK_URL)}
    ranking = baixa_json(a.analytics, AN_URL)["items"]
    pos = {it["cask"]: it for it in ranking}
    existentes = set()
    for e in man["apps"]:
        tok = e.get("cask") or e["id"]
        if tok not in casks and e["nome"].lower().replace(" ", "-") in casks:
            tok = e["nome"].lower().replace(" ", "-")
        existentes.add(tok)
        existentes.add(e["id"])
        if tok in pos and "rank_brew_365d" not in e:
            e["cask"] = tok
            e["rank_brew_365d"] = int(pos[tok]["number"])
            e["instalacoes_365d"] = int(str(pos[tok]["count"]).replace(",", ""))
    ids_antes = len(man["apps"])
    novos = escolhe(casks, ranking, existentes, max(0, a.total - ids_antes))
    man["apps"].extend(novos)
    man["fonte_ranking"] = AN_URL
    MANIFEST.write_text(json.dumps(man, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"manifesto: {ids_antes} existentes + {len(man['apps']) - ids_antes} novos = {len(man['apps'])}")


if __name__ == "__main__":
    main()
