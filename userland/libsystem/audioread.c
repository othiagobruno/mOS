#include "audioread.h"
/* Leitor de WAV/AIFF/CAF/LPCM. O mesmo cx_audio_open_path que AVURLAsset e AVAudioPlayer chamam.
 * Compila no libSystem e no teste de host. AAC e Opus continuam em cx_audiodec (não mexer). */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char i8;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef int OSStatus;
typedef ar_asbd_t asbd_t;
typedef struct { u32 mNumberChannels, mDataByteSize; void *mData; } abuf_t;
typedef struct { u32 mNumberBuffers; u32 pad; abuf_t mBuffers[1]; } ablist_t;
#define FCC(a, b, c, d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))
#define kAudioFormatLinearPCM FCC('l', 'p', 'c', 'm')
#define kFmtFloat 1u
#define kFmtBigEndian 2u
#define kFmtSignedInt 4u
#define kFmtPacked 8u
#define kFmtNonInterleaved 32u
#define kAudioFileUnsupportedFileTypeError FCC('t', 'y', 'p', '?')
#define kAudioFileUnsupportedDataFormatError FCC('f', 'm', 't', '?')
#define kAudioFileInvalidFileError FCC('d', 't', 'a', '?')
#define kAudioFileNotFound (-43)
extern u8 *cx_read_file(const char *path, u64 *len) __attribute__((weak));
typedef struct {
    int kind, status;
    const char *limit;
    int rate, channels, frames;
    short *pcm;
} cx_audiodec_out;
int cx_audiodec(const void *bytes, unsigned long long n, cx_audiodec_out *o);
void cx_audiodec_free(cx_audiodec_out *o);

static bool lpcm_ok(const asbd_t *d) {
    if (!d || d->mFormatID != kAudioFormatLinearPCM) return false;
    if (d->mChannelsPerFrame < 1 || d->mChannelsPerFrame > 32 || d->mSampleRate < 1000 || d->mSampleRate > 768000) return false;
    u32 b = d->mBitsPerChannel;
    if (d->mFormatFlags & kFmtFloat) return b == 32 || b == 64;
    return b == 8 || b == 16 || b == 24 || b == 32;
}
static float rd_sample(const asbd_t *d, const ablist_t *l, u32 f, u32 c) {
    u32 bps = d->mBitsPerChannel / 8;
    if (!bps) bps = d->mChannelsPerFrame ? d->mBytesPerFrame / d->mChannelsPerFrame : 0;
    const u8 *p;
    if (d->mFormatFlags & kFmtNonInterleaved) {
        if (c >= l->mNumberBuffers || !l->mBuffers[c].mData) return 0;
        p = (const u8 *)l->mBuffers[c].mData + (u64)f * d->mBytesPerFrame;
        if ((u64)(f + 1) * d->mBytesPerFrame > l->mBuffers[c].mDataByteSize) return 0;
    } else {
        if (!l->mNumberBuffers || !l->mBuffers[0].mData) return 0;
        u64 off = (u64)f * d->mBytesPerFrame + (u64)c * bps;
        if (off + bps > l->mBuffers[0].mDataByteSize) return 0;
        p = (const u8 *)l->mBuffers[0].mData + off;
    }
    bool be = d->mFormatFlags & kFmtBigEndian;
    u8 b[8];
    for (u32 i = 0; i < bps && i < 8; i++) b[i] = be ? p[bps - 1 - i] : p[i];
    if (d->mFormatFlags & kFmtFloat) {
        if (bps == 4) { float v; memcpy(&v, b, 4); return v; }
        if (bps == 8) { double v; memcpy(&v, b, 8); return (float)v; }
        return 0;
    }
    bool sgn = (d->mFormatFlags & kFmtSignedInt) != 0;
    switch (bps) {
    case 1: return sgn ? (float)(i8)b[0] / 128.0f : ((float)b[0] - 128.0f) / 128.0f;
    case 2: { i16 v = (i16)(b[0] | b[1] << 8); return (float)v / 32768.0f; }
    case 3: { i32 v = (i32)((u32)b[0] << 8 | (u32)b[1] << 16 | (u32)b[2] << 24) >> 8; return (float)v / 8388608.0f; }
    case 4: { i32 v = (i32)((u32)b[0] | (u32)b[1] << 8 | (u32)b[2] << 16 | (u32)b[3] << 24);
              return (float)((double)v / 2147483648.0); }
    }
    return 0;
}
static void to_stereo(const asbd_t *d, const ablist_t *l, u32 n, float *out) {
    u32 ch = d->mChannelsPerFrame;
    if (!(d->mFormatFlags & kFmtNonInterleaved) && (d->mFormatFlags & kFmtFloat) && d->mBitsPerChannel == 32 && !(d->mFormatFlags & kFmtBigEndian) &&
        l->mNumberBuffers && l->mBuffers[0].mData) {
        const float *s = l->mBuffers[0].mData;
        u32 have = l->mBuffers[0].mDataByteSize / (4 * ch);
        for (u32 i = 0; i < n; i++) {
            if (i >= have) { out[i * 2] = out[i * 2 + 1] = 0; continue; }
            out[i * 2] = s[i * ch];
            out[i * 2 + 1] = ch > 1 ? s[i * ch + 1] : s[i * ch];
        }
        return;
    }
    for (u32 i = 0; i < n; i++) {
        float a = rd_sample(d, l, i, 0);
        out[i * 2] = a;
        out[i * 2 + 1] = ch > 1 ? rd_sample(d, l, i, 1) : a;
    }
}

