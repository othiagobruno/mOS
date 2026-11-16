# Plataforma AMD Ryzen R2544 — plano e estado (frente da Larissa)

Alvo: o PC do Thiago (Ryzen Embedded R2544, Raven2/Picasso, FCH 51/61). Fonte de verdade do hardware:
`lspci -nn` e as tabelas ACPI lidas nesta máquina em 15 nov. 2026 (DSDT 34004 B, SSDT1..6, FACP, APIC,
IVRS, HPET, MCFG). As tabelas **não** entram no repositório (são do fabricante); o harness de host recebe o
caminho por argumento.

## O que o firmware do Thiago expõe (lido do DSDT/SSDT reais)

| Item | Onde | Observação |
|---|---|---|
| `\_S0 \_S3 \_S4 \_S5` | DSDT | SLP_TYP 0/3/4/5. `/sys/power/mem_sleep` = `s2idle [deep]` → S3 real existe |
| `\_PTS(1)` / `\_WAK(1)` | DSDT | chamam `TPM.TPTS`, `SPTS`, `MPTS`, `PCI0.NPTS/NWAK/NAPE`, `MWAK`, `SWAK` — exige interpretador AML de verdade |
| `\_GPE._L08`, `_L19` | DSDT | wake de PCIe/USB (XHC0/XHC1) e do botão |
| `\_SB.PWRB` PNP0C0C | DSDT | botão de energia de controle (além do botão fixo da FADT) |
| `\_TZ.TZ01` | DSDT | `_TMP` lê `CM00` (SystemMemory 0xFED80700), `_CRT` 90 °C, `_PSV` 50 °C, `_PSL` P000..P007 |
| `EC0` PNP0C09, `_GPE 3` | DSDT | portas 0x62/0x66, `ERAM` sem campos — EC presente mas sem dados úteis |
| `_PCT/_PSS/_PSD/_CST` por CPU | SSDT2 "AMD CPU" | 3 P-states (3350/2000/1600 MHz pelo `acpi-cpufreq`), C-states por `_CST` |
| Ventoinha | — | não há `_FAN`/`PNP0C0B`: a ventoinha é do firmware/SMU. O mOS só lê temperatura |
| IVRS | tabela | AMD-Vi em 00:00.2 [1022:15d1] |

## Itens da frente e estado

Legenda: ✅ pronto com teste de host · 🔧 em andamento · ⬜ não começou. Cada linha diz o teste.

