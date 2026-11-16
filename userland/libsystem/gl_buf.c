/*
 * mOS libSystem — OpenGL por software: buffers (VBO, EBO, UBO, PBO, TBO, transform feedback, cópia),
 * mapeamento, VAOs (inclusive GL_APPLE_vertex_array_object), atributos de vértice e os valores
 * correntes de glVertexAttrib*.
 * Um buffer vive enquanto tiver nome ou estiver ligado num VAO/textura: apagar o VBO depois de montar
 * o VAO é comum e o VAO continua desenhando.
 */
#include "gl_int.h"
#include "gl_proto.h"

/* ================================================================ referências */
void gl_buf_ref(gl_buf_t *b) { if (b) b->refs++; }
void gl_buf_unref(gl_buf_t *b) {
    if (!b || --b->refs > 0) return;
    free(b->data);
    free(b);
}
static void set_ref(gl_buf_t **slot, gl_buf_t *b) {
    if (*slot == b) return;
    gl_buf_ref(b);
    gl_buf_unref(*slot);
    *slot = b;
}

/* ponteiro de leitura de um atributo, índice ou pixel: buffer + deslocamento ou memória do cliente */
const u8 *gl_buf_ptr(gl_ctx_t *C, gl_buf_t *b, const void *p, i64 need) {
    (void)C;
    if (!b) return p;
    i64 off = (i64)(uintptr_t)p;
    if (!b->data || off < 0 || need < 0 || off + need > b->size) return 0;
    return b->data + off;
}

