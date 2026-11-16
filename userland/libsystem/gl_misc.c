/*
 * mOS libSystem — OpenGL por software: escolha do estágio (GLSL ou fixed function), queries
 * (occlusion, primitivos, transform feedback, tempo), sync (ARB_sync e APPLE_fence), transform
 * feedback (objetos, begin/end/pause), renderização condicional e glGetInternalformativ.
 * O desenho é síncrono: quando a chamada volta o resultado já está na memória, então fences e
 * queries ficam disponíveis na hora.
 */
#include "gl_int.h"
#include "gl_proto.h"

int clock_gettime(int clk, void *ts);
static u64 now_ns(void) { struct { long s, ns; } t = {0, 0}; clock_gettime(6, &t); return (u64)t.s * 1000000000ull + (u64)t.ns; }

int gl_stage_setup(gl_ctx_t *C) {
    if (C->prog) {
        if (!C->prog->linked) { gl_err(C, GL_INVALID_OPERATION); return 0; }
        gl_glsl_stage(C);
        return C->st.vs != 0;
    }
    if (C->profile_core) { gl_err(C, GL_INVALID_OPERATION); return 0; }   /* core sem programa não desenha */
    gl_ffp_stage(C);
    return 1;
}

/* ================================================================ queries */
static int qslot(GLenum t) {
    switch (t) {
    case GL_SAMPLES_PASSED: return 0;
    case GL_ANY_SAMPLES_PASSED: return 1;
    case GL_PRIMITIVES_GENERATED: return 2;
    case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN: return 3;
    case GL_TIME_ELAPSED: return 4;
    }
    return -1;
}
static u64 counter(gl_ctx_t *C, GLenum t) {
    switch (t) {
    case GL_SAMPLES_PASSED: case GL_ANY_SAMPLES_PASSED: return C->samples_passed;
    case GL_PRIMITIVES_GENERATED: return C->prims_generated;
    case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN: return C->prims_written;
    case GL_TIME_ELAPSED: return now_ns();
    }
    return 0;
}
void glGenQueries(GLsizei n, GLuint *out) { GLC(); if (n < 0) GLERR(GL_INVALID_VALUE, ); for (int i = 0; i < n; i++) out[i] = gl_names_gen(&C->queries); }
void glDeleteQueries(GLsizei n, const GLuint *ids) {
    GLC();
    for (int i = 0; i < n; i++) {
        gl_query_t *q = gl_names_get(&C->queries, ids[i]);
        if (q) for (int k = 0; k < 8; k++) if (C->q_active[k] == q) C->q_active[k] = 0;
        gl_names_del(&C->queries, ids[i]);
        free(q);
    }
}
GLboolean glIsQuery(GLuint id) { GLC(0); return gl_names_get(&C->queries, id) != 0; }
void glBeginQuery(GLenum target, GLuint id) {
    GLC();
    int s = qslot(target);
    if (s < 0) GLERR(GL_INVALID_ENUM, );
    if (!id || C->q_active[s]) GLERR(GL_INVALID_OPERATION, );
    if (!gl_names_is(&C->queries, id)) GLERR(GL_INVALID_OPERATION, );
    gl_query_t *q = gl_names_get(&C->queries, id);
    if (!q) { q = calloc(1, sizeof *q); if (!q) GLERR(GL_OUT_OF_MEMORY, ); q->name = id; gl_names_set(&C->queries, id, q); }
    if (q->target && q->target != target) GLERR(GL_INVALID_OPERATION, );
    q->target = target;
    q->active = 1;
    q->avail = 0;
    q->result = counter(C, target);   /* valor inicial; o fim subtrai */
    C->q_active[s] = q;
}
void glEndQuery(GLenum target) {
    GLC();
    int s = qslot(target);
    if (s < 0) GLERR(GL_INVALID_ENUM, );
    gl_query_t *q = C->q_active[s];
    if (!q) GLERR(GL_INVALID_OPERATION, );
    u64 v = counter(C, target) - q->result;
    if (target == GL_ANY_SAMPLES_PASSED) v = v != 0;
    q->result = v;
    q->active = 0;
    q->avail = 1;
    C->q_active[s] = 0;
}
void glQueryCounter(GLuint id, GLenum target) {
    GLC();
    if (target != GL_TIMESTAMP) GLERR(GL_INVALID_ENUM, );
    if (!gl_names_is(&C->queries, id)) GLERR(GL_INVALID_OPERATION, );
    gl_query_t *q = gl_names_get(&C->queries, id);
    if (!q) { q = calloc(1, sizeof *q); if (!q) GLERR(GL_OUT_OF_MEMORY, ); q->name = id; gl_names_set(&C->queries, id, q); }
    q->target = GL_TIMESTAMP;
    q->result = now_ns();
    q->avail = 1;
}
void glGetQueryiv(GLenum target, GLenum p, GLint *v) {
    GLC();
    if (target == GL_TIMESTAMP) { if (p == GL_QUERY_COUNTER_BITS) { *v = 64; return; } if (p == GL_CURRENT_QUERY) { *v = 0; return; } GLERR(GL_INVALID_ENUM, ); }
    int s = qslot(target);
    if (s < 0) GLERR(GL_INVALID_ENUM, );
    if (p == GL_CURRENT_QUERY) { *v = C->q_active[s] ? (GLint)C->q_active[s]->name : 0; return; }
    if (p == GL_QUERY_COUNTER_BITS) { *v = target == GL_TIME_ELAPSED ? 64 : 32; return; }
    GLERR(GL_INVALID_ENUM, );
}
static gl_query_t *qobj(gl_ctx_t *C, GLuint id) {
    gl_query_t *q = gl_names_get(&C->queries, id);
    if (!q || q->active) { gl_err(C, GL_INVALID_OPERATION); return 0; }
    return q;
}
static u64 qres(gl_ctx_t *C, GLuint id, GLenum p, int *ok) {
    gl_query_t *q = qobj(C, id);
    *ok = q != 0;
    if (!q) return 0;
    if (p == GL_QUERY_RESULT_AVAILABLE) return q->avail;
    if (p == GL_QUERY_RESULT) return q->result;
    *ok = 0;
    gl_err(C, GL_INVALID_ENUM);
    return 0;
}
void glGetQueryObjectiv(GLuint id, GLenum p, GLint *v) { GLC(); int ok; u64 r = qres(C, id, p, &ok); if (ok) *v = r > 0x7fffffff ? 0x7fffffff : (GLint)r; }
void glGetQueryObjectuiv(GLuint id, GLenum p, GLuint *v) { GLC(); int ok; u64 r = qres(C, id, p, &ok); if (ok) *v = r > 0xffffffffu ? 0xffffffffu : (GLuint)r; }
void glGetQueryObjecti64v(GLuint id, GLenum p, GLint64 *v) { GLC(); int ok; u64 r = qres(C, id, p, &ok); if (ok) *v = (GLint64)r; }
void glGetQueryObjectui64v(GLuint id, GLenum p, GLuint64 *v) { GLC(); int ok; u64 r = qres(C, id, p, &ok); if (ok) *v = r; }
void glGenQueriesARB(GLsizei n, GLuint *ids) { glGenQueries(n, ids); }
void glDeleteQueriesARB(GLsizei n, const GLuint *ids) { glDeleteQueries(n, ids); }
GLboolean glIsQueryARB(GLuint id) { return glIsQuery(id); }
void glBeginQueryARB(GLenum t, GLuint id) { glBeginQuery(t, id); }
void glEndQueryARB(GLenum t) { glEndQuery(t); }
void glGetQueryivARB(GLenum t, GLenum p, GLint *v) { glGetQueryiv(t, p, v); }
void glGetQueryObjectivARB(GLuint id, GLenum p, GLint *v) { glGetQueryObjectiv(id, p, v); }
void glGetQueryObjectuivARB(GLuint id, GLenum p, GLuint *v) { glGetQueryObjectuiv(id, p, v); }
void glBeginQueryIndexed(GLenum t, GLuint idx, GLuint id) { GLC(); if (idx) GLERR(GL_INVALID_VALUE, ); glBeginQuery(t, id); }
void glEndQueryIndexed(GLenum t, GLuint idx) { GLC(); if (idx) GLERR(GL_INVALID_VALUE, ); glEndQuery(t); }
void glGetQueryIndexediv(GLenum t, GLuint idx, GLenum p, GLint *v) { GLC(); if (idx) GLERR(GL_INVALID_VALUE, ); glGetQueryiv(t, p, v); }
/* renderização condicional: o resultado já existe, então a decisão é exata */
static int g_cond_skip;
void glBeginConditionalRender(GLuint id, GLenum mode) {
    GLC();
    (void)mode;
    gl_query_t *q = qobj(C, id);
    if (!q) return;
    g_cond_skip = q->avail && q->result == 0;
}
void glEndConditionalRender(void) { g_cond_skip = 0; }
void glBeginConditionalRenderNV(GLuint id, GLenum mode) { glBeginConditionalRender(id, mode); }
void glEndConditionalRenderNV(void) { glEndConditionalRender(); }
int gl_cond_skip(void) { return g_cond_skip; }

