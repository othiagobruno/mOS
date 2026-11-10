/* Conta do selo do Dock. desktop.c desenha; o teste de host inclui este arquivo. */
#pragma once

/* label[8] recebe o texto desenhado (vazio = sem selo). *badge é o número
 * (0 = sem selo). Texto que não é número positivo vira selo com esse texto. */
static inline void dock_badge_apply(int *badge, char label[8], const char *text) {
    int i, n = 0, digit = 1;
    if (!badge || !label) return;
    if (!text) text = "";
    for (i = 0; text[i] && i < 7; i++) label[i] = text[i];
    label[i] = 0;
    if (!label[0]) { *badge = 0; return; }
    for (i = 0; label[i]; i++) {
        if (label[i] < '0' || label[i] > '9') { digit = 0; break; }
        n = n * 10 + (label[i] - '0');
        if (n > 999) n = 999;
    }
    if (!digit) { *badge = 1; return; }
    if (n <= 0) { label[0] = 0; *badge = 0; return; }
    *badge = n;
}
