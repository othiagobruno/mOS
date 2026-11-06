# mOS × ravynOS — o que falta no mOS e existe no ravynOS

Referência: `C:\Users\Ruth Miranda\Desktop\ravynos`, snapshot `60aa4ff` (ravynOS 0.7.1, baseado em **XNU/Darwin 19**, não mais em FreeBSD).

## Retrato rápido

| | **mOS (Nucleo)** | **ravynOS** |
|---|---|---|
| Kernel | Monolítico, um só espaço de endereçamento, tudo em ring 0, laço de *polling* | XNU completo: Mach + camada BSD + IOKit (~2,3 mi de linhas) |
| Boot | `BOOTX64.EFI` próprio, usa o GOP do UEFI, `boot_info_t` | Booter `efiloader` (**não está no ZIP**) → `boot_args` do XNU + kernelcache pré-linkado |
| Apps | Compiladas dentro do kernel (`kernel/apps/*`) | Processos Mach-O separados, carregados pelo dyld |
| Interface gráfica | `gfx`/`wm`/`desktop` no kernel | WindowServer (processo) + AppKit/Foundation (Cocotron) via Mach IPC |
| Build | Zig cc no Windows → imagem GPT/FAT32 | bmake + toolchain LLVM própria, host macOS ou Linux |

Por isso, quase nada do ravynOS **encaixa direto** no mOS. Ele serve mais como **referência de implementação** e **fonte de drivers e bibliotecas para portar**.

## Lacunas por área

Legenda: ✅ o mOS já tem · 🟡 parcial ou stub · ❌ não tem

### 1. Núcleo do kernel
| Recurso | mOS | Onde está no ravynOS | Observação |
|---|---|---|---|
| Processos / espaço de usuário (ring 3) | ✅ `proc/process.c`: PML4 por processo, falhas isoladas, `kill` | `xnu/bsd/kern` (proc, fork, exec), `xnu/osfmk/x86_64` | Sem `fork` (só `spawn`). |
| Threads + escalonador preemptivo | ✅ `proc/sched.c`: threads do kernel e de usuário, fatias de 4 ms | `xnu/osfmk/kern/sched_*.c` | O desktop é a thread 0 cooperativa (cede só em `hlt`). |
| Syscalls | ✅ `int 0x80`, 31 chamadas (`include/syscall.h`, `-errno`) | `xnu/bsd/kern/syscalls.master`, `Libraries/Libsystem/libsystem_kernel` | Tabela própria; o Mach-O entra por cima (sessão de compatibilidade). |
| VM com espaços por processo, *page fault*, COW | 🟡 espaços por processo, `brk`, `mmap`/`munmap` anônimo; sem COW nem paginação sob demanda | `xnu/osfmk/vm/` | |
| IPC (portas Mach / pipes) | 🟡 pipes (`pipe`, `spawnio`, `dup2`, `a \| b` no msh), sessões de terminal, `wait`/`waitany` | `xnu/osfmk/ipc/`, `xnu/bsd/kern` (pipes, kqueue) | Sem portas Mach nem kqueue. |
| SMP / APIC / HPET | ✅ `arch/smp.c`: MADT, LAPIC calibrado, HPET, APs acordados (INIT-SIPI) | `Kernel/Extensions/AppleAPIC`, `xnu/osfmk/i386` | Os APs ficam ociosos: UI, rede, USB e áudio são de um núcleo só. |
| Loader ELF64 | ✅ `proc/elf.c`: estático, em 0x400000000000 | — | Programas em `userland/`, compilados pelo `build.ps1`. |
| Loader Mach-O | 🟡 gancho `macho_load()` no `proc_spawn` (sessão "Compatibilidade DMG" implementando) | `xnu/bsd/kern/mach_loader.c`, `mach_fat.c`; `Libraries/dyld` | |
| Sinais, tty/pty, kqueue | 🟡 sessões de terminal (`proc_spawn_tty`, `read(0)` bloqueante, Ctrl+C via `proc_tty_interrupt`); sem handlers de sinal no usuário | `xnu/bsd/kern` | |
| Segurança (MAC, credenciais) | ❌ | `xnu/security` | Baixa prioridade. |

