/* mOS libSystem — OpenGL por software: depuração.
 *
 * KHR_debug (e os nomes ARB_debug_output/KHR): log de mensagens, callback, glDebugMessageControl com regras por
 * grupo, glPush/PopDebugGroup, rótulos de objetos (glObjectLabel, glObjectPtrLabel). Os erros do GL viram
 * mensagens da fonte API quando GL_DEBUG_OUTPUT está ligado.
 * EXT_debug_label (glLabelObjectEXT) usa os mesmos rótulos; EXT_debug_marker vira grupos e marcadores do KHR.
 * Os rótulos moram na tabela de nomes de cada tipo: apagar o objeto apaga o rótulo.
 */
#include "gl_int.h"
#include "gl_proto.h"

#define DBG_LOG 64          /* GL_MAX_DEBUG_LOGGED_MESSAGES */
#define DBG_MSG 1024        /* GL_MAX_DEBUG_MESSAGE_LENGTH (com o zero) */
#define DBG_DEPTH 64        /* GL_MAX_DEBUG_GROUP_STACK_DEPTH */
#define DBG_LABEL 256       /* GL_MAX_LABEL_LENGTH */

typedef struct { GLenum src, type, sev; u32 id; char *msg; int len; } dmsg_t;
/* regra do glDebugMessageControl: 0 = GL_DONT_CARE; com ids, vale só para eles */
typedef struct { GLenum src, type, sev; u32 id; int has_id, on; } drule_t;
typedef struct { drule_t *r; int n, cap; GLenum src; u32 id; char *msg; } dgroup_t;
typedef struct gl_dbg {
    GLDEBUGPROC cb;
    const void *user;
    dmsg_t log[DBG_LOG];
    int head, count;
    dgroup_t g[DBG_DEPTH];
    int depth;                  /* grupos empilhados além do padrão (g[0]) */
    int busy;                   /* mensagem em andamento: o callback que erra não entra em laço */
} dbg_t;

static dbg_t *dbg(gl_ctx_t *C) {
    if (!C->dbg) C->dbg = calloc(1, sizeof(dbg_t));
    return C->dbg;
}
void gl_debug_free(gl_ctx_t *C) {
    dbg_t *d = C->dbg;
    if (!d) return;
    for (int i = 0; i < d->count; i++) free(d->log[(d->head + i) % DBG_LOG].msg);
    for (int i = 0; i <= d->depth; i++) { free(d->g[i].r); free(d->g[i].msg); }
    free(d);
    C->dbg = 0;
}

static int enabled(dbg_t *d, GLenum src, GLenum type, u32 id, GLenum sev) {
    int on = sev != GL_DEBUG_SEVERITY_LOW;   /* padrão do KHR_debug */
    dgroup_t *g = &d->g[d->depth];
    for (int i = 0; i < g->n; i++) {
        drule_t *r = &g->r[i];
        if (r->src && r->src != src) continue;
        if (r->type && r->type != type) continue;
        if (r->has_id ? r->id != id : (r->sev && r->sev != sev)) continue;
        on = r->on;
    }
    return on;
}
static void emit(gl_ctx_t *C, GLenum src, GLenum type, u32 id, GLenum sev, int len, const char *msg) {
    if (!gl_is_on(C, GL_DEBUG_OUTPUT)) return;
    dbg_t *d = dbg(C);
    if (!d || d->busy || !enabled(d, src, type, id, sev)) return;
    if (len < 0) len = (int)strlen(msg);
    if (len > DBG_MSG - 1) len = DBG_MSG - 1;
    if (d->cb) {
        char *t = malloc((size_t)len + 1);
        if (!t) return;
        memcpy(t, msg, (size_t)len);
        t[len] = 0;
        d->busy = 1;
        d->cb(src, type, id, sev, len, t, d->user);
        d->busy = 0;
        free(t);
        return;
    }
    if (d->count == DBG_LOG) return;            /* log cheio: a mensagem nova é descartada */
    dmsg_t *m = &d->log[(d->head + d->count) % DBG_LOG];
    m->msg = malloc((size_t)len + 1);
    if (!m->msg) return;
    memcpy(m->msg, msg, (size_t)len);
    m->msg[len] = 0;
    m->len = len;
    m->src = src; m->type = type; m->id = id; m->sev = sev;
    d->count++;
}

