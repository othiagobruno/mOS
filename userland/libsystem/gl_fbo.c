/*
 * mOS libSystem — OpenGL por software: renderbuffers, framebuffers (FBO e o padrão), draw/read buffers,
 * completude, glClear/glClearBuffer*, glReadPixels, glBlitFramebuffer e glDrawPixels/glCopyPixels
 * (compat). O framebuffer padrão é RGBA8 + depth32F/stencil8 com a linha 0 embaixo.
 */
#include "gl_int.h"
#include "gl_proto.h"

int gl_tex_image(gl_tex_t *t, int lvl, int layer, msl_tex_t *m, int *nlayers);
msl_tex_t *gl_tex_mt(gl_ctx_t *C, gl_tex_t *t);

/* ================================================================ renderbuffers */
void glGenRenderbuffers(GLsizei n, GLuint *out) { GLC(); if (n < 0) GLERR(GL_INVALID_VALUE, ); for (int i = 0; i < n; i++) out[i] = gl_names_gen(&C->sh->rbs); }
void gl_fbo_detach_rb(gl_ctx_t *C, GLuint name);
void glDeleteRenderbuffers(GLsizei n, const GLuint *names) {
    GLC();
    for (int i = 0; i < n; i++) {
        if (!names[i]) continue;
        gl_rb_t *r = gl_names_get(&C->sh->rbs, names[i]);
        gl_names_del(&C->sh->rbs, names[i]);
        if (C->rb == r) C->rb = 0;
        gl_fbo_detach_rb(C, names[i]);
        if (r) { free(r->data); free(r); }
    }
}
GLboolean glIsRenderbuffer(GLuint name) { GLC(0); return gl_names_get(&C->sh->rbs, name) != 0; }
void glBindRenderbuffer(GLenum target, GLuint name) {
    GLC();
    if (target != GL_RENDERBUFFER) GLERR(GL_INVALID_ENUM, );
    if (!name) { C->rb = 0; return; }
    gl_rb_t *r = gl_names_get(&C->sh->rbs, name);
    if (!r) {
        if (C->profile_core && !gl_names_is(&C->sh->rbs, name)) GLERR(GL_INVALID_OPERATION, );
        r = calloc(1, sizeof *r);
        if (!r) GLERR(GL_OUT_OF_MEMORY, );
        r->name = name;
        r->ifmt = GL_RGBA4;
        gl_names_set(&C->sh->rbs, name, r);
    }
    C->rb = r;
}
void glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum ifmt, GLsizei w, GLsizei h) {
    GLC();
    if (target != GL_RENDERBUFFER) GLERR(GL_INVALID_ENUM, );
    if (!C->rb) GLERR(GL_INVALID_OPERATION, );
    if (w < 0 || h < 0 || w > 16384 || h > 16384 || samples < 0 || samples > 8) GLERR(GL_INVALID_VALUE, );
    int ok;
    int fmt = gl_ifmt_to_mtl(ifmt, &ok);
    GLenum base = gl_ifmt_base(ifmt);
    if (!ok || base == GL_ALPHA || base == GL_LUMINANCE || base == GL_LUMINANCE_ALPHA || base == GL_INTENSITY) GLERR(GL_INVALID_ENUM, );
    gl_rb_t *r = C->rb;
    /* GL_RENDERBUFFER_SAMPLES fica >= o pedido, no próximo número suportado (2, 4, 8) */
    int ns = samples ? mx_samples_ok(samples < 2 ? 2 : samples) : 0;
    free(r->data);
    i64 bytes = (i64)w * h * mx_bpp(fmt) * (ns ? ns : 1);
    r->data = bytes ? calloc(1, (size_t)bytes) : 0;
    if (bytes && !r->data) GLERR(GL_OUT_OF_MEMORY, );
    r->ifmt = ifmt; r->fmt = fmt; r->w = w; r->h = h; r->samples = ns;
    memset(&r->mt, 0, sizeof r->mt);
    r->mt.data = r->data; r->mt.fmt = fmt; r->mt.type = ns ? MSL_TEX_2D_MS : MSL_TEX_2D; r->mt.w = w; r->mt.h = h; r->mt.d = 1; r->mt.levels = 1;
    r->mt.slices = ns ? ns : 1; r->mt.samples = ns;
    r->mt.lbpr[0] = (u64)w * mx_bpp(fmt); r->mt.lslice[0] = r->mt.lbpr[0] * h;
}
void glRenderbufferStorage(GLenum t, GLenum f, GLsizei w, GLsizei h) { glRenderbufferStorageMultisample(t, 0, f, w, h); }
void glGetRenderbufferParameteriv(GLenum target, GLenum p, GLint *v) {
    GLC();
    if (target != GL_RENDERBUFFER) GLERR(GL_INVALID_ENUM, );
    gl_rb_t *r = C->rb;
    if (!r) GLERR(GL_INVALID_OPERATION, );
    const pf_t *pf = mx_pf(r->fmt);
    int nc = pf ? pf->nc : 0, bits = pf ? pf->bpp * 8 / (nc ? nc : 1) : 0;
    switch (p) {
    case GL_RENDERBUFFER_WIDTH: *v = r->w; return;
    case GL_RENDERBUFFER_HEIGHT: *v = r->h; return;
    case GL_RENDERBUFFER_INTERNAL_FORMAT: *v = (GLint)r->ifmt; return;
    case GL_RENDERBUFFER_SAMPLES: *v = r->samples; return;
    case GL_RENDERBUFFER_RED_SIZE: *v = r->data && !mx_isdepth(r->fmt) && r->fmt != 253 && nc >= 1 ? bits : 0; return;
    case GL_RENDERBUFFER_GREEN_SIZE: *v = r->data && !mx_isdepth(r->fmt) && r->fmt != 253 && nc >= 2 ? bits : 0; return;
    case GL_RENDERBUFFER_BLUE_SIZE: *v = r->data && !mx_isdepth(r->fmt) && r->fmt != 253 && nc >= 3 ? bits : 0; return;
    case GL_RENDERBUFFER_ALPHA_SIZE: *v = r->data && !mx_isdepth(r->fmt) && r->fmt != 253 && nc >= 4 ? bits : 0; return;
    case GL_RENDERBUFFER_DEPTH_SIZE: *v = r->fmt == 250 ? 16 : r->fmt == 252 || r->fmt == 260 ? 32 : 0; return;
    case GL_RENDERBUFFER_STENCIL_SIZE: *v = r->fmt == 260 || r->fmt == 253 ? 8 : 0; return;
    }
    GLERR(GL_INVALID_ENUM, );
}
void glGenRenderbuffersEXT(GLsizei n, GLuint *r) { glGenRenderbuffers(n, r); }
void glDeleteRenderbuffersEXT(GLsizei n, const GLuint *r) { glDeleteRenderbuffers(n, r); }
GLboolean glIsRenderbufferEXT(GLuint r) { return glIsRenderbuffer(r); }
void glBindRenderbufferEXT(GLenum t, GLuint r) { GLC(); if (r && !gl_names_is(&C->sh->rbs, r)) gl_names_reserve(&C->sh->rbs, r); glBindRenderbuffer(t, r); }
void glRenderbufferStorageEXT(GLenum t, GLenum f, GLsizei w, GLsizei h) { glRenderbufferStorage(t, f, w, h); }
void glRenderbufferStorageMultisampleEXT(GLenum t, GLsizei s, GLenum f, GLsizei w, GLsizei h) { glRenderbufferStorageMultisample(t, s, f, w, h); }
void glGetRenderbufferParameterivEXT(GLenum t, GLenum p, GLint *v) { glGetRenderbufferParameteriv(t, p, v); }

