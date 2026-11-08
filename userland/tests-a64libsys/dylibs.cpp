// mOS — app arm64 ligado nas bibliotecas universais do disco (frente LIBSYS do tradutor arm64): /usr/lib/libc++.1.dylib,
// /usr/lib/libsqlite3.dylib e /usr/lib/libicucore.A.dylib, com o slice arm64 escolhido pelo carregador.
// tools/build-libsys-arm64-test.ps1 compila com -nostdlib++ contra os slices de build\libcxx-arm64 etc.
#include <stdio.h>
#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <map>
extern "C" {
int sqlite3_open(const char *, void **);
int sqlite3_exec(void *, const char *, int (*)(void *, int, char **, char **), void *, char **);
int sqlite3_close(void *);
const char *sqlite3_libversion(void);
typedef unsigned short UChar;
int u_strToUpper(UChar *dest, int cap, const UChar *src, int len, const char *locale, int *err);
const char *u_errorName(int);
}
static int falhas;
#define CHECK(n, c) do { if (c) printf("[arm64] ok    %s\n", n); else { printf("[arm64] FALHA %s (linha %d)\n", n, __LINE__); falhas++; } fflush(stdout); } while (0)
static int cb(void *p, int n, char **v, char **) { if (n > 0 && v[0]) *(std::string *)p += v[0]; return 0; }
int main() {
    std::ostringstream os;
    os << "x=" << 42 << " y=" << 2.5;
    CHECK("libc++ do disco: ostringstream", os.str() == "x=42 y=2.5");
    std::map<std::string, std::vector<int>> m;
    for (int i = 0; i < 100; i++) m[std::to_string(i % 7)].push_back(i);
    CHECK("libc++ do disco: map/vector/to_string", m.size() == 7 && m["3"].size() == 14 && m["0"].size() == 15);
    std::string w;
    try { throw std::out_of_range("fora"); } catch (const std::logic_error &e) { w = e.what(); }
    CHECK("libc++abi do disco: throw/catch por classe base", w == "fora");
    void *db = 0; std::string out;
    int r = sqlite3_open(":memory:", &db);
    r |= sqlite3_exec(db, "create table t(a); insert into t values(40); insert into t values(2);", 0, 0, 0);
    r |= sqlite3_exec(db, "select sum(a) from t;", cb, &out, 0);
    sqlite3_close(db);
    CHECK("libsqlite3 do disco (memória)", r == 0 && out == "42");
    printf("  sqlite %s\n", sqlite3_libversion());
    UChar src[] = {'a', 'b', 0xE7, 0}, dst[8] = {0}; int err = 0;
    int n = u_strToUpper(dst, 8, src, 3, "pt_BR", &err);
    CHECK("libicucore do disco: u_strToUpper", err <= 0 && n == 3 && dst[0] == 'A' && dst[2] == 0xC7);
    printf("[arm64] dylibs: %d falhas\n", falhas);
    return falhas;
}
