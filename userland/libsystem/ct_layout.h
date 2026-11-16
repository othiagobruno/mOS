/* mOS Core Text — quebra de linha, grafemas e bidi sobre a ICU (ct_layout.c). Só tipos básicos do C: o cabeçalho
 * entra tanto no libSystem (sys.h) quanto no host (stdint.h). */
#ifndef MOS_CT_LAYOUT_H
#define MOS_CT_LAYOUT_H
int ctl_has_icu(void);
void ctl_line_breaks(const unsigned short *s, long n, unsigned char *brk);       /* brk[i]: 0, 1 permitida, 2 obrigatória */
void ctl_clusters(const unsigned short *s, long n, unsigned char *cl);           /* cl[i] = 1: limite de grafema antes de i */
int ctl_bidi_levels(const unsigned short *s, long n, int para, unsigned char *levels);   /* devolve o nível do parágrafo */
void ctl_reorder_line(const unsigned short *s, const unsigned char *levels, long a, long b, int para_level, int *map);
#endif
