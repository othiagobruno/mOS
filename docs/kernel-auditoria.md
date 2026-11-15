# Auditoria do núcleo (kernel/proc, kernel/mm), novembro de 2026

O que foi corrigido neste ramo, como foi testado, e os pontos abertos que a leitura achou, por prioridade.
Ambiente: Linux, zig 0.17.1 ELF, QEMU 8 com OVMF 4M, TCG `-cpu max -smp 4`. Não houve Windows nem máquina física.

## Corrigido

### 1. Proteção máxima da memória compartilhada

Antes, três caminhos davam escrita a uma página que o processo só podia ler:

| Caminho | Efeito antes |
|---|---|
| `mprotect(PROT_WRITE)` (`SYS_MMAP b = 7`) num `MAP_SHARED` de fd `O_RDONLY` | `uvm_protect` ligava `PTE_RW`; o `munmap` gravava a página de volta no arquivo (`lazy_sync`) |
| `read()` (ou qualquer `uvm_copy_out`) com destino num mapa só-leitura | o kernel escrevia na página física compartilhada; só o COW era respeitado |
| `mach_vm_map` de uma entrada criada só-leitura, depois `mprotect` ou `read()` | a memória do outro processo mudava; o `KERN_INVALID_RIGHT` do `machipc.c` só valia na hora do mapa |
| `MACH_VM_REMAP` sem cópia de uma página assim | o apelido nascia gravável |

O Chromium confere que uma região só-leitura é mesmo só-leitura (comentário em `machipc.c`, `CheckPlatformHandlePermissionsCorrespondToMode`); essa garantia não valia.

Agora:

- `PTE_NOW` (bit 53, ignorado pelo hardware numa PTE presente; o 52 é a marca de swap numa PTE ausente) marca a página compartilhada cuja proteção máxima não tem escrita.
- `uvm_map_shared` põe `PTE_NOW` quando o prot não tem escrita nem `UVM_PROT_MAYW` (8). `lazy_add_shared_file` recebe `MAYW` só se o fd tem `O_WRONLY|O_RDWR`; a anônima compartilhada sempre tem.
- `uvm_map_phys(p, va, phys, UVM_PHYS_W | UVM_PHYS_MAYW)`: o `mach_vm_map` passa `MAYW` só se a entrada (`mprot`) permite escrita.
- `lazy_protect` devolve `-EACCES` se a faixa compartilhada não tem `MAYW`; `uvm_protect` recusa ligar `PTE_RW` com `PTE_NOW`.
- `uvm_copy_out` devolve falso (EFAULT) numa página sem `PTE_RW` que seja `PTE_SHARED`, `PTE_NOW` ou com mais de um dono. Só-leitura privada (texto, JWP) continua aceitando: o carregador e o `tracepoint.c` escrevem nela.
- O apelido do `MACH_VM_REMAP` herda o `PTE_NOW`.

Teste: `userland/mmshared.c`, oito checagens novas no fim. Mesmo teste nos dois kernels, mesma ESP:

| | `main` (eafefae) | este ramo |
|---|---|---|
| `mprotect(PROT_WRITE)` sem escrita no fd | 0 (FALHOU) | `-EACCES` |
| escrita na página depois disso | o filho escreve (FALHOU) | SIGBUS |
| `read()` para o mapa só-leitura | 16 bytes gravados, arquivo vira `W` (FALHOU) | `-EFAULT`, arquivo intacto |
| fd `O_RDWR`, mapa `PROT_READ`, `mprotect` RW | ok | ok (controle) |
| entrada Mach só-leitura: `mprotect` / `read()` | 0 / 8 bytes no original (FALHOU) | `-EACCES` / `-EFAULT` |
| `mmshared` inteiro | 6 FALHA(S) | TODOS OS TESTES OK |

### 2. Pilha do kernel

`clang -fstack-usage` em todo o `kernel/`: `proc_syscall_` tinha frame de **14 152 bytes** numa pilha de 32 KiB (`KSTACK_SIZE`, bloco de `kmalloc` sem página de guarda). O vetor `mos_spawn_fa_t acts[48]` (13 KiB) do `SYS_SPAWNVEC` entrava no frame de toda syscall. Agora vai para o heap: o frame caiu para **1 928 bytes**. O `path` de cada ação, que vem do ring 3, passa a terminar em NUL.

`kstack_check` (sched.c): duas palavras de marca no fundo de cada pilha do kernel, conferidas em toda troca de thread. Um estouro vira `panic("pilha do kernel estourou: thread N ...")` em vez de corromper o cabeçalho do heap e aparecer longe dali.

