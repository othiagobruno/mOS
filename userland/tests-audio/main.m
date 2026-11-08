/*
 * mOS — teste do áudio dos apps: CoreAudio (HAL), AudioUnit, AudioQueue, AudioConverter, AudioFile/ExtAudioFile,
 * AudioServices, AVAudioFile, AVAudioPlayer, AVAudioEngine (player, source, tap), AVPlayer e NSSound
 * (userland/libsystem/coreaudio.c, avfaudio.c; kernel/dev/sound.c). Saída: "audiotest: ok/FALHA <nome>" e "audiotest: N/M ok".
 *   .\build.ps1 -OutDir build-aud
 *   .\tools\build-audiotest.ps1 -OutDir build-aud
 *   .\run.ps1 -Headless -Image build-aud\mOS.img -Serial build-aud\serial.log -WavOut build-aud\saida.wav
 */
#import <AppKit/AppKit.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int OSStatus;
typedef uint32_t UInt32;
typedef uint64_t UInt64;
typedef unsigned char Boolean;
typedef const void *CFTypeRef;
typedef struct { double mSampleRate; UInt32 mFormatID, mFormatFlags, mBytesPerPacket, mFramesPerPacket, mBytesPerFrame, mChannelsPerFrame, mBitsPerChannel, mReserved; } ASBD;
typedef struct { UInt32 mNumberChannels, mDataByteSize; void *mData; } ABuf;
typedef struct { UInt32 mNumberBuffers; ABuf mBuffers[2]; } ABL;
typedef struct { UInt32 sel, scope, el; } PAddr;
typedef struct { double mSampleTime; UInt64 mHostTime; double mRateScalar; UInt64 mWordClockTime; char smpte[24]; UInt32 mFlags, mReserved; } ATS;
typedef struct { UInt32 type, subType, manu, flags, mask; } ACDesc;
typedef struct { UInt32 cap; void *data; UInt32 size; void *ud; UInt32 pdcap; void *pd; UInt32 pdcount; } AQBuf;
#define FCC(a, b, c, d) ((UInt32)(a) << 24 | (UInt32)(b) << 16 | (UInt32)(c) << 8 | (UInt32)(d))

OSStatus AudioObjectGetPropertyData(UInt32 o, const PAddr *a, UInt32 qs, const void *q, UInt32 *sz, void *d);
OSStatus AudioObjectGetPropertyDataSize(UInt32 o, const PAddr *a, UInt32 qs, const void *q, UInt32 *sz);
OSStatus AudioDeviceCreateIOProcID(UInt32 d, void *proc, void *ctx, void **id);
OSStatus AudioDeviceStart(UInt32 d, void *id);
OSStatus AudioDeviceStop(UInt32 d, void *id);
OSStatus AudioDeviceDestroyIOProcID(UInt32 d, void *id);
OSStatus AudioDeviceGetCurrentTime(UInt32 d, ATS *t);
void *AudioComponentFindNext(void *after, const ACDesc *d);
OSStatus AudioComponentInstanceNew(void *c, void **u);
OSStatus AudioComponentInstanceDispose(void *u);
OSStatus AudioUnitSetProperty(void *u, UInt32 id, UInt32 sc, UInt32 el, const void *d, UInt32 sz);
OSStatus AudioUnitGetProperty(void *u, UInt32 id, UInt32 sc, UInt32 el, void *d, UInt32 *sz);
OSStatus AudioUnitInitialize(void *u);
OSStatus AudioOutputUnitStart(void *u);
OSStatus AudioOutputUnitStop(void *u);
OSStatus AudioQueueNewOutput(const ASBD *f, void *cb, void *ud, void *rl, id mode, UInt32 flags, void **q);
OSStatus AudioQueueAllocateBuffer(void *q, UInt32 size, AQBuf **b);
OSStatus AudioQueueEnqueueBuffer(void *q, AQBuf *b, UInt32 n, const void *pd);
OSStatus AudioQueueStart(void *q, const ATS *t);
OSStatus AudioQueueStop(void *q, Boolean immediate);
OSStatus AudioQueueDispose(void *q, Boolean immediate);
OSStatus AudioQueueAddPropertyListener(void *q, UInt32 id, void *fn, void *ud);
OSStatus AudioQueueGetProperty(void *q, UInt32 id, void *d, UInt32 *sz);
OSStatus AudioQueueGetCurrentTime(void *q, void *tl, ATS *t, Boolean *disc);
OSStatus AudioConverterNew(const ASBD *s, const ASBD *d, void **c);
OSStatus AudioConverterConvertBuffer(void *c, UInt32 insz, const void *in, UInt32 *outsz, void *out);
OSStatus AudioConverterDispose(void *c);
OSStatus ExtAudioFileCreateWithURL(id url, UInt32 type, const ASBD *f, const void *layout, UInt32 flags, void **e);
OSStatus ExtAudioFileWrite(void *e, UInt32 n, const void *abl);
OSStatus ExtAudioFileDispose(void *e);
OSStatus ExtAudioFileOpenURL(id url, void **e);
OSStatus ExtAudioFileSetProperty(void *e, UInt32 id, UInt32 sz, const void *d);
OSStatus ExtAudioFileGetProperty(void *e, UInt32 id, UInt32 *sz, void *d);
OSStatus ExtAudioFileRead(void *e, UInt32 *n, void *abl);
OSStatus AudioServicesCreateSystemSoundID(id url, UInt32 *id);
void AudioServicesPlaySystemSoundWithCompletion(UInt32 id, void (^done)(void));