/* ================================================================ arquivos de áudio (WAV, AIFF/AIFC, CAF) */
/* adecode_t está em audioread.h */

static u32 le32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u16 le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u16 be16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }
static u64 be64(const u8 *p) { return (u64)be32(p) << 32 | be32(p + 4); }
static double ext80(const u8 *p) {   /* o "extended" de 80 bits do AIFF (taxa de amostragem) */
    int e = ((p[0] & 0x7F) << 8 | p[1]) - 16383 - 63;
    u64 m = be64(p + 2);
    double v = (double)m;
    while (e > 0) { v *= 2; e--; }
    while (e < 0) { v /= 2; e++; }
    return (p[0] & 0x80) ? -v : v;
}

/* 0 = virou LPCM, 1 = não é formato desta frente (o parser antigo segue), outro = erro da Apple. */
static OSStatus decode_from_compressed(adecode_t *d) {
    cx_audiodec_out ao;
    if (!cx_audiodec(d->file, d->file_len, &ao)) return 1;
    if (ao.status != 0 || !ao.pcm || ao.channels < 1 || ao.rate < 1000) {
        cx_audiodec_free(&ao);
        return kAudioFileUnsupportedDataFormatError;
    }
    d->type = FCC('L', 'P', 'C', 'M');
    d->fmt.mFormatID = kAudioFormatLinearPCM;
    d->fmt.mSampleRate = ao.rate;
    d->fmt.mChannelsPerFrame = (u32)ao.channels;
    d->fmt.mBitsPerChannel = 16;
    d->fmt.mFormatFlags = kFmtSignedInt | kFmtPacked;
    d->fmt.mFramesPerPacket = 1;
    d->fmt.mBytesPerFrame = d->fmt.mBytesPerPacket = 2u * (u32)ao.channels;
    d->pcm = (u8 *)ao.pcm;
    ao.pcm = 0;
    d->data = d->pcm;
    d->data_len = (u64)ao.frames * d->fmt.mBytesPerFrame;
    cx_audiodec_free(&ao);
    return 0;
}

