#!/usr/bin/env python3
"""Lê no Ubuntu o log de boot que o mOS grava em \\EFI\\mOS\\boot.log da ESP (docs/instalar-no-pc.md).

O arquivo tem 4 MiB alocados pelo instalador (tools/instalar-dual-boot.py), em 2 slots de 2 MiB. Cada boot usa o slot
(N mod 2): a primeira linha é "mOS boot <N> <versão>", depois o texto do console em sequência e zeros até o fim. Mostra
o boot mais novo primeiro; --todos mostra também o anterior.

    tools/ler-boot-log.py                       # /boot/efi/EFI/mOS/boot.log (pode precisar de sudo para ler)
    tools/ler-boot-log.py --todos caminho/boot.log
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

SLOT = 2 << 20
CAB = re.compile(rb"^mOS boot (\d+)(?: ([^\n]*))?\n")


def slots(dados: bytes) -> list[tuple[int, str, str]]:
    """(N, versão, texto) de cada slot com cabeçalho válido, do mais novo para o mais antigo."""
    achados = []
    for i in range(0, len(dados), SLOT):
        bloco = dados[i:i + SLOT]
        m = CAB.match(bloco)
        if not m:
            continue
        corpo = bloco[m.end():].split(b"\0", 1)[0]
        achados.append((int(m.group(1)), (m.group(2) or b"").decode("utf-8", "replace"), corpo.decode("utf-8", "replace")))
    achados.sort(key=lambda s: -s[0])
    return achados


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("arquivo", nargs="?", default="/boot/efi/EFI/mOS/boot.log")
    p.add_argument("--todos", action="store_true", help="mostra também o boot anterior")
    a = p.parse_args()
    try:
        dados = Path(a.arquivo).read_bytes()
    except OSError as e:
        print(f"ler-boot-log: não li {a.arquivo}: {e.strerror}", file=sys.stderr)
        return 1
    achados = slots(dados)
    if not achados:
        print("ler-boot-log: nenhum boot registrado (o mOS ainda não bootou, ou caiu antes de gravar a primeira linha)")
        return 0
    for n, versao, texto in achados if a.todos else achados[:1]:
        print(f"===== mOS boot {n} {versao} =====")
        print(texto, end="" if texto.endswith("\n") else "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