/* ================================================================ objetos */
static gl_buf_t *buf_get(gl_ctx_t *C, GLuint name, int create) {
    gl_buf_t *b = gl_names_get(&C->sh->bufs, name);
    if (!b && create && name) {
        b = calloc(1, sizeof *b);
        if (!b) return 0;
        b->name = name;
        b->usage = GL_STATIC_DRAW;
        b->refs = 1;   /* o nome */
        gl_names_set(&C->sh->bufs, name, b);
    }
    return b;
}
void glGenBuffers(GLsizei n, GLuint *out) {
    GLC();
    if (n < 0) GLERR(GL_INVALID_VALUE, );
    for (int i = 0; i < n; i++) out[i] = gl_names_gen(&C->sh->bufs);
}
static void unbind_everywhere(gl_ctx_t *C, gl_buf_t *b) {
    gl_buf_t **slots[] = {&C->b_array, &C->b_copy_r, &C->b_copy_w, &C->b_pack, &C->b_unpack, &C->b_ubo, &C->b_tbo, &C->b_xfb, &C->b_indirect};
    for (u32 i = 0; i < sizeof slots / sizeof *slots; i++) if (*slots[i] == b) *slots[i] = 0;
    for (int i = 0; i < GLX_UBO; i++) if (C->ubo[i] == b) C->ubo[i] = 0;
    for (int i = 0; i < GLX_XFB; i++) if (C->xfb->buf[i] == b) C->xfb->buf[i] = 0;
    /* no VAO ligado o ponto de ligação é desfeito (spec); nos outros VAOs a referência segura os dados */
    for (int i = 0; i < GLX_ATTR; i++) if (C->vao->a[i].buf == b) set_ref(&C->vao->a[i].buf, 0);
    if (C->vao->ebo == b) set_ref(&C->vao->ebo, 0);
    gl_attr_t *ca[] = {&C->vao->ca_vertex, &C->vao->ca_normal, &C->vao->ca_color, &C->vao->ca_color2, &C->vao->ca_fog, &C->vao->ca_index, &C->vao->ca_edge,
                       &C->vao->ca_tc[0], &C->vao->ca_tc[1], &C->vao->ca_tc[2], &C->vao->ca_tc[3], &C->vao->ca_tc[4], &C->vao->ca_tc[5], &C->vao->ca_tc[6], &C->vao->ca_tc[7]};
    for (u32 i = 0; i < sizeof ca / sizeof *ca; i++) if (ca[i]->buf == b) set_ref(&ca[i]->buf, 0);
}
void glDeleteBuffers(GLsizei n, const GLuint *names) {
    GLC();
    if (n < 0) GLERR(GL_INVALID_VALUE, );
    for (int i = 0; i < n; i++) {
        gl_buf_t *b = gl_names_get(&C->sh->bufs, names[i]);
        if (names[i] && gl_names_is(&C->sh->bufs, names[i])) gl_names_del(&C->sh->bufs, names[i]);
        if (!b) continue;
        unbind_everywhere(C, b);
        b->name = 0;
        gl_buf_unref(b);
    }
}
GLboolean glIsBuffer(GLuint name) { GLC(0); return gl_names_get(&C->sh->bufs, name) != 0; }
void glBindBuffer(GLenum target, GLuint name) {
    GLC();
    gl_buf_t **slot = gl_buf_slot(C, target);
    if (!slot) GLERR(GL_INVALID_ENUM, );
    if (name && !gl_names_is(&C->sh->bufs, name) && C->profile_core) GLERR(GL_INVALID_OPERATION, );
    gl_buf_t *b = name ? buf_get(C, name, 1) : 0;
    if (target == GL_ELEMENT_ARRAY_BUFFER) set_ref(&C->vao->ebo, b);
    else *slot = b;
}
void glBindBufferRange(GLenum target, GLuint idx, GLuint name, GLintptr off, GLsizeiptr size) {
    GLC();
    gl_buf_t *b = name ? buf_get(C, name, 1) : 0;
    if (name && (size <= 0 || off < 0)) GLERR(GL_INVALID_VALUE, );
    if (target == GL_UNIFORM_BUFFER) {
        if (idx >= GLX_UBO) GLERR(GL_INVALID_VALUE, );
        if (off % 16) GLERR(GL_INVALID_VALUE, );
        C->ubo[idx] = b; C->ubo_off[idx] = off; C->ubo_size[idx] = size; C->b_ubo = b;
    } else if (target == GL_TRANSFORM_FEEDBACK_BUFFER) {
        if (idx >= GLX_XFB) GLERR(GL_INVALID_VALUE, );
        if (off % 4) GLERR(GL_INVALID_VALUE, );
        C->xfb->buf[idx] = b; C->xfb->off[idx] = off; C->xfb->size[idx] = size; C->b_xfb = b;
    } else GLERR(GL_INVALID_ENUM, );
}
void glBindBufferBase(GLenum target, GLuint idx, GLuint name) {
    GLC();
    gl_buf_t *b = name ? buf_get(C, name, 1) : 0;
    if (target == GL_UNIFORM_BUFFER) {
        if (idx >= GLX_UBO) GLERR(GL_INVALID_VALUE, );
        C->ubo[idx] = b; C->ubo_off[idx] = 0; C->ubo_size[idx] = 0; C->b_ubo = b;
    } else if (target == GL_TRANSFORM_FEEDBACK_BUFFER) {
        if (idx >= GLX_XFB) GLERR(GL_INVALID_VALUE, );
        C->xfb->buf[idx] = b; C->xfb->off[idx] = 0; C->xfb->size[idx] = 0; C->b_xfb = b;
    } else GLERR(GL_INVALID_ENUM, );
}
void glBindBufferRangeEXT(GLenum t, GLuint i, GLuint b, GLintptr o, GLsizeiptr s) { glBindBufferRange(t, i, b, o, s); }
void glBindBufferBaseEXT(GLenum t, GLuint i, GLuint b) { glBindBufferBase(t, i, b); }
void glBindBufferOffsetEXT(GLenum t, GLuint i, GLuint b, GLintptr o) { glBindBufferRange(t, i, b, o, 0x7fffffff); }
static gl_buf_t *bound(gl_ctx_t *C, GLenum target) {
    gl_buf_t **slot = gl_buf_slot(C, target);
    if (!slot) { gl_err(C, GL_INVALID_ENUM); return 0; }
    if (!*slot) { gl_err(C, GL_INVALID_OPERATION); return 0; }
    return *slot;
}
static int usage_ok(GLenum u) { return u >= GL_STREAM_DRAW && u <= GL_DYNAMIC_COPY && u != 0x88E3 && u != 0x88E7 && u != 0x88EB; }
void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    GLC();
    if (size < 0) GLERR(GL_INVALID_VALUE, );
    if (!usage_ok(usage)) GLERR(GL_INVALID_ENUM, );
    gl_buf_t *b = bound(C, target);
    if (!b) return;
    if (b->mapped) b->mapped = 0;
    u8 *p = size ? malloc((size_t)size) : 0;
    if (size && !p) GLERR(GL_OUT_OF_MEMORY, );
    if (p) { if (data) memcpy(p, data, (size_t)size); else memset(p, 0, (size_t)size); }
    free(b->data);
    b->data = p;
    b->size = size;
    b->usage = usage;
}
void glBufferSubData(GLenum target, GLintptr off, GLsizeiptr size, const void *data) {
    GLC();
    gl_buf_t *b = bound(C, target);
    if (!b) return;
    if (off < 0 || size < 0 || off + size > b->size) GLERR(GL_INVALID_VALUE, );
    if (b->mapped) GLERR(GL_INVALID_OPERATION, );
    if (size && data) memcpy(b->data + off, data, (size_t)size);
}
void glGetBufferSubData(GLenum target, GLintptr off, GLsizeiptr size, void *data) {
    GLC();
    gl_buf_t *b = bound(C, target);
    if (!b) return;
    if (off < 0 || size < 0 || off + size > b->size) GLERR(GL_INVALID_VALUE, );
    if (size && data) memcpy(data, b->data + off, (size_t)size);
}
void *glMapBufferRange(GLenum target, GLintptr off, GLsizeiptr len, GLbitfield access) {
    GLC(0);
    gl_buf_t *b = bound(C, target);
    if (!b) return 0;
    if (off < 0 || len < 0 || off + len > b->size) GLERR(GL_INVALID_VALUE, 0);
    if (!(access & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT))) GLERR(GL_INVALID_OPERATION, 0);
    if ((access & GL_MAP_READ_BIT) && (access & (GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT))) GLERR(GL_INVALID_OPERATION, 0);
    if ((access & GL_MAP_FLUSH_EXPLICIT_BIT) && !(access & GL_MAP_WRITE_BIT)) GLERR(GL_INVALID_OPERATION, 0);
    if (b->mapped) GLERR(GL_INVALID_OPERATION, 0);
    b->mapped = 1;
    b->map_access = access;
    b->map_off = off;
    b->map_len = len;
    if (!b->data) { static u8 empty[16]; return empty; }
    return b->data + off;   /* a memória é a do próprio buffer: escrita aparece direto */
}
void *glMapBuffer(GLenum target, GLenum access) {
    GLC(0);
    GLbitfield a = access == GL_READ_ONLY ? GL_MAP_READ_BIT : access == GL_WRITE_ONLY ? GL_MAP_WRITE_BIT :
                   access == GL_READ_WRITE ? GL_MAP_READ_BIT | GL_MAP_WRITE_BIT : 0;
    if (!a) GLERR(GL_INVALID_ENUM, 0);
    gl_buf_t *b = bound(C, target);
    if (!b) return 0;
    return glMapBufferRange(target, 0, b->size, a);
}
GLboolean glUnmapBuffer(GLenum target) {
    GLC(0);
    gl_buf_t *b = bound(C, target);
    if (!b) return 0;
    if (!b->mapped) GLERR(GL_INVALID_OPERATION, 0);
    b->mapped = 0;
    b->map_access = 0;
    b->map_off = b->map_len = 0;
    return 1;
}
void glFlushMappedBufferRange(GLenum target, GLintptr off, GLsizeiptr len) {
    GLC();
    gl_buf_t *b = bound(C, target);
    if (!b) return;
    if (!b->mapped || !(b->map_access & GL_MAP_FLUSH_EXPLICIT_BIT)) GLERR(GL_INVALID_OPERATION, );
    if (off < 0 || len < 0 || off + len > b->map_len) GLERR(GL_INVALID_VALUE, );
}
void glBufferParameteriAPPLE(GLenum target, GLenum pname, GLint v) { (void)target; (void)pname; (void)v; }
void glFlushMappedBufferRangeAPPLE(GLenum target, GLintptr off, GLsizeiptr len) { (void)target; (void)off; (void)len; }
static i64 buf_param(gl_ctx_t *C, gl_buf_t *b, GLenum p) {
    switch (p) {
    case GL_BUFFER_SIZE: return b->size;
    case GL_BUFFER_USAGE: return b->usage;
    case GL_BUFFER_ACCESS: return (b->map_access & GL_MAP_READ_BIT) && (b->map_access & GL_MAP_WRITE_BIT) ? GL_READ_WRITE : (b->map_access & GL_MAP_READ_BIT) ? GL_READ_ONLY : GL_WRITE_ONLY;
    case GL_BUFFER_ACCESS_FLAGS: return b->map_access;
    case GL_BUFFER_MAPPED: return b->mapped;
    case GL_BUFFER_MAP_OFFSET: return b->map_off;
    case GL_BUFFER_MAP_LENGTH: return b->map_len;
    }
    gl_err(C, GL_INVALID_ENUM);
    return 0;
}
void glGetBufferParameteriv(GLenum target, GLenum p, GLint *v) { GLC(); gl_buf_t *b = bound(C, target); if (b && v) *v = (GLint)buf_param(C, b, p); }
void glGetBufferParameteri64v(GLenum target, GLenum p, GLint64 *v) { GLC(); gl_buf_t *b = bound(C, target); if (b && v) *v = buf_param(C, b, p); }
void glGetBufferPointerv(GLenum target, GLenum p, void **v) {
    GLC();
    if (p != GL_BUFFER_MAP_POINTER) GLERR(GL_INVALID_ENUM, );
    gl_buf_t *b = bound(C, target);
    if (b && v) *v = b->mapped && b->data ? b->data + b->map_off : 0;
}
void glCopyBufferSubData(GLenum rt, GLenum wt, GLintptr ro, GLintptr wo, GLsizeiptr size) {
    GLC();
    gl_buf_t *r = bound(C, rt), *w = bound(C, wt);
    if (!r || !w) return;
    if (ro < 0 || wo < 0 || size < 0 || ro + size > r->size || wo + size > w->size) GLERR(GL_INVALID_VALUE, );
    if (r == w && ((ro <= wo && wo < ro + size) || (wo <= ro && ro < wo + size))) GLERR(GL_INVALID_VALUE, );
    if (r->mapped || w->mapped) GLERR(GL_INVALID_OPERATION, );
    if (size) memmove(w->data + wo, r->data + ro, (size_t)size);
}
/* ARB_vertex_buffer_object */
void glGenBuffersARB(GLsizei n, GLuint *b) { glGenBuffers(n, b); }
void glDeleteBuffersARB(GLsizei n, const GLuint *b) { glDeleteBuffers(n, b); }
GLboolean glIsBufferARB(GLuint b) { return glIsBuffer(b); }
void glBindBufferARB(GLenum t, GLuint b) { glBindBuffer(t, b); }
void glBufferDataARB(GLenum t, GLsizeiptrARB s, const void *d, GLenum u) { glBufferData(t, s, d, u); }
void glBufferSubDataARB(GLenum t, GLintptrARB o, GLsizeiptrARB s, const void *d) { glBufferSubData(t, o, s, d); }
void glGetBufferSubDataARB(GLenum t, GLintptrARB o, GLsizeiptrARB s, void *d) { glGetBufferSubData(t, o, s, d); }
void *glMapBufferARB(GLenum t, GLenum a) { return glMapBuffer(t, a); }
GLboolean glUnmapBufferARB(GLenum t) { return glUnmapBuffer(t); }
void glGetBufferParameterivARB(GLenum t, GLenum p, GLint *v) { glGetBufferParameteriv(t, p, v); }
void glGetBufferPointervARB(GLenum t, GLenum p, void **v) { glGetBufferPointerv(t, p, v); }
/* APPLE_object_purgeable: o mOS nunca descarta o conteúdo */
GLenum glObjectPurgeableAPPLE(GLenum type, GLuint name, GLenum opt) { (void)type; (void)name; (void)opt; return GL_VOLATILE_APPLE; }
GLenum glObjectUnpurgeableAPPLE(GLenum type, GLuint name, GLenum opt) { (void)type; (void)name; (void)opt; return GL_RETAINED_APPLE; }
void glGetObjectParameterivAPPLE(GLenum type, GLuint name, GLenum p, GLint *v) { (void)type; (void)name; if (v) *v = p == GL_PURGEABLE_APPLE ? 0 : GL_RETAINED_APPLE; }
/* APPLE_vertex_array_range: a memória do cliente já é lida direto */
void glVertexArrayRangeAPPLE(GLsizei len, void *p) { (void)len; (void)p; }
void glFlushVertexArrayRangeAPPLE(GLsizei len, void *p) { (void)len; (void)p; }
void glVertexArrayParameteriAPPLE(GLenum p, GLint v) { (void)p; (void)v; }

