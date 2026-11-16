/* Testes de host do GLSL do OpenGL por software (chamados por tools/gl_host.c). */
#include "gl_int.h"
#include "gl_proto.h"

int printf(const char *, ...);
char *strstr(const char *, const char *);
static int *F, *K;
#define CHECK(c, ...) do { (*K)++; if (!(c)) { (*F)++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int near(int a, int b, int t) { return a - b <= t && b - a <= t; }
static void px(int x, int y, u8 o[4]) { glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, o); }

static GLuint prog(const char *vs, const char *fs, char *log, int nlog) {
    GLuint v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(v, 1, &vs, 0);
    glShaderSource(f, 1, &fs, 0);
    glCompileShader(v);
    glCompileShader(f);
    GLint ok;
    glGetShaderiv(v, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(v, nlog, 0, log); printf("VS: %s\n", log); return 0; }
    glGetShaderiv(f, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(f, nlog, 0, log); printf("FS: %s\n", log); return 0; }
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glBindAttribLocation(p, 0, "pos");
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(p, nlog, 0, log); printf("link: %s\n", log); return 0; }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

static void t_core_basic(void) {
    gl_ctx_t *C = gl_ctx_create(0, 1, 4, 1, 16, 16, 1, 1, 1, 1);
    gl_ctx_make_current(C);
    char log[1024];
    const char *vs = "#version 410 core\n"
                     "layout(location = 0) in vec2 pos;\n"
                     "in vec3 col;\n"
                     "uniform mat4 mvp;\n"
                     "out vec3 vcol;\n"
                     "void main() { vcol = col; gl_Position = mvp * vec4(pos, 0.0, 1.0); }\n";
    const char *fs = "#version 410 core\n"
                     "in vec3 vcol;\n"
                     "uniform vec4 tint;\n"
                     "out vec4 frag;\n"
                     "float lum(in vec3 c, out float k) { k = 2.0; return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
                     "void main() { float k; float l = lum(vcol, k); frag = vec4(vcol * tint.rgb, 1.0); if (l < 0.0) discard; frag.a = k * 0.5; }\n";
    GLuint p = prog(vs, fs, log, sizeof log);
    CHECK(p != 0, "programa core compila e liga");
    if (!p) { gl_ctx_destroy(C); return; }
    GLint na = 0;
    glGetProgramiv(p, GL_ACTIVE_ATTRIBUTES, &na);
    CHECK(na == 2, "2 atributos ativos (%d)", na);
    CHECK(glGetAttribLocation(p, "pos") == 0, "pos na location 0");
    GLint lc = glGetAttribLocation(p, "col");
    CHECK(lc == 1, "col automática na 1 (%d)", lc);
    GLint lm = glGetUniformLocation(p, "mvp"), lt = glGetUniformLocation(p, "tint");
    CHECK(lm >= 0 && lt >= 0 && lm != lt, "locations de uniform %d %d", lm, lt);
    GLuint vao, vbo[2];
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    float pos[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    float col[] = {1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0};
    glGenBuffers(2, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof pos, pos, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof col, col, GL_STATIC_DRAW);
    glVertexAttribPointer((GLuint)lc, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray((GLuint)lc);
    glUseProgram(p);
    float id[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    glUniformMatrix4fv(lm, 1, GL_FALSE, id);
    glUniform4f(lt, 1, 0.5f, 1, 1);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    u8 o[4];
    px(8, 8, o);
    CHECK(o[0] == 255 && near(o[1], 128, 1) && o[2] == 0 && o[3] == 255, "GLSL core: %d %d %d %d", o[0], o[1], o[2], o[3]);
    float got[4];
    glGetUniformfv(p, lt, got);
    CHECK(got[1] == 0.5f, "glGetUniformfv");
    glUniform1i(lt, 3);
    CHECK(glGetError() == GL_INVALID_OPERATION, "tipo errado no glUniform");
    /* erro de compilação com log */
    GLuint bad = glCreateShader(GL_FRAGMENT_SHADER);
    const char *bs = "#version 410\nout vec4 c;\nvoid main() { c = undefinedThing; }\n";
    glShaderSource(bad, 1, &bs, 0);
    glCompileShader(bad);
    GLint ok = 1;
    glGetShaderiv(bad, GL_COMPILE_STATUS, &ok);
    glGetShaderInfoLog(bad, sizeof log, 0, log);
    CHECK(!ok && strstr(log, "ERROR"), "erro de compilação reportado: %s", log);
    gl_ctx_destroy(C);
}

static void t_texture_discard_ubo(void) {
    gl_ctx_t *C = gl_ctx_create(0, 1, 4, 1, 8, 8, 1, 1, 1, 1);
    gl_ctx_make_current(C);
    char log[1024];
    const char *vs = "#version 330\nlayout(location=0) in vec2 pos;\nout vec2 uv;\n"
                     "void main() { uv = pos * 0.5 + 0.5; gl_Position = vec4(pos, 0, 1); }\n";
    const char *fs = "#version 330\nin vec2 uv;\nuniform sampler2D tex;\n"
                     "layout(std140) uniform Params { vec4 scale; float cut; } P;\n"
                     "out vec4 o;\n"
                     "void main() { vec4 t = texture(tex, uv); if (uv.x > P.cut) discard; o = t * P.scale; }\n";
    GLuint p = prog(vs, fs, log, sizeof log);
    CHECK(p != 0, "programa com textura/UBO liga");
    if (!p) { gl_ctx_destroy(C); return; }
    GLuint tex;
    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex);
    u8 img[4] = {200, 100, 50, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glUseProgram(p);
    glUniform1i(glGetUniformLocation(p, "tex"), 2);
    GLuint bi = glGetUniformBlockIndex(p, "Params");
    CHECK(bi == 0, "índice do bloco");
    GLint bsz = 0;
    glGetActiveUniformBlockiv(p, bi, GL_UNIFORM_BLOCK_DATA_SIZE, &bsz);
    CHECK(bsz == 32, "tamanho std140 do bloco %d", bsz);
    const char *nm = "Params.cut";
    GLuint ui;
    glGetUniformIndices(p, 1, &nm, &ui);
    GLint uoff = -1;
    glGetActiveUniformsiv(p, 1, &ui, GL_UNIFORM_OFFSET, &uoff);
    CHECK(uoff == 16, "offset std140 de cut %d", uoff);
    glUniformBlockBinding(p, bi, 3);
    float ubo[8] = {1, 1, 0, 1, 0.5f, 0, 0, 0};
    GLuint ub, vao, vb;
    glGenBuffers(1, &ub);
    glBindBuffer(GL_UNIFORM_BUFFER, ub);
    glBufferData(GL_UNIFORM_BUFFER, sizeof ubo, ubo, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, ub);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    float pos[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    glGenBuffers(1, &vb);
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferData(GL_ARRAY_BUFFER, sizeof pos, pos, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, 0);
    glEnableVertexAttribArray(0);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    u8 o[4];
    px(1, 4, o);
    CHECK(o[0] == 200 && o[1] == 100 && o[2] == 0, "textura * escala do UBO %d %d %d", o[0], o[1], o[2]);
    px(6, 4, o);
    CHECK(o[2] == 255 && o[0] == 0, "discard à direita %d %d %d", o[0], o[1], o[2]);
    CHECK(glGetError() == GL_NO_ERROR, "sem erro GL");
    gl_ctx_destroy(C);
}

static void t_compat_glsl(void) {
    gl_ctx_t *C = gl_ctx_create(0, 0, 2, 1, 8, 8, 1, 1, 1, 1);
    gl_ctx_make_current(C);
    char log[1024];
    const char *vs = "varying vec4 c;\nvoid main() { c = gl_Color; gl_TexCoord[0] = gl_MultiTexCoord0; gl_Position = ftransform(); }\n";
    const char *fs = "varying vec4 c;\nvoid main() { gl_FragColor = vec4(c.rgb, 1.0) + vec4(gl_TexCoord[0].x, 0.0, 0.0, 0.0); }\n";
    GLuint p = prog(vs, fs, log, sizeof log);
    CHECK(p != 0, "GLSL 1.10 compat liga");
    if (!p) { gl_ctx_destroy(C); return; }
    glUseProgram(p);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 8, 0, 8, -1, 1);
    glBegin(GL_QUADS);
    glColor3f(0, 1, 0); glTexCoord2f(0.5f, 0);
    glVertex2f(0, 0); glVertex2f(8, 0); glVertex2f(8, 8); glVertex2f(0, 8);
    glEnd();
    u8 o[4];
    px(4, 4, o);
    CHECK(near(o[0], 128, 1) && o[1] == 255 && o[2] == 0, "compat: gl_Color + gl_TexCoord %d %d %d", o[0], o[1], o[2]);
    /* MRT com gl_FragData e um FBO de 2 cores */
    const char *fs2 = "#version 120\nvoid main() { gl_FragData[0] = vec4(1,0,0,1); gl_FragData[1] = vec4(0,0,1,1); }\n";
    GLuint p2 = prog(vs, fs2, log, sizeof log);
    CHECK(p2 != 0, "programa MRT liga");
    GLuint t[2], fbo;
    glGenTextures(2, t);
    for (int i = 0; i < 2; i++) { glBindTexture(GL_TEXTURE_2D, t[i]); glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0); }
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t[0], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, t[1], 0);
    GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, bufs);
    glUseProgram(p2);
    glRectf(0, 0, 8, 8);
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    px(3, 3, o);
    CHECK(o[2] == 255 && o[0] == 0, "MRT: segundo anexo azul %d %d %d", o[0], o[1], o[2]);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    px(3, 3, o);
    CHECK(o[0] == 255 && o[2] == 0, "MRT: primeiro anexo vermelho");
    CHECK(glGetError() == GL_NO_ERROR, "compat sem erro GL");
    gl_ctx_destroy(C);
}

