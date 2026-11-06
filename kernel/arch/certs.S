/* mOS — lista de autoridades certificadoras confiáveis (bundle da Mozilla, MPL 2.0;
 * ver assets/certs/README). Decodificada por kernel/net/x509.c no primeiro uso. */
    .section .rdata,"dr"
    .balign 16
    .globl mos_cacert_pem, mos_cacert_pem_end
mos_cacert_pem:
    .incbin "assets/certs/cacert.pem"
mos_cacert_pem_end:
    .byte 0
