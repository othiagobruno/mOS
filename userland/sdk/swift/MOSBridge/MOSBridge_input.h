/* mOS SDK — entrada de eventos do canvas para o OpenSwiftUI (implementada em userland/libsystem/swiftbridge_input.c).
 * Complementa mos_canvas_input (só clique e um caractere): entrega mouse down/up/arrasto, botão direito,
 * rolagem e teclado (código de tecla, modificadores e texto UTF-8) como um registro único. */
#pragma once

enum {
    MOS_INPUT_MOUSE_DOWN = 1,
    MOS_INPUT_MOUSE_UP = 2,
    MOS_INPUT_MOUSE_DRAGGED = 3,
    MOS_INPUT_RIGHT_MOUSE_DOWN = 4,
    MOS_INPUT_SCROLL = 5,
    MOS_INPUT_KEY_DOWN = 6,
};

/* modificadores (mesmos bits do NSEventModifierFlags do AppKit do mOS) */
enum {
    MOS_INPUT_MOD_CAPS = 1 << 16,
    MOS_INPUT_MOD_SHIFT = 1 << 17,
    MOS_INPUT_MOD_CONTROL = 1 << 18,
    MOS_INPUT_MOD_OPTION = 1 << 19,
    MOS_INPUT_MOD_COMMAND = 1 << 20,
};

typedef struct mos_input_event {
    int kind;                 /* MOS_INPUT_* */
    int button;               /* 0 = principal, 1 = secundário */
    int click_count;
    int key_code;             /* código de tecla estilo Mac (36 Return, 51 Delete, 123..126 setas...) */
    unsigned long modifiers;  /* MOS_INPUT_MOD_* */
    double x, y;              /* posição na view do canvas, origem no topo à esquerda */
    double delta_x, delta_y;  /* rolagem */
    const char *characters;   /* texto da tecla em UTF-8 (válido só durante a chamada) */
} mos_input_event_t;

typedef void (*mos_input_handler_t)(void *ctx, const mos_input_event_t *event);

/* liga todos os eventos de entrada do canvas a handler(ctx, ev) e torna o canvas o primeiro respondedor */
void mos_canvas_events(void *view, mos_input_handler_t handler, void *ctx);
/* devolve o foco de teclado ao canvas (ex.: depois de um clique fora) */
void mos_canvas_focus(void *view);