static void t_xfb(void) {
    gl_ctx_t *C = gl_ctx_create(0, 1, 4, 1, 4, 4, 1, 1, 1, 1);
    gl_ctx_make_current(C);
    char log[1024];
    const char *vs = "#version 410\nlayout(location=0) in float x;\nout float y;\nvoid main() { y = x * 2.0 + 1.0; gl_Position = vec4(0); }\n";
    const char *fs = "#version 410\nout vec4 o;\nvoid main() { o = vec4(1); }\n";
    GLuint v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(v, 1, &vs, 0); glCompileShader(v);
    glShaderSource(f, 1, &fs, 0); glCompileShader(f);
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    const char *names[1] = {"y"};
    glTransformFeedbackVaryings(p, 1, names, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(p, sizeof log, 0, log); printf("%s\n", log); }
    CHECK(ok, "programa de transform feedback liga");
    GLuint vao, in, out;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    float xs[4] = {0, 1, 2, 3};
    glGenBuffers(1, &in);
    glBindBuffer(GL_ARRAY_BUFFER, in);
    glBufferData(GL_ARRAY_BUFFER, sizeof xs, xs, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 1, GL_FLOAT, 0, 0, 0);
    glEnableVertexAttribArray(0);
    glGenBuffers(1, &out);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, out);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 16, 0, GL_STATIC_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, out);
    glUseProgram(p);
    glEnable(GL_RASTERIZER_DISCARD);
    GLuint q;
    glGenQueries(1, &q);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, q);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, 4);
    glEndTransformFeedback();
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    glDisable(GL_RASTERIZER_DISCARD);
    GLuint nw = 0;
    glGetQueryObjectuiv(q, GL_QUERY_RESULT, &nw);
    float r[4] = {0};
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 16, r);
    CHECK(nw == 4 && r[0] == 1 && r[3] == 7, "transform feedback: %u prims, %g %g %g %g", nw, r[0], r[1], r[2], r[3]);
    gl_ctx_destroy(C);
}

