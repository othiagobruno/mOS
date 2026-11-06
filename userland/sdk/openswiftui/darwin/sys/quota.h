/*
 * mOS SDK — <sys/quota.h> do macOS (o zig não traz): cotas de disco, que o FileManager consulta para o espaço livre do
 * volume. Declarações com o ABI do macOS; quotactl fica no libSystem do mOS.
 */
#ifndef _SYS_QUOTA_H_
#define _SYS_QUOTA_H_

#include <sys/types.h>
#include <stdint.h>

#define USRQUOTA 0
#define GRPQUOTA 1
#define MAXQUOTAS 2

#define SUBCMDMASK 0x00ff
#define SUBCMDSHIFT 8
#define QCMD(cmd, type) (((cmd) << SUBCMDSHIFT) | ((type) & SUBCMDMASK))

#define Q_QUOTAON   0x0100
#define Q_QUOTAOFF  0x0200
#define Q_GETQUOTA  0x0300
#define Q_SETQUOTA  0x0400
#define Q_SETUSE    0x0500
#define Q_SYNC      0x0600
#define Q_QUOTASTAT 0x0700

struct dqblk {
    u_int64_t dqb_bhardlimit;
    u_int64_t dqb_bsoftlimit;
    u_int64_t dqb_curbytes;
    u_int32_t dqb_ihardlimit;
    u_int32_t dqb_isoftlimit;
    u_int32_t dqb_curinodes;
    u_int32_t dqb_btime;
    u_int32_t dqb_itime;
    u_int32_t dqb_id;
    u_int32_t dqb_spare[4];
};

#ifdef __cplusplus
extern "C" {
#endif
int quotactl(const char *path, int cmd, int id, char *addr);
#ifdef __cplusplus
}
#endif

#endif
