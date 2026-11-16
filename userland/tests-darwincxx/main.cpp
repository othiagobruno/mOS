// mOS — os casos C++ dos testes do Darling (tests/src: exception.cpp, tls2.cpp, fstream.cpp), reescritos com o
// comportamento do macOS: throw/catch de int e de classe, thread_local com construtor e destrutor por thread,
// filebuf/ofstream/ifstream de 8000 bytes. O mesmo binário roda no Darling, no macOS e no mOS.
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>

static int fails, total;
static void expect(long long got, long long want, const char *what) {
    total++;
    if (got == want) std::printf("  ok  %s = %lld\n", what, got);
    else { fails++; std::printf("  FALHOU  %s = %lld (esperado %lld)\n", what, got, want); }
}

static int ctors, dtors;
struct PerThread {
    int val;
    PerThread() : val(123) { __atomic_add_fetch(&ctors, 1, __ATOMIC_SEQ_CST); }
    ~PerThread() { __atomic_add_fetch(&dtors, 1, __ATOMIC_SEQ_CST); }
};
static thread_local PerThread tl;

static std::string read_line(const char *path) {
    std::ifstream in(path);
    std::string s;
    std::getline(in, s);
    return s;
}

int main() {
    std::setbuf(stdout, nullptr);

    std::printf("exceções\n");
    int caught = 0;
    try { throw 5; } catch (int e) { caught = e; }
    expect(caught, 5, "catch (int) pega o 5");
    std::string msg;
    try { throw std::runtime_error("falhou aqui"); } catch (const std::exception &e) { msg = e.what(); }
    expect(msg == "falhou aqui", 1, "catch (std::exception&) com what()");
    int order = 0;
    try { try { throw 1; } catch (...) { order = 1; throw; } } catch (int) { order = order * 10 + 2; }
    expect(order, 12, "rethrow chega ao catch de fora");

    std::printf("thread_local com construtor\n");
    expect(tl.val, 123, "construtor rodou na thread principal");
    tl.val = 321;
    int seen = -1;
    std::thread t([&] { seen = tl.val; });
    t.join();
    expect(seen, 123, "a outra thread tem a própria cópia");
    expect(tl.val, 321, "a principal não mudou");
    expect(dtors, 1, "o destrutor rodou quando a thread saiu");

    std::printf("fstream\n");
    char text[8001] = "";
    for (int i = 0; i < 800; i++) std::memcpy(text + i * 10, "1234567890", 11);
    const char *file = "/tmp/darwincxx-fstream";
    {
        std::filebuf fb;
        fb.open(file, std::ios_base::binary | std::ios_base::in | std::ios_base::out | std::ios_base::trunc);
        expect(fb.sputn(text, (std::streamsize)std::strlen(text)), 8000, "filebuf::sputn grava 8000 bytes");
        fb.close();
    }
    expect(read_line(file) == text, 1, "ifstream lê o que o filebuf gravou");
    {
        std::ofstream of(file, std::ios_base::binary | std::ios_base::trunc);
        of.write(text, (std::streamsize)std::strlen(text));
        expect(of.good(), 1, "ofstream::write");
    }
    expect(read_line(file) == text, 1, "ifstream lê o que o ofstream gravou");
    unlink(file);
    std::ifstream missing("/tmp/darwincxx-nao-existe");
    expect(missing.is_open(), 0, "ifstream de arquivo que não existe");

    std::printf("darwincxx: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
