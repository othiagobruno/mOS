/* gcc -DWA_HOST -I userland/libsystem tools/waframeworks_hosttest.c \
 *   userland/libsystem/wa_small_core.c -o /tmp/waframeworks && /tmp/waframeworks
 * Estado local. Não sobe a VM e não fala com rádio, GPS nem rede. */
#include "wa_small_core.h"
#include <stdio.h>

static int fails;
static void expect(int cond, const char *msg) {
    if (!cond) { fprintf(stderr, "FALHOU: %s\n", msg); fails++; }
    else printf("ok %s\n", msg);
}

static void test_callkit(void) {
    cx_box b;
    int idx = -1, rc;
    const cx_call *c;
    cx_box_init(&b);
    rc = cx_submit(&b, CX_START, "u1", "5511", 0, 0, &idx);
    expect(rc == CX_ERR_NO_PROVIDER && idx == -1, "CallKit sem provedor não aceita a transação");
    expect(cx_find_call(&b, "u1") == 0, "CallKit sem provedor não cria chamada");
    cx_set_provider(&b, 1, 1);
    rc = cx_submit(&b, CX_START, "u1", "5511", 0, 0, &idx);
    expect(rc == CX_OK && b.acts[idx].state == CX_PENDING, "CallKit guarda a ação pendente");
    expect(cx_fulfill(&b, idx) == CX_OK && b.acts[idx].state == CX_FULFILLED, "fulfill marca a ação cumprida");
    c = cx_find_call(&b, "u1");
    expect(c && c->alive == CX_CALL_ACTIVE && c->muted == 0, "a chamada local fica ativa, sem linha telefônica");
    rc = cx_submit(&b, CX_MUTE, "u1", 0, 1, 0, &idx);
    expect(rc == CX_OK && cx_fulfill(&b, idx) == CX_OK && cx_find_call(&b, "u1")->muted == 1, "mudo é estado local");
    rc = cx_submit(&b, CX_END, "u1", 0, 0, 0, &idx);
    expect(rc == CX_OK && cx_fail(&b, idx) == CX_OK && cx_find_call(&b, "u1")->alive == CX_CALL_ACTIVE, "fail não encerra a chamada");
    rc = cx_submit(&b, CX_ANSWER, "nao-existe", 0, 0, 0, &idx);
    expect(cx_fulfill(&b, idx) == CX_OK && cx_find_call(&b, "nao-existe") == 0, "atender UUID desconhecido não inventa chamada");
    expect(cx_report_incoming(&b, "in", "100") == CX_OK && cx_find_call(&b, "in")->alive == CX_CALL_RINGING, "entrada fica tocando só no estado local");
}

static void test_telephony(void) {
    expect(ct_provider_count() == 0, "CoreTelephony sem operadora");
    expect(ct_radio() == 0, "CoreTelephony sem tecnologia de rádio atual");
}

static void test_location(void) {
    expect(cl_auth_from_tcc(0) == CL_AUTH_NOT, "TCC ausente é NotDetermined");
    expect(cl_auth_from_tcc(1) == CL_AUTH_DENIED, "TCC negado é Denied");
    expect(cl_auth_from_tcc(2) == CL_AUTH_WHEN_IN_USE, "TCC concedido é WhenInUse");
    expect(cl_location_error(CL_AUTH_WHEN_IN_USE) == CL_ERR_UNKNOWN, "com permissão e sem GPS o erro é LocationUnknown");
    expect(cl_location_error(CL_AUTH_DENIED) == CL_ERR_DENIED, "sem permissão o erro é Denied");
    expect(cl_coord_valid(0.0 / 0.0, 0) == 0, "coordenada inválida (NaN) não passa");
    expect(cl_coord_valid(-23.5, -46.6) == 1, "a função só valida o par; ninguém devolve essa coordenada");
}

static void test_bg(void) {
    bg_box b;
    int err = 0;
    bg_box_init(&b);
    expect(bg_submit(&b, "com.wa.refresh", BG_REFRESH, &err) == -1 && err == BG_ERR_NOT_PERMITTED && b.nsub == 0, "submit sem registro não guarda e não é permitido");
    expect(bg_register(&b, "com.wa.refresh") == 0 && b.nreg == 1, "register guarda o identificador");
    expect(bg_submit(&b, "com.wa.refresh", BG_REFRESH, &err) == -1 && err == BG_ERR_UNAVAILABLE && b.nsub == 1, "submit registrado guarda e devolve indisponível");
    expect(b.sub[0].kind == BG_REFRESH, "a tarefa guardada é a que foi pedida");
}

int main(void) {
    test_callkit();
    test_telephony();
    test_location();
    test_bg();
    if (fails) { fprintf(stderr, "wa frameworks: %d falha(s)\n", fails); return 1; }
    printf("wa frameworks: ok\n");
    return 0;
}
