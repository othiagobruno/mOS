/*
 * Teste de host do OpenGL por software do mOS (userland/libsystem/gl_*.c). Sem QEMU e sem GPU.
 * Cria contextos direto (gl_ctx_create), desenha e confere pixels com glReadPixels.
 *
 *   L=userland/libsystem
 *   gcc -std=gnu11 -O1 -w -fno-builtin -I tools/hostinc -I $L -o /tmp/gl_host tools/gl_host.c \
 *       $L/gl_ctx.c $L/gl_buf.c $L/gl_tex.c $L/gl_fbo.c $L/gl_raster.c $L/gl_ffp.c $L/gl_misc.c $L/gl_glsl.c \
 *       $L/mtltex.c $L/mtlshader.c $L/mtlexec.c $L/mslspirv.c -lm -lpthread && /tmp/gl_host
 */
#include "gl_int.h"
#include "gl_proto.h"

int printf(const char *, ...);
static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* o mtlexec.c pede estes do libSystem */
int *__error(void) { static int e; return &e; }
int cx_fdprintf(int fd, const char *fmt, ...) { (void)fd; (void)fmt; return 0; }

static gl_ctx_t *mk(int core, int w, int h) {
    gl_ctx_t *C = gl_ctx_create(0, core, core ? 4 : 2, 1, w, h, 1, 1, 1, 1);
    gl_ctx_make_current(C);
    return C;
}
static void px(int x, int y, u8 o[4]) { glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, o); }
static int near(int a, int b, int t) { return a - b <= t && b - a <= t; }

static void t_clear_and_state(void) {
    gl_ctx_t *C = mk(0, 32, 32);
    CHECK(!strcmp((const char *)glGetString(GL_VERSION), "2.1 mOS-1.0"), "versão compat");
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    CHECK(vp[2] == 32 && vp[3] == 32, "viewport inicial %d %d", vp[2], vp[3]);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    u8 o[4];
    px(5, 5, o);
    CHECK(o[0] == 255 && o[1] == 0 && o[2] == 0 && o[3] == 255, "clear vermelho: %d %d %d %d", o[0], o[1], o[2], o[3]);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 8, 8);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    px(2, 2, o);
    CHECK(o[1] == 255 && o[0] == 0, "scissor dentro");
    px(20, 20, o);
    CHECK(o[0] == 255 && o[1] == 0, "scissor fora");
    glDisable(GL_SCISSOR_TEST);
    CHECK(glGetError() == GL_NO_ERROR, "sem erro");
    glEnable(0x1234);
    CHECK(glGetError() == GL_INVALID_ENUM, "enum inválido");
    gl_ctx_destroy(C);
}

static void t_triangle_ffp(void) {
    gl_ctx_t *C = mk(0, 32, 32);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 32, 0, 32, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glBegin(GL_TRIANGLES);
    glColor3f(1, 1, 1);
    glVertex2f(0, 0);
    glVertex2f(32, 0);
    glVertex2f(0, 32);
    glEnd();
    /* metade de baixo-esquerda do quadrado: 32*32/2 menos a diagonal pela regra top-left */
    u8 *buf = malloc(32 * 32 * 4);
    glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    int n = 0;
    for (int i = 0; i < 32 * 32; i++) if (buf[i * 4] == 255) n++;
    CHECK(n == 496, "cobertura do triângulo: %d (esperado 496)", n);
    free(buf);
    /* cor interpolada */
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_QUADS);
    glColor3f(1, 0, 0); glVertex2f(0, 0);
    glColor3f(1, 0, 0); glVertex2f(32, 0);
    glColor3f(0, 0, 1); glVertex2f(32, 32);
    glColor3f(0, 0, 1); glVertex2f(0, 32);
    glEnd();
    u8 o[4];
    px(16, 0, o);
    CHECK(o[0] > 240 && o[2] < 15, "gradiente embaixo %d %d %d", o[0], o[1], o[2]);
    px(16, 31, o);
    CHECK(o[2] > 240 && o[0] < 15, "gradiente em cima %d %d %d", o[0], o[1], o[2]);
    px(16, 16, o);
    CHECK(near(o[0], 124, 4) && near(o[2], 131, 4), "gradiente no meio %d %d %d", o[0], o[1], o[2]);
    /* flat: último vértice */
    glShadeModel(GL_FLAT);
    glBegin(GL_TRIANGLES);
    glColor3f(1, 0, 0); glVertex2f(0, 0);
    glColor3f(0, 1, 0); glVertex2f(32, 0);
    glColor3f(0, 0, 1); glVertex2f(0, 32);
    glEnd();
    px(4, 4, o);
    CHECK(o[2] == 255 && o[0] == 0 && o[1] == 0, "flat com o último vértice %d %d %d", o[0], o[1], o[2]);
    glShadeModel(GL_SMOOTH);
    gl_ctx_destroy(C);
}

