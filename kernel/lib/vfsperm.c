/*
 * mOS — permissões do VFS: bits POSIX (dono/grupo/outros), root, sticky e ACL no formato do xnu.
 *
 * A ACL fica no xattr com.apple.system.Security como uma kauth_filesec em big-endian (o mesmo formato que o
 * HFS+ guarda no disco, sys/kauth.h):
 *   u32 fsec_magic (0x012cc16d) | guid dono | guid grupo | u32 nº de ACEs (0xffffffff = sem ACL) | u32 flags |
 *   ACE[] { guid aplicável | u32 flags (1 permitir, 2 negar, herança) | u32 direitos (KAUTH_VNODE_*) }
 * Avaliação (kauth_acl_evaluate do xnu): as ACEs na ordem; uma negação que pega um direito pedido nega tudo;
 * uma permissão tira direitos da lista; o que sobrar vai para os bits POSIX. Root passa (execução de arquivo
 * ainda pede algum bit x). ACEs "só herança" não valem no próprio nó.
 * GUIDs: usuário FFFFEEEE-DDDD-CCCC-BBBB-AAAA<uid em 8 hex>, grupo ABCDEFAB-CDEF-ABCD-EFAB-CDEF<gid>, everyone =
 * grupo 12 (os mesmos que o mbr_uid_to_uuid do macOS dá para contas sem diretório).
 */
#include "vfs.h"
#include "syscall.h"
#include "cred.h"

#define FSEC_MAGIC 0x012cc16du
#define ACE_SIZE 24
#define FSEC_HDR 44

static u32 g32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static void s32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }

static const u8 user_pfx[12] = {0xFF, 0xFF, 0xEE, 0xEE, 0xDD, 0xDD, 0xCC, 0xCC, 0xBB, 0xBB, 0xAA, 0xAA};
static const u8 group_pfx[12] = {0xAB, 0xCD, 0xEF, 0xAB, 0xCD, 0xEF, 0xAB, 0xCD, 0xEF, 0xAB, 0xCD, 0xEF};

void vfs_guid_user(u8 g[16], u32 uid) { memcpy(g, user_pfx, 12); s32(g + 12, uid); }
void vfs_guid_group(u8 g[16], u32 gid) { memcpy(g, group_pfx, 12); s32(g + 12, gid); }
/* 1 = usuário, 2 = grupo, 0 = desconhecido */
int vfs_guid_id(const u8 g[16], u32 *id) {
    if (!memcmp(g, user_pfx, 12)) { *id = g32(g + 12); return 1; }
    if (!memcmp(g, group_pfx, 12)) { *id = g32(g + 12); return 2; }
    return 0;
}

/* grupos da conta do mOS: os de uma conta administradora do macOS (o "id" da 1ª conta): staff 20, everyone 12,
 * localaccounts 61, _appserverusr 79, admin 80, _appserveradm 81, _lpadmin 98, _appstore 33, _lpoperator 100,
 * _developer 204, _analyticsusers 250, com.apple.access_ftp 395, _screensharing 398, _ssh 399, _remote_ae 400,
 * com.apple.sharepoint.group.1 701. Root está em wheel 0 e em todos. */
static const u32 admin_groups[] = {20, 12, 61, 79, 80, 81, 98, 33, 100, 204, 250, 395, 398, 399, 400, 701};
static bool member(u32 gid, int euid, int egid) {
    if ((u32)egid == gid || gid == 12 || euid == 0) return true;
    if (euid == 501) for (u32 i = 0; i < ARRAY_LEN(admin_groups); i++) if (admin_groups[i] == gid) return true;
    return false;
}
bool vfs_in_group(u32 gid, int euid, int egid) { return member(gid, euid, egid); }

/* filesec válido: tamanho bate com o nº de ACEs */
bool vfs_filesec_valid(const u8 *d, u64 n) {
    if (n < FSEC_HDR || g32(d) != FSEC_MAGIC) return false;
    u32 cnt = g32(d + 36);
    if (cnt == 0xffffffffu) return n == FSEC_HDR;
    if (cnt > 128) return false;
    return n == FSEC_HDR + (u64)cnt * ACE_SIZE;
}

static bool ace_matches(const u8 *g, u32 uid_owner, int euid, int egid) {
    u32 id;
    int k = vfs_guid_id(g, &id);
    (void)uid_owner;
    if (k == 1) return (u32)euid == id;
    if (k == 2) return member(id, euid, egid);
    return false;
}