/* ================================================================ VAO */
void glGenVertexArrays(GLsizei n, GLuint *out) {
    GLC();
    if (n < 0) GLERR(GL_INVALID_VALUE, );
    for (int i = 0; i < n; i++) out[i] = gl_names_gen(&C->vaos);
}
static void vao_free(gl_vao_t *v) {
    for (int i = 0; i < GLX_ATTR; i++) gl_buf_unref(v->a[i].buf);
    gl_attr_t *ca[] = {&v->ca_vertex, &v->ca_normal, &v->ca_color, &v->ca_color2, &v->ca_fog, &v->ca_index, &v->ca_edge};
    for (u32 i = 0; i < 7; i++) gl_buf_unref(ca[i]->buf);
    for (int i = 0; i < 8; i++) gl_buf_unref(v->ca_tc[i].buf);
    gl_buf_unref(v->ebo);
    free(v);
}
void glDeleteVertexArrays(GLsizei n, const GLuint *names) {
    GLC();
    if (n < 0) GLERR(GL_INVALID_VALUE, );
    for (int i = 0; i < n; i++) {
        if (!names[i]) continue;
        gl_vao_t *v = gl_names_get(&C->vaos, names[i]);
        if (v == C->vao) C->vao = &C->vao0;
        gl_names_del(&C->vaos, names[i]);
        if (v) vao_free(v);
    }
}
GLboolean glIsVertexArray(GLuint name) { GLC(0); return gl_names_get(&C->vaos, name) != 0; }
void glBindVertexArray(GLuint name) {
    GLC();
    if (!name) { C->vao = &C->vao0; return; }
    if (!gl_names_is(&C->vaos, name)) GLERR(GL_INVALID_OPERATION, );
    gl_vao_t *v = gl_names_get(&C->vaos, name);
    if (!v) {
        v = calloc(1, sizeof *v);
        if (!v) GLERR(GL_OUT_OF_MEMORY, );
        v->name = name;
        gl_vao_defaults(v);
        gl_names_set(&C->vaos, name, v);
    }
    C->vao = v;
}
void glGenVertexArraysAPPLE(GLsizei n, GLuint *a) { glGenVertexArrays(n, a); }
void glDeleteVertexArraysAPPLE(GLsizei n, const GLuint *a) { glDeleteVertexArrays(n, a); }
GLboolean glIsVertexArrayAPPLE(GLuint a) { return glIsVertexArray(a); }
void glBindVertexArrayAPPLE(GLuint a) {
    GLC();
    /* no APPLE o nome nasce no bind, mesmo sem glGenVertexArraysAPPLE */
    if (a && !gl_names_is(&C->vaos, a)) gl_names_reserve(&C->vaos, a);
    glBindVertexArray(a);
}

