# mOS em dual boot com o Ubuntu (sem apagar o Ubuntu)

Feito para o PC do Thiago: SSD Kingspec KY281 256 GB, `sda1` = ESP FAT32 de 1 GiB (`/boot/efi`), `sda2` = ext4 do Ubuntu,
sem espaço livre na tabela, Secure Boot desligado.

## O que muda no disco

Só dentro da ESP, em duas pastas novas:

| Caminho | Conteúdo |
|---|---|
| `\mOS\` | o sistema (arquivos do manifesto do payload) e, depois, as contas e documentos do mOS |
| `\EFI\mOS\BOOTX64.EFI` | o carregador do mOS |

Não mudam: a GPT, o `sda2`, `\EFI\BOOT\` (fallback do Ubuntu), `\EFI\ubuntu\`. Na NVRAM entra uma entrada `mOS` **no fim**
da BootOrder (o Ubuntu continua sendo o padrão). No Ubuntu entra `/etc/grub.d/42_mos` e roda o `update-grub`.
Antes de gravar, o script salva `efibootmgr -v` e a lista de arquivos da ESP em `/var/backups/mos-dual-boot/<data>/`.

## Como usar (depois do build final)

```bash
tools/instalar-dual-boot.py --imagem build/mOS.img                            # simulação: sem root, não grava
sudo tools/instalar-dual-boot.py --imagem build/mOS.img --gravar --proximo-boot   # instala; pede para digitar "sda1"
```

Para entrar no mOS: o próximo boot (com `--proximo-boot`), o menu de boot do firmware (entrada `mOS`) ou o menu do GRUB
(segure Esc ao ligar). Atualizar mantendo as contas: `--gravar --atualizar`. Tirar tudo: `--desinstalar --gravar`.

## Como o mOS se comporta na ESP compartilhada

- Origem: o kernel e `\mOS\` vêm da mesma partição (`sda1`), então `fs/origin.c` marca a origem como confiável e o
  `persist` grava só sob `\mOS\`.
- Atualização: o kernel guarda os nós de arquivo do `LoadedImage->FilePath` (`boot_info_t.loader_fp`);
  `origin_loader_path()` devolve `/EFI/mOS/BOOTX64.EFI` e o `fs/update.c` troca esse arquivo, nunca o
  `\EFI\BOOT\BOOTX64.EFI` do Ubuntu. Caminho ausente ou estranho volta ao padrão `/EFI/BOOT/BOOTX64.EFI` (pendrive).
- Espaço: o sistema e os dados do mOS dividem o ~1 GiB livre da ESP. O script recusa se o payload + 25% + 32 MiB não couber.

A imagem é lida com o `mtools` (`mcopy` da ESP da GPT do `mOS.img`), sem montar e sem root; só `--gravar` precisa de sudo.

## Segurança da gravação

Simula por padrão; `--gravar` exige digitar o nome da partição. A origem inteira é conferida por SHA-256 contra o
manifesto antes da primeira escrita. Cada arquivo é gravado como temporário, `fsync`, renomeado e relido. O carregador é o
último arquivo (`MOSNEW.TMP` conferido, estado `PRIMEIRO_BOOT`, renomear): uma queda no meio não deixa um mOS que dê boot,
e o Ubuntu não é tocado em nenhum ponto. Escrita fora de `\mOS` e `\EFI\mOS` é recusada pelo próprio script.
`--desinstalar` só apaga `\mOS` que tenha o estado do sistema do mOS.

## Testes

- `tools/instalar-dual-boot-teste.sh`: 23/23 (ESP em pasta temporária com os arquivos do Ubuntu, stubs de `findmnt`,
  `lsblk`, `efibootmgr` com NVRAM em arquivo e `update-grub`): simulação não grava, confirmação errada e origem
  corrompida recusam sem gravar, instalação, arquivos do Ubuntu intactos (SHA-256), BootOrder `0000,0002,0001,0003`,
  BootNext, GRUB com a UUID da ESP, reinstalar sem `--atualizar` recusa, atualizar troca o kernel e mantém contas,
  desinstalar devolve a ESP ao estado inicial, `\mOS` alheio não é apagado; e uma imagem de verdade do `mkimg`
  (GPT + FAT32 + manifesto) lida pelo mtools, instalada com nome acentuado, sem sobra no temporário; arquivo que não é
  imagem recusado.
- `tools/mkimg-nomes-teste.sh`: 13/13 (nomes longos com acento, CJK, emoji e 80 caracteres saem iguais pelo mtools e pelas
  entradas LFN). A versão anterior do `mkimg` dava 4/13: gravava cada byte do UTF-8 como uma unidade UTF-16, e o mOS
  mostrava "CientÃ­fica" no lugar de "Científica".
- `tools/loaderpath_host.c`: 13/13 casos do caminho do carregador.

Não testado ainda: boot real do mOS a partir de `\EFI\mOS` (precisa do build final) e a gravação na ESP do Thiago
(só com a confirmação dele).