/* direitos pedidos (KAUTH_VNODE_*) contra a ACL: 1 = tudo concedido, 0 = sobra, -1 = negado. *left = o que sobra */
static int acl_eval(vnode_t *v, u32 want, int euid, int egid, u32 *left) {
    vxattr_t *x = vfs_xattr_find(v, VFS_XATTR_SECURITY);
    *left = want;
    if (!x || !vfs_filesec_valid(x->val, x->len)) return 0;
    u32 cnt = g32(x->val + 36);
    if (cnt == 0xffffffffu) return 0;
    for (u32 i = 0; i < cnt && *left; i++) {
        const u8 *a = x->val + FSEC_HDR + i * ACE_SIZE;
        u32 fl = g32(a + 16), rights = g32(a + 20);
        if (fl & VFS_ACE_ONLY_INHERIT) continue;
        if (!ace_matches(a, v->uid, euid, egid)) continue;
        u32 kind = fl & 0xf;
        if (kind == VFS_ACE_DENY && (rights & *left)) return -1;
        if (kind == VFS_ACE_PERMIT) *left &= ~rights;
    }
    return *left ? 0 : 1;
}

/* direitos -> bits rwx do POSIX */
static unsigned rights_to_rwx(u32 r, bool dir) {
    unsigned m = 0;
    if (r & (VFS_KR_READ_DATA | VFS_KR_READ_EXTATTR | VFS_KR_READ_SECURITY)) m |= 4;
    if (r & (VFS_KR_WRITE_DATA | VFS_KR_APPEND_DATA | VFS_KR_WRITE_EXTATTR | VFS_KR_DELETE_CHILD)) m |= 2;
    if (r & VFS_KR_EXECUTE) m |= 1;
    if (dir && (r & VFS_KR_DELETE_CHILD)) m |= 1;     /* apagar dentro pede busca também */
    (void)dir;
    return m;
}

int vfs_authorize(vnode_t *v, u32 rights, int euid, int egid) {
    if (!v || !rights) return 0;
    unsigned mode = mode_stat(v->mode, v->dir);
    if (euid == 0) {
        /* root: tudo, menos executar arquivo sem nenhum bit x */
        if ((rights & VFS_KR_EXECUTE) && !v->dir && !(mode & 0111)) return -MOS_EACCES;
        return 0;
    }
    /* metadados: dono decide (WRITE_SECURITY/ATTRIBUTES, TAKE_OWNERSHIP), a ACL pode conceder a outros */
    u32 left;
    int r = acl_eval(v, rights, euid, egid, &left);
    if (r < 0) return -MOS_EACCES;
    if (r > 0) return 0;
    bool owner = (u32)euid == v->uid;
    if (left & (VFS_KR_WRITE_SECURITY | VFS_KR_TAKE_OWNERSHIP | VFS_KR_WRITE_ATTRIBUTES)) {
        if (!owner) return -MOS_EPERM;
        left &= ~(VFS_KR_WRITE_SECURITY | VFS_KR_TAKE_OWNERSHIP | VFS_KR_WRITE_ATTRIBUTES);
    }
    left &= ~(VFS_KR_READ_ATTRIBUTES | VFS_KR_DELETE);  /* ler atributos: sempre; DELETE: decide a pasta-mãe */
    unsigned need = rights_to_rwx(left, v->dir);
    if (!need) return 0;
    unsigned have = owner ? (mode >> 6) & 7 : member(v->gid, euid, egid) ? (mode >> 3) & 7 : mode & 7;
    return (need & ~have) ? -MOS_EACCES : 0;
}

/* POSIX access(2)/open: R 4, W 2, X 1 */
int vfs_access(vnode_t *v, int rwx, int euid, int egid) {
    u32 r = 0;
    if (rwx & 4) r |= VFS_KR_READ_DATA;
    if (rwx & 2) r |= v && v->dir ? VFS_KR_WRITE_DATA | VFS_KR_APPEND_DATA : VFS_KR_WRITE_DATA;
    if (rwx & 1) r |= VFS_KR_EXECUTE;
    return vfs_authorize(v, r, euid, egid);
}

