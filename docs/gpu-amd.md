# Driver nativo da GPU AMD (Raven/Picasso/Raven2, 1002:15d8)

Dona: Thais (frente 1 de `TIME.md`). Código em `kernel/dev/amdgpu/`. Este arquivo diz o que está
pronto, com qual teste, e o que falta. Nada aqui foi executado no hardware pelo mOS ainda: o build e
o boot só acontecem no fim (`build-final.sh`).

## O hardware do Thiago (colhido no Linux dele, 15 nov. 2026, sem root)

| Item | Valor |
|---|---|
| PCI | `1002:15d8` rev PCI `0x75`, subsistema `1002:0123`, áudio HDMI `1002:15de` na função 1 |
| BARs | BAR0 VRAM visível `0xe0000000` (256 MiB), BAR2 doorbells `0xf0000000` (2 MiB), BAR4 I/O `0xd000`, BAR5 registradores `0xfcc00000` (512 KiB) |
| VRAM | 256 MiB reservados pela UEFI (carve-out), GTT 7,4 GiB |
| Firmware em uso | `picasso_*` (RLC `0x6f`, PFP `0xc3`, ME `0xa7`, CE `0x50`, MEC `0x1d8`, SDMA `0xa9`). O RLC `0x6f` só existe no `picasso_rlc.bin`: é um Picasso (rev_id do NBIO < 8), não Raven2 |
| Saídas | DP-1, DP-2, DP-3, HDMI-A-1. Monitor no HDMI, preferido 5120x1440 (EDID em `tools/testdata/amdgpu/edid-hdmi-thiago.bin`) |
| IP discovery | GC 9.1.0 (`0x2000`, `0xA000`), SDMA0 4.1.0 (`0x1260`), MMHUB 9.1.0 (`0x1A000`), DMU/DCN 1.0.0 (`0x12`, `0xC0`, `0x34C0`), NBIF 7.0.0 (`0x0`, `0x14`, `0xD20`, `0x10400`), OSSSYS 4.1.0 (`0x10A0`), MP0/MP1 10.0.0 (`0x16000`), HDP 4.1.0 (`0xF20`), SMUIO 10.0.0 (`0x16800`), THM 10.0.0 (`0x16600`), ATHUB 9.1.0 (`0xC20`), DF 2.1.0 (`0x7000`), UVD/VCN 1.0.0 (`0x7800`, `0x7E00`) |

As bases estão em dwords; registrador = base do segmento + deslocamento do header, vezes 4 em bytes.
Tudo cabe nos 512 KiB do BAR5, sem acesso indireto.

## Fontes e licenças

- Registradores: headers `asic_reg` do amdgpu (licença MIT da AMD). `kernel/dev/amdgpu/regs.h` é um
  subconjunto gerado à mão desses headers e mantém o aviso MIT da AMD.
- Microcódigo: `firmware/amdgpu/{picasso,raven2,raven}_*.bin` (o MEC2 é igual ao MEC e não entra; `picasso_rlc_am4.bin` e `raven_kicker_rlc.bin` também) do linux-firmware, licença
  "Redistributable-amdgpu" (`firmware/amdgpu/LICENSE.amdgpu`). Só redistribuição binária.
- Nada GPL no kernel. Nenhum código da Apple.

## Fases e estado

Legenda: ✅ pronto com teste de host · 🔶 escrito, sem teste no hardware · ⬜ falta.