static void t_bindable(void) {
    gl_ctx_t *C = gl_ctx_create(0, 0, 2, 1, 4, 4, 1, 1, 1, 1);
    gl_ctx_make_current(C);
    char log[512];
    const char *vs = "void main() { gl_Position = ftransform(); }\n";
    const char *fs = "#extension GL_EXT_bindable_uniform : enable\nbindable uniform vec4 bu;\nvoid main() { gl_FragColor = bu; }\n";
    GLuint p = prog(vs, fs, log, sizeof log);
    CHECK(p != 0, "bindable uniform liga");
    if (!p) { gl_ctx_destroy(C); return; }
    GLint loc = glGetUniformLocation(p, "bu");
    CHECK(glGetUniformBufferSizeEXT(p, loc) == 16 && glGetUniformOffsetEXT(p, loc) == 0, "tamanho e offset do bindable");
    float c[4] = {0, 1, 0, 1};
    GLuint b;
    glGenBuffers(1, &b);
    glBindBuffer(GL_UNIFORM_BUFFER_EXT, b);
    glBufferData(GL_UNIFORM_BUFFER_EXT, 16, c, GL_STATIC_DRAW);
    glUniformBufferEXT(p, loc, b);
    glUseProgram(p);
    glRectf(-1, -1, 1, 1);
    u8 o[4];
    px(2, 2, o);
    CHECK(o[1] == 255 && o[0] == 0, "bindable uniform lido do buffer %d %d %d", o[0], o[1], o[2]);
    gl_ctx_destroy(C);
}

