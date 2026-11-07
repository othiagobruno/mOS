/* temporário: implementações mínimas até os subsistemas reais entrarem (net/, dev/hda.c, dev/usb.c, apps/horizonte.c) */
#include "apps.h"
#include "net.h"

/* ---- áudio */
__attribute__((weak)) bool audio_init(void) { return false; }
__attribute__((weak)) bool audio_available(void) { return false; }
__attribute__((weak)) void audio_play(const i16 *s, u32 f) {}
static int vol = 70;
__attribute__((weak)) void audio_set_volume(int p) { vol = p; }
__attribute__((weak)) int audio_get_volume(void) { return vol; }
__attribute__((weak)) const char *audio_device_name(void) { return "Nenhum dispositivo"; }
__attribute__((weak)) void audio_poll(void) {}
__attribute__((weak)) bool audio_input_available(void) { return false; }
__attribute__((weak)) const char *audio_input_name(void) { return "Nenhuma entrada"; }
__attribute__((weak)) void audio_set_input_volume(int p) { (void)p; }
__attribute__((weak)) int audio_get_input_volume(void) { return 0; }
__attribute__((weak)) void audio_set_input_monitor(bool on) { (void)on; }
__attribute__((weak)) bool audio_get_input_monitor(void) { return false; }
__attribute__((weak)) u32 audio_capture_read(i16 *s, u32 f) { (void)s; (void)f; return 0; }
__attribute__((weak)) void sound_startup_chime(void) {}
__attribute__((weak)) void sound_click(void) {}
__attribute__((weak)) void sound_alert(void) {}
__attribute__((weak)) void synth_note(float f, int ms, int wave, float v) {}

/* ---- rede */
static net_info_t ni;
__attribute__((weak)) void net_init(void) {}
__attribute__((weak)) void net_poll(void) {}
__attribute__((weak)) bool net_link_up(void) { return false; }
__attribute__((weak)) bool net_configured(void) { return false; }
__attribute__((weak)) const char *net_device_name(void) { return "Nenhuma placa de rede"; }
__attribute__((weak)) void net_status_string(char *b, usize n) { snprintf(b, n, "Sem endereço IP"); }
__attribute__((weak)) net_info_t *net_info(void) { return &ni; }
__attribute__((weak)) void net_dhcp_renew(void) {}
__attribute__((weak)) void ip_to_str(u32 ip, char *b) { snprintf(b, 16, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255); }
__attribute__((weak)) bool str_to_ip(const char *s, u32 *ip) { return false; }
__attribute__((weak)) int dns_resolve(const char *h, u32 *ip) { return -1; }
__attribute__((weak)) bool icmp_ping_send(u32 ip, u16 seq) { return false; }
__attribute__((weak)) bool icmp_ping_reply(u16 seq, u64 *rtt) { return false; }
__attribute__((weak)) http_req_t *http_get(const char *url) {
    http_req_t *r = kcalloc(1, sizeof(http_req_t));
    strncpy(r->url, url, sizeof r->url - 1);
    strcpy(r->error, "rede indisponível");
    return r;
}
__attribute__((weak)) int http_poll(http_req_t *r) { return -1; }
__attribute__((weak)) void http_free(http_req_t *r) { if (r) { kfree(r->body); kfree(r); } }

__attribute__((weak)) http_req_t *http_post(const char *u, const char *ct, const void *b, u32 l) { return http_get(u); }
__attribute__((weak)) float http_progress(http_req_t *r) { return 0; }
__attribute__((weak)) void net_register_device(netdev_t *d) {}
__attribute__((weak)) bool udp_bind(u16 p, udp_handler_t h) { return false; }
__attribute__((weak)) bool udp_send(u32 ip, u16 sp, u16 dp, const void *d, u16 l) { return false; }
__attribute__((weak)) int tcp_connect(u32 ip, u16 port) { return -1; }
__attribute__((weak)) int tcp_state(int s) { return TCP_ERROR; }
__attribute__((weak)) int tcp_send(int s, const void *d, int l) { return -1; }
__attribute__((weak)) int tcp_recv(int s, void *b, int m) { return -1; }
__attribute__((weak)) void tcp_close(int s) {}
__attribute__((weak)) tls_conn_t *tls_new(int s, const char *sni) { return NULL; }
__attribute__((weak)) int tls_handshake(tls_conn_t *c) { return -1; }
__attribute__((weak)) int tls_send(tls_conn_t *c, const void *d, int l) { return -1; }
__attribute__((weak)) int tls_recv(tls_conn_t *c, void *b, int m) { return -1; }
__attribute__((weak)) void tls_free(tls_conn_t *c) {}
__attribute__((weak)) const char *tls_error(tls_conn_t *c) { return "TLS indisponível"; }

/* ---- USB / Bluetooth */
__attribute__((weak)) void usb_init(void) {}
__attribute__((weak)) void usb_poll(void) {}
__attribute__((weak)) const char *usb_status(void) { return "Pilha USB ainda não inicializada"; }
__attribute__((weak)) int usb_device_count(void) { return 0; }
__attribute__((weak)) const char *usb_device_desc(int i) { return ""; }
__attribute__((weak)) void bt_init(void) {}
__attribute__((weak)) const char *bt_status(void) { return "Sem adaptador"; }
__attribute__((weak)) int bt_device_count(void) { return 0; }
__attribute__((weak)) const char *bt_device_name(int i) { return ""; }
__attribute__((weak)) void bt_scan(void) {}

