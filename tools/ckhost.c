/* Vetor SHA-256 de "abc" pelo despacho genérico de HashFunction. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void ck_sha256(const void *p, int64_t n, void *out);
void ck_copy32(void *d, const void *s) { memcpy(d, s, 32); }

/* O descritor de conformidade guarda em +8 um ponteiro relativo à witness table. */
void *swift_getWitnessTable(const void *conf, const void *type, const void *arg) {
    const int32_t *d = conf;
    (void)type; (void)arg;
    return (void *)((const char *)(d + 2) + d[2]);
}

void *swift_allocObject(const void *md, size_t size, size_t align) {
    (void)md; (void)align;
    void *p = calloc(1, size ? size : 1);
    return p;
}
void *swift_retain(void *o) { return o; }
void swift_release(void *o) { (void)o; }
void swift_bridgeObjectRelease(void *o) { (void)o; }
void swift_bridgeObjectRetain(void *o) { (void)o; }
void *swift_allocBox(const void *t) { (void)t; return calloc(1, 64); }
void swift_willThrow(void) {}
void arc4random_buf(void *b, size_t n) { memset(b, 0, n); }

int main(void) {
    static const uint8_t abc[3] = {'a', 'b', 'c'};
    static const uint8_t want[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    uint8_t got[32];
    ck_sha256(abc, 3, got);
    if (memcmp(got, want, 32) != 0) {
        fprintf(stderr, "digest ");
        for (int i = 0; i < 32; i++) fprintf(stderr, "%02x", got[i]);
        fprintf(stderr, "\n");
        return 1;
    }
    printf("ok sha256 abc\n");
    return 0;
}