/* ================================================================ framebuffers */
void glGenFramebuffers(GLsizei n, GLuint *out) { GLC(); if (n < 0) GLERR(GL_INVALID_VALUE, ); for (int i = 0; i < n; i++) out[i] = gl_names_gen(&C->fbos); }
void glDeleteFramebuffers(GLsizei n, const GLuint *names) {
    GLC();
    for (int i = 0; i < n; i++) {
        if (!names[i]) continue;
        gl_fbo_t *f = gl_names_get(&C->fbos, names[i]);
        gl_names_del(&C->fbos, names[i]);
        if (C->draw_fbo == f) C->draw_fbo = 0;
        if (C->read_fbo == f) C->read_fbo = 0;
        free(f);
    }
}
GLboolean glIsFramebuffer(GLuint name) { GLC(0); return gl_names_get(&C->fbos, name) != 0; }
void glBindFramebuffer(GLenum target, GLuint name) {
    GLC();
    if (target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER && target != GL_READ_FRAMEBUFFER) GLERR(GL_INVALID_ENUM, );
    gl_fbo_t *f = 0;
    if (name) {
        f = gl_names_get(&C->fbos, name);
        if (!f) {
            if (C->profile_core && !gl_names_is(&C->fbos, name)) GLERR(GL_INVALID_OPERATION, );
            f = calloc(1, sizeof *f);
            if (!f) GLERR(GL_OUT_OF_MEMORY, );
            f->name = name;
            f->draw_buf[0] = GL_COLOR_ATTACHMENT0;
            f->read_buf = GL_COLOR_ATTACHMENT0;
            gl_names_set(&C->fbos, name, f);
        }
    }
    if (target != GL_READ_FRAMEBUFFER) C->draw_fbo = f;
    if (target != GL_DRAW_FRAMEBUFFER) C->read_fbo = f;
}
static gl_fbo_t *fbo_target(gl_ctx_t *C, GLenum target) {
    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) return C->draw_fbo;
    if (target == GL_READ_FRAMEBUFFER) return C->read_fbo;
    gl_err(C, GL_INVALID_ENUM);
    return 0;
}
static gl_att_t *att_slot(gl_ctx_t *C, gl_fbo_t *f, GLenum a, gl_att_t **second) {
    *second = 0;
    if (a >= GL_COLOR_ATTACHMENT0 && a < GL_COLOR_ATTACHMENT0 + GLX_DRAW) return &f->color[a - GL_COLOR_ATTACHMENT0];
    if (a == GL_DEPTH_ATTACHMENT) return &f->depth;
    if (a == GL_STENCIL_ATTACHMENT) return &f->stencil;
    if (a == GL_DEPTH_STENCIL_ATTACHMENT) { *second = &f->stencil; return &f->depth; }
    gl_err(C, GL_INVALID_ENUM);
    return 0;
}
static void attach(GLenum target, GLenum a, GLenum type, GLuint name, int lvl, int layer, GLenum textarget) {
    GLC();
    gl_fbo_t *f = fbo_target(C, target);
    if (!f) { if (!C->err) gl_err(C, GL_INVALID_OPERATION); return; }
    gl_att_t *s2, *s = att_slot(C, f, a, &s2);
    if (!s) return;
    if (name && type == GL_TEXTURE) {
        gl_tex_t *t = gl_names_get(&C->sh->texs, name);
        if (!t) GLERR(GL_INVALID_OPERATION, );
        if (lvl < 0 || lvl >= 16) GLERR(GL_INVALID_VALUE, );
        if (textarget >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && textarget <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z) layer = (int)(textarget - GL_TEXTURE_CUBE_MAP_POSITIVE_X);
    }
    if (name && type == GL_RENDERBUFFER && !gl_names_get(&C->sh->rbs, name)) GLERR(GL_INVALID_OPERATION, );
    gl_att_t v = {name ? type : 0, name, lvl, layer};
    *s = v;
    if (s2) *s2 = v;
}
void glFramebufferTexture2D(GLenum t, GLenum a, GLenum tt, GLuint tex, GLint lvl) { attach(t, a, GL_TEXTURE, tex, lvl, 0, tt); }
void glFramebufferTexture1D(GLenum t, GLenum a, GLenum tt, GLuint tex, GLint lvl) { attach(t, a, GL_TEXTURE, tex, lvl, 0, tt); }
void glFramebufferTexture3D(GLenum t, GLenum a, GLenum tt, GLuint tex, GLint lvl, GLint z) { attach(t, a, GL_TEXTURE, tex, lvl, z, tt); }
void glFramebufferTextureLayer(GLenum t, GLenum a, GLuint tex, GLint lvl, GLint layer) { attach(t, a, GL_TEXTURE, tex, lvl, layer, 0); }
/* textura inteira: array, 3D e cube viram anexo em camadas (layer -1, gl_Layer escolhe a camada) */
static int tex_is_layered(gl_ctx_t *C, GLuint tex) {
    gl_tex_t *t = tex ? gl_names_get(&C->sh->texs, tex) : 0;
    if (!t) return 0;
    GLenum g = t->target;
    return g == GL_TEXTURE_1D_ARRAY || g == GL_TEXTURE_2D_ARRAY || g == GL_TEXTURE_3D || g == GL_TEXTURE_CUBE_MAP ||
           g == GL_TEXTURE_CUBE_MAP_ARRAY || g == GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
}
void glFramebufferTexture(GLenum t, GLenum a, GLuint tex, GLint lvl) { GLC(); attach(t, a, GL_TEXTURE, tex, lvl, tex_is_layered(C, tex) ? -1 : 0, 0); }
void glFramebufferRenderbuffer(GLenum t, GLenum a, GLenum rt, GLuint rb) {
    GLC();
    if (rt != GL_RENDERBUFFER) GLERR(GL_INVALID_ENUM, );
    attach(t, a, GL_RENDERBUFFER, rb, 0, 0, 0);
}
void gl_fbo_detach_tex(gl_ctx_t *C, GLuint name) {
    gl_fbo_t *fs[2] = {C->draw_fbo, C->read_fbo};
    for (int k = 0; k < 2; k++) {
        gl_fbo_t *f = fs[k];
        if (!f) continue;
        for (int i = 0; i < GLX_DRAW; i++) if (f->color[i].type == GL_TEXTURE && f->color[i].name == name) memset(&f->color[i], 0, sizeof f->color[i]);
        if (f->depth.type == GL_TEXTURE && f->depth.name == name) memset(&f->depth, 0, sizeof f->depth);
        if (f->stencil.type == GL_TEXTURE && f->stencil.name == name) memset(&f->stencil, 0, sizeof f->stencil);
    }
}
void gl_fbo_detach_rb(gl_ctx_t *C, GLuint name) {
    gl_fbo_t *fs[2] = {C->draw_fbo, C->read_fbo};
    for (int k = 0; k < 2; k++) {
        gl_fbo_t *f = fs[k];
        if (!f) continue;
        for (int i = 0; i < GLX_DRAW; i++) if (f->color[i].type == GL_RENDERBUFFER && f->color[i].name == name) memset(&f->color[i], 0, sizeof f->color[i]);
        if (f->depth.type == GL_RENDERBUFFER && f->depth.name == name) memset(&f->depth, 0, sizeof f->depth);
        if (f->stencil.type == GL_RENDERBUFFER && f->stencil.name == name) memset(&f->stencil, 0, sizeof f->stencil);
    }
}
/* resolve um anexo em msl_tex_t (1 nível, a camada pedida) */
static msl_tex_t g_att_mt[2][GLX_DRAW + 2];
static int att_image(gl_ctx_t *C, const gl_att_t *a, msl_tex_t *out, int *ifmt_out) {
    if (!a->type) return 0;
    if (a->type == GL_RENDERBUFFER) {
        gl_rb_t *r = gl_names_get(&C->sh->rbs, a->name);
        if (!r || !r->data) return 0;
        *out = r->mt;
        if (ifmt_out) *ifmt_out = (int)r->ifmt;
        return 1;
    }
    gl_tex_t *t = gl_names_get(&C->sh->texs, a->name);
    if (!t) return 0;
    if (ifmt_out) *ifmt_out = (int)t->lv[a->level < 16 ? a->level : 0].ifmt;
    return gl_tex_image(t, a->level, a->layer, out, 0);
}
/* GL_TEXTURE_FIXED_SAMPLE_LOCATIONS do anexo (renderbuffer conta como fixo) */
static int att_fixed(gl_ctx_t *C, const gl_att_t *a) {
    if (a->type != GL_TEXTURE) return 1;
    gl_tex_t *t = gl_names_get(&C->sh->texs, a->name);
    return !t || t->lv[0].ns <= 1 || t->fixed_loc;
}
static GLenum fbo_status(gl_ctx_t *C, gl_fbo_t *f) {
    if (!f) return GL_FRAMEBUFFER_COMPLETE;
    int any = 0, w = -1, h = -1, ns = -1, fixed = -1;
    msl_tex_t m;
    int ifmt;
    /* todos os anexos com o mesmo número de amostras e o mesmo GL_TEXTURE_FIXED_SAMPLE_LOCATIONS */
#define MS_SAME(a) do { int n_ = mx_nsamp(&m), fx_ = att_fixed(C, (a)); \
        if ((ns >= 0 && n_ != ns) || (fixed >= 0 && fx_ != fixed)) { return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE; } \
        ns = n_; fixed = fx_; } while (0)
    for (int i = 0; i < GLX_DRAW; i++) {
        if (!f->color[i].type) continue;
        if (!att_image(C, &f->color[i], &m, &ifmt)) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        if (mx_isdepth(m.fmt) || m.fmt == 253) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        any = 1;
        MS_SAME(&f->color[i]);
        if (w < 0 || m.w < w) w = m.w;
        if (h < 0 || m.h < h) h = m.h;
    }
    if (f->depth.type) {
        if (!att_image(C, &f->depth, &m, &ifmt) || !mx_isdepth(m.fmt)) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        any = 1;
        MS_SAME(&f->depth);
    }
    if (f->stencil.type) {
        if (!att_image(C, &f->stencil, &m, &ifmt) || !mx_hasstencil(m.fmt)) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        any = 1;
        MS_SAME(&f->stencil);
    }
