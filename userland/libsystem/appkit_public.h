/* Aritmética compartilhada pelo AppKit e pelo teste de host (CX_AK_HOST). */
#pragma once
#include <string.h>

#define CX_PATH_PART 96

double cx_step_apply(double v, double mn, double mx, double inc, int up);
int cx_path_split(const char *path, char parts[][CX_PATH_PART], int max);
const char *cx_ax_role(const char *cls);
int cx_chain_index(const int *next, int start, int count, int (*has)(int id, const char *sel, void *ctx), void *ctx, const char *sel);
