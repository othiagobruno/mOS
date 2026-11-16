#!/usr/bin/env python3
"""Confere cobertura dos apps e variantes PNG sem compilar o mOS (Pillow)."""
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SIZES = (16, 32, 64, 128, 256, 512, 1024)


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def main():
    names = json.loads((ROOT / "assets/icones/catalogo.json").read_text())
    require(len(names) == len(set(names)), "Nomes repetidos no catálogo")
    require(all(re.fullmatch(r"[a-z]+", name) for name in names), "Nome inválido")
    registry = (ROOT / "kernel/apps/registry.c").read_text()
    registered = re.findall(r"&app_(\w+)", registry.split("all_apps[] = {")[1].split("};")[0])
    table = (ROOT / "kernel/ui/icons_tahoe.c").read_text().split("} table[] = {")[1].split("};")[0]
    renderers = re.findall(r'\{"([^"]+)",', table)
    require(set(names) == set(registered) == set(renderers), "Catálogo difere do registro ou carregador")
    originals = ROOT / "assets/icones/originais"
    require({p.stem for p in originals.glob("*.png")} == set(names), "Originais faltantes ou excedentes")
    digests = set()
    expected = set()
    runtime_files, runtime_pixels = [], []
    destination = ROOT / "assets/icones/png"
    runtime = ROOT / "root/mOS/Sistema/Biblioteca/Icones"
    runtime_expected = set()
    artworks = [(name, originals / f"{name}.png", SIZES, (1, 2)) for name in names]
    bases = ROOT / "assets/icones/bases"
    require({p.stem for p in bases.glob("*.png")} == {"calendario", "relogio"}, "Bases dinâmicas faltantes ou excedentes")
    artworks += [(name + "-base", bases / f"{name}.png", SIZES[:5], (1,)) for name in ("calendario", "relogio")]
    for name, source, sizes, densities in artworks:
        digest = hashlib.sha256(source.read_bytes()).digest()
        require(digest not in digests, f"Original repetido: {name}")
        digests.add(digest)
        with Image.open(source) as image:
            image.load()
            require(image.width == image.height and image.mode == "RGBA", f"Original inválido: {name}")
        for size in sizes:
            for density in densities:
                suffix = "@2x" if density == 2 else ""
                filename = f"{name}-{size}{suffix}.png"
                expected.add(filename)
                path = (runtime if name.endswith("-base") else destination) / filename
                require(path.exists(), f"Exportação ausente: {filename}")
                with Image.open(path) as image:
                    image.verify()
                with Image.open(path) as image:
                    image.load()
                    pixels = size * density
                    require(image.size == (pixels, pixels) and image.mode == "RGBA", f"Formato incorreto: {filename}")
                    require(image.getchannel("A").getextrema() == (0, 255), f"Alfa inválido: {filename}")
                    corners = ((0, 0), (pixels - 1, 0), (0, pixels - 1), (pixels - 1, pixels - 1))
                    require(max(image.getpixel(p)[3] for p in corners) <= 8, f"Canto opaco: {filename}")
                    if len(sys.argv) > 1 and density == 1 and size <= 256:
                        hash_value = 14695981039346656037
                        pixels_data = image.get_flattened_data() if hasattr(image, "get_flattened_data") else image.getdata()
                        for r, g, b, a in pixels_data:
                            pixel = a << 24 | (r * a // 255) << 16 | (g * a // 255) << 8 | b * a // 255
                            hash_value = ((hash_value ^ pixel) * 1099511628211) & ((1 << 64) - 1)
                        runtime_files.append(str(path))
                        runtime_pixels.append((pixels, pixels, hash_value))
                if density == 1 and size <= 256:
                    require(path.stat().st_size <= 512 * 1024, f"Acima do limite do carregador: {filename}")
                    runtime_expected.add(filename)
                    require((runtime / filename).exists() and (runtime / filename).read_bytes() == path.read_bytes(), f"Payload diferente: {filename}")
    actual_names = {p.name for p in destination.glob("*.png")} | {p.name for p in runtime.glob("*-base-*.png")}
    require(actual_names == expected, "Exportações excedentes")
    require({p.name for p in runtime.glob("*.png")} == runtime_expected, "Payload contém variantes desnecessárias ou incompletas")
    if len(sys.argv) > 1:
        decoded = subprocess.run([str(Path(sys.argv[1]).resolve()), *runtime_files],
                                 capture_output=True, text=True, check=True)
        actual = [tuple(map(int, line.split())) for line in decoded.stdout.splitlines()]
        require(actual == runtime_pixels, "Pixels do decodificador do kernel diferem do Pillow")
        print(f"{len(actual)} PNGs de até 256 px: pixels idênticos entre kernel e Pillow")
    print(f"{len(names)}/{len(registered)} apps; {len(digests)} fontes distintas (inclui 2 bases); {len(expected)} PNGs válidos")
    print(f"Payload: {len(runtime_expected)} PNGs, {sum(p.stat().st_size for p in runtime.glob('*.png'))} bytes")


if __name__ == "__main__":
    main()
