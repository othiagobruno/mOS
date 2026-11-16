# Frente 14 — OpenGL, Metal e Core Animation (dono: Otavio)

Plano e estado honesto da frente. Atualizado a cada PR. Docs de detalhe: [metal.md](metal.md),
[metallib-air.md](metallib-air.md), [metal-gpu-needs.md](metal-gpu-needs.md), [gpu-abi.md](gpu-abi.md),
[opengl.md](opengl.md).

## Levantamento (15 nov. 2026, `main` 97c611b)

| Área | Estado na `main` | O que falta |
|---|---|---|
| CGL / NSOpenGL | ✅ (PR 2) framebuffer real, `flushBuffer` no layer, offscreen, `CGLTexImageIOSurface2D`; antes: objetos sem framebuffer | Framebuffer real por contexto, `flushBuffer` que apresenta pixels na view, `CGLChoosePixelFormat` aceitando fora do Chromium, `CGLTexImageIOSurface2D` (13 apps), pbuffers/offscreen, contextos compartilhados |
| OpenGL (`gl*`) | 🟡 (PR 2) 1203 de 1203 comandos, 2.1 compat + 4.1 core por software, GLSL 1.10–4.10; ver [opengl.md](opengl.md) | GL 2.1 compat (fixed function: matrizes, `glBegin`, luz, texenv, fog, listas) e GL 3.2–4.1 core (VBO/VAO/EBO, FBO/RBO, texturas 1D/2D/3D/cube/array/rect, GLSL 1.10–4.10, UBO, instancing, transform feedback, queries, sync), `glGetString`/`glGetIntegerv` coerentes |
| Metal (software) | API ampla + rasterizador + MSL + AIR com 17 opcodes (`metal.c`, `metalexec.c`, `mtlshader.c`, `mtlexec.c`, `mtlair*`) | AIR: `double`, atômicos, `fcmp` não ordenado, pack/unpack, conferência com `.metallib` de app; MSL: templates, métodos em struct, argument buffers, function constants; derivadas reais (`dfdx` = 0 hoje); simdgroup com mais de 1 thread; indirect command buffers, argument encoders, binary archives, encoders paralelos (hoje nil) |
| Metal na GPU AMD | ABI r2: BO, IOSurface na memória da GPU, lote de compute validado; sem execução de shader | Compilador para a ISA GFX9 (Vega/Raven2) e submit pelo driver da Thais (frente 1). Depende de rings GFX/compute e gerência de VRAM dela |
| Core Animation | Animações, shape/gradient/text layers, `CADisplayLink` | Transform 3D com rotação e perspectiva no compositor, `CAReplicatorLayer` gerando réplicas, `CAEmitterLayer` com partículas, `CATiledLayer` com tiles, `CARenderer` desenhando, `CATransaction` com animação implícita completa |
| IOSurface | Criação, lock, planos, id global, BO da ABI | `IOSurfaceCreateXPCObject`/`LookupFromXPCObject` (hoje 0), mach port (`IOSurfaceCreateMachPort`), elementos de bloco (formatos comprimidos), uso entre processos pelo compositor |
| CVDisplayLink | Thread de 60 Hz (`corevideo.c`) | Período real do modo de vídeo (KMS da Thais), vários monitores, `CVDisplayLinkTranslateTime` |

## Ordem de trabalho

1. **OpenGL por software** (este PR em diante). Arquitetura:
   - `gl.c`: estado do contexto GL, objetos (buffers, texturas, VAO, FBO, RBO, programas, queries), API `gl*`
     com erros `GL_INVALID_*` e `glGet*` coerentes.
   - `glraster.c`: rasterizador próprio de GL (recorte em −w..w, viewport/depth range, polygon offset,
     culling, scissor, depth/stencil, blending, logic op, color mask, pontos e linhas). Texturas usam o
     `msl_tex_t` e a amostragem do Metal (`msl_tex_sample`), os mesmos formatos.
   - Fixed function em C (matrizes, `glBegin`/`glEnd`, arrays de cliente, luz, material, texenv/combine,
     fog, alpha test, listas de exibição).
   - **GLSL**: `glsl.c` traduz GLSL para o dialeto do compilador MSL do mOS (`mtlshader.c`), com prelúdio
     (tipos `vec*`/`mat*`, `sampler*` como par textura+sampler, `mod`, `texture*`, `lessThan`...). `main`
     vira função auxiliar; uniforms, `in`/`out` e `gl_*` são globais no `gmem` da biblioteca, escritos antes
     de cada invocação. O interpretador é o do Metal.
   - CGL/NSOpenGL: framebuffer por contexto, `flushBuffer` copia o back buffer para um IOSurface que vira
     `contents` do layer da view.
2. **Core Animation**: transform 3D completo no compositor do QuartzCore, réplicas, partículas, tiles,
   `CARenderer`.
3. **IOSurface/CVDisplayLink**: XPC/mach port, entre processos, período do KMS.
4. **Metal**: lacunas do MSL/AIR, derivadas reais (quads 2×2 no rasterizador), conferência com `.metallib`
   de app, objetos que hoje são nil.
5. **GPU AMD** com a Thais: backend GFX9 do interpretador (compilador de shader) e submit 3D.

Cada passo tem teste de host em `tools/` e entra por PR na `main`. Build completo só no fim
(`build-final.sh`).

## Progresso

| PR | O que entrou | Teste |
|---|---|---|
| #243 | Plano; `mtltex.c`; cabeçalhos GL gerados do registro da Khronos | sintaxe freestanding |
| (este) | OpenGL por software inteiro (fixed function, GLSL, FBO, UBO, transform feedback, queries), CGL/NSOpenGL apresentando na view; compilador MSL com modo GLSL; swizzle e cor de borda no amostrador | `gl_host` 66/66, `nsopengl_host` 0 falhas, `host-metal-gpu` ok |
| #341 | Avaliadores e `glAccum` (`gl_eval.c`) | `gl_host` 73/73 |
| #359 | `ARB_vertex_program`/`ARB_fragment_program`, `EXT_bindable_uniform`: todos os 1203 comandos | `gl_host` 83/83, `nsopengl_host` 0 falhas |
| #363 | Derivadas no GLSL por reexecução e lod implícita de mipmap | `gl_host` 88/88, `host-metal-gpu` ok |
| #408 | Geometry shader | `gl_host` 95/95 |
| #449 | Lacunas do corpus: `NSOpenGLGetOption`/`SetOption`/`GetVersion`, `CVOpenGLTextureCache`, CoreVideo planar e anexos, descritores de raytracing do Metal, `setVertexBuffers` do overlay Swift | `nsopengl_host` 0 falhas, `mtl_vbufs_test` ok |
| (este) | Pontilhado de linha e de polígono | `gl_host` 106/106 |

## Próximo (OpenGL)

MSAA com amostras; anisotropia; `KHR_debug` com mensagens; tesselação; teste com app real na VM.