#undef MS_SAME
    if (!any && !(f->def_w && f->def_h)) return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    /* em camadas: ou todos os anexos são, ou nenhum */
    int nl = 0, nn = 0;
    gl_att_t *all[GLX_DRAW + 2];
    for (int i = 0; i < GLX_DRAW; i++) all[i] = &f->color[i];
    all[GLX_DRAW] = &f->depth;
    all[GLX_DRAW + 1] = &f->stencil;
    for (int i = 0; i < GLX_DRAW + 2; i++) {
        if (!all[i]->type) continue;
        if (all[i]->type == GL_TEXTURE && all[i]->layer < 0) nl++; else nn++;
    }
    if (nl && nn) return GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS;
    for (int i = 0; i < GLX_DRAW; i++) {
        GLenum b = f->draw_buf[i];
        if (b >= GL_COLOR_ATTACHMENT0 && b < GL_COLOR_ATTACHMENT0 + GLX_DRAW && !f->color[b - GL_COLOR_ATTACHMENT0].type && !C->profile_core)
            return GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER;
    }
    return GL_FRAMEBUFFER_COMPLETE;
}
GLenum glCheckFramebufferStatus(GLenum target) {
    GLC(0);
    gl_fbo_t *f = fbo_target(C, target);
    if (C->err == GL_INVALID_ENUM) return 0;
    return fbo_status(C, f);
}
void glGetFramebufferAttachmentParameteriv(GLenum target, GLenum a, GLenum p, GLint *v) {
    GLC();
    gl_fbo_t *f = fbo_target(C, target);
    if (C->err == GL_INVALID_ENUM) return;
    if (!f) {
        /* framebuffer padrão */
        if (p == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE) { *v = GL_FRAMEBUFFER_DEFAULT; return; }
        if (p == GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING) { *v = GL_LINEAR; return; }
        if (p == GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE) { *v = a == GL_DEPTH || a == GL_STENCIL ? GL_UNSIGNED_NORMALIZED : GL_UNSIGNED_NORMALIZED; return; }
        int bits = 0;
        if (a == GL_DEPTH) bits = p == GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE && C->fb_has_depth ? 24 : 0;
        else if (a == GL_STENCIL) bits = p == GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE && C->fb_has_stencil ? 8 : 0;
        else bits = p == GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE ? (C->fb_alpha ? 8 : 0) : (p >= GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE && p <= GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE) ? 8 : 0;
        *v = bits;
        return;
    }
    gl_att_t *s2, *s = att_slot(C, f, a, &s2);
    if (!s) return;
    switch (p) {
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE: *v = s->type ? (GLint)s->type : GL_NONE; return;
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME: *v = (GLint)s->name; return;
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL: *v = s->level; return;
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER: *v = s->layer; return;
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE: *v = GL_TEXTURE_CUBE_MAP_POSITIVE_X + s->layer; return;
    case GL_FRAMEBUFFER_ATTACHMENT_LAYERED: *v = s->layer < 0; return;
    }
    msl_tex_t m;
    int ifmt = 0;
    if (!att_image(C, s, &m, &ifmt)) { if (s->type) GLERR(GL_INVALID_OPERATION, ); *v = 0; return; }
    const pf_t *pf = mx_pf(m.fmt);
    int k = pf ? pf->kind : 0, nc = pf ? pf->nc : 0, bits = pf ? pf->bpp * 8 / (nc ? nc : 1) : 0;
    switch (p) {
    case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE: *v = !mx_isdepth(m.fmt) && nc >= 1 ? bits : 0; return;
    case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE: *v = !mx_isdepth(m.fmt) && nc >= 2 ? bits : 0; return;
    case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE: *v = !mx_isdepth(m.fmt) && nc >= 3 ? bits : 0; return;
    case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE: *v = !mx_isdepth(m.fmt) && nc >= 4 ? bits : 0; return;
    case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE: *v = m.fmt == 250 ? 16 : m.fmt == 252 || m.fmt == 260 ? 32 : 0; return;
    case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE: *v = m.fmt == 260 || m.fmt == 253 ? 8 : 0; return;
    case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
        *v = k == PK_F32 || k == PK_HALF || k == PK_D32F || k == PK_D32FS8 || k == PK_RG11B10F ? GL_FLOAT :
             k == PK_U8 || k == PK_U16 || k == PK_U32 ? GL_UNSIGNED_INT : k == PK_S8 || k == PK_S16 || k == PK_S32 ? GL_INT :
             k == PK_SN8 || k == PK_SN16 ? GL_SIGNED_NORMALIZED : GL_UNSIGNED_NORMALIZED;
        return;
    case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING: *v = pf && (pf->fl & PF_SRGB) ? GL_SRGB : GL_LINEAR; return;
    }
    GLERR(GL_INVALID_ENUM, );
}
/* EXT_framebuffer_object: o nome nasce no bind */
void glGenFramebuffersEXT(GLsizei n, GLuint *f) { glGenFramebuffers(n, f); }
void glDeleteFramebuffersEXT(GLsizei n, const GLuint *f) { glDeleteFramebuffers(n, f); }
GLboolean glIsFramebufferEXT(GLuint f) { return glIsFramebuffer(f); }
void glBindFramebufferEXT(GLenum t, GLuint f) { GLC(); if (f && !gl_names_is(&C->fbos, f)) gl_names_reserve(&C->fbos, f); glBindFramebuffer(t, f); }
GLenum glCheckFramebufferStatusEXT(GLenum t) { return glCheckFramebufferStatus(t); }
void glFramebufferTexture1DEXT(GLenum t, GLenum a, GLenum tt, GLuint tex, GLint l) { glFramebufferTexture1D(t, a, tt, tex, l); }
void glFramebufferTexture2DEXT(GLenum t, GLenum a, GLenum tt, GLuint tex, GLint l) { glFramebufferTexture2D(t, a, tt, tex, l); }
void glFramebufferTexture3DEXT(GLenum t, GLenum a, GLenum tt, GLuint tex, GLint l, GLint z) { glFramebufferTexture3D(t, a, tt, tex, l, z); }
void glFramebufferRenderbufferEXT(GLenum t, GLenum a, GLenum rt, GLuint rb) { glFramebufferRenderbuffer(t, a, rt, rb); }
void glGetFramebufferAttachmentParameterivEXT(GLenum t, GLenum a, GLenum p, GLint *v) { glGetFramebufferAttachmentParameteriv(t, a, p, v); }
void glFramebufferTextureLayerEXT(GLenum t, GLenum a, GLuint tex, GLint l, GLint layer) { glFramebufferTextureLayer(t, a, tex, l, layer); }
void glFramebufferTextureEXT(GLenum t, GLenum a, GLuint tex, GLint l) { glFramebufferTexture(t, a, tex, l); }
void glFramebufferTextureARB(GLenum t, GLenum a, GLuint tex, GLint l) { glFramebufferTexture(t, a, tex, l); }
void glFramebufferTextureLayerARB(GLenum t, GLenum a, GLuint tex, GLint l, GLint layer) { glFramebufferTextureLayer(t, a, tex, l, layer); }
void glFramebufferTextureFaceARB(GLenum t, GLenum a, GLuint tex, GLint l, GLenum face) { attach(t, a, GL_TEXTURE, tex, l, 0, face); }
void glFramebufferTextureFaceEXT(GLenum t, GLenum a, GLuint tex, GLint l, GLenum face) { attach(t, a, GL_TEXTURE, tex, l, 0, face); }

