# OpenGL no mOS (por software)

O `OpenGL.framework` do mOS é um renderizador de software no libSystem. Com a CGL/NSOpenGL ele entrega o que o
renderizador genérico do macOS entrega: **2.1 compatibility** (padrão) e **4.1 core** (`kCGLPFAOpenGLProfile`
`kCGLOGLPVersion_3_2_Core` ou `GL4_Core`). `glGetString(GL_RENDERER)` = "mOS Software Renderer". Não há GPU no
caminho: a aceleração pela Radeon Vega do Thiago depende do driver da frente 1 (ver
[opengl-metal-ca-plano.md](opengl-metal-ca-plano.md)).

## Arquivos (`userland/libsystem`)

| Arquivo | O que faz |
|---|---|
| `gl_int.h`, `gl_enum.h`, `gl_proto.h` | Estado interno; constantes e protótipos gerados do `gl.xml` da Khronos por `tools/gl_gen.py` (o compilador confere cada assinatura) |
| `gl_ctx.c` | Contexto, grupo compartilhado, nomes, erros, enable/disable, estado fixo, `glGet*` (todos os tipos e os indexados), `glGetString(i)`, extensões |
| `gl_buf.c` | Buffers (todos os alvos, map/flush/copy), VAOs (inclusive APPLE), atributos e `glVertexAttrib*` |
| `gl_tex.c` | Texturas 1D/2D/3D/cube/rect/arrays/buffer/multisample, todos os formatos internos do 4.1 e do compat (luminância, alfa, intensidade via swizzle), transferência de pixels em todos os formatos/tipos, S3TC e RGTC, mipmaps, samplers, `glTexStorage*`, `glCopyTex*`, `CGLTexImageIOSurface2D` |
| `gl_fbo.c` | Renderbuffers, FBOs (EXT e core), completude, draw/read buffers, `glClear(Buffer*)`, `glReadPixels`, `glBlitFramebuffer` |
| `gl_raster.c` | Pipeline: índices e primitive restart, cache de vértices, montagem de todas as primitivas (inclusive quads, polígonos e adjacência), transform feedback, recorte (−w..w, depth clamp, 8 planos), viewport, culling, polygon mode/offset, triângulos (top-left, perspectiva, flat/noperspective), linhas largas, pontos/sprites, scissor, alpha test, stencil, depth, blending (dual source), logic op, sRGB, máscaras, seleção e feedback |
| `gl_ffp.c` | Fixed function: matrizes, `glBegin/glEnd`, arrays de cliente (no VAO), luz completa, texgen, texenv/combine, fog, raster pos, `glDrawPixels`/`glBitmap`/`glCopyPixels`, seleção, `glPushAttrib`, listas de exibição |
| `gl_glsl*.c` | GLSL 1.10–4.10: tradução para o dialeto do compilador de MSL, prelúdio com tipos, funções embutidas e todas as funções de textura, link, uniforms, blocos std140, embutidas do compat, programas separáveis e pipelines, `ARB_shader_objects` |
| `gl_eval.c` | Avaliadores (`glMap*`, `glEval*`, `glMapGrid*`, `GL_AUTO_NORMAL`) e buffer de acumulação (`glAccum`) |
| `gl_arbprog.c` | `ARB_vertex_program`/`ARB_fragment_program`: o assembly vira GLSL 1.20 compat (todas as instruções, vínculos `vertex.*`/`fragment.*`/`result.*`/`state.*`, `program.env/local`, endereçamento relativo, `TEX/TXB/TXP`, `KIL`, `_SAT`, opções de fog e posição invariante) |
| `gl_misc.c` | Queries, sync, `APPLE_fence`, objetos de transform feedback, renderização condicional |
| `mtltex.c` | Formatos de pixel e amostragem, divididos com o Metal |
| `appkit_nsopengl.c` | CGL e NSOpenGL ligados ao contexto GL: framebuffer do tamanho da view, `flushBuffer` → IOSurface BGRA no `contents` do layer, `CGLSetOffScreen`, `kCGLCPSurfaceBackingSize` |

## Como o GLSL roda

O texto passa pelo pré-processador do compilador de MSL (`msl_preprocess`), pela tradução (`gl_glsl_tr.c`) e é
compilado com o prelúdio (`gl_glsl_pre.c`) no modo GLSL do `mtlshader.c` (swizzle `stpq`; `==` de vetores devolve um
bool). `main` roda no interpretador do Metal (`mtlexec.c`). Uniforms, `in`/`out` e `gl_*` são globais do `gmem`:
o rasterizador escreve atributos/varyings antes de cada invocação e lê as saídas depois. `sampler*` é um par
textura + sampler preenchido a cada draw com a unidade escolhida por `glUniform1i`.

