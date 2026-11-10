/* Teste de host do FSEventStream (userland/libsystem/fsevents_core.inc).
 *   gcc -std=c11 -Wall -Wextra -O1 -DFSE_HOST tools/fseventstest.c -o /tmp/fseventstest -pthread
 *   /tmp/fseventstest
 * Cobre criar, editar, apagar, renomear, chmod, exclusão e o modo sem FileEvents.
 */
#define _POSIX_C_SOURCE 200809L
#define FSE_HOST 1
#include "../userland/libsystem/fsevents_core.inc"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

#define MAXE 128
typedef struct { char path[FSE_PATH]; u32 flags; u64 id; } rec_t;
static rec_t recs[MAXE];
static int nrec, overflow;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;

#define FILEF (kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagWatchRoot)
#define CREATED (kFSEventStreamEventFlagItemCreated)
#define REMOVED (kFSEventStreamEventFlagItemRemoved)
#define RENAMED (kFSEventStreamEventFlagItemRenamed)
#define MODIFIED (kFSEventStreamEventFlagItemModified)
#define METAMOD (kFSEventStreamEventFlagItemInodeMetaMod)
#define ISFILE (kFSEventStreamEventFlagItemIsFile)
#define ISDIR (kFSEventStreamEventFlagItemIsDir)
#define ISLNK (kFSEventStreamEventFlagItemIsSymlink)
#define MUSTSCAN (kFSEventStreamEventFlagMustScanSubDirs)
#define HISTORY (kFSEventStreamEventFlagHistoryDone)

static void on_event(void *stream, void *info, size_t n, void *eventPaths, const u32 *eventFlags, const u64 *eventIds) {
    char **ps = eventPaths;
    size_t i;
    (void)stream;
    (void)info;
    pthread_mutex_lock(&mu);
    for (i = 0; i < n; i++) {
        if (nrec >= MAXE) { overflow = 1; break; }
        snprintf(recs[nrec].path, sizeof recs[nrec].path, "%s", ps[i] ? ps[i] : "");
        recs[nrec].flags = eventFlags[i];
        recs[nrec].id = eventIds[i];
        nrec++;
    }
    pthread_mutex_unlock(&mu);
}

static void pause_ms(unsigned ms) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {}
}

static void arm(void) {
    pthread_mutex_lock(&mu);
    nrec = 0;
    overflow = 0;
    pthread_mutex_unlock(&mu);
}

static void dump(void) {
    int i;
    pthread_mutex_lock(&mu);
    fprintf(stderr, "eventos (%d):\n", nrec);
    for (i = 0; i < nrec; i++) fprintf(stderr, "  %08x id=%llu %s\n", recs[i].flags, recs[i].id, recs[i].path);
    pthread_mutex_unlock(&mu);
}

static int match(const char *path, u32 flags, int exact) {
    int i, ok = 0;
    pthread_mutex_lock(&mu);
    for (i = 0; i < nrec; i++) {
        if (strcmp(recs[i].path, path) != 0) continue;
        if (exact ? recs[i].flags == flags : (recs[i].flags & flags) == flags) {
            if (recs[i].id == 0) ok = 0;
            else ok = 1;
            break;
        }
    }
    pthread_mutex_unlock(&mu);
    return ok;
}

static int absent(const char *path) {
    int i, ok = 1;
    pthread_mutex_lock(&mu);
    for (i = 0; i < nrec; i++) if (strcmp(recs[i].path, path) == 0) ok = 0;
    pthread_mutex_unlock(&mu);
    return ok;
}

static int wait_flags(void *s, const char *path, u32 flags, int exact) {
    int k;
    for (k = 0; k < 40; k++) {
        FSEventStreamFlushSync(s);
        if (match(path, flags, exact)) return 1;
        pause_ms(20);
    }
    dump();
    return 0;
}

static int write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    if (fputs(text, f) < 0) { fclose(f); return -1; }
    if (fclose(f) != 0) return -1;
    return 0;
}

static void *open_stream(const char *root, u32 flags, u64 since) {
    const char *paths[2];
    paths[0] = root;
    paths[1] = 0;
    return FSEventStreamCreate(0, on_event, 0, paths, since, 0.05, flags);
}

static void join2(char *dst, size_t n, const char *dir, const char *name) {
    snprintf(dst, n, "%s/%s", dir, name);
}

