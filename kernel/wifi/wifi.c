/*
 * mOS — inicialização do Wi-Fi: procura placas conhecidas (PCIe e USB) e, sem nenhuma, o rádio
 * simulado de teste. Também roda o autoteste ponta a ponta quando o simulador está ligado.
 */
#include "wifi.h"
#include "net.h"

bool wifi_sim_init(void);
__attribute__((weak)) bool wifi_iwl_probe(void) { return false; }      /* Intel (wifi/iwlwifi.c) */
__attribute__((weak)) bool wifi_rtw_probe(void) { return false; }      /* Realtek RTL8821CE (wifi/rtw88/rtw8821c.c) */
__attribute__((weak)) bool wifi_rt2800_probe(void) { return false; }   /* Ralink/MediaTek USB (wifi/rt2800usb.c) */

static bool selftest;

/* chips Wi-Fi PCIe mais comuns (para identificar a placa mesmo sem driver) */
static const struct { u16 vendor, device; const char *name; } known[] = {
    {0x8086, 0x2723, "Intel Wi-Fi 6 AX200"},        {0x8086, 0x2725, "Intel Wi-Fi 6E AX210"},
    {0x8086, 0x43F0, "Intel Wi-Fi 6 AX201"},        {0x8086, 0xA0F0, "Intel Wi-Fi 6 AX201"},
    {0x8086, 0x02F0, "Intel Wi-Fi 6 AX201"},        {0x8086, 0x06F0, "Intel Wi-Fi 6 AX201"},
    {0x8086, 0x34F0, "Intel Wi-Fi 6 AX201"},        {0x8086, 0x4DF0, "Intel Wi-Fi 6 AX201"},
    {0x8086, 0x51F0, "Intel Wi-Fi 6E AX211"},       {0x8086, 0x51F1, "Intel Wi-Fi 6E AX211"},
    {0x8086, 0x54F0, "Intel Wi-Fi 6E AX211"},       {0x8086, 0x7A70, "Intel Wi-Fi 6E AX211"},
    {0x8086, 0x272B, "Intel Wi-Fi 7 BE200"},        {0x8086, 0x2526, "Intel Wireless-AC 9260"},
    {0x8086, 0x9DF0, "Intel Wireless-AC 9560"},     {0x8086, 0xA370, "Intel Wireless-AC 9560"},
    {0x8086, 0x31DC, "Intel Wireless-AC 9560"},     {0x8086, 0x30DC, "Intel Wireless-AC 9560"},
    {0x8086, 0x24FD, "Intel Wireless 8265"},        {0x8086, 0x24F3, "Intel Wireless 8260"},
    {0x8086, 0x095A, "Intel Wireless 7265"},        {0x8086, 0x095B, "Intel Wireless 7265"},
    {0x10EC, 0xC821, "Realtek RTL8821CE"},          {0x10EC, 0xC822, "Realtek RTL8822CE"},
    {0x10EC, 0xB822, "Realtek RTL8822BE"},          {0x10EC, 0xB821, "Realtek RTL8821BE"},
    {0x10EC, 0xD723, "Realtek RTL8723DE"},          {0x10EC, 0xB723, "Realtek RTL8723BE"},
    {0x10EC, 0x8852, "Realtek RTL8852AE"},          {0x10EC, 0xB852, "Realtek RTL8852BE"},
    {0x10EC, 0xC852, "Realtek RTL8852CE"},          {0x10EC, 0x818B, "Realtek RTL8192EE"},
    {0x14C3, 0x7961, "MediaTek MT7921 (Wi-Fi 6)"},  {0x14C3, 0x0608, "MediaTek MT7921K / RZ608"},
    {0x14C3, 0x0616, "MediaTek MT7922 / RZ616"},    {0x14C3, 0x7922, "MediaTek MT7922"},
    {0x168C, 0x003E, "Qualcomm Atheros QCA6174"},   {0x168C, 0x0042, "Qualcomm Atheros QCA9377"},
    {0x17CB, 0x1103, "Qualcomm WCN6855 / QCA6390"}, {0x168C, 0x0032, "Qualcomm Atheros AR9485"},
    {0x168C, 0x0030, "Qualcomm Atheros AR93xx"},    {0x168C, 0x0034, "Qualcomm Atheros AR9462"},
    {0x14E4, 0x43A0, "Broadcom BCM4360"},           {0x14E4, 0x43B1, "Broadcom BCM4352"},
    {0x14E4, 0x4331, "Broadcom BCM4331"},           {0x14E4, 0x43BA, "Broadcom BCM43602"},
};