## Testes de host

```
L=userland/libsystem
gcc -std=gnu11 -O1 -w -fno-builtin -I tools/hostinc -I $L -o /tmp/gl_host tools/gl_host.c tools/gl_host_glsl.c \
    $L/gl_ctx.c $L/gl_buf.c $L/gl_tex.c $L/gl_fbo.c $L/gl_raster.c $L/gl_ffp.c $L/gl_misc.c $L/gl_eval.c $L/gl_arbprog.c $L/gl_glsl.c \
    $L/gl_glsl_tr.c $L/gl_glsl_pre.c $L/mtltex.c $L/mtlshader.c $L/mtlexec.c $L/mslspirv.c -lm -lpthread && /tmp/gl_host
```

Resultado registrado (16 nov.): `gl_host: 95/95 ok`. Confere clear/scissor, cobertura exata de um triângulo
(496 pixels em 32×32), gradiente, flat, depth, stencil, blending, textura nearest/modulate/luminância, BGRA, DXT1,
FBO + blit + renderbuffer, lista de exibição, luz difusa, fog, seleção, arrays de cliente, VBO/EBO, VAO do APPLE com
buffer apagado, GLSL 4.10 core (atributos, uniforms, `out`, função com `out`, erro de compilação no log), textura +
`discard` + UBO std140 (offsets e tamanho), GLSL 1.10 compat (`ftransform`, `gl_Color`, `gl_TexCoord`), MRT com
`gl_FragData`, transform feedback com query, avaliador 2D com cor, `glGetMap`, `glAccum`, programas ARB (vp com `state.matrix.mvp`, fp com `TEX`, `program.local`, `KIL`, `MUL_SAT`, erro de sintaxe com posição), `EXT_bindable_uniform`, `dFdx`/`dFdy`/`fwidth` com valor conhecido, nível de mipmap escolhido pela lod implícita, geometry shader (ponto → quadrado com cor interpolada, triângulos com bloco e `gl_PrimitiveIDIn`, erro de modo incompatível). Também roda no `tools/build.sh`.

`tools/nsopengl_host.c` (objetos compilados com as flags do libSystem): `CGLChoosePixelFormat`, contexto legado (2.1)
e core (4.1), `flushBuffer` pondo o quadro BGRA no layer da view, `CGLSetOffScreen`. Resultado: 0 falhas.

Não houve VM nem app real nesta entrega: nada aqui diz que um app OpenGL específico abre.

## Cobertura

1203 comandos do GL 1.0–4.1 e das extensões que o macOS anuncia estão em `tools/gl_cmds.txt`. Exportados: **1203 de 1203** (conferido com `nm` nos objetos compilados como no libSystem).

## Limites conhecidos

- Geometry shader (GL 3.2): todas as entradas (pontos, linhas, triângulos, com adjacência), saídas `points`/`line_strip`/
  `triangle_strip`, `max_vertices`, `invocations`, `gl_in[]`, `gl_PrimitiveIDIn`, blocos de interface. `EmitVertex` grava as
  saídas em arrays gerados no link. Sem renderização em camadas (`gl_Layer` é ignorado), sem `gl_ClipDistance` vindo do GS
  e sem transform feedback das saídas do GS (captura as do vértice só quando não há GS).
- Tesselação (GL 4.0): a compilação falha com mensagem no log.
- Derivadas no GLSL são por reexecução: quando o shader usa `dFdx`/`dFdy`/`fwidth` ou amostra uma textura com
  mipmap (ou filtros min e mag diferentes), o fragmento roda antes nos vizinhos x+1 e y+1 (varyings pelas derivadas
  analíticas do raster) e cada chamada usa a diferença; a lod implícita do `texture()` sai disso. Custa 3 execuções
  por pixel nesses shaders. Uma derivada cujo argumento depende de outra derivada usa 0 no vizinho.
- Uma amostra por pixel (MSAA guarda uma amostra; o resolve é cópia). Sem anisotropia real. Sem pontilhado de linha
  e de polígono no raster (o estado é guardado).
- Subrotinas do GLSL e binários de programa não existem (`GL_NUM_PROGRAM_BINARY_FORMATS` = 0).
- `KHR_debug`/`EXT_debug_label`: aceitos, sem mensagens nem rótulos guardados.
- Desempenho de CPU com o interpretador: UI, ferramentas e cenas simples; jogos pesados ficam lentos.
- Processos do Chromium/Electron continuam sem OpenGL (a CGL recusa o pixel format): eles usam o Metal/SwiftShader.
