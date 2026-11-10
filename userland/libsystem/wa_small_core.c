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

void wa_cx_box_init(cx_box *b) { memset(b, 0, sizeof *b); }

int wa_cx_set_provider(cx_box *b, int on, int delegate_on) {
    if (!b) return CX_ERR_BAD;
    b->provider = on ? 1 : 0;
    b->delegate = (on && delegate_on) ? 1 : 0;
    return CX_OK;
}

void wa_cx_set_delegate_queue(cx_box *b, int qid) { if (b) b->delegate_q = qid; }
void wa_cx_set_observer(cx_box *b, int on, int qid) {
    if (!b) return;
    b->observer = on ? 1 : 0;
    b->observer_q = qid;
}
void wa_cx_set_config(cx_box *b, const cx_cfg *cfg) {
    if (!b || !cfg) return;
    b->cfg = *cfg;
    b->cfg.on = 1;
    if (b->cfg.max_groups < 0) b->cfg.max_groups = 0;
    if (b->cfg.max_per < 0) b->cfg.max_per = 0;
}

int wa_cx_handle_equal(int t1, const char *v1, int t2, const char *v2) {
    if (t1 != t2) return 0;
    if (!v1) v1 = "";
    if (!v2) v2 = "";
    return strcmp(v1, v2) == 0;
}

int wa_cx_cap_on(int flag) { return flag == 1; }

unsigned wa_cx_handle_hash(int type, const char *value) {
    unsigned h = 2166136261u ^ (unsigned)type;
    const unsigned char *p = (const unsigned char *)(value ? value : "");
    for (; *p; p++) h = (h ^ *p) * 16777619u;
    return h;
}

static int live_in_group(const cx_box *b, int g) {
    int i, n = 0;
    for (i = 0; i < b->ncalls; i++)
        if (b->calls[i].alive && !b->calls[i].has_ended && b->calls[i].group == g) n++;
    for (i = 0; i < b->nacts; i++)
        if (b->acts[i].state == CX_PENDING && b->acts[i].kind == CX_START && b->acts[i].group == g) n++;
    return n;
}

static int group_known(const cx_box *b, int g, int upto_act) {
    int i;
    for (i = 0; i < b->ncalls; i++)
        if (b->calls[i].alive && !b->calls[i].has_ended && b->calls[i].group == g) return 1;
    for (i = 0; i < upto_act; i++)
        if (b->acts[i].state == CX_PENDING && b->acts[i].kind == CX_START && b->acts[i].group == g) return 1;
    return 0;
}

static int count_groups(const cx_box *b) {
    int i, n = 0;
    for (i = 0; i < b->ncalls; i++) {
        int g, seen = 0, j;
        if (!b->calls[i].alive || b->calls[i].has_ended) continue;
        g = b->calls[i].group;
        for (j = 0; j < i; j++)
            if (b->calls[j].alive && !b->calls[j].has_ended && b->calls[j].group == g) seen = 1;
        if (!seen) n++;
    }
    for (i = 0; i < b->nacts; i++) {
        if (b->acts[i].state != CX_PENDING || b->acts[i].kind != CX_START) continue;
        if (!group_known(b, b->acts[i].group, i)) n++;
    }
    return n;
}

static int pick_group(cx_box *b) {
    int i, best = 0, bestn = -1;
    if (!b->cfg.on) {
        b->next_group++;
        return b->next_group;
    }
    for (i = 0; i < b->ncalls; i++) {
        int g, n;
        if (!b->calls[i].alive || b->calls[i].has_ended) continue;
        g = b->calls[i].group;
        n = live_in_group(b, g);
        if (n < b->cfg.max_per && n > bestn) { best = g; bestn = n; }
    }
    for (i = 0; i < b->nacts; i++) {
        int g, n;
        if (b->acts[i].state != CX_PENDING || b->acts[i].kind != CX_START) continue;
        g = b->acts[i].group;
        n = live_in_group(b, g);
        if (n < b->cfg.max_per && n > bestn) { best = g; bestn = n; }
    }
    if (best) return best;
    if (count_groups(b) >= b->cfg.max_groups) return 0;
    b->next_group++;
    return b->next_group;
}

static void enqueue(cx_box *b, int qid, const cx_ev *ev) {
    cx_ev *d;
    if (!b || b->nq >= 32) return;
    d = &b->q[b->nq++];
    *d = *ev;
    d->qid = qid;
}

