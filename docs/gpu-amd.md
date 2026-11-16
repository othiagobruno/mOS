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
| F3 | SMU10 (MP1): caixa de mensagens, PowerUpGfx/Sdma, dispclk/dppclk/dcfclk/socclk mínimos | ⬜ | simulador de mailbox |
| F4a | Display sem trocar o modo: leitura do timing de cada OTG e do plano de cada HUBP deixados pela UEFI, conferência com o framebuffer do GOP, vsync pelo OTG (vblank, posição, contador, taxa medida), troca de superfície no vblank (BGRA/RGBA linear, trava do OTG, confirmação por EARLIEST_INUSE), buffer duplo da tela do compositor na VRAM (WC), HPD, alocador de VRAM | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c`: 190/190 (timing de 25+ modos ida e volta, 40 quadros com áreas sujas aleatórias iguais à superfície, travamento da troca volta ao GOP) |
| F4b | Troca de modo e outras saídas: DDC/I2C e AUX por hardware (EDID ao vivo), DCCG/PLL, DIG + PHY por ATOM, HDMI (AVI, SCDC), DP (treino de link), DPP/MPC/OPP, watermarks, várias saídas | ⬜ | — |
| F5 | GMC v9: gfxhub + mmhub, GART do sistema, VMID 1..15 com tabelas de 4 níveis, alocador de VRAM e GTT, BOs da ABI | ⬜ | host |
| F6 | IH (OSSSYS 4.1): anel de interrupções, MSI, fontes vblank/pageflip/HPD/EOP/SDMA | ⬜ | host |
| F7 | SDMA 4.1: microcódigo, anel, copy/fill linear, fences; cópia do compositor pela GPU | ⬜ | host |
| F8 | GFX9: RLC, CP (PFP/CE/ME/MEC), golden registers do Picasso/Raven2, CSB, anel GFX, IBs, fences EOP, fila de compute (HQD) | ⬜ | host |
| F9 | 3D para Metal/OpenGL: contexto AMD na ABI (`docs/gpu-abi.md`), IB PM4 do usuário com VM por processo, biblioteca PM4 no userland, backend GCN (GFX9) para os shaders do Otavio | ⬜ | host + combinar com o Otavio |
| F10 | Integração: `gpu_present`/`gpu_vsync_wait`/`gpu_kms_set` na AMD, Sobre/Monitores, build final | ⬜ | build final |

## O que já roda no boot (sem build ainda: confere no build final)

`gpu_init` → `amd_probe` → `amdgpu_probe`: lê tudo, liga o vsync pelo OTG e marca a AMD como dona da tela.
`fb_flush` → `gpu_present` → `amdgpu_present`: buffer duplo com troca no vblank; se a troca travar, volta
sozinho ao GOP (com o quadro atual) e o compositor segue com a cópia direta. `MOS_GPU_KMS_SET` troca a
superfície no vblank se for do mesmo tamanho; outro tamanho devolve `-MOS_EINVAL` até a F4b.

Pendência com o Gustavo (memória virtual): `gpu_user_map` conta referência por página e `gpu_user_unmap`
chama `page_put`, que faria `pmm_free` de páginas da VRAM. Por isso BOs `MOS_GPU_BO_GPU` continuam em RAM;
para mapear VRAM em processos falta um mapeamento de dispositivo sem refcount (pedido feito a ele).

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
