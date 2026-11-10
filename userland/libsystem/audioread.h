#ifndef MOS_AUDIOREAD_H
#define MOS_AUDIOREAD_H
/* O mesmo leitor que AVURLAsset abre com cx_audio_open_path. */
typedef struct {
    double mSampleRate;
    unsigned mFormatID, mFormatFlags, mBytesPerPacket, mFramesPerPacket, mBytesPerFrame, mChannelsPerFrame, mBitsPerChannel, mReserved;
} ar_asbd_t;
typedef struct adecode {
    unsigned type;
    ar_asbd_t fmt;
    unsigned char *file;
    unsigned long long file_len;
    unsigned char *pcm;
    const unsigned char *data;
    unsigned long long data_len;
    double duration;
} adecode_t;
adecode_t *decode_open_path(const char *path, int *err);
adecode_t *decode_open_data(const void *bytes, unsigned long long len, int *err);
void decode_close(adecode_t *d);
const void *ar_asbd(adecode_t *d);
unsigned long long ar_bytes(adecode_t *d);
unsigned ar_type(adecode_t *d);
double ar_duration(adecode_t *d);
#endif
