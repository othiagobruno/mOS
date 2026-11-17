#!/usr/bin/env python3
"""Instala o mOS em dual boot com o Ubuntu, a partir do próprio Ubuntu, sem apagar nada dele.

Onde o mOS fica: na partição EFI (ESP) que já existe e o Ubuntu usa (no PC do Thiago, /dev/sda1, FAT32 de 1 GiB,
montada em /boot/efi). Nenhuma partição é criada, movida, redimensionada ou formatada; a tabela GPT não é tocada.
Só aparecem duas pastas novas na ESP:
    \\mOS\\                  o sistema (os arquivos do manifesto do payload, cada um conferido por SHA-256) e, depois,
                            as contas e os documentos criados no mOS
    \\EFI\\mOS\\BOOTX64.EFI   o carregador do mOS
    \\EFI\\mOS\\boot.log      log de boot (4 MiB alocados aqui; o kernel só reescreve o conteúdo)
\\EFI\\BOOT, \\EFI\\ubuntu e o resto da ESP não são lidos para escrita nem alterados.

A mesma ordem de escrita do instalador do kernel (kernel/fs/install.c): estado INSTALANDO, cópia de cada arquivo
(arquivo temporário + fsync + renomear), releitura de todos com SHA-256, preferências, manifesto instalado, carregador
como MOSNEW.TMP conferido, estado PRIMEIRO_BOOT, e por último o carregador renomeado para BOOTX64.EFI. Uma queda de
energia no meio deixa a ESP sem um mOS que dê boot, e o Ubuntu continua bootando.

Entrada de boot: uma entrada "mOS" na NVRAM (efibootmgr) colocada no FIM da BootOrder, então o Ubuntu continua sendo o
padrão; e uma entrada "mOS" no menu do GRUB do Ubuntu (/etc/grub.d/42_mos + update-grub). --proximo-boot faz só o
próximo boot ir para o mOS (BootNext).

Padrão: SIMULA e mostra o plano; não grava nada. Gravar exige --gravar e a confirmação digitada do nome da partição
(ex.: sda1), como no instalador do mOS.

    sudo tools/instalar-dual-boot.py --imagem build/mOS.img                 # simulação
    sudo tools/instalar-dual-boot.py --imagem build/mOS.img --gravar        # instala (pede "sda1")
    sudo tools/instalar-dual-boot.py --imagem build/mOS.img --gravar --atualizar   # troca o sistema, mantém as contas
    sudo tools/instalar-dual-boot.py --desinstalar --gravar                 # tira o mOS da ESP, da NVRAM e do GRUB
    sudo tools/instalar-dual-boot.py --concluir --gravar                    # depois do 1º boot: libera a persistência

Teste de host sem root e sem disco: tools/instalar-dual-boot-teste.sh.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ESP_GUID = "c12a7328-f81f-11d2-ba4b-00a0c93ec93b"
MANIFESTO = "mOS/Sistema/Instalacao/manifesto.txt"
CARREGADOR_MANIFESTO = "/EFI/BOOT/BOOTX64.EFI"          # nome do carregador no payload
CARREGADOR = "EFI/mOS/BOOTX64.EFI"                       # onde ele fica na ESP compartilhada
CARREGADOR_TMP = "EFI/mOS/MOSNEW.TMP"
CARREGADOR_EFI = "\\EFI\\mOS\\BOOTX64.EFI"
# Log de boot: arquivo já alocado pelo instalador (2 slots de 2 MiB, zerados). No primeiro boot o kernel só escreve DENTRO
# dele (os clusters dele, sem alocar nem mudar diretório); tools/ler-boot-log.py lê no Ubuntu. docs/instalar-no-pc.md
BOOTLOG = "EFI/mOS/boot.log"
BOOTLOG_TAM = 4 << 20
ESTADO = "mOS/Sistema/Biblioteca/estado-sistema"
PREFS = "mOS/Sistema/Biblioteca/Preferencias/com.mos.sistema.cfg"
INSTALADO = "mOS/Sistema/Atualizacao/instalado.txt"
GRUB_ARQ = "etc/grub.d/42_mos"
ROTULO = "mOS"
MIB = 1 << 20
# mesma política de kernel/fs/install.c (install_path_allowed) e do mkimg
NEGADOS = ("/mOS/Users", "/mOS/Sistema/Biblioteca/Usuarios", "/mOS/Sistema/Biblioteca/Logs",
           "/mOS/Sistema/Biblioteca/Preferencias", "/mOS/Sistema/Instalacao", "/mOS/tmp", "/mOS/Volumes",
           "/mOS/Sistema/Biblioteca/estado-sistema", "/mOS/Sistema/Atualizacao")


class Recusa(Exception):
    """Motivo para não gravar. Levantada antes da primeira escrita sempre que possível."""


def caminho_permitido(p: str) -> bool:
    if len(p) < 2 or len(p) > 1000 or not p.startswith("/"):
        return False
    if ".." in p or "\\" in p or "//" in p or p.endswith("/"):
        return False
    if p != CARREGADOR_MANIFESTO and not p.startswith("/mOS/"):
        return False
    for d in NEGADOS:
        if p == d or p.startswith(d + "/"):
            return False
    base = p.rsplit("/", 1)[1]
    if base.endswith(".conta") or base.endswith(".log") or "/.Lixeira" in p:
        return False
    return True


def le_manifesto(origem: Path) -> tuple[bytes, list[tuple[str, int, str]], str]:
    """(texto bruto, [(sha256, tamanho, caminho)], versão). Recusa manifesto sem assinatura, fora de ordem ou incompleto."""
    arq = origem / MANIFESTO
    if not arq.is_file():
        raise Recusa(f"a origem não tem {MANIFESTO}: não é uma imagem do mOS")
    bruto = arq.read_bytes()
    if len(bruto) < 64 or len(bruto) > 8 * MIB:
        raise Recusa("manifesto vazio ou grande demais")
    m = re.search(rb"\nASSINATURA-SHA256 ([0-9a-f]{64})\n", bruto)
    if not m:
        raise Recusa("manifesto sem a linha ASSINATURA-SHA256")
    if hashlib.sha256(bruto[: m.start() + 1]).hexdigest() != m.group(1).decode():
        raise Recusa("manifesto corrompido: o SHA-256 não confere")
    linhas = bruto[: m.start() + 1].decode("utf-8").splitlines()
    cab = linhas[0].split(" ")
    if cab[0] != "mOS-MANIFESTO-1" or len(cab) < 3:
        raise Recusa("versão de manifesto desconhecida")
    n, total = int(cab[1]), int(cab[2])
    versao = " ".join(cab[3:])
    itens = []
    anterior = ""
    for ln in linhas[1:]:
        if len(ln) < 68 or ln[64] != " " or not re.fullmatch(r"[0-9a-f]{64}", ln[:64]):
            raise Recusa(f"linha de manifesto inválida: {ln[:80]}")
        tam, _, caminho = ln[65:].partition(" ")
        if not caminho_permitido(caminho):
            raise Recusa(f"caminho proibido no manifesto: {caminho}")
        if itens and anterior >= caminho:
            raise Recusa("manifesto fora de ordem ou com duplicata")
        anterior = caminho
        itens.append((ln[:64], int(tam), caminho))
    if len(itens) != n or sum(t for _, t, _ in itens) != total:
        raise Recusa("manifesto incompleto (contagem ou total de bytes)")
    if not any(c == CARREGADOR_MANIFESTO for _, _, c in itens):
        raise Recusa("o payload não tem o carregador EFI")
    return bruto, itens, versao


def sha(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for bloco in iter(lambda: f.read(MIB), b""):
            h.update(bloco)
    return h.hexdigest()


def roda(cmd: list[str], checar: bool = True) -> str:
    p = subprocess.run(cmd, text=True, capture_output=True)
    if checar and p.returncode != 0:
        raise Recusa(f"{' '.join(cmd)} saiu {p.returncode}: {p.stderr.strip()[:300]}")
    return p.stdout


# ------------------------------------------------------------------ a ESP
class Esp:
    def __init__(self, ponto: Path):
        self.ponto = ponto
        info = json.loads(roda(["findmnt", "-J", "-n", "-o", "SOURCE,FSTYPE,TARGET", "--target", str(ponto)]))
        fs = info["filesystems"][0]
        if Path(fs["target"]).resolve() != ponto.resolve():
            raise Recusa(f"{ponto} não é um ponto de montagem (está dentro de {fs['target']})")
        if fs["fstype"] != "vfat":
            raise Recusa(f"{ponto} é {fs['fstype']}, não FAT: não é a partição EFI")
        self.dev = fs["source"]
        bl = json.loads(roda(["lsblk", "-J", "-n", "-o", "NAME,PKNAME,PARTN,PARTTYPE,UUID,TYPE", self.dev]))["blockdevices"][0]
        if bl.get("type") != "part" or (bl.get("parttype") or "").lower() != ESP_GUID:
            raise Recusa(f"{self.dev} não é uma partição do tipo EFI System (GPT {ESP_GUID})")
        self.nome = bl["name"]
        self.disco = "/dev/" + bl["pkname"]
        self.partn = int(bl["partn"])
        self.uuid = bl["uuid"]

    def livre(self) -> int:
        st = os.statvfs(self.ponto)
        return st.f_bavail * st.f_frsize

    def tem_mos(self) -> bool:
        return (self.ponto / "mOS").exists() or (self.ponto / "EFI" / "mOS").exists()

    def mos_anterior(self) -> bool:
        """Só conta como um mOS (instalado por este script ou pelo mOS) com o arquivo de estado do sistema."""
        return (self.ponto / ESTADO).is_file()

    def alvo(self, rel: str) -> Path:
        """Caminho na ESP; recusa qualquer coisa fora de \\mOS e \\EFI\\mOS."""
        p = (self.ponto / rel).resolve()
        raiz = self.ponto.resolve()
        if not (str(p).startswith(str(raiz / "mOS") + os.sep) or str(p).startswith(str(raiz / "EFI" / "mOS") + os.sep)):
            raise Recusa(f"escrita fora de \\mOS e \\EFI\\mOS recusada: {rel}")
        return p


def grava_arquivo(dst: Path, dados_de: Path | bytes, esperado: str | None = None) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    tmp = dst.with_name(dst.name + ".mostmp")
    if isinstance(dados_de, bytes):
        tmp.write_bytes(dados_de)
    else:
        shutil.copyfile(dados_de, tmp)
    with tmp.open("rb+") as f:
        os.fsync(f.fileno())
    if esperado and sha(tmp) != esperado:
        tmp.unlink()
        raise Recusa(f"o que foi gravado em {dst} não confere com o manifesto")
    os.replace(tmp, dst)



# ------------------------------------------------------------------ NVRAM e GRUB
def entradas_mos() -> list[str]:
    saida = roda(["efibootmgr", "-v"])
    nums = []
    for ln in saida.splitlines():
        # "Boot0003* mOS\tHD(1,GPT,...)/\EFI\mOS\BOOTX64.EFI" (o separador é tab ou espaços, conforme a versão)
        m = re.match(r"Boot([0-9A-Fa-f]{4})\*?\s+(.*?)(?:\t|\s{2,}|\s+HD\()", ln)
        if m and m.group(2).strip() == ROTULO and "\\EFI\\MOS\\BOOTX64.EFI" in ln.upper():
            nums.append(m.group(1).upper())
    return nums


def ordem_boot() -> list[str]:
    for ln in roda(["efibootmgr"]).splitlines():
        if ln.startswith("BootOrder:"):
            return [x.strip().upper() for x in ln.split(":", 1)[1].split(",") if x.strip()]
    return []


def cria_entrada(esp: Esp) -> str:
    existentes = entradas_mos()
    if existentes:
        return existentes[0]
    antes = ordem_boot()
    roda(["efibootmgr", "--create", "--disk", esp.disco, "--part", str(esp.partn), "--label", ROTULO,
          "--loader", CARREGADOR_EFI])
    nova = [n for n in entradas_mos() if n not in existentes]
    if not nova:
        raise Recusa("efibootmgr não mostrou a entrada nova do mOS")
    num = nova[0]
    # o efibootmgr põe a entrada nova na frente; o Ubuntu volta a ser o primeiro e o mOS vai para o fim
    roda(["efibootmgr", "--bootorder", ",".join([n for n in antes if n != num] + [num])])
    return num


def grub_texto(esp: Esp) -> str:
    return f"""#!/bin/sh
