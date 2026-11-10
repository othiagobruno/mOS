/* Liga um cx_tcc_request forte e confere que cx_media_tcc deixa de ser -1.
 * clang -std=c11 -O1 -o /tmp/audiodec_tcc tools/audiodec_tcc_test.c userland/libsystem/audiodec.c -lm
 */
#include <stdio.h>
int cx_tcc_request(const char *service) { return service && service[0] == 'k' ? 1 : 0; }
int cx_media_tcc(const char *service);
int main(void) {
    int mic = cx_media_tcc("kTCCServiceMicrophone");
    int cam = cx_media_tcc("kTCCServiceCamera");
    int bad = cx_media_tcc("");
    printf("tcc forte mic %d cam %d vazio %d\n", mic, cam, bad);
    if (mic != 1 || cam != 1 || bad != 0) { printf("FALHOU\n"); return 1; }
    printf("PASSOU\n");
    return 0;
}
