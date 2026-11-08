/*
 * mOS libSystem — desenrolar através do próprio libSystem, nas duas arquiteturas (libunwind do LLVM, compilado no modo
 * Apple por tools/build-libsystem.ps1 e tools/build-libsystem-arm64.ps1). As imagens Mach-O vêm de
 * _dyld_find_unwind_sections (dlfcn.c); para um pc dentro do libSystem (ELF) ela chama cx_ls_unwind_sections, que devolve
 * o .eh_frame do blob. O deslocamento e o tamanho do .eh_frame são gravados depois do link pelos scripts (procuram o
 * marcador abaixo): o blob carregado não tem cabeçalhos de seção.
 */
extern const char __ehdr_start[], _end[];
volatile unsigned long long cx_ls_ehframe[4] = {0x4d4f5345485f4652ULL /* "RF_HESOM" */, 0x4152344d34365f41ULL, 0, 0};
struct cx_unwind_sections { const void *mh, *dwarf; unsigned long dwarf_len; const void *compact; unsigned long compact_len; };
_Bool cx_ls_unwind_sections(const void *addr, struct cx_unwind_sections *info) {
    if ((const char *)addr < __ehdr_start || (const char *)addr >= _end || !cx_ls_ehframe[3]) return 0;
    info->mh = __ehdr_start;
    info->dwarf = __ehdr_start + cx_ls_ehframe[2];
    info->dwarf_len = (unsigned long)cx_ls_ehframe[3];
    info->compact = 0;
    info->compact_len = 0;
    return 1;
}
/* o libunwind (RWMutex.hpp) chama estes com o nome Mach-O dos cabeçalhos do Darwin (__DARWIN_ALIAS: "_pthread_...") */
int pthread_rwlock_rdlock(void *), pthread_rwlock_wrlock(void *), pthread_rwlock_unlock(void *);
int cx_rw_rdlock(void *l) __asm__("_pthread_rwlock_rdlock");
int cx_rw_wrlock(void *l) __asm__("_pthread_rwlock_wrlock");
int cx_rw_unlock(void *l) __asm__("_pthread_rwlock_unlock");
int cx_rw_rdlock(void *l) { return pthread_rwlock_rdlock(l); }
int cx_rw_wrlock(void *l) { return pthread_rwlock_wrlock(l); }
int cx_rw_unlock(void *l) { return pthread_rwlock_unlock(l); }

/* Chave para desligar o desenrolar novo (exceções C++/ObjC) e voltar ao comportamento antigo, em que toda exceção virava
 * terminate: MOS_DESENROLAR=0 no ambiente do processo, ou o arquivo /Sistema/Biblioteca/sem-desenrolar na imagem. Com ela,
 * _dyld_find_unwind_sections não acha tabela nenhuma e o _Unwind_RaiseException devolve _URC_END_OF_STACK. */
char *getenv(const char *);
int access(const char *, int);
int cx_unwind_disabled(void) {
    static int st = -1;
    if (st < 0) {
        const char *e = getenv("MOS_DESENROLAR");
        st = (e && e[0] == '0') || access("/Sistema/Biblioteca/sem-desenrolar", 0) == 0;
    }
    return st;
}
