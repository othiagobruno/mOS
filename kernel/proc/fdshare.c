#include "fdshare.h"
#ifdef MOS_HOST_TEST
#include <string.h>
#else
#include "kernel.h"
#endif

void *fd_share_tab(void *parent_tab) { return parent_tab; }

void fd_unshare_tab(void *own, void **tab, unsigned bytes) {
    if (!own || !tab || !*tab || *tab == own) { if (tab && own) *tab = own; return; }
    memcpy(own, *tab, bytes);
    *tab = own;
}

int fd_tab_shared(const void *tab, const void *own) { return tab && tab != own; }
