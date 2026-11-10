/* mOS — ponto de entrada do kernel após sair dos serviços de boot do UEFI */
#include "kernel.h"
#include "storage.h"
#include "proc.h"
#include "wifi.h"
#include "gpu.h"
#include "origin.h"

void kmain(void) {
    serial_init();
    kprintf("\n%s %s — Nucleo x86_64\n", MOS_NAME, MOS_VERSION);
    kprintf("Vídeo: %ux%u (pitch %u, %s)\n", g_boot->fb.width, g_boot->fb.height, g_boot->fb.pitch,
            g_boot->fb.bgr ? "BGR" : "RGB");
    gdt_init();
    idt_init();
    fpu_init();           /* AVX (XSAVE): antes do smp_init, que copia o CR4/XCR0 do BSP para os APs */
    pic_init();
    pmm_init();
    paging_init();
    pmm_reclaim_boot();
    swap_init();
    heap_init();
    pit_init(1000);
    sti();
    srand32((u32)rdtsc());
    acpi_init();
    pci_init();
    gpu_init();           /* GPUs Intel/AMD: detecção e modo atual, só leitura (dev/gpu.c) */
    ps2_init();
    usb_init();          /* antes do vfs_init: pendrives USB precisam estar enumerados para a persistência */
    ahci_init();          /* antes do VFS: o disco do sistema pode fornecer a árvore de arquivos */
    nvme_init();
    origin_resolve();     /* de qual disco/partição viemos? (confiável ou ambígua: ver fs/origin.c); antes do VFS */
    vfs_init();
    dfs_mount_all();
    storage_mount_all();
    audio_init();
    net_init();
    wifi_init();          /* placas Wi-Fi (ou o rádio simulado das imagens de teste) */
    bt_init();
    {
#ifdef MOS_UNIT_TESTS
        extern void unit_tests_run(void);
        unit_tests_run();         /* build de teste: só RAM (a política de escrita nos discos reais é idempotente) */
#endif
    }
    smp_init();           /* LAPIC, HPET e núcleos extras (operários de par_for: gráficos em paralelo) */
    nvme_smp_ready();     /* filas NVMe por CPU e MSI no LAPIC que o smp_init acabou de mapear */
    ahci_smp_ready();
    proc_init();          /* escalonador + processos de usuário (rodam quando o desktop fica ocioso) */
    kprintf("Boot concluído em %llu ms, heap %llu KiB\n", timer_ticks(), (u64)heap_used() / 1024);
    desktop_run();
}