static void push_action(cx_box *b, int kind, const char *uuid) {
    cx_ev e;
    memset(&e, 0, sizeof e);
    e.what = CX_EV_ACTION;
    e.action = kind;
    copy_s(e.uuid, sizeof e.uuid, uuid);
    enqueue(b, b->delegate_q, &e);
}

static void push_changed(cx_box *b, const cx_call *c) {
    cx_ev e;
    if (!b->observer) return;
    memset(&e, 0, sizeof e);
    e.what = CX_EV_CHANGED;
    e.outgoing = c->outgoing;
    e.on_hold = c->on_hold;
    e.connected = c->has_connected;
    e.ended = c->has_ended;
    copy_s(e.uuid, sizeof e.uuid, c->uuid);
    enqueue(b, b->observer_q, &e);
}

int wa_cx_submit_full(cx_box *b, int kind, const char *uuid, const char *handle, int handle_type, int muted, int held, int video, int *idx) {
    cx_act *a;
    int group = 0;
    if (idx) *idx = -1;
    if (!b || kind < CX_START || kind > CX_MUTE || !uuid || !uuid[0]) return CX_ERR_BAD;
    if (!b->provider) return CX_ERR_NO_PROVIDER;
    if (b->cfg.on && video && !b->cfg.video) return CX_ERR_VIDEO;
    if (b->cfg.on && handle_type && !(b->cfg.handle_mask & (1u << handle_type))) return CX_ERR_HANDLE;
    if (kind == CX_START) {
        if (b->cfg.on && b->cfg.max_per < 1) return CX_ERR_LIMIT;
        group = pick_group(b);
        if (b->cfg.on && !group) return CX_ERR_LIMIT;
    }
    if (b->nacts >= 16) return CX_ERR_BAD;
    a = &b->acts[b->nacts];
    memset(a, 0, sizeof *a);
    a->kind = kind;
    a->state = CX_PENDING;
    a->muted = muted;
    a->held = held;
    a->video = video;
    a->handle_type = handle_type;
    a->group = group;
    copy_s(a->uuid, sizeof a->uuid, uuid);
    copy_s(a->handle, sizeof a->handle, handle);
    if (idx) *idx = b->nacts;
    b->nacts++;
    push_action(b, kind, uuid);
    return CX_OK;
}

int wa_cx_submit(cx_box *b, int kind, const char *uuid, const char *handle, int muted, int held, int *idx) {
    return wa_cx_submit_full(b, kind, uuid, handle, 0, muted, held, 0, idx);
}

static cx_call *find_mut(cx_box *b, const char *uuid) {
    int i;
    for (i = 0; i < b->ncalls; i++)
        if (b->calls[i].alive && !strcmp(b->calls[i].uuid, uuid)) return &b->calls[i];
    return 0;
}

const cx_call *wa_cx_find_call(const cx_box *b, const char *uuid) {
    int i;
    if (!b || !uuid) return 0;
    for (i = 0; i < b->ncalls; i++)
        if (b->calls[i].alive && !strcmp(b->calls[i].uuid, uuid)) return &b->calls[i];
    return 0;
}

int wa_cx_fulfill(cx_box *b, int idx) {
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
            c->handle_type = a->handle_type;
            c->group = a->group;
        }
        if (c) {
            c->alive = CX_CALL_ACTIVE;
            c->outgoing = 1;
            c->has_connected = 1;
            c->has_ended = 0;
            c->has_video = a->video;
            push_changed(b, c);
        }
    } else if (a->kind == CX_ANSWER) {
        if (c) {
            c->alive = CX_CALL_ACTIVE;
            c->has_connected = 1;
            c->has_ended = 0;
            push_changed(b, c);
        }
    } else if (a->kind == CX_END) {
        if (c) {
            c->alive = CX_CALL_ENDED;
            c->has_ended = 1;
            push_changed(b, c);
        }
    } else if (a->kind == CX_HOLD) {
        if (c) {
            c->held = a->held ? 1 : 0;
            c->on_hold = c->held;
            push_changed(b, c);
        }
    } else if (a->kind == CX_MUTE) {
        if (c) c->muted = a->muted ? 1 : 0;
    }
    return CX_OK;
}

int wa_cx_fail(cx_box *b, int idx) {
    if (!b || idx < 0 || idx >= b->nacts) return CX_ERR_BAD;
    if (b->acts[idx].state != CX_PENDING) return CX_ERR_BAD;
    b->acts[idx].state = CX_FAILED;
    return CX_OK;
}

