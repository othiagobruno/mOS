/* mOS — teste de host do servidor SDP e do alvo AVRCP (kernel/bt/avrcp.c).
 *   gcc -O1 -Wall -Wextra -DMOS_HOST_TEST -I tools -I kernel/include tools/avrcp_host.c kernel/bt/avrcp.c -o /tmp/avrcp_host && /tmp/avrcp_host
 * Os pedidos SDP são os que um fone manda ao procurar o AVRCP (o mesmo formato que o bluetoothd do Linux
 * gera); a resposta é percorrida elemento por elemento, como o lado do fone faria. */
#include "host_shim.h"

long long g_live_allocs, g_live_bytes;
static int fails;
#define CHECK(c, ...) do { if (c) printf("ok   "); else { printf("FALHA "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int sdp_handle(const u8 *q, int len, u8 *o, int max);
int avrcp_handle(const u8 *f, int len, u8 *o, int max);
void avrcp_set_playing(bool on);
static u32 last_usage; static int ups, downs;
void input_media_usage(u32 usage, bool down) { last_usage = usage; if (down) downs++; else ups++; }
static int sysvol = 30;
void audio_set_volume(int pct) { sysvol = pct; }
int audio_get_volume(void) { return sysvol; }
int avrcp_ct_start(u8 *o);
int avrcp_volume_poll(u8 *o);
bool avrcp_abs_volume(void);

/* acha o valor do atributo id dentro de uma lista DES{id valor ...} */
static const u8 *find_attr(const u8 *l, int n, u16 id) {
    int i = 2;
    while (i + 3 <= n) {
        u16 a = (u16)(l[i + 1] << 8 | l[i + 2]);
        const u8 *v = l + i + 3;
        int vl = (v[0] & 7) == 5 ? 2 + v[1] : (v[0] == 0x09 ? 3 : v[0] == 0x0A ? 5 : v[0] == 0x19 ? 3 : 1);
        if (a == id) return v;
        i += 3 + vl;
    }
    return NULL;
}

