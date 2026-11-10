/* Lista de janelas do compositor (DEV_WINDOW_LIST). O TCC de gravação de tela fica em quem chama. */
#pragma once
#include "sys.h"
#include "../../kernel/include/compat_gui.h"

/* 1 = preencheu esta posição (0 = a da frente). 0 = acabou a lista. */
int cx_cg_window_at(int index, gui_cgwindow_t *out);