/* ================================================================ sync */
#define SYNC_MAGIC 0x53594e43u
GLsync glFenceSync(GLenum cond, GLbitfield flags) {
    GLC(0);
    if (cond != GL_SYNC_GPU_COMMANDS_COMPLETE) GLERR(GL_INVALID_ENUM, 0);
    if (flags) GLERR(GL_INVALID_VALUE, 0);
    gl_sync_t *s = calloc(1, sizeof *s);
    if (!s) GLERR(GL_OUT_OF_MEMORY, 0);
    s->magic = SYNC_MAGIC;
    s->signaled = 1;
    return (GLsync)s;
}
static gl_sync_t *sync_of(GLsync s) { gl_sync_t *x = (gl_sync_t *)s; return x && x->magic == SYNC_MAGIC ? x : 0; }
GLboolean glIsSync(GLsync s) { return sync_of(s) != 0; }
void glDeleteSync(GLsync s) { GLC(); if (!s) return; gl_sync_t *x = sync_of(s); if (!x) GLERR(GL_INVALID_VALUE, ); x->magic = 0; free(x); }
GLenum glClientWaitSync(GLsync s, GLbitfield flags, GLuint64 timeout) {
    GLC(GL_WAIT_FAILED);
    (void)timeout;
    if (flags & ~GL_SYNC_FLUSH_COMMANDS_BIT) GLERR(GL_INVALID_VALUE, GL_WAIT_FAILED);
    if (!sync_of(s)) GLERR(GL_INVALID_VALUE, GL_WAIT_FAILED);
    return GL_ALREADY_SIGNALED;
}
void glWaitSync(GLsync s, GLbitfield flags, GLuint64 timeout) {
    GLC();
    if (flags || timeout != GL_TIMEOUT_IGNORED) GLERR(GL_INVALID_VALUE, );
    if (!sync_of(s)) GLERR(GL_INVALID_VALUE, );
}
void glGetSynciv(GLsync s, GLenum p, GLsizei bs, GLsizei *len, GLint *v) {
    GLC();
    if (!sync_of(s)) GLERR(GL_INVALID_VALUE, );
    GLint r;
    switch (p) {
    case GL_OBJECT_TYPE: r = GL_SYNC_FENCE; break;
    case GL_SYNC_STATUS: r = GL_SIGNALED; break;
    case GL_SYNC_CONDITION: r = GL_SYNC_GPU_COMMANDS_COMPLETE; break;
    case GL_SYNC_FLAGS: r = 0; break;
    default: GLERR(GL_INVALID_ENUM, );
    }
    if (bs > 0 && v) v[0] = r;
    if (len) *len = 1;
}
/* APPLE_fence: nomes simples; tudo termina na hora */
static gl_names_t g_fences;
void glGenFencesAPPLE(GLsizei n, GLuint *f) { for (int i = 0; i < n; i++) f[i] = gl_names_gen(&g_fences); }
void glDeleteFencesAPPLE(GLsizei n, const GLuint *f) { for (int i = 0; i < n; i++) gl_names_del(&g_fences, f[i]); }
void glSetFenceAPPLE(GLuint f) { gl_names_reserve(&g_fences, f); }
GLboolean glIsFenceAPPLE(GLuint f) { return (GLboolean)gl_names_is(&g_fences, f); }
GLboolean glTestFenceAPPLE(GLuint f) { (void)f; return 1; }
void glFinishFenceAPPLE(GLuint f) { (void)f; }
GLboolean glTestObjectAPPLE(GLenum o, GLuint n) { (void)o; (void)n; return 1; }
void glFinishObjectAPPLE(GLenum o, GLint n) { (void)o; (void)n; }

