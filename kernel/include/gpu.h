/* mOS — GPUs (Intel integrada e AMD): detecção e configuração atual (dev/gpu.c).
 *
 * Fase 1, somente leitura: identifica a GPU e a família, lê a memória reservada (stolen), o tamanho
 * da tabela de páginas da GPU (GTT) e, na Intel, o modo que o firmware deixou ligado (pipe, plano,
 * resolução, stride e o endereço da superfície), conferindo se é o framebuffer do UEFI que o mOS usa.
 * Nada é escrito nos registradores: é seguro rodar em qualquer máquina. As próximas fases (motor de
 * cópia/blitter, troca de quadro com vsync) partem destas informações. */
#pragma once
#include "kernel.h"

typedef struct {
    pci_dev_t  *pci;
    u16         vendor, device;
    const char *maker;          /* "Intel", "AMD" */
    const char *family;         /* codinome: "Tiger Lake", "Rembrandt"... */
    const char *arch;           /* "Gen12", "RDNA2"... */
    bool        guessed;        /* família estimada pelo prefixo do ID (não está na tabela) */
    int         gen;            /* Intel: geração (7..20); AMD: 0 */
    u64         mmio;           /* registradores (Intel BAR0, AMD BAR5) */
    u64         aperture;       /* Intel: abertura GMADR (BAR2); AMD: VRAM visível (BAR0) */
    u32         stolen_mb, gtt_mb;
    u64         stolen_base;
    bool        display;        /* é a GPU que mostra o framebuffer do UEFI */
    int         pipe;           /* Intel: pipe ativo com o framebuffer (-1 = não identificado) */
    u32         mode_w, mode_h, stride; /* modo do pipe ativo (stride em bytes) */
    bool        surf_match;     /* Intel: a superfície do plano é exatamente o framebuffer do UEFI */
    int         ext_plugged;    /* Intel: monitores externos com sinal de presença (hotplug) */
    int         ext_driven;     /* Intel: portas externas que o firmware deixou ligadas (espelho) */
    char        ports[96];      /* Intel: resumo das portas externas com monitor ("HDMI/DP B, USB-C 1") */
} gpu_info_t;

void              gpu_init(void);
int               gpu_count(void);
const gpu_info_t *gpu_get(int i);
const char       *gpu_summary(void);   /* para "Sobre este mOS" */
/* aviso sobre monitores externos (ex.: plugado mas sem driver para ligá-lo); "" se não houver nada a dizer.
 * O desktop mostra como notificação quando fica pronto e o Sobre, na aba Monitores. */
const char       *gpu_display_notice(void);

/* vsync (Intel): espera o retorno vertical antes da cópia para a tela. Não faz nada se não houver
 * GPU Intel mostrando a tela ou se a autoverificação do boot falhou. Chamado por fb_flush. */
void              gpu_vsync_wait(void);

/* apresentação pela GPU (virtio-gpu por enquanto): mostra o retângulo (x, y, rw, rh) da superfície
 * px (w x h, stride = w) sem cópia na CPU. false = sem GPU apta; quem chama copia para o framebuffer. */
bool              gpu_present(const u32 *px, int w, int h, int x, int y, int rw, int rh);
bool              virtio_gpu_probe(gpu_info_t *g);   /* dev/virtio_gpu.c */
void              gpu_displays_changed(void);        /* o arranjo dos monitores mudou: religar as saídas */
bool              gpu_vsync_active(void);
u32               gpu_refresh_hz(void);   /* 0 = desconhecida */