static void t_derivatives(void) {
    gl_ctx_t *C = gl_ctx_create(0, 1, 4, 1, 8, 8, 1, 1, 1, 1);
    gl_ctx_make_current(C);
    char log[512];
    const char *vs = "#version 330\nlayout(location=0) in vec2 pos;\nout vec2 uv;\nvoid main() { uv = pos * 0.5 + 0.5; gl_Position = vec4(pos, 0, 1); }\n";
    const char *fs = "#version 330\nin vec2 uv;\nout vec4 o;\nvoid main() { o = vec4(dFdx(uv.x) * 8.0, dFdy(uv.y) * 8.0, fwidth(uv.x) * 4.0, 1.0); }\n";
    GLuint p = prog(vs, fs, log, sizeof log);
    CHECK(p != 0, "programa com dFdx liga");
    if (!p) { gl_ctx_destroy(C); return; }
    GLuint vao, vb;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    float pos[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    glGenBuffers(1, &vb);
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferData(GL_ARRAY_BUFFER, sizeof pos, pos, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, 0);
    glEnableVertexAttribArray(0);
    glUseProgram(p);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    u8 o[4];
    px(3, 3, o);
    CHECK(near(o[0], 255, 2) && near(o[1], 255, 2) && near(o[2], 128, 2), "dFdx/dFdy/fwidth %d %d %d", o[0], o[1], o[2]);
    /* mipmap pela lod implícita: 4x4 vermelho, 2x2 verde, 1x1 azul; desenhado em 2x2 pixels -> nível 1 */
    const char *fs2 = "#version 330\nin vec2 uv;\nuniform sampler2D t;\nout vec4 o;\nvoid main() { o = texture(t, uv); }\n";
    GLuint p2 = prog(vs, fs2, log, sizeof log);
    CHECK(p2 != 0, "programa com textura liga");
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    u8 l0[64], l1[16], l2[4] = {0, 0, 255, 255};
    for (int i = 0; i < 16; i++) { l0[i * 4] = 255; l0[i * 4 + 1] = 0; l0[i * 4 + 2] = 0; l0[i * 4 + 3] = 255; }
    for (int i = 0; i < 4; i++) { l1[i * 4] = 0; l1[i * 4 + 1] = 255; l1[i * 4 + 2] = 0; l1[i * 4 + 3] = 255; }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, l0);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, l1);
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, l2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glUseProgram(p2);
    glViewport(0, 0, 2, 2);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    px(0, 0, o);
    CHECK(o[1] == 255 && o[0] == 0 && o[2] == 0, "lod implícita escolhe o nível 1 %d %d %d", o[0], o[1], o[2]);
    glViewport(0, 0, 8, 8);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    px(5, 5, o);
    CHECK(o[0] == 255 && o[1] == 0, "magnificação usa o nível 0 %d %d %d", o[0], o[1], o[2]);
    /* anisotropia: pegada de 16x4 texels; sem ela nível 4 (verde), com 4x nível 2 (azul) */
    const char *fs3 = "#version 330\nin vec2 uv;\nuniform sampler2D t;\nout vec4 o;\nvoid main() { o = texture(t, vec2(uv.x * 2.0, uv.y * 0.5)); }\n";
    GLuint p3 = prog(vs, fs3, log, sizeof log);
    CHECK(p3 != 0, "programa anisotrópico liga");
    static u8 lv[64 * 64 * 4];
    for (int l = 0, w = 64; w >= 1; l++, w /= 2) {
        for (int i = 0; i < w * w; i++) { lv[i * 4] = l == 2 || l == 4 ? 0 : 255; lv[i * 4 + 1] = l == 4 ? 255 : 0; lv[i * 4 + 2] = l == 2 ? 255 : 0; lv[i * 4 + 3] = 255; }
        glTexImage2D(GL_TEXTURE_2D, l, GL_RGBA8, w, w, 0, GL_RGBA, GL_UNSIGNED_BYTE, lv);
    }
    glUseProgram(p3);
    glViewport(0, 0, 8, 8);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    px(4, 4, o);
    CHECK(o[1] == 255 && o[2] == 0, "sem anisotropia: nível 4 (%d %d %d)", o[0], o[1], o[2]);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 4);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    px(4, 4, o);
    CHECK(o[2] == 255 && o[1] == 0, "anisotropia 4x: nível 2 (%d %d %d)", o[0], o[1], o[2]);
    gl_ctx_destroy(C);
}

