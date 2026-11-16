/*
 * mOS libSystem — aritmética do NSDecimal, sem ObjC (testada no host por tools/fdn_dec_test.c).
 * Layout igual ao da Foundation: expoente de 10 em 8 bits, até 8 dígitos base 65536 (38 dígitos decimais).
 */
#ifndef FDN_DECIMAL_CORE_H
#define FDN_DECIMAL_CORE_H

typedef struct {
    signed int exp : 8;
    unsigned int length : 4;
    unsigned int neg : 1;
    unsigned int compact : 1;
    unsigned int reserved : 18;
    unsigned short m[8];
} nsdec_t;

/* NSCalculationError e NSRoundingMode */
enum { DEC_OK, DEC_PREC, DEC_UNDER, DEC_OVER, DEC_DIV0 };
enum { DEC_PLAIN, DEC_DOWN, DEC_UP, DEC_BANKERS };
#define DEC_NO_SCALE 32767

int fdn_dec_isnan(const nsdec_t *d);
void fdn_dec_nan(nsdec_t *d);
void fdn_dec_from_u64(nsdec_t *d, unsigned long long mant, int exp, int neg);
void fdn_dec_from_i64(nsdec_t *d, long long v);
int fdn_dec_add(nsdec_t *r, const nsdec_t *a, const nsdec_t *b, int mode);
int fdn_dec_sub(nsdec_t *r, const nsdec_t *a, const nsdec_t *b, int mode);
int fdn_dec_mul(nsdec_t *r, const nsdec_t *a, const nsdec_t *b, int mode);
int fdn_dec_div(nsdec_t *r, const nsdec_t *a, const nsdec_t *b, int mode);
int fdn_dec_pow(nsdec_t *r, const nsdec_t *a, unsigned long power, int mode);
int fdn_dec_pow10(nsdec_t *r, const nsdec_t *a, int power, int mode);
int fdn_dec_round(nsdec_t *r, const nsdec_t *a, int scale, int mode);
int fdn_dec_normalize(nsdec_t *a, nsdec_t *b, int mode);
void fdn_dec_compact(nsdec_t *d);
int fdn_dec_cmp(const nsdec_t *a, const nsdec_t *b); /* -1, 0, 1 (NSComparisonResult) */
/* texto: sep é o separador decimal; devolve o tamanho escrito (sem o 0 final) */
int fdn_dec_to_str(const nsdec_t *d, char *out, int cap, const char *sep);
/* lê "[+-]dígitos[sep dígitos][e[+-]dígitos]"; devolve quantos bytes leu (0 = nada válido, d fica NaN) */
int fdn_dec_parse(nsdec_t *d, const char *s, const char *sep);
double fdn_dec_to_double(const nsdec_t *d);
void fdn_dec_from_double(nsdec_t *d, double v);
long long fdn_dec_to_i64(const nsdec_t *d);       /* trunca para zero, satura */
unsigned long long fdn_dec_to_u64(const nsdec_t *d);

#endif
