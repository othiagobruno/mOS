#pragma once
#include <stddef.h>
#ifndef MOS_OSSER_HOST
#include <stdint.h>
#endif
typedef struct {
    uint8_t *p;
    uint32_t n, cap;
    int end;
    const void *seen[128];
    int nseen;
} osser_t;
void osser_init(osser_t *s);
void osser_free(osser_t *s);
int osser_add(osser_t *s, const void *id, uint32_t key, const void *bits, uint32_t size);
void osser_set_end(osser_t *s, int end);
const uint8_t *osser_bytes(osser_t *s);
uint32_t osser_len(osser_t *s);
int mos_osser_selftest(void);
