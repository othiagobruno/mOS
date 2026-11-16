# Frente 20 — integração, compatibilidade e release (Igor)

Dono: Igor (`igor`, `TIME.md`). Este arquivo é o plano e o estado honesto da frente. Atualizado a cada PR.

## Escopo

1. Corpus de apps (`tools/corpus`) ampliado de 61 para os **300** apps de macOS fora da App Store mais usados.
2. Relatório de lacunas por app e por dono, e repasse de cada lista ao dono da frente.
3. Instalador do mOS no SSD do Thiago em **dual boot** com o Ubuntu, sem apagar o Ubuntu.
4. Imagem final e acompanhamento da fila do build final (`build-final.sh`).

## Levantamento (15 nov. 2026)

| Item | Estado encontrado | O que faltava |
|---|---|---|
| Corpus | 61 apps escolhidos à mão; harness exigia `tools/build.sh` para ter o libSystem; `.pkg` recusado; só x86_64; 7z do sistema obrigatório (o Ubuntu daqui não tem) | ranking real de uso, `.pkg`, apps só-arm64, casks `no_check`, medir sem rodar o build, apagar o binário depois de medir (o disco tem ~40 GB livres) |
| Lacunas | `docs/compat-gaps.md` por símbolo e framework | nenhum mapa símbolo → dono; nenhum repasse |
| Disco do Thiago | `sda` 256 GB: `sda1` ESP FAT32 1 GiB (6 MiB usados), `sda2` ext4 237 GiB com o Ubuntu montado em `/`; **sem espaço livre** na GPT; Secure Boot desligado; NVRAM `0000 Ubuntu`, `0002`, `0001` | o instalador do mOS só aceitava disco inteiro, partição dedicada ou espaço livre: nenhum serve sem apagar ou encolher o Ubuntu |
| Atualização | `kernel/fs/update.c` trocava sempre `\EFI\BOOT\BOOTX64.EFI` | numa ESP compartilhada esse arquivo é o shim do Ubuntu: a atualização do mOS o sobrescreveria |

## Decisões

- **Ranking**: instalações de 365 dias do Homebrew Cask (`formulae.brew.sh`). Todo cask é distribuído fora da Mac App Store.
  Entram casks com artefato `app` ou `pkg`; deprecados e variantes `nome@canal` ficam de fora. URL Intel quando existe.
  Licença: não declarada no cask; o binário só é baixado para medir e não é redistribuído.
- **Dual boot sem encolher o ext4**: o mOS vai para a ESP que já existe (`\mOS\` e `\EFI\mOS\BOOTX64.EFI`). Encolher o `sda2`
  exigiria desmontar a raiz do Ubuntu (live USB) e é o único passo com risco real ao Ubuntu; fica fora.
  Limite: o sistema e os dados do mOS dividem ~1 GiB. Quando faltar espaço, a opção é uma partição de dados criada pelo
  Thiago com o Ubuntu desligado; o instalador do mOS já sabe usar espaço livre (`INST_NEWPART`).
- **Entrada de boot pelo Ubuntu**: `efibootmgr` (entrada no fim da BootOrder; o Ubuntu continua padrão) e GRUB
  (`/etc/grub.d/42_mos`). O kernel do mOS não chama os runtime services da UEFI, então não cria a entrada sozinho.

## Entregas e testes

| Entrega | Onde | Teste | Resultado |
|---|---|---|---|
| Manifesto com 300 apps | `tools/corpus/gerar-manifesto.py`, `manifest.json` | 300 ids únicos, 13 sem hash (fixados na medição) | ok |
| Medir sem build, `.pkg`, arm64, hash fixado, binário apagado | `tools/corpus/harness.py scan --lib ... --limpar --fixar` | `harness.py hosttest` | ok |
| Varredura um app por vez na trava de RAM | `tools/corpus/varrer-tudo.sh` | em andamento | ver `docs/lacunas-por-dono.md` |
| Lacunas por dono | `harness.py donos` → `docs/lacunas-por-dono.md`, `docs/lacunas/<dono>.md` | `hosttest donos` (mapa de frentes) | ok; números saem da varredura |
| Caminho do carregador EFI | `kernel/fs/loaderpath.c`, `origin_loader_path()`, `boot_info_t.loader_fp` | `tools/loaderpath_host.c` 13/13 | ok no host; boot não rodou (build proibido até o fim) |
| Atualização troca o carregador certo | `kernel/fs/update.c` | `clang -fsyntax-only` | sintaxe ok; sem VM |
| Instalador dual boot (Ubuntu) | `tools/instalar-dual-boot.py`, doc `docs/dual-boot.md` | `tools/instalar-dual-boot-teste.sh` 19/19 (ESP simulada, stubs de efibootmgr/GRUB) | ok no host; **nada gravado no disco do Thiago** |

## Falta

- Terminar a varredura dos 300 (a trava pesada é compartilhada; um app por vez) e refazer as que falharam.
- Mandar a cada dono a lista dele (`docs/lacunas/<dono>.md`).
- Build final pela fila, uma vez, com a frente pronta; depois a simulação do instalador contra o `build/mOS.img` real e,
  só com a confirmação do Thiago, a gravação na ESP.