# Gerado por tools/instalar-dual-boot.py (mOS). Remova com: instalar-dual-boot.py --desinstalar --gravar
exec tail -n +4 $0
menuentry 'mOS' --class mos {{
\tinsmod part_gpt
\tinsmod fat
\tsearch --no-floppy --fs-uuid --set=root {esp.uuid}
\tchainloader /EFI/mOS/BOOTX64.EFI
}}
"""


def grava_grub(raiz: Path, esp: Esp) -> None:
    arq = raiz / GRUB_ARQ
    arq.parent.mkdir(parents=True, exist_ok=True)
    arq.write_text(grub_texto(esp), encoding="utf-8")
    arq.chmod(0o755)
    roda(["update-grub"])


# ------------------------------------------------------------------ origem
def particao_esp_da_imagem(imagem: str) -> int:
    """Deslocamento em bytes da 1ª partição EFI da GPT da imagem (o mkimg grava GPT + ESP FAT32)."""
    with open(imagem, "rb") as f:
        f.seek(512)
        cab = f.read(92)
        if cab[:8] != b"EFI PART":
            raise Recusa(f"{imagem} não tem GPT: não é uma imagem do mOS")
        lba_ent = int.from_bytes(cab[72:80], "little")
        n_ent = int.from_bytes(cab[80:84], "little")
        tam = int.from_bytes(cab[84:88], "little")
        f.seek(lba_ent * 512)
        tabela = f.read(n_ent * tam)
    esp = bytes.fromhex("28732ac11ff8d211ba4b00a0c93ec93b")      # C12A7328-F81F-11D2-BA4B-00A0C93EC93B em disco
    for i in range(n_ent):
        e = tabela[i * tam:(i + 1) * tam]
        if e[:16] == esp:
            return int.from_bytes(e[32:40], "little") * 512
    raise Recusa(f"{imagem}: GPT sem partição EFI")


class Origem:
    """--imagem: extrai a ESP de build/mOS.img com o mtools (mcopy), só leitura e sem root; nada é montado.
    --origem: uma árvore já extraída ou montada."""

    def __init__(self, imagem: str | None, pasta: str | None):
        self.tmp = None
        if pasta:
            self.raiz = Path(pasta)
            return
        if not imagem:
            raise Recusa("diga a origem: --imagem build/mOS.img (ou --origem PASTA)")
        if not shutil.which("mcopy"):
            raise Recusa("falta o mtools (sudo apt install mtools) para ler a imagem sem montá-la")
        off = particao_esp_da_imagem(imagem)
        self.tmp = tempfile.mkdtemp(prefix="mos-origem-", dir=os.environ.get("MOS_TMP") or None)
        env = dict(os.environ, MTOOLS_SKIP_CHECK="1", LC_ALL="C.UTF-8")
        for pasta_img in ("::/mOS", "::/EFI"):
            p = subprocess.run(["mcopy", "-s", "-n", "-m", "-i", f"{imagem}@@{off}", pasta_img, self.tmp],
                               text=True, capture_output=True, env=env)
            if p.returncode != 0:
                self.fecha()
                raise Recusa(f"mcopy {pasta_img} saiu {p.returncode}: {p.stderr.strip()[:300]}")
        self.raiz = Path(self.tmp)

    def fecha(self) -> None:
        if self.tmp:
            shutil.rmtree(self.tmp, ignore_errors=True)
            self.tmp = None


# ------------------------------------------------------------------ instalar / desinstalar
def plano(esp: Esp, itens, atualizar: bool) -> int:
    total = sum(t for _, t, _ in itens)
    precisa = total + total // 4 + 32 * MIB + BOOTLOG_TAM
    if atualizar:
        precisa -= sum((esp.ponto / c.lstrip("/")).stat().st_size for _, _, c in itens
                       if c != CARREGADOR_MANIFESTO and (esp.ponto / c.lstrip("/")).is_file())
    return max(precisa, 0)


def confirma(esp: Esp, digitado: str | None) -> None:
    if digitado is None:
        print(f"\nPara gravar na partição {esp.dev} digite o nome dela ({esp.nome}): ", end="", flush=True)
        digitado = sys.stdin.readline().strip()
    if digitado != esp.nome:
        raise Recusa("a confirmação digitada não confere; nada foi gravado")


def backup(raiz: Path, esp: Esp) -> Path:
    d = raiz / "var/backups/mos-dual-boot" / time.strftime("%Y%m%d-%H%M%S")
    d.mkdir(parents=True, exist_ok=True)
    (d / "efibootmgr-v.txt").write_text(roda(["efibootmgr", "-v"]), encoding="utf-8")
    lista = []
    for dirpath, _dirs, files in os.walk(esp.ponto):
        for f in files:
            p = Path(dirpath) / f
            lista.append(f"{p.stat().st_size}\t{p.relative_to(esp.ponto)}")
    (d / "esp-arquivos.txt").write_text("\n".join(sorted(lista)) + "\n", encoding="utf-8")
    return d


def instalar(a, esp: Esp, raiz: Path) -> None:
    orig = Origem(a.imagem, a.origem)
    try:
        bruto, itens, versao = le_manifesto(orig.raiz)
        if esp.tem_mos() and not a.atualizar:
            raise Recusa("já existe \\mOS ou \\EFI\\mOS nesta ESP: use --atualizar (mantém as contas) ou --desinstalar antes")
        if a.atualizar and not esp.mos_anterior():
            raise Recusa("--atualizar sem um mOS instalado nesta ESP")
        precisa, livre = plano(esp, itens, a.atualizar), esp.livre()
        total = sum(t for _, t, _ in itens)
        print(f"mOS {versao or '(sem versão)'}: {len(itens)} arquivos, {total / MIB:.1f} MiB")
        print(f"ESP {esp.dev} ({esp.ponto}): {livre / MIB:.0f} MiB livres, a instalação precisa de {precisa / MIB:.0f} MiB")
        print(f"grava só em {esp.ponto}/mOS, {esp.ponto}/{CARREGADOR} e {esp.ponto}/{BOOTLOG} "
              f"({BOOTLOG_TAM // MIB} MiB); entrada '{ROTULO}' no fim da BootOrder e no GRUB")
        if precisa > livre:
            raise Recusa("a ESP não tem espaço livre suficiente")
        # a origem inteira é conferida antes da primeira escrita
        for h, t, c in itens:
            p = orig.raiz / c.lstrip("/")
            if not p.is_file() or p.stat().st_size != t or sha(p) != h:
                raise Recusa(f"a origem não confere com o manifesto (SHA-256): {c}; nada foi gravado")
        print("origem conferida: todos os arquivos batem com o manifesto")
        if not a.gravar:
            print("SIMULAÇÃO: nada foi gravado. Repita com --gravar para instalar.")
            return
        confirma(esp, a.confirmo)
        bk = backup(raiz, esp)
        print(f"cópia de segurança da NVRAM e da lista da ESP em {bk}")
        if not a.atualizar:
            grava_arquivo(esp.alvo(ESTADO), b"INSTALANDO\n")
        for i, (h, t, c) in enumerate(itens, 1):
            if c == CARREGADOR_MANIFESTO:
                continue
            dst = esp.alvo(c.lstrip("/"))
            if a.atualizar and dst.is_file() and dst.stat().st_size == t and sha(dst) == h:
                continue
            grava_arquivo(dst, orig.raiz / c.lstrip("/"), h)
            if i % 200 == 0:
                print(f"  {i}/{len(itens)}", flush=True)
        os.sync()
        for h, t, c in itens:
            if c != CARREGADOR_MANIFESTO and sha(esp.alvo(c.lstrip("/"))) != h:
                raise Recusa(f"verificação falhou depois da cópia: {c}")
        if not esp.alvo(PREFS).is_file():
            grava_arquivo(esp.alvo(PREFS), f"idioma={a.idioma}\nteclado={a.teclado}\n".encode())
        grava_arquivo(esp.alvo(INSTALADO), bruto)
        h_carr = next(h for h, _, c in itens if c == CARREGADOR_MANIFESTO)
        if not esp.alvo(BOOTLOG).is_file():
            grava_arquivo(esp.alvo(BOOTLOG), bytes(BOOTLOG_TAM))
        grava_arquivo(esp.alvo(CARREGADOR_TMP), orig.raiz / CARREGADOR_MANIFESTO.lstrip("/"), h_carr)
        if not a.atualizar or (esp.ponto / ESTADO).read_text().strip() in ("INSTALANDO", ""):
            grava_arquivo(esp.alvo(ESTADO), b"PRIMEIRO_BOOT\n")
        os.sync()
        os.replace(esp.alvo(CARREGADOR_TMP), esp.alvo(CARREGADOR))     # o único passo que torna o mOS inicializável
        os.sync()
        if sha(esp.alvo(CARREGADOR)) != h_carr:
            raise Recusa("verificação final do carregador falhou")
        print(f"sistema copiado e conferido ({len(itens)} arquivos)")
        if not a.sem_nvram:
            num = cria_entrada(esp)
            print(f"NVRAM: Boot{num} 'mOS' no fim da BootOrder ({','.join(ordem_boot())})")
            if a.proximo_boot:
                roda(["efibootmgr", "--bootnext", num])
                print(f"o próximo boot (só ele) vai para o mOS (BootNext={num})")
        if not a.sem_grub:
            grava_grub(raiz, esp)
            print(f"GRUB: entrada 'mOS' em /{GRUB_ARQ} (segure Esc ao ligar para ver o menu do GRUB)")
        print("pronto: o Ubuntu continua como padrão; escolha 'mOS' no menu de boot do firmware ou do GRUB")
    finally:
        orig.fecha()


def concluir(a, esp: Esp, raiz: Path) -> None:
    """Fim do primeiro boot, rodado no Ubuntu depois de ler o log (tools/ler-boot-log.py): PRIMEIRO_BOOT -> INSTALADO.
    Enquanto o estado for PRIMEIRO_BOOT, o kernel não escreve em partição nenhuma além do boot.log."""
    if not esp.mos_anterior():
        raise Recusa("não há mOS instalado nesta ESP")
    est = (esp.ponto / ESTADO).read_text().strip()
    if est != "PRIMEIRO_BOOT":
        raise Recusa(f"o estado do mOS é {est or '(vazio)'}, não PRIMEIRO_BOOT: nada a concluir")
    print("estado PRIMEIRO_BOOT -> INSTALADO: nos próximos boots o mOS grava as contas e os documentos em \\mOS da ESP")
    if not a.gravar:
        print("SIMULAÇÃO: nada foi gravado. Repita com --gravar.")
        return
    confirma(esp, a.confirmo)
    grava_arquivo(esp.alvo(ESTADO), b"INSTALADO\n")
    os.sync()
    print("pronto")


def desinstalar(a, esp: Esp, raiz: Path) -> None:
    if (esp.ponto / "mOS").exists() and not esp.mos_anterior():
        raise Recusa("\\mOS nesta ESP não tem o estado do sistema do mOS; não apago o que não sei de quem é")
    nums = entradas_mos()
    print(f"remove {esp.ponto}/mOS (contas e documentos do mOS incluídos), {esp.ponto}/EFI/mOS, "
          f"entradas NVRAM {nums or 'nenhuma'} e /{GRUB_ARQ}")
    if not a.gravar:
        print("SIMULAÇÃO: nada foi apagado. Repita com --gravar.")
        return
    confirma(esp, a.confirmo)
    backup(raiz, esp)
    for n in nums:
        roda(["efibootmgr", "--bootnum", n, "--delete-bootnum"])
    for d in (esp.ponto / "EFI" / "mOS", esp.ponto / "mOS"):
        if d.exists():
            shutil.rmtree(d)
    os.sync()
    g = raiz / GRUB_ARQ
    if g.exists():
        g.unlink()
        roda(["update-grub"])
    print("mOS removido; o Ubuntu e o resto da ESP ficaram como estavam")


def main() -> int:
    p = argparse.ArgumentParser(description="Instala o mOS na ESP do Ubuntu, em dual boot, sem apagar nada do Ubuntu.")
    p.add_argument("--imagem", help="build/mOS.img gerado pelo build final")
    p.add_argument("--origem", help="árvore já montada da imagem (no lugar de --imagem)")
    p.add_argument("--esp", default="/boot/efi")
    p.add_argument("--gravar", action="store_true", help="sem isto, só simula")
    p.add_argument("--confirmo", help="o nome da partição (ex.: sda1), no lugar de digitar")
    p.add_argument("--atualizar", action="store_true", help="troca o sistema de um mOS já instalado; mantém as contas")
    p.add_argument("--desinstalar", action="store_true")
    p.add_argument("--concluir", action="store_true", help="depois do 1º boot e do log lido: PRIMEIRO_BOOT -> INSTALADO")
    p.add_argument("--proximo-boot", action="store_true", help="BootNext: só o próximo boot vai para o mOS")
    p.add_argument("--sem-nvram", action="store_true", help="não cria entrada na NVRAM (só GRUB)")
    p.add_argument("--sem-grub", action="store_true", help="não cria entrada no GRUB (só NVRAM)")
    p.add_argument("--idioma", default="pt")
    p.add_argument("--teclado", default="abnt2")
    p.add_argument("--raiz-sistema", default="/", help="prefixo de /etc e /var (testes)")
    a = p.parse_args()
    raiz = Path(a.raiz_sistema)
    if a.gravar and raiz == Path("/") and os.geteuid() != 0:
        print("recusado: gravar na ESP precisa de root (sudo)", file=sys.stderr)
        return 2
    try:
        esp = Esp(Path(a.esp))
        (desinstalar if a.desinstalar else concluir if a.concluir else instalar)(a, esp, raiz)
    except Recusa as e:
        print(f"recusado: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