/* ---- draw/read buffers */
static int buf_ok_default(gl_ctx_t *C, GLenum b) {
    switch (b) {
    case GL_NONE: case GL_FRONT_LEFT: case GL_FRONT: case GL_LEFT: case GL_FRONT_AND_BACK: return 1;
    case GL_BACK_LEFT: case GL_BACK: return C->fb_double;
    }
    return 0;
}
void glDrawBuffers(GLsizei n, const GLenum *bufs) {
    GLC();
    if (n < 0 || n > GLX_DRAW) GLERR(GL_INVALID_VALUE, );
    gl_fbo_t *f = C->draw_fbo;
    for (int i = 0; i < n; i++) {
        GLenum b = bufs[i];
        if (f) { if (b != GL_NONE && (b < GL_COLOR_ATTACHMENT0 || b >= GL_COLOR_ATTACHMENT0 + GLX_DRAW)) GLERR(GL_INVALID_ENUM, ); }
        else if (!buf_ok_default(C, b) || (n > 1 && b == GL_FRONT_AND_BACK)) GLERR(GL_INVALID_OPERATION, );
    }
    for (int i = 0; i < GLX_DRAW; i++) {
        GLenum b = i < n ? bufs[i] : GL_NONE;
        if (f) f->draw_buf[i] = b; else C->def_draw_buf[i] = b;
    }
}
void glDrawBuffer(GLenum b) {
    GLC();
    if (C->draw_fbo) { if (b != GL_NONE && (b < GL_COLOR_ATTACHMENT0 || b >= GL_COLOR_ATTACHMENT0 + GLX_DRAW)) GLERR(GL_INVALID_OPERATION, ); }
    else if (!buf_ok_default(C, b) && b != GL_BACK_RIGHT && b != GL_FRONT_RIGHT && b != GL_RIGHT) GLERR(GL_INVALID_ENUM, );
    glDrawBuffers(1, &b);
}
void glDrawBuffersARB(GLsizei n, const GLenum *b) { glDrawBuffers(n, b); }
void glReadBuffer(GLenum b) {
    GLC();
    if (C->read_fbo) {
        if (b != GL_NONE && (b < GL_COLOR_ATTACHMENT0 || b >= GL_COLOR_ATTACHMENT0 + GLX_DRAW)) GLERR(GL_INVALID_OPERATION, );
        C->read_fbo->read_buf = b;
    } else {
        if (!buf_ok_default(C, b) || b == GL_FRONT_AND_BACK) GLERR(GL_INVALID_OPERATION, );
        C->def_read_buf = b;
    }
}