### 2. Drivers
| Recurso | mOS | ravynOS | Observação |
|---|---|---|---|
| Framebuffer GOP | ✅ | `IOGraphics/IOGOPFramebuffer.cpp` | O mOS já cobre isto. |
| PS/2 teclado/mouse | ✅ | `ApplePS2Controller` | |
| PCI | ✅ (enumeração) | `IOPCIFamily`, `AppleI386PCI` | MSI/MSI-X e PCIe ECAM: referência no IOPCIFamily. |
| ACPI | 🟡 (poweroff/reboot) | `IOACPIFamily` (CPU, RTC, NVRAM EFI) | |
| **Disco SATA (AHCI)** | ✅ `dev/ahci.c` (polling, leitura/escrita LBA48) | `Kernel/Extensions/RavynAHCIPort` | Driver recente e pequeno. **É o melhor candidato a portar.** |
| IDE/ATA | ❌ | `IOATAFamily`, `AppleIntelPIIXATA` | Legado, só útil no QEMU. |
| NVMe | ✅ `dev/nvme.c` (polling, leitura/escrita, vários namespaces) | ❌ | O mOS está na frente aqui. |
| **USB (xHCI)** | ✅ `dev/xhci.c`, `usb_hub.c` (sessão principal) | `RavynXHCIPort` (fora do build) | |
| Teclado/mouse USB (HID) | ✅ `dev/usb_hid.c` (sessão principal) | `IOHIDFamily` | |
| Armazenamento USB | ✅ `dev/usb_msc.c` + persistência no pendrive | `RavynXHCIMassStorageDisk` | |
| Rede (e1000/virtio) | ✅ e1000, rtl8139, IPv4/DHCP/DNS/TCP/TLS/HTTP (sessão principal) | ❌ (nenhum driver NIC no tree) | |
| Áudio (HDA) | ✅ `dev/hda.c` (sessão principal) | ❌ | |
| Bluetooth | 🟡 HCI (sessão principal) | ❌ | |
| NVRAM EFI | ❌ | `IOACPIFamily/EFINVRAM.cpp` | |

