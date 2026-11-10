/* Agenda local do mOS. O teste de host compila cn_store.c com -DCX_CN_HOST.
 * Sem rede e sem a agenda da Apple. Autorização: kTCCServiceAddressBook. */
#pragma once

enum {
    CX_CN_NOT_DETERMINED = 0,
    CX_CN_DENIED = 2,
    CX_CN_AUTHORIZED = 3
};

enum {
    CX_CN_OK = 0,
    CX_CN_ERR_DENIED = 1,
    CX_CN_ERR_BAD = 2,
    CX_CN_ERR_IO = 3,
    CX_CN_ERR_FULL = 4
};

typedef struct {
    char id[40];
    char given[64];
    char family[64];
    char org[80];
    char phone[64];
    char email[80];
    char street[96];
    char city[48];
    char state[32];
    char zip[16];
    char country[48];
    char iso[8];
} cx_cn_contact;

void cx_cn_reset(void);
void cx_cn_set_path(const char *path);
void cx_cn_set_auth(int status);
int cx_cn_auth(void);

void cx_cn_begin(void);
int cx_cn_stage_add(const cx_cn_contact *in, char id_out[40]);
int cx_cn_stage_update(const cx_cn_contact *in);
int cx_cn_stage_delete(const char *id);
int cx_cn_commit(void);

int cx_cn_count(void);
int cx_cn_get(int index, cx_cn_contact *out);

/* Linhas com os campos preenchidos. BR usa "cidade - UF". */
int cx_cn_format_postal(const cx_cn_contact *c, char *out, int cap);
