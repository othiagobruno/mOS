/*
 * mOS SDK — cabeçalho guarda-chuva do módulo clang MOSDarwin: a libc/POSIX do macOS (cabeçalhos Darwin do zig, ABI
 * x86_64-macos que o libSystem do mOS implementa), no papel que o módulo Darwin tem no SDK da Apple. Copiado para
 * build\swift\openswiftui\darwin-libc pelo tools\build-openswiftui-deps.ps1.
 */
#pragma once

#if __has_include(<assert.h>)
#include <assert.h>
#endif
#if __has_include(<ctype.h>)
#include <ctype.h>
#endif
#if __has_include(<errno.h>)
#include <errno.h>
#endif
#if __has_include(<fenv.h>)
#include <fenv.h>
#endif
#if __has_include(<float.h>)
#include <float.h>
#endif
#if __has_include(<inttypes.h>)
#include <inttypes.h>
#endif
#if __has_include(<limits.h>)
#include <limits.h>
#endif
#if __has_include(<locale.h>)
#include <locale.h>
#endif
#if __has_include(<xlocale.h>)
#include <xlocale.h>
#endif
#if __has_include(<math.h>)
#include <math.h>
#endif
#if __has_include(<setjmp.h>)
#include <setjmp.h>
#endif
#if __has_include(<signal.h>)
#include <signal.h>
#endif
#if __has_include(<stdarg.h>)
#include <stdarg.h>
#endif
#if __has_include(<stdbool.h>)
#include <stdbool.h>
#endif
#if __has_include(<stddef.h>)
#include <stddef.h>
#endif
#if __has_include(<stdint.h>)
#include <stdint.h>
#endif
#if __has_include(<stdio.h>)
#include <stdio.h>
#endif
#if __has_include(<stdlib.h>)
#include <stdlib.h>
#endif
#if __has_include(<string.h>)
#include <string.h>
#endif
#if __has_include(<strings.h>)
#include <strings.h>
#endif
#if __has_include(<time.h>)
#include <time.h>
#endif
#if __has_include(<wchar.h>)
#include <wchar.h>
#endif
#if __has_include(<wctype.h>)
#include <wctype.h>
#endif

#if __has_include(<unistd.h>)
#include <unistd.h>
#endif
#if __has_include(<fcntl.h>)
#include <fcntl.h>
#endif
#if __has_include(<dirent.h>)
#include <dirent.h>
#endif
#if __has_include(<dlfcn.h>)
#include <dlfcn.h>
#endif
#if __has_include(<fnmatch.h>)
#include <fnmatch.h>
#endif
#if __has_include(<glob.h>)
#include <glob.h>
#endif
#if __has_include(<grp.h>)
#include <grp.h>
#endif
#if __has_include(<pwd.h>)
#include <pwd.h>
#endif
#if __has_include(<libgen.h>)
#include <libgen.h>
#endif
#if __has_include(<poll.h>)
#include <poll.h>
#endif
#if __has_include(<pthread.h>)
#include <pthread.h>
#endif
#if __has_include(<sched.h>)
#include <sched.h>
#endif
#if __has_include(<semaphore.h>)
#include <semaphore.h>
#endif
#if __has_include(<spawn.h>)
#include <spawn.h>
#endif
#if __has_include(<termios.h>)
#include <termios.h>
#endif
#if __has_include(<utime.h>)
#include <utime.h>
#endif
#if __has_include(<fts.h>)
#include <fts.h>
#endif
#if __has_include(<getopt.h>)
#include <getopt.h>
#endif
#if __has_include(<sysexits.h>)
#include <sysexits.h>
#endif

#if __has_include(<sys/types.h>)
#include <sys/types.h>
#endif
#if __has_include(<sys/stat.h>)
#include <sys/stat.h>
#endif
#if __has_include(<sys/mman.h>)
#include <sys/mman.h>
#endif
#if __has_include(<sys/time.h>)
#include <sys/time.h>
#endif
#if __has_include(<sys/times.h>)
#include <sys/times.h>
#endif
#if __has_include(<sys/wait.h>)
#include <sys/wait.h>
#endif
#if __has_include(<sys/param.h>)
#include <sys/param.h>
#endif
#if __has_include(<sys/mount.h>)
#include <sys/mount.h>
#endif
#if __has_include(<sys/sysctl.h>)
#include <sys/sysctl.h>
#endif
#if __has_include(<sys/ioctl.h>)
#include <sys/ioctl.h>
#endif
#if __has_include(<sys/socket.h>)
#include <sys/socket.h>
#endif
#if __has_include(<sys/uio.h>)
#include <sys/uio.h>
#endif
#if __has_include(<sys/resource.h>)
#include <sys/resource.h>
#endif
#if __has_include(<sys/select.h>)
#include <sys/select.h>
#endif
#if __has_include(<sys/file.h>)
#include <sys/file.h>
#endif
#if __has_include(<sys/xattr.h>)
#include <sys/xattr.h>
#endif
#if __has_include(<sys/attr.h>)
#include <sys/attr.h>
#endif
#if __has_include(<sys/un.h>)
#include <sys/un.h>
#endif
#if __has_include(<sys/utsname.h>)
#include <sys/utsname.h>
#endif
#if __has_include(<sys/event.h>)
#include <sys/event.h>
#endif
#if __has_include(<sys/clonefile.h>)
#include <sys/clonefile.h>
#endif
#if __has_include(<sys/acl.h>)
#include <sys/acl.h>
#endif

#if __has_include(<netinet/in.h>)
#include <netinet/in.h>
#endif
#if __has_include(<arpa/inet.h>)
#include <arpa/inet.h>
#endif
#if __has_include(<netdb.h>)
#include <netdb.h>
#endif

#if __has_include(<malloc/malloc.h>)
#include <malloc/malloc.h>
#endif
#if __has_include(<copyfile.h>)
#include <copyfile.h>
#endif
#if __has_include(<removefile.h>)
#include <removefile.h>
#endif
#if __has_include(<os/lock.h>)
#include <os/lock.h>
#endif
#if __has_include(<crt_externs.h>)
#include <crt_externs.h>
#endif
#if __has_include(<uuid/uuid.h>)
#include <uuid/uuid.h>
#endif
#if __has_include(<sysdir.h>)
#include <sysdir.h>
#endif
#if __has_include(<execinfo.h>)
#include <execinfo.h>
#endif
#if __has_include(<mach/mach.h>)
#include <mach/mach.h>
#endif
#if __has_include(<mach/mach_time.h>)
#include <mach/mach_time.h>
#endif
#if __has_include(<mach-o/dyld.h>)
#include <mach-o/dyld.h>
#endif
#if __has_include(<libkern/OSByteOrder.h>)
#include <libkern/OSByteOrder.h>
#endif

/* errno é macro (*__error()); o overlay Swift do MOSDarwin expõe `errno` como variável. */
