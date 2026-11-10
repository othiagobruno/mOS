/*
 * mOS — configuração do fabricante do adaptador Bluetooth USB, chamada por dev/bluetooth.c depois do VFS
 * e antes do HCI_Reset. Cada carregador devolve true quando o adaptador não é dele.
 */
#include "bluetooth.h"

bool bt_vendor_setup(u16 vid, u16 pid) {
    if (vid == 0x8087) return btintel_setup(vid, pid);   /* Intel: firmware assinado (ibt-*.sfi) */
    return btrtl_setup(vid, pid);                        /* Realtek (0bda e OEMs); confere o fabricante 93 */
}
