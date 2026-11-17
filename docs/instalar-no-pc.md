# Instalar o mOS no PC do Thiago (dual boot com o Ubuntu)

O primeiro boot do mOS no PC real. O Ubuntu continua sendo o sistema padrão. Nenhuma partição é criada, apagada ou
redimensionada, e a tabela GPT não muda. Só se grava com o Thiago no PC e com o ok dele: reiniciar derruba os bots.

## O que vai para a ESP

A ESP é a `/dev/sda1` (FAT32, 1 GiB, montada em `/boot/efi`), a mesma que o Ubuntu usa. Com a imagem do sistema sem os
apps de terceiros (docs/disco-de-apps.md), entram só:

| na ESP | o que é | tamanho |
|---|---|---:|
| `\mOS\` | o sistema: 438 arquivos do manifesto, cada um conferido por SHA-256 | 186,1 MiB |
| `\EFI\mOS\BOOTX64.EFI` | o carregador do mOS (fora do `\EFI\ubuntu` e do `\EFI\BOOT`) | (incluído acima) |
| `\EFI\mOS\boot.log` | o log de boot, já alocado (2 boots de 2 MiB) | 4 MiB |

Total: uns **190 MiB**. A ESP tem 1067 MiB livres, e o instalador pede 269 MiB, com folga. `\EFI\ubuntu`, `\EFI\BOOT`
e o resto da ESP não são alterados. Números da simulação contra o `mOS.img` sem os apps de terceiros (240 MiB, versão
0.1-onda1-sem-apps).

Situação do PC lida sem gravar: Secure Boot desligado (o carregador do mOS não é assinado); BootOrder `0000,0002,0001`
(Ubuntu, Windows Boot Manager, OpenCore); Ubuntu pelo `\EFI\ubuntu\shimx64.efi`.

## Primeiro boot em modo seguro

Enquanto o estado do sistema for `PRIMEIRO_BOOT` (o instalador grava esse estado), o kernel não escreve em partição
nenhuma do SSD. A sessão roda só em RAM: contas e arquivos criados nesse boot somem ao desligar. A única escrita é o
log, dentro do `\EFI\mOS\boot.log` que o instalador já deixou alocado. O kernel só reescreve o conteúdo dos clusters
desse arquivo, sem alocar espaço, sem mudar o tamanho e sem tocar em diretório nem na FAT. O estado só sai de
`PRIMEIRO_BOOT` pelo passo 6, rodado no Ubuntu.

No kernel: com o estado `PRIMEIRO_BOOT`, o `persist.c` chama `storage_seal()` (`kernel/dev/storage.c`) antes de
qualquer outra coisa. Daí em diante, `storage_grant_write` recusa tudo: persistência, volumes de `/Volumes`, instalador e
TRIM. Os drivers (AHCI, NVMe, USB) só aceitam as faixas do log. O `kernel/fs/bootlog.c` acha os clusters do
`boot.log` pela FAT, só lendo (`fat_extents`). Ele registra essas faixas com `storage_allow_log` e grava direto no
driver, em setores inteiros, sem page cache, FAT ou diretório: ~1 vez por segundo e no pânico, depois da tela. Ao encher
os 2 MiB do slot, para com `...[cortado]`. O log também é gravado nos boots seguintes (`INSTALADO`), sempre nos mesmos
clusters. Só grave no PC depois que o build final tiver sido refeito com esse kernel.

## Passo a passo

1. **Build final** com o kernel do modo seguro: `/home/thiago/OpenBot/Shared/mos-time/build-final.sh <worktree> igor`.
   Gera `build/mOS.img` (uns 240 MiB).

2. **Simular** (não grava nada; pode rodar sem sudo):

       python3 tools/instalar-dual-boot.py --imagem build/mOS.img

   Tem de terminar com "origem conferida" e "SIMULAÇÃO: nada foi gravado".

3. **Instalar** (com o Thiago no PC):

       sudo python3 tools/instalar-dual-boot.py --imagem build/mOS.img --gravar --proximo-boot

   O instalador pede para digitar `sda1`. Ele guarda uma cópia da NVRAM e da lista da ESP em
   `/var/backups/mos-dual-boot/<data>/` e copia o sistema arquivo por arquivo (temporário, fsync, renomeia), conferindo
   cada um por SHA-256. Depois aloca o `boot.log` e grava o estado `PRIMEIRO_BOOT`. O último passo é renomear o
   carregador para `\EFI\mOS\BOOTX64.EFI`: se a energia cair antes disso, o mOS só não aparece e o Ubuntu continua
   bootando.

   Entradas de boot criadas:
   - NVRAM: uma entrada "mOS" no **fim** da BootOrder (fica `0000,0002,0001,<mOS>`), então o padrão continua o Ubuntu.
   - `--proximo-boot` grava BootNext: **só o próximo boot** vai para o mOS. O firmware apaga o BootNext ao usá-lo.
   - GRUB: entrada "mOS" em `/etc/grub.d/42_mos` (`update-grub`). Para ver o menu, segure Esc ao ligar.

4. **Reiniciar.** O PC entra no mOS uma vez. Se travar, desligue no botão: o próximo boot volta ao Ubuntu sozinho,
   porque o BootNext já foi consumido.

5. **Ler o log no Ubuntu:**

       sudo python3 tools/ler-boot-log.py            # o boot mais novo
       sudo python3 tools/ler-boot-log.py --todos    # e o anterior

   Para bootar o mOS de novo: `sudo efibootmgr --bootnext <número da entrada mOS>`, ou escolha "mOS" no GRUB. Cada boot
   usa um dos dois espaços de 2 MiB do log; os dois últimos boots ficam guardados.

6. **Liberar a persistência** (só quando o primeiro boot estiver bom; é a onda 2):

       sudo python3 tools/instalar-dual-boot.py --concluir --gravar

   O estado passa a `INSTALADO`, e a partir do próximo boot o mOS grava contas e documentos em `\mOS`.

## Voltar atrás

    sudo python3 tools/instalar-dual-boot.py --desinstalar --gravar

Apaga `\mOS` e `\EFI\mOS` da ESP, as entradas "mOS" da NVRAM e `/etc/grub.d/42_mos` (com `update-grub`). A ESP volta a
ter só o que tinha. Recusa apagar um `\mOS` que não tenha o estado do mOS. A cópia da NVRAM de antes da instalação
continua em `/var/backups/mos-dual-boot/`. Sem o Ubuntu, pelo firmware: escolha "Ubuntu" no menu de boot da placa.

## Testes

- `tools/instalar-dual-boot-teste.sh`: 29/29 numa ESP simulada, sem root (instala, `boot.log` de 4 MiB zerado, arquivos
  do Ubuntu intactos, BootOrder com o Ubuntu primeiro, `--concluir`, atualizar, desinstalar).
- Simulação contra a ESP real (`/dev/sda1`), só leitura: origem conferida, 269 MiB de 1067 MiB.
- `tools/bootlog_test.sh`: 3 boots num disco falso com o `boot.log` em 2 faixas, lidos pelo `ler-boot-log.py`. Confere
  o N, a alternância de slots, o corte em 2 MiB e que nenhum byte fora do `boot.log` muda.
- Boot na VM (KVM, sem `-snapshot`) com uma ESP de dual boot falsa (`\EFI\ubuntu` + `\EFI\mOS` + `\mOS` +
  `boot.log` + `PRIMEIRO_BOOT`), 2 boots seguidos: o `ler-boot-log.py --todos` mostra o boot 2 e o boot 1 inteiros, até
  o "Desktop pronto". O sha256 da imagem inteira, com os clusters do `boot.log` zerados, é igual antes e depois, e a
  árvore copiada com o `mcopy` é igual fora do `boot.log`.
