/*
 * mOS — pilha USB (núcleo + xHCI + drivers de classe)
 *
 *   xhci.c        controlador xHCI (USB 1.1/2.0/3.x): anéis, contextos, transferências por polling
 *   usb.c         núcleo: enumeração, tabela de dispositivos, despacho para drivers de classe
 *   usb_hid.c     teclado (boot protocol), mouse e tablet/apontadores absolutos (report protocol)
 *   usb_hub.c     hubs USB 2.0 (um ou mais níveis)
 *   usb_msc.c     armazenamento em massa Bulk-Only (SCSI)
 *   bluetooth.c   Bluetooth HCI sobre USB
 *
 * Tudo funciona sem IRQ: usb_poll() (chamada no laço principal) consome o anel de eventos.
 * Transferências de controle/bulk são síncronas com timeout; interrupt IN é assíncrono
 * (callback chamado de dentro do processamento de eventos — o callback NÃO pode fazer
 * transferências síncronas; marque uma flag e trate em um *_poll()).
 */
#pragma once
#include "kernel.h"

/* ------------------------------------------------------------------ descritores */
typedef struct PACKED {
    u8  bLength, bDescriptorType;
    u16 bcdUSB;
    u8  bDeviceClass, bDeviceSubClass, bDeviceProtocol, bMaxPacketSize0;
    u16 idVendor, idProduct, bcdDevice;
    u8  iManufacturer, iProduct, iSerialNumber, bNumConfigurations;
} usb_dev_desc_t;

typedef struct PACKED {
    u8  bLength, bDescriptorType;
    u16 wTotalLength;
    u8  bNumInterfaces, bConfigurationValue, iConfiguration, bmAttributes, bMaxPower;
} usb_cfg_desc_t;

typedef struct PACKED {
    u8 bLength, bDescriptorType, bInterfaceNumber, bAlternateSetting, bNumEndpoints;
    u8 bInterfaceClass, bInterfaceSubClass, bInterfaceProtocol, iInterface;
} usb_iface_desc_t;

typedef struct PACKED {
    u8  bLength, bDescriptorType, bEndpointAddress, bmAttributes;
    u16 wMaxPacketSize;
    u8  bInterval;
} usb_ep_desc_t;

enum { USB_DT_DEVICE = 1, USB_DT_CONFIG = 2, USB_DT_STRING = 3, USB_DT_INTERFACE = 4, USB_DT_ENDPOINT = 5,
       USB_DT_HID = 0x21, USB_DT_REPORT = 0x22, USB_DT_HUB = 0x29, USB_DT_SS_EP_COMP = 0x30 };
enum { USB_EP_CONTROL = 0, USB_EP_ISOC = 1, USB_EP_BULK = 2, USB_EP_INTR = 3 };

/* velocidades (numeração do xHCI PORTSC) */
enum { USB_SPEED_FULL = 1, USB_SPEED_LOW = 2, USB_SPEED_HIGH = 3, USB_SPEED_SUPER = 4, USB_SPEED_SUPERPLUS = 5 };

/* tipos de dispositivo reconhecidos (para a lista nas Preferências) */
enum { USB_KIND_UNKNOWN, USB_KIND_KEYBOARD, USB_KIND_MOUSE, USB_KIND_TABLET, USB_KIND_HID,
       USB_KIND_HUB, USB_KIND_STORAGE, USB_KIND_BLUETOOTH, USB_KIND_COMBO };

/* ------------------------------------------------------------------ dispositivos */
#define USB_MAX_DEVS   32
#define USB_MAX_IFACES 8
#define USB_MAX_EPS    6

typedef struct {
    u8  num, cls, sub, proto;
    u8  nep;
    usb_ep_desc_t ep[USB_MAX_EPS];
    u8  ss_burst[USB_MAX_EPS];
    const u8 *extra;     /* descritores de classe entre o descritor de interface e os endpoints */
    u16 extra_len;
    int driver;          /* índice do driver que reivindicou (-1 = nenhum) */
    void *priv;          /* estado do driver */
} usb_iface_t;

typedef struct usb_dev {
    bool  used, configured;
    int   id;
    void *hc;            /* controlador (xhci_t *) */
    void *hcpriv;        /* estado do controlador para este dispositivo */
    u8    speed, root_port, depth, port;   /* port = porta no hub pai (ou porta raiz) */
    u32   route;
    struct usb_dev *parent;
    u8    tt_slot, tt_port;
    u16   mps0;
    usb_dev_desc_t desc;
    u8   *config;
    u16   config_len;
    u8    nifaces;
    usb_iface_t iface[USB_MAX_IFACES];
    char  manufacturer[48], product[64];
    int   kind;
    char  path[16];      /* "2" ou "2.3" (porta raiz . porta do hub) */
    char  desc_str[160];
} usb_dev_t;

/* ------------------------------------------------------------------ API do núcleo (usb.c) */
typedef void (*usb_xfer_cb)(usb_dev_t *d, u8 ep, u8 *data, u32 len, int status, void *ctx);

enum { USB_OK = 0, USB_ERR = -1, USB_STALL = -2, USB_TIMEOUT = -3, USB_NODEV = -4 };

