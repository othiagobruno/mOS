/* mOS libSystem — classes que o helper "Codex Computer Use" do ChatGPT desktop referencia (EventKit, Intents,
 * ScriptingBridge, OSLog, AVFAudio). São, no uso do helper, portadores de dados: aqui cada uma é uma classe de
 * verdade (superclasse certa, isKindOfClass: funciona) com um dicionário interno por chave-valor, então init...,
 * os getters e setters e setValue:forKey:/valueForKey: guardam e devolvem o que recebem. O que depende de um
 * serviço que o mOS não tem falha com honestidade: EKEventStore nega o acesso (não há calendário do sistema),
 * SBApplication não acha outro app para roteirizar, e OSLogStore devolve uma sequência vazia (não há log unificado). */
#include "foundation.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
int strcmp(const char *a, const char *b);
int cx_tcc_state(const char *service);

/* bolsa chave-valor: a primeira palavra extra do objeto guarda um NSMutableDictionary */
static id bag(id self, int make) {
    id *slot = (id *)((u8 *)self + sizeof(void *));
    if (!*slot && make) *slot = objc_retain(MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary"));
    return *slot;
}
static id db_value_for_key(id self, SEL c, id key) {
    (void)c;
    id d = bag(self, 0);
    return d ? MSG(id, d, "objectForKey:", key) : 0;
}
static void db_set_value_for_key(id self, SEL c, id v, id key) {
    (void)c;
    id d = bag(self, 1);
    if (!d || !key) return;
    if (v) MSG(void, d, "setObject:forKey:", v, key);
    else MSG(void, d, "removeObjectForKey:", key);
}
static id db_init(id self, SEL c) { (void)c; return self; }
static void db_dealloc(id self, SEL c) {
    id d = bag(self, 0);
    if (d) objc_release(d);
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}

/* getter/setter por nome de propriedade, sobre a bolsa (setFoo: guarda em "foo", foo devolve) */
static id prop_get(id self, SEL c) {
    const char *s = (const char *)c;
    return db_value_for_key(self, 0, cx_str(s));
}
static void prop_set(id self, SEL c, id v) {
    const char *s = (const char *)c;    /* "setFoo:" -> "foo" */
    char k[128];
    int i = 3, o = 0;
    if (!s || s[0] != 's' || s[1] != 'e' || s[2] != 't') return;
    k[o++] = (char)(s[i] >= 'A' && s[i] <= 'Z' ? s[i] + 32 : s[i]);
    for (i = 4; s[i] && s[i] != ':' && o < 126; i++) k[o++] = s[i];
    k[o] = 0;
    db_set_value_for_key(self, 0, v, cx_str(k));
}

void cx_add_cmethod(cx_class_t *cls, const char *sel, void *imp);

/* os símbolos de classe/metaclasse (OBJC_CLASS_$_Nome): o carregador os resolve neste arquivo */
#define CLS(n) cx_class_t n##_c __asm__("OBJC_CLASS_$_" #n); cx_class_t n##_m __asm__("OBJC_METACLASS_$_" #n);
CLS(EKRecurrenceRule) CLS(EKRecurrenceEnd) CLS(EKRecurrenceDayOfWeek) CLS(EKAlarm) CLS(EKStructuredLocation)
CLS(EKParticipant) CLS(EKAttendee) CLS(INPerson) CLS(INPersonHandle) CLS(INImage) CLS(INFile)
CLS(INSendMessageAttachment) CLS(INSendMessageIntent) CLS(INInteraction) CLS(SBObject) CLS(SBApplication)
CLS(OSLogStore) CLS(AVAudioApplication)
/* EKEventStore: o símbolo é do vscode_fwclasses.c (classe vazia); aqui só ganha os métodos */
extern cx_class_t EKEventStore_c __asm__("OBJC_CLASS_$_EKEventStore"); extern cx_class_t EKEventStore_m __asm__("OBJC_METACLASS_$_EKEventStore");
#define PAIR(n) { if (!strcmp(name, #n)) { if (meta_out) *meta_out = &n##_m; return &n##_c; } }
static cx_class_t *pair_by_name(const char *name, cx_class_t **meta_out) {
    PAIR(EKRecurrenceRule) PAIR(EKRecurrenceEnd) PAIR(EKRecurrenceDayOfWeek) PAIR(EKAlarm) PAIR(EKStructuredLocation)
    PAIR(EKParticipant) PAIR(EKAttendee) PAIR(INPerson) PAIR(INPersonHandle) PAIR(INImage) PAIR(INFile)
    PAIR(INSendMessageAttachment) PAIR(INSendMessageIntent) PAIR(INInteraction) PAIR(SBObject) PAIR(SBApplication)
    PAIR(OSLogStore) PAIR(AVAudioApplication) PAIR(EKEventStore)
    return 0;
}

static void setter_sel(const char *prop, char *out, int cap) {
    int o = 0;
    out[o++] = 's'; out[o++] = 'e'; out[o++] = 't';
    if (prop[0] && o < cap - 2) out[o++] = (char)(prop[0] >= 'a' && prop[0] <= 'z' ? prop[0] - 32 : prop[0]);
    for (int i = 1; prop[i] && o < cap - 2; i++) out[o++] = prop[i];
    out[o++] = ':';
    out[o] = 0;
}
void cx_dataholder_class(const char *name, const char *super, const char *const *props, int nprops,
                         const char *const *inits, int ninits) {
    cx_class_t *c, *m = 0;
    if (cx_class_named(name)) return;
    cx_class_t *sc = cx_class_named(super);
    c = pair_by_name(name, &m);
    if (!c || !m) return;
    cx_define_class(c, m, name, sc ? sc : cx_class_named("NSObject"), (u32)sizeof(void *));   /* +1 palavra: a bolsa */
    cx_add_method(c, "init", (void *)db_init);
    cx_add_method(c, "dealloc", (void *)db_dealloc);
    cx_add_method(c, "valueForKey:", (void *)db_value_for_key);
    cx_add_method(c, "setValue:forKey:", (void *)db_set_value_for_key);
    for (int i = 0; i < nprops; i++) {
        char ss[128];
        cx_add_method(c, props[i], (void *)prop_get);
        setter_sel(props[i], ss, (int)sizeof ss);
        cx_add_method(c, ss, (void *)prop_set);
    }
    for (int i = 0; i < ninits; i++) cx_add_method(c, inits[i], (void *)db_init);
}

static const char *EK_RULE[] = {"frequency", "interval", "daysOfTheWeek", "daysOfTheMonth", "monthsOfTheYear",
                                "weeksOfTheYear", "daysOfTheYear", "setPositions", "recurrenceEnd", "calendarIdentifier"};
static const char *EK_END[] = {"endDate", "occurrenceCount"};
static const char *EK_DOW[] = {"dayOfTheWeek", "weekNumber"};
static const char *EK_ALARM[] = {"absoluteDate", "relativeOffset", "structuredLocation", "proximity"};
static const char *EK_LOC[] = {"title", "geoLocation", "radius"};
static const char *EK_PART[] = {"name", "URL", "participantRole", "participantStatus", "participantType", "isCurrentUser", "contactPredicate"};
static const char *IN_PERSON[] = {"personHandle", "nameComponents", "displayName", "image", "contactIdentifier", "customIdentifier", "aliases"};
static const char *IN_HANDLE[] = {"value", "type", "label"};
static const char *IN_FILE[] = {"data", "filename", "typeIdentifier", "fileURL"};
static const char *IN_ATT[] = {"audioMessageFile", "type"};
static const char *IN_MSG[] = {"recipients", "content", "speakableGroupName", "conversationIdentifier", "serviceName", "sender", "attachments"};
static const char *IN_INTER[] = {"intent", "intentResponse", "intentHandlingStatus", "direction", "identifier", "dateInterval"};

static const char *INIT_NAME[] = {"initWithName:", "init"};

void cx_agentes_classes_init(void) {
    cx_dataholder_class("EKRecurrenceRule", "NSObject", EK_RULE, (int)(sizeof EK_RULE / sizeof EK_RULE[0]), 0, 0);
    cx_dataholder_class("EKRecurrenceEnd", "NSObject", EK_END, 2, 0, 0);
    cx_dataholder_class("EKRecurrenceDayOfWeek", "NSObject", EK_DOW, 2, 0, 0);
    cx_dataholder_class("EKAlarm", "NSObject", EK_ALARM, 4, 0, 0);
    cx_dataholder_class("EKStructuredLocation", "NSObject", EK_LOC, 3, INIT_NAME, 2);
    cx_dataholder_class("EKParticipant", "NSObject", EK_PART, 7, 0, 0);
    cx_dataholder_class("EKAttendee", "EKParticipant", 0, 0, 0, 0);
    cx_dataholder_class("INPerson", "NSObject", IN_PERSON, 7, 0, 0);
    cx_dataholder_class("INPersonHandle", "NSObject", IN_HANDLE, 3, 0, 0);
    cx_dataholder_class("INImage", "NSObject", 0, 0, 0, 0);
    cx_dataholder_class("INFile", "NSObject", IN_FILE, 4, 0, 0);
    cx_dataholder_class("INSendMessageAttachment", "NSObject", IN_ATT, 2, 0, 0);
    cx_dataholder_class("INSendMessageIntent", "NSObject", IN_MSG, 7, 0, 0);
    cx_dataholder_class("INInteraction", "NSObject", IN_INTER, 6, 0, 0);
    cx_dataholder_class("SBObject", "NSObject", 0, 0, 0, 0);
    cx_dataholder_class("SBApplication", "NSObject", 0, 0, 0, 0);
    { extern void cx_agentes_classes_extra(void); cx_agentes_classes_extra(); }
}

/* ---------------------------------------------------------------- serviços que o mOS não tem (falha honesta) */
/* EKEventStore: não há calendário/lembretes do sistema. authorizationStatus = negado (2); requestAccess chama o
 * bloco com (NO, erro). O init existe para o app não quebrar antes de pedir acesso. */
enum { EKAuthDenied = 2 };
static long ek_auth_status(id cls, SEL c, long entity) { (void)cls; (void)c; (void)entity; return EKAuthDenied; }
static id ek_store_init(id self, SEL c) { (void)c; return self; }
typedef void (*ek_cb)(id blk, signed char ok, id err);
static void ek_request(id self, SEL c, long entity, id blk) {
    (void)self; (void)c; (void)entity;
    if (!blk) return;
    id err = MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("EKErrorDomain"), (long)EKAuthDenied, (id)0);
    ((void (*)(id, signed char, id))(*(void **)((char *)blk + 16)))(blk, 0, err);
}
/* SBApplication: roteirizar outro app. Não há outro app no mOS; o objeto existe mas isRunning = NO. */
static id sb_app_with(id cls, SEL c, id ident) { (void)c; (void)ident; return objc_autorelease(MSG0(id, cls, "alloc")); }
static signed char sb_is_running(id self, SEL c) { (void)self; (void)c; return 0; }
/* OSLogStore: não há log unificado. getEntries devolve uma sequência vazia; o store existe. */
static id osl_store(id cls, SEL c, long scope, id *err) { (void)c; (void)scope; if (err) *err = 0; return objc_autorelease(MSG0(id, cls, "alloc")); }
static id osl_entries(id self, SEL c, long opts, id pos, id pred, id *err) {
    (void)self; (void)c; (void)opts; (void)pos; (void)pred; if (err) *err = 0;
    return MSG0(id, (id)cx_class_named("NSArray"), "array");
}
/* AVAudioApplication: estado do mudo de entrada guardado no processo (não há servidor de áudio compartilhado). */
static id ava_shared(id cls, SEL c) {
    (void)c;
    static id sh;
    if (!sh) sh = MSG0(id, cls, "new");
    return sh;
}
static signed char ava_input_muted(id self, SEL c) { id v = db_value_for_key(self, 0, cx_str("inputMuted")); (void)c; return v ? MSG0(signed char, v, "boolValue") : 0; }
static signed char ava_set_muted(id self, SEL c, signed char m, id *err) {
    (void)c; if (err) *err = 0;
    db_set_value_for_key(self, 0, MSG(id, (id)cx_class_named("NSNumber"), "numberWithBool:", m), cx_str("inputMuted"));
    return 1;
}

