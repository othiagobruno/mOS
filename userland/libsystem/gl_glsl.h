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
} gl_decl_t;
typedef struct {
    int stage;                /* 0 vértice, 1 fragmento */
    int version, compat;
    gl_decl_t *decl;
    int ndecl, capdecl;
    int prelude_lines;
    int origin_ul, pixel_int;
} gl_shinfo_t;
char *gl_glsl_translate(const char *pp_text, gl_shinfo_t *I);
const char *gl_glsl_prelude(int stage, int compat, u64 *len);
const char *gl_glsl_sampler_name(int i);