Teste: `userland/parateste.c` ganhou um `SYS_SPAWNVEC` com 48 ações (47 `CLOSE` com `path` sem NUL e um `OPEN` de stdout para arquivo).

### 3. Menores

- `copy_str_in` copiava um byte por `uvm_copy_in` (quatro níveis de tabela por byte de caminho). Agora copia por trecho de página.
- `tools/build.sh`, `tools/build-libcxx.sh` e `tools/swift-rt-grok-teste.sh`: a pasta do zig sai do binário real (`readlink -f`). Com `~/.local/bin/zig` como link, procuravam `lib/` em `~/.local` e paravam em "libunwind: falta any-darwin-any" / "libcxx do zig ausente".
- `ahci.c:stage_at`: com NCQ, o tag só é reusado com PxCI e PxSACT limpos (mesma regra do `ahci_reap` do PR #231). Hoje o reuso já é protegido pelo lote e pelo `recover_port` nos caminhos de erro; é uma defesa a mais.

### Revisão do PR #231 (disco), depois do merge

`ahci_reap` com `PxCI | PxSACT`, página do `pcache` nascendo com `stamp`, `evict_one` pulando `valid == 0`, política no `blk_write` e `blk_flush` no `storage_revoke_write`: corretos. Os caminhos de falha e de prazo do NCQ (`wait_batch`) passam por `recover_port`, que limpa PxCI e PxSACT antes de o tag voltar. Única sobra: página criada por um `load_run` cuja leitura falhou fica com `valid == 0` e agora não sai mais pelo `evict_one`; é reaproveitada na próxima leitura do mesmo deslocamento, então o custo é no máximo uma janela de leitura antecipada por erro de disco.

## Pontos abertos, por prioridade

1. **fd guarda só o caminho** (`struct fdent.path`, sem vnode). `SYS_READ` refaz `vfs_lookup` a cada chamada. Arquivo apagado com o fd aberto dá `ENOENT`; `rename` por cima faz o fd ler o arquivo novo. SQLite (journal), git e o salvar atômico dos editores dependem do contrário. Pede vnode com contagem de referência e órfão até o último `close`.
2. **Trava grande (BKL)** e varreduras O(threads) sob ela: `sweep_ready` em todo `next_ready`, `wake_sleepers` a cada tique, `SYS_FUTEX_WAKE` percorre todas as threads do sistema. Com Electron (centenas de threads) isso é custo por troca de contexto. Próximo passo medido: fila de futex por (processo, endereço) e lista de dormentes ordenada por prazo.
3. **Futex só privado**: a chave é `(proc, va)`. Um `pthread_mutex` com `PTHREAD_PROCESS_SHARED` numa página `MAP_SHARED` não acorda o outro processo.
4. **Outros frames grandes no kernel** (`-fstack-usage`): `pkgextract.c:add_component` 32 936 bytes (roda na thread do desktop, pilha de boot de 1 MiB; numa thread do kernel estouraria), `launchsvc.c` 6 KiB em quatro funções, `web/layout.c:layout_block` 6,4 KiB com recursão.
5. **Página anônima nasce RWX** (`SYS_MMAP b = 3` sem prot). Só o `mprotect` liga o NX.
6. **KVM nesta máquina**: com `-accel kvm` o OVMF 4M trava antes do `BdsDxe` na maioria das partidas (3 de 3 com 80 s); quando passa, o mOS sobe em 2 s e o autoteste passa. É do ambiente (Ryzen R2544, OVMF do Ubuntu), não do kernel, mas impede rodar a suíte rápida aqui.
7. `tools/build.sh` não fecha a imagem sem rede: depois do kernel ele baixa o `Claude.zip` (`downloads.claude.ai`) e para se o download falha. O kernel e o libSystem saem antes disso.

## Como repetir

```bash
# kernel (libSystem já em build/): flags do build.ps1
zig cc -target x86_64-uefi-msvc -O2 ... $(find kernel -name '*.c' -o -name '*.S') -o BOOTX64.EFI
# ESP mínima: EFI/BOOT/BOOTX64.EFI, root/mOS, userland/*.c como ELF em Sistema/Programas, .autoteste = "teste",
# e os três Mach-O do -Tests (posixteste, dispatchteste, sinaisteste)
qemu-system-x86_64 -machine q35 -m 1024 -cpu max -smp 4 \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=vars.fd \
  -drive file=fat:rw:esp,format=raw,if=ide,index=0 \
  -vga std -display none -serial file:serial.log -nic none -no-reboot -monitor none
```
