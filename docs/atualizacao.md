# Atualização sem reinstalar

Um mOS instalado (ou um pendrive Live com persistência) se atualiza no lugar: baixa só os arquivos do sistema que mudaram,
confere cada um e troca tudo no próximo reinício. Contas, senhas, preferências, pastas pessoais e logs não são tocados.

Interface: **Ajustes do Sistema > Geral > Atualização de Software** (Procurar → Atualizar Agora → Reiniciar Agora), ou no
Terminal (msh): `atualizar procurar <origem>`, `atualizar baixar`, `atualizar` (estado), `atualizar cancelar`.

## Do lado do host

```
.\build.ps1                          # build\mOS.img + o pacote: build\esp (arquivos) e build\manifesto.txt (assinado)
.\tools\servir-atualizacao.ps1       # HTTP em 127.0.0.1:8080; dentro do QEMU o host é http://10.0.2.2:8080
.\tools\servir-atualizacao.ps1 -Rede # todas as interfaces (computador real na rede local; o firewall pode perguntar)
```

O pacote é o mesmo layout de um pendrive do mOS, então também serve uma imagem/pendrive novo montado em `/Volumes`
(origem `/Volumes/MOS`).

### Chave de assinatura

O `build.ps1` passa ao `mkimg` a semente Ed25519 versionada no repositório, `tools\chave-atualizacao.semente` (ou o
caminho de `MOS_CHAVE_ATUALIZACAO`; se o arquivo não existir, o mkimg cria um novo). Por estar no repositório, qualquer
pessoa com acesso a ele consegue assinar pacotes aceitos pelos sistemas instalados. O mkimg:

- põe a chave pública no payload, em `/mOS/Sistema/Biblioteca/chave-atualizacao.pub`;
- assina o manifesto: a linha `ASSINATURA-ED25519` vem depois da `ASSINATURA-SHA256` e cobre tudo até ela (o instalador
  continua lendo só a do SHA-256);
- grava a versão no 4º campo do cabeçalho (`mOS-MANIFESTO-1 <n> <bytes> 0.1-AAAAMMDD.HHMM-<commit>`).

Um sistema só aceita pacotes assinados pela chave que ele tem. Trocar a semente = os sistemas instalados com a antiga não
aceitam mais atualizações (reinstalar com uma imagem nova resolve). Um pacote assinado pode trazer outra chave pública (rotação).
Sistemas instalados antes do atualizador não têm a chave nem o código: precisam ser reinstalados uma vez.

## Do lado do mOS (`kernel/fs/update.c`)

1. **Procurar**: baixa `/mOS/Sistema/Instalacao/manifesto.txt` da origem, confere o SHA-256 e a assinatura Ed25519 com a chave
   instalada, aplica a mesma política de caminhos do instalador (`install_path_allowed`: nada de contas, logs, preferências,
   pastas pessoais) e exige que o pacote traga o carregador EFI e a chave.
2. **Comparar**: para cada arquivo, se o manifesto instalado (`/mOS/Sistema/Atualizacao/instalado.txt`, gravado pelo
   instalador e por cada atualização; numa mídia Live, o próprio manifesto dela) tem o mesmo hash e o arquivo no VFS tem o
   tamanho certo, está igual; senão o conteúdo atual é conferido por SHA-256 (em fatias de 25 ms, a interface não trava).
   Arquivos do manifesto instalado que saíram do pacote entram na lista de remoção.
3. **Baixar**: cada arquivo novo/alterado é conferido (tamanho e SHA-256), gravado em `/mOS/Sistema/Atualizacao/novos/<i>` e
   relido do disco para conferir de novo. No fim vão para o disco o plano (`plano.txt`: `S <i> <caminho>` / `D <caminho>` /
   `FIM`), o manifesto novo e o estado `PRONTA`. Nada do sistema em uso mudou até aqui; `atualizar cancelar` apaga tudo.
4. **Aplicar**, ao desligar ou reiniciar (desktop.c, depois do `persist_sync`): estado `APLICANDO`, troca de cada arquivo com
   `fat_replace`, o carregador EFI por último, remoções, `instalado.txt` ← manifesto novo, `ultima.txt` com a versão, e a área
   de trabalho é apagada. Numa mídia Live persistente o manifesto do payload dela também é atualizado (o "Instalar mOS" dela
   continua conferindo).

