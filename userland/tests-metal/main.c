// mOS — teste do Metal por software: compute (com threadgroup_barrier e atômicos), render de um
// triângulo com interpolação, textura com sampler, escrita em textura no kernel, blit e profundidade.
//   .\tools\build-metaltest.ps1
//   .\build\machotest.exe build\libSystem.mos build\metaltest build\metaltest\MetalTest.app\Contents\MacOS\MetalTest
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef void *id;
typedef void *SEL;
extern id objc_msgSend(id, SEL, ...);
extern id objc_getClass(const char *);
extern SEL sel_registerName(const char *);
extern id MTLCreateSystemDefaultDevice(void);
#define S(x) sel_registerName(x)
#define MS(T, o, s, ...) ((T(*)(id, SEL, ...))objc_msgSend)((id)(o), S(s), ##__VA_ARGS__)
typedef struct { uint64_t w, h, d; } MSize;
typedef struct { uint64_t x, y, z; } MOrigin;
typedef struct { MOrigin o; MSize s; } MRegion;
typedef struct { double r, g, b, a; } MClear;

static int fails;
static void check(const char *what, int ok) { printf("%s %s\n", ok ? "OK  " : "FALHA", what); if (!ok) fails++; }
static id O(id o, const char *s) { return ((id(*)(id, SEL))objc_msgSend)(o, S(s)); }
static id O1(id o, const char *s, id a) { return ((id(*)(id, SEL, id))objc_msgSend)(o, S(s), a); }
static id OU(id o, const char *s, uint64_t a) { return ((id(*)(id, SEL, uint64_t))objc_msgSend)(o, S(s), a); }
static id OE(id o, const char *s, id a, id *e) { return ((id(*)(id, SEL, id, id *))objc_msgSend)(o, S(s), a, e); }
static id str(const char *s) { return ((id(*)(id, SEL, const char *))objc_msgSend)(objc_getClass("NSString"), S("stringWithUTF8String:"), s); }

static const char *SRC =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "#define ESCALA 2\n"
    "struct VIn { float2 pos [[attribute(0)]]; float4 color [[attribute(1)]]; };\n"
    "struct VOut { float4 pos [[position]]; float4 color; float2 uv; };\n"
    "vertex VOut vs(VIn in [[stage_in]]) { VOut o; o.pos = float4(in.pos, 0.5, 1); o.color = in.color; o.uv = in.pos * 0.5 + 0.5; return o; }\n"
    "fragment float4 fs(VOut in [[stage_in]]) { return in.color; }\n"
    "constant float2 quad[] = { {-1,-1}, {1,-1}, {-1,1}, {1,1} };\n"
    "vertex VOut vs_quad(uint vid [[vertex_id]], constant float &z [[buffer(0)]]) {\n"
    "  VOut o; o.pos = float4(quad[vid], z, 1); o.color = float4(z, 1 - z, 0, 1); o.uv = quad[vid] * float2(0.5, -0.5) + 0.5; return o; }\n"
    "constexpr sampler nearest(filter::nearest, address::clamp_to_edge);\n"
    "fragment float4 fs_tex(VOut in [[stage_in]], texture2d<float> t [[texture(0)]]) { return t.sample(nearest, in.uv); }\n"
    "fragment half4 fs_col(VOut in [[stage_in]]) { return half4(in.color); }\n"
    "kernel void dbl(device float *a [[buffer(0)]], uint i [[thread_position_in_grid]]) { a[i] = a[i] * ESCALA + 1; }\n"
    "kernel void sum(device const uint *in [[buffer(0)]], device atomic_uint *out [[buffer(1)]],\n"
    "                uint tid [[thread_index_in_threadgroup]], uint gid [[thread_position_in_grid]], uint n [[threads_per_threadgroup]]) {\n"
    "  threadgroup uint sh[64];\n"
    "  sh[tid] = in[gid];\n"
    "  threadgroup_barrier(mem_flags::mem_threadgroup);\n"
    "  for (uint s = n / 2; s > 0; s >>= 1) { if (tid < s) sh[tid] += sh[tid + s]; threadgroup_barrier(mem_flags::mem_threadgroup); }\n"
    "  if (tid == 0) atomic_fetch_add_explicit(out, sh[0], memory_order_relaxed);\n"
    "}\n"
    "struct P { float3 v; uint k; };\n"
    "static float3 rot(float3 v) { float3x3 m = float3x3(float3(0,1,0), float3(-1,0,0), float3(0,0,1)); return m * v; }\n"
    "kernel void misc(device P *p [[buffer(0)]], uint i [[thread_position_in_grid]]) {\n"
    "  P x = p[i]; x.v = rot(x.v); x.v.xy += float2(10, 20); uint h = x.k * 747796405u + 2891336453u;\n"
    "  switch (i) { case 0: x.k = h; break; case 1: x.k = popcount(255u); break; default: x.k = (uint)dot(x.v, float3(1)); }\n"
    "  p[i] = x; }\n"
    "kernel void img(texture2d<float, access::write> o [[texture(0)]], uint2 p [[thread_position_in_grid]]) {\n"
    "  o.write(float4(p.x / 8.0, p.y / 8.0, 0, 1), p); }\n";