@interface NSURL : NSObject
+ (instancetype)fileURLWithPath:(NSString *)p;
@end
@interface NSError (T)
- (NSInteger)code;
@end
@interface NSSet : NSObject
- (NSUInteger)count;
@end
@interface NSObject (T)
- (void)performSelector:(SEL)s withObject:(id)o afterDelay:(double)d;
@end
@interface NSNotificationCenter (T)
- (id)addObserverForName:(NSString *)n object:(id)o queue:(id)q usingBlock:(void (^)(NSNotification *))b;
@end
@interface AVAudioFormat : NSObject
- (instancetype)initStandardFormatWithSampleRate:(double)r channels:(UInt32)c;
- (instancetype)initWithCommonFormat:(NSUInteger)f sampleRate:(double)r channels:(UInt32)c interleaved:(BOOL)i;
@property (readonly) double sampleRate;
@property (readonly) UInt32 channelCount;
@end
@interface AVAudioPCMBuffer : NSObject
- (instancetype)initWithPCMFormat:(AVAudioFormat *)f frameCapacity:(UInt32)n;
@property UInt32 frameLength;
@property (readonly) float *const *floatChannelData;
@property (readonly) int16_t *const *int16ChannelData;
@end
@interface AVAudioTime : NSObject
@property (readonly) int64_t sampleTime;
@end
@interface AVAudioFile : NSObject
- (instancetype)initForReading:(NSURL *)u error:(NSError **)e;
- (instancetype)initForWriting:(NSURL *)u settings:(NSDictionary *)s error:(NSError **)e;
- (BOOL)readIntoBuffer:(AVAudioPCMBuffer *)b error:(NSError **)e;
- (BOOL)writeFromBuffer:(AVAudioPCMBuffer *)b error:(NSError **)e;
@property (readonly) int64_t length;
@property (readonly) AVAudioFormat *processingFormat;
@property (readonly) AVAudioFormat *fileFormat;
@end
@interface AVAudioNode : NSObject
- (void)installTapOnBus:(NSUInteger)b bufferSize:(UInt32)n format:(AVAudioFormat *)f block:(void (^)(AVAudioPCMBuffer *, AVAudioTime *))blk;
- (void)removeTapOnBus:(NSUInteger)b;
@property float volume;
@end
@interface AVAudioMixerNode : AVAudioNode
@property float outputVolume;
@end
@interface AVAudioPlayerNode : AVAudioNode
- (void)scheduleBuffer:(AVAudioPCMBuffer *)b completionHandler:(void (^)(void))h;
- (void)scheduleFile:(AVAudioFile *)f atTime:(AVAudioTime *)t completionHandler:(void (^)(void))h;
- (void)play;
- (void)stop;
@property (readonly) BOOL isPlaying;
@end
@interface AVAudioSourceNode : AVAudioNode
- (instancetype)initWithRenderBlock:(OSStatus (^)(BOOL *, const ATS *, UInt32, ABL *))b;
@end
@interface AVAudioEngine : NSObject
@property (readonly) AVAudioMixerNode *mainMixerNode;
@property (readonly) AVAudioNode *outputNode;
- (void)attachNode:(AVAudioNode *)n;
- (void)connect:(AVAudioNode *)a to:(AVAudioNode *)b format:(AVAudioFormat *)f;
- (BOOL)startAndReturnError:(NSError **)e;
- (void)stop;
@property (readonly) BOOL isRunning;
@property (readonly) NSSet *attachedNodes;
@end
@interface AVAudioPlayer : NSObject
- (instancetype)initWithContentsOfURL:(NSURL *)u error:(NSError **)e;
- (instancetype)initWithData:(NSData *)d error:(NSError **)e;
- (BOOL)play;
- (void)stop;
@property (weak) id delegate;
@property (readonly) double duration;
@property double currentTime;
@property (readonly) BOOL isPlaying;
@property float volume;
@property NSInteger numberOfLoops;
@end
@interface AVAudioSession : NSObject
+ (instancetype)sharedInstance;
- (BOOL)setCategory:(NSString *)c error:(NSError **)e;
- (BOOL)setActive:(BOOL)a error:(NSError **)e;
@property (readonly) double sampleRate;
@property (readonly) NSString *category;
@end
@interface AVPlayer : NSObject
+ (instancetype)playerWithURL:(NSURL *)u;
- (void)play;
@property (readonly) NSInteger status;
@property (readonly) id currentItem;
@property float rate;
@end
@interface NSSound : NSObject
- (instancetype)initWithContentsOfFile:(NSString *)p byReference:(BOOL)r;
- (BOOL)play;
@property (weak) id delegate;
@property (readonly) double duration;
@end

