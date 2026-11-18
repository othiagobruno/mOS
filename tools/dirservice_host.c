/* Teste no host de userland/libsystem/dirservice.c (rodar por tools/dirservice-hosttest.sh): o fluxo de autenticação do VirtualBox. */
#include "sys.h"
#include <assert.h>
static mos_account_t acc[2] = {{"thiago", "Thiago Bruno", 1, 1}, {"visita", "Visitante", 0, 1}};
long fake_sys(long n, long a, long b, long c) {
    (void)n;
    if (a == MOS_ACCT_COUNT) return 2;
    if (a == MOS_ACCT_GET) { if (b > 1) return -1; *(mos_account_t *)c = acc[b]; return 0; }
    if (a == MOS_ACCT_CURRENT) { *(mos_account_t *)c = acc[0]; return 0; }
    if (a == MOS_ACCT_VERIFY) return !strcmp((char *)b, "thiago") && !strcmp((char *)c, "segredo");
    return -1;
}
unsigned getuid(void) { return 501; }
void CC_MD5(const void *d, u32 n, u8 *md) { memset(md, 0, 16); for (u32 i = 0; i < n; i++) md[i % 16] ^= ((const u8 *)d)[i]; }
typedef struct { u32 fBufferSize, fBufferLength; char fBufferData[1]; } B;
typedef struct { u32 n; B *h; } L;
typedef struct { u32 r; u32 acl[5]; u32 cnt; B nt; } RE;
typedef struct { u32 r; u32 acl[5]; u32 cnt, size, max; B sig; } AE;
typedef struct { u32 id; B data; } VE;
int dsOpenDirService(u32 *); B *dsDataBufferAllocate(u32, u32); int dsFindDirNodes(u32, B *, L *, u32, u32 *, u32 *);
int dsGetDirNodeName(u32, B *, u32, L **); int dsOpenDirNode(u32, L *, u32 *); L *dsBuildListFromStrings(u32, const char *, ...);
int dsGetRecordList(u32, B *, L *, u32, L *, L *, int, u32 *, u32 *); int dsGetRecordEntry(u32, B *, u32, u32 *, RE **);
int dsGetAttributeEntry(u32, B *, u32, u32, u32 *, AE **); int dsGetAttributeValue(u32, B *, u32, u32, VE **);
L *dsBuildFromPath(u32, const char *, const char *); int dsDoDirNodeAuth(u32, B *, int, B *, B *, u32 *);
B *dsDataNodeAllocateString(u32, const char *); int dsCloseAttributeList(u32); int dsCloseAttributeValueList(u32);
int dsDeallocRecordEntry(u32, RE *); int dsDeallocAttributeEntry(u32, AE *); int dsDeallocAttributeValueEntry(u32, VE *);
int dsDataListDeallocate(u32, L *); int dsCloseDirNode(u32); int dsCloseDirService(u32); int dsDataBufferDeAllocate(u32, B *);
char *dsGetPathFromList(u32, const L *, const char *);
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %d: %s\n", __LINE__, #c); fails++; } } while (0)
static int auth(u32 node, const char *u, const char *p) {
    B *m = dsDataNodeAllocateString(1, "dsAuthMethodStandard:dsAuthNodeNativeCanUseClearText"), *in = dsDataBufferAllocate(1, 256), *out = dsDataBufferAllocate(1, 256);
    u32 nl = strlen(u), pl = strlen(p);
    memcpy(in->fBufferData, &nl, 4); memcpy(in->fBufferData + 4, u, nl); memcpy(in->fBufferData + 4 + nl, &pl, 4); memcpy(in->fBufferData + 8 + nl, p, pl);
    in->fBufferLength = 8 + nl + pl;
    u32 cont = 0;
    return dsDoDirNodeAuth(node, m, 1, in, out, &cont);
}
int main(void) {
    u32 dir, cnt = 0, cont = 0, node, al, vl;
    CHECK(dsOpenDirService(&dir) == 0);
    B *buf = dsDataBufferAllocate(dir, 4096);
    CHECK(dsFindDirNodes(dir, buf, 0, 0x2201, &cnt, &cont) == 0 && cnt == 1);
    L *nn;
    CHECK(dsGetDirNodeName(dir, buf, 1, &nn) == 0);
    char *pth = dsGetPathFromList(dir, nn, "/");
    CHECK(!strcmp(pth, "/Search"));
    CHECK(dsOpenDirNode(dir, nn, &node) == 0);
    L *rn = dsBuildListFromStrings(dir, "Thiago Bruno", NULL), *rt = dsBuildListFromStrings(dir, "dsRecTypeStandard:Users", NULL),
      *at = dsBuildListFromStrings(dir, "dsAttrTypeStandard:AppleMetaNodeLocation", "dsAttrTypeStandard:RecordName", "dsAttrTypeStandard:NFSHomeDirectory", NULL);
    cnt = 1;
    CHECK(dsGetRecordList(node, buf, rn, 0x2001, rt, at, 0, &cnt, &cont) == 0 && cnt == 1);
    RE *re;
    CHECK(dsGetRecordEntry(node, buf, 1, &al, &re) == 0 && re->cnt == 3);
    u16 l; memcpy(&l, re->nt.fBufferData, 2);
    CHECK(l == 6 && !memcmp(re->nt.fBufferData + 2, "thiago", 6));
    char meta[128] = "", home[128] = ""; int names = 0;
    for (u32 i = 1; i <= re->cnt; i++) {
        AE *ae; VE *ve;
        CHECK(dsGetAttributeEntry(node, buf, al, i, &vl, &ae) == 0);
        CHECK(dsGetAttributeValue(node, buf, 1, vl, &ve) == 0);
        if (!strcmp(ae->sig.fBufferData, "dsAttrTypeStandard:AppleMetaNodeLocation")) strcpy(meta, ve->data.fBufferData);
        if (!strcmp(ae->sig.fBufferData, "dsAttrTypeStandard:NFSHomeDirectory")) strcpy(home, ve->data.fBufferData);
        if (!strcmp(ae->sig.fBufferData, "dsAttrTypeStandard:RecordName")) names = ae->cnt;
        dsDeallocAttributeValueEntry(dir, ve); dsDeallocAttributeEntry(dir, ae); dsCloseAttributeValueList(vl);
    }
    CHECK(!strcmp(meta, "/Local/Default") && names == 2 && home[0] == '/');
    dsCloseAttributeList(al); dsDeallocRecordEntry(dir, re);
    L *mp = dsBuildFromPath(dir, meta, "/");
    u32 unode;
    CHECK(mp->n == 2 && dsOpenDirNode(dir, mp, &unode) == 0);
    CHECK(auth(unode, "thiago", "segredo") == 0);
    CHECK(auth(unode, "Thiago Bruno", "segredo") == 0);
    CHECK(auth(unode, "thiago", "errada") == -14090);
    CHECK(auth(unode, "ninguem", "x") == -14098);
    /* grupos e "começa com" sem caixa */
    L *gn = dsBuildListFromStrings(dir, "ADM", NULL), *gt = dsBuildListFromStrings(dir, "dsRecTypeStandard:Groups", NULL), *ga = dsBuildListFromStrings(dir, "dsAttributesAll", NULL);
    cnt = 0;
    CHECK(dsGetRecordList(node, buf, gn, 0x2102, gt, ga, 0, &cnt, &cont) == 0 && cnt == 1);
    cnt = 0;
    L *all = dsBuildListFromStrings(dir, "dsRecordsAll", NULL);
    CHECK(dsGetRecordList(node, buf, all, 0x2001, rt, ga, 1, &cnt, &cont) == 0 && cnt == 2);
    cnt = 0;
    L *none = dsBuildListFromStrings(dir, "zzz", NULL);
    CHECK(dsGetRecordList(node, buf, none, 0x2001, rt, ga, 0, &cnt, &cont) == 0 && cnt == 0);
    CHECK(dsGetRecordEntry(node, buf, 1, &al, &re) == -14079);
    CHECK(dsCloseDirNode(unode) == 0 && dsCloseDirNode(unode) == -14074 && dsCloseDirNode(node) == 0);
    CHECK(dsDataBufferDeAllocate(dir, buf) == 0 && dsCloseDirService(dir) == 0);
    puts(fails ? "dirservice: FALHOU" : "dirservice: ok");
    return fails != 0;
}