static const char *err_name(GLenum e) {
    switch (e) {
    case GL_INVALID_ENUM: return "GL_INVALID_ENUM: enum fora do conjunto aceito";
    case GL_INVALID_VALUE: return "GL_INVALID_VALUE: valor numérico fora do intervalo";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION: operação não permitida no estado atual";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION: framebuffer incompleto";
    case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY: sem memória";
    case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW: pilha cheia";
    case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW: pilha vazia";
    }
    return "erro do GL";
}
void gl_debug_error(gl_ctx_t *C, GLenum e) {
    emit(C, GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, e, GL_DEBUG_SEVERITY_HIGH, -1, err_name(e));
}

/* ---- mensagens */
static int src_ok(GLenum s, int dc) {
    return (dc && s == GL_DONT_CARE) || s == GL_DEBUG_SOURCE_API || s == GL_DEBUG_SOURCE_WINDOW_SYSTEM || s == GL_DEBUG_SOURCE_SHADER_COMPILER ||
           s == GL_DEBUG_SOURCE_THIRD_PARTY || s == GL_DEBUG_SOURCE_APPLICATION || s == GL_DEBUG_SOURCE_OTHER;
}
static int type_ok(GLenum t, int dc) {
    return (dc && t == GL_DONT_CARE) || t == GL_DEBUG_TYPE_ERROR || t == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR || t == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR ||
           t == GL_DEBUG_TYPE_PORTABILITY || t == GL_DEBUG_TYPE_PERFORMANCE || t == GL_DEBUG_TYPE_OTHER || t == GL_DEBUG_TYPE_MARKER ||
           t == GL_DEBUG_TYPE_PUSH_GROUP || t == GL_DEBUG_TYPE_POP_GROUP;
}
static int sev_ok(GLenum s, int dc) {
    return (dc && s == GL_DONT_CARE) || s == GL_DEBUG_SEVERITY_HIGH || s == GL_DEBUG_SEVERITY_MEDIUM || s == GL_DEBUG_SEVERITY_LOW ||
           s == GL_DEBUG_SEVERITY_NOTIFICATION;
}
void glDebugMessageInsert(GLenum s, GLenum t, GLuint id, GLenum sev, GLsizei n, const GLchar *m) {
    GLC();
    if (!m) GLERR(GL_INVALID_VALUE, );
    if ((s != GL_DEBUG_SOURCE_APPLICATION && s != GL_DEBUG_SOURCE_THIRD_PARTY) || !type_ok(t, 0) || !sev_ok(sev, 0)) GLERR(GL_INVALID_ENUM, );
    int len = n < 0 ? (int)strlen(m) : n;
    if (len >= DBG_MSG) GLERR(GL_INVALID_VALUE, );
    emit(C, s, t, id, sev, len, m);
}
void glDebugMessageControl(GLenum s, GLenum t, GLenum sev, GLsizei n, const GLuint *ids, GLboolean en) {
    GLC();
    if (!src_ok(s, 1) || !type_ok(t, 1) || !sev_ok(sev, 1)) GLERR(GL_INVALID_ENUM, );
    if (n < 0) GLERR(GL_INVALID_VALUE, );
    if (n > 0 && (s == GL_DONT_CARE || t == GL_DONT_CARE || sev != GL_DONT_CARE)) GLERR(GL_INVALID_OPERATION, );
    dbg_t *d = dbg(C);
    if (!d) GLERR(GL_OUT_OF_MEMORY, );
    dgroup_t *g = &d->g[d->depth];
    int add = n > 0 ? n : 1;
    if (g->n + add > g->cap) {
        int nc = (g->n + add) * 2;
        drule_t *r = realloc(g->r, (size_t)nc * sizeof *r);
        if (!r) GLERR(GL_OUT_OF_MEMORY, );
        g->r = r;
        g->cap = nc;
    }
    if (n == 0 && s == GL_DONT_CARE && t == GL_DONT_CARE && sev == GL_DONT_CARE) g->n = 0;   /* regra geral substitui as anteriores */
    for (int i = 0; i < add; i++) {
        drule_t *r = &g->r[g->n++];
        r->src = s == GL_DONT_CARE ? 0 : s;
        r->type = t == GL_DONT_CARE ? 0 : t;
        r->sev = sev == GL_DONT_CARE ? 0 : sev;
        r->has_id = n > 0;
        r->id = n > 0 && ids ? ids[i] : 0;
        r->on = en != 0;
    }
}
void glDebugMessageCallback(GLDEBUGPROC cb, const void *user) {
    GLC();
    dbg_t *d = dbg(C);
    if (!d) GLERR(GL_OUT_OF_MEMORY, );
    d->cb = cb;
    d->user = user;
}
GLuint glGetDebugMessageLog(GLuint count, GLsizei bs, GLenum *s, GLenum *t, GLuint *ids, GLenum *sev, GLsizei *len, GLchar *log) {
    GLC(0);
    if (log && bs < 0) GLERR(GL_INVALID_VALUE, 0);
    dbg_t *d = C->dbg;
    if (!d) return 0;
    GLuint got = 0;
    GLsizei used = 0;
    while (got < count && d->count) {
        dmsg_t *m = &d->log[d->head];
        if (log) {
            if (used + m->len + 1 > bs) break;   /* a mensagem que não cabe fica no log */
            memcpy(log + used, m->msg, (size_t)m->len + 1);
            used += m->len + 1;
        }
        if (s) s[got] = m->src;
        if (t) t[got] = m->type;
        if (ids) ids[got] = m->id;
        if (sev) sev[got] = m->sev;
        if (len) len[got] = m->len + 1;
        free(m->msg);
        m->msg = 0;
        d->head = (d->head + 1) % DBG_LOG;
        d->count--;
        got++;
    }
    return got;
}

