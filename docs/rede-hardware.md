# Rede no hardware do Thiago — plano e estado (frente do Felipe)

Alvo: o PC do Thiago, que é a mesma máquina deste time. Medido no Linux dele (só leitura, 15 nov.):

| Peça | ID | Situação no Linux | O que isso decide aqui |
|---|---|---|---|
| Wi-Fi RTL8821CE | PCIe `10ec:c821`, BAR2 MMIO | `rtw88_8821ce`, firmware **24.11**, RFE 0, antenas **não compartilhadas** (2 antenas, DPDT, WLG) | Rede de casa é **5 GHz canal 42 (36–48, VHT80)**: o driver precisa de 5 GHz, 80 MHz e VHT, não só 2,4 GHz/legado. O Linux registra `firmware failed to leave lps state` em laço: o LPS "deep" (PG) do 8821CE é instável; aqui o LPS usa só o modo sem PG |
| Bluetooth RTL8821C | USB `0bda:c821` | `btusb` + `rtl8821c_fw` | Sem enlace ativo na medição (coex: `BT status non-conn`) |
| Ethernet RTL8168H | PCIe `10ec:8168` rev 0x15 | `r8169`, sem cabo (`NO-CARRIER`) | Família 8168H (XID 0x541) |

Referência de comportamento: `drivers/net/wireless/realtek/rtw88` do Linux, que a Realtek publica sob
**GPL-2.0 OR BSD-3-Clause**. O mOS usa a opção BSD-3-Clause: as tabelas de BB/RF/AGC/potência e as
sequências de energia do 8821C entram com o aviso de copyright da Realtek no topo do arquivo. A lógica é
reescrita para o mOS (polling, sem mac80211). O firmware `rtw88/rtw8821c_fw.bin` vem do linux-firmware
(licença redistribuível da Realtek ao lado, em `firmware/rtw88/`). Nada do r8169 do Linux (GPL-2.0 só)
é copiado: só endereços e ordem de passos.

## Arquitetura do Wi-Fi

```
kernel/wifi/ieee80211.c   pilha de estação softMAC (scan, auth, assoc, WPA2/WPA3, CCMP, PMF, roaming, PS)
kernel/wifi/rtw88/        driver RTL8821CE que implementa wifi_radio_t
  rtw_reg.h               registradores e bits
  rtw_pci.c               anéis de buffer descriptor TX (BE/MGMT/H2C/BCN) e RX (MPDU), polling
  rtw_mac.c               sequência de energia, eFuse, download do firmware (DDMA), init do MAC, páginas/RQPN
  rtw_phy.c               interpretador das tabelas (condições RFE/pacote), RF por SIPI, troca de canal, TX power
  rtw_fw.c                H2C/C2H: RA (rate adaptation do firmware), media status, LPS, IQK, coex
  rtw_coex.c              coexistência com o BT do combo: PTA, chave de antena, tabela/TDMA
  rtw8821c.c              específico do chip e registro do rádio
  rtw8821c_table.c        tabelas da Realtek (BSD-3-Clause)
```

## O que falta (levantamento de 15 nov.)

### Wi-Fi RTL8821CE — não existia driver (só identificação da placa)
1. [x] Firmware no repositório (`firmware/rtw88/rtw8821c_fw.bin` + licença) e parser do cabeçalho.
2. [x] Energia: sequências carddis→cardemu→act e o caminho inverso; eFuse físico → mapa lógico (MAC, RFE, cristal, potência).
3. [x] Download do firmware (DDMA, `MCUFWDL`, checksum, `WINTINI_RDY`).
4. [x] Init do MAC: TRX, FIFO/páginas, RQPN, LLT, filtros RX, EDCA, `rtw8821c_mac_init`.
5. [x] PCIe: anéis com buffer descriptor (TX 16 B, RX 8 B), descritores TX 48 B / RX 24 B, H2C por anel.
6. [x] PHY: carga das tabelas MAC/BB/AGC/RF com condições; cristal; `set_channel` 2,4/5 GHz, 20/40/80 MHz; potência por taxa.
7. [x] Firmware: H2C (media status, RA, general info, phydm info, IQK), C2H (relatório de TX, RA), LPS sem PG.
8. [x] Coex com o BT: init do PTA, chave de antena por RFE, tabela e TDMA por estado do BT.
9. [x] Testes de host: tabelas contra o estado do chip no Linux, energia contra um chip simulado, TX/RX/H2C.

### Pilha 802.11 (`kernel/wifi/ieee80211.c`)
10. [ ] HT/VHT na associação (IEs de capacidade), A-MPDU RX (ADDBA, reordenação), QoS data.
11. [~] WPA3-SAE (criptografia pronta: `kernel/wifi/sae.c`, teste `tools/sae_host.c` 16/16 com os vetores J.10; falta ligar na pilha de estação) (hunting-and-pecking e H2E sobre P-256), PMF/802.11w (BIP-CMAC, SA query) — obrigatório no WPA3.
12. [ ] Roaming: varredura em segundo plano por RSSI, reassociação no mesmo ESS, PMKSA cache.
13. [ ] Economia de energia: bit PM, null data, LPS do firmware, DTIM.
14. [ ] Varredura de 5 GHz (canais 36–165, passivo nos DFS) e regulatório.

### Bluetooth (`kernel/bt/`, `kernel/dev/bluetooth.c`)
15. [ ] Revisar o caminho de firmware rtl8821c no USB real (já existe `btrtl.c` com teste de host).
16. [ ] RPA: pedir e guardar a IRK no pareamento LE e resolver endereços privados.
17. [ ] A2DP: conferir SBC/AVDTP com o teste de host; AVRCP básico (volume/play/pause).
18. [ ] Coex com o Wi-Fi do lado BT (o firmware BT recebe o estado do Wi-Fi pelo PTA/mailbox).

