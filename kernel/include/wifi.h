/*
 * mOS — Wi-Fi: pilha 802.11 de estação (kernel/wifi/), WPA2-PSK e drivers de rádio.
 *
 * Camadas:
 *   driver (wifi_radio_t)  ->  ieee80211.c (scan, autenticação, associação, handshake WPA2, CCMP,
 *   802.11 <-> Ethernet)   ->  net.c (netdev_t "wlan0": DHCP, TCP/IP...)
 *
 * Uso pela interface (do laço do desktop):
 *   wifi_scan();  ...  n = wifi_networks(list, max);  wifi_connect("Casa", "senha");  wifi_status(&st);
 */
#pragma once
#ifdef MOS_HOST_TEST
#include "host_shim.h"
#else
#include "kernel.h"
#endif

/* ---------------- API pública */
enum { WIFI_SEC_OPEN, WIFI_SEC_WEP, WIFI_SEC_WPA2_PSK, WIFI_SEC_WPA3_SAE, WIFI_SEC_ENTERPRISE };
enum { WIFI_OFF, WIFI_IDLE, WIFI_SCANNING, WIFI_AUTHENTICATING, WIFI_ASSOCIATING, WIFI_HANDSHAKE,
       WIFI_CONNECTED, WIFI_FAILED };

typedef struct {
    char ssid[33];
    u8   bssid[6];
    int  rssi;              /* dBm (ex.: -45 forte, -85 fraco) */
    int  channel;
    int  security;          /* WIFI_SEC_* */
    bool supported;         /* o mOS sabe se conectar a esta rede (aberta ou WPA2-PSK) */
} wifi_network_t;

typedef struct {
    int  state;             /* WIFI_* */
    char ssid[33];
    u8   bssid[6];
    int  rssi, channel;
    char error[96];         /* motivo da última falha, em português */
    char driver[48];        /* "Simulado", "Intel Wi-Fi 6 AX200", ... */
    bool present;           /* existe placa Wi-Fi */
    int  security;          /* WIFI_SEC_* da rede conectada */
} wifi_status_t;

bool wifi_present(void);
void wifi_init(void);                       /* procura placas (PCIe/USB) */
void wifi_poll(void);                       /* chamado no laço principal */
bool wifi_scan(void);                       /* inicia uma varredura (~2 s) */
int  wifi_networks(wifi_network_t *out, int max);   /* resultados, mais fortes primeiro */
bool wifi_connect(const char *ssid, const char *password);
void wifi_disconnect(void);
void wifi_status(wifi_status_t *st);
void wifi_set_enabled(bool on);

/* ---------------- interface dos drivers de rádio (softMAC: a pilha monta os quadros 802.11) */
/* o que a pilha sabe da rede associada; o driver usa para rate adaptation, filtros e economia de energia */
typedef struct {
    u8   bssid[6];
    u16  aid;
    int  primary, center, bw;     /* canal primário, central e largura (0 = 20, 1 = 40, 2 = 80 MHz) */
    bool ht, vht, sgi20, sgi40, sgi80, ldpc, stbc;
    u8   ht_mcs;                  /* máscara dos MCS 0-7 que o AP recebe */
    u16  vht_mcs_map;             /* mapa VHT de RX do AP (2 bits por fluxo) */
    u16  legacy;                  /* bits 0-3 CCK 1/2/5,5/11, 4-11 OFDM 6..54 */
    u16  beacon_int;
    u8   dtim_period;
    int  rssi;
} wifi_bss_t;

typedef struct wifi_radio {
    const char *name;
    u8   mac[6];
    void *priv;
    bool (*set_channel)(struct wifi_radio *r, int channel);
    bool (*tx)(struct wifi_radio *r, const u8 *frame, u16 len);              /* quadro 802.11 completo, sem FCS */
    int  (*rx)(struct wifi_radio *r, u8 *frame, u16 max, int *rssi);         /* 0 = nada; sem FCS */
    void (*poll)(struct wifi_radio *r);                                      /* opcional */
    /* opcionais de um rádio 802.11ac: canal com largura, estado da associação e economia de energia */
    bool (*set_chan)(struct wifi_radio *r, int primary, int center, int bw);
    void (*bss)(struct wifi_radio *r, const wifi_bss_t *b);                   /* NULL = desassociado */
    bool (*powersave)(struct wifi_radio *r, bool on);
    /* taxa de TX atual (código de taxa do descritor, SGI, largura 0/1/2) e ruído em dBm (0 = não mede) */
    void (*link_info)(struct wifi_radio *r, int *rate, int *sgi, int *bw, int *noise);
    /* firmware com MLME próprio (fullMAC, ex.: Intel): se presente, a pilha delega scan/conexão */
    bool (*fw_connect)(struct wifi_radio *r, const char *ssid, const u8 *bssid, int channel, const u8 pmk[32]);
} wifi_radio_t;

