/* gcc -DWA_HOST -I userland/libsystem tools/waframeworks_hosttest.c \
 *   userland/libsystem/wa_small_core.c -o /tmp/waframeworks && /tmp/waframeworks
 * Estado local. Não sobe a VM e não fala com rádio, GPS nem rede. */
#include "wa_small_core.h"
#include <stdio.h>
#include <string.h>

static int fails;
static void expect(int cond, const char *msg) {
    if (!cond) { fprintf(stderr, "FALHOU: %s\n", msg); fails++; }
    else printf("ok %s\n", msg);
}

static void test_callkit(void) {
    cx_box b;
    int idx = -1, rc;
    const cx_call *c;
    wa_cx_box_init(&b);
    rc = wa_cx_submit(&b, CX_START, "u1", "5511", 0, 0, &idx);
    expect(rc == CX_ERR_NO_PROVIDER && idx == -1, "CallKit sem provedor não aceita a transação");
    expect(wa_cx_find_call(&b, "u1") == 0, "CallKit sem provedor não cria chamada");
    wa_cx_set_provider(&b, 1, 1);
    rc = wa_cx_submit(&b, CX_START, "u1", "5511", 0, 0, &idx);
    expect(rc == CX_OK && b.acts[idx].state == CX_PENDING, "CallKit guarda a ação pendente");
    expect(wa_cx_fulfill(&b, idx) == CX_OK && b.acts[idx].state == CX_FULFILLED, "fulfill marca a ação cumprida");
    c = wa_cx_find_call(&b, "u1");
    expect(c && c->alive == CX_CALL_ACTIVE && c->muted == 0, "a chamada local fica ativa, sem linha telefônica");
    rc = wa_cx_submit(&b, CX_MUTE, "u1", 0, 1, 0, &idx);
    expect(rc == CX_OK && wa_cx_fulfill(&b, idx) == CX_OK && wa_cx_find_call(&b, "u1")->muted == 1, "mudo é estado local");
    rc = wa_cx_submit(&b, CX_END, "u1", 0, 0, 0, &idx);
    expect(rc == CX_OK && wa_cx_fail(&b, idx) == CX_OK && wa_cx_find_call(&b, "u1")->alive == CX_CALL_ACTIVE, "fail não encerra a chamada");
    rc = wa_cx_submit(&b, CX_ANSWER, "nao-existe", 0, 0, 0, &idx);
    expect(wa_cx_fulfill(&b, idx) == CX_OK && wa_cx_find_call(&b, "nao-existe") == 0, "atender UUID desconhecido não inventa chamada");
    expect(wa_cx_report_incoming(&b, "in", "100") == CX_OK && wa_cx_find_call(&b, "in")->alive == CX_CALL_RINGING, "entrada fica tocando só no estado local");
}

