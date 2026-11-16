/* Só para o teste de host do AudioConverter (tools/audioconverter_test.c): o resto do coreaudio.c (HAL, AudioQueue,
 * ObjC, TCC) entra no link, mas o caminho do conversor não chama nada daqui. Cada função aborta se for chamada. */
#include <stdio.h>
#include <stdlib.h>
#define NUNCA(n) void n(void) { fprintf(stderr, "stub de teste chamado: %s\n", #n); abort(); }
NUNCA(ar_asbd) NUNCA(ar_bytes) NUNCA(ar_duration) NUNCA(ar_type) NUNCA(_Block_copy) NUNCA(_Block_release)
NUNCA(cx_audio_frames) NUNCA(cx_audio_read_stereo) NUNCA(cx_cstr) NUNCA(cx_is_str) NUNCA(cx_str)
NUNCA(cx_tap_add_ioproc) NUNCA(cx_tap_owns) NUNCA(cx_tap_push) NUNCA(cx_tap_remove_ioproc) NUNCA(decode_close)
NUNCA(decode_open_path) NUNCA(dispatch_async_f) NUNCA(dispatch_get_main_queue) NUNCA(NSBeep) NUNCA(objc_msgSend)
NUNCA(objc_retain) NUNCA(sel_registerName)
int mach_timebase_info(unsigned *tb) { tb[0] = tb[1] = 1; return 0; }
unsigned long long mach_absolute_time(void) { return 0; }
