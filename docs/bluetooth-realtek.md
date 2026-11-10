# Bluetooth Realtek (RTL8821C) e teclado/mouse BLE

Alvo: o Bluetooth USB `0bda:c821` do combo RTL8821CE, com teclado e mouse BLE (HOGP) ao mesmo tempo.

## Firmware

`kernel/bt/btrtl.c` (chamado por `kernel/bt/vendor.c` antes do HCI Reset):

1. `HCI Read Local Version` (0x1001): fabricante 93 e `lmp_subver 0x8821` / `hci_rev 0x000c` / `hci_ver 8`
   = RTL8821C ainda na ROM. Outro `lmp_subver` = patch já carregado; não faz nada.
2. Versão da ROM pelo comando de fabricante 0xFC6D.
3. `rtl_bt/rtl8821c_fw.bin` (epatch v1, "Realtech"): confere a assinatura de extensão `51 04 fd 77` e o
   ID do projeto (10 = 8821C), escolhe o patch com `chip_id == ROM + 1`, troca os 4 últimos bytes pelo
   `fw_version` e cola o `rtl_bt/rtl8821c_config.bin` (assinatura `0x8723ab55`).
4. Envia por 0xFC20 em blocos de 252 bytes; o índice volta para 1 depois de 0x7F e o último leva o bit 7.

Os arquivos vêm do linux-firmware (pacote `linux-firmware-realtek`), descompactados em `firmware/rtl_bt/`
com a licença ao lado, como o firmware Intel. `tools/build.sh` agora copia `firmware/` para
`/mOS/Sistema/Firmware` na imagem (antes só o `build.ps1` fazia isso; o firmware Intel também não
entrava na imagem do build Linux).

## BLE (`kernel/bt/ble.c`)

- Até 4 enlaces LE ao mesmo tempo (antes era um só: teclado **ou** mouse).
- Reconexão: os vinculados que não estão conectados vão para a lista de aceitação do controlador e uma
  única conexão pendente (filtro 1) pega o primeiro que anunciar. Um mouse dormindo não segura o teclado.
  Uma conexão pedida pela UI cancela essa pendência (0x200E) e entra no lugar.
- Pareamento: IO "só tela" sem exigir MITM. Mouse (sem E/S) = Just Works. Teclado que exige MITM =
  passkey: o mOS mostra "Digite NNNNNN no teclado … e tecle Enter" (notificação e Ajustes > Bluetooth).
  Funciona em LE Secure Connections (20 rodadas) e no legado (TK = passkey).
- Falha no pareamento derruba o enlace. O pareamento automático tenta até 3 vezes por aparelho.
- Os nonces, a chave P-256 e a passkey vêm do `crypto_random` (DRBG ChaCha20 do kernel); antes era
  `rand32() ^ rdtsc()`.
- O vínculo guarda o nome do aparelho (os arquivos antigos de 33 bytes continuam válidos).
- Primeiro uso: 60 s procurando mouse/teclado em modo de pareamento (30 s se já houver vínculo).

## Testes de host

```
gcc -O2 -Wall -Wextra -Wno-incompatible-library-redeclaration -I kernel/include tools/btrtl_host.c \
    kernel/bt/btrtl.c kernel/bt/vendor.c kernel/bt/btintel.c -o /tmp/btrtl_host && /tmp/btrtl_host firmware
gcc -O1 -Wall -Wextra -Wno-unused-parameter -Wno-incompatible-library-redeclaration -I kernel/include \
    tools/ble_host.c kernel/bt/ble.c kernel/bt/smp_crypto.c kernel/bt/p256.c kernel/bt/hid_parse.c \
    kernel/wifi/wpa.c -o /tmp/ble_host && /tmp/ble_host
```

## Ao usar no PC

- Teclado e mouse estão pareados com o Linux deste PC, que usa o **mesmo** adaptador (mesmo BD_ADDR).
  O mOS não lê as chaves do Linux; no primeiro boot é preciso pôr os dois em modo de pareamento. O novo
  vínculo substitui o do Linux no aparelho; ao voltar ao Linux, pareie de novo (ou use outro canal
  Easy-Switch do MX Master para cada sistema).
- Endereço privado resolvível (RPA) não é resolvido: o mOS não pede a IRK. Se um dos aparelhos usar RPA,
  a reconexão automática falha e é preciso parear de novo.

## Só o hardware prova

- Que o RTL8821C aceita o patch (0xFC20) e volta com o `lmp_subver` novo depois do HCI Reset.
- O comportamento real do teclado "Slim X1" (se exige MITM, se aceita passkey, se usa RPA) e do MX
  Master 3S.
- Coexistência com o Wi-Fi na mesma antena (o driver Wi-Fi ainda não existe).
- A notificação com a passkey aparecer antes de haver teclado/mouse (só um mouse USB ou a tela ajudam
  a ler; a passkey é digitada no próprio teclado BT, então não depende de outro teclado).