void wifi_register_radio(wifi_radio_t *r);

/* ---------------- WPA2 (kernel/wifi/wpa.c) */
typedef struct { u8 rk[176]; } wpa_aes_t;
void wpa_sha1(const void *data, usize len, u8 out[20]);
void wpa_hmac_sha1(const u8 *key, usize klen, const void *msg, usize mlen, u8 out[20]);
void wpa_psk(const char *pass, const u8 *ssid, usize ssid_len, u8 psk[32]);
void wpa_prf(const u8 *key, usize klen, const char *label, const u8 *data, usize dlen, u8 *out, usize len);
void wpa_ptk(const u8 pmk[32], const u8 aa[6], const u8 sa[6], const u8 anonce[32], const u8 snonce[32], u8 ptk[48]);
void wpa_eapol_mic(const u8 kck[16], const u8 *frame, usize len, u8 mic[16]);
void wpa_aes_setkey(wpa_aes_t *c, const u8 key[16]);
void wpa_aes_encrypt(const wpa_aes_t *c, const u8 in[16], u8 out[16]);
void wpa_aes_decrypt(const wpa_aes_t *c, const u8 in[16], u8 out[16]);
bool wpa_key_wrap(const u8 kek[16], const u8 *in, usize len, u8 *out);      /* out: len + 8 */
bool wpa_key_unwrap(const u8 kek[16], const u8 *in, usize len, u8 *out);    /* out: len - 8 */
void wpa_ccm_encrypt(const u8 key[16], const u8 nonce[13], const u8 *aad, usize alen, u8 *data, usize len, u8 mic[8]);
bool wpa_ccm_decrypt(const u8 key[16], const u8 nonce[13], const u8 *aad, usize alen, u8 *data, usize len, const u8 mic[8]);

/* ---------------- CCMP sobre quadros 802.11 (kernel/wifi/ieee80211.c) */
u16  ccmp_encrypt_frame(const u8 tk[16], u64 pn, int keyid, const u8 *in, u16 len, u8 *out);  /* devolve o novo tamanho */
int  ccmp_decrypt_frame(const u8 tk[16], const u8 *in, u16 len, u8 *out, u64 *pn);            /* -1 se falhar */

/* ---------------- WPA3-SAE (kernel/wifi/sae.c), grupo 19; escalares e pontos em big-endian */
#include "p256.h"
typedef struct {
    p256_fe pwe_x, pwe_y;
    u32  rand[8];
    u8   scalar[32], elem[64];           /* o nosso commit */
    u8   peer_scalar[32], peer_elem[64];
    u8   kck[32], pmk[32], pmkid[16];
    u16  send_confirm;
} sae_t;
bool sae_pwe_hnp(sae_t *s, const u8 own[6], const u8 peer[6], const char *pw);
bool sae_derive_pt(u8 pt[64], const u8 *ssid, usize ssid_len, const char *pw, const char *ident);
bool sae_pwe_h2e(sae_t *s, const u8 pt[64], const u8 own[6], const u8 peer[6]);
bool sae_commit(sae_t *s, const u8 *rand_be, const u8 *mask_be);   /* NULL = aleatórios */
bool sae_process_commit(sae_t *s, const u8 *peer_scalar, const u8 *peer_elem);
u16  sae_confirm(sae_t *s, u8 out[32]);
bool sae_check_confirm(const sae_t *s, u16 peer_sc, const u8 conf[32]);
void wpa_kdf_sha256(const u8 *key, usize klen, const char *label, const u8 *ctx, usize clen, u8 *out, usize bits);
void aes_cmac(const u8 key[16], const u8 *m, usize len, u8 out[16]);   /* kernel/bt/smp_crypto.c */
void wifi_radio_lost(void);                 /* driver: o enlace caiu por baixo (S3) */

int  wifi_rate_kbps(int rate, int sgi, int bw);      /* taxa do descritor (CCK/OFDM/HT/VHT 1 fluxo) em kb/s */
wifi_radio_t *wifi_radio(void);
