/* Binário mínimo que importa o CoreImage real. Não roda no Linux: o machocheck lê o import. */
extern int mos_coreimage_real(void);
int main(void) { return mos_coreimage_real(); }