/* ================================================================ resolução dos alvos */
int gl_resolve_target(gl_ctx_t *C, int draw, gl_target_t *T) {
    memset(T, 0, sizeof *T);
    T->ybottom = 1;
    gl_fbo_t *f = draw ? C->draw_fbo : C->read_fbo;
    msl_tex_t *slot = g_att_mt[draw ? 0 : 1];
    if (!f) {
        T->w = C->fb_w;
        T->h = C->fb_h;
        int n = 0;
        if (draw) {
            for (int i = 0; i < GLX_DRAW; i++) {
                GLenum b = C->def_draw_buf[i];
                msl_tex_t *t = 0;
                if (b == GL_BACK || b == GL_BACK_LEFT || b == GL_FRONT_AND_BACK || b == GL_LEFT) t = &C->fb_back;
                else if (b == GL_FRONT || b == GL_FRONT_LEFT) t = C->fb_double ? &C->fb_front : &C->fb_back;
                T->color[i] = t;
                if (t) n = i + 1;
            }
        } else {
            GLenum b = C->def_read_buf;
            T->color[0] = b == GL_NONE ? 0 : (b == GL_FRONT || b == GL_FRONT_LEFT) && C->fb_double ? &C->fb_front : &C->fb_back;
            n = 1;
        }
        T->ncolor = n;
        if (C->fb_has_depth) T->depth = &C->fb_depth;
        if (C->fb_has_stencil) T->stencil = &C->fb_depth;
        T->samples = C->fb_samples > 1 ? C->fb_samples : 1;
        return 1;
    }
    if (fbo_status(C, f) != GL_FRAMEBUFFER_COMPLETE) return 0;
    int w = 1 << 30, h = 1 << 30, any = 0;
    if (draw) {
        for (int i = 0; i < GLX_DRAW; i++) {
            GLenum b = f->draw_buf[i];
            if (b < GL_COLOR_ATTACHMENT0 || b >= GL_COLOR_ATTACHMENT0 + GLX_DRAW) continue;
            gl_att_t *a = &f->color[b - GL_COLOR_ATTACHMENT0];
            if (att_image(C, a, &slot[i], 0)) { T->color[i] = &slot[i]; T->ncolor = i + 1; }
        }
    } else if (f->read_buf >= GL_COLOR_ATTACHMENT0 && f->read_buf < GL_COLOR_ATTACHMENT0 + GLX_DRAW) {
        if (att_image(C, &f->color[f->read_buf - GL_COLOR_ATTACHMENT0], &slot[0], 0)) { T->color[0] = &slot[0]; T->ncolor = 1; }
    }
    for (int i = 0; i < GLX_DRAW; i++) {
        msl_tex_t m;
        if (f->color[i].type && att_image(C, &f->color[i], &m, 0)) { if (m.w < w) w = m.w; if (m.h < h) h = m.h; any = 1; }
    }
    if (att_image(C, &f->depth, &slot[GLX_DRAW], 0)) { T->depth = &slot[GLX_DRAW]; if (slot[GLX_DRAW].w < w) w = slot[GLX_DRAW].w; if (slot[GLX_DRAW].h < h) h = slot[GLX_DRAW].h; any = 1; }
    if (att_image(C, &f->stencil, &slot[GLX_DRAW + 1], 0)) { T->stencil = &slot[GLX_DRAW + 1]; if (slot[GLX_DRAW + 1].w < w) w = slot[GLX_DRAW + 1].w; if (slot[GLX_DRAW + 1].h < h) h = slot[GLX_DRAW + 1].h; any = 1; }
    if (!any) { w = f->def_w; h = f->def_h; }
    T->w = w;
    T->h = h;
    /* anexos em camadas: base (camada 0) e passo entre camadas */
    {
        gl_att_t *src[GLX_DRAW + 2];
        msl_tex_t *dst[GLX_DRAW + 2];
        int n = 0;
        if (draw) for (int i = 0; i < GLX_DRAW; i++) {
            GLenum b = f->draw_buf[i];
            if (b < GL_COLOR_ATTACHMENT0 || b >= GL_COLOR_ATTACHMENT0 + GLX_DRAW || !T->color[i]) continue;
            src[n] = &f->color[b - GL_COLOR_ATTACHMENT0]; dst[n++] = T->color[i];
        }
        if (T->depth) { src[n] = &f->depth; dst[n++] = T->depth; }
        if (T->stencil) { src[n] = &f->stencil; dst[n++] = T->stencil; }
        for (int i = 0; i < n; i++) {
            if (src[i]->type != GL_TEXTURE || src[i]->layer >= 0) continue;
            gl_tex_t *tx = gl_names_get(&C->sh->texs, src[i]->name);
            int nlay = 0;
            msl_tex_t m;
            if (!tx || !gl_tex_image(tx, src[i]->level, 0, &m, &nlay)) continue;
            int ns = mx_nsamp(&m);
            T->lt[T->layered] = dst[i];
            T->lbase[T->layered] = dst[i]->data;
            T->lstride[T->layered] = (tx->target == GL_TEXTURE_1D_ARRAY ? m.lbpr[0] : m.lslice[0]) * (u64)ns;
            if (!T->layered || nlay < T->nlayers) T->nlayers = nlay;
            T->layered++;
        }
    }
    T->samples = 1;
    for (int i = 0; i < T->ncolor; i++) if (T->color[i]) T->samples = mx_nsamp(T->color[i]);
    if (T->depth) T->samples = mx_nsamp(T->depth);
    if (T->stencil) T->samples = mx_nsamp(T->stencil);
    for (int i = 0; i < T->ncolor; i++) if (T->color[i]) {
        const pf_t *p = mx_pf(T->color[i]->fmt);
        T->srgb[i] = p && (p->fl & PF_SRGB);
        T->isint[i] = msl_fmt_isint(T->color[i]->fmt);
    }
    return 1;
}
/* fonte das cópias (glCopyTex*, glCopyPixels): o framebuffer padrão multisample entra resolvido;
 * um FBO multisample não pode ser lido assim (GL_INVALID_OPERATION) */
