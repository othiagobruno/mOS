/* mOS — registro da GPU AMD na gerência de energia (kernel/dev/pm.c, da Larissa). As regras do que acontece em
 * cada estado estão em engine.c (amdgpu_suspend/amdgpu_resume). */
#include "amdgpu.h"
#ifndef MOS_HOST_TEST
#include "pm.h"

static int pm_suspend(int state) { return amdgpu_suspend(amdgpu_main(), state); }
static void pm_resume(int state) { amdgpu_resume(amdgpu_main(), state); }

/* Com o VBIOS (tabelas ATOM da troca de modo), a volta do S3 religa as telas (amdgpu_display_restore): o driver
 * declara PM_RESTORES_DISPLAY e o pm.c pode usar o S3. Sem VBIOS, só o s2idle, onde a saída só perde o sincronismo. */
static const pm_ops_t amd_pm_restores = { "GPU AMD", PM_RESTORES_DISPLAY, pm_suspend, pm_resume, NULL };
static const pm_ops_t amd_pm = { "GPU AMD", 0, pm_suspend, pm_resume, NULL };

void amdgpu_pm_register(amdgpu_t *a) {
    static bool done;
    if (!a || done) return;
    done = true;
    pm_register(a->vbios_ok ? &amd_pm_restores : &amd_pm);
}
#endif
