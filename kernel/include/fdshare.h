#pragma once
/* vfork: o filho usa a tabela do pai. No exec a tabela é copiada para o filho. No _exit não fecha. */
void *fd_share_tab(void *parent_tab);
void fd_unshare_tab(void *own, void **tab, unsigned bytes);
int fd_tab_shared(const void *tab, const void *own);
