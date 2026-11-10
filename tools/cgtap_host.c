/* Tap local engole o evento. Outro processo recebe o pacote do CGEventPost.
 * gcc -std=c11 -Wall -Wextra -pthread userland/libsystem/cgtap.c tools/cgtap_host.c -o /tmp/cgtap-host && /tmp/cgtap-host
 */
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

void *cx_cgtap_create(unsigned long long mask, void *cb, void *info);
void cx_cgtap_enable(void *tap, int on);
void *cx_cgtap_offer(unsigned type, void *event);
void cx_cgtap_fanout(unsigned type, int x, int y, int key, int button, int down, int wheel, unsigned flags);
void cx_cgtap_set_wire(void (*fn)(unsigned, int, int, int, int, int, int, unsigned));

static int *hits;
static int swallow;

static void *on_local(void *proxy, unsigned type, void *event, void *info) {
    (void)proxy; (void)type; (void)info;
    (*hits)++;
    return swallow ? 0 : event;
}
static void on_wire(unsigned type, int x, int y, int key, int button, int down, int wheel, unsigned flags) {
    (void)key; (void)button; (void)down; (void)wheel; (void)flags;
    if (type == 1 && x == 12 && y == 34) (*hits)++;
}

int main(void) {
    int ready[2];
    pid_t child;
    void *tap;
    char marker = 1;
    unlink("/tmp/mos-cgevent/pids");
    hits = mmap(0, sizeof *hits, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (hits == MAP_FAILED) return 1;
    *hits = 0;
    tap = cx_cgtap_create(1ull << 1, on_local, 0);
    if (!tap) return 2;
    if (cx_cgtap_offer(1, marker ? &marker : 0) != &marker || *hits != 1) return 3;
    swallow = 1;
    if (cx_cgtap_offer(1, &marker) != 0) return 4;
    cx_cgtap_enable(tap, 0);
    if (pipe(ready) != 0) return 5;
    child = fork();
    if (child < 0) return 6;
    if (child == 0) {
        close(ready[0]);
        *hits = 0;
        cx_cgtap_set_wire(on_wire);
        if (!cx_cgtap_create(1ull << 1, on_local, 0)) _exit(7);
        if (write(ready[1], "1", 1) != 1) _exit(8);
        for (;;) pause();
    }
    close(ready[1]);
    if (read(ready[0], &marker, 1) != 1) return 9;
    cx_cgtap_fanout(1, 12, 34, 0, 0, 1, 0, 0);
    for (int i = 0; i < 1000 && *hits < 1; i++) usleep(1000);
    kill(child, 9);
    waitpid(child, 0, 0);
    if (*hits < 1) return 10;
    printf("cgtap host ok\n");
    return 0;
}
