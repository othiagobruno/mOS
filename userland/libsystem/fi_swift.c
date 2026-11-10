/*
 * Símbolos Swift de Foundation que o WhatsApp importa e que o overlay do mOS não traz
 * (FormatStyle, ISO8601, NSDecimal, Measurement). O miolo é fmt_core.inc.
 * Os outros 187 nomes estão em fi_thunks.c. No arm64, fi_exports.c continua com ret.
 *
 * swiftcc x86_64: self em r13, sret em rax, inteiros em rdi/rsi/rdx, Double em xmm0,
 * String de volta em rax:rdx.
 */
#ifdef FI_HOST
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>
#else
#include "foundation.h"
#include "libc.h"
void *dlsym(void *h, const char *name);
char *strcat(char *d, const char *s);
int sscanf(const char *s, const char *fmt, ...);
#endif
#include "fi_abi.inc"
#include "fi_behavior.inc"

typedef struct { double value; void *unit; } fi_meas;

fi_meas fi_meas_converted(const void *self, void *to_unit) {
    fi_meas r;
    double v = 0, outv = 0;
    const char *from = 0, *to = 0;
    if (self) memcpy(&v, self, sizeof v);
#ifndef FI_HOST
    if (self) {
        void *u = *(void **)((const char *)self + 8);
        if (u) from = cx_cstr(objc_msgSend(u, sel_registerName("symbol")));
    }
    if (to_unit) to = cx_cstr(objc_msgSend(to_unit, sel_registerName("symbol")));
#endif
    if (from && to && from[0] && to[0] && !fi_convert_units(from, to, v, &outv)) v = outv;
    r.value = v;
    r.unit = to_unit;
    return r;
}