### 3. Sistemas de arquivos e armazenamento
| Recurso | mOS | ravynOS | Observação |
|---|---|---|---|
| APFS (leitura) | ✅ | ❌ (só headers de terceiros) | **Aqui o mOS está na frente.** |
| HFS+ leitura | ✅ | ✅ `Kernel/Extensions/hfs` | |
| **HFS+ escrita, journal, fsck, newfs** | ❌ | `Extensions/hfs`, `BSD/sbin/{fsck,newfs,mount}_hfs` | Para ter disco persistente, é o caminho mais completo. |
| **FAT12/16/32 (msdosfs)** | ✅ `fs/fat.c` leitura e escrita com LFN, formatação FAT32 (`fat_format`); `fs/persist.c` grava o VFS em `\mOS\` | `Kernel/Extensions/msdosfs` | Volumes FAT de outros discos aparecem em `/Volumes` só para leitura. |
| Partições GPT (escrita) e instalador | ✅ `fs/gpt.c`, `fs/install.c` (app "Instalar mOS" da sessão principal) | `SysApps/Installer` | Testado: SATA → NVMe, o disco instalado boota sozinho. |
| DMG (UDIF) | ✅ | ❌ no kernel | |
| Partições GPT/APM/MBR | ✅ | `IOStorageFamily` | |
| VFS gravável / persistência | ✅ VFS em RAM gravado no disco do sistema (SATA, NVMe ou pendrive) | `xnu/bsd/vfs` | |
| devfs, NFS | ❌ | `xnu/bsd/miscfs`, `xnu/bsd/nfs` | Baixa prioridade. |

### 4. Rede
| Recurso | mOS | ravynOS |
|---|---|---|
| Ethernet/ARP/IPv4/ICMP/UDP/DHCP/DNS/TCP | ✅ `kernel/net/` (sessão principal) | Pilha BSD completa: `xnu/bsd/net*`, `netinet`, `netinet6` |
| TLS | ✅ `net/tls.c` + `net/crypto.c` próprios | `BSD/lib/bearssl` (MIT) |
| HTTP | ✅ `net/http.c` | `BSD/usr.bin/curl` |
| IPv6 | ❌ | `netinet6` |

### 5. Bibliotecas e runtime
| Recurso | mOS | ravynOS | Observação |
|---|---|---|---|
| libc | 🟡 kernel: `lib/string.c`, `printf.c`; usuário: `userland/mos.h` (syscalls, string, malloc, print, readline) | `Libsystem/libsystem_c`, `BSD/lib/msun` (libm) | |
| libm | 🟡 floats aproximados (`lib/math.c`) | `BSD/lib/msun` (BSD) | Dá para portar funções individualmente. |
| malloc | ✅ `kmalloc` | `libsystem_malloc` | |
| zlib/bzip2/LZFSE | ✅ (próprios) | `Libraries/zlib`, `BSD/lib/bzip2`, `zstd` | |
| Imagens PNG/JPEG | ✅ `kernel/web/image.c` (sessão principal) | `BSD/lib/{libpng,jpeg,libtiff}` | |
| Fontes TrueType | ✅ (`gfx/font.c`) | FreeType (em `Onyx2D`) | Se o rasterizador próprio limitar, a alternativa é o FreeType. |
| Unicode / ICU | 🟡 NFD→NFC latino | `Libraries/ICU` | |
| SQLite | ❌ | `BSD/lib/sqlite3` | |
| Objective-C runtime, Blocks, libdispatch | ❌ | `Libraries/objc4`, `libsystem_blocks`, `libdispatch` | Só faz sentido com espaço de usuário. |

### 6. Interface e frameworks
| Recurso | mOS | ravynOS | Observação |
|---|---|---|---|
| Servidor de janelas | ✅ (`ui/wm.c`, no kernel) | `CoreServices/WindowServer` | O mOS é mais simples, mas funciona. |
| Barra de menus | ✅ (em `desktop.c`) | `WindowServer/SystemUIServer` | |
| Dock | ✅ (em `desktop.c`) | `CoreServices/Dock` | |
| Finder | ✅ `apps/arquivos.c` | `CoreServices/Filer` | |
| Tela de login / contas | 🟡 backend `lib/account.c` pronto (contas, senha com hash e sal, HOME por conta); tela na sessão principal | `WindowServer/LoginWindow` | |
| Preferências | ✅ `apps/prefs.c` | `SysApps/Prefs` | |
| Instalador em disco | ✅ `apps/instalador.c` + `fs/install.c` | `SysApps/Installer` | |
| Captura de tela | 🟡 `lib/screenshot.c` (`screenshot_save` → BMP em ~/Imagens); falta o atalho na UI | `SysApps/grabscr.m` | |
| Renderização 2D vetorial (paths, antialiasing, gradientes) | 🟡 (`gfx.c`) | `Frameworks/Onyx2D` (MIT, ~285 mil linhas) | Ótima referência para o `gfx`. |
| Foundation / AppKit / CoreFoundation | ❌ | `Frameworks/{Foundation,AppKit,CoreFoundation}` | Só fazem sentido com userland e runtime ObjC. |
| Navegador | 🟡 stub ("Horizonte") | ❌ | |

### 7. Userland e serviços
| Recurso | mOS | ravynOS |
|---|---|---|
| init / launchd | ✅ `userland/init.c`: serviços em `/Sistema/Biblioteca/LaunchDaemons/*.servico`, com `ManterVivo` | `BSD/sbin/launchd`, `SystemLibrary/LaunchDaemons` |
| Shell real (sh/zsh/bash) | ✅ `userland/sh.c` (msh em ring 3: cd, ls, cat, ps, kill, programas, `&`) + terminal embutido | `BSD/bin/sh`, `BSD/usr.bin/zsh`, `GNU/bin/bash` |
| Utilitários Unix (ls, cp, grep, ...) | 🟡 embutidos do msh e do terminal | `BSD/bin`, `BSD/usr.bin` |
| syslog, notifyd, configd, mDNS | ❌ | `BSD/usr.sbin/*` |
| Toolchain nativa (clang/ld64) | ❌ | `Developer/Default.xctoolchain` |

## O que dá para reaproveitar de fato (e licenças)

| Item | Licença | Esforço | Por quê |
|---|---|---|---|
| `BSD/lib/bearssl` → TLS 1.2/1.3 do `net.h` | MIT | Médio | Sem alocação dinâmica, feito para embarcado. |
| `RavynAHCIPort` → driver SATA próprio | Verificar o cabeçalho (provável MIT ou BSD) | Médio | Abre caminho para disco real. Precisa tirar o IOKit (é C++). |
| `Extensions/msdosfs` → ler e gravar no ESP | APSL-2.0 | Médio | Dá persistência sem criar um FS novo. |
| `Extensions/hfs` (escrita/journal) | APSL-2.0 | Alto | Persistência no formato do macOS. |
| `BSD/lib/msun` (funções de libm) | BSD | Baixo | Precisão maior que o `lib/math.c`. |
| `BSD/lib/libpng` (+ zlib própria) | libpng | Baixo/Médio | Wallpapers e ícones em PNG. |
| `IOHIDFamily` (parser de descritores) | APSL-2.0 | Médio | Para quando o xHCI existir. |
| `Onyx2D` (rasterização) | MIT | Referência | Algoritmos de path, antialiasing e blending. |
| `xnu/bsd/kern/mach_loader.c` | APSL-2.0 | Referência | Quando houver processos. |

**APSL-2.0**: dá para usar, mas as modificações nos arquivos derivados precisam ser publicadas com o código. **GPL** (bash, partes do CoreText): evitar se o mOS não for GPL.

## Estado (6 nov. 2026)

Os itens 1 a 5 da ordem original estão feitos: AHCI + NVMe + FAT gravável e persistência, rede, xHCI + HID + pendrive, APIC/HPET/SMP + escalonador e ring 3 + syscalls + ELF, além de init, shell e instalador. Testes: `tools/fattest.c` no host e `build.ps1 -Tests`, que roda no boot os autotestes de modo usuário e do shell, com saída na serial.

O que ainda falta:
- **Mach-O**: o carregador está sendo feito pela sessão "Compatibilidade DMG", sobre o gancho `macho_load()`.
- **Trabalho real nos APs**: exige trancas (locks) na UI, no VFS e nos drivers, que hoje dependem de um núcleo só.
- **Escrita em volumes de `/Volumes`**: FAT de outros discos e HFS+.
- **Memória virtual**: COW e paginação sob demanda.
- **Processos**: handlers de sinal no usuário e `fork`.
- **Interface**: tela de login (sessão principal, com o backend de contas já pronto), atalho de captura de tela (sessão de UI).
- **Grandes portes**: IPv6, objc4/Foundation/AppKit e toolchain nativa.