int gl_read_source(gl_ctx_t *C, msl_tex_t **src) {
    static gl_target_t T;
    if (!gl_resolve_target(C, 0, &T)) { *src = 0; return 0; }
    *src = T.color[0];
    if (*src && T.samples > 1) {
        if (C->read_fbo) { *src = 0; return 0; }
        *src = (msl_tex_t *)gl_fb_resolved(C, *src);
    }
    return 1;
}

void gl_target_layer(gl_target_t *T, int layer) {
    for (int i = 0; i < T->layered; i++) T->lt[i]->data = T->lbase[i] + (u64)layer * T->lstride[i];
}

/* ================================================================ clear */
static void stencil_write_s(msl_tex_t *t, int x, int y, int smp, u32 v, u32 mask) {
    u8 *q = mx_texel(t, x, y, 0, smp, 0);
    u8 *s = t->fmt == 260 ? q + 4 : t->fmt == 253 ? q : 0;
    if (s) *s = (u8)((*s & ~mask) | (v & mask));
}
static void depth_write_s(msl_tex_t *t, int x, int y, int smp, double d) {
    u8 *q = mx_texel(t, x, y, 0, smp, 0);
    if (t->fmt == 260 || t->fmt == 252) { float f = (float)d; memcpy(q, &f, 4); }
    else if (t->fmt == 250) { u16 v = (u16)(mx_cl01(d) * 65535 + 0.5); memcpy(q, &v, 2); }
}
/* todas as amostras do pixel */
static void stencil_write(msl_tex_t *t, int x, int y, u32 v, u32 mask) { for (int s = 0; s < mx_nsamp(t); s++) stencil_write_s(t, x, y, s, v, mask); }
static void depth_write(msl_tex_t *t, int x, int y, double d) { for (int s = 0; s < mx_nsamp(t); s++) depth_write_s(t, x, y, s, d); }
static void clear_rect(gl_ctx_t *C, gl_target_t *T, int *x0, int *y0, int *x1, int *y1) {
    *x0 = 0; *y0 = 0; *x1 = T->w; *y1 = T->h;
    if (gl_is_on(C, GL_SCISSOR_TEST)) {
        if (C->sc[0] > *x0) *x0 = C->sc[0];
        if (C->sc[1] > *y0) *y0 = C->sc[1];
        if (C->sc[0] + C->sc[2] < *x1) *x1 = C->sc[0] + C->sc[2];
        if (C->sc[1] + C->sc[3] < *y1) *y1 = C->sc[1] + C->sc[3];
    }
}
static void clear_color(gl_ctx_t *C, gl_target_t *T, int i, const double v[4], int isint) {
    msl_tex_t *t = T->color[i];
    if (!t || !t->data) return;
    int x0, y0, x1, y1;
    clear_rect(C, T, &x0, &y0, &x1, &y1);
    const u8 *m = C->cmask[i];
    const pf_t *p = mx_pf(t->fmt);
    if (!p) return;
    int bpp = p->bpp;
    u8 px[16];
    double c[4] = {v[0], v[1], v[2], v[3]};
    if (!isint && !(p->kind == PK_F32 || p->kind == PK_HALF)) for (int k = 0; k < 4; k++) c[k] = mx_cl01(c[k]);
    mx_srgb_init();
    int full = m[0] && m[1] && m[2] && m[3];
    if (full) mx_encode(p, px, c);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            for (int smp = 0; smp < mx_nsamp(t); smp++) {
                u8 *q = mx_texel(t, x, y, 0, smp, 0);
                if (full) { memcpy(q, px, bpp); continue; }
                double o[4];
                mx_decode(p, q, o);
                for (int k = 0; k < 4; k++) if (m[k]) o[k] = c[k];
                mx_encode(p, q, o);
            }
}
static void clear_ds(gl_ctx_t *C, gl_target_t *T, int depth, double d, int stencil, int s) {
    int x0, y0, x1, y1;
    clear_rect(C, T, &x0, &y0, &x1, &y1);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            if (depth && T->depth && C->dmask) depth_write(T->depth, x, y, mx_cl01(d));
            if (stencil && T->stencil) stencil_write(T->stencil, x, y, (u32)s, C->swmask[0] & 0xff);
        }
}
void gl_clear_target(gl_ctx_t *C, GLbitfield mask) {
    gl_target_t T;
    if (!gl_resolve_target(C, 1, &T)) { gl_err(C, GL_INVALID_FRAMEBUFFER_OPERATION); return; }
    /* FBO em camadas: o glClear limpa todas */
    for (int L = 0; L < (T.layered ? T.nlayers : 1); L++) {
        if (T.layered) gl_target_layer(&T, L);
        if (mask & GL_COLOR_BUFFER_BIT) {
            double v[4] = {C->clear_c[0], C->clear_c[1], C->clear_c[2], C->clear_c[3]};
            for (int i = 0; i < T.ncolor; i++) clear_color(C, &T, i, v, 0);
        }
        if (mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) clear_ds(C, &T, !!(mask & GL_DEPTH_BUFFER_BIT), C->clear_d, !!(mask & GL_STENCIL_BUFFER_BIT), C->clear_s);
    }
    if (T.layered) gl_target_layer(&T, 0);
    if (mask & GL_ACCUM_BUFFER_BIT) { void gl_accum_clear(gl_ctx_t *C); gl_accum_clear(C); }
}
void glClear(GLbitfield mask) {
    GLC();
    if (mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT)) GLERR(GL_INVALID_VALUE, );
    if (C->in_begin) GLERR(GL_INVALID_OPERATION, );
    if (C->dl_cur) { extern int gl_dl_record(gl_ctx_t *, int, const void *, int); if (gl_dl_record(C, 5, &mask, sizeof mask)) return; }
    if (gl_is_on(C, GL_RASTERIZER_DISCARD)) return;
    gl_clear_target(C, mask);
}
static void clear_buf(GLenum buf, GLint db, const double *v, int isint) {
    GLC();
    gl_target_t T;
    if (!gl_resolve_target(C, 1, &T)) GLERR(GL_INVALID_FRAMEBUFFER_OPERATION, );
    if (buf != GL_COLOR && buf != GL_DEPTH && buf != GL_STENCIL) GLERR(GL_INVALID_ENUM, );
    if (buf == GL_COLOR && (db < 0 || db >= GLX_DRAW)) GLERR(GL_INVALID_VALUE, );
    for (int L = 0; L < (T.layered ? T.nlayers : 1); L++) {
        if (T.layered) gl_target_layer(&T, L);
        if (buf == GL_COLOR) clear_color(C, &T, db, v, isint);
        else if (buf == GL_DEPTH) clear_ds(C, &T, 1, v[0], 0, 0);
        else clear_ds(C, &T, 0, 0, 1, (int)v[0]);
    }
    if (T.layered) gl_target_layer(&T, 0);
}
void glClearBufferfv(GLenum b, GLint db, const GLfloat *v) { double d[4] = {v[0], b == GL_COLOR ? v[1] : 0, b == GL_COLOR ? v[2] : 0, b == GL_COLOR ? v[3] : 0}; clear_buf(b, db, d, 0); }
void glClearBufferiv(GLenum b, GLint db, const GLint *v) { double d[4] = {v[0], b == GL_COLOR ? v[1] : 0, b == GL_COLOR ? v[2] : 0, b == GL_COLOR ? v[3] : 0}; clear_buf(b, db, d, 1); }
void glClearBufferuiv(GLenum b, GLint db, const GLuint *v) { double d[4] = {v[0], v[1], v[2], v[3]}; clear_buf(b, db, d, 1); }
void glClearBufferfi(GLenum b, GLint db, GLfloat depth, GLint stencil) {
    GLC();
    (void)db;
    if (b != GL_DEPTH_STENCIL) GLERR(GL_INVALID_ENUM, );
    gl_target_t T;
    if (!gl_resolve_target(C, 1, &T)) GLERR(GL_INVALID_FRAMEBUFFER_OPERATION, );
    for (int L = 0; L < (T.layered ? T.nlayers : 1); L++) { if (T.layered) gl_target_layer(&T, L); clear_ds(C, &T, 1, depth, 1, stencil); }
    if (T.layered) gl_target_layer(&T, 0);
}
void glClearColorIiEXT(GLint r, GLint g, GLint b, GLint a) { glClearColor((float)r, (float)g, (float)b, (float)a); }
void glClearColorIuiEXT(GLuint r, GLuint g, GLuint b, GLuint a) { glClearColor((float)r, (float)g, (float)b, (float)a); }

