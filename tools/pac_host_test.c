/* Teste de host do PAC (cfpac.c) com o motor do JavaScriptCore.framework do mOS: tools/pac_host.sh */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
char *cx_pac_result(const char *script, const char *url, char **err);
static int fails, checks;
static void eq(const char *script, const char *url, const char *want) {
    char *err = 0, *r = cx_pac_result(script, url, &err);
    checks++;
    if (!r || strcmp(r, want)) { fails++; printf("FALHOU %s -> \"%s\" (esperado \"%s\") erro=%s\n", url, r ? r : "(null)", want, err ? err : ""); }
    free(r); free(err);
}
int main(void) {
    const char *pac =
        "function FindProxyForURL(url, host) {\n"
        "  if (isPlainHostName(host) || dnsDomainIs(host, '.local')) return 'DIRECT';\n"
        "  if (shExpMatch(url, 'http://*.exemplo.com/api/*')) return 'PROXY api-px.exemplo.com:3128';\n"
        "  if (isInNet(host, '10.0.0.0', '255.0.0.0')) return 'DIRECT';\n"
        "  if (isInNet(myIpAddress(), '192.168.15.0', '255.255.255.0')) return 'PROXY casa:8080; SOCKS5 sk:1080; DIRECT';\n"
        "  return 'DIRECT';\n"
        "}\n";
    eq(pac, "http://servidor/x", "DIRECT");
    eq(pac, "http://impressora.local/", "DIRECT");
    eq(pac, "http://www.exemplo.com/api/v1", "PROXY api-px.exemplo.com:3128");
    eq(pac, "http://10.2.3.4/", "DIRECT");
    eq(pac, "https://www.example.org/", "PROXY casa:8080; SOCKS5 sk:1080; DIRECT");
    /* funções auxiliares, cada uma devolvendo texto */
    struct { const char *expr, *want; } t[] = {
        {"localHostOrDomainIs('www','www.mos.dev')", "true"}, {"localHostOrDomainIs('www.x.dev','www.mos.dev')", "false"},
        {"dnsDomainLevels('a.b.c')", "2"}, {"isValidIpAddress('1.2.3.4')", "true"}, {"isValidIpAddress('1.2.3.256')", "false"},
        {"isValidIpAddress('a.b.c.d')", "false"}, {"convert_addr('1.2.3.4')", "16909060"},
        {"shExpMatch('www.mos.dev','*.mos.*')", "true"}, {"shExpMatch('mos.dev','*.mos.*')", "false"}, {"shExpMatch('ab','a?')", "true"},
        {"isInNetEx('192.168.1.7','192.168.0.0/16')", "true"}, {"isInNetEx('192.169.1.7','192.168.0.0/16')", "false"},
        {"isResolvable('localhost')", "true"}, {"dnsResolve('localhost')", "127.0.0.1"}, {"isResolvable('nao-existe.invalid')", "false"},
        {"weekdayRange('SUN','SAT')", "true"}, {"timeRange(0,24)", "true"}, {"dateRange('JAN','DEC')", "true"},
        {"sortIpAddressList('2.2.2.2;1.1.1.1')", "1.1.1.1;2.2.2.2"},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        char s[400];
        snprintf(s, sizeof s, "function FindProxyForURL(u,h){return ''+(%s);}", t[i].expr);
        eq(s, "http://x/", t[i].want);
    }
    /* dia da semana de hoje exato e o seguinte */
    time_t now = time(0); struct tm tm; localtime_r(&now, &tm);
    static const char *D[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    char s[200];
    snprintf(s, sizeof s, "function FindProxyForURL(u,h){return ''+weekdayRange('%s')+weekdayRange('%s');}", D[tm.tm_wday], D[(tm.tm_wday + 1) % 7]);
    eq(s, "http://x/", "truefalse");
    snprintf(s, sizeof s, "function FindProxyForURL(u,h){return ''+timeRange(%d)+timeRange(%d);}", tm.tm_hour, (tm.tm_hour + 1) % 24);
    eq(s, "http://x/", "truefalse");
    /* FindProxyForURLEx tem preferência; erro de script volta como erro */
    eq("function FindProxyForURL(u,h){return 'DIRECT';} function FindProxyForURLEx(u,h){return 'PROXY v6:1';}", "http://x/", "PROXY v6:1");
    char *err = 0, *r = cx_pac_result("function FindProxyForURL(u,h){ return nada(); }", "http://x/", &err);
    checks++; if (r || !err) { fails++; printf("FALHOU erro de script não voltou\n"); }
    free(r); free(err);
    printf("pac_host: %d/%d ok\n", checks - fails, checks);
    return fails ? 1 : 0;
}
