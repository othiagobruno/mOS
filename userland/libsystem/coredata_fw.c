/* Core Data no libSystem.
 *
 * O Electron (Slack, Claude Desktop, Cursor) liga CoreData.framework e não importa
 * símbolo nenhum: o Mantle só tem o LC_LOAD_DYLIB. As classes abaixo existem para
 * o runtime (NSClassFromString) e para um app que caia no libSystem porque a
 * dylib não está no disco. São fachada (cx_mark_stub_class): se o CoreData.framework
 * do disco carregar, a classe de lá ganha a busca por nome.
 *
 * Persistência: coredata_store.inc grava SQLite (sqlite3_open_v2 do amalgama).
 * Arquivo MOSCD1 antigo é migrado na primeira abertura.
 */
#include "foundation.h"
#include "libc.h"
#include "coredata_store.inc"

void cx_mark_stub_class(cx_class_t *c);
id objc_autorelease(id o);
id objc_retain(id o);
extern id NSHomeDirectory(void);
extern cx_class_t NSConstantString_c __asm__("__CFConstantStringClassReference");
extern cx_class_t NSNumber_c __asm__("OBJC_CLASS_$_NSNumber");

#define CDSTR(sym, lit) static nsstr_t sym##_o = { &NSConstantString_c, 0, (char *)lit, sizeof(lit) - 1 }; id sym = (id)&sym##_o;
CDSTR(NSSQLiteStoreType, "SQLite")
CDSTR(NSInMemoryStoreType, "InMemory")
CDSTR(NSBinaryStoreType, "Binary")
CDSTR(NSXMLStoreType, "XML")
CDSTR(NSInferMappingModelAutomaticallyOption, "NSInferMappingModelAutomaticallyOption")
CDSTR(NSMigratePersistentStoresAutomaticallyOption, "NSMigratePersistentStoresAutomaticallyOption")
CDSTR(NSReadOnlyPersistentStoreOption, "NSReadOnlyPersistentStoreOption")
CDSTR(NSPersistentStoreFileProtectionKey, "NSPersistentStoreFileProtectionKey")
CDSTR(NSSQLitePragmasOption, "NSSQLitePragmasOption")
CDSTR(NSManagedObjectContextDidSaveNotification, "NSManagedObjectContextDidSaveNotification")
CDSTR(NSInsertedObjectsKey, "inserted")
CDSTR(NSUpdatedObjectsKey, "updated")
CDSTR(NSDeletedObjectsKey, "deleted")
CDSTR(NSManagedObjectContextObjectsDidChangeNotification, "NSManagedObjectContextObjectsDidChangeNotification")

#define CDCLASS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); static cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
CDCLASS(NSEntityDescription)
CDCLASS(NSManagedObjectModel)
CDCLASS(NSPersistentStoreCoordinator)
CDCLASS(NSPersistentStore)
CDCLASS(NSPersistentStoreDescription)
CDCLASS(NSManagedObjectContext)
CDCLASS(NSManagedObject)
CDCLASS(NSFetchRequest)
CDCLASS(NSPersistentContainer)

typedef struct {
    id key;
    cd_store *st;
    unsigned long oid;
    char name[160];
    char entity[160];
    id pred;
    int changed, deleted;
    unsigned limit, offset, result_type;
    id coord, ctx, model, stores, view;
} cd_box;

static cd_box *boxes;
static int nboxes, cboxes;

static cd_box *box_of(id o) {
    for (int i = 0; i < nboxes; i++) if (boxes[i].key == o) return &boxes[i];
    return 0;
}
static cd_box *box_make(id o) {
    cd_box *b = box_of(o);
    if (b) return b;
    if (nboxes == cboxes) {
        int nc = cboxes ? cboxes * 2 : 16;
        cd_box *g = realloc(boxes, (size_t)nc * sizeof *g);
        if (!g) return 0;
        boxes = g;
        cboxes = nc;
    }
    b = &boxes[nboxes++];
    memset(b, 0, sizeof *b);
    b->key = o;
    return b;
}

