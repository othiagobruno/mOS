#define _POSIX_C_SOURCE 200809L
/* Teste de host do ServiceManagement do mOS (userland/libsystem/smjob.c):
 *   gcc -std=c11 -Wall -Wextra -O1 -Ikernel/include tools/smjobtest.c -o /tmp/smjobtest && /tmp/smjobtest
 * O launchd (userland/init.c) responde "launchctl list RÓTULO" com lj_fmt_list_one (kernel/include/launchjob.inc) e
 * o cliente lê com smj_parse_one (smjob_core.inc). Aqui os dois lados se encontram sem o Mach no meio. */
#define SM_HOST 1
#include "../userland/libsystem/smlogin_core.inc"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

typedef long long i64;
typedef unsigned long long u64;
#include "launchjob.inc"
#include "../userland/libsystem/smjob_core.inc"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void) {
    char txt[4096];
    smj_info_t in;

    /* ida e volta: o que o launchd escreve é o que o SMJobCopyDictionary lê (installer do Sparkle no Setapp) */
    const char *svc[] = { "org.sparkle-project.InstallerLauncher" };
    int n = lj_fmt_list_one(txt, sizeof txt, "com.setapp.DesktopClient.SetappUpdater-sparkle-updater",
                            "/Users/Visitante/Library/Caches/Setapp/Installer.xpc/Contents/MacOS/Installer", 4242, 0, svc, 1);
    CHECK(n > 0);
    CHECK(smj_parse_one(txt, &in) == 0);
    CHECK(!strcmp(in.label, "com.setapp.DesktopClient.SetappUpdater-sparkle-updater"));
    CHECK(in.pid == 4242 && in.last_status == 0);
    CHECK(!strcmp(in.program, "/Users/Visitante/Library/Caches/Setapp/Installer.xpc/Contents/MacOS/Installer"));
    n = lj_fmt_list_one(txt, sizeof txt, "parado", "", 0, -11, 0, 0);
    CHECK(n > 0 && smj_parse_one(txt, &in) == 0 && in.pid == 0 && in.last_status == -11 && in.program[0] == 0);
    CHECK(smj_parse_one("Could not find service \"x\" in domain for port", &in) == -1);
    CHECK(smj_parse_one("", &in) == -1);

    /* tabela de LC_LIST (SMCopyAllJobDictionaries) */
    const char *tab = "PID\tStatus\tLabel\n812\t0\tcom.setapp.DesktopClient.SetappAgent\n-\t-15\tcom.setapp.DesktopClient.SetappUpdater\n"
                      "linha quebrada\n77\t1\tcom.mos.relogiod";
    const char *cur = tab;
    CHECK(smj_parse_row(&cur, &in) == 1 && in.pid == 812 && in.last_status == 0 && !strcmp(in.label, "com.setapp.DesktopClient.SetappAgent"));
    CHECK(smj_parse_row(&cur, &in) == 1 && in.pid == 0 && in.last_status == -15 && !strcmp(in.label, "com.setapp.DesktopClient.SetappUpdater"));
    CHECK(smj_parse_row(&cur, &in) == 1 && in.pid == 77 && in.last_status == 1 && !strcmp(in.label, "com.mos.relogiod"));
    CHECK(smj_parse_row(&cur, &in) == 0);
    cur = "PID\tStatus\tLabel\n";
    CHECK(smj_parse_row(&cur, &in) == 0);

    /* errno no CFError de kSMErrorDomainLaunchd */
    CHECK(smj_errno_of(SMJ_LAUNCHD_EXISTS) == 17);
    CHECK(smj_errno_of(SMJ_LAUNCHD_NOTFOUND) == 3);
    CHECK(smj_errno_of(-1) == 60);
    CHECK(smj_errno_of(22) == 22);

    /* LoginItems do bundle principal (SMLoginItemSetEnabled) */
    char dir[1100];
    CHECK(smj_loginitems_dir("/Aplicativos/Setapp.app", dir, sizeof dir) == 0 && !strcmp(dir, "/Aplicativos/Setapp.app/Contents/Library/LoginItems"));
    CHECK(smj_loginitems_dir("/Aplicativos/Setapp.app/", dir, sizeof dir) == 0 && !strcmp(dir, "/Aplicativos/Setapp.app/Contents/Library/LoginItems"));
    CHECK(smj_loginitems_dir("relativo.app", dir, sizeof dir) == -1);
    CHECK(smj_loginitems_dir("/Aplicativos/Setapp.app", dir, 20) == -1);

    /* arquivo do job (cx_sm_job_write usa sm_copy_plist): fica fora de LaunchAgents e sai no SMJobRemove */
    char tmp[] = "/tmp/mos-smjob-XXXXXX";
    if (!mkdtemp(tmp)) { perror("mkdtemp"); return 1; }
    char jd[600], path[800];
    snprintf(jd, sizeof jd, "%s/Library/Application Support/com.mos.smjob", tmp);
    const char *xml = "<plist><dict><key>Label</key><string>org.sparkle-project.InstallerLauncher</string></dict></plist>";
    CHECK(sm_copy_plist(jd, "org.sparkle-project.InstallerLauncher", xml, strlen(xml)) == 0);
    snprintf(path, sizeof path, "%s/org.sparkle-project.InstallerLauncher.plist", jd);
    CHECK(access(path, F_OK) == 0);
    CHECK(sm_copy_plist(jd, "../fora", xml, strlen(xml)) == -1);
    CHECK(sm_remove_plist(jd, "org.sparkle-project.InstallerLauncher") == 0);
    CHECK(access(path, F_OK) != 0);
    rmdir(jd);
    char up[700];
    snprintf(up, sizeof up, "%s/Library/Application Support", tmp); rmdir(up);
    snprintf(up, sizeof up, "%s/Library", tmp); rmdir(up);
    rmdir(tmp);

    printf("smjobtest: %d checagens, %d falhas\n", checks, fails);
    return fails ? 1 : 0;
}