static char detected[96];
const char *wifi_detected_chip(void) { return detected; }

void wifi_init(void) {
    for (int i = 0; i < pci_count(); i++) {
        pci_dev_t *d = pci_get(i);
        for (u32 k = 0; k < ARRAY_LEN(known); k++)
            if (d->vendor == known[k].vendor && d->device == known[k].device) {
                snprintf(detected, sizeof detected, "%s", known[k].name);
                kprintf("wifi: placa %s (%04x:%04x) em %02x:%02x.%x\n", known[k].name, d->vendor, d->device, d->bus, d->dev, d->fn);
            }
        if (!detected[0] && d->class_ == 2 && d->subclass == 0x80) {
            snprintf(detected, sizeof detected, "placa de rede sem fio %04x:%04x", d->vendor, d->device);
            kprintf("wifi: %s (desconhecida)\n", detected);
        }
    }
    if (wifi_iwl_probe() || wifi_rtw_probe() || wifi_rt2800_probe()) return;
    if (wifi_sim_init()) { selftest = true; return; }
    if (detected[0]) kprintf("wifi: %s encontrada, mas o driver dela ainda não existe no mOS\n", detected);
    else kprintf("wifi: nenhuma placa Wi-Fi encontrada\n");
}

/* ------------------------------------------------------------ autoteste com o rádio simulado
 * Roda dentro de wifi_poll (laço do desktop): varre, tenta a senha errada, conecta com a certa,
 * espera o DHCP e faz ping no roteador simulado. */
static int step;
static u64 t_step;
static u16 ping_seq;

static void report(bool ok, const char *what) { kprintf("wifi-teste: %s %s\n", ok ? "ok " : "FALHOU", what); }

void wifi_selftest_poll(void) {
    if (!selftest) return;
    wifi_status_t st;
    wifi_status(&st);
    u64 now = timer_ticks();
    switch (step) {
    case 0:
        if (now < 3000) return;                                    /* deixa o boot terminar */
        wifi_scan();
        step = 1; t_step = now;
        return;
    case 1: {
        if (st.state == WIFI_SCANNING) return;
        wifi_network_t l[8];
        int n = wifi_networks(l, 8);
        bool wpa2 = false, open = false, sae = false;
        for (int i = 0; i < n; i++) {
            kprintf("wifi-teste:   rede \"%s\" canal %d %d dBm seg %d%s\n", l[i].ssid, l[i].channel, l[i].rssi, l[i].security, l[i].supported ? "" : " (não suportada)");
            if (!strcmp(l[i].ssid, "mOS-Teste") && l[i].security == WIFI_SEC_WPA2_PSK) wpa2 = true;
            if (!strcmp(l[i].ssid, "Cafe Aberto") && l[i].security == WIFI_SEC_OPEN) open = true;
            if (!strcmp(l[i].ssid, "Vizinho WPA3") && !l[i].supported) sae = true;
        }
        report(n == 3 && wpa2 && open && sae, "varredura encontra as 3 redes com a segurança certa");
        report(n >= 1 && !strcmp(l[0].ssid, "mOS-Teste"), "lista ordenada pelo sinal");
        wifi_connect("mOS-Teste", "senha-errada-123");
        step = 2; t_step = now;
        return;
    }
    case 2:
        if (st.state != WIFI_FAILED && now - t_step < 8000) return;
        report(st.state == WIFI_FAILED && strstr(st.error, "Senha"), "senha errada é recusada");
        kprintf("wifi-teste:   (erro mostrado: %s)\n", st.error);
        wifi_connect("mOS-Teste", "senha-do-teste");
        step = 3; t_step = now;
        return;
    case 3:
        if (st.state != WIFI_CONNECTED && st.state != WIFI_FAILED && now - t_step < 8000) return;
        report(st.state == WIFI_CONNECTED, "conecta com WPA2-PSK (handshake de 4 vias + CCMP)");
        step = st.state == WIFI_CONNECTED ? 4 : 99; t_step = now;
        return;
    case 4: {
        net_info_t *ni = net_info();
        if ((net_configured() || now - t_step > 1500) && !strstr(ni->driver, "Wi-Fi")) {    /* cabo ativo: ele tem prioridade */
            kprintf("wifi-teste: pulado DHCP/ping (a interface ativa é \"%s\"; rode com run.ps1 -NoNet)\n", ni->driver);
            wifi_disconnect();
            step = 6;
            return;
        }
        if (!net_configured() && now - t_step < 10000) return;
        char ip[16];
        ip_to_str(ni->ip, ip);
        report(net_configured() && ni->ip == 0xC0A84D0A, "DHCP pela rede Wi-Fi cifrada");
        kprintf("wifi-teste:   IP %s via %s\n", ip, ni->driver);
        icmp_ping_send(0xC0A84D01, ++ping_seq);
        step = 5; t_step = now;
        return;
    }
    case 5: {
        u64 rtt;
        bool got = icmp_ping_reply(ping_seq, &rtt);
        if (!got && now - t_step < 3000) return;
        report(got, "ping 192.168.77.1 pelo Wi-Fi");
        wifi_disconnect();
        step = 6;
        kprintf("wifi-teste: fim\n");
        return;
    }
    default:
        return;
    }
}