static void t_depth_stencil_blend(void) {
    gl_ctx_t *C = mk(0, 16, 16);
    glClearColor(0, 0, 0, 0);
    glClearDepth(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    /* perto (z = -0.5 -> 0.25) verde, depois longe vermelho: o verde fica */
    glColor3f(0, 1, 0);
    glRectf(-1, -1, 1, 1);   /* z = 0 -> 0.5 */
    glBegin(GL_QUADS);
    glColor3f(1, 0, 0);
    glVertex3f(-1, -1, 0.5f); glVertex3f(1, -1, 0.5f); glVertex3f(1, 1, 0.5f); glVertex3f(-1, 1, 0.5f);
    glEnd();
    u8 o[4];
    px(8, 8, o);
    CHECK(o[1] == 255 && o[0] == 0, "depth: o mais perto fica %d %d %d", o[0], o[1], o[2]);
    float d;
    glReadPixels(8, 8, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d);
    CHECK(d > 0.49f && d < 0.51f, "profundidade lida %f", d);
    glDisable(GL_DEPTH_TEST);
    /* stencil: marca metade esquerda, depois desenha só onde é 1 */
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(0, 0, 0, 0);
    glRectf(-1, -1, 0, 1);
    glColorMask(1, 1, 1, 1);
    glStencilFunc(GL_EQUAL, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glColor3f(0, 0, 1);
    glRectf(-1, -1, 1, 1);
    glDisable(GL_STENCIL_TEST);
    px(2, 8, o);
    CHECK(o[2] == 255, "stencil passa na esquerda %d %d %d", o[0], o[1], o[2]);
    px(13, 8, o);
    CHECK(o[2] == 0 && o[1] == 255, "stencil barra na direita %d %d %d", o[0], o[1], o[2]);
    /* blend alfa */
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1, 1, 1, 0.5f);
    glRectf(-1, -1, 1, 1);
    glDisable(GL_BLEND);
    px(8, 8, o);
    CHECK(near(o[0], 128, 1), "blend 50%% %d", o[0]);
    gl_ctx_destroy(C);
}

static void t_texture(void) {
    gl_ctx_t *C = mk(0, 8, 8);
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    /* 2x2: linha de baixo vermelho, verde; linha de cima azul, branco */
    u8 img[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, 1);
    glTexCoord2f(0, 1); glVertex2f(-1, 1);
    glEnd();
    u8 o[4];
    px(1, 1, o);
    CHECK(o[0] == 255 && o[1] == 0 && o[2] == 0, "texel baixo-esq %d %d %d", o[0], o[1], o[2]);
    px(6, 1, o);
    CHECK(o[1] == 255 && o[0] == 0, "texel baixo-dir %d %d %d", o[0], o[1], o[2]);
    px(1, 6, o);
    CHECK(o[2] == 255 && o[0] == 0, "texel cima-esq %d %d %d", o[0], o[1], o[2]);
    /* modulate com a cor */
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, 1);
    glTexCoord2f(0, 1); glVertex2f(-1, 1);
    glEnd();
    px(6, 6, o);
    CHECK(near(o[0], 128, 1) && near(o[1], 128, 1), "modulate %d %d %d", o[0], o[1], o[2]);
    /* luminância: o swizzle reconstrói rgb */
    u8 lum[1] = {200};
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, lum);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glRectf(-1, -1, 1, 1);
    px(4, 4, o);
    CHECK(o[0] == 200 && o[1] == 200 && o[2] == 200, "luminância %d %d %d %d", o[0], o[1], o[2], o[3]);
    /* glGetTexImage com BGRA */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    u8 back[64];
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, back);
    CHECK(back[0] == 0 && back[2] == 255 && back[4 + 1] == 255, "glGetTexImage BGRA");
    /* DXT1: bloco de uma cor (c0 = c1 = vermelho 565) */
    u8 blk[8] = {0x00, 0xf8, 0x00, 0xf8, 0, 0, 0, 0};
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0, 8, blk);
    glDisable(GL_TEXTURE_2D);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, back);
    CHECK(back[0] == 255 && back[1] == 0 && back[2] == 0 && back[3] == 255, "DXT1 %d %d %d %d", back[0], back[1], back[2], back[3]);
    CHECK(glGetError() == GL_NO_ERROR, "texturas sem erro");
    gl_ctx_destroy(C);
}

