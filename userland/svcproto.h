/* mOS — protocolo dos serviços do sistema (launchd, notifyd, logd, configd) sobre portas Mach.
 * Incluído pelos daemons nativos (programas em userland/, via svcnative.h) e pelo libSystem (notify.c, oslog.c,
 * sysconfig.c, launchctl). Só constantes e o formato da mensagem: os dois lados são código do mOS,
 * então o fio não é o da Apple (o que importa para os apps é a API: notify_*, os_log, SCDynamicStore...).
 *
 * Mensagem de pedido (sem descritores):
 *     header Mach (24 bytes) | svc_body_t | n1 bytes | n2 bytes
 * Pedido que leva um direito de envio (notify_register_mach_port/dispatch, configd notificações):
 *     header com MACH_MSGH_BITS_COMPLEX | u32 contagem = 1 | descritor de porta (12) | svc_body_t | ...
 * O id do header é SVC_MSG_ID. Pedido com resposta leva a porta de resposta no local (MAKE_SEND_ONCE; quem recebe a acha no remote);
 * a resposta é header | svc_body_t (op = código de retorno) | dados. */
#pragma once

#define SVC_MSG_ID      0x6d4f5331u      /* 'mOS1' */
#define SVC_REPLY_ID    0x6d4f5332u
#define SVC_NOTE_ID     0x6d4f5333u      /* aviso de um daemon a um cliente (notifyd, configd) */
#define SVC_MAX_MSG     (64u * 1024u)

typedef struct {
    unsigned op;                 /* operação (pedido) ou código de retorno (resposta) */
    unsigned a, b, c;            /* argumentos inteiros */
    unsigned long long q;        /* argumento de 64 bits (estado do notify, tempo) */
    unsigned n1, n2;             /* tamanhos das duas cadeias/blocos que seguem */
} svc_body_t;

/* ---- nomes no bootstrap */
#define LAUNCHD_SERVICE  "com.apple.launchd.mos"            /* controle (launchctl, SMAppService, XPC em bundles) */
#define NOTIFYD_SERVICE  "com.apple.system.notification_center"
#define LOGD_SERVICE     "com.apple.system.logger"
#define CONFIGD_SERVICE  "com.apple.SystemConfiguration.configd"

/* ---- launchd */
enum {
    LC_LIST = 1,      /* -> texto "PID\tStatus\tLabel" */
    LC_LOAD,          /* s1 = caminho do plist -> 0 ou erro */
    LC_UNLOAD,        /* s1 = rótulo ou caminho */
    LC_START,         /* s1 = rótulo */
    LC_STOP,          /* s1 = rótulo (KeepAlive o reinicia) */
    LC_SUBMIT,        /* s1 = rótulo (= nome Mach), s2 = executável: serviço sob demanda (XPC dentro de bundles) */
    LC_PRINT,         /* s1 = rótulo -> texto */
    LC_REMOVE,        /* s1 = rótulo: para e esquece (launchctl remove) */
};

/* ---- notifyd */
enum {
    NT_POST = 1,      /* s1 = nome */
    NT_REGISTER_PORT, /* s1 = nome, a = token do cliente; descritor = envio para a porta do cliente */
    NT_REGISTER_SIGNAL,/* s1 = nome, a = token, b = sinal (pid vem do trailer) */
    NT_CANCEL,        /* a = token (do mesmo pid) */
    NT_GET_STATE,     /* s1 = nome -> q = estado, a = geração */
    NT_SET_STATE,     /* s1 = nome, q = estado */
    NT_GENERATION,    /* s1 = nome -> a = geração (quantos posts) */
};
/* aviso do notifyd: msgh_id = token (como o notifyd da Apple), svc_body_t.a = token, s1 = nome */

/* ---- logd (só envio, sem resposta) */
enum { LG_WRITE = 1 };
/* a = nível (os_log_type_t: 0 padrão, 1 info, 2 debug, 16 erro, 17 falha), b = pid de origem;
 * s1 = "processo\0subsistema\0categoria\0", s2 = mensagem já formatada */
#define LOG_STORE_DIR   "/private/var/db/diagnostics"
#define LOG_STORE       "/private/var/db/diagnostics/mos-log.txt"
#define LOG_STORE_OLD   "/private/var/db/diagnostics/mos-log.0.txt"
#define LOG_STORE_MAX   (2u << 20)

/* ---- configd: valores como plist XML (o libSystem converte de/para CFPropertyList) */
enum {
    CF_GET = 1,       /* s1 = chave -> dados = XML do valor (n1 = 0: não existe) */
    CF_SET,           /* s1 = chave, s2 = XML */
    CF_REMOVE,        /* s1 = chave */
    CF_LIST,          /* s1 = padrão (regex simples: literal, ".*", "[^/]+") -> chaves separadas por '\n' */
    CF_WATCH,         /* s1 = chaves exatas separadas por '\n', s2 = padrões; descritor = porta do cliente; a = id */
    CF_UNWATCH,       /* a = id */
    CF_NOTIFY,        /* s1 = chave: força o aviso aos interessados (SCDynamicStoreNotifyValue) */
};
/* aviso do configd: header id SVC_NOTE_ID, a = id do registro, s1 = chaves mudadas separadas por '\n' */
#define CONFIG_PROXIES_PLIST "/Library/Preferences/SystemConfiguration/preferences.plist"
#define NET_CHANGE_NOTIFICATION "com.apple.system.config.network_change"