/* ---- grupos */
void glPushDebugGroup(GLenum s, GLuint id, GLsizei n, const GLchar *m) {
    GLC();
    if (s != GL_DEBUG_SOURCE_APPLICATION && s != GL_DEBUG_SOURCE_THIRD_PARTY) GLERR(GL_INVALID_ENUM, );
    if (!m) GLERR(GL_INVALID_VALUE, );
    int len = n < 0 ? (int)strlen(m) : n;
    if (len >= DBG_MSG) GLERR(GL_INVALID_VALUE, );
    dbg_t *d = dbg(C);
    if (!d) GLERR(GL_OUT_OF_MEMORY, );
    if (d->depth + 1 >= DBG_DEPTH) GLERR(GL_STACK_OVERFLOW, );
    dgroup_t *p = &d->g[d->depth], *g = &d->g[d->depth + 1];
    memset(g, 0, sizeof *g);
    if (p->n) {
        /* o grupo novo herda o controle de mensagens do atual */
        g->r = malloc((size_t)p->n * sizeof *g->r);
        if (!g->r) GLERR(GL_OUT_OF_MEMORY, );
        memcpy(g->r, p->r, (size_t)p->n * sizeof *g->r);
        g->n = g->cap = p->n;
    }
    g->src = s;
    g->id = id;
    g->msg = malloc((size_t)len + 1);
    if (g->msg) { memcpy(g->msg, m, (size_t)len); g->msg[len] = 0; }
    d->depth++;
    emit(C, s, GL_DEBUG_TYPE_PUSH_GROUP, id, GL_DEBUG_SEVERITY_NOTIFICATION, len, m);
}
void glPopDebugGroup(void) {
    GLC();
    dbg_t *d = C->dbg;
    if (!d || !d->depth) GLERR(GL_STACK_UNDERFLOW, );
    dgroup_t *g = &d->g[d->depth];
    d->depth--;
    /* a mensagem de saída usa o controle do grupo que volta a valer */
    emit(C, g->src, GL_DEBUG_TYPE_POP_GROUP, g->id, GL_DEBUG_SEVERITY_NOTIFICATION, -1, g->msg ? g->msg : "");
    free(g->r);
    free(g->msg);
    memset(g, 0, sizeof *g);
}