static int responds(id o, const char *sel) {
    return o && cx_responds(o->isa, sel_registerName(sel));
}
static id cd_err(const char *msg) {
    id ui = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), (long)134060, ui);
}
static const char *url_path(id url) {
    if (!url) return 0;
    if (responds(url, "path")) {
        id p = MSG0(id, url, "path");
        if (p) return cx_cstr(p);
    }
    if (cx_is_str(url)) return cx_cstr(url);
    return 0;
}
static int memory_type(id type) {
    const char *t = type ? cx_cstr(type) : 0;
    return t && !strcmp(t, "InMemory");
}

static id ent_for_name(id cls, SEL c, id name, id ctx) {
    (void)c; (void)ctx;
    id e = cx_alloc_instance((cx_class_t *)cls);
    cd_box *b = box_make(e);
    if (b && name) snprintf(b->entity, sizeof b->entity, "%s", cx_cstr(name));
    return objc_autorelease(e);
}
static id ent_insert(id cls, SEL c, id name, id ctx) {
    id e = ent_for_name(cls, c, name, ctx);
    id obj = MSG(id, (id)&NSManagedObject_c, "initWithEntity:insertIntoManagedObjectContext:", e, ctx);
    return obj;
}
static id ent_name(id self, SEL c) { (void)c; cd_box *b = box_of(self); return b ? cx_str(b->entity) : 0; }

static id model_init(id self, SEL c) { (void)c; box_make(self); return self; }
static id model_init_url(id self, SEL c, id url) { (void)url; return model_init(self, c); }
static id model_merged(id cls, SEL c, id bundles) { (void)c; (void)bundles; return objc_autorelease(model_init(cx_alloc_instance((cx_class_t *)cls), 0)); }
static id model_entities(id self, SEL c) { (void)self; (void)c; return objc_autorelease(cx_arr_new(cx_class_named("NSMutableArray"))); }
static id model_by_name(id self, SEL c) { (void)self; (void)c; return objc_autorelease(cx_alloc_instance(cx_class_named("NSMutableDictionary"))); }

static cd_store *open_typed(id type, id url, id *err) {
    cd_store *st;
    if (memory_type(type) || !url) st = cd_open_memory();
    else {
        const char *p = url_path(url);
        st = (p && p[0]) ? cd_open_file(p) : cd_open_memory();
    }
    if (!st && err) *err = cd_err("não foi possível abrir a loja do Core Data");
    if (st && err) *err = 0;
    return st;
}
static id coord_init(id self, SEL c, id model) {
    (void)c;
    cd_box *b = box_make(self);
    if (b) { b->model = model; b->stores = cx_arr_new(cx_class_named("NSMutableArray")); }
    return self;
}
static id coord_add(id self, SEL c, id type, id conf, id url, id opts, id *err) {
    cd_box *b = box_of(self);
    id store;
    cd_store *st;
    (void)c; (void)conf; (void)opts;
    st = open_typed(type, url, err);
    if (!st) return 0;
    if (b) {
        if (b->st) cd_close(b->st);
        b->st = st;
    }
    store = cx_alloc_instance(&NSPersistentStore_c);
    box_make(store);
    if (b && b->stores) cx_arr_add(b->stores, store);
    return objc_autorelease(store);
}
static id coord_model(id self, SEL c) { (void)c; cd_box *b = box_of(self); return b ? b->model : 0; }
static id coord_stores(id self, SEL c) { (void)c; cd_box *b = box_of(self); return b ? b->stores : 0; }

