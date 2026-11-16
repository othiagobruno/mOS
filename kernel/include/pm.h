/* mOS — gerência de energia (dev/pm.c): repouso por S3 ou s2idle e o registro dos drivers. */
#pragma once
#include "types.h"

#define PM_RESTORES_DISPLAY 1u     /* o driver de vídeo reprograma a saída depois do S3 */
#define PM_RESTORES_NET     2u     /* o driver de rede (Ethernet/Wi-Fi) reprograma a placa depois do S3 */

typedef struct {
    const char *name;
    u32 flags;
    /* state: 3 = S3 (o dispositivo perde energia), 0 = s2idle (fica ligado, só quieto). 0 = ok; outro = recusa */
    int  (*suspend)(int state);
    void (*resume)(int state);
    const char *(*can_s3)(void);   /* NULL = pode; texto = por que não (ex.: "pendrive montado") */
} pm_ops_t;

/* o escalonador tira os APs de linha antes do S3 e os religa depois (dev/pm.c chama pelo BSP) */
typedef struct {
    bool (*offline)(void);         /* threads migradas, APs parados em cli/hlt; false = não deu */
    void (*online)(void);          /* INIT-SIPI de novo e APs de volta ao escalonador */
} pm_cpu_hotplug_t;

void pm_register(const pm_ops_t *ops);
void pm_register_cpu_hotplug(const pm_cpu_hotplug_t *h);
void pm_wake_request(void);        /* sai do s2idle (botão de energia, tecla de repouso) */
const char *pm_s3_blocker(void);   /* NULL = S3 disponível */
const char *pm_status(void);
