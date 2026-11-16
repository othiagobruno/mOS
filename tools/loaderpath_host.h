/* shim de host para kernel/fs/loaderpath.c (tools/loaderpath_host.c) */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
typedef uint8_t u8; typedef uint16_t u16; typedef size_t usize;
static inline u16 le16(const void *p) { const u8 *b = p; return (u16)(b[0] | b[1] << 8); }
bool origin_parse_loader_path(const u8 *fp, usize len, char *out, usize cap);
