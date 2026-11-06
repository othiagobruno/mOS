/* mOS — teste do dlopen: dylib dependência do plugin (carregada junto, por @rpath) */
int dep_ready;
__attribute__((constructor)) static void dep_init(void) { dep_ready = 7; }   /* roda antes do construtor do plugin */
int dep_mul(int a, int b) { return a * b * (dep_ready == 7 ? 1 : -1); }
