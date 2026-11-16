/* mOS — registro da GPU AMD na gerência de energia (kernel/dev/pm.c, da Larissa). As regras do que acontece em
 * cada estado estão em engine.c (amdgpu_suspend/amdgpu_resume). */
#include "amdgpu.h"
#ifndef MOS_HOST_TEST
#include "pm.h"

static int pm_suspend(int state) { return amdgpu_suspend(amdgpu_main(), state); }
static void pm_resume(int state) { amdgpu_resume(amdgpu_main(), state); }

/* Sem PM_RESTORES_DISPLAY: depois do S3 nem a GOP volta, e a saída só é religada com a troca de modo completa
 * (docs/gpu-amd.md, F4b). Até lá o pm.c fica no s2idle, onde a saída só perde o sincronismo e volta. */
static const pm_ops_t amd_pm = { "GPU AMD", 0, pm_suspend, pm_resume, NULL };

void amdgpu_pm_register(amdgpu_t *a) {
    static bool done;
    if (!a || done) return;
    done = true;
    pm_register(&amd_pm);
}
#endif
