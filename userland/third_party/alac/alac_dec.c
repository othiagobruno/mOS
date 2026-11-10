/* Entrada em C do decodificador ALAC da Apple (Apache-2.0, ver APPLE_LICENSE.txt).
 * O miolo (Golomb, preditor, unmix) é o código da Apple. Esta casca cobre o quadro
 * de 16 bits; outras profundidades devolvem erro em vez de fingir o resultado. */
#include <stdlib.h>
#include <string.h>
#include "ALACBitUtilities.h"
#include "ALACAudioTypes.h"
#include "aglib.h"
#include "dplib.h"
#include "matrixlib.h"

#include "ag_dec.c"
#include "dp_dec.c"
#include "matrix_dec.c"
#include "ALACBitUtilities.c"
#include "EndianPortable.c"

typedef struct {
    ALACSpecificConfig cfg;
    int32_t *mixU, *mixV, *pred;
    uint16_t *shift;
} alac_st;

static void zero16(int16_t *buffer, uint32_t n, uint32_t stride) {
    if (stride == 1) { memset(buffer, 0, n * sizeof(int16_t)); return; }
    for (uint32_t i = 0; i < n * stride; i += stride) buffer[i] = 0;
}
static int32_t fill_element(BitBuffer *bits) {
    int16_t count = BitBufferReadSmall(bits, 4);
    if (count == 15) count = (int16_t)(count + (int16_t)BitBufferReadSmall(bits, 8) - 1);
    BitBufferAdvance(bits, (uint32_t)count * 8);
    if (bits->cur > bits->end) return kALAC_ParamError;
    return ALAC_noErr;
}
static int32_t data_element(BitBuffer *bits) {
    BitBufferReadSmall(bits, 4);
    int align = BitBufferReadOne(bits);
    uint16_t count = BitBufferReadSmall(bits, 8);
    if (count == 255) count = (uint16_t)(count + BitBufferReadSmall(bits, 8));
    if (align) BitBufferByteAlign(bits, 0);
    BitBufferAdvance(bits, count * 8);
    if (bits->cur > bits->end) return kALAC_ParamError;
    return ALAC_noErr;
}

int alac_open(alac_st *s, const uint8_t *cookie, int cookie_len) {
    memset(s, 0, sizeof *s);
    if (!cookie || cookie_len < 24) return -1;
    const uint8_t *p = cookie;
    int n = cookie_len;
    if (n >= 12 && p[4] == 'f' && p[5] == 'r' && p[6] == 'm' && p[7] == 'a') { p += 12; n -= 12; }
    if (n >= 12 && p[4] == 'a' && p[5] == 'l' && p[6] == 'a' && p[7] == 'c') { p += 12; n -= 12; }
    if (n < 24) return -1;
    s->cfg.frameLength = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
    s->cfg.compatibleVersion = p[4];
    s->cfg.bitDepth = p[5];
    s->cfg.pb = p[6];
    s->cfg.mb = p[7];
    s->cfg.kb = p[8];
    s->cfg.numChannels = p[9];
    s->cfg.maxRun = (uint16_t)((p[10] << 8) | p[11]);
    s->cfg.maxFrameBytes = ((uint32_t)p[12] << 24) | ((uint32_t)p[13] << 16) | ((uint32_t)p[14] << 8) | p[15];
    s->cfg.avgBitRate = ((uint32_t)p[16] << 24) | ((uint32_t)p[17] << 16) | ((uint32_t)p[18] << 8) | p[19];
    s->cfg.sampleRate = ((uint32_t)p[20] << 24) | ((uint32_t)p[21] << 16) | ((uint32_t)p[22] << 8) | p[23];
    if (s->cfg.compatibleVersion > kALACVersion) return -1;
    if (s->cfg.bitDepth != 16) return -2; /* 20/24/32: o miolo existe, a saída deste quadro não */
    if (s->cfg.frameLength < 1 || s->cfg.frameLength > 16384) return -1;
    if (s->cfg.numChannels < 1 || s->cfg.numChannels > 8) return -1;
    s->mixU = calloc((size_t)s->cfg.frameLength, sizeof(int32_t));
    s->mixV = calloc((size_t)s->cfg.frameLength, sizeof(int32_t));
    s->pred = calloc((size_t)s->cfg.frameLength, sizeof(int32_t));
    s->shift = (uint16_t *)s->pred;
    if (!s->mixU || !s->mixV || !s->pred) { free(s->mixU); free(s->mixV); free(s->pred); return -1; }
    return 0;
}
void alac_close(alac_st *s) {
    if (!s) return;
    free(s->mixU); free(s->mixV); free(s->pred);
    memset(s, 0, sizeof *s);
}