/* ------------------------------------------------------------ taxa do descritor -> kb/s (1 fluxo) */
int wifi_rate_kbps(int rate, int sgi, int bw) {
    static const int cck[4] = {1000, 2000, 5500, 11000};
    static const int ofdm[8] = {6000, 9000, 12000, 18000, 24000, 36000, 48000, 54000};
    static const int mcs20[10] = {6500, 13000, 19500, 26000, 39000, 52000, 58500, 65000, 78000, 0};   /* MCS9 não existe em 20 MHz */
    static const int mcs40[10] = {13500, 27000, 40500, 54000, 81000, 108000, 121500, 135000, 162000, 180000};
    static const int mcs80[10] = {29300, 58500, 87800, 117000, 175500, 234000, 263300, 292500, 351000, 390000};
    int v;
    if (rate < 0) return 0;
    if (rate <= 0x03) return cck[rate];
    if (rate <= 0x0B) return ofdm[rate - 4];
    if (rate >= 0x0C && rate <= 0x13) v = (bw ? mcs40 : mcs20)[rate - 0x0C];
    else if (rate >= 0x2C && rate <= 0x35) v = (bw == 2 ? mcs80 : bw == 1 ? mcs40 : mcs20)[rate - 0x2C];
    else return 0;
    return sgi ? v * 10 / 9 : v;
}

/* ------------------------------------------------------------ CoreWLAN (GUI_DEVICE, DEV_WIFI_INFO) */
#ifndef MOS_HOST_TEST
#include "proc.h"
#include "syscall.h"
#include "../include/compat_gui.h"
static void mac_str(const u8 *m, char *o) { snprintf(o, 18, "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]); }

i64 wifi_user_info(process_t *p, u64 uptr) {
    gui_wifi_info_t o;
    memset(&o, 0, sizeof o);
    wifi_status_t st;
    wifi_status(&st);
    wifi_radio_t *r = wifi_radio();
    o.present = r != NULL;
    o.power = st.state != WIFI_OFF;
    o.state = st.state;
    snprintf(o.ifname, sizeof o.ifname, "en1");
    if (r) mac_str(r->mac, o.mac);
    if (st.state == WIFI_CONNECTED) {
        snprintf(o.ssid, sizeof o.ssid, "%s", st.ssid);
        mac_str(st.bssid, o.bssid);
        o.rssi = st.rssi;
        o.channel = st.channel;
        o.band = st.channel > 14 ? 2 : 1;
        o.security = st.security;
        int rate = -1, sgi = 0, bw = 0, noise = 0;
        if (r && r->link_info) r->link_info(r, &rate, &sgi, &bw, &noise);
        o.width = bw + 1;
        o.noise = noise;
        o.tx_kbps = wifi_rate_kbps(rate, sgi, bw);
        o.phy = rate >= 0x2C ? 5 : rate >= 0x0C ? 4 : rate >= 0x04 ? (o.band == 2 ? 1 : 3) : rate >= 0 ? 2 : (o.band == 2 ? 1 : 3);
    }
    if (uptr && !uvm_copy_out(p, uptr, &o, sizeof o)) return -MOS_EFAULT;
    return o.present;
}
#endif