static cd_store *ctx_store(id ctx) {
    cd_box *b = box_of(ctx);
    if (!b) return 0;
    if (b->coord) {
        cd_box *k = box_of(b->coord);
        if (k && k->st) return k->st;
    }
    return b->st;
}
static id ctx_init(id self, SEL c) { (void)c; box_make(self); return self; }
static id ctx_init_type(id self, SEL c, unsigned long ty) { (void)ty; return ctx_init(self, c); }
static void ctx_set_coord(id self, SEL c, id coord) {
    cd_box *b = box_make(self);
    (void)c;
    if (!b) return;
    b->coord = coord;
}
static id ctx_coord(id self, SEL c) { (void)c; cd_box *b = box_of(self); return b ? b->coord : 0; }
static void ctx_insert(id self, SEL c, id obj) {
    cd_box *b = box_make(self);
    cd_box *o = box_of(obj);
    (void)c;
    if (b) b->changed = 1;
    if (o) o->ctx = self;
}
static void ctx_delete(id self, SEL c, id obj) {
    cd_box *b = box_make(self);
    cd_box *o = box_of(obj);
    cd_store *st = ctx_store(self);
    (void)c;
    if (b) b->changed = 1;
    if (o && st) { cd_delete(st, o->oid); o->deleted = 1; }
}
static id mo_wrap(id ctx, cd_store *st, unsigned long oid) {
    id mo = cx_alloc_instance(&NSManagedObject_c);
    cd_box *o = box_make(mo);
    const char *en;
    if (!o) return mo;
    o->st = st;
    o->oid = oid;
    o->ctx = ctx;
    en = cd_entity_of(st, oid);
    if (en) snprintf(o->entity, sizeof o->entity, "%s", en);
    return mo;
}
static int pred_ok(id pred, id obj) {
    if (!pred) return 1;
    if (!responds(pred, "evaluateWithObject:")) return 0;
    return MSG(BOOL, pred, "evaluateWithObject:", obj) ? 1 : 0;
}
static id ctx_fetch(id self, SEL c, id req, id *err) {
    cd_store *st = ctx_store(self);
    cd_box *r = box_of(req);
    unsigned n = 0, i, seen = 0, kept = 0;
    unsigned long *ids;
    id arr;
    (void)c;
    if (err) *err = 0;
    arr = cx_arr_new(cx_class_named("NSMutableArray"));
    if (!st || !r) return objc_autorelease(arr);
    if (r->pred && !responds(r->pred, "evaluateWithObject:")) return objc_autorelease(arr);
    ids = cd_fetch(st, r->entity[0] ? r->entity : 0, 0, 0, 0, 0, &n);
    for (i = 0; i < n; i++) {
        id mo = mo_wrap(self, st, ids[i]);
        if (!pred_ok(r->pred, mo)) continue;
        if (seen++ < r->offset) continue;
        kept++;
        if (r->result_type != 4) cx_arr_add(arr, mo);
        if (r->limit && kept >= r->limit) break;
    }
    free(ids);
    if (r->result_type == 4) {
        id one = cx_arr_new(cx_class_named("NSMutableArray"));
        cx_arr_add(one, cx_num_i((long long)kept));
        return objc_autorelease(one);
    }
    return objc_autorelease(arr);
}
static unsigned long ctx_count(id self, SEL c, id req, id *err) {
    id arr = ctx_fetch(self, c, req, err);
    cd_box *r = box_of(req);
    if (r && r->result_type == 4) {
        id n = MSG(id, arr, "objectAtIndex:", (unsigned long)0);
        return n ? (unsigned long)MSG0(long long, n, "longLongValue") : 0;
    }
    return (unsigned long)MSG0(unsigned long, arr, "count");
}
static BOOL ctx_save(id self, SEL c, id *err) {
    cd_store *st = ctx_store(self);
    cd_box *b = box_of(self);
    (void)c;
    if (!st || cd_save(st)) {
        if (err) *err = cd_err("falha ao gravar a loja do Core Data");
        return NO;
    }
    if (b) b->changed = 0;
    if (err) *err = 0;
    cx_post(cx_note(NSManagedObjectContextDidSaveNotification, self, 0));
    return YES;
}
static BOOL ctx_changes(id self, SEL c) { (void)c; cd_box *b = box_of(self); return b && b->changed ? YES : NO; }
static void ctx_block_wait(id self, SEL c, void *blk) {
    typedef struct { void *isa; int flags; int reserved; void (*invoke)(void *); } blk_t;
    blk_t *b = blk;
    (void)self; (void)c;
    if (b && b->invoke) b->invoke(blk);
}
static void ctx_block(id self, SEL c, void *blk) { ctx_block_wait(self, c, blk); }
static void ctx_rollback(id self, SEL c) { (void)c; cd_box *b = box_of(self); if (b) b->changed = 0; }
static void ctx_reset(id self, SEL c) { ctx_rollback(self, c); }