#if defined(__x86_64__)
__asm__(
".text\n"
".globl \"$s10Foundation4DateV9formattedSSyF\"\n"
"\"$s10Foundation4DateV9formattedSSyF\":\n"
"  movq %r13, %rdi\n"
"  jmp fi_date_default\n"
".globl \"$s10Foundation4DateV9formatted4date4timeSSAC11FormatStyleV0bG0V_AH04TimeG0VtF\"\n"
"\"$s10Foundation4DateV9formatted4date4timeSSAC11FormatStyleV0bG0V_AH04TimeG0VtF\":\n"
"  movq %rdi, %rdx\n"
"  movq %r13, %rdi\n"
"  jmp fi_date_styles\n"
".globl \"$s10Foundation4DateV18ISO8601FormatStyleV6formatySSACF\"\n"
"\"$s10Foundation4DateV18ISO8601FormatStyleV6formatySSACF\":\n"
"  movq %xmm0, %rdi\n"
"  movq %r13, %rsi\n"
"  jmp fi_iso_format_styled\n"
".globl \"$s10Foundation4DateV18ISO8601FormatStyleV5parseyACSSKF\"\n"
"\"$s10Foundation4DateV18ISO8601FormatStyleV5parseyACSSKF\":\n"
"  subq $8, %rsp\n"
"  call fi_iso_parse\n"
"  addq $8, %rsp\n"
"  movq %rax, %xmm0\n"
"  ret\n"
".globl \"$sSo9NSDecimala10FoundationEyABSicfC\"\n"
"\"$sSo9NSDecimala10FoundationEyABSicfC\":\n"
"  pushq %rax\n"
"  movq %rax, %rsi\n"
"  call fi_dec_from_int\n"
"  popq %rax\n"
"  ret\n"
".globl \"$sSo9NSDecimala10FoundationE14integerLiteralABSi_tcfC\"\n"
"\"$sSo9NSDecimala10FoundationE14integerLiteralABSi_tcfC\":\n"
"  pushq %rax\n"
"  movq %rax, %rsi\n"
"  call fi_dec_from_int\n"
"  popq %rax\n"
"  ret\n"
".globl \"$sSo9NSDecimala10FoundationEyABSdcfC\"\n"
"\"$sSo9NSDecimala10FoundationEyABSdcfC\":\n"
"  pushq %rax\n"
"  movq %xmm0, %rdi\n"
"  movq %rax, %rsi\n"
"  call fi_dec_from_double\n"
"  popq %rax\n"
"  ret\n"
".globl \"$sSo9NSDecimala10FoundationE12floatLiteralABSd_tcfC\"\n"
"\"$sSo9NSDecimala10FoundationE12floatLiteralABSd_tcfC\":\n"
"  pushq %rax\n"
"  movq %xmm0, %rdi\n"
"  movq %rax, %rsi\n"
"  call fi_dec_from_double\n"
"  popq %rax\n"
"  ret\n"
".globl \"$sSo9NSDecimala10FoundationE1poiyA2B_ABtFZ\"\n"
"\"$sSo9NSDecimala10FoundationE1poiyA2B_ABtFZ\":\n"
"  pushq %rax\n"
"  movq %rax, %rcx\n"
"  movl $0, %edx\n"
"  call fi_dec_op_sret\n"
"  popq %rax\n"
"  ret\n"
".globl \"$sSo9NSDecimala10FoundationE1soiyA2B_ABtFZ\"\n"
"\"$sSo9NSDecimala10FoundationE1soiyA2B_ABtFZ\":\n"
"  pushq %rax\n"
"  movq %rax, %rcx\n"
"  movl $1, %edx\n"
"  call fi_dec_op_sret\n"
"  popq %rax\n"
"  ret\n"
".globl \"$sSo9NSDecimala10FoundationE1moiyA2B_ABtFZ\"\n"
"\"$sSo9NSDecimala10FoundationE1moiyA2B_ABtFZ\":\n"
"  pushq %rax\n"
"  movq %rax, %rcx\n"
"  movl $2, %edx\n"
"  call fi_dec_op_sret\n"
"  popq %rax\n"
"  ret\n"
".globl \"$sSo9NSDecimala10FoundationE1doiyA2B_ABtFZ\"\n"
"\"$sSo9NSDecimala10FoundationE1doiyA2B_ABtFZ\":\n"
"  pushq %rax\n"
"  movq %rax, %rcx\n"
"  movl $3, %edx\n"
"  call fi_dec_op_sret\n"
"  popq %rax\n"
"  ret\n"
".globl \"$sSo9NSDecimala10FoundationE2eeoiySbAB_ABtFZ\"\n"
"\"$sSo9NSDecimala10FoundationE2eeoiySbAB_ABtFZ\":\n"
"  subq $8, %rsp\n"
"  call fi_dec_cmp\n"
"  addq $8, %rsp\n"
"  testl %eax, %eax\n"
"  setz %al\n"
"  movzbl %al, %eax\n"
"  ret\n"
".globl \"$sSo9NSDecimala10FoundationE1loiySbAB_ABtFZ\"\n"
"\"$sSo9NSDecimala10FoundationE1loiySbAB_ABtFZ\":\n"
"  subq $8, %rsp\n"
"  call fi_dec_cmp\n"
"  addq $8, %rsp\n"
"  shrl $31, %eax\n"
"  ret\n"
".globl \"$s10Foundation11MeasurementV5valueSdvg\"\n"
"\"$s10Foundation11MeasurementV5valueSdvg\":\n"
"  movsd (%r13), %xmm0\n"
"  ret\n"
".globl \"$s10Foundation11MeasurementV5value4unitACyxGSd_xtcfC\"\n"
"\"$s10Foundation11MeasurementV5value4unitACyxGSd_xtcfC\":\n"
"  movq %rdi, %rax\n"
"  ret\n"
".globl \"$s10Foundation11MeasurementVAASo11NSDimensionCRbzrlE9converted2toACyxGx_tF\"\n"
"\"$s10Foundation11MeasurementVAASo11NSDimensionCRbzrlE9converted2toACyxGx_tF\":\n"
"  movq %rdi, %rsi\n"
"  movq %r13, %rdi\n"
"  jmp fi_meas_converted\n"
);
#endif

/* rdi = a, rsi = b, edx = op, rcx = destino. O sret original volta em rax pelo thunk. */
void fi_dec_op_sret(const void *a, const void *b, int op, void *out) {
    fi_dec_binop(a, b, out, op);
}