/* ---- rótulos */
extern gl_names_t *gl_ppl_names(void);
static gl_names_t *label_table(gl_ctx_t *C, GLenum id, int *shader_kind) {
    *shader_kind = 0;
    switch (id) {
    case GL_BUFFER: case GL_BUFFER_OBJECT_EXT: return &C->sh->bufs;
    case GL_SHADER: case GL_SHADER_OBJECT_EXT: *shader_kind = 1; return &C->sh->progs;
    case GL_PROGRAM: case GL_PROGRAM_OBJECT_EXT: *shader_kind = 2; return &C->sh->progs;
    case GL_VERTEX_ARRAY: case GL_VERTEX_ARRAY_OBJECT_EXT: return &C->vaos;
    case GL_QUERY: case GL_QUERY_OBJECT_EXT: return &C->queries;
    case GL_PROGRAM_PIPELINE: case GL_PROGRAM_PIPELINE_OBJECT_EXT: return gl_ppl_names();
    case GL_TRANSFORM_FEEDBACK: return &C->xfbs;
    case GL_SAMPLER: return &C->sh->samps;
    case GL_TEXTURE: return &C->sh->texs;
    case GL_RENDERBUFFER: return &C->sh->rbs;
    case GL_FRAMEBUFFER: return &C->fbos;
    case GL_DISPLAY_LIST: return &C->sh->lists;
    }
    return 0;
}
/* 1 = ok; o erro já foi marcado quando devolve 0 */
static int label_slot(gl_ctx_t *C, GLenum id, GLuint name, gl_names_t **out) {
    int kind;
    gl_names_t *t = label_table(C, id, &kind);
    if (!t) { gl_err(C, GL_INVALID_ENUM); return 0; }
    if (!gl_names_is(t, name) || !t->lab) { gl_err(C, GL_INVALID_VALUE); return 0; }
    if (kind) {
        int *k = gl_names_get(t, name);
        if (!k || *k != kind) { gl_err(C, GL_INVALID_VALUE); return 0; }
    }
    *out = t;
    return 1;
}
static void set_label(gl_ctx_t *C, char **slot, GLsizei n, const GLchar *l) {
    if (!l) { free(*slot); *slot = 0; return; }
    int len = n < 0 ? (int)strlen(l) : n;
    if (len >= DBG_LABEL) { gl_err(C, GL_INVALID_VALUE); return; }
    char *c = malloc((size_t)len + 1);
    if (!c) { gl_err(C, GL_OUT_OF_MEMORY); return; }
    memcpy(c, l, (size_t)len);
    c[len] = 0;
    free(*slot);
    *slot = c;
}
static void get_label(const char *lab, GLsizei bs, GLsizei *len, GLchar *out) {
    int n = lab ? (int)strlen(lab) : 0;
    if (out && bs > 0) {
        if (n > bs - 1) n = bs - 1;
        if (n > 0) memcpy(out, lab, (size_t)n);
        out[n] = 0;
    } else if (out) n = 0;
    if (len) *len = n;
}
void glObjectLabel(GLenum id, GLuint name, GLsizei n, const GLchar *l) {
    GLC();
    gl_names_t *t;
    if (!label_slot(C, id, name, &t)) return;
    set_label(C, &t->lab[name], n, l);
}
void glGetObjectLabel(GLenum id, GLuint name, GLsizei bs, GLsizei *len, GLchar *l) {
    GLC();
    if (bs < 0) GLERR(GL_INVALID_VALUE, );
    gl_names_t *t;
    if (!label_slot(C, id, name, &t)) return;
    get_label(t->lab[name], bs, len, l);
}
void glLabelObjectEXT(GLenum t, GLuint o, GLsizei n, const GLchar *l) {
    GLC();
    if (n < 0) GLERR(GL_INVALID_VALUE, );
    glObjectLabel(t, o, n == 0 ? -1 : n, l);   /* no EXT, comprimento 0 = texto terminado em zero */
}
void glGetObjectLabelEXT(GLenum t, GLuint o, GLsizei bs, GLsizei *len, GLchar *l) { glGetObjectLabel(t, o, bs, len, l); }

