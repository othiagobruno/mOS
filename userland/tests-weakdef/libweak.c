/* mOS — biblioteca com definições weak que o executável também define (teste da coalescência do carregador).
 * No macOS o dyld deixa uma só definição por nome no processo: a primeira forte em ordem de carga, senão a
 * primeira weak. O executável carrega primeiro, então as dele ganham. */
__attribute__((weak, visibility("default"))) int coalesce_val = 20;
__attribute__((weak, visibility("default"))) int quem(void) { return 2; }
__attribute__((weak, visibility("default"))) int so_na_lib(void) { return 30; }
int lib_quem(void) { return quem(); }
int *lib_val(void) { return &coalesce_val; }
int lib_so(void) { return so_na_lib(); }