static int g_ok, g_total;
static void check(BOOL c, const char *name, NSString *d) {
    g_total++;
    if (c) { g_ok++; fprintf(stderr, "audiotest: ok %s\n", name); }
    else fprintf(stderr, "audiotest: FALHA %s (%s)\n", name, d ? [[d description] UTF8String] : "");
}
static void spin(double s) { [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:s]]; }
static BOOL wait_for(BOOL (^c)(void), double s) {
    NSDate *end = [NSDate dateWithTimeIntervalSinceNow:s];
    while (!c()) { if ([end timeIntervalSinceNow] < 0) return NO; spin(0.02); }
    return YES;
}

/* ---- callbacks em C */
static volatile int g_io_calls;
static volatile double g_io_last;
static OSStatus io_proc(UInt32 dev, const ATS *now, const ABL *in, const ATS *intime, ABL *out, const ATS *outtime, void *ctx) {
    g_io_calls++;
    g_io_last = outtime->mSampleTime;
    float *f = out->mBuffers[0].mData;
    UInt32 n = out->mBuffers[0].mDataByteSize / 8;
    static double ph;
    for (UInt32 i = 0; i < n; i++) { float v = 0.2f * sinf((float)ph); f[i * 2] = f[i * 2 + 1] = v; ph += 2 * M_PI * 440 / 48000; }
    return 0;
}
static volatile long g_au_frames;
static OSStatus au_render(void *ref, UInt32 *flags, const ATS *ts, UInt32 bus, UInt32 n, ABL *data) {
    int16_t *s = data->mBuffers[0].mData;
    static double ph;
    for (UInt32 i = 0; i < n; i++) { int16_t v = (int16_t)(8000 * sin(ph)); s[i * 2] = s[i * 2 + 1] = v; ph += 2 * M_PI * 660 / 44100; }
    g_au_frames += n;
    return 0;
}
static volatile int g_aq_cb, g_aq_running_changes, g_aq_running = 1;
static void aq_cb(void *ud, void *q, AQBuf *b) {
    g_aq_cb++;
    if (g_aq_cb < 6) {   /* reabastece algumas vezes, depois deixa esvaziar */
        float *f = b->data;
        for (UInt32 i = 0; i < b->cap / 4; i++) f[i] = 0.1f * sinf((float)i * 0.2f);
        b->size = b->cap;
        AudioQueueEnqueueBuffer(q, b, 0, 0);
    }
}
static void aq_prop(void *ud, void *q, UInt32 pid) {
    UInt32 r = 0, sz = 4;
    AudioQueueGetProperty(q, FCC('a', 'q', 'r', 'n'), &r, &sz);
    g_aq_running = (int)r;
    g_aq_running_changes++;
}