int main(void) {
    id dev = MTLCreateSystemDefaultDevice();
    check("MTLCreateSystemDefaultDevice", dev != 0);
    if (!dev) return 1;
    printf("device: %s\n", MS(const char *, O(dev, "name"), "UTF8String"));
    id err = 0;
    id lib = MS(id, dev, "newLibraryWithSource:options:error:", str(SRC), (id)0, &err);
    if (!lib && err) printf("erro: %s\n", MS(const char *, O(err, "localizedDescription"), "UTF8String"));
    check("newLibraryWithSource", lib != 0);
    if (!lib) return 1;
    id q = O(dev, "newCommandQueue");

    /* compute: dbl */
    {
        float a[100];
        for (int i = 0; i < 100; i++) a[i] = (float)i;
        id buf = MS(id, dev, "newBufferWithBytes:length:options:", a, (uint64_t)sizeof a, (uint64_t)0);
        id fn = O1(lib, "newFunctionWithName:", str("dbl"));
        id ps = OE(dev, "newComputePipelineStateWithFunction:error:", fn, &err);
        id cb = O(q, "commandBuffer");
        id e = O(cb, "computeCommandEncoder");
        MS(void, e, "setComputePipelineState:", ps);
        MS(void, e, "setBuffer:offset:atIndex:", buf, (uint64_t)0, (uint64_t)0);
        MSize n = {100, 1, 1}, t = {32, 1, 1};
        ((void (*)(id, SEL, MSize, MSize))objc_msgSend)(e, S("dispatchThreads:threadsPerThreadgroup:"), n, t);
        MS(void, e, "endEncoding");
        MS(void, cb, "commit");
        MS(void, cb, "waitUntilCompleted");
        float *r = MS(float *, buf, "contents");
        int ok = 1;
        for (int i = 0; i < 100; i++) if (r[i] != i * 2 + 1) ok = 0;
        printf("dbl: r[0]=%g r[99]=%g status=%llu\n", r[0], r[99], (unsigned long long)MS(uint64_t, cb, "status"));
        check("compute dispatchThreads (100 threads, grupos de 32)", ok && MS(uint64_t, cb, "status") == 4);
    }
    /* compute: soma com threadgroup memory + barreira + atômico */
    {
        uint32_t in[256];
        for (int i = 0; i < 256; i++) in[i] = (uint32_t)i;
        id bi = MS(id, dev, "newBufferWithBytes:length:options:", in, (uint64_t)sizeof in, (uint64_t)0);
        id bo = MS(id, dev, "newBufferWithLength:options:", (uint64_t)4, (uint64_t)0);
        id fn = O1(lib, "newFunctionWithName:", str("sum"));
        id ps = OE(dev, "newComputePipelineStateWithFunction:error:", fn, &err);
        id cb = O(q, "commandBuffer");
        id e = O(cb, "computeCommandEncoder");
        MS(void, e, "setComputePipelineState:", ps);
        MS(void, e, "setBuffer:offset:atIndex:", bi, (uint64_t)0, (uint64_t)0);
        MS(void, e, "setBuffer:offset:atIndex:", bo, (uint64_t)0, (uint64_t)1);
        MSize g = {4, 1, 1}, t = {64, 1, 1};
        ((void (*)(id, SEL, MSize, MSize))objc_msgSend)(e, S("dispatchThreadgroups:threadsPerThreadgroup:"), g, t);
        MS(void, e, "endEncoding");
        MS(void, cb, "commit");
        uint32_t s = *MS(uint32_t *, bo, "contents");
        printf("soma = %u (esperado 32640)\n", s);
        check("redução com threadgroup_barrier e atomic_fetch_add", s == 32640);
    }
    /* compute: structs, matriz, switch, inteiros */
    {
        struct { float v[4]; uint32_t k; uint32_t pad[3]; } p[3] = {{{1, 2, 3, 0}, 7}, {{0, 0, 0, 0}, 0}, {{1, 1, 1, 0}, 0}};
        id b = MS(id, dev, "newBufferWithBytes:length:options:", p, (uint64_t)sizeof p, (uint64_t)0);
        id ps = OE(dev, "newComputePipelineStateWithFunction:error:", O1(lib, "newFunctionWithName:", str("misc")), &err);
        id cb = O(q, "commandBuffer");
        id e = O(cb, "computeCommandEncoder");
        MS(void, e, "setComputePipelineState:", ps);
        MS(void, e, "setBuffer:offset:atIndex:", b, (uint64_t)0, (uint64_t)0);
        MSize n = {3, 1, 1}, t = {3, 1, 1};
        ((void (*)(id, SEL, MSize, MSize))objc_msgSend)(e, S("dispatchThreads:threadsPerThreadgroup:"), n, t);
        MS(void, e, "endEncoding");
        MS(void, cb, "commit");
        typeof(p[0]) *r = MS(void *, b, "contents");
        uint32_t h = 7u * 747796405u + 2891336453u;
        printf("misc: v0=(%g,%g,%g) k0=%u (esp %u) k1=%u k2=%u\n", r[0].v[0], r[0].v[1], r[0].v[2], r[0].k, h, r[1].k, r[2].k);
        check("struct float3 + matriz float3x3 + uint com overflow + switch", r[0].v[0] == 8 && r[0].v[1] == 21 && r[0].v[2] == 3 && r[0].k == h && r[1].k == 8 && r[2].k == 31);
    }
    /* render: triângulo com cores por vértice */
    id tex;
    {
        id td = MS(id, objc_getClass("MTLTextureDescriptor"), "texture2DDescriptorWithPixelFormat:width:height:mipmapped:", (uint64_t)70, (uint64_t)32, (uint64_t)32, 0);
        MS(void, td, "setUsage:", (uint64_t)5);
        tex = MS(id, dev, "newTextureWithDescriptor:", td);
        float verts[] = {-1, -1, 1, 0, 0, 1, 1, -1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1};
        id vb = MS(id, dev, "newBufferWithBytes:length:options:", verts, (uint64_t)sizeof verts, (uint64_t)0);
        id vd = MS(id, objc_getClass("MTLVertexDescriptor"), "vertexDescriptor");
        id a0 = OU(O(vd, "attributes"), "objectAtIndexedSubscript:", (uint64_t)0);
        MS(void, a0, "setFormat:", (uint64_t)29); /* float2 */
        MS(void, a0, "setOffset:", (uint64_t)0);
        MS(void, a0, "setBufferIndex:", (uint64_t)0);
        id a1 = OU(O(vd, "attributes"), "objectAtIndexedSubscript:", (uint64_t)1);
        MS(void, a1, "setFormat:", (uint64_t)31); /* float4 */
        MS(void, a1, "setOffset:", (uint64_t)8);
        MS(void, a1, "setBufferIndex:", (uint64_t)0);
        MS(void, OU(O(vd, "layouts"), "objectAtIndexedSubscript:", (uint64_t)0), "setStride:", (uint64_t)24);
        id pd = O(O(objc_getClass("MTLRenderPipelineDescriptor"), "alloc"), "init");
        MS(void, pd, "setVertexFunction:", O1(lib, "newFunctionWithName:", str("vs")));
        MS(void, pd, "setFragmentFunction:", O1(lib, "newFunctionWithName:", str("fs")));
        MS(void, pd, "setVertexDescriptor:", vd);
        MS(void, OU(O(pd, "colorAttachments"), "objectAtIndexedSubscript:", (uint64_t)0), "setPixelFormat:", (uint64_t)70);
        id rps = MS(id, dev, "newRenderPipelineStateWithDescriptor:error:", pd, &err);
        if (!rps && err) printf("erro: %s\n", MS(const char *, O(err, "localizedDescription"), "UTF8String"));
        check("render pipeline com vertexDescriptor", rps != 0);
        id rp = MS(id, objc_getClass("MTLRenderPassDescriptor"), "renderPassDescriptor");
        id ca = OU(O(rp, "colorAttachments"), "objectAtIndexedSubscript:", (uint64_t)0);
        MS(void, ca, "setTexture:", tex);
        MS(void, ca, "setLoadAction:", (uint64_t)2);
        MS(void, ca, "setStoreAction:", (uint64_t)1);
        MClear cc = {0, 0, 0.25, 1};
        ((void (*)(id, SEL, MClear))objc_msgSend)(ca, S("setClearColor:"), cc);
        id cb = O(q, "commandBuffer");
        id e = MS(id, cb, "renderCommandEncoderWithDescriptor:", rp);
        MS(void, e, "setRenderPipelineState:", rps);
        MS(void, e, "setVertexBuffer:offset:atIndex:", vb, (uint64_t)0, (uint64_t)0);
        MS(void, e, "drawPrimitives:vertexStart:vertexCount:", (uint64_t)3, (uint64_t)0, (uint64_t)3);
        MS(void, e, "endEncoding");
        __block int done = 0;
        MS(void, cb, "addCompletedHandler:", ^(id b) { (void)b; done = 1; });
        MS(void, cb, "commit");
        uint8_t px[32 * 32 * 4];
        MRegion r = {{0, 0, 0}, {32, 32, 1}};
        ((void (*)(id, SEL, void *, uint64_t, MRegion, uint64_t))objc_msgSend)(tex, S("getBytes:bytesPerRow:fromRegion:mipmapLevel:"), px, (uint64_t)128, r, (uint64_t)0);
        uint8_t *c0 = px, *cc2 = px + (16 * 32 + 16) * 4, *bl = px + (31 * 32 + 1) * 4, *br = px + (31 * 32 + 30) * 4;
        printf("canto (0,0)=%d,%d,%d,%d  centro=%d,%d,%d,%d  baixo-esq=%d,%d,%d  baixo-dir=%d,%d,%d\n", c0[0], c0[1], c0[2], c0[3],
               cc2[0], cc2[1], cc2[2], cc2[3], bl[0], bl[1], bl[2], br[0], br[1], br[2]);
        check("clear (loadAction Clear)", c0[0] == 0 && c0[1] == 0 && c0[2] == 64 && c0[3] == 255);
        check("interpolação: baixo-esquerda vermelho, baixo-direita verde", bl[0] > 200 && bl[1] < 40 && br[1] > 200 && br[0] < 40);
        check("interpolação: centro mistura com azul", cc2[2] > 80 && cc2[0] > 30 && cc2[1] > 30);
        check("addCompletedHandler chamado", done == 1);
        int cnt = 0;
        for (int i = 0; i < 32 * 32; i++) if (px[i * 4 + 2] != 64 || px[i * 4]) cnt++;
        printf("pixels cobertos: %d (área ~512)\n", cnt);
        check("cobertura do triângulo ~ metade do alvo", cnt > 480 && cnt < 560);
    }
    /* compute escrevendo textura + render de um quad texturizado (sampler constexpr) + blit */
    {
        id td = MS(id, objc_getClass("MTLTextureDescriptor"), "texture2DDescriptorWithPixelFormat:width:height:mipmapped:", (uint64_t)70, (uint64_t)8, (uint64_t)8, 0);
        MS(void, td, "setUsage:", (uint64_t)3);
        id src = MS(id, dev, "newTextureWithDescriptor:", td);
        id cb = O(q, "commandBuffer");
        id e = O(cb, "computeCommandEncoder");
        MS(void, e, "setComputePipelineState:", OE(dev, "newComputePipelineStateWithFunction:error:", O1(lib, "newFunctionWithName:", str("img")), &err));
        MS(void, e, "setTexture:atIndex:", src, (uint64_t)0);
        MSize n = {8, 8, 1}, t = {4, 4, 1};
        ((void (*)(id, SEL, MSize, MSize))objc_msgSend)(e, S("dispatchThreads:threadsPerThreadgroup:"), n, t);
        MS(void, e, "endEncoding");
        id pd = O(O(objc_getClass("MTLRenderPipelineDescriptor"), "alloc"), "init");
        MS(void, pd, "setVertexFunction:", O1(lib, "newFunctionWithName:", str("vs_quad")));
        MS(void, pd, "setFragmentFunction:", O1(lib, "newFunctionWithName:", str("fs_tex")));
        MS(void, OU(O(pd, "colorAttachments"), "objectAtIndexedSubscript:", (uint64_t)0), "setPixelFormat:", (uint64_t)80);
        MS(void, pd, "setDepthAttachmentPixelFormat:", (uint64_t)252);
        id rps = MS(id, dev, "newRenderPipelineStateWithDescriptor:error:", pd, &err);
        id td2 = MS(id, objc_getClass("MTLTextureDescriptor"), "texture2DDescriptorWithPixelFormat:width:height:mipmapped:", (uint64_t)80, (uint64_t)16, (uint64_t)16, 0);
        id dst = MS(id, dev, "newTextureWithDescriptor:", td2);
        id tdd = MS(id, objc_getClass("MTLTextureDescriptor"), "texture2DDescriptorWithPixelFormat:width:height:mipmapped:", (uint64_t)252, (uint64_t)16, (uint64_t)16, 0);
        id dep = MS(id, dev, "newTextureWithDescriptor:", tdd);
        id dsd = O(O(objc_getClass("MTLDepthStencilDescriptor"), "alloc"), "init");
        MS(void, dsd, "setDepthCompareFunction:", (uint64_t)1);
        MS(void, dsd, "setDepthWriteEnabled:", 1);
        id dss = MS(id, dev, "newDepthStencilStateWithDescriptor:", dsd);
        id rp = MS(id, objc_getClass("MTLRenderPassDescriptor"), "renderPassDescriptor");
        id ca = OU(O(rp, "colorAttachments"), "objectAtIndexedSubscript:", (uint64_t)0);
        MS(void, ca, "setTexture:", dst);
        MS(void, ca, "setLoadAction:", (uint64_t)2);
        id da = O(rp, "depthAttachment");
        MS(void, da, "setTexture:", dep);
        MS(void, da, "setLoadAction:", (uint64_t)2);
        ((void (*)(id, SEL, double))objc_msgSend)(da, S("setClearDepth:"), 1.0);
        id re = MS(id, cb, "renderCommandEncoderWithDescriptor:", rp);
        MS(void, re, "setRenderPipelineState:", rps);
        MS(void, re, "setDepthStencilState:", dss);
        MS(void, re, "setFragmentTexture:atIndex:", src, (uint64_t)0);
        float z = 0.3f;
        MS(void, re, "setVertexBytes:length:atIndex:", &z, (uint64_t)4, (uint64_t)0);
        MS(void, re, "drawPrimitives:vertexStart:vertexCount:", (uint64_t)4, (uint64_t)0, (uint64_t)4);
        /* segundo quad mais longe: precisa perder no teste de profundidade */
        id pd2 = O(pd, "copy");
        MS(void, pd2, "setFragmentFunction:", O1(lib, "newFunctionWithName:", str("fs_col")));
        id rps2 = MS(id, dev, "newRenderPipelineStateWithDescriptor:error:", pd2, &err);
        MS(void, re, "setRenderPipelineState:", rps2);
        z = 0.6f;
        MS(void, re, "setVertexBytes:length:atIndex:", &z, (uint64_t)4, (uint64_t)0);
        MS(void, re, "drawPrimitives:vertexStart:vertexCount:", (uint64_t)4, (uint64_t)0, (uint64_t)4);
        MS(void, re, "endEncoding");
        id out = MS(id, dev, "newBufferWithLength:options:", (uint64_t)(16 * 16 * 4), (uint64_t)0);
        id be = O(cb, "blitCommandEncoder");
        MOrigin o = {0, 0, 0};
        MSize sz = {16, 16, 1};
        ((void (*)(id, SEL, id, uint64_t, uint64_t, MOrigin, MSize, id, uint64_t, uint64_t, uint64_t))objc_msgSend)(
            be, S("copyFromTexture:sourceSlice:sourceLevel:sourceOrigin:sourceSize:toBuffer:destinationOffset:destinationBytesPerRow:destinationBytesPerImage:"),
            dst, (uint64_t)0, (uint64_t)0, o, sz, out, (uint64_t)0, (uint64_t)64, (uint64_t)1024);
        MS(void, be, "endEncoding");
        MS(void, cb, "commit");
        uint8_t *p = MS(uint8_t *, out, "contents");
        /* BGRA: textura do kernel tem r = x/8, g = y/8; quad 16x16 amostra 8x8 nearest; v para baixo */
        uint8_t *tl = p, *brp = p + (15 * 16 + 15) * 4;
        printf("quad: tl=B%d G%d R%d  br=B%d G%d R%d\n", tl[0], tl[1], tl[2], brp[0], brp[1], brp[2]);
        check("textura escrita no kernel e amostrada (nearest) no fragmento", tl[2] == 0 && tl[1] == 0 && brp[2] == 223 && brp[1] == 223);
        check("teste de profundidade (Less) descartou o quad mais longe (que seria R153 G102)", brp[2] == 223 && brp[1] == 223 && tl[2] == 0);
    }
    /* erro de compilação devolve NSError */
    {
        id e2 = 0;
        id bad = MS(id, dev, "newLibraryWithSource:options:error:", str("kernel void k(device float *a [[buffer(0)]]) { a[0] = nao_existe; }"), (id)0, &e2);
        if (e2) printf("erro esperado: %s\n", MS(const char *, O(e2, "localizedDescription"), "UTF8String"));
        check("erro de compilação vira NSError", !bad && e2);
    }
    printf(fails ? "METAL: %d FALHA(S)\n" : "METAL: TUDO OK\n", fails);
    return fails ? 1 : 0;
}
