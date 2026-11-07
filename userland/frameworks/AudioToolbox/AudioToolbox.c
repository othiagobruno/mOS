/*
 * mOS — AudioToolbox.framework (Mach-O, em /System/Library/Frameworks): os sons de sistema (AudioServices). O som
 * de alerta vai para o NSBeep do AppKit do mOS; arquivos de som próprios ainda não tocam (o id fica registrado).
 * tools/build-frameworks-objc.ps1
 */
#include <stdint.h>
#include <stdio.h>

typedef int32_t OSStatus;
typedef uint32_t SystemSoundID;
typedef const void *CFURLRef;
#define kSystemSoundID_UserPreferredAlert 0x00001000

void NSBeep(void);

static SystemSoundID next_id = 0x10000;

OSStatus AudioServicesCreateSystemSoundID(CFURLRef url, SystemSoundID *out) {
    (void)url;
    if (!out) return -50;   /* paramErr */
    *out = next_id++;
    return 0;
}

OSStatus AudioServicesDisposeSystemSoundID(SystemSoundID id) { (void)id; return 0; }

void AudioServicesPlaySystemSound(SystemSoundID id) {
    if (id == kSystemSoundID_UserPreferredAlert) { NSBeep(); return; }
    static int warned;
    if (!warned++) fprintf(stderr, "AudioToolbox[mOS]: AudioServicesPlaySystemSound de arquivo ainda não toca (id %u)\n", id);
}

void AudioServicesPlayAlertSound(SystemSoundID id) { (void)id; NSBeep(); }
