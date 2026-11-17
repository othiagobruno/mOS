#!/bin/sh
# VFS sob demanda no volume do sistema: fs/persist.c real sobre um FAT32 com \mOS\ (boot sem conteúdo, leitura aos
# pedaços, LRU, mudança/rename/mover/apagar); o volume final passa no verificador independente (volimg check)
set -e
cd "$(dirname "$0")/.."
D=$(mktemp -d)
# o persist.c inclui sysstate.h/update.h/crypto.h, que puxam o kernel.h de verdade: no host vão versões mínimas,
# achadas antes por estarem na pasta da cópia do persist.c
cp kernel/fs/persist.c "$D/persist.c"
cat > "$D/sysstate.h" <<'H'
#pragma once
#include "vfs_host_kernel.h"
enum { SYS_EPHEMERAL, SYS_LIVE_PERSIST, SYS_FIRSTBOOT, SYS_INSTALLED, SYS_INCOMPLETE };
int sysstate(void); const char *sysstate_name(void); bool sysstate_guest_allowed(void); bool persist_writable(void);
bool persist_consent_live(void); bool persist_set_state(int st);
u64 pmm_free_pages(void); void acpi_reboot(void);
H
cat > "$D/update.h" <<'H'
#pragma once
#include "fs.h"
bool update_boot_pending(fatfs_t *fs); bool update_boot_apply(fatfs_t *fs); void update_poll(void);
H
cat > "$D/crypto.h" <<'H'
#pragma once
static inline bool crypto_rng_hw(void) { return true; }
H
S="kernel/fs/blkdev.c kernel/fs/fat.c kernel/fs/dfs.c kernel/fs/dmg.c kernel/fs/apfs.c kernel/fs/hfsplus.c kernel/fs/hfsplus_wr.c kernel/fs/inflate.c kernel/fs/lzfse.c kernel/fs/bzip2.c kernel/fs/xz.c kernel/fs/vfs_bridge.c kernel/lib/vfs.c kernel/lib/vfsmeta.c kernel/lib/vfsperm.c kernel/lib/vfsevents.c kernel/lib/vfsops.c kernel/proc/cred.c"
gcc -O1 -D_FILE_OFFSET_BITS=64 tools/volimg.c -o "$D/volimg"
gcc -O1 -g -w -fsanitize=address,undefined -D_FILE_OFFSET_BITS=64 -DMOS_HOST_TEST -I"$D" -Itools/vfsmeta_host -Ikernel/include -Itools \
    tools/persistlazy_test.c "$D/persist.c" $S -o "$D/t"
"$D/volimg" mkdisk "$D/a.img" 160 fat32:MOS:150 >/dev/null
r=0
ASAN_OPTIONS=detect_leaks=0 "$D/t" "$D/a.img" | grep -v "^dfs\|^hfs+\|^fat\|^VFS\|^vol:\|^persist:" || r=1
"$D/volimg" check "$D/a.img" | tail -1
"$D/volimg" check "$D/a.img" >/dev/null || r=1
rm -rf "$D"
exit $r