@interface Del : NSObject
@property int finished;
@property int soundDone;
@end
@implementation Del
- (void)audioPlayerDidFinishPlaying:(AVAudioPlayer *)p successfully:(BOOL)ok { self.finished++; }
- (void)sound:(NSSound *)s didFinishPlaying:(BOOL)ok { self.soundDone++; }
@end

static NSString *write_wav(NSString *path, double rate, UInt32 ch, double secs, double freq) {
    ASBD f = {rate, FCC('l', 'p', 'c', 'm'), 4 | 8, 2 * ch, 1, 2 * ch, ch, 16, 0};
    void *e = 0;
    OSStatus r = ExtAudioFileCreateWithURL([NSURL fileURLWithPath:path], FCC('W', 'A', 'V', 'E'), &f, 0, 1, &e);
    if (r) return nil;
    UInt32 n = (UInt32)(rate * secs);
    int16_t *s = malloc(n * 2 * ch);
    for (UInt32 i = 0; i < n; i++) for (UInt32 c = 0; c < ch; c++) s[i * ch + c] = (int16_t)(12000 * sin(2 * M_PI * freq * i / rate));
    ABL l = {1, {{ch, n * 2 * ch, s}}};
    ExtAudioFileWrite(e, n, &l);
    ExtAudioFileDispose(e);
    free(s);
    return path;
}