/* ================================================================ transform feedback */
void glGenTransformFeedbacks(GLsizei n, GLuint *out) { GLC(); if (n < 0) GLERR(GL_INVALID_VALUE, ); for (int i = 0; i < n; i++) out[i] = gl_names_gen(&C->xfbs); }
void glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids) {
    GLC();
    for (int i = 0; i < n; i++) {
        gl_xfb_t *x = gl_names_get(&C->xfbs, ids[i]);
        if (x && x->active) GLERR(GL_INVALID_OPERATION, );
        if (x == C->xfb) C->xfb = &C->xfb0;
        gl_names_del(&C->xfbs, ids[i]);
        free(x);
    }
}
GLboolean glIsTransformFeedback(GLuint id) { GLC(0); return gl_names_get(&C->xfbs, id) != 0; }
void glBindTransformFeedback(GLenum target, GLuint id) {
    GLC();
    if (target != GL_TRANSFORM_FEEDBACK) GLERR(GL_INVALID_ENUM, );
    if (C->xfb->active && !C->xfb->paused) GLERR(GL_INVALID_OPERATION, );
    if (!id) { C->xfb = &C->xfb0; return; }
    if (!gl_names_is(&C->xfbs, id)) GLERR(GL_INVALID_OPERATION, );
    gl_xfb_t *x = gl_names_get(&C->xfbs, id);
    if (!x) { x = calloc(1, sizeof *x); if (!x) GLERR(GL_OUT_OF_MEMORY, ); x->name = id; gl_names_set(&C->xfbs, id, x); }
    C->xfb = x;
}
void glBeginTransformFeedback(GLenum prim) {
    GLC();
    if (prim != GL_POINTS && prim != GL_LINES && prim != GL_TRIANGLES) GLERR(GL_INVALID_ENUM, );
    if (C->xfb->active) GLERR(GL_INVALID_OPERATION, );
    if (!C->prog || !C->prog->nxfb) GLERR(GL_INVALID_OPERATION, );
    C->xfb->active = 1;
    C->xfb->paused = 0;
    C->xfb->prim = prim;
    C->xfb->written = 0;
}
void glEndTransformFeedback(void) { GLC(); if (!C->xfb->active) GLERR(GL_INVALID_OPERATION, ); C->xfb->active = 0; C->xfb->paused = 0; }
void glPauseTransformFeedback(void) { GLC(); if (!C->xfb->active || C->xfb->paused) GLERR(GL_INVALID_OPERATION, ); C->xfb->paused = 1; }
void glResumeTransformFeedback(void) { GLC(); if (!C->xfb->active || !C->xfb->paused) GLERR(GL_INVALID_OPERATION, ); C->xfb->paused = 0; }
void glBeginTransformFeedbackEXT(GLenum p) { glBeginTransformFeedback(p); }
void glEndTransformFeedbackEXT(void) { glEndTransformFeedback(); }
/* desenha o que o último transform feedback gravou neste objeto */
void glDrawTransformFeedback(GLenum mode, GLuint id) {
    GLC();
    gl_xfb_t *x = id ? gl_names_get(&C->xfbs, id) : &C->xfb0;
    if (!x) GLERR(GL_INVALID_VALUE, );
    gl_draw(C, mode, 0, (i64)x->written, 0, 0, 1, 0, 0);
}
void glDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream) { GLC(); if (stream) GLERR(GL_INVALID_VALUE, ); glDrawTransformFeedback(mode, id); }