`/mOS/Sistema/Atualizacao` fica fora do VFS (persist.c não carrega nem grava ali), e o payload não pode conter essa pasta
(mkimg e instalador recusam).

### Queda de energia

- `fat_replace` (kernel/fs/fat.c) troca um arquivo sem copiar dados: o único passo que muda o que o destino é consiste em
  regravar a entrada de diretório dele com a cadeia e o tamanho da cópia nova; depois a entrada da cópia sai e a cadeia antiga
  é liberada. Uma queda entre os dois passos deixa as duas entradas na mesma cadeia, e chamar de novo reconhece isso.
- O plano é gravado antes da primeira troca e cada linha pode ser repetida (um arquivo já trocado não tem mais cópia em
  `novos/`). No boot, `persist_load` vê `PRONTA`/`APLICANDO`, termina a aplicação **antes** de carregar a árvore no VFS e, se o
  carregador acabou de ser trocado, reinicia para entrar no kernel novo.
- Custo conhecido: uma queda entre a troca e o sync da FAT deixa clusters perdidos da versão antiga (espaço desperdiçado, sem
  efeito no conteúdo); a 2ª cópia da FAT pode ficar atrasada como em qualquer escrita do driver.

`tools/updimg.c` testa isso no host com o driver FAT real: `updimg trocar <disco.img> build\volimg.exe` corta as escritas
depois da k-ésima (k = 0..todas) no meio de duas trocas (arquivo existente e arquivo novo), remonta, confere que cada
arquivo é a versão velha ou a nova inteira, repete a aplicação como o boot faz e roda o `volimg check`.

```
zig cc -target x86_64-windows-gnu -O1 -DMOS_HOST_TEST -Ikernel/include -Itools tools/updimg.c ^
  kernel/fs/{apfs,blkdev,bzip2,dfs,dmg,hfsplus,hfsplus_wr,inflate,lzfse,xz,fat}.c kernel/net/crypto.c -o build/updimg.exe
volimg mkdisk t.img 64 fat32:T:40
updimg trocar t.img build\volimg.exe
updimg instalar build\mOS.img        # imagem Live -> "instalada" (estado INSTALADO, conta "teste" sem senha), só para testes
updimg ver build\mOS.img /EFI/BOOT/BOOTX64.EFI
```

## Limites

- HTTP sem TLS por padrão (a autenticidade vem da assinatura; o conteúdo trafega aberto). `https://` funciona se o servidor
  tiver certificado válido para o kernel.
- Cada arquivo é baixado inteiro na memória (o cliente HTTP aceita, nesses pedidos, exatamente o tamanho do manifesto, em
  vez do limite de 128 MB das outras respostas).

## Testado (8 nov. 2026, QEMU/WHPX)

Imagem V1 do `build.ps1` convertida com `updimg instalar`; pacote V2 com kernel alterado, um arquivo modificado, um novo e um
removido, servido por `servir-atualizacao.ps1`:
- `atualizar procurar` achou exatamente 3 arquivos (9,8 MB, "inclui o kernel") e 1 remoção; `atualizar baixar` → PRONTA;
- VM morta à força com PRONTA (queda de energia): o boot seguinte aplicou antes de carregar a árvore e reiniciou; o kernel
  novo subiu, `procurar` disse "atualizado", conta e sessão preservadas, `volimg check` sem erros;
- pacote assinado por outra chave: recusado ("assinatura Ed25519 inválida");
- V3 (um arquivo) aplicado pelo Reiniciar normal (desktop.c).

Não testado: hardware real, pendrive Live persistente como alvo, origem `/Volumes/...`, arquivos de centenas de MB.
- Não há volta automática para a versão anterior depois de aplicada; um kernel novo que não boota precisa do pendrive.
- O Ed25519 (`kernel/include/ed25519_impl.h`, no estilo do TweetNaCl) foi conferido byte a byte contra o `crypto` do Node
  (chave pública e assinaturas de 3 mensagens); a verificação recusa S >= L.