int main(void) {
    u8 o[512];
    /* ServiceSearchAttributeRequest: UUID 0x110C (alvo AVRCP), atributos 0x0000-0xFFFF */
    const u8 q1[] = {0x06, 0x00, 0x01, 0x00, 0x0F, 0x35, 0x03, 0x19, 0x11, 0x0C, 0x02, 0x00, 0x35, 0x05, 0x0A, 0x00, 0x00, 0xFF, 0xFF, 0x00};
    int n = sdp_handle(q1, sizeof q1, o, sizeof o);
    CHECK(n > 10 && o[0] == 0x07 && o[1] == 0 && o[2] == 1, "SDP 0x06: resposta 0x07 com a mesma transação");
    u16 total = (u16)(o[5] << 8 | o[6]);
    CHECK(o[7] == 0x35 && o[9] == 0x35 && n == 8 + total && o[n - 1] == 0, "SDP: DES de listas, um registro, sem continuação");
    const u8 *rec = o + 9; int rl = 2 + rec[1];
    const u8 *proto = find_attr(rec, rl, 0x0004), *cls = find_attr(rec, rl, 0x0001), *prof = find_attr(rec, rl, 0x0009);
    CHECK(cls && cls[2] == 0x19 && cls[3] == 0x11 && cls[4] == 0x0C, "registro do alvo AVRCP (0x110C)");
    CHECK(proto && proto[4] == 0x19 && proto[5] == 0x01 && proto[6] == 0x00 && proto[8] == 0x00 && proto[9] == 0x17, "protocolo: L2CAP PSM 0x0017 (AVCTP)");
    CHECK(prof && prof[4] == 0x19 && prof[5] == 0x11 && prof[6] == 0x0E && prof[8] == 0x01 && prof[9] == 0x06, "perfil AVRCP 1.6");
    /* só o atributo 0x0311 */
    const u8 q2[] = {0x06, 0x00, 0x02, 0x00, 0x0D, 0x35, 0x03, 0x19, 0x11, 0x0C, 0x02, 0x00, 0x35, 0x03, 0x09, 0x03, 0x11, 0x00};
    n = sdp_handle(q2, sizeof q2, o, sizeof o);
    CHECK(n > 0 && o[7] == 0x35 && o[9] == 0x35 && o[10] == 6 && o[11] == 0x09 && o[12] == 0x03 && o[13] == 0x11, "SDP: filtro de atributo devolve só o 0x0311");
    /* busca A2DP (0x110A) e um UUID que não existe */
    const u8 q3[] = {0x02, 0x00, 0x03, 0x00, 0x08, 0x35, 0x03, 0x19, 0x11, 0x0A, 0x00, 0x05, 0x00};
    n = sdp_handle(q3, sizeof q3, o, sizeof o);
    CHECK(n == 14 && o[0] == 0x03 && o[8] == 1 && o[9] == 0x00 && o[10] == 0x01 && o[11] == 0x00 && o[12] == 0x01, "SDP 0x02: fonte A2DP achada (handle 0x00010001)");
    const u8 q4[] = {0x02, 0x00, 0x04, 0x00, 0x08, 0x35, 0x03, 0x19, 0x11, 0x08, 0x00, 0x05, 0x00};
    n = sdp_handle(q4, sizeof q4, o, sizeof o);
    CHECK(n == 10 && o[8] == 0, "SDP: UUID sem registro (0x1108, fone) devolve zero registros");
    const u8 q5[] = {0x06, 0x00, 0x05, 0x00, 0x30, 0x35};
    n = sdp_handle(q5, sizeof q5, o, sizeof o);
    CHECK(n == 7 && o[0] == 0x01, "SDP: pedido truncado vira erro");
    /* ServiceAttributeRequest pelo handle do controle */
    const u8 q6[] = {0x04, 0x00, 0x06, 0x00, 0x0C, 0x00, 0x01, 0x00, 0x03, 0x00, 0x80, 0x35, 0x03, 0x09, 0x00, 0x01, 0x00};
    n = sdp_handle(q6, sizeof q6, o, sizeof o);
    CHECK(n > 0 && o[0] == 0x05 && o[7] == 0x35 && o[9] == 0x09 && o[12] == 0x35 && o[13] == 6 && o[15] == 0x11 && o[16] == 0x0E && o[18] == 0x11 && o[19] == 0x0F, "SDP 0x04: classes 0x110E/0x110F do controle");

    /* AVRCP: PASS THROUGH play (aperta e solta) */
    const u8 play_dn[] = {0x10, 0x11, 0x0E, 0x00, 0x48, 0x7C, 0x44, 0x00}, play_up[] = {0x20, 0x11, 0x0E, 0x00, 0x48, 0x7C, 0xC4, 0x00};
    n = avrcp_handle(play_dn, sizeof play_dn, o, sizeof o);
    CHECK(n == 8 && o[0] == 0x12 && o[3] == 0x09 && o[6] == 0x44 && last_usage == 0x0C00CD && downs == 1, "AVRCP: play apertado -> aceito, tecla play/pausa do sistema");
    n = avrcp_handle(play_up, sizeof play_up, o, sizeof o);
    CHECK(n == 8 && o[0] == 0x22 && ups == 1, "AVRCP: play solto");
    const u8 vol[] = {0x30, 0x11, 0x0E, 0x00, 0x48, 0x7C, 0x41, 0x00};
    avrcp_handle(vol, sizeof vol, o, sizeof o);
    CHECK(last_usage == 0x0C00E9, "AVRCP: volume + vira a tecla de volume");
    const u8 next[] = {0x40, 0x11, 0x0E, 0x00, 0x48, 0x7C, 0x4B, 0x00};
    avrcp_handle(next, sizeof next, o, sizeof o);
    CHECK(last_usage == 0x0C00B5, "AVRCP: próxima faixa");
    const u8 bad[] = {0x50, 0x11, 0x0E, 0x00, 0x48, 0x7C, 0x7E, 0x00};
    n = avrcp_handle(bad, sizeof bad, o, sizeof o);
    CHECK(n == 8 && o[3] == 0x08, "AVRCP: operação desconhecida -> NOT IMPLEMENTED");
    const u8 unit[] = {0x60, 0x11, 0x0E, 0x01, 0xFF, 0x30, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    n = avrcp_handle(unit, sizeof unit, o, sizeof o);
    CHECK(n == 11 && o[3] == 0x0C && o[7] == 0x48, "AVRCP: UNIT INFO = painel");
    const u8 caps[] = {0x70, 0x11, 0x0E, 0x01, 0x48, 0x00, 0x00, 0x19, 0x58, 0x10, 0x00, 0x00, 0x01, 0x03};
    n = avrcp_handle(caps, sizeof caps, o, sizeof o);
    CHECK(n == 16 && o[3] == 0x0C && o[9] == 0x10 && o[13] == 0x03 && o[14] == 1 && o[15] == 0x01, "AVRCP: GetCapabilities(eventos) = estado de reprodução");
    avrcp_set_playing(true);
    const u8 reg[] = {0x80, 0x11, 0x0E, 0x03, 0x48, 0x00, 0x00, 0x19, 0x58, 0x31, 0x00, 0x00, 0x05, 0x01, 0, 0, 0, 0};
    n = avrcp_handle(reg, sizeof reg, o, sizeof o);
    CHECK(n == 15 && o[3] == 0x0F && o[13] == 0x01 && o[14] == 0x01, "AVRCP: RegisterNotification -> INTERIM tocando");
    const u8 pid[] = {0x90, 0x11, 0x0F, 0x00, 0x48, 0x7C, 0x44, 0x00};
    n = avrcp_handle(pid, sizeof pid, o, sizeof o);
    CHECK(n == 3 && (o[0] & 1), "AVCTP: PID errado -> IPID");
    /* volume absoluto: o mOS como controle */
    n = avrcp_ct_start(o);
    CHECK(n == 14 && !(o[0] & 0x02) && o[3] == 0x01 && o[9] == 0x10 && o[13] == 0x03, "volume: GetCapabilities(eventos) para o fone");
    u8 lbl = o[0];
    u8 r1[18] = {(u8)(lbl | 0x02), 0x11, 0x0E, 0x0C, 0x48, 0x00, 0x00, 0x19, 0x58, 0x10, 0x00, 0x00, 0x04, 0x03, 2, 0x01, 0x0D};
    n = avrcp_handle(r1, 17, o, sizeof o);
    CHECK(n == 18 && o[3] == 0x03 && o[9] == 0x31 && o[13] == 0x0D && avrcp_abs_volume(), "o fone tem VOLUME_CHANGED: registra a notificação");
    u8 r2[16] = {(u8)(o[0] | 0x02), 0x11, 0x0E, 0x0F, 0x48, 0x00, 0x00, 0x19, 0x58, 0x31, 0x00, 0x00, 0x02, 0x0D, 0x40};
    n = avrcp_handle(r2, 15, o, sizeof o);
    CHECK(n == 0 && sysvol == 0x40 * 100 / 127, "INTERIM: volume do fone (64/127) vira o do sistema (%d %%)", sysvol);
    CHECK(avrcp_volume_poll(o) == 0, "sem mudança local: nada é enviado (sem eco)");
    u8 r3[16] = {0x52, 0x11, 0x0E, 0x0D, 0x48, 0x00, 0x00, 0x19, 0x58, 0x31, 0x00, 0x00, 0x02, 0x0D, 0x7F};
    n = avrcp_handle(r3, 15, o, sizeof o);
    CHECK(sysvol == 100 && n == 18 && o[9] == 0x31, "CHANGED no fone: volume 100 %% e registra de novo");
    sysvol = 25;
    n = avrcp_volume_poll(o);
    CHECK(n == 14 && o[3] == 0x00 && o[9] == 0x50 && o[13] == 25 * 127 / 100, "volume do sistema mudou: SetAbsoluteVolume(%u)", o[13]);
    CHECK(avrcp_volume_poll(o) == 0, "SetAbsoluteVolume só uma vez por mudança");
    printf(fails ? "avrcp: %d FALHA(S)\n" : "avrcp: ok\n", fails);
    return fails != 0;
}