static void test_callkit_objetos(void) {
    cx_box b;
    cx_cfg cfg;
    cx_update u;
    cx_ev ev[8];
    int idx = -1, n;
    const cx_call *c;
    unsigned h1, h2;
    wa_cx_box_init(&b);
    expect(wa_cx_handle_equal(CX_HANDLE_PHONE, "5511", CX_HANDLE_PHONE, "5511") == 1, "CXHandle igual em type e value");
    expect(wa_cx_handle_equal(CX_HANDLE_PHONE, "5511", CX_HANDLE_EMAIL, "5511") == 0, "CXHandle de outro type não é igual");
    h1 = wa_cx_handle_hash(CX_HANDLE_PHONE, "5511");
    h2 = wa_cx_handle_hash(CX_HANDLE_PHONE, "5511");
    expect(h1 == h2 && h1 != wa_cx_handle_hash(CX_HANDLE_PHONE, "5512"), "hash do CXHandle acompanha o value");
    wa_cx_set_provider(&b, 1, 1);
    memset(&cfg, 0, sizeof cfg);
    cfg.max_groups = 1;
    cfg.max_per = 1;
    cfg.video = 0;
    cfg.handle_mask = 1u << CX_HANDLE_PHONE;
    wa_cx_set_config(&b, &cfg);
    wa_cx_set_delegate_queue(&b, 4);
    wa_cx_set_observer(&b, 1, 9);
    expect(wa_cx_submit_full(&b, CX_START, "v", "x", CX_HANDLE_PHONE, 0, 0, 1, &idx) == CX_ERR_VIDEO, "supportsVideo recusa vídeo");
    expect(wa_cx_submit_full(&b, CX_START, "e", "a@b", CX_HANDLE_EMAIL, 0, 0, 0, &idx) == CX_ERR_HANDLE, "supportedHandleTypes recusa e-mail");
    expect(wa_cx_submit_full(&b, CX_START, "a", "1", CX_HANDLE_PHONE, 0, 0, 0, &idx) == CX_OK, "primeira chamada cabe no grupo");
    expect(wa_cx_drain(&b, 4, ev, 8) == 1 && ev[0].what == CX_EV_ACTION && ev[0].action == CX_START, "o delegate sai na fila pedida");
    expect(wa_cx_drain(&b, 5, ev, 8) == 0, "outra fila não recebe o delegate");
    expect(wa_cx_submit_full(&b, CX_START, "b", "2", CX_HANDLE_PHONE, 0, 0, 0, &idx) == CX_ERR_LIMIT, "maximumCallsPerCallGroup recusa a segunda");
    expect(wa_cx_fulfill(&b, 0) == CX_OK, "fulfill da primeira");
    c = wa_cx_find_call(&b, "a");
    expect(c && c->outgoing == 1 && c->has_connected == 1 && c->has_ended == 0, "CXCall de saída conectada");
    n = wa_cx_drain(&b, 9, ev, 8);
    expect(n == 1 && ev[0].what == CX_EV_CHANGED && ev[0].connected == 1 && ev[0].outgoing == 1, "callChanged na fila do observer");
    memset(&u, 0, sizeof u);
    u.has_video = -1;
    u.hold = u.group = u.ungroup = u.dtmf = -1;
    u.has_name = 1;
    memcpy(u.name, "Ana", 4);
    u.has_handle = 1;
    u.handle_type = CX_HANDLE_PHONE;
    memcpy(u.handle, "5511999", 8);
    u.hold = 1;
    u.group = 0;
    u.dtmf = 1;
    expect(wa_cx_apply_update(&b, "a", &u) == CX_OK, "reportCallWithUUID:updated: aplica no estado");
    c = wa_cx_find_call(&b, "a");
    expect(c && !strcmp(c->name, "Ana") && !strcmp(c->handle, "5511999") && c->cap_hold == 1 && c->cap_group == 0 && c->cap_dtmf == 1, "nome, handle e capacidades ficam na chamada");
    expect(wa_cx_apply_update(&b, "nao", &u) == CX_ERR_BAD, "update não inventa chamada");
    expect(wa_cx_call_count(&b) == 1 && wa_cx_call_at(&b, 0) == c, "o observer publica essa CXCall");
    expect(wa_cx_submit(&b, CX_END, "a", 0, 0, 0, &idx) == CX_OK && wa_cx_fulfill(&b, idx) == CX_OK && wa_cx_find_call(&b, "a")->has_ended == 1, "encerrar marca hasEnded");
    cfg.max_groups = 1;
    cfg.max_per = 2;
    wa_cx_set_config(&b, &cfg);
    expect(wa_cx_submit_full(&b, CX_START, "c", "3", CX_HANDLE_PHONE, 0, 0, 0, &idx) == CX_OK && wa_cx_fulfill(&b, idx) == CX_OK, "um grupo novo depois do encerramento");
    expect(wa_cx_submit_full(&b, CX_START, "d", "4", CX_HANDLE_PHONE, 0, 0, 0, &idx) == CX_OK, "maximumCallsPerCallGroup 2 cabe no mesmo grupo");
    expect(wa_cx_submit_full(&b, CX_START, "e", "5", CX_HANDLE_PHONE, 0, 0, 0, &idx) == CX_ERR_LIMIT, "o grupo cheio e maximumCallGroups 1 recusam a próxima");
}

static void test_telephony(void) {
    expect(wa_ct_providers() == 0, "CoreTelephony sem operadora");
    expect(wa_ct_radio() == 0, "CoreTelephony sem tecnologia de rádio atual");
}

static void test_location(void) {
    expect(wa_cl_auth(0) == CL_AUTH_NOT, "TCC ausente é NotDetermined");
    expect(wa_cl_auth(1) == CL_AUTH_DENIED, "TCC negado é Denied");
    expect(wa_cl_auth(2) == CL_AUTH_WHEN_IN_USE, "TCC concedido é WhenInUse");
    expect(wa_cl_error(CL_AUTH_WHEN_IN_USE) == CL_ERR_UNKNOWN, "com permissão e sem GPS o erro é LocationUnknown");
    expect(wa_cl_error(CL_AUTH_DENIED) == CL_ERR_DENIED, "sem permissão o erro é Denied");
    expect(wa_cl_coord_valid(0.0 / 0.0, 0) == 0, "coordenada inválida (NaN) não passa");
    expect(wa_cl_coord_valid(-23.5, -46.6) == 1, "a função só valida o par; ninguém devolve essa coordenada");
}

static void test_bg(void) {
    bg_box b;
    int err = 0;
    wa_bg_init(&b);
    expect(wa_bg_submit(&b, "com.wa.refresh", BG_REFRESH, &err) == -1 && err == BG_ERR_NOT_PERMITTED && b.nsub == 0, "submit sem registro não guarda e não é permitido");
    expect(wa_bg_register(&b, "com.wa.refresh") == 0 && b.nreg == 1, "register guarda o identificador");
    expect(wa_bg_submit(&b, "com.wa.refresh", BG_REFRESH, &err) == -1 && err == BG_ERR_UNAVAILABLE && b.nsub == 1, "submit registrado guarda e devolve indisponível");
    expect(b.sub[0].kind == BG_REFRESH, "a tarefa guardada é a que foi pedida");
}

int main(void) {
    test_callkit();
    test_callkit_objetos();
    test_telephony();
    test_location();
    test_bg();
    if (fails) { fprintf(stderr, "wa frameworks: %d falha(s)\n", fails); return 1; }
    printf("wa frameworks: ok\n");
    return 0;
}
