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

void gl_glsl_host_tests(int *fails, int *checks) {
    F = fails;
    K = checks;
    t_core_basic();
    t_texture_discard_ubo();
    t_compat_glsl();
    t_xfb();
    t_bindable();
}