/* rótulos de sync: a chave é o ponteiro do objeto */
typedef struct plab { const void *p; char *l; struct plab *next; } plab_t;
static plab_t *g_plab;
static int g_plab_mx;
static void plab_lock(void) { while (__atomic_exchange_n(&g_plab_mx, 1, __ATOMIC_ACQUIRE)) {} }
static void plab_unlock(void) { __atomic_store_n(&g_plab_mx, 0, __ATOMIC_RELEASE); }
static int gl_sync_valid(const void *s) { return glIsSync((GLsync)s); }
void glObjectPtrLabel(const void *p, GLsizei n, const GLchar *l) {
    GLC();
    if (!gl_sync_valid(p)) GLERR(GL_INVALID_VALUE, );
    int len = l && n < 0 ? (int)strlen(l) : n;
    if (l && len >= DBG_LABEL) GLERR(GL_INVALID_VALUE, );
    plab_lock();
    plab_t **pp = &g_plab;
    while (*pp && (*pp)->p != p) pp = &(*pp)->next;
    if (!*pp && l) { *pp = calloc(1, sizeof **pp); if (*pp) (*pp)->p = p; }
    if (*pp) {
        set_label(C, &(*pp)->l, n, l);
        if (!(*pp)->l) { plab_t *x = *pp; *pp = x->next; free(x); }
    }
    plab_unlock();
}
void glGetObjectPtrLabel(const void *p, GLsizei bs, GLsizei *len, GLchar *l) {
    GLC();
    if (bs < 0) GLERR(GL_INVALID_VALUE, );
    if (!gl_sync_valid(p)) GLERR(GL_INVALID_VALUE, );
    plab_lock();
    plab_t *x = g_plab;
    while (x && x->p != p) x = x->next;
    get_label(x ? x->l : 0, bs, len, l);
    plab_unlock();
}
/* o glDeleteSync chama para o rótulo não passar a um sync novo no mesmo endereço */
void gl_debug_sync_deleted(const void *p) {
    plab_lock();
    for (plab_t **pp = &g_plab; *pp; pp = &(*pp)->next)
        if ((*pp)->p == p) { plab_t *x = *pp; *pp = x->next; free(x->l); free(x); break; }
    plab_unlock();
}

/* ---- EXT_debug_marker: grupos e marcadores do KHR com fonte APPLICATION */
void glPushGroupMarkerEXT(GLsizei n, const GLchar *m) {
    GLC();
    const char *s = m ? m : "";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, m ? (n > 0 ? n : (int)strlen(s)) : 0, s);
}
void glPopGroupMarkerEXT(void) {
    GLC();
    dbg_t *d = C->dbg;
    if (!d || !d->depth) return;   /* o EXT ignora o pop sem push */
    glPopDebugGroup();
}
void glInsertEventMarkerEXT(GLsizei n, const GLchar *m) {
    GLC();
    if (!m) return;
    int len = n > 0 ? n : (int)strlen(m);
    if (len >= DBG_MSG) len = DBG_MSG - 1;
    emit(C, GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, len, m);
}