int  usb_control(usb_dev_t *d, u8 rt, u8 req, u16 val, u16 idx, u16 len, void *data);     /* bytes ou <0 */
int  usb_bulk(usb_dev_t *d, u8 ep_addr, void *buf, u32 len, u32 timeout_ms);             /* bytes ou <0 */
int  usb_intr_start(usb_dev_t *d, u8 ep_addr, u16 len, usb_xfer_cb cb, void *ctx);         /* re-enfileira sozinho */
int  usb_clear_halt(usb_dev_t *d, u8 ep_addr);
int  usb_get_descriptor(usb_dev_t *d, u8 type, u8 index, u16 lang, void *buf, u16 len);
void usb_events(void);     /* processa eventos pendentes de todos os controladores (sem enumeração) */
usb_dev_t *usb_attach(void *hc, usb_dev_t *parent, u8 port, u8 speed);                     /* enumera */
void usb_detach(usb_dev_t *d);                                                            /* recursivo */
usb_dev_t *usb_dev_get(int i);
int  usb_device_count(void);
const char *usb_device_desc(int i);
void usb_poll(void);
void usb_set_kind(usb_dev_t *d, int kind);

/* drivers de classe: probe reivindica a interface (pode registrar endpoints com usb_add_ep);
 * start roda após SET_CONFIGURATION; detach na remoção; poll no laço principal */
typedef struct {
    const char *name;
    bool (*probe)(usb_dev_t *d, usb_iface_t *it);
    void (*start)(usb_dev_t *d, usb_iface_t *it);
    void (*detach)(usb_dev_t *d, usb_iface_t *it);
    void (*poll)(void);
} usb_driver_t;
int  usb_add_ep(usb_dev_t *d, usb_iface_t *it, int idx);     /* registra it->ep[idx] no controlador */
usb_ep_desc_t *usb_find_ep(usb_iface_t *it, u8 type, bool in, int *idx);

/* hubs: atualiza o contexto do slot com dados do hub */
int  usb_hub_configure(usb_dev_t *d, int nports, bool mtt, int ttt);

/* ------------------------------------------------------------------ controlador (xhci.c) */
int  xhci_init_all(void);
int  xhci_count(void);
int  xhci_total_ports(void);
void xhci_poll(void);                  /* eventos + mudanças de porta raiz (pode enumerar) */
void xhci_process_events(void);        /* só eventos (seguro dentro de esperas) */
int  xhci_address_device(usb_dev_t *d);
int  xhci_update_ep0(usb_dev_t *d);
int  xhci_control(usb_dev_t *d, u8 rt, u8 req, u16 val, u16 idx, u16 len, void *data, u32 timeout_ms);
int  xhci_add_endpoint(usb_dev_t *d, const usb_ep_desc_t *e, u8 burst);
int  xhci_commit_endpoints(usb_dev_t *d);
int  xhci_set_hub(usb_dev_t *d, int nports, bool mtt, int ttt);
int  xhci_bulk(usb_dev_t *d, u8 ep, void *buf, u32 len, u32 timeout_ms);
int  xhci_intr_start(usb_dev_t *d, u8 ep, u16 len, usb_xfer_cb cb, void *ctx);
int  xhci_reset_endpoint(usb_dev_t *d, u8 ep);
void xhci_free_device(usb_dev_t *d);
int  xhci_slot_of(usb_dev_t *d);
const char *xhci_name(void *hc);

/* ------------------------------------------------------------------ drivers */
extern const usb_driver_t usb_hid_driver, usb_hub_driver, usb_msc_driver, usb_bt_driver, usb_xinput_driver;
/* controles de jogo (dev/gamepad.c): registro para o GameController.framework */
int  gamepad_attach(const char *name, u16 vid, u16 pid, u32 kind, u32 caps, void (*rumble)(void *ctx, u8 strong, u8 weak), void *ctx);
void gamepad_detach(int id);
void gamepad_report(int id, u32 buttons, float lx, float ly, float rx, float ry, float lt, float rt);
u32  gamepad_hat_buttons(int hat);
void usb_hub_child_gone(usb_dev_t *hub, usb_dev_t *child);

/* ------------------------------------------------------------------ armazenamento em massa
 * Dispositivos de bloco USB (Bulk-Only Transport + SCSI transparente).
 *   usb_msc_count()                     número de unidades prontas (índices 0..n-1; mudam com hot-plug)
 *   usb_msc_read(dev, lba, count, buf)  lê `count` blocos a partir de `lba` em `buf`
 *                                       (qualquer memória do kernel; usa buffer DMA interno).
 *                                       Retorna 0 em sucesso, <0 em erro (USB_* acima).
 *   usb_msc_write(...)                  idem para escrita.
 *   usb_msc_block_size / usb_msc_blocks tamanho do bloco (normalmente 512) e total de blocos.
 *   usb_msc_name                        "Fabricante Produto" do INQUIRY.
 *   usb_msc_disk(i)                     blkdev_t (fs.h) da unidade i, por deslocamento em bytes:
 *        read/write aceitam qualquer deslocamento e tamanho (setores parciais: lê o setor
 *        inteiro e copia o trecho / ler-modificar-gravar); flush = SYNCHRONIZE CACHE(10).
 *        write == NULL se a mídia estiver protegida contra escrita (MODE SENSE).  O ponteiro
 *        continua válido após a remoção da unidade (as operações passam a falhar); release == NULL.
 * As funções são síncronas (bloqueiam até ~10 s por comando no pior caso) e não podem ser
 * chamadas de dentro de callbacks USB. */
#include "fs.h"
int  usb_msc_count(void);
blkdev_t *usb_msc_disk(int i);
int  usb_msc_read(int dev, u64 lba, u32 count, void *buf);
int  usb_msc_write(int dev, u64 lba, u32 count, const void *buf);
u32  usb_msc_block_size(int dev);
u64  usb_msc_blocks(int dev);
const char *usb_msc_name(int dev);

/* ------------------------------------------------------------------ Bluetooth */
void bt_poll(void);