/* devolve 0 e *got quadros intercalados em out (capacidade em quadros). */
int alac_decode(alac_st *s, const uint8_t *pkt, int pkt_len, int16_t *out, int out_frames, int *got) {
    if (!s || !s->mixU || !pkt || pkt_len < 1 || !out || !got) return -1;
    BitBuffer bits;
    BitBufferInit(&bits, (uint8_t *)pkt, (uint32_t)pkt_len);
    uint32_t numSamples = (uint32_t)s->cfg.frameLength;
    uint32_t numChannels = s->cfg.numChannels;
    if (out_frames < (int)numSamples) return -1;
    uint32_t channelIndex = 0;
    int32_t status = ALAC_noErr;
    *got = (int)numSamples;
    while (status == ALAC_noErr) {
        if (bits.cur >= bits.end) { status = kALAC_ParamError; break; }
        uint16_t pb = s->cfg.pb;
        uint8_t tag = BitBufferReadSmall(&bits, 3);
        if (tag == ID_SCE || tag == ID_LFE) {
            BitBufferReadSmall(&bits, 4);
            uint16_t unused = (uint16_t)BitBufferRead(&bits, 12);
            if (unused != 0) { status = kALAC_ParamError; break; }
            uint8_t header = (uint8_t)BitBufferRead(&bits, 4);
            uint8_t partial = header >> 3;
            uint8_t bytesShifted = (header >> 1) & 3;
            if (bytesShifted == 3) { status = kALAC_ParamError; break; }
            uint8_t escape = header & 1;
            uint32_t chanBits = s->cfg.bitDepth - bytesShifted * 8;
            if (partial) { numSamples = BitBufferRead(&bits, 16) << 16; numSamples |= BitBufferRead(&bits, 16); }
            int16_t coefs[32];
            uint8_t numU = 0, modeU = 0;
            uint32_t denShift = 0;
            BitBuffer shiftBits;
            if (!escape) {
                BitBufferRead(&bits, 8); BitBufferRead(&bits, 8);
                header = (uint8_t)BitBufferRead(&bits, 8);
                modeU = header >> 4; denShift = header & 0xf;
                header = (uint8_t)BitBufferRead(&bits, 8);
                uint16_t pbFactor = header >> 5; numU = header & 0x1f;
                for (uint32_t i = 0; i < numU; i++) coefs[i] = (int16_t)BitBufferRead(&bits, 16);
                if (bytesShifted) { shiftBits = bits; BitBufferAdvance(&bits, (bytesShifted * 8) * numSamples); }
                AGParamRec ag;
                uint32_t bits1 = 0;
                set_ag_params(&ag, s->cfg.mb, (pb * pbFactor) / 4, s->cfg.kb, numSamples, numSamples, s->cfg.maxRun);
                status = dyn_decomp(&ag, &bits, s->pred, (int32_t)numSamples, (int32_t)chanBits, &bits1);
                if (status) break;
                if (modeU == 0) unpc_block(s->pred, s->mixU, (int32_t)numSamples, coefs, numU, chanBits, denShift);
                else { unpc_block(s->pred, s->pred, (int32_t)numSamples, NULL, 31, chanBits, 0); unpc_block(s->pred, s->mixU, (int32_t)numSamples, coefs, numU, chanBits, denShift); }
            } else {
                uint32_t shift = 32 - chanBits;
                for (uint32_t i = 0; i < numSamples; i++) {
                    int32_t val = (int32_t)BitBufferRead(&bits, (uint8_t)chanBits);
                    val = (val << shift) >> shift;
                    s->mixU[i] = val;
                }
                bytesShifted = 0;
            }
            if (bytesShifted) {
                uint32_t shift = bytesShifted * 8;
                for (uint32_t i = 0; i < numSamples; i++) s->shift[i] = (uint16_t)BitBufferRead(&shiftBits, (uint8_t)shift);
            }
            int16_t *o = out + channelIndex;
            for (uint32_t i = 0, j = 0; i < numSamples; i++, j += numChannels) o[j] = (int16_t)s->mixU[i];
            channelIndex += 1;
            *got = (int)numSamples;
        } else if (tag == ID_CPE) {
            if (channelIndex + 2 > numChannels) break;
            BitBufferReadSmall(&bits, 4);
            uint16_t unused = (uint16_t)BitBufferRead(&bits, 12);
            if (unused != 0) { status = kALAC_ParamError; break; }
            uint8_t header = (uint8_t)BitBufferRead(&bits, 4);
            uint8_t partial = header >> 3;
            uint8_t bytesShifted = (header >> 1) & 3;
            if (bytesShifted == 3) { status = kALAC_ParamError; break; }
            uint8_t escape = header & 1;
            uint32_t chanBits = s->cfg.bitDepth - bytesShifted * 8 + 1;
            if (partial) { numSamples = BitBufferRead(&bits, 16) << 16; numSamples |= BitBufferRead(&bits, 16); }
            int16_t coefsU[32], coefsV[32];
            uint8_t numU = 0, numV = 0, modeU = 0, modeV = 0, mixBits = 0;
            int8_t mixRes = 0;
            uint32_t denU = 0, denV = 0;
            BitBuffer shiftBits;
            if (!escape) {
                mixBits = (uint8_t)BitBufferRead(&bits, 8);
                mixRes = (int8_t)BitBufferRead(&bits, 8);
                header = (uint8_t)BitBufferRead(&bits, 8);
                modeU = header >> 4; denU = header & 0xf;
                header = (uint8_t)BitBufferRead(&bits, 8);
                uint16_t pbU = header >> 5; numU = header & 0x1f;
                for (uint32_t i = 0; i < numU; i++) coefsU[i] = (int16_t)BitBufferRead(&bits, 16);
                header = (uint8_t)BitBufferRead(&bits, 8);
                modeV = header >> 4; denV = header & 0xf;
                header = (uint8_t)BitBufferRead(&bits, 8);
                uint16_t pbV = header >> 5; numV = header & 0x1f;
                for (uint32_t i = 0; i < numV; i++) coefsV[i] = (int16_t)BitBufferRead(&bits, 16);
                if (bytesShifted) { shiftBits = bits; BitBufferAdvance(&bits, (bytesShifted * 8) * 2 * numSamples); }
                AGParamRec ag;
                uint32_t bits1 = 0;
                set_ag_params(&ag, s->cfg.mb, (pb * pbU) / 4, s->cfg.kb, numSamples, numSamples, s->cfg.maxRun);
                status = dyn_decomp(&ag, &bits, s->pred, (int32_t)numSamples, (int32_t)chanBits, &bits1);
                if (status) break;
                if (modeU == 0) unpc_block(s->pred, s->mixU, (int32_t)numSamples, coefsU, numU, chanBits, denU);
                else { unpc_block(s->pred, s->pred, (int32_t)numSamples, NULL, 31, chanBits, 0); unpc_block(s->pred, s->mixU, (int32_t)numSamples, coefsU, numU, chanBits, denU); }
                set_ag_params(&ag, s->cfg.mb, (pb * pbV) / 4, s->cfg.kb, numSamples, numSamples, s->cfg.maxRun);
                status = dyn_decomp(&ag, &bits, s->pred, (int32_t)numSamples, (int32_t)chanBits, &bits1);
                if (status) break;
                if (modeV == 0) unpc_block(s->pred, s->mixV, (int32_t)numSamples, coefsV, numV, chanBits, denV);
                else { unpc_block(s->pred, s->pred, (int32_t)numSamples, NULL, 31, chanBits, 0); unpc_block(s->pred, s->mixV, (int32_t)numSamples, coefsV, numV, chanBits, denV); }
            } else {
                chanBits = s->cfg.bitDepth;
                uint32_t shift = 32 - chanBits;
                for (uint32_t i = 0; i < numSamples; i++) {
                    int32_t val = (int32_t)BitBufferRead(&bits, (uint8_t)chanBits);
                    s->mixU[i] = (val << shift) >> shift;
                    val = (int32_t)BitBufferRead(&bits, (uint8_t)chanBits);
                    s->mixV[i] = (val << shift) >> shift;
                }
                mixBits = 0; mixRes = 0; bytesShifted = 0;
            }
            if (bytesShifted) {
                uint32_t shift = bytesShifted * 8;
                for (uint32_t i = 0; i < numSamples * 2; i += 2) {
                    s->shift[i] = (uint16_t)BitBufferRead(&shiftBits, (uint8_t)shift);
                    s->shift[i + 1] = (uint16_t)BitBufferRead(&shiftBits, (uint8_t)shift);
                }
            }
            unmix16(s->mixU, s->mixV, out + channelIndex, numChannels, (int32_t)numSamples, mixBits, mixRes);
            channelIndex += 2;
            *got = (int)numSamples;
        } else if (tag == ID_CCE || tag == ID_PCE) { status = kALAC_ParamError; break; }
        else if (tag == ID_DSE) { status = data_element(&bits); }
        else if (tag == ID_FIL) { status = fill_element(&bits); }
        else if (tag == ID_END) { BitBufferByteAlign(&bits, 0); break; }
        else { status = kALAC_ParamError; break; }
        if (channelIndex >= numChannels) break;
    }
    for (; channelIndex < numChannels; channelIndex++) zero16(out + channelIndex, numSamples, numChannels);
    return status == ALAC_noErr ? 0 : -1;
}