/* ---- nomes ARB_debug_output e KHR (GLES) */
void glDebugMessageControlARB(GLenum s, GLenum t, GLenum sev, GLsizei n, const GLuint *ids, GLboolean en) { glDebugMessageControl(s, t, sev, n, ids, en); }
void glDebugMessageInsertARB(GLenum s, GLenum t, GLuint id, GLenum sev, GLsizei n, const GLchar *m) { glDebugMessageInsert(s, t, id, sev, n, m); }
void glDebugMessageCallbackARB(GLDEBUGPROC cb, const void *user) { glDebugMessageCallback(cb, user); }
GLuint glGetDebugMessageLogARB(GLuint c, GLsizei bs, GLenum *s, GLenum *t, GLuint *ids, GLenum *sev, GLsizei *len, GLchar *log) { return glGetDebugMessageLog(c, bs, s, t, ids, sev, len, log); }
void glDebugMessageControlKHR(GLenum s, GLenum t, GLenum sev, GLsizei n, const GLuint *ids, GLboolean en) { glDebugMessageControl(s, t, sev, n, ids, en); }
void glDebugMessageInsertKHR(GLenum s, GLenum t, GLuint id, GLenum sev, GLsizei n, const GLchar *m) { glDebugMessageInsert(s, t, id, sev, n, m); }
void glDebugMessageCallbackKHR(GLDEBUGPROC cb, const void *user) { glDebugMessageCallback(cb, user); }
GLuint glGetDebugMessageLogKHR(GLuint c, GLsizei bs, GLenum *s, GLenum *t, GLuint *ids, GLenum *sev, GLsizei *len, GLchar *log) { return glGetDebugMessageLog(c, bs, s, t, ids, sev, len, log); }
void glPushDebugGroupKHR(GLenum s, GLuint id, GLsizei n, const GLchar *m) { glPushDebugGroup(s, id, n, m); }
void glPopDebugGroupKHR(void) { glPopDebugGroup(); }
void glObjectLabelKHR(GLenum id, GLuint name, GLsizei n, const GLchar *l) { glObjectLabel(id, name, n, l); }
void glGetObjectLabelKHR(GLenum id, GLuint name, GLsizei bs, GLsizei *len, GLchar *l) { glGetObjectLabel(id, name, bs, len, l); }
void glObjectPtrLabelKHR(const void *p, GLsizei n, const GLchar *l) { glObjectPtrLabel(p, n, l); }
void glGetObjectPtrLabelKHR(const void *p, GLsizei bs, GLsizei *len, GLchar *l) { glGetObjectPtrLabel(p, bs, len, l); }

/* ---- glGet */
int gl_debug_get(gl_ctx_t *C, GLenum p, long long *v) {
    dbg_t *d = C->dbg;
    switch (p) {
    case GL_MAX_DEBUG_MESSAGE_LENGTH: *v = DBG_MSG; return 1;
    case GL_MAX_DEBUG_LOGGED_MESSAGES: *v = DBG_LOG; return 1;
    case GL_MAX_DEBUG_GROUP_STACK_DEPTH: *v = DBG_DEPTH; return 1;
    case GL_MAX_LABEL_LENGTH: *v = DBG_LABEL; return 1;
    case GL_DEBUG_LOGGED_MESSAGES: *v = d ? d->count : 0; return 1;
    case GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH: *v = d && d->count ? d->log[d->head].len + 1 : 0; return 1;
    case GL_DEBUG_GROUP_STACK_DEPTH: *v = (d ? d->depth : 0) + 1; return 1;
    }
    return 0;
}
int gl_debug_getp(gl_ctx_t *C, GLenum p, void **out) {
    dbg_t *d = C->dbg;
    if (p == GL_DEBUG_CALLBACK_FUNCTION) { *out = d ? (void *)d->cb : 0; return 1; }
    if (p == GL_DEBUG_CALLBACK_USER_PARAM) { *out = d ? (void *)d->user : 0; return 1; }
    return 0;
}
