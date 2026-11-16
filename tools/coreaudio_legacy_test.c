/* Host: API antiga do CoreAudio (AudioHardware/AudioDevice...Property, AudioDeviceAddIOProc) sobre o coreaudio.c de
 * verdade (sem taps de processo). → "coreaudio legacy ok" */
#include <stdio.h>
#include <string.h>
typedef int OSStatus;
OSStatus AudioHardwareGetProperty(unsigned, unsigned *, void *);
OSStatus AudioHardwareGetPropertyInfo(unsigned, unsigned *, unsigned char *);
OSStatus AudioDeviceGetProperty(unsigned, unsigned, unsigned char, unsigned, unsigned *, void *);
OSStatus AudioDeviceGetPropertyInfo(unsigned, unsigned, unsigned char, unsigned, unsigned *, unsigned char *);
OSStatus AudioDeviceAddIOProc(unsigned, void *, void *);
OSStatus AudioDeviceRemoveIOProc(unsigned, void *);
OSStatus AudioDeviceStart(unsigned, void *);
OSStatus AudioDeviceStop(unsigned, void *);
/* sem taps de processo neste teste */
int cx_tap_owns(unsigned d) { (void)d; return 0; }
int cx_tap_remove_ioproc(void *pid) { (void)pid; return 0; }
#define FCC(a, b, c, d) ((unsigned)(a) << 24 | (unsigned)(b) << 16 | (unsigned)(c) << 8 | (unsigned)(d))
static OSStatus proc(unsigned d, const void *now, const void *in, const void *it, void *out, const void *ot, void *ctx) { (void)d; (void)now; (void)in; (void)it; (void)out; (void)ot; (void)ctx; return 0; }
static OSStatus other(unsigned d, const void *now, const void *in, const void *it, void *out, const void *ot, void *ctx) { (void)d; (void)now; (void)in; (void)it; (void)out; (void)ot; (void)ctx; return 0; }
int main(void) {
    int ok = 1;
    unsigned dev = 0, sz = sizeof dev, isz = 0;
    unsigned char w = 9;
    ok &= AudioHardwareGetPropertyInfo(FCC('d', 'O', 'u', 't'), &isz, &w) == 0 && isz == 4;
    ok &= AudioHardwareGetProperty(FCC('d', 'O', 'u', 't'), &sz, &dev) == 0 && dev != 0;
    double rate = 0; sz = sizeof rate;
    ok &= AudioDeviceGetProperty(dev, 0, 0, FCC('n', 's', 'r', 't'), &sz, &rate) == 0 && rate > 0;
    ok &= AudioDeviceGetPropertyInfo(dev, 0, 0, FCC('n', 's', 'r', 't'), &isz, &w) == 0 && isz == 8;
    ok &= AudioDeviceAddIOProc(dev, (void *)proc, 0) == 0 && AudioDeviceAddIOProc(dev, (void *)proc, 0) != 0;   /* repetida */
    ok &= AudioDeviceStart(dev, (void *)proc) == 0 && AudioDeviceStop(dev, (void *)proc) == 0;
    ok &= AudioDeviceStart(dev, (void *)other) != 0;   /* nunca registrada */
    ok &= AudioDeviceRemoveIOProc(dev, (void *)proc) == 0 && AudioDeviceRemoveIOProc(dev, (void *)proc) != 0;
    printf("dispositivo %u, %.0f Hz\n", dev, rate);
    puts(ok ? "coreaudio legacy ok" : "coreaudio legacy FALHA");
    return !ok;
}
