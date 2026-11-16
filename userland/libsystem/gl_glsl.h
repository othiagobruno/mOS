/* mOS libSystem — OpenGL por software: tipos internos do GLSL (gl_glsl*.c) */
#pragma once
#include "gl_int.h"
#include "mtlint.h"

enum { GS_PLAIN = 0, GS_UNIFORM = 1, GS_IN = 2, GS_OUT = 3 };
/* uma global do shader com qualificador (ou comum, para voltar ao valor inicial) */
typedef struct {
    char *name;               /* nome da global no MSL (a instância, nos blocos) */
    char *block;              /* bloco de interface: nome do bloco */
    int storage, loc, index, interp, rowmajor, arr, has_inst;
    int aux;                  /* 1 centroid, 2 sample (interpolação em multiamostra) */
    int patch;                /* tesselação: 'patch in/out' (um valor por patch, não por vértice) */
    char *type, *dims;        /* texto do tipo e das dimensões (o geometry shader gera as cópias de EmitVertex) */
} gl_decl_t;
typedef struct {
    int stage;                /* 0 vértice, 1 fragmento, 2 geometria, 3 controle de tesselação, 4 avaliação de tesselação */
    int version, compat;
    gl_decl_t *decl;
    int ndecl, capdecl;
    int prelude_lines;
    int origin_ul, pixel_int;
    GLenum gs_in, gs_out;     /* geometry shader: primitivos de entrada e de saída */
    int gs_max, gs_inv, gs_in_set, gs_out_set;
    int tcs_vertices;         /* layout(vertices = N) out */
    GLenum tes_mode, tes_spacing;   /* GL_TRIANGLES/GL_QUADS/GL_ISOLINES; GL_EQUAL/GL_FRACTIONAL_EVEN/GL_FRACTIONAL_ODD */
    int tes_cw, tes_point, tes_set;
} gl_shinfo_t;
char *gl_glsl_translate(const char *pp_text, gl_shinfo_t *I);
const char *gl_glsl_prelude(int stage, int compat, u64 *len);
const char *gl_glsl_sampler_name(int i);