/* ================================================================ diversos */
void glGetInternalformativ(GLenum target, GLenum ifmt, GLenum p, GLsizei bs, GLint *v) {
    GLC();
    (void)target;
    int ok;
    gl_ifmt_to_mtl(ifmt, &ok);
    if (bs <= 0 || !v) return;
    if (p == GL_NUM_SAMPLE_COUNTS) { v[0] = ok ? 4 : 0; return; }
    if (p == GL_SAMPLES) { static const GLint s[4] = {8, 4, 2, 1}; for (int i = 0; i < bs && i < 4; i++) v[i] = s[i]; return; }
    GLERR(GL_INVALID_ENUM, );
}
void glTextureBarrierNV(void) {}
void glMemoryBarrier(GLbitfield b) { (void)b; }
void glReleaseShaderCompiler(void) {}
void glClampColor(GLenum target, GLenum clamp) { (void)target; (void)clamp; }
void glClampColorARB(GLenum target, GLenum clamp) { (void)target; (void)clamp; }
/* uma amostra por pixel, no centro */
void glGetMultisamplefv(GLenum p, GLuint i, GLfloat *v) {
    GLC();
    if (p != GL_SAMPLE_POSITION) GLERR(GL_INVALID_ENUM, );
    if (i > 0) GLERR(GL_INVALID_VALUE, );
    v[0] = v[1] = 0.5f;
}
void glProgramParameteriARB(GLuint p, GLenum q, GLint v) { glProgramParameteri(p, q, v); }
void glProgramParameteriEXT(GLuint p, GLenum q, GLint v) { glProgramParameteri(p, q, v); }