/* apagar ou tirar n de sua pasta (unlink, rmdir, rename): DELETE em n pela ACL, ou escrever+buscar na pasta;
 * com sticky na pasta só o dono do arquivo, o dono da pasta ou root */
int vfs_may_delete(vnode_t *n, int euid, int egid) {
    vnode_t *d = n ? n->parent : NULL;
    if (!d) return -MOS_EPERM;
    if (euid == 0) return 0;
    u32 left;
    int r = acl_eval(n, VFS_KR_DELETE, euid, egid, &left);
    if (r < 0) return -MOS_EACCES;
    int pr = vfs_authorize(d, VFS_KR_DELETE_CHILD | VFS_KR_EXECUTE, euid, egid);
    if (r == 0 && pr < 0) return pr;
    unsigned dm = mode_stat(d->mode, true);
    if ((dm & 01000) && (u32)euid != n->uid && (u32)euid != d->uid) return -MOS_EPERM;   /* sticky */
    return 0;
}

/* criar dentro de d */
int vfs_may_create(vnode_t *d, bool dir, int euid, int egid) {
    if (!d || !d->dir) return -MOS_ENOTDIR;
    return vfs_authorize(d, (dir ? VFS_KR_APPEND_DATA : VFS_KR_WRITE_DATA) | VFS_KR_EXECUTE, euid, egid);
}

/* herança (kauth_acl_inherit): o nó novo leva as ACEs da pasta marcadas para arquivo ou pasta */
void vfs_acl_inherit(vnode_t *n, vnode_t *dir) {
    vxattr_t *x = dir ? vfs_xattr_find(dir, VFS_XATTR_SECURITY) : NULL;
    if (!x || !vfs_filesec_valid(x->val, x->len)) return;
    u32 cnt = g32(x->val + 36);
    if (cnt == 0xffffffffu || !cnt) return;
    u8 *out = kmalloc(FSEC_HDR + (u64)cnt * ACE_SIZE);
    if (!out) return;
    memcpy(out, x->val, FSEC_HDR);
    vfs_guid_user(out + 4, n->uid);
    vfs_guid_group(out + 20, n->gid);
    u32 k = 0;
    for (u32 i = 0; i < cnt; i++) {
        const u8 *a = x->val + FSEC_HDR + i * ACE_SIZE;
        u32 fl = g32(a + 16);
        /* arquivo: as de FILE_INHERIT. Pasta: as de DIRECTORY_INHERIT valem nela; as só de FILE_INHERIT entram como
         * só-herança, para chegar aos arquivos de baixo (a não ser com LIMIT_INHERIT, que para no 1º nível) */
        bool take = n->dir ? (fl & VFS_ACE_DIRECTORY_INHERIT) ||
                             ((fl & VFS_ACE_FILE_INHERIT) && !(fl & VFS_ACE_LIMIT_INHERIT))
                           : (fl & VFS_ACE_FILE_INHERIT);
        if (!take) continue;
        u8 *b = out + FSEC_HDR + k * ACE_SIZE;
        memcpy(b, a, ACE_SIZE);
        u32 nf = (fl & 0xf) | VFS_ACE_INHERITED;
        /* pasta: continua herdando para baixo, a não ser com LIMIT_INHERIT */
        if (n->dir && !(fl & VFS_ACE_LIMIT_INHERIT)) nf |= fl & (VFS_ACE_FILE_INHERIT | VFS_ACE_DIRECTORY_INHERIT);
        if (n->dir && !(fl & VFS_ACE_DIRECTORY_INHERIT) && (fl & VFS_ACE_FILE_INHERIT)) nf |= VFS_ACE_ONLY_INHERIT;
        s32(b + 16, nf);
        k++;
    }
    if (k) {
        s32(out + 36, k);
        vxattr_t *old = vfs_xattr_find(n, VFS_XATTR_SECURITY);
        if (!old) {
            vxattr_t *nx = kcalloc(1, sizeof *nx);
            if (nx) {
                strcpy(nx->name, VFS_XATTR_SECURITY);
                nx->val = out;
                nx->len = FSEC_HDR + k * ACE_SIZE;
                nx->next = n->xattr;           /* "com.apple.system..." fica na ordem certa no próximo set; a lista */
                n->xattr = nx;                 /* não depende de ordem para achar */
                n->xattr_bytes += nx->len;
                return;
            }
        }
    }
    kfree(out);
}