/* ================================================================ atributos */
static int vtype_ok(GLenum t, int integer) {
    switch (t) {
    case GL_BYTE: case GL_UNSIGNED_BYTE: case GL_SHORT: case GL_UNSIGNED_SHORT: case GL_INT: case GL_UNSIGNED_INT: return 1;
    case GL_FLOAT: case GL_DOUBLE: case GL_HALF_FLOAT: case GL_FIXED: case GL_INT_2_10_10_10_REV: case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT_10F_11F_11F_REV: return !integer;
    }
    return 0;
}
static void attr_set(gl_ctx_t *C, gl_attr_t *a, GLint size, GLenum type, int norm, int integer, GLsizei stride, const void *ptr) {
    gl_buf_t *b = C->b_array;
    a->size = size;
    a->type = type;
    a->normalized = norm;
    a->integer = integer;
    a->isdouble = 0;
    a->stride = stride;
    if (b) { a->off = (i64)(uintptr_t)ptr; a->ptr = ptr; }
    else { a->off = 0; a->ptr = ptr; }
    set_ref(&a->buf, b);
}
void glVertexAttribPointer(GLuint i, GLint size, GLenum type, GLboolean norm, GLsizei stride, const void *ptr) {
    GLC();
    if (i >= GLX_ATTR) GLERR(GL_INVALID_VALUE, );
    if ((size < 1 || size > 4) && size != GL_BGRA) GLERR(GL_INVALID_VALUE, );
    if (stride < 0) GLERR(GL_INVALID_VALUE, );
    if (!vtype_ok(type, 0)) GLERR(GL_INVALID_ENUM, );
    if (size == GL_BGRA && (type != GL_UNSIGNED_BYTE && type != GL_INT_2_10_10_10_REV && type != GL_UNSIGNED_INT_2_10_10_10_REV)) GLERR(GL_INVALID_OPERATION, );
    if ((type == GL_INT_2_10_10_10_REV || type == GL_UNSIGNED_INT_2_10_10_10_REV) && size != 4 && size != GL_BGRA) GLERR(GL_INVALID_OPERATION, );
    if (C->profile_core && C->vao == &C->vao0) GLERR(GL_INVALID_OPERATION, );
    if (C->profile_core && !C->b_array && ptr) GLERR(GL_INVALID_OPERATION, );
    attr_set(C, &C->vao->a[i], size, type, norm != 0, 0, stride, ptr);
}
void glVertexAttribIPointer(GLuint i, GLint size, GLenum type, GLsizei stride, const void *ptr) {
    GLC();
    if (i >= GLX_ATTR) GLERR(GL_INVALID_VALUE, );
    if (size < 1 || size > 4 || stride < 0) GLERR(GL_INVALID_VALUE, );
    if (!vtype_ok(type, 1)) GLERR(GL_INVALID_ENUM, );
    if (C->profile_core && C->vao == &C->vao0) GLERR(GL_INVALID_OPERATION, );
    attr_set(C, &C->vao->a[i], size, type, 0, 1, stride, ptr);
}
void glVertexAttribLPointer(GLuint i, GLint size, GLenum type, GLsizei stride, const void *ptr) {
    GLC();
    if (i >= GLX_ATTR) GLERR(GL_INVALID_VALUE, );
    if (size < 1 || size > 4 || stride < 0) GLERR(GL_INVALID_VALUE, );
    if (type != GL_DOUBLE) GLERR(GL_INVALID_ENUM, );
    attr_set(C, &C->vao->a[i], size, type, 0, 0, stride, ptr);
    C->vao->a[i].isdouble = 1;
}
void glVertexAttribPointerARB(GLuint i, GLint s, GLenum t, GLboolean n, GLsizei st, const void *p) { glVertexAttribPointer(i, s, t, n, st, p); }
void glVertexAttribIPointerEXT(GLuint i, GLint s, GLenum t, GLsizei st, const void *p) { glVertexAttribIPointer(i, s, t, st, p); }
void glEnableVertexAttribArray(GLuint i) { GLC(); if (i >= GLX_ATTR) GLERR(GL_INVALID_VALUE, ); C->vao->a[i].enabled = 1; }
void glDisableVertexAttribArray(GLuint i) { GLC(); if (i >= GLX_ATTR) GLERR(GL_INVALID_VALUE, ); C->vao->a[i].enabled = 0; }
void glEnableVertexAttribArrayARB(GLuint i) { glEnableVertexAttribArray(i); }
void glDisableVertexAttribArrayARB(GLuint i) { glDisableVertexAttribArray(i); }
void glVertexAttribDivisor(GLuint i, GLuint d) { GLC(); if (i >= GLX_ATTR) GLERR(GL_INVALID_VALUE, ); C->vao->a[i].divisor = d; }
void glVertexAttribDivisorARB(GLuint i, GLuint d) { glVertexAttribDivisor(i, d); }
static void attr_get(gl_ctx_t *C, GLuint i, GLenum p, double *out, int *n) {
    *n = 1;
    if (i >= GLX_ATTR) { gl_err(C, GL_INVALID_VALUE); *n = 0; return; }
    gl_attr_t *a = &C->vao->a[i];
    switch (p) {
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED: out[0] = a->enabled; return;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE: out[0] = a->size == GL_BGRA ? 4 : a->size; return;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE: out[0] = a->stride; return;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE: out[0] = a->type; return;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED: out[0] = a->normalized; return;
    case GL_VERTEX_ATTRIB_ARRAY_INTEGER: out[0] = a->integer; return;
    case GL_VERTEX_ATTRIB_ARRAY_DIVISOR: out[0] = a->divisor; return;
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: out[0] = a->buf ? a->buf->name : 0; return;
    case GL_CURRENT_VERTEX_ATTRIB: *n = 4; for (int k = 0; k < 4; k++) out[k] = C->cur_attr[i][k]; return;
    }
    gl_err(C, GL_INVALID_ENUM);
    *n = 0;
}
void glGetVertexAttribdv(GLuint i, GLenum p, GLdouble *v) { GLC(); double d[4]; int n; attr_get(C, i, p, d, &n); for (int k = 0; k < n; k++) v[k] = d[k]; }
void glGetVertexAttribfv(GLuint i, GLenum p, GLfloat *v) { GLC(); double d[4]; int n; attr_get(C, i, p, d, &n); for (int k = 0; k < n; k++) v[k] = (float)d[k]; }
void glGetVertexAttribiv(GLuint i, GLenum p, GLint *v) { GLC(); double d[4]; int n; attr_get(C, i, p, d, &n); for (int k = 0; k < n; k++) v[k] = (GLint)d[k]; }
void glGetVertexAttribIiv(GLuint i, GLenum p, GLint *v) { glGetVertexAttribiv(i, p, v); }
void glGetVertexAttribIuiv(GLuint i, GLenum p, GLuint *v) { GLC(); double d[4]; int n; attr_get(C, i, p, d, &n); for (int k = 0; k < n; k++) v[k] = (GLuint)d[k]; }
void glGetVertexAttribLdv(GLuint i, GLenum p, GLdouble *v) { glGetVertexAttribdv(i, p, v); }
void glGetVertexAttribPointerv(GLuint i, GLenum p, void **v) {
    GLC();
    if (i >= GLX_ATTR) GLERR(GL_INVALID_VALUE, );
    if (p != GL_VERTEX_ATTRIB_ARRAY_POINTER) GLERR(GL_INVALID_ENUM, );
    *v = (void *)C->vao->a[i].ptr;
}
void glGetVertexAttribdvARB(GLuint i, GLenum p, GLdouble *v) { glGetVertexAttribdv(i, p, v); }
void glGetVertexAttribfvARB(GLuint i, GLenum p, GLfloat *v) { glGetVertexAttribfv(i, p, v); }
void glGetVertexAttribivARB(GLuint i, GLenum p, GLint *v) { glGetVertexAttribiv(i, p, v); }
void glGetVertexAttribPointervARB(GLuint i, GLenum p, void **v) { glGetVertexAttribPointerv(i, p, v); }
void glGetVertexAttribIivEXT(GLuint i, GLenum p, GLint *v) { glGetVertexAttribiv(i, p, v); }
void glGetVertexAttribIuivEXT(GLuint i, GLenum p, GLuint *v) { glGetVertexAttribIuiv(i, p, v); }