static GLuint prog3(const char *vs, const char *gs, const char *fs, char *log, int nlog) {
    const char *src[3] = {vs, gs, fs};
    GLenum ty[3] = {GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER};
    GLuint p = glCreateProgram();
    for (int i = 0; i < 3; i++) {
        GLuint s = glCreateShader(ty[i]);
        glShaderSource(s, 1, &src[i], 0);
        glCompileShader(s);
        GLint ok;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) { glGetShaderInfoLog(s, nlog, 0, log); printf("shader %d: %s\n", i, log); return 0; }
        glAttachShader(p, s);
        glDeleteShader(s);
    }
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(p, nlog, 0, log); printf("link: %s\n", log); return 0; }
    return p;
}
static void t_geometry(void) {
    gl_ctx_t *C = gl_ctx_create(0, 1, 4, 1, 8, 8, 1, 1, 1, 1);
    gl_ctx_make_current(C);
    char log[1024];
    const char *vs = "#version 330\nlayout(location=0) in vec2 pos;\nout vec3 vc;\nvoid main() { vc = vec3(1.0, 0.0, 0.0); gl_Position = vec4(pos, 0, 1); }\n";
    const char *gs = "#version 330\nlayout(points) in;\nlayout(triangle_strip, max_vertices = 4) out;\n"
                     "in vec3 vc[];\nout vec3 gc;\n"
                     "void main() { vec4 c = gl_in[0].gl_Position; float h = 0.5;\n"
                     "  gc = vc[0]; gl_Position = c + vec4(-h, -h, 0, 0); EmitVertex();\n"
                     "  gc = vc[0]; gl_Position = c + vec4( h, -h, 0, 0); EmitVertex();\n"
                     "  gc = vec3(0, 0, 1); gl_Position = c + vec4(-h,  h, 0, 0); EmitVertex();\n"
                     "  gc = vec3(0, 0, 1); gl_Position = c + vec4( h,  h, 0, 0); EmitVertex(); EndPrimitive(); }\n";
    const char *fs = "#version 330\nin vec3 gc;\nout vec4 o;\nvoid main() { o = vec4(gc, 1.0); }\n";
    GLuint p = prog3(vs, gs, fs, log, sizeof log);
    CHECK(p != 0, "programa com geometry shader liga");
    if (!p) { gl_ctx_destroy(C); return; }
    GLuint vao, vb;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    float pt[2] = {0, 0};
    glGenBuffers(1, &vb);
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferData(GL_ARRAY_BUFFER, sizeof pt, pt, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, 0);
    glEnableVertexAttribArray(0);
    glUseProgram(p);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_POINTS, 0, 1);
    u8 o[4];
    px(4, 2, o);
    CHECK(o[0] > 128 && o[2] < 128, "GS: ponto virou quadrado (baixo vermelho) %d %d %d", o[0], o[1], o[2]);
    px(4, 5, o);
    CHECK(o[2] > 128 && o[0] < 128, "GS: cor interpolada até o azul %d %d %d", o[0], o[1], o[2]);
    px(0, 0, o);
    CHECK(o[0] == 0 && o[2] == 0, "GS: fora do quadrado fica limpo %d %d %d", o[0], o[1], o[2]);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK(glGetError() == GL_INVALID_OPERATION, "modo do draw diferente da entrada do GS");
    /* triângulos com bloco de interface e gl_PrimitiveIDIn */
    const char *vs2 = "#version 330\nlayout(location=0) in vec2 pos;\nout V { vec2 p; } v;\nvoid main() { v.p = pos; gl_Position = vec4(pos, 0, 1); }\n";
    const char *gs2 = "#version 330\nlayout(triangles) in;\nlayout(triangle_strip, max_vertices = 3) out;\n"
                      "in V { vec2 p; } v[];\nflat out float k;\n"
                      "void main() { for (int i = 0; i < gl_in.length(); i++) { k = float(gl_PrimitiveIDIn) + 0.5 + v[i].p.x * 0.0; gl_Position = gl_in[i].gl_Position; EmitVertex(); } EndPrimitive(); }\n";
    const char *fs2 = "#version 330\nflat in float k;\nout vec4 o;\nvoid main() { o = vec4(0, k, 0, 1); }\n";
    GLuint p2 = prog3(vs2, gs2, fs2, log, sizeof log);
    CHECK(p2 != 0, "GS de triângulos com bloco liga");
    if (p2) {
        float tri[] = {-1, -1, 1, -1, -1, 1};
        glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
        glUseProgram(p2);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        px(1, 1, o);
        CHECK(near(o[1], 128, 2) && o[0] == 0, "GS: triângulo passa com gl_PrimitiveIDIn %d %d %d", o[0], o[1], o[2]);
    }
    gl_ctx_destroy(C);
}


/* multiamostra no GLSL: sombreamento por amostra (gl_SampleID), texelFetch de sampler2DMS, gl_SampleMask e centroid */
static void t_msaa_glsl(void) {
    gl_ctx_t *C = gl_ctx_create(0, 1, 4, 1, 8, 8, 1, 1, 1, 1);
    gl_ctx_make_current(C);
    char log[1024];
    const char *vs = "#version 410 core\nlayout(location = 0) in vec2 pos;\ncentroid out float cu; out float u;\n"
                     "void main() { cu = pos.x / 8.0; u = pos.x / 8.0; gl_Position = vec4(pos / 4.0 - 1.0, 0.0, 1.0); }\n";
    const char *fs_id = "#version 410 core\nout vec4 c;\nvoid main() { c = vec4(float(gl_SampleID) / 3.0, 0.0, 0.0, 1.0); }\n";
    const char *fs_rd = "#version 410 core\nuniform sampler2DMS t; uniform int s;\nout vec4 c;\n"
                        "void main() { ivec2 sz = textureSize(t); c = texelFetch(t, ivec2(gl_FragCoord.xy), s); if (sz.x != 8) c = vec4(0.0, 1.0, 0.0, 1.0); }\n";
    const char *fs_mask = "#version 410 core\nout vec4 c;\nvoid main() { gl_SampleMask[0] = 1; c = vec4(1.0); }\n";
    const char *fs_cen = "#version 410 core\ncentroid in float cu; in float u; uniform int which;\nout vec4 c;\n"
                         "void main() { c = vec4(which == 1 ? cu : u, 0.0, 0.0, 1.0); }\n";
    GLuint p_id = prog(vs, fs_id, log, sizeof log), p_rd = prog(vs, fs_rd, log, sizeof log), p_mask = prog(vs, fs_mask, log, sizeof log), p_cen = prog(vs, fs_cen, log, sizeof log);
    CHECK(p_id && p_rd && p_mask && p_cen, "programas de multiamostra compilam");
    if (!p_id || !p_rd || !p_mask || !p_cen) { gl_ctx_destroy(C); return; }
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    float full[] = {0, 0, 8, 0, 8, 8, 0, 0, 8, 8, 0, 8, /* borda em x = 2.4 */ 0, 0, 2.4f, 0, 2.4f, 8, 0, 0, 2.4f, 8, 0, 8};
    glBufferData(GL_ARRAY_BUFFER, sizeof full, full, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    GLuint mt, fb;
    glGenTextures(1, &mt);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mt);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, 8, 8, GL_TRUE);
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, mt, 0);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "FBO com textura 4x");
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(p_id);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    /* lê de volta cada amostra com texelFetch */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(p_rd);
    glUniform1i(glGetUniformLocation(p_rd, "t"), 0);
    u8 o[4];
    for (int smp = 0; smp < 4; smp++) {
        glUniform1i(glGetUniformLocation(p_rd, "s"), smp);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        px(3, 3, o);
        CHECK(near(o[0], smp * 85, 1) && o[1] == 0, "gl_SampleID %d por texelFetch: %d %d", smp, o[0], o[1]);
    }
    /* gl_SampleMask = 1: só a amostra 0; o blit resolve em 1/4 */
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(p_mask);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, 8, 8, 0, 0, 8, 8, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    px(3, 3, o);
    CHECK(near(o[0], 64, 1), "gl_SampleMask[0] = 1: %d", o[0]);
    /* centroid: o centro do pixel 2 (2.5) fica fora da borda 2.4; a amostra 0 (2.375) está dentro */
    for (int which = 0; which < 2; which++) {
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(p_cen);
        glUniform1i(glGetUniformLocation(p_cen, "which"), which);
        glDrawArrays(GL_TRIANGLES, 6, 6);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(p_rd);
        glUniform1i(glGetUniformLocation(p_rd, "s"), 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        px(2, 3, o);
        if (which) CHECK(near(o[0], 76, 1), "centroid na amostra coberta: %d", o[0]);
        else CHECK(near(o[0], 80, 1), "sem centroid: valor do centro %d", o[0]);
    }
    CHECK(glGetError() == GL_NO_ERROR, "sem erro (GLSL multisample)");
    gl_ctx_destroy(C);
}


