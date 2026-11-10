#define _POSIX_C_SOURCE 200809L
/* gcc -std=c11 -Wall -Wextra -O1 tools/smlogintest.c -o /tmp/smlogintest && /tmp/smlogintest */
#define SM_HOST 1
#include "../userland/libsystem/smlogin_core.inc"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void) {
    char dir[] = "/tmp/mos-sm-test-XXXXXX";
    if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
    const char *xml = "<plist><dict><key>Label</key><string>com.mos.helper</string><key>RunAtLoad</key><true/></dict></plist>";
    char label[64];
    CHECK(sm_label_xml(xml, strlen(xml), label, sizeof label) == 0);
    CHECK(strcmp(label, "com.mos.helper") == 0);
    const char *bad = "<plist><key>Label</key><string>../x</string></plist>";
    char bad_label[64];
    CHECK(sm_label_xml(bad, strlen(bad), bad_label, sizeof bad_label) != 0);
    CHECK(sm_copy_plist(dir, label, xml, strlen(xml)) == 0);
    CHECK(sm_plist_present(dir, "com.mos.helper") == 1);
    char path[512];
    snprintf(path, sizeof path, "%s/com.mos.helper.plist", dir);
    FILE *f = fopen(path, "rb");
    CHECK(f != 0);
    char got[256] = {0};
    if (f) { size_t n = fread(got, 1, sizeof got - 1, f); got[n] = 0; fclose(f); }
    CHECK(strstr(got, "com.mos.helper") != 0);
    CHECK(sm_remove_plist(dir, "com.mos.helper") == 0);
    CHECK(sm_plist_present(dir, "com.mos.helper") == 0);
    char st[512];
    snprintf(st, sizeof st, "%s/status.txt", dir);
    CHECK(sm_status_set(st, "agent:com.mos.helper", 1) == 0);
    CHECK(sm_status_get(st, "agent:com.mos.helper") == 1);
    CHECK(sm_status_set(st, "main:", 1) == 0);
    CHECK(sm_status_get(st, "agent:com.mos.helper") == 1);
    CHECK(sm_status_set(st, "agent:com.mos.helper", 0) == 0);
    CHECK(sm_status_get(st, "agent:com.mos.helper") == 0);
    CHECK(sm_status_get(st, "nao.existe") == -1);
    char cmd[640];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
    if (system(cmd) != 0) fprintf(stderr, "aviso: não apaguei %s\n", dir);
    if (fails) { fprintf(stderr, "smlogintest: %d falhas em %d\n", fails, checks); return 1; }
    printf("smlogintest: %d/%d PASS\n", checks, checks);
    return 0;
}
