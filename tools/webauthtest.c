#define _POSIX_C_SOURCE 200809L
/* cc -std=c11 -Wall -O1 tools/webauthtest.c -o /tmp/webauthtest && /tmp/webauthtest
 * Simula o callback com uma URL local. Não abre navegador nem fala com a rede. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "../userland/libsystem/webauth_core.inc"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void roundtrip(const char *name, const char *auth, const char *scheme, const char *cb, const char *tok, const char *kind) {
    wa_reset();
    int id = wa_begin(auth, scheme);
    CHECK(id == 0);
    CHECK(wa_slots[id].opened == 1);
    CHECK(strcmp(wa_slots[id].auth_url, auth) == 0);
    int got = -1;
    wa_secret sec;
    CHECK(wa_offer("https://evil.example/callback?code=no", &got, &sec) == 0);
    CHECK(wa_offer(cb, &got, &sec) == 1);
    CHECK(got == id);
    CHECK(strcmp(sec.secret, tok) == 0);
    CHECK(strcmp(sec.kind, kind) == 0);
    CHECK(strncmp(sec.service, "mos.oauth.", 10) == 0);
    CHECK(strcmp(sec.account, "callback") == 0);
    CHECK(wa_offer(cb, &got, &sec) == 0);
    printf("  ok  %s\n", name);
}

int main(void) {
    roundtrip("whatsapp", "https://web.whatsapp.com/login", "whatsapp",
              "whatsapp://login?code=wa-local", "wa-local", "code");
    roundtrip("slack", "https://app.slack.com/ssb/signin", "slack",
              "slack://auth?access_token=sl-local", "sl-local", "access_token");
    roundtrip("claude", "https://claude.ai/login", "claude",
              "claude://callback?code=cl-local", "cl-local", "code");
    roundtrip("cursor", "https://cursor.com/login", "cursor",
              "cursor://callback?code=cu%2Dlocal", "cu-local", "code");
    roundtrip("vscode", "https://vscode.dev/login", "vscode",
              "vscode://vscode.github-authentication/did-authenticate?code=vs-local", "vs-local", "code");
    roundtrip("chatgpt", "https://auth.openai.com/authorize", "http",
              "http://127.0.0.1:1455/callback?code=gpt-local", "gpt-local", "code");
    roundtrip("grok", "https://accounts.x.ai/login", "grok",
              "grok://callback#token=gr-local", "gr-local", "token");
    roundtrip("openbot", "https://openbot.run/login", "openbot",
              "openbot://callback?code=ob-local", "ob-local", "code");

    wa_reset();
    int a = wa_begin("https://cursor.com/login", "cursor");
    int b = wa_begin("https://slack.com", "slack");
    CHECK(a == 0 && b == 1);
    int id = -1;
    wa_secret sec;
    CHECK(wa_offer("slack://x?code=s", &id, &sec) == 1);
    CHECK(id == b && strcmp(sec.secret, "s") == 0);
    CHECK(wa_slots[a].done == 0);
    CHECK(wa_cancel(a) == 1);
    CHECK(wa_slots[a].cancelled == 1);
    CHECK(wa_offer("cursor://x?code=c", &id, &sec) == 0);

    printf("webauthtest: %d checagens, %d falhas\n", checks, fails);
    return fails ? 1 : 0;
}