int main(void) {
    char dir[] = "/tmp/fseventsXXXXXX";
    char file[FSE_PATH], renamed[FSE_PATH], sub[FSE_PATH], nested[FSE_PATH];
    char skip[FSE_PATH], hidden[FSE_PATH], visible[FSE_PATH], linkp[FSE_PATH], stopped[FSE_PATH];
    void *s, *legacy, *hist;
    const char *ex[2];
    u32 file_created = CREATED | ISFILE;
    u32 file_modified = MODIFIED | ISFILE;
    u32 file_removed = REMOVED | ISFILE;
    u32 file_renamed = RENAMED | ISFILE;
    u32 file_meta = METAMOD | ISFILE;

    CHECK(FSEventStreamStart(0) == 0);
    if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
    join2(file, sizeof file, dir, "nota.txt");
    join2(renamed, sizeof renamed, dir, "nota-nova.txt");
    join2(sub, sizeof sub, dir, "sub");
    join2(nested, sizeof nested, sub, "dentro.txt");
    join2(skip, sizeof skip, dir, "pula");
    join2(hidden, sizeof hidden, skip, "escondido.txt");
    join2(visible, sizeof visible, dir, "visivel.txt");
    join2(linkp, sizeof linkp, dir, "atalho");
    join2(stopped, sizeof stopped, dir, "parado.txt");

    s = open_stream(dir, FILEF, kFSEventStreamEventIdSinceNow);
    CHECK(s != 0);
    CHECK(FSEventStreamStart(s) == 1);
    CHECK(FSEventStreamGetDeviceBeingWatched(s) == (int)FSE_DEV);
    CHECK(FSEventStreamStart(s) == 0); /* já iniciado */

    arm();
    CHECK(write_file(file, "a") == 0);
    CHECK(wait_flags(s, file, file_created, 1));

    arm();
    CHECK(write_file(file, "abcd") == 0);
    CHECK(wait_flags(s, file, file_modified, 1));

    arm();
    CHECK(chmod(file, 0600) == 0);
    CHECK(wait_flags(s, file, file_meta, 1));

    arm();
    CHECK(rename(file, renamed) == 0);
    CHECK(wait_flags(s, file, file_renamed, 1));
    CHECK(wait_flags(s, renamed, file_renamed, 1));

    arm();
    CHECK(unlink(renamed) == 0);
    CHECK(wait_flags(s, renamed, file_removed, 1));

    arm();
    CHECK(mkdir(sub, 0755) == 0);
    CHECK(wait_flags(s, sub, CREATED | ISDIR, 1));
    arm();
    CHECK(symlink("lugar-nenhum", linkp) == 0);
    CHECK(wait_flags(s, linkp, CREATED | ISLNK, 1));

    CHECK(mkdir(skip, 0755) == 0);
    ex[0] = skip;
    ex[1] = 0;
    CHECK(FSEventStreamSetExclusionPaths(s, ex) == 1);
    arm();
    CHECK(write_file(hidden, "x") == 0);
    CHECK(write_file(visible, "y") == 0);
    CHECK(wait_flags(s, visible, file_created, 1));
    CHECK(absent(hidden));

    FSEventStreamStop(s);
    pause_ms(50);
    arm();
    CHECK(write_file(stopped, "z") == 0);
    {
        int k, leaked = 0;
        for (k = 0; k < 5; k++) {
            FSEventStreamFlushSync(s);
            if (!absent(stopped)) leaked = 1;
            pause_ms(20);
        }
        CHECK(!leaked);
    }
    CHECK(FSEventStreamStart(s) == 1);
    CHECK(wait_flags(s, stopped, file_created, 1));
    CHECK(FSEventsGetCurrentEventId() != 0);
    CHECK(FSEventStreamGetLatestEventId(s) != 0);

    FSEventStreamInvalidate(s);
    CHECK(FSEventStreamStart(s) == 0);
    FSEventStreamRelease(s);
    s = 0;

    legacy = open_stream(dir, kFSEventStreamCreateFlagNoDefer, kFSEventStreamEventIdSinceNow);
    CHECK(legacy != 0);
    CHECK(FSEventStreamStart(legacy) == 1);
    arm();
    CHECK(write_file(nested, "n") == 0);
    CHECK(wait_flags(legacy, sub, MUSTSCAN | ISDIR, 0));
    CHECK(absent(nested));
    FSEventStreamStop(legacy);
    FSEventStreamInvalidate(legacy);
    FSEventStreamRelease(legacy);

    hist = open_stream(dir, FILEF, 0);
    CHECK(hist != 0);
    arm();
    CHECK(FSEventStreamStart(hist) == 1);
    CHECK(wait_flags(hist, dir, HISTORY | MUSTSCAN | ISDIR, 1));
    FSEventStreamStop(hist);
    FSEventStreamInvalidate(hist);
    FSEventStreamRelease(hist);

    CHECK(!overflow);
    cx_fse_shutdown();

    unlink(hidden);
    unlink(visible);
    unlink(stopped);
    unlink(nested);
    unlink(linkp);
    rmdir(skip);
    rmdir(sub);
    rmdir(dir);

    if (fails) fprintf(stderr, "%d de %d falharam\n", fails, checks);
    else printf("fseventstest: %d checagens ok\n", checks);
    return fails ? 1 : 0;
}
