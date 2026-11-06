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
typedef struct wifi_radio {
    const char *name;
    u8   mac[6];
    void *priv;
    bool (*set_channel)(struct wifi_radio *r, int channel);
    bool (*tx)(struct wifi_radio *r, const u8 *frame, u16 len);              /* quadro 802.11 completo, sem FCS */
    int  (*rx)(struct wifi_radio *r, u8 *frame, u16 max, int *rssi);         /* 0 = nada; sem FCS */
    void (*poll)(struct wifi_radio *r);                                      /* opcional */
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