/* ---- valores correntes: o atributo 0 no compat também é glVertex (gl_ffp.c faz a emissão) */
void gl_ffp_attr0(gl_ctx_t *C, const float *v);
static void cur_attr(GLuint i, float x, float y, float z, float w) {
    GLC();
    if (i >= GLX_ATTR) GLERR(GL_INVALID_VALUE, );
    float v[4] = {x, y, z, w};
    if (i == 0 && !C->profile_core && C->in_begin) { gl_ffp_attr0(C, v); return; }
    for (int k = 0; k < 4; k++) { C->cur_attr[i][k] = v[k]; C->cur_attri[i][k] = (i32)v[k]; }
}
static void cur_attr_i(GLuint i, i32 x, i32 y, i32 z, i32 w) {
    GLC();
    if (i >= GLX_ATTR) GLERR(GL_INVALID_VALUE, );
    i32 v[4] = {x, y, z, w};
    for (int k = 0; k < 4; k++) { C->cur_attri[i][k] = v[k]; C->cur_attr[i][k] = (float)v[k]; }
}
#define NB(x) ((float)(x) / 127.0f < -1 ? -1 : (float)(x) / 127.0f)
#define NS(x) ((float)(x) / 32767.0f < -1 ? -1 : (float)(x) / 32767.0f)
#define NI(x) ((float)((double)(x) / 2147483647.0) < -1 ? -1 : (float)((double)(x) / 2147483647.0))
void glVertexAttrib1f(GLuint i, GLfloat x) { cur_attr(i, x, 0, 0, 1); }
void glVertexAttrib2f(GLuint i, GLfloat x, GLfloat y) { cur_attr(i, x, y, 0, 1); }
void glVertexAttrib3f(GLuint i, GLfloat x, GLfloat y, GLfloat z) { cur_attr(i, x, y, z, 1); }
void glVertexAttrib4f(GLuint i, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { cur_attr(i, x, y, z, w); }
void glVertexAttrib1d(GLuint i, GLdouble x) { cur_attr(i, (float)x, 0, 0, 1); }
void glVertexAttrib2d(GLuint i, GLdouble x, GLdouble y) { cur_attr(i, (float)x, (float)y, 0, 1); }
void glVertexAttrib3d(GLuint i, GLdouble x, GLdouble y, GLdouble z) { cur_attr(i, (float)x, (float)y, (float)z, 1); }
void glVertexAttrib4d(GLuint i, GLdouble x, GLdouble y, GLdouble z, GLdouble w) { cur_attr(i, (float)x, (float)y, (float)z, (float)w); }
void glVertexAttrib1s(GLuint i, GLshort x) { cur_attr(i, x, 0, 0, 1); }
void glVertexAttrib2s(GLuint i, GLshort x, GLshort y) { cur_attr(i, x, y, 0, 1); }
void glVertexAttrib3s(GLuint i, GLshort x, GLshort y, GLshort z) { cur_attr(i, x, y, z, 1); }
void glVertexAttrib4s(GLuint i, GLshort x, GLshort y, GLshort z, GLshort w) { cur_attr(i, x, y, z, w); }
void glVertexAttrib1fv(GLuint i, const GLfloat *v) { cur_attr(i, v[0], 0, 0, 1); }
void glVertexAttrib2fv(GLuint i, const GLfloat *v) { cur_attr(i, v[0], v[1], 0, 1); }
void glVertexAttrib3fv(GLuint i, const GLfloat *v) { cur_attr(i, v[0], v[1], v[2], 1); }
void glVertexAttrib4fv(GLuint i, const GLfloat *v) { cur_attr(i, v[0], v[1], v[2], v[3]); }
void glVertexAttrib1dv(GLuint i, const GLdouble *v) { cur_attr(i, (float)v[0], 0, 0, 1); }
void glVertexAttrib2dv(GLuint i, const GLdouble *v) { cur_attr(i, (float)v[0], (float)v[1], 0, 1); }
void glVertexAttrib3dv(GLuint i, const GLdouble *v) { cur_attr(i, (float)v[0], (float)v[1], (float)v[2], 1); }
void glVertexAttrib4dv(GLuint i, const GLdouble *v) { cur_attr(i, (float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void glVertexAttrib1sv(GLuint i, const GLshort *v) { cur_attr(i, v[0], 0, 0, 1); }
void glVertexAttrib2sv(GLuint i, const GLshort *v) { cur_attr(i, v[0], v[1], 0, 1); }
void glVertexAttrib3sv(GLuint i, const GLshort *v) { cur_attr(i, v[0], v[1], v[2], 1); }
void glVertexAttrib4sv(GLuint i, const GLshort *v) { cur_attr(i, v[0], v[1], v[2], v[3]); }
void glVertexAttrib4iv(GLuint i, const GLint *v) { cur_attr(i, (float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void glVertexAttrib4bv(GLuint i, const GLbyte *v) { cur_attr(i, v[0], v[1], v[2], v[3]); }
void glVertexAttrib4ubv(GLuint i, const GLubyte *v) { cur_attr(i, v[0], v[1], v[2], v[3]); }
void glVertexAttrib4usv(GLuint i, const GLushort *v) { cur_attr(i, v[0], v[1], v[2], v[3]); }
void glVertexAttrib4uiv(GLuint i, const GLuint *v) { cur_attr(i, (float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void glVertexAttrib4Nbv(GLuint i, const GLbyte *v) { cur_attr(i, NB(v[0]), NB(v[1]), NB(v[2]), NB(v[3])); }
void glVertexAttrib4Nsv(GLuint i, const GLshort *v) { cur_attr(i, NS(v[0]), NS(v[1]), NS(v[2]), NS(v[3])); }
void glVertexAttrib4Niv(GLuint i, const GLint *v) { cur_attr(i, NI(v[0]), NI(v[1]), NI(v[2]), NI(v[3])); }
void glVertexAttrib4Nub(GLuint i, GLubyte x, GLubyte y, GLubyte z, GLubyte w) { cur_attr(i, x / 255.0f, y / 255.0f, z / 255.0f, w / 255.0f); }
void glVertexAttrib4Nubv(GLuint i, const GLubyte *v) { glVertexAttrib4Nub(i, v[0], v[1], v[2], v[3]); }
void glVertexAttrib4Nusv(GLuint i, const GLushort *v) { cur_attr(i, v[0] / 65535.0f, v[1] / 65535.0f, v[2] / 65535.0f, v[3] / 65535.0f); }
void glVertexAttrib4Nuiv(GLuint i, const GLuint *v) { cur_attr(i, (float)(v[0] / 4294967295.0), (float)(v[1] / 4294967295.0), (float)(v[2] / 4294967295.0), (float)(v[3] / 4294967295.0)); }
/* inteiros: guardados como float exato até 2^24; o GLSL lê como int/uint */
void glVertexAttribI1i(GLuint i, GLint x) { cur_attr_i(i, (i32)x, 0, 0, 1); }
void glVertexAttribI2i(GLuint i, GLint x, GLint y) { cur_attr_i(i, (i32)x, (i32)y, 0, 1); }
void glVertexAttribI3i(GLuint i, GLint x, GLint y, GLint z) { cur_attr_i(i, (i32)x, (i32)y, (i32)z, 1); }
void glVertexAttribI4i(GLuint i, GLint x, GLint y, GLint z, GLint w) { cur_attr_i(i, (i32)x, (i32)y, (i32)z, (i32)w); }
void glVertexAttribI1ui(GLuint i, GLuint x) { cur_attr_i(i, (i32)x, 0, 0, 1); }
void glVertexAttribI2ui(GLuint i, GLuint x, GLuint y) { cur_attr_i(i, (i32)x, (i32)y, 0, 1); }
void glVertexAttribI3ui(GLuint i, GLuint x, GLuint y, GLuint z) { cur_attr_i(i, (i32)x, (i32)y, (i32)z, 1); }
void glVertexAttribI4ui(GLuint i, GLuint x, GLuint y, GLuint z, GLuint w) { cur_attr_i(i, (i32)x, (i32)y, (i32)z, (i32)w); }
void glVertexAttribI1iv(GLuint i, const GLint *v) { glVertexAttribI1i(i, v[0]); }
void glVertexAttribI2iv(GLuint i, const GLint *v) { glVertexAttribI2i(i, v[0], v[1]); }
void glVertexAttribI3iv(GLuint i, const GLint *v) { glVertexAttribI3i(i, v[0], v[1], v[2]); }
void glVertexAttribI4iv(GLuint i, const GLint *v) { glVertexAttribI4i(i, v[0], v[1], v[2], v[3]); }
void glVertexAttribI1uiv(GLuint i, const GLuint *v) { glVertexAttribI1ui(i, v[0]); }
void glVertexAttribI2uiv(GLuint i, const GLuint *v) { glVertexAttribI2ui(i, v[0], v[1]); }
void glVertexAttribI3uiv(GLuint i, const GLuint *v) { glVertexAttribI3ui(i, v[0], v[1], v[2]); }
void glVertexAttribI4uiv(GLuint i, const GLuint *v) { glVertexAttribI4ui(i, v[0], v[1], v[2], v[3]); }
void glVertexAttribI4bv(GLuint i, const GLbyte *v) { glVertexAttribI4i(i, v[0], v[1], v[2], v[3]); }
void glVertexAttribI4sv(GLuint i, const GLshort *v) { glVertexAttribI4i(i, v[0], v[1], v[2], v[3]); }
void glVertexAttribI4ubv(GLuint i, const GLubyte *v) { glVertexAttribI4ui(i, v[0], v[1], v[2], v[3]); }
void glVertexAttribI4usv(GLuint i, const GLushort *v) { glVertexAttribI4ui(i, v[0], v[1], v[2], v[3]); }
void glVertexAttribL1d(GLuint i, GLdouble x) { glVertexAttrib1d(i, x); }
void glVertexAttribL2d(GLuint i, GLdouble x, GLdouble y) { glVertexAttrib2d(i, x, y); }
void glVertexAttribL3d(GLuint i, GLdouble x, GLdouble y, GLdouble z) { glVertexAttrib3d(i, x, y, z); }
void glVertexAttribL4d(GLuint i, GLdouble x, GLdouble y, GLdouble z, GLdouble w) { glVertexAttrib4d(i, x, y, z, w); }
void glVertexAttribL1dv(GLuint i, const GLdouble *v) { glVertexAttrib1dv(i, v); }
void glVertexAttribL2dv(GLuint i, const GLdouble *v) { glVertexAttrib2dv(i, v); }
void glVertexAttribL3dv(GLuint i, const GLdouble *v) { glVertexAttrib3dv(i, v); }
void glVertexAttribL4dv(GLuint i, const GLdouble *v) { glVertexAttrib4dv(i, v); }
/* 2_10_10_10 */
static void p1010(GLenum type, GLboolean norm, GLuint v, int n, float *o) {
    for (int k = 0; k < 4; k++) {
        int bits = k == 3 ? 2 : 10, sh = k * 10;
        u32 raw = (v >> sh) & ((1u << bits) - 1);
        float f;
        if (type == GL_INT_2_10_10_10_REV) {
            i32 s = (i32)(raw << (32 - bits)) >> (32 - bits);
            f = norm ? (float)s / (float)((1 << (bits - 1)) - 1) : (float)s;
            if (f < -1 && norm) f = -1;
        } else f = norm ? (float)raw / (float)((1u << bits) - 1) : (float)raw;
        o[k] = k < n ? f : (k == 3 ? 1 : 0);
    }
}
void glVertexAttribP1ui(GLuint i, GLenum t, GLboolean n, GLuint v) { float o[4]; p1010(t, n, v, 1, o); cur_attr(i, o[0], o[1], o[2], o[3]); }
void glVertexAttribP2ui(GLuint i, GLenum t, GLboolean n, GLuint v) { float o[4]; p1010(t, n, v, 2, o); cur_attr(i, o[0], o[1], o[2], o[3]); }
void glVertexAttribP3ui(GLuint i, GLenum t, GLboolean n, GLuint v) { float o[4]; p1010(t, n, v, 3, o); cur_attr(i, o[0], o[1], o[2], o[3]); }
void glVertexAttribP4ui(GLuint i, GLenum t, GLboolean n, GLuint v) { float o[4]; p1010(t, n, v, 4, o); cur_attr(i, o[0], o[1], o[2], o[3]); }
void glVertexAttribP1uiv(GLuint i, GLenum t, GLboolean n, const GLuint *v) { glVertexAttribP1ui(i, t, n, v[0]); }
void glVertexAttribP2uiv(GLuint i, GLenum t, GLboolean n, const GLuint *v) { glVertexAttribP2ui(i, t, n, v[0]); }
void glVertexAttribP3uiv(GLuint i, GLenum t, GLboolean n, const GLuint *v) { glVertexAttribP3ui(i, t, n, v[0]); }
void glVertexAttribP4uiv(GLuint i, GLenum t, GLboolean n, const GLuint *v) { glVertexAttribP4ui(i, t, n, v[0]); }
/* sufixo ARB */
void glVertexAttrib1fARB(GLuint i, GLfloat x) { glVertexAttrib1f(i, x); }
void glVertexAttrib2fARB(GLuint i, GLfloat x, GLfloat y) { glVertexAttrib2f(i, x, y); }
void glVertexAttrib3fARB(GLuint i, GLfloat x, GLfloat y, GLfloat z) { glVertexAttrib3f(i, x, y, z); }
void glVertexAttrib4fARB(GLuint i, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { glVertexAttrib4f(i, x, y, z, w); }
void glVertexAttrib1fvARB(GLuint i, const GLfloat *v) { glVertexAttrib1fv(i, v); }
void glVertexAttrib2fvARB(GLuint i, const GLfloat *v) { glVertexAttrib2fv(i, v); }
void glVertexAttrib3fvARB(GLuint i, const GLfloat *v) { glVertexAttrib3fv(i, v); }
void glVertexAttrib4fvARB(GLuint i, const GLfloat *v) { glVertexAttrib4fv(i, v); }
void glVertexAttrib1dARB(GLuint i, GLdouble x) { glVertexAttrib1d(i, x); }
void glVertexAttrib2dARB(GLuint i, GLdouble x, GLdouble y) { glVertexAttrib2d(i, x, y); }
void glVertexAttrib3dARB(GLuint i, GLdouble x, GLdouble y, GLdouble z) { glVertexAttrib3d(i, x, y, z); }
void glVertexAttrib4dARB(GLuint i, GLdouble x, GLdouble y, GLdouble z, GLdouble w) { glVertexAttrib4d(i, x, y, z, w); }
void glVertexAttrib1dvARB(GLuint i, const GLdouble *v) { glVertexAttrib1dv(i, v); }
void glVertexAttrib2dvARB(GLuint i, const GLdouble *v) { glVertexAttrib2dv(i, v); }
void glVertexAttrib3dvARB(GLuint i, const GLdouble *v) { glVertexAttrib3dv(i, v); }
void glVertexAttrib4dvARB(GLuint i, const GLdouble *v) { glVertexAttrib4dv(i, v); }
void glVertexAttrib1sARB(GLuint i, GLshort x) { glVertexAttrib1s(i, x); }
void glVertexAttrib2sARB(GLuint i, GLshort x, GLshort y) { glVertexAttrib2s(i, x, y); }
void glVertexAttrib3sARB(GLuint i, GLshort x, GLshort y, GLshort z) { glVertexAttrib3s(i, x, y, z); }
void glVertexAttrib4sARB(GLuint i, GLshort x, GLshort y, GLshort z, GLshort w) { glVertexAttrib4s(i, x, y, z, w); }
void glVertexAttrib1svARB(GLuint i, const GLshort *v) { glVertexAttrib1sv(i, v); }
void glVertexAttrib2svARB(GLuint i, const GLshort *v) { glVertexAttrib2sv(i, v); }
void glVertexAttrib3svARB(GLuint i, const GLshort *v) { glVertexAttrib3sv(i, v); }
void glVertexAttrib4svARB(GLuint i, const GLshort *v) { glVertexAttrib4sv(i, v); }
void glVertexAttrib4ivARB(GLuint i, const GLint *v) { glVertexAttrib4iv(i, v); }
void glVertexAttrib4bvARB(GLuint i, const GLbyte *v) { glVertexAttrib4bv(i, v); }
void glVertexAttrib4ubvARB(GLuint i, const GLubyte *v) { glVertexAttrib4ubv(i, v); }
void glVertexAttrib4usvARB(GLuint i, const GLushort *v) { glVertexAttrib4usv(i, v); }
void glVertexAttrib4uivARB(GLuint i, const GLuint *v) { glVertexAttrib4uiv(i, v); }
void glVertexAttrib4NbvARB(GLuint i, const GLbyte *v) { glVertexAttrib4Nbv(i, v); }
void glVertexAttrib4NsvARB(GLuint i, const GLshort *v) { glVertexAttrib4Nsv(i, v); }
void glVertexAttrib4NivARB(GLuint i, const GLint *v) { glVertexAttrib4Niv(i, v); }
void glVertexAttrib4NubARB(GLuint i, GLubyte x, GLubyte y, GLubyte z, GLubyte w) { glVertexAttrib4Nub(i, x, y, z, w); }
void glVertexAttrib4NubvARB(GLuint i, const GLubyte *v) { glVertexAttrib4Nubv(i, v); }
void glVertexAttrib4NusvARB(GLuint i, const GLushort *v) { glVertexAttrib4Nusv(i, v); }
void glVertexAttrib4NuivARB(GLuint i, const GLuint *v) { glVertexAttrib4Nuiv(i, v); }
void glVertexAttribI1iEXT(GLuint i, GLint x) { glVertexAttribI1i(i, x); }
void glVertexAttribI2iEXT(GLuint i, GLint x, GLint y) { glVertexAttribI2i(i, x, y); }
void glVertexAttribI3iEXT(GLuint i, GLint x, GLint y, GLint z) { glVertexAttribI3i(i, x, y, z); }
void glVertexAttribI4iEXT(GLuint i, GLint x, GLint y, GLint z, GLint w) { glVertexAttribI4i(i, x, y, z, w); }
void glVertexAttribI1uiEXT(GLuint i, GLuint x) { glVertexAttribI1ui(i, x); }
void glVertexAttribI2uiEXT(GLuint i, GLuint x, GLuint y) { glVertexAttribI2ui(i, x, y); }
void glVertexAttribI3uiEXT(GLuint i, GLuint x, GLuint y, GLuint z) { glVertexAttribI3ui(i, x, y, z); }
void glVertexAttribI4uiEXT(GLuint i, GLuint x, GLuint y, GLuint z, GLuint w) { glVertexAttribI4ui(i, x, y, z, w); }
void glVertexAttribI1ivEXT(GLuint i, const GLint *v) { glVertexAttribI1iv(i, v); }
void glVertexAttribI2ivEXT(GLuint i, const GLint *v) { glVertexAttribI2iv(i, v); }
void glVertexAttribI3ivEXT(GLuint i, const GLint *v) { glVertexAttribI3iv(i, v); }
void glVertexAttribI4ivEXT(GLuint i, const GLint *v) { glVertexAttribI4iv(i, v); }
void glVertexAttribI1uivEXT(GLuint i, const GLuint *v) { glVertexAttribI1uiv(i, v); }
void glVertexAttribI2uivEXT(GLuint i, const GLuint *v) { glVertexAttribI2uiv(i, v); }
void glVertexAttribI3uivEXT(GLuint i, const GLuint *v) { glVertexAttribI3uiv(i, v); }
void glVertexAttribI4uivEXT(GLuint i, const GLuint *v) { glVertexAttribI4uiv(i, v); }
void glVertexAttribI4bvEXT(GLuint i, const GLbyte *v) { glVertexAttribI4bv(i, v); }
void glVertexAttribI4svEXT(GLuint i, const GLshort *v) { glVertexAttribI4sv(i, v); }
void glVertexAttribI4ubvEXT(GLuint i, const GLubyte *v) { glVertexAttribI4ubv(i, v); }
void glVertexAttribI4usvEXT(GLuint i, const GLushort *v) { glVertexAttribI4usv(i, v); }

/* ================================================================ leitura de atributos (usada pelo rasterizador) */
/* lê o elemento e de um atributo; devolve 0 se a memória não cobre o elemento */
int gl_attr_fetch(gl_ctx_t *C, const gl_attr_t *a, u32 e, float out[4], i32 iout[4]) {
    int n = a->size == GL_BGRA ? 4 : a->size;
    int csz;
    switch (a->type) {
    case GL_BYTE: case GL_UNSIGNED_BYTE: csz = 1; break;
    case GL_SHORT: case GL_UNSIGNED_SHORT: case GL_HALF_FLOAT: csz = 2; break;
    case GL_DOUBLE: csz = 8; break;
    case GL_INT_2_10_10_10_REV: case GL_UNSIGNED_INT_2_10_10_10_REV: case GL_UNSIGNED_INT_10F_11F_11F_REV: csz = 4; n = 1; break;
    default: csz = 4;
    }
    i64 esz = (i64)csz * n;
    i64 stride = a->stride ? a->stride : esz;
    i64 off = (i64)e * stride;
    const u8 *p;
    if (a->buf) {
        i64 base = (i64)(uintptr_t)a->ptr;
        if (!a->buf->data || base < 0 || base + off + esz > a->buf->size) return 0;
        p = a->buf->data + base + off;
    } else {
        if (!a->ptr) return 0;
        p = (const u8 *)a->ptr + off;
    }
    (void)C;
    float v[4] = {0, 0, 0, 1};
    i32 iv[4] = {0, 0, 0, 1};
    if (a->type == GL_INT_2_10_10_10_REV || a->type == GL_UNSIGNED_INT_2_10_10_10_REV) {
        p1010(a->type, (GLboolean)a->normalized, *(const u32 *)p, 4, v);
        if (a->size == GL_BGRA) { float t = v[0]; v[0] = v[2]; v[2] = t; }
        for (int k = 0; k < 4; k++) { out[k] = v[k]; iout[k] = (i32)v[k]; }
        return 1;
    }
    if (a->type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
        double d[4];
        mx_decode(mx_pf(92), p, d);
        for (int k = 0; k < 4; k++) { out[k] = (float)d[k]; iout[k] = (i32)d[k]; }
        return 1;
    }
    for (int k = 0; k < n; k++) {
        double d = 0;
        i32 ii = 0;
        switch (a->type) {
        case GL_BYTE: ii = ((const i8 *)p)[k]; d = a->normalized ? (ii / 127.0 < -1 ? -1 : ii / 127.0) : ii; break;
        case GL_UNSIGNED_BYTE: ii = p[k]; d = a->normalized ? ii / 255.0 : ii; break;
        case GL_SHORT: ii = ((const i16 *)p)[k]; d = a->normalized ? (ii / 32767.0 < -1 ? -1 : ii / 32767.0) : ii; break;
        case GL_UNSIGNED_SHORT: ii = ((const u16 *)p)[k]; d = a->normalized ? ii / 65535.0 : ii; break;
        case GL_INT: ii = ((const i32 *)p)[k]; d = a->normalized ? (ii / 2147483647.0 < -1 ? -1 : ii / 2147483647.0) : ii; break;
        case GL_UNSIGNED_INT: { u32 u = ((const u32 *)p)[k]; ii = (i32)u; d = a->normalized ? u / 4294967295.0 : u; break; }
        case GL_FLOAT: d = ((const float *)p)[k]; ii = (i32)d; break;
        case GL_DOUBLE: d = ((const double *)p)[k]; ii = (i32)d; break;
        case GL_HALF_FLOAT: d = msl_h2f(((const u16 *)p)[k]); ii = (i32)d; break;
        case GL_FIXED: d = ((const i32 *)p)[k] / 65536.0; ii = (i32)d; break;
        }
        v[k] = (float)d;
        iv[k] = ii;
    }
    if (a->size == GL_BGRA) { float t = v[0]; v[0] = v[2]; v[2] = t; i32 ti = iv[0]; iv[0] = iv[2]; iv[2] = ti; }
    for (int k = 0; k < 4; k++) { out[k] = v[k]; iout[k] = a->integer ? iv[k] : (i32)v[k]; }
    return 1;
}
