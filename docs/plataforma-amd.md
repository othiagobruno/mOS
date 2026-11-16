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
| A5 | Suspensão S3: `_PTS(3)`, FACS waking vector, trampolim de 16 bits, salvar/restaurar CPU, LAPIC, PCI e drivers, `_WAK(3)` | ⬜ | só no hardware; harness para a sequência |
| A6 | s2idle de reserva (`hlt` em todos os núcleos, wake por GPE) | ⬜ | |
| A7 | Térmica: Tctl do Zen pelo SMN 0x59800 (o `_TMP` do TZ01 só lê 4 bits de `CM00`), `_CRT` a cada 5 s com desligamento crítico; falta limitação passiva por P-state | 🔧 | `_TMP/_CRT/_PSV` avaliados no harness (90 °C / 50 °C) |
| A8 | P-states: `_PSS/_PCT` (FFixedHW → MSR 0xC0010062 PStateCtl) e CPPC (`_CPC`, se existir) | ⬜ | |
| A9 | `_PRT` (roteamento de IRQ PCI) e IOAPIC pela MADT | ⬜ | |
| U1 | xHCI 15e0/15e1: interrupções MSI, USB 3.x, quirks AMD, suspensão/retomada | ⬜ | |
| U2 | Hubs Genesys (USB2 e USB3 SuperSpeed) | ⬜ | |
| U3 | HID: Satechi 0c45:7016 (teclas de mídia, consumer page), mouse Sunplus | ⬜ | |
| S1 | AHCI 7901: NCQ (já na main), TRIM por DATA SET MANAGEMENT (IDENTIFY 169/105/69), `blkdev_t.discard` + `blk_discard()` através das partições, com a política de escrita; FLUSH CACHE EXT | ✅ código; ⬜ hardware | `tools/disktest.c` (empacotamento das faixas, 48 bits, 64 por bloco): `disktest: ok` |
| H1 | HDA: 15e3 com ALC269VC (fone 0x15 com HP+EAPD, microfone 0x18; snoop do FCH no reg. 0x42) + 15de HDMI/DP em `kernel/dev/hda_hdmi.c` (controlador próprio sem snoop, `clflush`, pin sense, ELD, conversor digital, espelho da mesma mistura) | ✅ código; ⬜ hardware | só sintaxe; codecs conferidos no `/proc/asound` do PC (ALC269VC, ATI aa01, monitor PG-CFCFS49U com LPCM 2 canais) |
| H2 | ACP 15e2 (co-processador de áudio; DMIC) | ⬜ | |
| R1 | RTC: século pelo índice da FADT (0x32), 12 h corrigido, escrita (`rtc_set_utc`), alarme (`rtc_alarm_set_utc`) | ✅ código; ⬜ hardware | só sintaxe (`zig cc -fsyntax-only`) |
| I1 | IOMMU AMD-Vi pela IVRS (tabela de dispositivos, remapeamento DMA) | ⬜ | |

Ordem: A1 → A2 → A3 → A4 → A9 → A7 → A8 → S1 → H1 → U1..U3 → R1 → I1 → A5/A6 → H2.
Build completo só no fim, pela fila `build-final.sh`.