static void t_fbo(void) {
    gl_ctx_t *C = mk(0, 8, 8);
    GLuint tex, fbo;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "fbo completo");
    glViewport(0, 0, 4, 4);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    u8 o[4];
    px(3, 3, o);
    CHECK(o[2] == 255 && o[0] == 0, "fbo limpo de azul");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    u8 t[64];
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, t);
    CHECK(t[2] == 255 && t[60 + 2] == 255, "textura do fbo recebeu o clear");
    /* blit 4x4 -> 8x8 */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 8, 8, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    px(7, 7, o);
    CHECK(o[2] == 255, "blit para a janela");
    /* renderbuffer de profundidade */
    GLuint rb;
    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 4, 4);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "fbo com depth completo");
    glDeleteFramebuffers(1, &fbo);
    CHECK(glGetError() == GL_NO_ERROR, "fbo sem erro");
    gl_ctx_destroy(C);
}

static void t_lists_and_lighting(void) {
    gl_ctx_t *C = mk(0, 8, 8);
    GLuint l = glGenLists(1);
    glNewList(l, GL_COMPILE);
    glColor3f(1, 1, 0);
    glRectf(-1, -1, 1, 1);
    glEndList();
    glClear(GL_COLOR_BUFFER_BIT);
    u8 o[4];
    px(4, 4, o);
    CHECK(o[0] == 0, "GL_COMPILE não desenha");
    glCallList(l);
    px(4, 4, o);
    CHECK(o[0] == 255 && o[1] == 255 && o[2] == 0, "lista desenha %d %d %d", o[0], o[1], o[2]);
    /* luz difusa de frente: n·l = 1 -> material difuso 0.8 * luz 1 + ambiente 0.2*0.2 */
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glNormal3f(0, 0, 1);
    glRectf(-1, -1, 1, 1);
    px(4, 4, o);
    CHECK(near(o[0], 214, 2), "luz difusa %d", o[0]);
    glDisable(GL_LIGHTING);
    /* fog linear total */
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0);
    glFogf(GL_FOG_END, 0.0001f);
    float fc[4] = {0, 1, 1, 1};
    glFogfv(GL_FOG_COLOR, fc);
    glColor3f(1, 0, 0);
    glBegin(GL_QUADS);
    glVertex3f(-1, -1, -0.5f); glVertex3f(1, -1, -0.5f); glVertex3f(1, 1, -0.5f); glVertex3f(-1, 1, -0.5f);
    glEnd();
    glDisable(GL_FOG);
    px(4, 4, o);
    CHECK(o[0] == 0 && o[1] == 255, "fog %d %d %d", o[0], o[1], o[2]);
    /* seleção */
    GLuint sel[16];
    glSelectBuffer(16, sel);
    glRenderMode(GL_SELECT);
    glInitNames();
    glPushName(7);
    glRectf(-1, -1, 1, 1);
    GLint hits = glRenderMode(GL_RENDER);
    CHECK(hits == 1 && sel[0] == 1 && sel[3] == 7, "seleção: %d hits, nome %u", hits, sel[3]);
    CHECK(glGetError() == GL_NO_ERROR, "listas sem erro");
    gl_ctx_destroy(C);
}