void cx_agentes_classes_extra(void) {
    cx_class_t *m = 0, *c;
    c = pair_by_name("EKEventStore", &m);
    if (c && m) {
        if (!cx_class_named("EKEventStore")) cx_define_class(c, m, "EKEventStore", cx_class_named("NSObject"), (u32)sizeof(void *));
        cx_add_method(c, "init", (void *)ek_store_init);
        cx_add_cmethod(c, "authorizationStatusForEntityType:", (void *)ek_auth_status);
        cx_add_method(c, "requestAccessToEntityType:completion:", (void *)ek_request);
        cx_add_method(c, "requestFullAccessToEventsWithCompletion:", (void *)ek_request);
    }
    c = pair_by_name("SBApplication", &m);
    if (c && m && cx_class_named("SBApplication")) {
        cx_add_cmethod(c, "applicationWithBundleIdentifier:", (void *)sb_app_with);
        cx_add_cmethod(c, "applicationWithURL:", (void *)sb_app_with);
        cx_add_method(c, "isRunning", (void *)sb_is_running);
    }
    c = pair_by_name("OSLogStore", &m);
    if (c && m && !cx_class_named("OSLogStore")) {
        cx_define_class(c, m, "OSLogStore", cx_class_named("NSObject"), (u32)sizeof(void *));
        cx_add_cmethod(c, "storeWithScope:error:", (void *)osl_store);
        cx_add_cmethod(c, "localStoreAndReturnError:", (void *)osl_store);
        cx_add_method(c, "entriesEnumeratorWithOptions:position:predicate:error:", (void *)osl_entries);
    }
    c = pair_by_name("AVAudioApplication", &m);
    if (c && m && !cx_class_named("AVAudioApplication")) {
        cx_define_class(c, m, "AVAudioApplication", cx_class_named("NSObject"), (u32)sizeof(void *));
        cx_add_method(c, "dealloc", (void *)db_dealloc);
        cx_add_cmethod(c, "sharedInstance", (void *)ava_shared);
        cx_add_method(c, "isInputMuted", (void *)ava_input_muted);
        cx_add_method(c, "setInputMuted:error:", (void *)ava_set_muted);
    }
}