### Ethernet RTL8168H (`kernel/dev/r8169.c`)
19. [x] Firmware de PHY `rtl_nic/rtl8168h-2.fw` (no repositório com a licença; interpretador próprio das 211 ações; recusa soma errada e saltos fora da lista).
20. [x] Afinação do PHY do 8168H, tabela EPHY e `rtl_hw_start_8168h_1` inteiro; link acompanhado (velocidade/duplex e queda no log). Fora: latência de entrada no ASPM (config PCI 0x70F; ASPM desligado). Teste `tools/r8169_host.c`: 61/61.

## Pronto, com teste

| Peça | Arquivo | Teste de host | Resultado |
|---|---|---|---|
| Firmware 24.11 no repositório + validação do cabeçalho (DMEM/IMEM/EMEM) | `firmware/rtw88/`, `kernel/wifi/rtw88/rtw_core.c` | `tools/rtw88_host.c` | DMEM 0x200000+31984, IMEM 0x30000+63248, EMEM 44176; truncado é recusado |
| Tabelas MAC/BB/AGC/RF/potência/limites (5970 valores, conversão mecânica) | `kernel/wifi/rtw88/rtw8821c_table.c` | idem | MAC 138, BB 1669, AGC 386, RF 586 pares para RFE 0/PCIe |
| Interpretador das tabelas condicionais | `rtw_core.c` | idem, mais o despejo real em `tools/rtw88-fixture/` | **421 de 485** registradores de BB escritos pela tabela iguais aos do chip deste PC com o Linux associado (os outros são canal, DIG e IQK, que mudam depois) |
| Sequências de energia ligar/desligar (PCIe) | `rtw_core.c` | chip simulado | ligar completa; poll preso vira erro |
| eFuse físico → lógico e campos do 8821CE | `rtw_core.c` | mapa sintético | cabeçalhos de 1 e 2 bytes, palavras ausentes, MAC e RFE |

Comando (também roda no `tools/build.sh`):
```
gcc -O1 -Wall -Wextra -DMOS_HOST_TEST -I tools -I kernel/include -I kernel/wifi/rtw88 tools/rtw88_host.c \
    kernel/wifi/rtw88/rtw_core.c kernel/wifi/rtw88/rtw8821c_table.c -o /tmp/rtw88_host && \
    /tmp/rtw88_host firmware/rtw88/rtw8821c_fw.bin tools/rtw88-fixture/bb_8821ce_linux.txt
```

### Driver (`kernel/wifi/rtw88/rtw8821c.c`, `rtw_pwr.c`)

`wifi_rtw_probe` acha o 10ec:c821, mapeia o BAR2, desliga o timeout de conclusão do PCIe e sobe o chip:
energia → firmware (só para ler o eFuse) → eFuse → desliga → energia → firmware → filas/LLT/fila de H2C →
MAC → tabelas de BB/AGC/RF → H2C de informação geral e de PHY → coex. Registra o rádio `Realtek RTL8821CE`
na pilha, que passa a usá-lo no lugar do simulado.

- Canal: 2,4 e 5 GHz em 20/40/80 MHz (`set_chan(primário, central, largura)`), com a chave de RF, os filtros
  de CCK do canal 14, o subcanal primário e a potência por taxa recalculada (regulador FCC, o do Brasil).
- TX: filas de gestão, BE, alta (broadcast) e H2C; quadros de gestão a 1/6 Mb/s fixos; dados unicast com a taxa
  escolhida pelo firmware (RA) a partir da máscara da associação (VHT MCS0-9, HT MCS0-7, legado).
- RX: anel de 64 buffers de 11,4 KiB, etiqueta de RX conferida, FCS tirado, RSSI da PHY (OFDM e CCK), C2H do
  firmware (relatório de RA) separado.
- Associação: BSSID/AID/tipo de rede, H2C de mídia e de RA, filtro de dados da própria BSS, canal para o AFH
  do BT, IQK pelo firmware.
- Coex (RFE 0 deste PC, antenas separadas): 5 GHz e 2,4 GHz em "free run"; antena compartilhada (outras
  placas) com a tabela do PTA.
- LPS do firmware sem PG (`powersave`). Se o firmware não sair do LPS (o mesmo `failed to leave lps state` do
  Linux deste PC), o driver limpa o TCR e desliga o LPS na sessão.
- Vigia a cada 2 s: ganho inicial (IGI) pelo RSSI e RSSI para o firmware.

Teste do driver inteiro contra um chip simulado (`tools/rtw88_chip_host.c`, também no `tools/build.sh`):
**32/32** — o firmware chega byte a byte à DMEM/IMEM/EMEM pela fila de beacon + DDMA; MAC e RFE do eFuse;
CR/RCR; H2C pelo anel; canal 42/80 MHz no RF, BB e MAC; potência gravada igual à do Linux neste PC
(OFDM 42/41/39/37, MCS 31, VHT 28); probe request pela fila de gestão com checksum certo; beacon de RX com
RSSI; C2H consumido; associação com RA ARFR1 AC 1SS/80 MHz/SGI; LPS entra, sai e o caso travado.

Só o hardware prova: que o firmware sobe (`rtw88: pronto` no log), a varredura em 5 GHz e a associação.
A pilha ainda chama só `set_channel` (20 MHz) e não chama `bss`/`powersave`: isso entra com o item 10.

DMA de 32 bits: o RTL8821CE só endereça abaixo de 4 GiB e o PC tem 16 GB (2 GB reservados para a GPU,
14 GB visíveis ao sistema). O driver usa `dma_alloc32` (Gustavo, PR #249), alocado na subida do driver.