static void run_tests(void) {
    /* 1. HAL: dispositivo de saída padrão e propriedades */
    PAddr a = {FCC('d', 'O', 'u', 't'), FCC('g', 'l', 'o', 'b'), 0};
    UInt32 dev = 0, sz = 4;
    check(AudioObjectGetPropertyData(1, &a, 0, 0, &sz, &dev) == 0 && dev == 2, "kAudioHardwarePropertyDefaultOutputDevice", nil);
    PAddr nm = {FCC('l', 'n', 'a', 'm'), FCC('g', 'l', 'o', 'b'), 0};
    CFTypeRef name = 0;
    sz = 8;
    check(AudioObjectGetPropertyData(dev, &nm, 0, 0, &sz, &name) == 0 && name && [(__bridge id)name length] > 0, "nome do dispositivo", nil);
    PAddr sr = {FCC('n', 's', 'r', 't'), FCC('g', 'l', 'o', 'b'), 0};
    double rate = 0;
    sz = 8;
    check(AudioObjectGetPropertyData(dev, &sr, 0, 0, &sz, &rate) == 0 && rate == 48000, "taxa nominal 48 kHz", nil);
    PAddr fm = {FCC('s', 'f', 'm', 't'), FCC('o', 'u', 't', 'p'), 0};
    ASBD dfmt;
    sz = sizeof dfmt;
    check(AudioObjectGetPropertyData(dev, &fm, 0, 0, &sz, &dfmt) == 0 && dfmt.mChannelsPerFrame == 2 && dfmt.mBitsPerChannel == 32, "formato do stream (float32 estéreo)", nil);
    PAddr sl = {FCC('s', 'l', 'a', 'y'), FCC('o', 'u', 't', 'p'), 0};
    sz = 0;
    check(AudioObjectGetPropertyDataSize(dev, &sl, 0, 0, &sz) == 0 && sz >= 24, "kAudioDevicePropertyStreamConfiguration", nil);

    /* 2. IOProc */
    void *pid = 0;
    check(AudioDeviceCreateIOProcID(dev, io_proc, 0, &pid) == 0 && pid, "AudioDeviceCreateIOProcID", nil);
    AudioDeviceStart(dev, pid);
    BOOL ran = wait_for(^{ return (BOOL)(g_io_calls > 20); }, 5);
    ATS now;
    OSStatus tr = AudioDeviceGetCurrentTime(dev, &now);
    check(ran, "IOProc chamado em ritmo de dispositivo", [NSString stringWithFormat:@"%d chamadas", g_io_calls]);
    check(tr == 0 && now.mSampleTime > 0 && g_io_last > 0, "AudioDeviceGetCurrentTime anda", nil);
    AudioDeviceStop(dev, pid);
    int calls = g_io_calls;
    spin(0.3);
    check(g_io_calls <= calls + 1, "AudioDeviceStop para o IOProc", nil);
    AudioDeviceDestroyIOProcID(dev, pid);

    /* 3. AudioUnit DefaultOutput com formato do cliente 44,1 kHz int16 intercalado */
    ACDesc d = {FCC('a', 'u', 'o', 'u'), FCC('d', 'e', 'f', ' '), FCC('a', 'p', 'p', 'l'), 0, 0};
    void *comp = AudioComponentFindNext(0, &d), *au = 0;
    check(comp && AudioComponentInstanceNew(comp, &au) == 0, "AudioComponent DefaultOutput", nil);
    ASBD cf = {44100, FCC('l', 'p', 'c', 'm'), 4 | 8, 4, 1, 4, 2, 16, 0};
    check(AudioUnitSetProperty(au, 8, 1, 0, &cf, sizeof cf) == 0, "StreamFormat do cliente (int16 44,1 kHz)", nil);
    struct { void *fn, *ref; } cb = {au_render, 0};
    AudioUnitSetProperty(au, 23, 1, 0, &cb, sizeof cb);
    AudioUnitInitialize(au);
    check(AudioOutputUnitStart(au) == 0, "AudioOutputUnitStart", nil);
    NSDate *t0 = [NSDate date];
    spin(1.5);
    double el = -[t0 timeIntervalSinceNow];
    AudioOutputUnitStop(au);
    double per = g_au_frames / el;
    check(per > 30000 && per < 60000, "render callback na taxa do cliente", [NSString stringWithFormat:@"%.0f quadros/s", per]);
    AudioComponentInstanceDispose(au);

    /* 4. AudioQueue de saída: callbacks, tempo, parada com drenagem e IsRunning */
    ASBD qf = {22050, FCC('l', 'p', 'c', 'm'), 1 | 8, 4, 1, 4, 1, 32, 0};
    void *q = 0;
    check(AudioQueueNewOutput(&qf, aq_cb, 0, 0, 0, 0, &q) == 0, "AudioQueueNewOutput", nil);
    AudioQueueAddPropertyListener(q, FCC('a', 'q', 'r', 'n'), aq_prop, 0);
    for (int i = 0; i < 3; i++) {
        AQBuf *b = 0;
        AudioQueueAllocateBuffer(q, 4410 * 4, &b);
        float *f = b->data;
        for (UInt32 k = 0; k < 4410; k++) f[k] = 0.1f * sinf((float)k * 0.3f);
        b->size = 4410 * 4;
        AudioQueueEnqueueBuffer(q, b, 0, 0);
    }
    AudioQueueStart(q, 0);
    ran = wait_for(^{ return (BOOL)(g_aq_cb >= 4); }, 5);
    check(ran, "callbacks do AudioQueue", [NSString stringWithFormat:@"%d", g_aq_cb]);
    ATS qt;
    AudioQueueGetCurrentTime(q, 0, &qt, 0);
    check(qt.mSampleTime > 2000, "AudioQueueGetCurrentTime anda", [NSString stringWithFormat:@"%.0f", qt.mSampleTime]);
    AudioQueueStop(q, false);
    ran = wait_for(^{ return (BOOL)(g_aq_running == 0); }, 5);
    check(ran && g_aq_running_changes >= 2, "Stop sem imediato drena e avisa IsRunning", nil);
    AudioQueueDispose(q, true);

    /* 5. AudioConverter int16 estéreo -> float32 mono */
    ASBD s16 = {48000, FCC('l', 'p', 'c', 'm'), 4 | 8, 4, 1, 4, 2, 16, 0}, f32 = {48000, FCC('l', 'p', 'c', 'm'), 1 | 8, 4, 1, 4, 1, 32, 0};
    void *cv = 0;
    int16_t in[8] = {16384, 16384, -16384, -16384, 0, 0, 32767, 32767};
    float out[4] = {0};
    UInt32 osz = sizeof out;
    check(AudioConverterNew(&s16, &f32, &cv) == 0 && AudioConverterConvertBuffer(cv, sizeof in, in, &osz, out) == 0 && osz == 16 &&
          fabsf(out[0] - 0.5f) < 0.01f && fabsf(out[1] + 0.5f) < 0.01f, "AudioConverter int16 estéreo -> float mono", nil);
    AudioConverterDispose(cv);

    /* 6. arquivos: grava WAV, lê de volta (ExtAudioFile com formato de cliente e AVAudioFile) */
    NSString *wav = write_wav(@"/tmp/mos-teste-tom.wav", 22050, 1, 0.5, 440);
    check(wav != nil, "ExtAudioFileCreateWithURL + Write (WAV)", nil);
    void *ef = 0;
    check(ExtAudioFileOpenURL([NSURL fileURLWithPath:wav], &ef) == 0, "ExtAudioFileOpenURL", nil);
    ASBD cl = {44100, FCC('l', 'p', 'c', 'm'), 1 | 8, 8, 1, 8, 2, 32, 0};
    ExtAudioFileSetProperty(ef, FCC('c', 'f', 'm', 't'), sizeof cl, &cl);
    int64_t frames = 0;
    sz = 8;
    ExtAudioFileGetProperty(ef, FCC('#', 'f', 'r', 'm'), &sz, &frames);
    check(frames == 11025, "kExtAudioFileProperty_FileLengthFrames", [NSString stringWithFormat:@"%lld", frames]);
    float *rb = malloc(4096 * 8);
    ABL rl = {1, {{2, 4096 * 8, rb}}};
    UInt32 rn = 2048;
    ExtAudioFileRead(ef, &rn, &rl);
    float peak = 0;
    for (UInt32 i = 0; i < rn * 2; i++) peak = fmaxf(peak, fabsf(rb[i]));
    check(rn > 2000 && peak > 0.3f, "ExtAudioFileRead convertido (44,1 kHz float estéreo)", [NSString stringWithFormat:@"n=%u pico=%.2f", rn, peak]);
    free(rb);
    NSError *err = nil;
    AVAudioFile *af = [[AVAudioFile alloc] initForReading:[NSURL fileURLWithPath:wav] error:&err];
    check(af && af.length == 11025 && af.processingFormat.sampleRate == 22050, "AVAudioFile initForReading", [err description]);
    AVAudioPCMBuffer *pb = [[AVAudioPCMBuffer alloc] initWithPCMFormat:af.processingFormat frameCapacity:(UInt32)af.length];
    check([af readIntoBuffer:pb error:&err] && pb.frameLength == 11025 && fabsf(pb.floatChannelData[0][100]) > 0.0f, "readIntoBuffer", nil);
    NSString *wav2 = @"/tmp/mos-teste-av.wav";
    AVAudioFile *wf = [[AVAudioFile alloc] initForWriting:[NSURL fileURLWithPath:wav2]
                                                 settings:@{@"AVFormatIDKey": @((int)FCC('l', 'p', 'c', 'm')), @"AVSampleRateKey": @22050, @"AVNumberOfChannelsKey": @1,
                                                            @"AVLinearPCMBitDepthKey": @16, @"AVLinearPCMIsFloatKey": @NO} error:&err];
    check(wf && [wf writeFromBuffer:pb error:&err], "AVAudioFile initForWriting + writeFromBuffer", [err description]);
    wf = nil;

    /* 7. AVAudioPlayer: duração, play, delegate no fim; formato comprimido recusado */
    Del *del = [[Del alloc] init];
    AVAudioPlayer *pl = [[AVAudioPlayer alloc] initWithContentsOfURL:[NSURL fileURLWithPath:wav2] error:&err];
    check(pl && fabs(pl.duration - 0.5) < 0.01, "AVAudioPlayer duration", [err description]);
    pl.delegate = del;
    check([pl play] && pl.isPlaying, "AVAudioPlayer play", nil);
    BOOL fin = wait_for(^{ return (BOOL)(del.finished > 0); }, 5);
    check(fin && !pl.isPlaying, "audioPlayerDidFinishPlaying:successfully:", nil);
    unsigned char mp3[16] = {'I', 'D', '3', 3, 0, 0, 0, 0, 0, 0, 0xFF, 0xFB, 0x90, 0x64, 0, 0};
    AVAudioPlayer *bad = [[AVAudioPlayer alloc] initWithData:[NSData dataWithBytes:mp3 length:sizeof mp3] error:&err];
    check(!bad && err && [err code] == FCC('f', 'm', 't', '?'), "MP3 ainda recusado com kAudioFileUnsupportedDataFormatError", [err description]);

    /* 8. AVAudioEngine: player com buffer e conclusão, tap no mixer com amostras, source node */
    AVAudioEngine *eng = [[AVAudioEngine alloc] init];
    AVAudioPlayerNode *pn = [[AVAudioPlayerNode alloc] init];
    [eng attachNode:pn];
    AVAudioFormat *f44 = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100 channels:2];
    [eng connect:pn to:eng.mainMixerNode format:f44];
    AVAudioPCMBuffer *tone = [[AVAudioPCMBuffer alloc] initWithPCMFormat:f44 frameCapacity:22050];
    tone.frameLength = 22050;
    for (UInt32 i = 0; i < 22050; i++) tone.floatChannelData[0][i] = tone.floatChannelData[1][i] = 0.3f * sinf(2 * M_PI * 523.25f * i / 44100);
    __block int done = 0;
    [pn scheduleBuffer:tone completionHandler:^{ done++; }];
    __block float tap_peak = 0;
    __block int taps = 0;
    [eng.mainMixerNode installTapOnBus:0 bufferSize:4096 format:nil block:^(AVAudioPCMBuffer *b, AVAudioTime *w) {
        for (UInt32 i = 0; i < b.frameLength; i++) tap_peak = fmaxf(tap_peak, fabsf(b.floatChannelData[0][i]));
        taps++;
    }];
    check([eng startAndReturnError:&err] && eng.isRunning, "AVAudioEngine startAndReturnError", [err description]);
    [pn play];
    fin = wait_for(^{ return (BOOL)(done > 0); }, 5);
    check(fin, "AVAudioPlayerNode completionHandler", nil);
    check(taps > 0 && tap_peak > 0.2f, "tap do mainMixerNode com o som do player", [NSString stringWithFormat:@"taps=%d pico=%.2f", taps, tap_peak]);
    __block int src_calls = 0;
    AVAudioSourceNode *sn = [[AVAudioSourceNode alloc] initWithRenderBlock:^OSStatus(BOOL *sil, const ATS *ts, UInt32 n, ABL *o) {
        float *l = o->mBuffers[0].mData;
        for (UInt32 i = 0; i < n; i++) l[i] = 0.05f * sinf((float)i * 0.1f);
        src_calls++;
        return 0;
    }];
    [eng attachNode:sn];
    [eng connect:sn to:eng.mainMixerNode format:[[AVAudioFormat alloc] initStandardFormatWithSampleRate:48000 channels:1]];
    fin = wait_for(^{ return (BOOL)(src_calls > 5); }, 5);
    check(fin, "AVAudioSourceNode render block", [NSString stringWithFormat:@"%d", src_calls]);
    check(eng.attachedNodes.count >= 3, "attachedNodes", nil);
    [eng.mainMixerNode removeTapOnBus:0];
    [eng stop];
    check(!eng.isRunning, "AVAudioEngine stop", nil);

    /* 9. AudioServices com som de arquivo e conclusão */
    UInt32 sid = 0;
    check(AudioServicesCreateSystemSoundID([NSURL fileURLWithPath:wav], &sid) == 0 && sid, "AudioServicesCreateSystemSoundID", nil);
    __block int ss = 0;
    AudioServicesPlaySystemSoundWithCompletion(sid, ^{ ss++; });
    fin = wait_for(^{ return (BOOL)(ss > 0); }, 5);
    check(fin, "AudioServicesPlaySystemSoundWithCompletion", nil);

    /* 10. AVAudioSession, AVPlayer, NSSound */
    AVAudioSession *se = [AVAudioSession sharedInstance];
    check([se setCategory:@"AVAudioSessionCategoryPlayback" error:nil] && [se setActive:YES error:nil] && se.sampleRate == 48000 &&
          [se.category isEqualToString:@"AVAudioSessionCategoryPlayback"], "AVAudioSession", nil);
    AVPlayer *avp = [AVPlayer playerWithURL:[NSURL fileURLWithPath:wav]];
    __block int ended = 0;
    [[NSNotificationCenter defaultCenter] addObserverForName:@"AVPlayerItemDidPlayToEndTimeNotification" object:avp.currentItem queue:nil
                                                  usingBlock:^(NSNotification *n) { ended++; }];
    check(avp.status == 1, "AVPlayer status ReadyToPlay", nil);
    [avp play];
    fin = wait_for(^{ return (BOOL)(ended > 0); }, 5);
    check(fin && avp.rate == 0, "AVPlayerItemDidPlayToEndTimeNotification", nil);
    NSSound *snd = [[NSSound alloc] initWithContentsOfFile:wav byReference:YES];
    snd.delegate = del;
    check(snd && fabs(snd.duration - 0.5) < 0.01 && [snd play], "NSSound play", nil);
    fin = wait_for(^{ return (BOOL)(del.soundDone > 0); }, 5);
    check(fin, "sound:didFinishPlaying:", nil);
    NSBeep();

    fprintf(stderr, "audiotest: %d/%d ok\n", g_ok, g_total);
    fflush(stderr);
    exit(g_ok == g_total ? 0 : 1);
}

@interface Starter : NSObject
@end
@implementation Starter
- (void)go { run_tests(); }
@end

int main(void) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        Starter *s = [[Starter alloc] init];
        [s performSelector:@selector(go) withObject:nil afterDelay:0.1];
        [NSApp run];
    }
    return 0;
}