/* ================================================================ leitura */
void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, void *px) {
    GLC();
    if (w < 0 || h < 0) GLERR(GL_INVALID_VALUE, );
    gl_target_t T;
    if (!gl_resolve_target(C, 0, &T)) GLERR(GL_INVALID_FRAMEBUFFER_OPERATION, );
    u8 *dst = px;
    if (C->b_pack) { if (!C->b_pack->data) GLERR(GL_INVALID_OPERATION, ); dst = C->b_pack->data + (uintptr_t)px; }
    if (!dst) return;
    msl_tex_t *src = T.color[0];
    if (f == GL_DEPTH_COMPONENT) src = T.depth;
    else if (f == GL_STENCIL_INDEX || f == GL_DEPTH_STENCIL) src = T.stencil ? T.stencil : T.depth;
    if (!src) GLERR(GL_INVALID_OPERATION, );
    if (T.samples > 1) {
        /* FBO multisample: precisa de glBlitFramebuffer antes; o framebuffer padrão é resolvido aqui */
        if (C->read_fbo) GLERR(GL_INVALID_OPERATION, );
        src = (msl_tex_t *)gl_fb_resolved(C, src);
    }
    gl_pack_pixels(C, src, 0, 0, x, y, w, h, f, ty, dst, 1);
}
void glReadnPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, GLsizei bs, void *d) { (void)bs; glReadPixels(x, y, w, h, f, t, d); }