static id mo_init(id self, SEL c, id entity, id ctx) {
    cd_box *e = box_of(entity);
    cd_box *o;
    cd_store *st;
    const char *name;
    (void)c;
    o = box_make(self);
    name = e && e->entity[0] ? e->entity : "Entity";
    if (o) snprintf(o->entity, sizeof o->entity, "%s", name);
    st = ctx_store(ctx);
    if (o && st) {
        o->st = st;
        o->oid = cd_insert(st, name);
        o->ctx = ctx;
    }
    if (ctx) ctx_insert(ctx, 0, self);
    return self;
}
static id mo_init_ctx(id self, SEL c, id ctx) {
    id e = ent_for_name((id)&NSEntityDescription_c, 0, cx_str("NSManagedObject"), ctx);
    return mo_init(self, c, e, ctx);
}
static id mo_entity_desc(id self, SEL c) {
    cd_box *o = box_of(self);
    (void)c;
    return ent_for_name((id)&NSEntityDescription_c, 0, o ? cx_str(o->entity) : 0, 0);
}
static id mo_ctx(id self, SEL c) { (void)c; cd_box *o = box_of(self); return o ? o->ctx : 0; }
static id mo_oid(id self, SEL c) { (void)self; (void)c; return self; }
static BOOL mo_ins(id self, SEL c) { (void)c; cd_box *o = box_of(self); return o && o->st ? YES : NO; }
static BOOL mo_del(id self, SEL c) { (void)c; cd_box *o = box_of(self); return o && o->deleted ? YES : NO; }
static BOOL mo_fault(id self, SEL c) { (void)self; (void)c; return NO; }
static id mo_get(id self, SEL c, id key) {
    cd_box *o = box_of(self);
    int kind;
    const char *k;
    (void)c;
    if (!o || !o->st || !key) return 0;
    k = cx_cstr(key);
    kind = cd_get_kind(o->st, o->oid, k);
    if (kind == 1) return objc_autorelease(cx_str(cd_get_str(o->st, o->oid, k)));
    if (kind == 2) return objc_autorelease(cx_num_i(cd_get_i64(o->st, o->oid, k)));
    if (kind == 3) return objc_autorelease(cx_num_d(cd_get_f64(o->st, o->oid, k)));
    return 0;
}
static void mo_set(id self, SEL c, id value, id key) {
    cd_box *o = box_of(self);
    const char *k;
    (void)c;
    if (!o || !o->st || !key) return;
    k = cx_cstr(key);
    if (!value) cd_set_str(o->st, o->oid, k, 0);
    else if (cx_is_str(value)) cd_set_str(o->st, o->oid, k, cx_cstr(value));
    else if (cx_is_subclass(value->isa, &NSNumber_c)) {
        double d = MSG0(double, value, "doubleValue");
        long long i = MSG0(long long, value, "longLongValue");
        if (d == (double)i) cd_set_i64(o->st, o->oid, k, i);
        else cd_set_f64(o->st, o->oid, k, d);
    } else cd_set_str(o->st, o->oid, k, cx_cstr(value));
    if (o->ctx) { cd_box *b = box_of(o->ctx); if (b) b->changed = 1; }
}
static void mo_touch(id self, SEL c, id key) { (void)self; (void)c; (void)key; }