static void t_arrays(void) {
    gl_ctx_t *C = mk(0, 8, 8);
    float v[] = {-1, -1, 1, -1, 1, 1, -1, 1};
    u8 c[] = {255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255};
    u16 idx[] = {0, 1, 2, 0, 2, 3};
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, v);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, c);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, idx);
    u8 o[4];
    px(4, 4, o);
    CHECK(o[0] == 255 && o[2] == 255 && o[1] == 0, "arrays de cliente %d %d %d", o[0], o[1], o[2]);
    /* VBO + EBO */
    GLuint b[2];
    glGenBuffers(2, b);
    glBindBuffer(GL_ARRAY_BUFFER, b[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);
    glVertexPointer(2, GL_FLOAT, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableClientState(GL_COLOR_ARRAY);
    glColor3f(0, 1, 0);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
    px(4, 4, o);
    CHECK(o[1] == 255 && o[0] == 0, "VBO/EBO %d %d %d", o[0], o[1], o[2]);
    /* VAO do APPLE_vertex_array_object guarda os arrays de cliente; o VBO apagado fora do bind continua vivo */
    GLuint vao, vb2;
    glGenVertexArraysAPPLE(1, &vao);
    glBindVertexArrayAPPLE(vao);
    glGenBuffers(1, &vb2);
    glBindBuffer(GL_ARRAY_BUFFER, vb2);
    glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_STATIC_DRAW);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArrayAPPLE(0);
    glDeleteBuffers(1, &vb2);
    glBindVertexArrayAPPLE(vao);
    glColor3f(1, 1, 1);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    px(4, 4, o);
    CHECK(o[0] == 255 && o[1] == 255, "buffer apagado ainda referenciado pelo VAO %d %d %d", o[0], o[1], o[2]);
    glBindVertexArrayAPPLE(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b[1]);
    float *m = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, 4, GL_MAP_READ_BIT);
    CHECK(m && ((u16 *)m)[1] == 1, "map do EBO");
    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    CHECK(glGetError() == GL_NO_ERROR, "arrays sem erro");
    gl_ctx_destroy(C);
}

static void t_eval_accum(void) {
    gl_ctx_t *C = mk(0, 16, 16);
    /* mapa 2D bilinear cobrindo a janela, com cor do vermelho (embaixo) ao azul (em cima) */
    float ctl[] = {-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0};
    float col[] = {1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1};
    glMap2f(GL_MAP2_VERTEX_3, 0, 1, 3, 2, 0, 1, 6, 2, ctl);
    glMap2f(GL_MAP2_COLOR_4, 0, 1, 4, 2, 0, 1, 8, 2, col);
    glEnable(GL_MAP2_VERTEX_3);
    glEnable(GL_MAP2_COLOR_4);
    glMapGrid2f(4, 0, 1, 4, 0, 1);
    glEvalMesh2(GL_FILL, 0, 4, 0, 4);
    u8 o[4];
    px(8, 1, o);
    CHECK(o[0] > 200 && o[2] < 40, "avaliador 2D embaixo %d %d %d", o[0], o[1], o[2]);
    px(8, 14, o);
    CHECK(o[2] > 200 && o[0] < 40, "avaliador 2D em cima %d %d %d", o[0], o[1], o[2]);
    GLint ord[2] = {0, 0};
    glGetMapiv(GL_MAP2_VERTEX_3, GL_ORDER, ord);
    CHECK(ord[0] == 2 && ord[1] == 2, "glGetMap ordem %d %d", ord[0], ord[1]);
    CHECK(C->cur_col[0] == 1 && C->cur_col[2] == 1, "a cor corrente não muda com o avaliador");
    /* curva quadrática de Bézier: ponto do meio */
    float q[] = {0, 0, 0, 1, 2, 0, 2, 0, 0};
    glMap1f(GL_MAP1_VERTEX_3, 0, 1, 3, 3, q);
    glGetMapfv(GL_MAP1_VERTEX_3, GL_COEFF, ctl);
    CHECK(ctl[4] == 2, "glGetMap coeficientes");
    /* acumulação: média de vermelho e verde */
    glDisable(GL_MAP2_VERTEX_3);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_ACCUM_BUFFER_BIT);
    glAccum(GL_LOAD, 0.5f);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glAccum(GL_ACCUM, 0.5f);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glAccum(GL_RETURN, 1.0f);
    px(3, 3, o);
    CHECK(near(o[0], 128, 1) && near(o[1], 128, 1) && o[2] == 0, "glAccum média %d %d %d", o[0], o[1], o[2]);
    CHECK(glGetError() == GL_NO_ERROR, "avaliadores e acumulação sem erro");
    gl_ctx_destroy(C);
}

