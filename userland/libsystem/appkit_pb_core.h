/* mOS — núcleo do NSPasteboard (sem Objective-C): nome canônico dos tipos (legados → UTI), tipos de texto e
 * caminho ↔ URL file://. Teste de host: userland/tests-appkit/pasteboard_host.c (-DCX_PBK_HOST). */
#pragma once
/* UTI canônica de um tipo (devolve o próprio nome quando já é moderno ou desconhecido) */
const char *pbk_canonical(const char *type);
/* 1 = NSFilenamesPboardType (lista de caminhos, vira um item public.file-url por arquivo) */
int pbk_is_filenames(const char *type);
/* tipo textual: lê como string */
int pbk_is_text(const char *type);
/* /a b/ç → file:///a%20b/%C3%A7 ; devolve 0 se não coube */
int pbk_path_to_url(const char *path, char *out, int cap);
/* file:///a%20b → /a b ; aceita file://localhost/ ; 0 se não é URL de arquivo */
int pbk_url_to_path(const char *url, char *out, int cap);
/* os dois tipos são o mesmo depois de canonizar */
int pbk_same(const char *a, const char *b);
