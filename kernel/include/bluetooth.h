/* mOS — Bluetooth (HCI sobre USB): API de baixo nível para a pilha superior (kernel/bt/) */
#pragma once
#include "kernel.h"

int  bt_hci_cmd(u16 opcode, const void *p, u8 len, int timeout_ms); /* status HCI (0 = ok) ou <0 (sem resposta/sem adaptador) */
bool bt_acl_send(u16 handle, const void *data, u16 len);           /* cabeçalho ACL incluído; len <= 1021 */
extern void (*bt_on_event)(const u8 *ev, u32 len);                 /* todo evento HCI (código, tamanho, parâmetros) */
extern void (*bt_on_acl)(const u8 *acl, u32 len);                  /* pacote ACL completo, com o cabeçalho de 4 bytes */
void bt_poll(void);

/* configuração do fabricante / firmware */
int  bt_hci_cmd_resp(u16 opcode, const void *p, u8 len, u8 *resp, int max, int timeout_ms); /* bytes do Command Complete (status incluso) ou <0 */
bool bt_hci_cmd_async(u16 opcode, const void *p, u8 len);          /* envia sem esperar resposta */
int  bt_wait_event(u8 *buf, int max, int timeout_ms);              /* só dentro de bt_vendor_setup; <0 em timeout */
bool bt_vendor_setup(u16 vid, u16 pid);                            /* kernel/bt/vendor.c. Roda após o VFS, antes do HCI_Reset; false = adaptador fica desligado */
bool btintel_setup(u16 vid, u16 pid);                              /* kernel/bt/btintel.c: true se não for Intel */
bool btrtl_setup(u16 vid, u16 pid);                                /* kernel/bt/btrtl.c: true se não for Realtek */