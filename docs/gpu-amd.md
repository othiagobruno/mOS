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
| Saídas | DP-1, DP-2, DP-3, HDMI-A-1 (VBIOS: DP DDC1/HPD1, HDMI DDC2/HPD2 em UNIPHY0 link B, DP DDC3/HPD3 e DDC4/HPD4; conferido no HPD2 vivo). Monitor no HDMI, preferido 5120x1440 (EDID em `tools/testdata/amdgpu/edid-hdmi-thiago.bin`) |
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
| F2 | VBIOS e ATOM (`atom.c`, `vbios.c`): imagem pela VFCT do ACPI; interpretador ATOM completo (126 opcodes, operandos com alinhamento, IIO, CALL_TABLE com limite de profundidade, laço preso por orçamento de instruções, toda leitura conferida contra a imagem, scratch de 20 KiB); tabelas atomfirmware v2.1: `dce_info` (refclk 48 MHz, I2C 24 MHz, dispclk 600 MHz), `display_object_info` v1.4 e `gpio_pin_lut` (conector, UNIPHY e link, linha DDC e HPD de cada saída) | ✅ escrito e testado no host | `tools/amdgpu_atom_host.c` 27/27 e `tools/amdgpu_vbios_host.c` (sintético 13/13; com `MOS_VFCT=` a VFCT real do Picasso do Thiago 21/21: DP no DDC1/HPD1, HDMI no DDC2/HPD2 em UNIPHY0 link B, DP no DDC3/4; as 66 tabelas de comando reais rodam no interpretador contra registradores falsos sem erro de decodificação). A VFCT real não entra no repositório (binário da AMD) |
| F3 | SMU 10 (MP1): caixa de mensagens (C2PMSG 66/82/90), GetSmuVersion, PowerUpSdma (`engine.c`) | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c`. PowerUpGfx e DisableGfxOff entram com a F8a. Falta: AllowGfxOff (GFXOFF de verdade, com o RLC), relógios mínimos de display (com a F4b) |
| F4a | Display sem trocar o modo: leitura do timing de cada OTG e do plano de cada HUBP deixados pela UEFI, conferência com o framebuffer do GOP, vsync pelo OTG (vblank, posição, contador, taxa medida), troca de superfície no vblank (BGRA/RGBA linear, trava do OTG, confirmação por EARLIEST_INUSE), buffer duplo da tela do compositor na VRAM (WC), HPD, alocador de VRAM | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c`: 190/190 (timing de 25+ modos ida e volta, 40 quadros com áreas sujas aleatórias iguais à superfície, travamento da troca volta ao GOP) |
| F4b | Troca de modo e outras saídas: DDC/I2C e AUX por hardware (EDID ao vivo), DCCG/PLL, DIG + PHY por ATOM, HDMI (AVI, SCDC), DP (treino de link), DPP/MPC/OPP, watermarks, várias saídas | ⬜ | — |
| F5 | GMC da MMHUB 1.0: abertura do sistema na VRAM, AGP desligado, TLB L1, cache L2, contexto 0 com GART plana de 256 MiB (PTE sistema/snoop/UC), invalidação pelo motor 17 com semáforo; alocador de VRAM e BOs da ABI na VRAM (`engine.c`, `amdgpu.c`) | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c` (o SDMA simulado traduz pela tabela que o driver montou; página desligada ou só de leitura dá falta). A GFXHUB entrou com a F8a. Falta: VMIDs 1..15 com tabelas de 4 níveis por processo (com a F9) |
| F6 | IH (OSSSYS 4.x): anel de 64 KiB na RAM com endereço de barramento e snoop, writeback do wptr, controle do NBIO, MSI no vetor 0xE0 (polling se a tabela de tratadores estiver cheia), estouro do anel, trap do SDMA (fonte 224) depois de cada fence, hot-plug HPD1..6 com polaridade invertida a cada mudança (`irq.c`) | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c` 233/233: um trap por fence em 6008 fences, plugar e desplugar, estouro com 2500 entradas num anel de 2048, fonte desconhecida. Falta: vblank/pageflip por IRQ (o vsync segue por leitura do OTG) (o EOP do GFX entrou com a F8a) |
| F7 | SDMA 4.1: golden da família 4.1 e da variante, microcódigo direto, anel GFX de 64 KiB na VRAM com writeback do rptr, WPTR por registrador, teste do anel, COPY_LINEAR (até 4 MiB por pacote), cópia de retângulo, CONST_FILL, fences por polling (`engine.c`) | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c`: 221/221 (microcódigo real conferido palavra por palavra, 9 MiB VRAM→VRAM, RAM↔VRAM pela GART, retângulo 1000x300, 6000 pacotes dando 5 voltas no anel, SMU mudo e SDMA morto). Falta: interrupção de fence (F6) e power gating do SDMA |
| F8a | GFX9 gráfico (`gfx.c`): PowerUpGfx e DisableGfxOff no SMU; GFXHUB 1.0 com a mesma GART (`engine.c`, as duas hubs pelo mesmo código); golden gc_9_1 + rv1/rv2 + comum; constantes (READ_TIMEOUT, SH_MEM de cada VMID sem retry, GDS só no VMID 0, máscara de CUs); RLC (parada com espera do SERDES, CSB na VRAM, lista de save/restore v2.1 do Raven2, SRM, microcódigo, LBPW com 4 CUs sempre ligados, SPM); CP PFP/CE/ME (microcódigo, anel 0 de 64 KiB sem doorbell, preâmbulo de clear state, SET_BASE do CE, VGT_INDEX_TYPE); teste por SCRATCH_REG0 e por IB; fences RELEASE_MEM com EOP no IH (cliente 0x14, fonte 181); faltas do CP no IH; `amdgpu_gfx_submit/wait/done` | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c` 268/268, limpo com ASan/UBSan: CP simulado que confere o microcódigo real de RLC/PFP/CE/ME palavra por palavra e executa o PM4 pela GFXHUB; IB da GART escrevendo na RAM, 300 IBs com o anel dando a volta, escrita em página só de leitura vira falta, Raven2 com a lista de save/restore, CP morto e ME errado desligam só o GFX |
| F8b | Compute (`compute.c`): microcódigo do MEC (código na VRAM pelo IC_BASE, tabela de saltos por registrador); abertura de doorbells do NBIO e BAR2 mapeado; MQD v9 na VRAM; HQD do MEC1/pipe 0/fila 0 programada direto (tira do ar uma fila que a UEFI deixou ativa); doorbell 64 bits de índice 3 (faixa do MEC 0..0x8A); IBs com INDIRECT_BUFFER_VALID e fences RELEASE_MEM; EOP de compute no IH (ME 1); `amdgpu_compute_submit/wait/done`. Sem KIQ: uma fila só, do kernel | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c`: MEC simulado que só anda pelo doorbell certo, confere a tabela de saltos e o código no IC_BASE, exige a seleção ME1 nos registradores da HQD; 300 IBs com o anel dando a volta; MEC morto desliga só o compute e devolve a VRAM |
| F9a | VM por processo (`vm.c`): contextos 1..15 nas duas hubs (4 níveis, 48 bits, faltas para a página de rascunho, sem retry); um VMID fixo por espaço; tabelas na VRAM em blocos de 64 KiB; PTE para VRAM (físico do carve-out) e RAM (SYSTEM + SNOOPED); invalidação por VMID; IB do processo no anel GFX e na fila de compute (`*_submit_vm`). Correção: a base da tabela do contexto 0 (GART) passa a ser o endereço físico com o bit de válido, como no amdgpu; L2_CNTL3 sem translate_further | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c`: o CP simulado percorre as tabelas do VMID; IB do processo escreve na VRAM e na RAM, página desmapeada e IB só de leitura viram falta, 16º VMID recusado, destruição devolve a VRAM. Tabelas vazias só voltam na destruição da VM |
| F9b-1 | Validador de PM4 do userland (`pm4check.c`, lógica pura): só pacotes tipo 3; opcodes de desenho, dispatch, índice, SET_CONTEXT/SH_REG nas janelas e SET_UCONFIG_REG a partir de 0x201; WRITE_DATA/COPY_DATA só para memória dentro dos BOs do lote; WAIT_REG_MEM só esperando memória; RELEASE_MEM sem interrupção; EVENT_WRITE com endereço checado; INDIRECT_BUFFER só como IB2 sem VMID (reportado para validar); SET_CONFIG_REG, filas, DMA_DATA e SET_BASE do CE recusados | ✅ escrito e testado no host | `tools/amdgpu_pm4_host.c` 33/33, limpo com ASan/UBSan |
| E1 | Repouso (`engine.c` + `pm.c`, registro no `dev/pm.c` da Larissa): s2idle tira o sincronismo da saída (OTG parado, o monitor dorme) e devolve; S3 espera os motores esvaziarem, para CP/MEC/SDMA/IH e na volta reprograma SMU, as duas hubs (GART e VMs dos processos), IH, SDMA, GFX e compute reaproveitando anéis, tabelas e BOs (a VRAM da APU sobrevive). Registrado sem `PM_RESTORES_DISPLAY`: depois do S3 a saída só volta com a troca de modo (F4b), então o pm.c fica no s2idle até lá | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c`: chip simulado apagado entre suspend e resume; o processo continua da mesma fence |
| F4b-1 | DDC por hardware (`ddc.c`): motor de I2C do DCN com arbitragem com o DMCU, 100 kHz, até 4 transações; EDID ao vivo (blocos de 128 com segmento E-DDC, nova tentativa, extensões ruins ficam de fora); DDC/CI (MCCS) get/set de VCP; varredura das 6 linhas. ABI: `MOS_GPU_DISPLAY_INFO` e `MOS_GPU_DISPLAY_BRIGHTNESS`. libSystem: `CoreDisplay_DisplayCreateInfoDictionary`, `DisplayServicesGet/SetBrightness`, `DisplayServicesCanChangeBrightness` e `CGDisplayVendor/Model/SerialNumber` pelo EDID | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c`: monitor simulado na linha do HDMI (EDID de 3 blocos, DDC/CI), linha vazia, motor com o firmware, soma errada; libSystem compila com as flags do build |
| F4b-2 | AUX do DisplayPort (`aux.c`): motor DP_AUX do DCN com arbitragem com o DMCU, transações nativas (DPCD, até 16 bytes) e I2C sobre AUX, novas tentativas em DEFER; EDID de monitores DP pelo AUX (segmento E-DDC). Com o VBIOS, a varredura usa AUX nas saídas DP e I2C nas HDMI/DVI | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c`: sink DP simulado (DPCD 1.2, EDID de 3 blocos), DEFER com e sem fim, NACK, linha sem nada, motor com o DMCU |
| F9b | ABI revisão 3 no kernel (`abi.c` + `gpu_abi.c`, `mos_gpu.h` com `MOS_GPU_ABI` 3): `MOS_GPU_CAP_AMD`, INFO com família/CUs/memória, contexto `MOS_GPU_CTX_AMD`, BO com `gpu_va` (VRAM ou RAM, `MOS_GPU_BO_EXEC`), lote AMD (até 16 IBs, 256 BOs) copiado para um pool do kernel na VM do processo, validado (com IB2) e enviado pelo anel GFX ou compute; fence pelo EOP; BO ocupado até o lote passar; falha de página da GPU marca o contexto como perdido (-MOS_EIO); saída e exec do processo devolvem tudo (fecha a lacuna de memória presa) | ✅ escrito e testado no host | `tools/amdgpu_hw_host.c` (IB1 + IB2 pela VM do processo, cópia contra troca depois do envio, 3000 lotes com o pool dando a volta, recusas, falha de página, saída) e `tools/gpu_abi_test.c`; o gerador de PM4 e o compilador GFX9 são do Otavio |
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