| # | Item | Estado | Teste / evidência |
|---|---|---|---|
| A1 | Interpretador AML `kernel/dev/aml.c` (namespace, métodos, Field/IndexField/BankField, OpRegion Memory/IO/PCI/EC/CMOS, Package, Buffer, CreateField, Mutex, Event, Notify, Load/LoadTable, `_CRS`) | ✅ | `tools/aml_host.c --teste` (52 verificações de `tools/aml_tests/teste.asl`, ASan+UBSan): `aml-teste: ok`. Tabelas reais do PC: DSDT+6 SSDT, 2084 nós, **0 erros**; `_PSS` 3350/2000/1600 MHz (igual ao `acpi-cpufreq` do Linux), `PCI0._PRT` 12 entradas, 42 `_CRS` decodificados: `aml-real: ok` |
| A2 | Todas as SSDT, `_REG`, `_STA`/`_INI` (95 dispositivos presentes, 8 `_INI` no PC), `\_OSI` (Windows 2000–2022), `_PIC(0)` | ✅ | harness com as tabelas reais |
| A3 | Desligar (S5) com `_TTS/_PTS(5)/_SI._SST` e SLP_TYP do `\_S5` avaliado; reinício pelo reset register da FADT (GAS: E/S, memória ou PCI) | ✅ código; ⬜ hardware | `_PTS(5)` roda sem erro no harness; falta o teste no PC |
| A4 | SCI (IRQ 9 da FADT, ELCR em nível), PM1 status/enable, botão de energia fixo → diálogo de desligar, GPEs de tempo de execução (`_Lxx/_Exx`, sem os de `_PRW`), EC (portas do `_CRS`/ECDT, `_Qxx`), trava global da FACS | ✅ código; ⬜ hardware | `acpi_poll()` no laço do desktop; GPE `_L08/_L19` avaliados no harness |
| A5 | S3 `kernel/dev/pm.c` + `pm.h`: `pm_register()` dos drivers, `_TTS/_PTS(3)/_SST`, `_DSW/_PSW` nos 11 dispositivos com `_PRW`, só GPEs de despertar + PWRBTN/RTC, vetor de 32 bits na FACS com o trampolim de modo real dos APs, contexto por `__builtin_setjmp`, GDT/TSS/IDT/CR0/CR3/CR4/EFER/XCR0/MSRs/MTRR/APIC_BASE, 8259/ELCR/PIT/LAPIC/HPET (contador continua), espaço PCI de todas as funções (pontes antes, MSI/MSI-X), IOMMU, `_WAK(3)`. Drivers: AHCI (FLUSH+STANDBY, COMRESET na volta), xHCI (Save/Restore State; SRE → reset e reenumeração), HDA/HDMI (refaz codec e streams com os mesmos buffers). **O S3 só é escolhido** quando os núcleos extras saem de linha (`pm_register_cpu_hotplug`, escalonador — Gustavo), o vídeo volta (`PM_RESTORES_DISPLAY` — Thais), a rede volta (`PM_RESTORES_NET` — Felipe) e não há pendrive ligado; senão vai para o s2idle | ✅ plataforma; ⏳ depende dos três ganchos; ⬜ hardware | harness com o firmware real: `_TTS/_PTS(3)/_SST(3)`, `_DSW/_PSW` e `_WAK(3)` sem erro de AML |
| A6 | s2idle (`acpi_sleep()` quando o S3 não serve): drivers quietos (FLUSH, áudio mudo), P-state mais baixo, `hlt` com consulta ao USB a cada 20 ms; acorda com tecla, mouse, botão de energia ou tecla de repouso | ✅ código; ⬜ hardware | só sintaxe |
| A7 | Térmica: Tctl do Zen pelo SMN 0x59800; crítico = zona em `_CRT` ou Tctl > 105 °C; passivo pela fórmula `_TC1/_TC2/_PSV` a cada `_TSP`, baixando o teto de P-state | ✅ código; ⬜ hardware | `_TMP/_CRT/_PSV/_TC1/_TC2/_TSP` avaliados no harness |
| A8 | P-states `kernel/dev/cpufreq.c`: `_PSS/_PCT/_PPC` (FFixedHW → MSR 0xC0010062 por núcleo; APs no tique do LAPIC), carga por MPERF/TSC, sobe ≥ 70 %, desce ≤ 30 %. Sem `_PSS`, lê 0xC0010064.. O R2544 não tem `_CPC` (CPPC) | ✅ código; ⬜ hardware | harness: `_PCT[0]` = FFixedHW 64 bits 0xC0010062 e controle = índice; MSRs lidos no PC: P0 FID 0x86/DID 8 = 3350, P1 2000, P2 1600 MHz |
| A9 | `_PRT` em modo PIC (`kernel/dev/acpi_prt.c`, `acpi_pci_irq()`): ponte raiz, `_PRT` das pontes ou rotação, link devices com `_CRS`/`_PRS`/`_SRS` e penalidade das IRQs legadas, conferência do `_SRS`. IOAPIC: **não** entra — o mOS usa MSI direto no LAPIC para os dispositivos e o 8259 para o legado (decisão do núcleo, frente do Gustavo) | ✅ | harness com o DSDT real: 00:14.0 INTA → IRQ 5 (LNKA), 03:00.6 INTC → IRQ 10 (LNKG); achou bug da BIOS: `LNKE._SRS` lê `PIRE` em vez do argumento, rota recusada |
| U1 | xHCI 15e0/15e1: enumeração USB 2/3 por consulta (o laço principal chama `usb_poll`; interrupções MSI ficam de fora de propósito: o teclado é lido a cada volta do laço e o s2idle consulta a cada 20 ms); suspensão S3 com Save/Restore State e, com SRE, reset + reenumeração; S3 recusado com pendrive ligado | ✅ código; ⬜ hardware | só sintaxe |
| U2 | Hubs: USB 2 (Genesys 05e3:0610) já funcionava; SuperSpeed (05e3:0626) novo — descritor 0x2A, SET_HUB_DEPTH, bits de porta USB 3, warm reset (BH) para link Inactive/Compliance | ✅ código; ⬜ hardware | só sintaxe |
| U3 | HID: teclado boot (Satechi interface 0) e mouse Sunplus (report id 1, X/Y 12 bits, roda) já funcionavam; teclas de mídia novas — `kernel/dev/hid_cc.c` (Consumer/System Control, array ou variável), `KEY_VOL_UP..KEY_LAUNCHPAD`, volume/mudo e botão de energia tratados no kernel; PS/2 E0 multimídia também | ✅ | `tools/hidcc_host.c` com os descritores reais do Satechi e do mouse (`tools/hidcc/*.rd`): `hidcc: ok` |
| S1 | AHCI 7901: NCQ (já na main), TRIM por DATA SET MANAGEMENT (IDENTIFY 169/105/69), `blkdev_t.discard` + `blk_discard()` através das partições, com a política de escrita; FLUSH CACHE EXT | ✅ código; ⬜ hardware | `tools/disktest.c` (empacotamento das faixas, 48 bits, 64 por bloco): `disktest: ok` |
| H1 | HDA: 15e3 com ALC269VC (fone 0x15 com HP+EAPD, microfone 0x18; snoop do FCH no reg. 0x42) + 15de HDMI/DP em `kernel/dev/hda_hdmi.c` (controlador próprio sem snoop, `clflush`, pin sense, ELD, conversor digital, espelho da mesma mistura) | ✅ código; ⬜ hardware | só sintaxe; codecs conferidos no `/proc/asound` do PC (ALC269VC, ATI aa01, monitor PG-CFCFS49U com LPCM 2 canais) |
| H2 | ACP 15e2 `kernel/dev/acp.c`: rev 0x00 (ACP3x). No firmware do PC o nó `\_SB.PCI0.GP17.ACP` só tem `_ADR` (nenhum codec I2S nem microfone PDM) e o Linux também não cria placa de som para ele; o driver confere isso no namespace e põe o ACP em D3hot (refaz depois do S3). Com codec descrito, fica em D0 e o log diz qual | ✅ | harness com o DSDT real: `ACP _ADR 0x5, 0 dispositivos filhos` |
| R1 | RTC: século pelo índice da FADT (0x32), 12 h corrigido, escrita (`rtc_set_utc`), alarme (`rtc_alarm_set_utc`) | ✅ código; ⬜ hardware | só sintaxe (`zig cc -fsyntax-only`) |
| I1 | IOMMU AMD-Vi `kernel/dev/iommu.c` + `iommu_pt.c`: IVRS (IVHD 0x40/0x11/0x10, faixas, apelidos, especiais), DTE de todos os requisitantes num domínio com tabela identidade de 2 MiB só sobre a memória do mapa UEFI (MMIO e BARs bloqueados), buffer de comandos com COMPLETION_WAIT/INVALIDATE_ALL, log de eventos (IO_PAGE_FAULT) no `acpi_poll`, interrupções sem remapear; desliga sozinho se o hardware não confirmar | ✅ código; ⬜ hardware | `tools/iommu_host.c` com a IVRS real: IVHD 0x40 em 0xFEB80000, EFR 0x004F77EF22294ADA, requisitantes de 00:01.0 a FF:1F.7 + 00A0/00A4; tabela: RAM e UMA mapeadas, MMIO/MSI/além da RAM bloqueados: `iommu: ok` |

