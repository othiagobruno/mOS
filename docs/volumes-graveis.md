# Volumes graváveis em `/Volumes` (FAT e HFS+)

Volumes FAT12/16/32 e HFS+/HFSX de discos graváveis (SATA/AHCI, NVMe, pendrive USB) aparecem em
`/Volumes/<nome>` e aceitam escrita: o msh, os apps do macOS (pelo libSystem) e o Finder criam, gravam,
renomeiam, movem e apagam arquivos e pastas lá como em qualquer pasta. Imagens `.dmg`/`.img` e APFS
continuam só leitura.

## Como funciona

```
processo / Finder / msh
   │  open/write/rename/unlink/mkdir (SYS_*)      ← kernel/proc/process.c (sem mudança)
   ▼
VFS em RAM (kernel/lib/vfs.c)
   │  persist_mark / persist_delete / persist_move ← os mesmos ganchos do disco do sistema
   ▼
kernel/fs/vfs_bridge.c  vol_mark / vol_delete / vol_move / vol_flush
   │  caminho relativo ao volume ("/a/b.txt")
   ▼
dfs_ops_t.write_file / mkdir / remove / rename / sync   (kernel/include/fs.h)
   ├── FAT:  kernel/fs/fat.c         (fat_write_file, fat_mkdir, fat_remove, fat_rename, fat_sync)
   └── HFS+: kernel/fs/hfsplus_wr.c  (transações, journal, B-trees, bitmap)
```

- `dmount_t.writable` diz se o volume aceita escrita; `vfs_is_read_only()` passa a perguntar ao volume
  (`vfs_ext_writable`), então o Finder habilita "Nova Pasta", apagar e arrastar normalmente.
- **Quando grava**: pastas criadas, apagadas, renomeadas ou movidas vão para o disco na hora. Arquivos
  alterados ficam "sujos" e são gravados inteiros ~1,5 s depois da última alteração (`vol_tick`, chamado
  por `persist_tick`), ao ejetar (`vfs_eject` grava o pendente antes; se falhar, não ejeta) e em
  `persist_sync` (desligar/reiniciar).
- Um arquivo alterado deixa de apontar para o nó do volume (`vnode->ext = NULL`): a cópia em RAM passa
  a ser a verdadeira e a LRU de conteúdo da ponte nunca a descarta.
- **Mover entre volumes** (ou entre um volume e a pasta pessoal): a subárvore é carregada inteira para
  a RAM ("materializada"), apagada da origem e marcada como nova no destino.
- A raiz de um volume não pode ser apagada nem movida pelo VFS (só ejetada).

## FAT12/16/32

O driver `kernel/fs/fat.c` já gravava (é o mesmo do disco do sistema); a novidade é montar em
`/Volumes` com escrita ligada quando o dispositivo aceita escrita. Nomes longos (LFN, UTF-16),
datas de criação/modificação, crescimento de diretórios, raiz fixa do FAT12/16, duas cópias da FAT e
FSInfo. Detalhes:
- **Partições EFI** (pasta `/EFI` na raiz e rótulo `EFI`/`SYSTEM`/`ESP`/vazio, ou `/EFI/Microsoft`,
  `/EFI/APPLE`) ficam só leitura, para não estragar o boot de outro sistema.
- Substituir um arquivo grave primeiro os clusters novos e só depois solta os antigos; com o disco
  cheio, solta antes (para caber).
- `fat_rename` recusa mover uma pasta para dentro dela mesma.

## HFS+/HFSX

`kernel/fs/hfsplus_wr.c` (escrito do zero; o HFS do ravynOS/xnu foi só referência de formato):

- **Transações**: cada operação (gravar arquivo inteiro, criar pasta, apagar recursivo, renomear) junta
  os nós de B-tree alterados (cache por operação), as páginas do bitmap e o cabeçalho do volume e grava
  tudo de uma vez no fim. Se a operação falhar no meio, nada de metadados vai para o disco.
  O conteúdo dos arquivos vai direto para blocos recém-alocados antes dos metadados, então um arquivo
  substituído nunca fica pela metade.