static id fr_with(id cls, SEL c, id name) {
    id r = cx_alloc_instance((cx_class_t *)cls);
    cd_box *b = box_make(r);
    (void)c;
    if (b && name) snprintf(b->entity, sizeof b->entity, "%s", cx_cstr(name));
    return objc_autorelease(r);
}
static id fr_init(id self, SEL c, id name) {
    cd_box *b = box_make(self);
    (void)c;
    if (b && name) snprintf(b->entity, sizeof b->entity, "%s", cx_cstr(name));
    return self;
}
static id fr_entity_name(id self, SEL c) { (void)c; cd_box *b = box_of(self); return b && b->entity[0] ? objc_autorelease(cx_str(b->entity)) : 0; }
static void fr_set_entity_name(id self, SEL c, id name) {
    cd_box *b = box_make(self);
    (void)c;
    if (b) snprintf(b->entity, sizeof b->entity, "%s", name ? cx_cstr(name) : "");
}
static void fr_set_entity(id self, SEL c, id ent) {
    cd_box *e = box_of(ent);
    (void)c;
    fr_set_entity_name(self, 0, e ? cx_str(e->entity) : 0);
}
static id fr_entity(id self, SEL c) {
    cd_box *b = box_of(self);
    (void)c;
    return b ? ent_for_name((id)&NSEntityDescription_c, 0, cx_str(b->entity), 0) : 0;
}
static void fr_set_pred(id self, SEL c, id pred) {
    cd_box *b = box_make(self);
    (void)c;
    if (!b) return;
    b->pred = pred ? objc_retain(pred) : 0;
}
static void fr_limit(id self, SEL c, unsigned long n) { (void)c; cd_box *b = box_make(self); if (b) b->limit = (unsigned)n; }
static unsigned long fr_get_limit(id self, SEL c) { (void)c; cd_box *b = box_of(self); return b ? b->limit : 0; }
static void fr_offset(id self, SEL c, unsigned long n) { (void)c; cd_box *b = box_make(self); if (b) b->offset = (unsigned)n; }
static void fr_rtype(id self, SEL c, unsigned long n) { (void)c; cd_box *b = box_make(self); if (b) b->result_type = (unsigned)n; }
static id fr_copy(id self, SEL c, void *zone) {
    cd_box *b = box_of(self);
    id r = fr_with((id)&NSFetchRequest_c, c, b ? cx_str(b->entity) : 0);
    cd_box *n = box_of(r);
    (void)zone;
    if (b && n) { *n = *b; n->key = r; if (n->pred) objc_retain(n->pred); }
    return r;
}

static id pc_init_name(id self, SEL c, id name) {
    cd_box *b = box_make(self);
    (void)c;
    if (b && name) snprintf(b->name, sizeof b->name, "%s", cx_cstr(name));
    return self;
}
static id pc_init(id self, SEL c) { return pc_init_name(self, c, cx_str("Store")); }
static id pc_view(id self, SEL c) {
    cd_box *b = box_make(self);
    (void)c;
    if (!b) return 0;
    if (!b->view) {
        b->view = cx_alloc_instance(&NSManagedObjectContext_c);
        box_make(b->view);
        ctx_set_coord(b->view, 0, self);
        /* o container é o dono da loja; o contexto lê ctx_store pelo coord */
        { cd_box *v = box_of(b->view); if (v) v->coord = self; }
    }
    return b->view;
}
static id pc_model(id self, SEL c) {
    cd_box *b = box_make(self);
    (void)c;
    if (b && !b->model) b->model = model_init(cx_alloc_instance(&NSManagedObjectModel_c), 0);
    return b ? b->model : 0;
}
static id pc_coord(id self, SEL c) {
    cd_box *b = box_make(self);
    (void)c;
    if (!b) return 0;
    if (!b->coord) {
        b->coord = coord_init(cx_alloc_instance(&NSPersistentStoreCoordinator_c), 0, pc_model(self, 0));
        if (b->st) { cd_box *k = box_of(b->coord); if (k) k->st = b->st; }
    }
    return b->coord;
}
static void pc_load(id self, SEL c, void *blk) {
    cd_box *b = box_make(self);
    char path[1024];
    id home, desc, err = 0;
    typedef struct { void *isa; int flags; int reserved; void (*invoke)(void *, id, id); } blk_t;
    blk_t *bb = blk;
    (void)c;
    home = NSHomeDirectory();
    snprintf(path, sizeof path, "%s/Library/Application Support/%s/store.moscd", cx_cstr(home), b && b->name[0] ? b->name : "Store");
    if (b) {
        if (b->st) cd_close(b->st);
        b->st = cd_open_file(path);
        if (!b->st) err = cd_err("não foi possível abrir a loja do Core Data");
        if (b->coord) { cd_box *k = box_of(b->coord); if (k) k->st = b->st; }
    }
    desc = cx_alloc_instance(&NSPersistentStoreDescription_c);
    box_make(desc);
    if (bb && bb->invoke) bb->invoke(blk, desc, err);
}
static id desc_type(id self, SEL c) { (void)self; (void)c; return NSSQLiteStoreType; }

