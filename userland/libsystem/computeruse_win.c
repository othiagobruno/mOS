/* mOS — CGWindowListCopyWindowInfo lê o window server (DEV_WINDOW_LIST). */
#include "computeruse_win.h"

int cx_cg_window_at(int index, gui_cgwindow_t *out) {
    if (!out || index < 0) return 0;
    long r = (long)SYS4(GUI_DEVICE, DEV_WINDOW_LIST, (u64)index, (u64)out, 0);
    return r == 1;
}