int wa_cx_report_incoming(cx_box *b, const char *uuid, const char *handle) {
    cx_call *c;
    int group;
    if (!b || !b->provider || !uuid || !uuid[0]) return CX_ERR_BAD;
    if (b->cfg.on && b->cfg.max_per < 1) return CX_ERR_LIMIT;
    group = pick_group(b);
    if (b->cfg.on && !group) return CX_ERR_LIMIT;
    if (b->ncalls >= 16) return CX_ERR_BAD;
    c = &b->calls[b->ncalls++];
    memset(c, 0, sizeof *c);
    c->alive = CX_CALL_RINGING;
    c->outgoing = 0;
    c->group = group;
    copy_s(c->uuid, sizeof c->uuid, uuid);
    copy_s(c->handle, sizeof c->handle, handle);
    push_changed(b, c);
    return CX_OK;
}

int wa_cx_apply_update(cx_box *b, const char *uuid, const cx_update *u) {
    cx_call *c;
    if (!b || !u || !uuid || !uuid[0]) return CX_ERR_BAD;
    c = find_mut(b, uuid);
    if (!c) return CX_ERR_BAD;
    if (u->has_video == 1 && b->cfg.on && !b->cfg.video) return CX_ERR_VIDEO;
    if (u->has_handle) {
        if (b->cfg.on && u->handle_type && !(b->cfg.handle_mask & (1u << u->handle_type))) return CX_ERR_HANDLE;
        c->handle_type = u->handle_type;
        copy_s(c->handle, sizeof c->handle, u->handle);
    }
    if (u->has_name) copy_s(c->name, sizeof c->name, u->name);
    if (u->has_video >= 0) c->has_video = u->has_video ? 1 : 0;
    if (u->hold >= 0) c->cap_hold = u->hold ? 1 : 0;
    if (u->group >= 0) c->cap_group = u->group ? 1 : 0;
    if (u->ungroup >= 0) c->cap_ungroup = u->ungroup ? 1 : 0;
    if (u->dtmf >= 0) c->cap_dtmf = u->dtmf ? 1 : 0;
    push_changed(b, c);
    return CX_OK;
}

int wa_cx_call_count(const cx_box *b) {
    int i, n = 0;
    if (!b) return 0;
    for (i = 0; i < b->ncalls; i++) if (b->calls[i].alive) n++;
    return n;
}

const cx_call *wa_cx_call_at(const cx_box *b, int index) {
    int i, n = 0;
    if (!b || index < 0) return 0;
    for (i = 0; i < b->ncalls; i++) {
        if (!b->calls[i].alive) continue;
        if (n == index) return &b->calls[i];
        n++;
    }
    return 0;
}

int wa_cx_drain(cx_box *b, int qid, cx_ev *out, int cap) {
    int i, w = 0, n = 0;
    if (!b) return 0;
    for (i = 0; i < b->nq; i++) {
        if (b->q[i].qid != qid) {
            if (w != i) b->q[w] = b->q[i];
            w++;
            continue;
        }
        if (out && n < cap) out[n] = b->q[i];
        n++;
    }
    b->nq = w;
    return n;
}

int wa_cl_auth(int tcc_state) {
    if (tcc_state == 2) return CL_AUTH_WHEN_IN_USE;
    if (tcc_state == 1) return CL_AUTH_DENIED;
    return CL_AUTH_NOT;
}

int wa_cl_error(int auth) {
    if (auth == CL_AUTH_WHEN_IN_USE) return CL_ERR_UNKNOWN;
    return CL_ERR_DENIED;
}

int wa_cl_coord_valid(double lat, double lon) {
    if (lat != lat || lon != lon) return 0;
    if (lat < -90.0 || lat > 90.0) return 0;
    if (lon < -180.0 || lon > 180.0) return 0;
    return 1;
}

void wa_bg_init(bg_box *b) { memset(b, 0, sizeof *b); }

static int bg_has(const bg_box *b, const char *ident) {
    int i;
    for (i = 0; i < b->nreg; i++)
        if (!strcmp(b->reg[i].id, ident)) return 1;
    return 0;
}

int wa_bg_register(bg_box *b, const char *ident) {
    if (!b || !ident || !ident[0] || strlen(ident) > 79) return -1;
    if (bg_has(b, ident)) return 0;
    if (b->nreg >= 8) return -1;
    memset(b->reg[b->nreg].id, 0, sizeof b->reg[b->nreg].id);
    memcpy(b->reg[b->nreg].id, ident, strlen(ident));
    b->nreg++;
    return 0;
}

int wa_bg_submit(bg_box *b, const char *ident, int kind, int *err) {
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

int wa_ct_providers(void) { return 0; }
const char *wa_ct_radio(void) { return 0; }
