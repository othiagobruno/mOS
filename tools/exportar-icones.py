#!/usr/bin/env python3
"""Exporta os originais RGBA do mOS em PNG, preservando transparência.

Requer Pillow. Executar da raiz ou de qualquer diretório.
Os tamanhos @2x indicam densidade; não adicionam detalhe ao original.
"""
from pathlib import Path
import json

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SIZES = (16, 32, 64, 128, 256, 512, 1024)
NAMES = json.loads((ROOT / "assets/icones/catalogo.json").read_text())


def export_bases(destination):
    for name in ("calendario", "relogio"):
        with Image.open(ROOT / f"assets/icones/bases/{name}.png") as source:
            source = source.convert("RGBA")
            for size in SIZES[:5]:
                image = source.convert("RGBa").resize((size, size), Image.Resampling.LANCZOS).convert("RGBA")
                image.save(destination / f"{name}-base-{size}.png")


def main():
    destination = ROOT / "root/mOS/Sistema/Biblioteca/Icones"
    destination.mkdir(parents=True, exist_ok=True)
    for name in NAMES:
        with Image.open(ROOT / f"assets/icones/originais/{name}.png") as source:
            source = source.convert("RGBA")
            if source.width != source.height:
                raise ValueError(f"Original precisa ser quadrado: {name}")
            for size in SIZES:
                for density in (1, 2):
                    pixels = size * density
                    suffix = "@2x" if density == 2 else ""
                    # RGBa pré-multiplica durante a redução para evitar halos.
                    image = source.convert("RGBa").resize(
                        (pixels, pixels), Image.Resampling.LANCZOS
                    ).convert("RGBA")
                    image.save(destination / f"{name}-{size}{suffix}.png")
        print(f"{name}: 14 PNGs exportados")
    export_bases(destination)


if __name__ == "__main__":
    main()