/* ================================================================ blit */
/* multiamostra: fonte com N amostras é resolvida (média na cor; amostra 0 em profundidade, estêncil e inteiros);
 * destino com N amostras recebe o valor em todas; as duas com o mesmo N copiam amostra a amostra */
void glBlitFramebuffer(GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint dx0, GLint dy0, GLint dx1, GLint dy1, GLbitfield mask, GLenum filter) {
    GLC();
    if (mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) GLERR(GL_INVALID_VALUE, );
    if (filter != GL_NEAREST && filter != GL_LINEAR) GLERR(GL_INVALID_ENUM, );
    if ((mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) && filter != GL_NEAREST) GLERR(GL_INVALID_OPERATION, );
    gl_target_t S, D;
    if (!gl_resolve_target(C, 0, &S) || !gl_resolve_target(C, 1, &D)) GLERR(GL_INVALID_FRAMEBUFFER_OPERATION, );
    int sms = S.samples > 1, dms = D.samples > 1;
    if (sms || dms) {
        int sw = sx1 - sx0, sh = sy1 - sy0, dw = dx1 - dx0, dh = dy1 - dy0;
        if ((sw < 0 ? -sw : sw) != (dw < 0 ? -dw : dw) || (sh < 0 ? -sh : sh) != (dh < 0 ? -dh : dh)) GLERR(GL_INVALID_OPERATION, );
        if (sms && dms && S.samples != D.samples) GLERR(GL_INVALID_OPERATION, );
    }
    int same = sms && dms;   /* cópia amostra a amostra */
    int dxa = dx0 < dx1 ? dx0 : dx1, dxb = dx0 < dx1 ? dx1 : dx0, dya = dy0 < dy1 ? dy0 : dy1, dyb = dy0 < dy1 ? dy1 : dy0;
    double scx = (double)(sx1 - sx0) / (dx1 - dx0 ? dx1 - dx0 : 1), scy = (double)(sy1 - sy0) / (dy1 - dy0 ? dy1 - dy0 : 1);
    int scis = gl_is_on(C, GL_SCISSOR_TEST);
    mx_srgb_init();
    for (int y = dya; y < dyb; y++) {
        if (y < 0 || y >= D.h) continue;
        if (scis && (y < C->sc[1] || y >= C->sc[1] + C->sc[3])) continue;
        for (int x = dxa; x < dxb; x++) {
            if (x < 0 || x >= D.w) continue;
            if (scis && (x < C->sc[0] || x >= C->sc[0] + C->sc[2])) continue;
            double fx = sx0 + (x + 0.5 - dx0) * scx, fy = sy0 + (y + 0.5 - dy0) * scy;
            int ix = (int)floor(fx), iy = (int)floor(fy);
            if (mask & GL_COLOR_BUFFER_BIT && S.color[0]) {
                double v[4] = {0, 0, 0, 1};
                msl_tex_t *s = S.color[0];
                int have = 1;
                if (filter == GL_LINEAR && !sms) {
                    double ux = fx - 0.5, uy = fy - 0.5;
                    int x0 = (int)floor(ux), y0 = (int)floor(uy);
                    double ax = ux - x0, ay = uy - y0, acc[4] = {0, 0, 0, 0};
                    for (int j = 0; j < 2; j++)
                        for (int i = 0; i < 2; i++) {
                            int px = x0 + i, py = y0 + j;
                            if (px < 0) px = 0;
                            if (py < 0) py = 0;
                            if (px >= s->w) px = s->w - 1;
                            if (py >= s->h) py = s->h - 1;
                            double t[4], wgt = (i ? ax : 1 - ax) * (j ? ay : 1 - ay);
                            msl_tex_load(s, px, py, 0, 0, 0, t);
                            for (int k = 0; k < 4; k++) acc[k] += wgt * t[k];
                        }
                    for (int k = 0; k < 4; k++) v[k] = acc[k];
                } else if (ix >= 0 && iy >= 0 && ix < s->w && iy < s->h) { if (!same) mx_resolve_texel(s, ix, iy, 0, v); }
                else have = 0;
                if (have) for (int i = 0; i < D.ncolor; i++) {
                    msl_tex_t *d = D.color[i];
                    if (!d) continue;
                    for (int smp = 0; smp < mx_nsamp(d); smp++) {
                        double o[4];
                        if (same) msl_tex_load(s, ix, iy, 0, smp, 0, v);
                        msl_tex_load(d, x, y, 0, smp, 0, o);
                        for (int k = 0; k < 4; k++) if (C->cmask[i][k]) o[k] = v[k];
                        msl_tex_store(d, x, y, 0, smp, 0, o);
                    }
                }
            }
            if (ix < 0 || iy < 0 || ix >= S.w || iy >= S.h) continue;
            if ((mask & GL_DEPTH_BUFFER_BIT) && S.depth && D.depth) {
                for (int smp = 0; smp < mx_nsamp(D.depth); smp++) {
                    double v[4];
                    msl_tex_load(S.depth, ix, iy, 0, same ? smp : 0, 0, v);
                    depth_write_s(D.depth, x, y, smp, v[0]);
                }
            }
            if ((mask & GL_STENCIL_BUFFER_BIT) && S.stencil && D.stencil) {
                for (int smp = 0; smp < mx_nsamp(D.stencil); smp++) {
                    u8 *q = mx_texel(S.stencil, ix, iy, 0, same ? smp : 0, 0);
                    u32 sv = S.stencil->fmt == 260 ? q[4] : q[0];
                    stencil_write_s(D.stencil, x, y, smp, sv, C->swmask[0] & 0xff);
                }
            }
        }
    }
}
void glBlitFramebufferEXT(GLint a, GLint b, GLint c, GLint d, GLint e, GLint f, GLint g, GLint h, GLbitfield m, GLenum fl) { glBlitFramebuffer(a, b, c, d, e, f, g, h, m, fl); }
