#include "wa_small_core.h"
#ifdef WA_HOST
#include <string.h>
#else
#include "libc.h"
#endif

static void copy_s(char *d, int cap, const char *s) {
    int i = 0;
    if (!s) s = "";
    for (; s[i] && i + 1 < cap; i++) d[i] = s[i];
    d[i] = 0;
}

void cx_box_init(cx_box *b) { memset(b, 0, sizeof *b); }

int cx_set_provider(cx_box *b, int on, int delegate_on) {
    if (!b) return CX_ERR_BAD;
    b->provider = on ? 1 : 0;
    b->delegate = (on && delegate_on) ? 1 : 0;
    return CX_OK;
}

int cx_submit(cx_box *b, int kind, const char *uuid, const char *handle, int muted, int held, int *idx) {
    cx_act *a;
    if (idx) *idx = -1;
    if (!b || kind < CX_START || kind > CX_MUTE || !uuid || !uuid[0]) return CX_ERR_BAD;
    if (!b->provider) return CX_ERR_NO_PROVIDER;
    if (b->nacts >= 16) return CX_ERR_BAD;
    a = &b->acts[b->nacts];
    memset(a, 0, sizeof *a);
    a->kind = kind;
    a->state = CX_PENDING;
    a->muted = muted;
    a->held = held;
    copy_s(a->uuid, sizeof a->uuid, uuid);
    copy_s(a->handle, sizeof a->handle, handle);
    if (idx) *idx = b->nacts;
    b->nacts++;
    return CX_OK;
}

static cx_call *find_mut(cx_box *b, const char *uuid) {
    int i;
    for (i = 0; i < b->ncalls; i++)
        if (b->calls[i].alive && !strcmp(b->calls[i].uuid, uuid)) return &b->calls[i];
    return 0;
}

const cx_call *cx_find_call(const cx_box *b, const char *uuid) {
    int i;
    if (!b || !uuid) return 0;
    for (i = 0; i < b->ncalls; i++)
        if (b->calls[i].alive && !strcmp(b->calls[i].uuid, uuid)) return &b->calls[i];
    return 0;
}

int cx_fulfill(cx_box *b, int idx) {
    cx_act *a;
    cx_call *c;
    if (!b || idx < 0 || idx >= b->nacts) return CX_ERR_BAD;
    a = &b->acts[idx];
    if (a->state != CX_PENDING) return CX_ERR_BAD;
    a->state = CX_FULFILLED;
    c = find_mut(b, a->uuid);
    if (a->kind == CX_START) {
        if (!c && b->ncalls < 16) {
            c = &b->calls[b->ncalls++];
            memset(c, 0, sizeof *c);
            copy_s(c->uuid, sizeof c->uuid, a->uuid);
            copy_s(c->handle, sizeof c->handle, a->handle);
        }
        if (c) c->alive = CX_CALL_ACTIVE;
    } else if (a->kind == CX_ANSWER) {
        if (c) c->alive = CX_CALL_ACTIVE;
    } else if (a->kind == CX_END) {
        if (c) c->alive = CX_CALL_ENDED;
    } else if (a->kind == CX_HOLD) {
        if (c) c->held = a->held ? 1 : 0;
    } else if (a->kind == CX_MUTE) {
        if (c) c->muted = a->muted ? 1 : 0;
    }
    return CX_OK;
}

int cx_fail(cx_box *b, int idx) {
    if (!b || idx < 0 || idx >= b->nacts) return CX_ERR_BAD;
    if (b->acts[idx].state != CX_PENDING) return CX_ERR_BAD;
    b->acts[idx].state = CX_FAILED;
    return CX_OK;
}

int cx_report_incoming(cx_box *b, const char *uuid, const char *handle) {
    cx_call *c;
    if (!b || !b->provider || !uuid || !uuid[0]) return CX_ERR_BAD;
    if (b->ncalls >= 16) return CX_ERR_BAD;
    c = &b->calls[b->ncalls++];
    memset(c, 0, sizeof *c);
    c->alive = CX_CALL_RINGING;
    copy_s(c->uuid, sizeof c->uuid, uuid);
    copy_s(c->handle, sizeof c->handle, handle);
    return CX_OK;
}

int cl_auth_from_tcc(int tcc_state) {
    if (tcc_state == 2) return CL_AUTH_WHEN_IN_USE;
    if (tcc_state == 1) return CL_AUTH_DENIED;
    return CL_AUTH_NOT;
}

int cl_location_error(int auth) {
    if (auth == CL_AUTH_WHEN_IN_USE) return CL_ERR_UNKNOWN;
    return CL_ERR_DENIED;
}

int cl_coord_valid(double lat, double lon) {
    if (lat != lat || lon != lon) return 0;
    if (lat < -90.0 || lat > 90.0) return 0;
    if (lon < -180.0 || lon > 180.0) return 0;
    return 1;
}

void bg_box_init(bg_box *b) { memset(b, 0, sizeof *b); }

static int bg_has(const bg_box *b, const char *ident) {
    int i;
    for (i = 0; i < b->nreg; i++)
        if (!strcmp(b->reg[i].id, ident)) return 1;
    return 0;
}

int bg_register(bg_box *b, const char *ident) {
    if (!b || !ident || !ident[0] || strlen(ident) > 79) return -1;
    if (bg_has(b, ident)) return 0;
    if (b->nreg >= 8) return -1;
    memset(b->reg[b->nreg].id, 0, sizeof b->reg[b->nreg].id);
    memcpy(b->reg[b->nreg].id, ident, strlen(ident));
    b->nreg++;
    return 0;
}

int bg_submit(bg_box *b, const char *ident, int kind, int *err) {
    if (err) *err = BG_ERR_NOT_PERMITTED;
    if (!b || !ident || !ident[0] || (kind != BG_REFRESH && kind != BG_PROCESSING)) return -1;
    if (!bg_has(b, ident)) return -1;
    if (b->nsub >= 8) {
        if (err) *err = BG_ERR_UNAVAILABLE;
        return -1;
    }
    memset(&b->sub[b->nsub], 0, sizeof b->sub[b->nsub]);
    memcpy(b->sub[b->nsub].id, ident, strlen(ident) < 79 ? strlen(ident) : 79);
    b->sub[b->nsub].kind = kind;
    b->nsub++;
    if (err) *err = BG_ERR_UNAVAILABLE;
    return -1;
}

int ct_provider_count(void) { return 0; }
const char *ct_radio(void) { return 0; }