| # | Fase | Estado | Teste |
|---|---|---|---|
| F0 | Escolha e validação do firmware: variante (Raven/Picasso/Raven2 pelo rev_id do NBIO), RLC AM4 e "kicker", cabeçalhos gfx v1, rlc v2.0/v2.1, sdma v1, gpu_info v1 (`ucode.c`) | ✅ parte lógica | `tools/amdgpu_host.c` com os 23 blobs reais |
| F0b | MMIO do BAR5, rev_id do NBIO, variante e RLC no hardware, faixa da VRAM na MMHUB (`amdgpu.c`) | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c` (Picasso simulado) |
| F1 | EDID 1.3/1.4, CTA-861 (DTD, SVD/VIC, HDMI VSDB e VICs HDMI, HF-VSDB/HF-SCDB, Y420 VDB/CMDB, FreeSync), DisplayID 1.x tipo I e 2.x tipo VII, DMT, CVT 1.2 e CVT-RB (`edid.c`) | ✅ parte lógica | `tools/amdgpu_host.c`: 34 modos do monitor real, preferido 5120x1440@60 como no Linux; CVT igual ao `cvt` do X. Leitura do EDID pelo DDC/AUX fica na F4 |
| F2 | VBIOS: VFCT do ACPI ou ROM do BAR, tabelas ATOM (master data/command), interpretador ATOM | ⬜ | VBIOS real (precisa de um dump com root) |
| F3 | SMU 10 (MP1): caixa de mensagens (C2PMSG 66/82/90), GetSmuVersion, PowerUpSdma (`engine.c`) | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c`. Falta: PowerUpGfx/GFXOFF (com a F8), relógios mínimos de display (com a F4b) |
| F4a | Display sem trocar o modo: leitura do timing de cada OTG e do plano de cada HUBP deixados pela UEFI, conferência com o framebuffer do GOP, vsync pelo OTG (vblank, posição, contador, taxa medida), troca de superfície no vblank (BGRA/RGBA linear, trava do OTG, confirmação por EARLIEST_INUSE), buffer duplo da tela do compositor na VRAM (WC), HPD, alocador de VRAM | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c`: 190/190 (timing de 25+ modos ida e volta, 40 quadros com áreas sujas aleatórias iguais à superfície, travamento da troca volta ao GOP) |
| F4b | Troca de modo e outras saídas: DDC/I2C e AUX por hardware (EDID ao vivo), DCCG/PLL, DIG + PHY por ATOM, HDMI (AVI, SCDC), DP (treino de link), DPP/MPC/OPP, watermarks, várias saídas | ⬜ | — |
| F5 | GMC da MMHUB 1.0: abertura do sistema na VRAM, AGP desligado, TLB L1, cache L2, contexto 0 com GART plana de 256 MiB (PTE sistema/snoop/UC), invalidação pelo motor 17 com semáforo; alocador de VRAM e BOs da ABI na VRAM (`engine.c`, `amdgpu.c`) | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c` (o SDMA simulado traduz pela tabela que o driver montou; página desligada ou só de leitura dá falta). Falta: GFXHUB (com a F8) e VMIDs 1..15 com tabelas de 4 níveis por processo (com a F9) |
| F6 | IH (OSSSYS 4.1): anel de interrupções, MSI, fontes vblank/pageflip/HPD/EOP/SDMA | ⬜ | host |
| F7 | SDMA 4.1: golden da família 4.1 e da variante, microcódigo direto, anel GFX de 64 KiB na VRAM com writeback do rptr, WPTR por registrador, teste do anel, COPY_LINEAR (até 4 MiB por pacote), cópia de retângulo, CONST_FILL, fences por polling (`engine.c`) | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c`: 221/221 (microcódigo real conferido palavra por palavra, 9 MiB VRAM→VRAM, RAM↔VRAM pela GART, retângulo 1000x300, 6000 pacotes dando 5 voltas no anel, SMU mudo e SDMA morto). Falta: interrupção de fence (F6) e power gating do SDMA |
| F8 | GFX9: RLC, CP (PFP/CE/ME/MEC), golden registers do Picasso/Raven2, CSB, anel GFX, IBs, fences EOP, fila de compute (HQD) | ⬜ | host |
| F9 | 3D para Metal/OpenGL: ABI revisão 3 combinada com o Otavio (`docs/gpu-abi.md`: PM4 no userland, IB validado pelo kernel, VA por contexto, `BO_EXEC`); o kernel faz VMID 1..15, validação e submit; o gerador de PM4 e o compilador GFX9 são do Otavio | ⬜ | host + combinar com o Otavio |
| F10 | Integração: `gpu_present`/`gpu_vsync_wait`/`gpu_kms_set` na AMD, Sobre/Monitores, build final | ⬜ | build final |

## O que já roda no boot (sem build ainda: confere no build final)

`gpu_init` → `amd_probe` → `amdgpu_probe`: lê tudo, liga o vsync pelo OTG e marca a AMD como dona da tela.
`fb_flush` → `gpu_present` → `amdgpu_present`: buffer duplo com troca no vblank; se a troca travar, volta
sozinho ao GOP (com o quadro atual) e o compositor segue com a cópia direta. `MOS_GPU_KMS_SET` troca a
superfície no vblank se for do mesmo tamanho; outro tamanho devolve `-MOS_EINVAL` até a F4b.

BOs `MOS_GPU_BO_GPU` vão para a VRAM quando há uma Raven (até 64 MiB cada; em RAM continuam com 16 MiB), são
zerados, mapeados no processo em WC por `gpu_user_map_io` do Gustavo (sem refcount: unmap, fork e saída nunca
devolvem VRAM ao PMM) e podem ir para a tela por `MOS_GPU_KMS_SET` quando o compositor não está com o buffer
duplo. Falta na ABI (não é só da AMD): liberar os BOs de um processo que sai sem `BO_DESTROY`; hoje ficam
presos até o boot seguinte, na RAM e na VRAM.

`gpu_late_init` (main.c, depois do `bt_init`, com o VFS montado) → `amdgpu_engines_init`: SMU, GMC e SDMA.
Se o SMU não responder ou o anel não rodar, os motores ficam desligados e a tela segue como antes. Quando o
buffer duplo desliga, a cópia da frente para o GOP passa pelo SDMA.

Suspensão (combinado com a Larissa, `kernel/dev/pm.c`): o driver ainda não chama `pm_register`. Sem isso o
sistema usa s2idle e a tela fica acesa; S3 só quando o resume refizer firmware, anéis e modo (depende da F4b).
IOMMU: a GART só aponta para RAM do mapa UEFI, que o AMD-Vi deixa em identidade. Um `IO_PAGE_FAULT` do 03:00.0
no serial vai para a Larissa.

## Ordem de bring-up no boot

1. A UEFI (GOP) já fez o ASIC init e deixou o HDMI aceso com o framebuffer em VRAM. O driver não
   repete o ASIC init; confere `CONFIG_MEMSIZE`, `MC_VM_FB_LOCATION` e o estado do OTG.
2. Firmware direto (sem PSP): `fw_load_type` 0 do amdgpu vale para a família Raven. RLC, CP e SDMA
   recebem o microcódigo pelos registradores `*_UCODE_ADDR/DATA`.
3. SMU10 liga GFX e SDMA; depois GMC, IH, SDMA, GFX, e por último o display (troca de modo).
4. Se qualquer etapa falhar, o framebuffer do UEFI continua sendo a tela (comportamento de hoje).

## Combinações com outras frentes

- Otavio (Metal/OpenGL/Core Animation): ABI de 3D da F9 e o backend de shader.
- Larissa (áudio HDMI `1002:15de`): o endpoint de áudio do DCN (AZ) fica comigo; o controlador HDA com ela.
- Camila (desktop/compositor): `gpu_present` e troca de resolução pelo Ajustes → Monitores.