/* 0 ou o OSStatus de erro da Apple */
static OSStatus decode_parse(adecode_t *d) {
    const u8 *b = d->file;
    u64 n = d->file_len;
    memset(&d->fmt, 0, sizeof d->fmt);
    d->fmt.mFormatID = kAudioFormatLinearPCM;
    d->fmt.mFramesPerPacket = 1;
    OSStatus compressed = decode_from_compressed(d);
    if (compressed != 1) {
        if (compressed) return compressed;
        goto lpcm_tail;
    }
    if (n >= 12 && !memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WAVE", 4)) {
        d->type = FCC('W', 'A', 'V', 'E');
        bool have_fmt = false;
        for (u64 o = 12; o + 8 <= n;) {
            u32 cl = le32(b + o + 4);
            const u8 *c = b + o + 8;
            if (!memcmp(b + o, "fmt ", 4) && cl >= 16 && o + 8 + 16 <= n) {
                u16 tag = le16(c);
                u16 ch = le16(c + 2);
                u32 rate = le32(c + 4);
                u16 bits = le16(c + 14);
                if (tag == 0xFFFE && cl >= 40) tag = le16(c + 24);   /* WAVE_FORMAT_EXTENSIBLE: o subformato */
                if (tag != 1 && tag != 3) return kAudioFileUnsupportedDataFormatError;   /* PCM ou IEEE float */
                d->fmt.mSampleRate = rate;
                d->fmt.mChannelsPerFrame = ch;
                d->fmt.mBitsPerChannel = bits;
                d->fmt.mFormatFlags = tag == 3 ? kFmtFloat | kFmtPacked : (bits > 8 ? kFmtSignedInt : 0) | kFmtPacked;
                d->fmt.mBytesPerFrame = d->fmt.mBytesPerPacket = (u32)((bits + 7) / 8) * ch;
                have_fmt = true;
            } else if (!memcmp(b + o, "data", 4)) {
                d->data = c;
                d->data_len = cl <= n - (o + 8) ? cl : n - (o + 8);
            }
            o += 8 + cl + (cl & 1);
        }
        if (!have_fmt || !d->data) return kAudioFileInvalidFileError;
    } else if (n >= 12 && !memcmp(b, "FORM", 4) && (!memcmp(b + 8, "AIFF", 4) || !memcmp(b + 8, "AIFC", 4))) {
        bool aifc = !memcmp(b + 8, "AIFC", 4);
        d->type = aifc ? FCC('A', 'I', 'F', 'C') : FCC('A', 'I', 'F', 'F');
        bool have = false;
        for (u64 o = 12; o + 8 <= n;) {
            u32 cl = be32(b + o + 4);
            const u8 *c = b + o + 8;
            if (!memcmp(b + o, "COMM", 4) && cl >= 18) {
                u16 ch = be16(c);
                u16 bits = be16(c + 6);
                d->fmt.mSampleRate = ext80(c + 8);
                d->fmt.mChannelsPerFrame = ch;
                d->fmt.mBitsPerChannel = bits;
                d->fmt.mFormatFlags = kFmtBigEndian | kFmtSignedInt | kFmtPacked;
                if (aifc && cl >= 22) {
                    u32 comp = be32(c + 18);
                    if (comp == FCC('s', 'o', 'w', 't')) d->fmt.mFormatFlags = kFmtSignedInt | kFmtPacked;   /* little endian */
                    else if (comp == FCC('f', 'l', '3', '2') || comp == FCC('F', 'L', '3', '2')) { d->fmt.mFormatFlags = kFmtFloat | kFmtBigEndian | kFmtPacked; d->fmt.mBitsPerChannel = 32; }
                    else if (comp == FCC('f', 'l', '6', '4') || comp == FCC('F', 'L', '6', '4')) { d->fmt.mFormatFlags = kFmtFloat | kFmtBigEndian | kFmtPacked; d->fmt.mBitsPerChannel = 64; }
                    else if (comp != FCC('N', 'O', 'N', 'E') && comp != FCC('t', 'w', 'o', 's')) return kAudioFileUnsupportedDataFormatError;
                }
                d->fmt.mBytesPerFrame = d->fmt.mBytesPerPacket = (u32)((d->fmt.mBitsPerChannel + 7) / 8) * ch;
                have = true;
            } else if (!memcmp(b + o, "SSND", 4) && cl >= 8) {
                u32 off = be32(c);
                d->data = c + 8 + off;
                d->data_len = cl - 8 - off;
                if (d->data + d->data_len > b + n) d->data_len = (u64)(b + n - d->data);
            }
            o += 8 + cl + (cl & 1);
        }
        if (!have || !d->data) return kAudioFileInvalidFileError;
    } else if (n >= 8 && !memcmp(b, "caff", 4)) {
        d->type = FCC('c', 'a', 'f', 'f');
        bool have = false;
        for (u64 o = 8; o + 12 <= n;) {
            u64 cl = be64(b + o + 4);
            const u8 *c = b + o + 12;
            if (!memcmp(b + o, "desc", 4) && cl >= 32) {
                u64 rb = be64(c);
                double rate;
                memcpy(&rate, &rb, 8);
                u32 fid = be32(c + 8), fl = be32(c + 12);
                if (fid != kAudioFormatLinearPCM) return kAudioFileUnsupportedDataFormatError;
                d->fmt.mSampleRate = rate;
                /* no CAF o bit 1 é kCAFLinearPCMFormatFlagIsLittleEndian (o inverso do kAudioFormatFlagIsBigEndian) */
                d->fmt.mFormatFlags = ((fl & 1) ? kFmtFloat : kFmtSignedInt) | ((fl & 2) ? 0 : kFmtBigEndian) | kFmtPacked;
                d->fmt.mBytesPerPacket = be32(c + 16);
                d->fmt.mFramesPerPacket = be32(c + 20);
                d->fmt.mChannelsPerFrame = be32(c + 24);
                d->fmt.mBitsPerChannel = be32(c + 28);
                d->fmt.mBytesPerFrame = d->fmt.mBytesPerPacket;
                have = true;
            } else if (!memcmp(b + o, "data", 4)) {
                d->data = c + 4;   /* mEditCount */
                u64 len = cl == (u64)-1 ? n - (o + 16) : cl - 4;
                d->data_len = len <= n - (o + 16) ? len : n - (o + 16);
            }
            if (cl == (u64)-1) break;
            o += 12 + cl;
        }
        if (!have || !d->data) return kAudioFileInvalidFileError;
    } else if (n >= 3 && (!memcmp(b, "ID3", 3) || (b[0] == 0xFF && (b[1] & 0xE0) == 0xE0))) {
        return kAudioFileUnsupportedDataFormatError;   /* MP3 que o decodificador não aceitou */
    } else if (n >= 12 && !memcmp(b + 4, "ftyp", 4)) {
        return kAudioFileUnsupportedDataFormatError;   /* M4A que não é ALAC de 16 bits (AAC fica aqui) */
    } else return kAudioFileUnsupportedFileTypeError;
lpcm_tail:
    if (!lpcm_ok(&d->fmt)) return kAudioFileUnsupportedDataFormatError;
    if (d->fmt.mBytesPerFrame) d->data_len -= d->data_len % d->fmt.mBytesPerFrame;
    d->duration = d->fmt.mBytesPerFrame && d->fmt.mSampleRate ? (double)(d->data_len / d->fmt.mBytesPerFrame) / d->fmt.mSampleRate : 0;
    return 0;
}

