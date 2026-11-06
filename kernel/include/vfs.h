/* mOS — sistema de arquivos virtual em memória (populado a partir do pendrive no boot) */
#pragma once
#include "kernel.h"

typedef struct vnode {
    char name[256];
    bool dir;
    u8 *data;
    usize size, cap;
    u64 mtime;          /* ms desde o boot (ou 0 = arquivo do pendrive) */
    struct vnode *parent, *child, *next;
    void *ext;          /* nó de um sistema de arquivos externo (ex.: volume .dmg), carregado sob demanda */
    bool dirty;         /* alterado e ainda não gravado no disco do sistema (fs/persist.c) */
} vnode_t;

/* persistência no disco do sistema (kernel/fs/persist.c; padrão = sem disco, só RAM) */
bool persist_load(void);                  /* carrega a árvore do disco; false = usar arquivos do boot */
void persist_enable(void);
void persist_mark(vnode_t *n);            /* pasta criada (grava já) ou arquivo alterado (grava depois) */
void persist_delete(vnode_t *n);          /* antes de remover n */
void persist_move(const char *old_path, vnode_t *n); /* depois de mover/renomear */
void persist_sync(void);                  /* grava tudo que estiver pendente */
void persist_tick(void);                  /* chamado no loop principal */
bool persist_available(void);

/* ganchos para sistemas de arquivos externos (implementados em kernel/fs/vfs_bridge.c; padrão = no-op) */
bool vfs_ensure(vnode_t *n);   /* materializa filhos/conteúdo de um nó com ext */
void dfs_mount_all(void);      /* monta imagens .dmg/.img encontradas em /Volumes */
bool vfs_is_volume(vnode_t *n); /* n é a raiz de um volume montado em /Volumes */
bool vfs_eject(vnode_t *vol);   /* desmonta e libera o volume (n deixa de ser válido) */
int  vfs_mount_file(vnode_t *f); /* monta uma imagem .dmg/.img; devolve nº de volumes criados (0 = falha/já montada) */
bool vfs_is_read_only(vnode_t *n); /* dentro de um volume externo (somente leitura) */

vnode_t *vfs_root(void);
vnode_t *vfs_lookup(const char *path);
vnode_t *vfs_mkdir(const char *path);              /* cria toda a cadeia */
vnode_t *vfs_create(const char *path);             /* arquivo vazio (sobrescreve) */
vnode_t *vfs_write_file(const char *path, const void *data, usize n);
bool     vfs_append(vnode_t *f, const void *data, usize n);
bool     vfs_delete(vnode_t *n);
bool     vfs_move(vnode_t *n, vnode_t *newparent, const char *newname);
void     vfs_path(vnode_t *n, char *buf, usize cap);
int      vfs_count(vnode_t *dir);
vnode_t *vfs_child(vnode_t *dir, const char *name);
int      vfs_search(const char *q, const char **out, int max);
usize    vfs_total_bytes(void);
const char *vfs_kind(vnode_t *n);
const char *vfs_ext(const char *name);

#define HOME "/Users/Visitante"
#define TRASH HOME "/.Lixeira"

/* pasta pessoal da conta atual (account_home()) + sufixo; buffers rotativos, não guardar o ponteiro */
const char *home_sub(const char *sub);
#define CUR_HOME  home_sub("")
#define CUR_TRASH home_sub("/.Lixeira")
