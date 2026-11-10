/* Teste no host das fontes da libc++/libc++abi do LLVM (as mesmas do dylib Mach-O).
 * Chama os símbolos que o vsce-sign e o helpers/node do Cursor importam quando a
 * definição do cabeçalho atual também é inline: o corpo executado é o do .cpp. */
#include <algorithm>
#include <atomic>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <typeinfo>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

extern "C" {
unsigned long _ZNKSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE4copyEPcmm(
    const std::string *, char *, unsigned long, unsigned long);
void _ZNSt3__115basic_stringbufIcNS_11char_traitsIcEENS_9allocatorIcEEE3strERKNS_12basic_stringIcS2_S4_EE(
    std::stringbuf *, const std::string *);
void _ZNSt3__114basic_ofstreamIcNS_11char_traitsIcEEE4openERKNS_12basic_stringIcS2_NS_9allocatorIcEEEEj(
    std::ofstream *, const std::string *, unsigned);
void _ZNSt9exceptionD1Ev(void *);
void _ZNSt9exceptionD2Ev(void *);
extern char _ZTVSt9exception[];
void *_ZNSt3__112__get_sp_mutEPKv(const void *);
}

namespace fs = std::filesystem;

static int falhas;

static void espera(bool ok, const char *msg) {
    if (!ok) {
        std::fprintf(stderr, "libcxx-host: %s\n", msg);
        falhas++;
    }
}

static void teste_string() {
    std::string s("abcdef");
    char buf[8];
    std::memset(buf, 0, sizeof buf);
    unsigned long n = _ZNKSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE4copyEPcmm(&s, buf, 3, 1);
    espera(n == 3 && std::memcmp(buf, "bcd", 3) == 0, "string::copy");
    bool lancou = false;
    try {
        _ZNKSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE4copyEPcmm(&s, buf, 1, 99);
    } catch (const std::out_of_range &) {
        lancou = true;
    }
    espera(lancou, "string::copy fora do intervalo");
}

static void teste_stringbuf() {
    std::stringbuf b;
    std::string in("texto-do-stringbuf");
    _ZNSt3__115basic_stringbufIcNS_11char_traitsIcEENS_9allocatorIcEEE3strERKNS_12basic_stringIcS2_S4_EE(&b, &in);
    espera(b.str() == in, "stringbuf::str");
}

static void teste_exception() {
    std::exception e;
    espera(e.what() != nullptr && e.what()[0] != 0, "exception::what");
    bool pegou = false;
    try {
        throw std::exception();
    } catch (const std::exception &ex) {
        pegou = ex.what() != nullptr;
    }
    espera(pegou, "throw/catch std::exception");
    volatile char *vt = _ZTVSt9exception;
    espera(vt != nullptr, "vtable std::exception");
    std::exception outra;
    _ZNSt9exceptionD1Ev(&outra);
    _ZNSt9exceptionD2Ev(&e);
}

struct BaseVirtual {
    virtual ~BaseVirtual() {}
};

static void teste_bad_typeid() {
    bool pegou = false;
    try {
        BaseVirtual *p = nullptr;
        (void)typeid(*p);
    } catch (const std::bad_typeid &) {
        pegou = true;
    }
    espera(pegou, "typeid em ponteiro nulo");
}

static void teste_ofstream(const fs::path &arq) {
    std::ofstream o;
    std::string caminho = arq.string();
    _ZNSt3__114basic_ofstreamIcNS_11char_traitsIcEEE4openERKNS_12basic_stringIcS2_NS_9allocatorIcEEEEj(
        &o, &caminho, static_cast<unsigned>(std::ios::out));
    espera(o.good(), "ofstream::open");
    o << "linha-1\n";
    o.close();
    std::ifstream in(arq);
    std::string lido;
    std::getline(in, lido);
    espera(lido == "linha-1", "ofstream releitura");
}

static void teste_filesystem(const fs::path &raiz) {
    fs::path arvore = raiz / "arvore";
    fs::create_directories(arvore / "sub");
    {
        std::ofstream f(arvore / "sub" / "a.txt");
        f << "x";
    }
    fs::path alvo = arvore / "sub" / "a.txt";
    fs::path link = arvore / "atalho";
    fs::create_symlink(alvo, link);
    espera(fs::read_symlink(link) == alvo, "read_symlink");

    fs::path antigo = fs::current_path();
    fs::current_path(arvore);
    espera(fs::current_path() == arvore, "current_path");
    fs::current_path(antigo);
    espera(fs::current_path() == antigo, "current_path restaurado");

    std::uintmax_t n = fs::remove_all(arvore);
    espera(n >= 3 && !fs::exists(arvore), "remove_all");
}

static void teste_sort() {
    int a[] = {4, 1, 3, 2, 2};
    std::sort(a, a + 5);
    int esp[] = {1, 2, 2, 3, 4};
    espera(std::memcmp(a, esp, sizeof esp) == 0, "sort int");
    double d[] = {0.5, -1.0, 0.5, 3.0};
    std::sort(d, d + 4);
    espera(d[0] == -1.0 && d[3] == 3.0, "sort double");
    std::vector<unsigned> u{9u, 0u, 4u};
    std::sort(u.begin(), u.end());
    espera(u[0] == 0u && u[2] == 9u, "sort unsigned");
}

static void teste_sp_mut() {
    auto p = std::make_shared<int>(7);
    std::atomic<int> ok{1};
    std::thread t([&] {
        for (int i = 0; i < 2000; i++) {
            std::shared_ptr<int> c = p;
            if (!c || *c != 7) ok.store(0);
        }
    });
    for (int i = 0; i < 2000; i++) {
        std::shared_ptr<int> c = p;
        if (!c || *c != 7) ok.store(0);
    }
    t.join();
    espera(ok.load() == 1 && p.use_count() == 1, "shared_ptr concorrente");
    espera(_ZNSt3__112__get_sp_mutEPKv(p.get()) != nullptr, "__get_sp_mut");
}

int main() {
    char modelo[] = "/tmp/mos-libcxx-XXXXXX";
    char *dir = mkdtemp(modelo);
    espera(dir != nullptr, "mkdtemp");
    if (!dir) return 1;
    fs::path raiz(dir);
    teste_string();
    teste_stringbuf();
    teste_exception();
    teste_bad_typeid();
    teste_ofstream(raiz / "saida.txt");
    teste_filesystem(raiz);
    teste_sort();
    teste_sp_mut();
    std::error_code ec;
    fs::remove_all(raiz, ec);
    if (falhas) {
        std::fprintf(stderr, "libcxx-host: %d falha(s)\n", falhas);
        return 1;
    }
    std::printf("libcxx-host: ok\n");
    return 0;
}