/* tesselação: contagem de primitivos por domínio/nível, espaçamento fracionário, barrier() no controle, patch out,
 * níveis padrão sem controle, point_mode, isolinhas, GS depois da avaliação e os erros de draw */
typedef struct { float u, v, w; } tess_pt_t;
int gl_tess_gen(GLenum mode, GLenum sp, int cw, int point, const float outer[4], const float inner[2],
                tess_pt_t **pts, int *npts, u32 **ix, int *nix, int *pv);
void free(void *);
static GLuint progN(const char **src, const GLenum *ty, int n, char *log, int nlog) {
    GLuint p = glCreateProgram();
    for (int i = 0; i < n; i++) {
        GLuint s = glCreateShader(ty[i]);
        glShaderSource(s, 1, &src[i], 0);
        glCompileShader(s);
        GLint ok;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) { glGetShaderInfoLog(s, nlog, 0, log); printf("shader %d: %s\n", i, log); return 0; }
        glAttachShader(p, s);
        glDeleteShader(s);
    }
    glBindAttribLocation(p, 0, "pos");
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(p, nlog, 0, log); printf("link: %s\n", log); return 0; }
    return p;
}
static int tess_count(GLuint p, int nverts) {
    GLuint q;
    glGenQueries(1, &q);
    glBeginQuery(GL_PRIMITIVES_GENERATED, q);
    glUseProgram(p);
    glDrawArrays(GL_PATCHES, 0, nverts);
    glEndQuery(GL_PRIMITIVES_GENERATED);
    GLuint n = 0;
    glGetQueryObjectuiv(q, GL_QUERY_RESULT, &n);
    glDeleteQueries(1, &q);
    return (int)n;
}
static void t_tess(void) {
    /* gerador: subdivisão fracionária de uma aresta (isolinhas: outer[1] = segmentos) */
    {
        tess_pt_t *pt; u32 *ix; int np, ni, pv;
        float o[4] = {1, 3, 0, 0}, in[2] = {0, 0};
        int ok = gl_tess_gen(GL_ISOLINES, GL_FRACTIONAL_EVEN, 0, 0, o, in, &pt, &np, &ix, &ni, &pv);
        float want[5] = {0, 1.0f / 3, 0.5f, 2.0f / 3, 1};
        int good = ok && np == 5 && ni == 8 && pv == 2;
        for (int i = 0; good && i < 5; i++) if (pt[i].u < want[i] - 1e-5f || pt[i].u > want[i] + 1e-5f) good = 0;
        CHECK(good, "fractional_even 3 -> 4 segmentos (dois curtos no meio): np %d ni %d", np, ni);
        free(pt); free(ix);
        o[1] = 2;
        ok = gl_tess_gen(GL_ISOLINES, GL_FRACTIONAL_ODD, 0, 0, o, in, &pt, &np, &ix, &ni, &pv);
        float w2[4] = {0, 0.25f, 0.75f, 1};
        good = ok && np == 4;
        for (int i = 0; good && i < 4; i++) if (pt[i].u < w2[i] - 1e-5f || pt[i].u > w2[i] + 1e-5f) good = 0;
        CHECK(good, "fractional_odd 2 -> 3 segmentos (curto, longo, curto): np %d", np);
        free(pt); free(ix);
        o[0] = 0; o[1] = 4;
        CHECK(!gl_tess_gen(GL_ISOLINES, GL_EQUAL, 0, 0, o, in, &pt, &np, &ix, &ni, &pv), "nível externo 0 descarta o patch");
        /* triângulos com cw: área negativa no plano (u, v) */
        float t3[4] = {3, 3, 3, 0}, i3[2] = {3, 0};
        ok = gl_tess_gen(GL_TRIANGLES, GL_EQUAL, 1, 0, t3, i3, &pt, &np, &ix, &ni, &pv);
        int allcw = ok && ni == 13 * 3;
        for (int i = 0; allcw && i + 2 < ni; i += 3) {
            tess_pt_t *a = &pt[ix[i]], *b = &pt[ix[i + 1]], *c = &pt[ix[i + 2]];
            if ((b->u - a->u) * (c->v - a->v) - (c->u - a->u) * (b->v - a->v) >= 0) allcw = 0;
        }
        CHECK(allcw, "triângulos nível 3: 13 em sentido horário (%d índices)", ni);
        int sumok = 1;
        for (int i = 0; ok && i < np; i++) { float s = pt[i].u + pt[i].v + pt[i].w; if (s < 0.9999f || s > 1.0001f) sumok = 0; }
        CHECK(sumok, "u + v + w = 1 em todos os pontos do triângulo");
        free(pt); free(ix);
    }
    gl_ctx_t *C = gl_ctx_create(0, 1, 4, 1, 8, 8, 1, 1, 1, 1);
    gl_ctx_make_current(C);
    char log[1024];
    const char *vs = "#version 410 core\nlayout(location = 0) in vec2 pos;\nout vec2 vp;\nvoid main() { vp = pos; gl_Position = vec4(pos, 0.0, 1.0); }\n";
    const char *tcs = "#version 410 core\nlayout(vertices = 4) out;\nin vec2 vp[];\nout vec2 cp[];\nout float tv[];\npatch out float pk;\nuniform float lvl;\n"
                      "void main() {\n  cp[gl_InvocationID] = vp[gl_InvocationID]; tv[gl_InvocationID] = float(gl_InvocationID + 1);\n"
                      "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n  barrier();\n"
                      "  if (gl_InvocationID == 0) {\n    float l = lvl > 0.0 ? lvl : tv[3] - 2.0;\n"
                      "    gl_TessLevelOuter[0] = l; gl_TessLevelOuter[1] = l; gl_TessLevelOuter[2] = l; gl_TessLevelOuter[3] = l;\n"
                      "    gl_TessLevelInner[0] = l; gl_TessLevelInner[1] = l; pk = (tv[1] + tv[2] + tv[3]) / 10.0; }\n}\n";
    const char *tes = "#version 410 core\nlayout(quads, equal_spacing, ccw) in;\nin vec2 cp[];\npatch in float pk;\nout vec3 col;\nuniform float zoff;\n"
                      "void main() { vec2 a = mix(cp[0], cp[1], gl_TessCoord.x), b = mix(cp[3], cp[2], gl_TessCoord.x);\n"
                      "  gl_Position = vec4(mix(a, b, gl_TessCoord.y), zoff, 1.0); col = vec3(gl_TessCoord.xy, pk); }\n";
    const char *fs = "#version 410 core\nin vec3 col;\nout vec4 c;\nvoid main() { c = vec4(col, 1.0); }\n";
    const char *s4[4] = {vs, tcs, tes, fs};
    GLenum t4[4] = {GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_FRAGMENT_SHADER};
    GLuint p = progN(s4, t4, 4, log, sizeof log);
    CHECK(p != 0, "programa com tesselação liga");
    if (!p) { gl_ctx_destroy(C); return; }
    GLint v = 0;
    glGetProgramiv(p, GL_TESS_GEN_MODE, &v);
    CHECK(v == GL_QUADS, "GL_TESS_GEN_MODE %x", v);
    glGetProgramiv(p, GL_TESS_CONTROL_OUTPUT_VERTICES, &v);
    CHECK(v == 4, "GL_TESS_CONTROL_OUTPUT_VERTICES %d", v);
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    float quad[] = {-1, -1, 1, -1, 1, 1, -1, 1};
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glPatchParameteri(GL_PATCH_VERTICES, 4);
    glUseProgram(p);
    GLint ll = glGetUniformLocation(p, "lvl");
    CHECK(ll >= 0 && glGetUniformLocation(p, "zoff") >= 0, "uniforms do controle e da avaliação");
    /* barrier(): o nível vem de tv[3] (escrito pela invocação 3) - 2 = 2: quads 2/2 -> 8 triângulos */
    glUniform1f(ll, 0);
    CHECK(tess_count(p, 4) == 8, "barrier() no controle: quads nível 2 = 8 triângulos (%d)", tess_count(p, 4));
    glUniform1f(ll, 1);
    CHECK(tess_count(p, 4) == 2, "quads nível 1 = 2 triângulos");
    glUniform1f(ll, 3);
    CHECK(tess_count(p, 4) == 18, "quads nível 3 = 18 triângulos (%d)", tess_count(p, 4));
    glUniform1f(ll, 0);
    CHECK(tess_count(p, 8) == 16, "dois patches = 16 triângulos");
    /* desenho: gl_TessCoord interpolado na tela e o patch out */
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_PATCHES, 0, 4);
    u8 o[4];
    px(1, 1, o);
    CHECK(near(o[0], 48, 3) && near(o[1], 48, 3) && near(o[2], 230, 2), "cor em (1,1): %d %d %d", o[0], o[1], o[2]);
    px(6, 2, o);
    CHECK(near(o[0], 207, 3) && near(o[1], 80, 3), "cor em (6,2): %d %d %d", o[0], o[1], o[2]);
    /* erros */
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK(glGetError() == GL_INVALID_OPERATION, "GL_TRIANGLES com tesselação: erro");
    /* sem controle: níveis padrão de glPatchParameterfv; triângulos com point_mode e isolinhas */
    const char *tes_t = "#version 410 core\nlayout(triangles, fractional_odd_spacing, point_mode) in;\nin vec2 vp[];\n"
                        "void main() { gl_Position = vec4(vp[0] * gl_TessCoord.x + vp[1] * gl_TessCoord.y + vp[2] * gl_TessCoord.z, 0.0, 1.0); gl_PointSize = 1.0; }\n";
    const char *fs1 = "#version 410 core\nout vec4 c;\nvoid main() { c = vec4(1.0); }\n";
    const char *s3[3] = {vs, tes_t, fs1};
    GLenum t3[3] = {GL_VERTEX_SHADER, GL_TESS_EVALUATION_SHADER, GL_FRAGMENT_SHADER};
    GLuint p2 = progN(s3, t3, 3, log, sizeof log);
    CHECK(p2 != 0, "avaliação sem controle liga");
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    float ol[4] = {2, 2, 2, 2}, il[2] = {2, 2};
    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, ol);
    glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, il);
    float got[4] = {0};
    glGetFloatv(GL_PATCH_DEFAULT_OUTER_LEVEL, got);
    CHECK(got[0] == 2 && got[3] == 2, "GL_PATCH_DEFAULT_OUTER_LEVEL");
    /* fractional_odd 2 -> 3 segmentos por aresta: 9 pontos na borda + 3 do anel interno = 12 pontos */
    if (p2) CHECK(tess_count(p2, 3) == 12, "point_mode: 12 pontos (%d)", tess_count(p2, 3));
    const char *tes_i = "#version 410 core\nlayout(isolines) in;\nin vec2 vp[];\n"
                        "void main() { gl_Position = vec4(mix(vp[0], vp[1], gl_TessCoord.x), gl_TessCoord.y, 1.0); }\n";
    const char *si[3] = {vs, tes_i, fs1};
    GLuint p3 = progN(si, t3, 3, log, sizeof log);
    float oi[4] = {3, 4, 0, 0};
    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, oi);
    glPatchParameteri(GL_PATCH_VERTICES, 2);
    if (p3) CHECK(tess_count(p3, 2) == 12, "isolinhas 3 x 4 = 12 segmentos (%d)", tess_count(p3, 2));
    /* GS depois da avaliação: recebe triângulos */
    const char *tes_g = "#version 410 core\nlayout(triangles) in;\nin vec2 vp[];\n"
                        "void main() { gl_Position = vec4(vp[0] * gl_TessCoord.x + vp[1] * gl_TessCoord.y + vp[2] * gl_TessCoord.z, 0.0, 1.0); }\n";
    const char *gs = "#version 410 core\nlayout(triangles) in;\nlayout(points, max_vertices = 1) out;\nvoid main() { gl_Position = gl_in[0].gl_Position; EmitVertex(); EndPrimitive(); }\n";
    const char *sg[4] = {vs, tes_g, gs, fs1};
    GLenum tg[4] = {GL_VERTEX_SHADER, GL_TESS_EVALUATION_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER};
    GLuint p4 = progN(sg, tg, 4, log, sizeof log);
    CHECK(p4 != 0, "tesselação + geometry shader liga");
    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, ol);
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    if (p4) CHECK(tess_count(p4, 3) == 6, "GS recebe os 6 triângulos de nível 2 (%d)", tess_count(p4, 3));
    glUseProgram(0);
    glDrawArrays(GL_PATCHES, 0, 3);
    CHECK(glGetError() == GL_INVALID_OPERATION, "GL_PATCHES sem tesselação: erro");
    gl_ctx_destroy(C);
}

void gl_glsl_host_tests(int *fails, int *checks) {
    F = fails;
    K = checks;
    t_core_basic();
    t_texture_discard_ubo();
    t_compat_glsl();
    t_xfb();
    t_bindable();
    t_derivatives();
    t_geometry();
    t_msaa_glsl();
    t_tess();
}