- **Journal** (volumes com `kHFSVolumeJournaledBit`, journal no próprio volume):
  - *replay ao montar*: transações pendentes (lista de blocos com checksum, formato do xnu, little ou
    big-endian) são repetidas antes de ler qualquer estrutura;
  - *escrita journaled*: os blocos de metadados de cada transação vão primeiro para o journal; o
    cabeçalho do journal avança, os blocos vão para o lugar e o journal é esvaziado (checkpoint
    imediato). Uma queda em qualquer ponto deixa o volume na versão anterior ou na nova, nunca no meio;
    o macOS também repete esse journal (o volume é marcado `lastMountedVersion = 'HFSJ'`).
  - Sem disco gravável, journal em outro dispositivo, cabeçalho de journal inválido ou falha no replay:
    o volume monta só leitura.
- **B-trees** (catálogo, extents overflow, atributos): busca binária nos nós, inserção com divisão de
  nós e nova raiz, remoção que libera nós vazios, religa irmãos e encolhe a raiz; chaves de índice
  sempre iguais à 1ª chave do filho (variáveis ou de tamanho fixo, conforme o cabeçalho da árvore).
  Árvores crescem dobrando de tamanho (no mínimo o clump size), em um extent contíguo; cabem 8
  extents no cabeçalho do volume, então a árvore pode crescer 256 vezes.
- **Alocação**: bitmap inteiro em RAM (até 64 MB, ou seja volumes de até ~2 TB com blocos de 4 KB);
  primeiro procura um trecho contíguo a partir de `nextAllocation`, senão junta trechos. Arquivos com
  mais de 8 extents ganham registros na árvore de extents overflow.
- **Catálogo**: registros de arquivo/pasta e threads, valência e `folderCount` (quando a raiz usa
  `kHFSHasFolderCountMask`), contadores e `nextCatalogID` do cabeçalho, datas (UTC desde 1904),
  permissões `_unknown` (99) como os volumes externos do macOS. Nomes vão para o disco decompostos (NFD
  dos acentos latinos, como o macOS guarda) e `:` do VFS volta a ser `/`.
  Comparação de nomes: binária no HFSX case-sensitive (`kHFSBinaryCompare`); sem diferenciar
  maiúsculas no HFS+, com a dobra de caixa do HFS+ escrita para latim, grego, cirílico e largura total.
- Sobrescrever um arquivo comprimido (decmpfs) o descomprime: tira a flag `UF_COMPRESSED`, o atributo
  `com.apple.decmpfs` e o resource fork. Apagar remove também os atributos estendidos do arquivo.
- **Estado "desmontado"**: o bit `kHFSVolumeUnmountedBit` fica ligado enquanto não há operação em
  andamento (cada sync em repouso liga; a 1ª operação seguinte desliga na mesma transação). Assim um
  desligamento sem ejetar não pede fsck. Um volume sem journal que não foi desmontado direito (ou
  marcado inconsistente/travado) monta só leitura.

Limitações conhecidas:
- Hard links: gravar num link grava no inode; apagar um link que faz parte de uma cadeia de links
  (`kHFSHasLinkChainMask`) é recusado. Hard links de pasta não são tratados.
- Volumes dentro de um wrapper HFS (discos muito antigos) ficam só leitura.
- A árvore de atributos não cresce (só se apagam atributos); criar xattrs não está implementado.
- Arquivos especiais (catálogo etc.) não usam extents overflow ao crescer.
- O VFS do mOS compara nomes com diferença de maiúsculas e o FAT/HFS+ sem: criar `A.txt` ao lado de
  `a.txt` pelo VFS substitui o mesmo arquivo no disco.

## Testes

Ferramentas no host (Windows, sem WSL/fsck): `tools/volimg.c` é independente dos drivers do kernel
(formata, verifica e lê); `tools/voltest.c` roda os drivers reais (`dfs` + `fat.c` + `hfsplus*.c`).