static void t_arb_programs(void) {
    gl_ctx_t *C = mk(0, 8, 8);
    const char *vp =
        "!!ARBvp1.0\n"
        "PARAM mvp[4] = { state.matrix.mvp };\n"
        "ATTRIB p = vertex.position;\n"
        "TEMP t;\n"
        "DP4 result.position.x, mvp[0], p;\n"
        "DP4 result.position.y, mvp[1], p;\n"
        "DP4 result.position.z, mvp[2], p;\n"
        "DP4 result.position.w, mvp[3], p;\n"
        "MOV result.color, vertex.color;\n"
        "MOV result.texcoord[0], vertex.texcoord[0];\n"
        "END\n";
    const char *fp =
        "!!ARBfp1.0\n"
        "PARAM k = program.local[0];\n"
        "TEMP t, c;\n"
        "TEX t, fragment.texcoord[0], texture[0], 2D;\n"
        "SUB c, fragment.texcoord[0].x, 0.75;\n"
        "KIL -c.x;\n"
        "MUL_SAT result.color, t, k;\n"
        "END\n";
    GLuint ids[2];
    glGenProgramsARB(2, ids);
    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, ids[0]);
    glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(vp), vp);
    GLint pos = 0;
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &pos);
    CHECK(pos == -1, "vp aceito (erro %d: %s)", pos, glGetString(GL_PROGRAM_ERROR_STRING_ARB));
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ids[1]);
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(fp), fp);
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &pos);
    CHECK(pos == -1, "fp aceito (erro %d: %s)", pos, glGetString(GL_PROGRAM_ERROR_STRING_ARB));
    glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, 1, 0.5f, 2, 1);
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    u8 img[4] = {200, 200, 100, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glEnable(GL_VERTEX_PROGRAM_ARB);
    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, 1);
    glTexCoord2f(0, 1); glVertex2f(-1, 1);
    glEnd();
    u8 o[4];
    px(1, 4, o);
    CHECK(o[0] == 200 && o[1] == 100 && o[2] == 200, "ARB fp: textura * local, saturado %d %d %d", o[0], o[1], o[2]);
    px(7, 4, o);
    CHECK(o[0] == 0 && o[1] == 0, "ARB fp: KIL depois de 0.75 %d %d %d", o[0], o[1], o[2]);
    CHECK(glGetError() == GL_NO_ERROR, "ARB sem erro GL");
    const char *bad = "!!ARBvp1.0\nMOV result.position, nada;\nEND\n";
    glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(bad), bad);
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &pos);
    CHECK(glGetError() == GL_INVALID_OPERATION && pos > 0, "erro de sintaxe com posição %d", pos);
    GLint len = 0;
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_LENGTH_ARB, &len);
    CHECK(len == (GLint)strlen(vp), "o programa anterior continua (%d)", len);
    gl_ctx_destroy(C);
}

void gl_glsl_host_tests(int *fails, int *checks);
int main(void) {
    t_clear_and_state();
    t_triangle_ffp();
    t_depth_stencil_blend();
    t_texture();
    t_fbo();
    t_lists_and_lighting();
    t_arrays();
    t_eval_accum();
    t_arb_programs();
    gl_glsl_host_tests(&fails, &checks);
    printf("gl_host: %d/%d ok\n", checks - fails, checks);
    return fails != 0;
}