## Ventoinha

O firmware do PC não tem `_FAN`, `PNP0C0B` nem controle de PWM no EC (a `ERAM` do `EC0` não tem campos), e o Linux
desta máquina também não expõe sensor de ventoinha (`hwmon`: só `acpitz`, `k10temp`, `amdgpu`). A ventoinha é do SMU/
firmware. O mOS cuida do lado que é dele: P-state pela carga, teto passivo pela zona térmica e desligamento crítico.

## Testes de host (rodam no Linux, sem build)

```bash
# interpretador AML: autoteste e as tabelas reais (a pasta das tabelas fica fora do repositório)
gcc -O1 -g -fsanitize=address,undefined -DAML_HOST -Ikernel/include tools/aml_host.c kernel/dev/aml.c kernel/dev/acpi_prt.c -o /tmp/aml
iasl -p /tmp/teste tools/aml_tests/teste.asl && /tmp/aml --teste /tmp/teste.aml      # aml-teste: ok
/tmp/aml -q DSDT.dat SSDT1.dat ... SSDT6.dat                                         # aml-real: ok
# IOMMU (tabela de páginas e a IVRS real)
gcc -O1 -fsanitize=address,undefined -DMOS_HOST_TEST -Ikernel/include tools/iommu_host.c kernel/dev/iommu_pt.c -o /tmp/iommu && /tmp/iommu IVRS.dat
# teclas de mídia com os descritores reais
gcc -O1 -fsanitize=address,undefined -DMOS_HOST_TEST -Ikernel/include tools/hidcc_host.c kernel/dev/hid_cc.c -o /tmp/hidcc && /tmp/hidcc tools/hidcc
# TRIM (faixas do DATA SET MANAGEMENT) e NCQ
gcc -O1 -fsanitize=address,undefined -DMOS_HOST_TEST -Ikernel/include tools/disktest.c kernel/dev/ahci_ncq.c kernel/dev/nvme_xfer.c -o /tmp/disk && /tmp/disk
```

As tabelas ACPI foram lidas de `/sys/firmware/acpi/tables` nesta máquina (sem a MSDM, que guarda a licença do Windows).

## O que só o hardware responde

No log serial do primeiro boot no PC: `ACPI: 7 tabelas AML ... 0 erros; SCI 9 ... S3 ok, S5 ok, reset ok 0xcf9`,
`cpufreq: 3 P-states (3350/2000/1600 MHz) por MSR`, `IOMMU: AMD-Vi em 0xfeb80000 ... requisitantes` (e nenhuma linha
`IO_PAGE_FAULT`), `ahci: porta N: TRIM`, `HDMI: pino 0x05 (monitor presente)` com o nome do monitor, `HDA: ... pino 0x15 fone`,
`USB HID: teclas de mídia na interface 1`, `ACP: rev 00 ... em D3hot`. Depois: volume pelas teclas do Satechi, botão de energia
abrindo o diálogo, desligar e reiniciar, repouso e volta (hoje s2idle até os ganchos de CPU, vídeo e rede existirem).