```
zig cc -O2 -target x86_64-windows-gnu tools/volimg.c -o build/volimg.exe -lshell32
zig cc -target x86_64-windows-gnu -O1 -DMOS_HOST_TEST -Ikernel/include -Itools tools/voltest.c kernel/fs/{apfs,blkdev,bzip2,dfs,dmg,hfsplus,hfsplus_wr,inflate,lzfse,xz,fat}.c -o build/voltest.exe

volimg mkdisk t.img 160 fat32:MOSFAT:40 fat16:CARTAO:20 fat12:DISQUETE:4 hfs:Mac:30 hfsx:Caixa:30 hfsj:Jornal:30
voltest t.img                 # 209 verificações: criar, substituir, truncar, acentos, 300 arquivos, mover,
                              # renomear, rm -r, remontar e reler, arquivo fragmentado com extents overflow
volimg check t.img            # fsck independente: B-trees (ordem, índices, irmãos, mapa de nós), catálogo
                              # (threads, valência, folderCount, contadores), extents x bitmap, journal, FAT
                              # (cadeias, cruzados, perdidos, LFN, ".", "..", cópias da FAT)
volimg jinject j.img 1 /seed.txt "JOURNAL REPETIDO."   # transação pendente no journal (queda simulada)
voltest j.img --mount         # o mount repete o journal: seed.txt mostra o texto novo
voltest c.img --crash build/volimg.exe   # corta a energia depois de k escritas (k = 1..29) no meio de
                              # gravar+renomear; a cada k remonta, confere atomicidade e roda o check
volimg ls t.img 4 | volimg cat t.img 4 "/caminho"
```

No QEMU (`run.ps1`/`qemu-test.ps1` sem `-Persist` usam `-snapshot`: as escritas não chegam ao arquivo):

```
volimg mkdisk build-vol\dados.img 200 fat32:MOSFAT:50 fat16:CARTAO:30 hfs:MacHD:50 hfsj:Jornal:50
tools\qemu-test.ps1 -OutDir build-vol -Persist -NvmeDisk <caminho>\dados.img -Steps "wait:45000;shot:x"
volimg check build-vol\dados.img
```

> **Mudança posterior (assistente de instalação).** O kernel não tem mais o autoteste `vol_selftest` (nem o `install_selftest`
> nem o autoteste de escrita do AHCI por "assinatura"): eles gravavam em qualquer volume/disco que os acionasse. Além disso,
> **nenhum disco físico é gravável por padrão**: `kernel/dev/storage.c` só deixa os drivers (AHCI, NVMe, USB MSC) emitirem
> comandos de escrita dentro de faixas liberadas explicitamente (`storage_grant_write`): a partição do sistema instalado, o
> pendrive Live depois do consentimento do usuário, ou o alvo de uma instalação depois da confirmação digitada. Por isso, no
> estado atual, volumes de OUTROS discos aparecem em `/Volumes` **somente leitura** até o usuário consentir **por volume** (cadeado
> no app Arquivos; `vol_enable_write` em `vfs_bridge.c`): só a faixa daquele volume é liberada, até ejetá-lo; partições EFI, o disco do
> sistema, HFS+ com journal pendente e qualquer caso com origem não identificada são recusados. O texto abaixo descreve o comportamento e os testes da época em que os volumes eram graváveis.

O autoteste (`vol_selftest`, removido) gravava pelo VFS — o mesmo caminho do msh e dos apps —:
pasta, nome com acentos, 60 arquivos, 1 MB truncado para 300 KB, append, renomear, mover pasta,
apagar pasta com conteúdo, mover da pasta pessoal para o volume e de volta, e um arquivo pendente na
hora de ejetar. Resultado em 7/nov/2026: 4 volumes OK, ejeção OK, `volimg check` com 0 erros, conteúdo
conferido byte a byte. Também pelo Terminal (msh, via QMP): `echo > arquivo`, `mkdir`, `mv`, `rm` e
`cat` em `/Volumes/...`, desligando o QEMU sem ejetar: 0 erros e 0 avisos no `volimg check`.

Não verificado: leitura dos volumes gravados pelo macOS/`fsck_hfs` de verdade e pelo Windows (não há
como montar a imagem nesta máquina); o `volimg check` segue a TN1150, mas foi escrito junto com o
driver, então um engano de leitura da especificação pode estar nos dois.