static void def(cx_class_t *c, cx_class_t *m, const char *name) {
    cx_define_class(c, m, name, cx_class_named("NSObject"), 0);
    cx_mark_stub_class(c);
}

void cx_coredata_init(void) {
    def(&NSEntityDescription_c, &NSEntityDescription_m, "NSEntityDescription");
    def(&NSManagedObjectModel_c, &NSManagedObjectModel_m, "NSManagedObjectModel");
    def(&NSPersistentStoreCoordinator_c, &NSPersistentStoreCoordinator_m, "NSPersistentStoreCoordinator");
    def(&NSPersistentStore_c, &NSPersistentStore_m, "NSPersistentStore");
    def(&NSPersistentStoreDescription_c, &NSPersistentStoreDescription_m, "NSPersistentStoreDescription");
    def(&NSManagedObjectContext_c, &NSManagedObjectContext_m, "NSManagedObjectContext");
    def(&NSManagedObject_c, &NSManagedObject_m, "NSManagedObject");
    def(&NSFetchRequest_c, &NSFetchRequest_m, "NSFetchRequest");
    def(&NSPersistentContainer_c, &NSPersistentContainer_m, "NSPersistentContainer");

    cx_add_cmethod(&NSEntityDescription_c, "entityForName:inManagedObjectContext:", (void *)ent_for_name);
    cx_add_cmethod(&NSEntityDescription_c, "insertNewObjectForEntityForName:inManagedObjectContext:", (void *)ent_insert);
    cx_add_method(&NSEntityDescription_c, "name", (void *)ent_name);
    cx_add_method(&NSManagedObjectModel_c, "init", (void *)model_init);
    cx_add_method(&NSManagedObjectModel_c, "initWithContentsOfURL:", (void *)model_init_url);
    cx_add_cmethod(&NSManagedObjectModel_c, "mergedModelFromBundles:", (void *)model_merged);
    cx_add_method(&NSManagedObjectModel_c, "entities", (void *)model_entities);
    cx_add_method(&NSManagedObjectModel_c, "entitiesByName", (void *)model_by_name);
    cx_add_method(&NSPersistentStoreCoordinator_c, "initWithManagedObjectModel:", (void *)coord_init);
    cx_add_method(&NSPersistentStoreCoordinator_c, "addPersistentStoreWithType:configuration:URL:options:error:", (void *)coord_add);
    cx_add_method(&NSPersistentStoreCoordinator_c, "managedObjectModel", (void *)coord_model);
    cx_add_method(&NSPersistentStoreCoordinator_c, "persistentStores", (void *)coord_stores);
    cx_add_method(&NSManagedObjectContext_c, "init", (void *)ctx_init);
    cx_add_method(&NSManagedObjectContext_c, "initWithConcurrencyType:", (void *)ctx_init_type);
    cx_add_method(&NSManagedObjectContext_c, "setPersistentStoreCoordinator:", (void *)ctx_set_coord);
    cx_add_method(&NSManagedObjectContext_c, "persistentStoreCoordinator", (void *)ctx_coord);
    cx_add_method(&NSManagedObjectContext_c, "insertObject:", (void *)ctx_insert);
    cx_add_method(&NSManagedObjectContext_c, "deleteObject:", (void *)ctx_delete);
    cx_add_method(&NSManagedObjectContext_c, "executeFetchRequest:error:", (void *)ctx_fetch);
    cx_add_method(&NSManagedObjectContext_c, "countForFetchRequest:error:", (void *)ctx_count);
    cx_add_method(&NSManagedObjectContext_c, "save:", (void *)ctx_save);
    cx_add_method(&NSManagedObjectContext_c, "hasChanges", (void *)ctx_changes);
    cx_add_method(&NSManagedObjectContext_c, "performBlock:", (void *)ctx_block);
    cx_add_method(&NSManagedObjectContext_c, "performBlockAndWait:", (void *)ctx_block_wait);
    cx_add_method(&NSManagedObjectContext_c, "rollback", (void *)ctx_rollback);
    cx_add_method(&NSManagedObjectContext_c, "reset", (void *)ctx_reset);
    cx_add_method(&NSManagedObject_c, "initWithEntity:insertIntoManagedObjectContext:", (void *)mo_init);
    cx_add_method(&NSManagedObject_c, "initWithContext:", (void *)mo_init_ctx);
    cx_add_method(&NSManagedObject_c, "entity", (void *)mo_entity_desc);
    cx_add_method(&NSManagedObject_c, "managedObjectContext", (void *)mo_ctx);
    cx_add_method(&NSManagedObject_c, "objectID", (void *)mo_oid);
    cx_add_method(&NSManagedObject_c, "isInserted", (void *)mo_ins);
    cx_add_method(&NSManagedObject_c, "isDeleted", (void *)mo_del);
    cx_add_method(&NSManagedObject_c, "isFault", (void *)mo_fault);
    cx_add_method(&NSManagedObject_c, "valueForKey:", (void *)mo_get);
    cx_add_method(&NSManagedObject_c, "setValue:forKey:", (void *)mo_set);
    cx_add_method(&NSManagedObject_c, "primitiveValueForKey:", (void *)mo_get);
    cx_add_method(&NSManagedObject_c, "setPrimitiveValue:forKey:", (void *)mo_set);
    cx_add_method(&NSManagedObject_c, "willAccessValueForKey:", (void *)mo_touch);
    cx_add_method(&NSManagedObject_c, "didAccessValueForKey:", (void *)mo_touch);
    cx_add_method(&NSManagedObject_c, "willChangeValueForKey:", (void *)mo_touch);
    cx_add_method(&NSManagedObject_c, "didChangeValueForKey:", (void *)mo_touch);
    cx_add_cmethod(&NSFetchRequest_c, "fetchRequestWithEntityName:", (void *)fr_with);
    cx_add_method(&NSFetchRequest_c, "initWithEntityName:", (void *)fr_init);
    cx_add_method(&NSFetchRequest_c, "entityName", (void *)fr_entity_name);
    cx_add_method(&NSFetchRequest_c, "setEntityName:", (void *)fr_set_entity_name);
    cx_add_method(&NSFetchRequest_c, "setEntity:", (void *)fr_set_entity);
    cx_add_method(&NSFetchRequest_c, "entity", (void *)fr_entity);
    cx_add_method(&NSFetchRequest_c, "setPredicate:", (void *)fr_set_pred);
    cx_add_method(&NSFetchRequest_c, "setFetchLimit:", (void *)fr_limit);
    cx_add_method(&NSFetchRequest_c, "fetchLimit", (void *)fr_get_limit);
    cx_add_method(&NSFetchRequest_c, "setFetchOffset:", (void *)fr_offset);
    cx_add_method(&NSFetchRequest_c, "setResultType:", (void *)fr_rtype);
    cx_add_method(&NSFetchRequest_c, "copyWithZone:", (void *)fr_copy);
    cx_add_method(&NSPersistentContainer_c, "init", (void *)pc_init);
    cx_add_method(&NSPersistentContainer_c, "initWithName:", (void *)pc_init_name);
    cx_add_method(&NSPersistentContainer_c, "viewContext", (void *)pc_view);
    cx_add_method(&NSPersistentContainer_c, "managedObjectModel", (void *)pc_model);
    cx_add_method(&NSPersistentContainer_c, "persistentStoreCoordinator", (void *)pc_coord);
    cx_add_method(&NSPersistentContainer_c, "loadPersistentStoresWithCompletionHandler:", (void *)pc_load);
    cx_add_method(&NSPersistentStoreDescription_c, "type", (void *)desc_type);
}
