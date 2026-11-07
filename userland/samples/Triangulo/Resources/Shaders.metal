// Shaders do Triangulo (compilados do fonte pelo Metal por software do mOS)
#include <metal_stdlib>
using namespace metal;

struct Uniforms { float angle; float aspect; };
struct VOut { float4 pos [[position]]; float3 color; };

constant float2 P[3] = { float2(0.0, 0.8), float2(-0.7, -0.5), float2(0.7, -0.5) };
constant float3 C[3] = { float3(1.0, 0.25, 0.3), float3(0.2, 0.9, 0.35), float3(0.25, 0.45, 1.0) };

vertex VOut vs(uint vid [[vertex_id]], constant Uniforms &u [[buffer(0)]]) {
    float s = sin(u.angle), c = cos(u.angle);
    float2 p = P[vid];
    p = float2(p.x * c - p.y * s, p.x * s + p.y * c);
    VOut o;
    o.pos = float4(p.x / u.aspect, p.y, 0.0, 1.0);
    o.color = C[vid];
    return o;
}

fragment float4 fs(VOut in [[stage_in]]) {
    return float4(in.color, 1.0);
}
