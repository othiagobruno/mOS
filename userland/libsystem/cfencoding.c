/*
 * mOS libSystem — conversão entre CFStringEncoding e NSStringEncoding (CoreFoundation), como a da Apple:
 * as codificações com par no NSString têm número próprio (UTF-8 = 4, ASCII = 1...); as demais viram
 * 0x80000000 | CFStringEncoding. O WhatsApp chama CFStringConvertEncodingToNSStringEncoding na partida.
 */
typedef unsigned int u32;
typedef unsigned long u64;

static const u32 pairs[][2] = {               /* CF, NS */
    {0x0600, 1}, {0x0B01, 2}, {0x0920, 3}, {0x08000100, 4}, {0x0201, 5}, {0x0006, 6}, {0x0BFF, 7},
    {0x0421, 8}, {0x0202, 9}, {0x0100, 10}, {0x0502, 11}, {0x0500, 12}, {0x0503, 13}, {0x0504, 14},
    {0x0501, 15}, {0x0820, 21}, {0x0000, 30},
    {0x10000100, 0x90000100}, {0x14000100, 0x94000100}, {0x0c000100, 0x8c000100},
    {0x18000100, 0x98000100}, {0x1c000100, 0x9c000100},
};
#define NPAIRS (sizeof pairs / sizeof pairs[0])

u64 CFStringConvertEncodingToNSStringEncoding(u32 cf) {
    for (u32 i = 0; i < NPAIRS; i++) if (pairs[i][0] == cf) return pairs[i][1];
    if (cf == 0xffffffffu) return 0;                      /* kCFStringEncodingInvalidId */
    return 0x80000000ul | cf;
}
u32 CFStringConvertNSStringEncodingToEncoding(u64 ns) {
    for (u32 i = 0; i < NPAIRS; i++) if (pairs[i][1] == ns) return pairs[i][0];
    if (ns & 0x80000000ul) return (u32)(ns & 0x7fffffffu);
    return 0xffffffffu;
}