/* abre um arquivo pelo caminho; devolve um adecode_t (free com cx_audio_close) ou 0 com *err */
adecode_t *decode_open_path(const char *path, OSStatus *err) {
    u64 len = 0;
    u8 *f = path ? cx_read_file(path, &len) : 0;
    if (!f) { if (err) *err = kAudioFileNotFound; return 0; }
    adecode_t *d = calloc(1, sizeof *d);
    if (!d) { free(f); if (err) *err = -108; return 0; }
    d->file = f;
    d->file_len = len;
    OSStatus r = decode_parse(d);
    if (r) { decode_close(d); if (err) *err = r; return 0; }
    if (err) *err = 0;
    return d;
}
adecode_t *decode_open_data(const void *bytes, u64 len, OSStatus *err) {
    u8 *f = malloc(len ? len : 1);
    if (!f) { if (err) *err = -108; return 0; }
    memcpy(f, bytes, len);
    adecode_t *d = calloc(1, sizeof *d);
    if (!d) { free(f); if (err) *err = -108; return 0; }
    d->file = f;
    d->file_len = len;
    OSStatus r = decode_parse(d);
    if (r) { decode_close(d); if (err) *err = r; return 0; }
    if (err) *err = 0;
    return d;
}
void decode_close(adecode_t *d) { if (d) { free(d->pcm); free(d->file); free(d); } }

/* para o avfaudio.c: abrir, formato, quadros e leitura em estéreo float na taxa do arquivo */
void *cx_audio_open_path(const char *path, int *err) { OSStatus e = 0; void *d = decode_open_path(path, &e); if (err) *err = e; return d; }
void *cx_audio_open_data(const void *b, u64 n, int *err) { OSStatus e = 0; void *d = decode_open_data(b, n, &e); if (err) *err = e; return d; }
void cx_audio_close(void *d) { decode_close(d); }
double cx_audio_rate(void *d) { return ((adecode_t *)d)->fmt.mSampleRate; }
u32 cx_audio_channels(void *d) { return ((adecode_t *)d)->fmt.mChannelsPerFrame; }
u64 cx_audio_frames(void *d) { adecode_t *a = d; return a->fmt.mBytesPerFrame ? a->data_len / a->fmt.mBytesPerFrame : 0; }
const void *cx_audio_asbd(void *d) { return &((adecode_t *)d)->fmt; }
u32 cx_audio_type(void *d) { return ((adecode_t *)d)->type; }
/* lê n quadros a partir de 'from' em estéreo float; devolve os lidos */
u32 cx_audio_read_stereo(void *d_, u64 from, u32 n, float *out) {
    adecode_t *d = d_;
    u64 total = cx_audio_frames(d);
    if (from >= total) return 0;
    if (from + n > total) n = (u32)(total - from);
    ablist_t l = {1, 0, {{d->fmt.mChannelsPerFrame, (u32)(n * d->fmt.mBytesPerFrame), (void *)(d->data + from * d->fmt.mBytesPerFrame)}}};
    to_stereo(&d->fmt, &l, n, out);
    return n;
}
/* lê n quadros num canal (0..) em float: para o AVAudioPCMBuffer com mais de dois canais */
const void *ar_asbd(adecode_t *d) { return d ? &d->fmt : 0; }
u64 ar_bytes(adecode_t *d) { return d ? d->data_len : 0; }
u32 ar_type(adecode_t *d) { return d ? d->type : 0; }
double ar_duration(adecode_t *d) { return d ? d->duration : 0; }
float cx_audio_sample(void *d_, u64 frame, u32 ch) {
    adecode_t *d = d_;
    if (frame >= cx_audio_frames(d) || ch >= d->fmt.mChannelsPerFrame) return 0;
    ablist_t l = {1, 0, {{d->fmt.mChannelsPerFrame, (u32)d->fmt.mBytesPerFrame, (void *)(d->data + frame * d->fmt.mBytesPerFrame)}}};
    return rd_sample(&d->fmt, &l, 0, ch);
}
