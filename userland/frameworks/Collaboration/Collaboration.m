/*
 * mOS — Collaboration.framework (Mach-O, em /System/Library/Frameworks): identidades de usuário e grupo
 * (CBIdentity, CBUserIdentity, CBGroupIdentity) e as autoridades que as resolvem (CBIdentityAuthority).
 * O Setapp pergunta por elas para mostrar o nome do usuário.
 *
 * A base é o banco de usuários do libSystem (getpwnam/getpwuid, getgrnam/getgrgid): o mOS tem um usuário local e
 * os grupos do sistema. As autoridades local, gerenciada e padrão resolvem no mesmo banco (não há diretório de
 * rede). authenticateWithPassword: responde NO: o mOS não guarda senha para conferir (pw_passwd é "*").
 * tools/build-frameworks-objc.sh
 */
#import <AppKit/AppKit.h>
#include <string.h>

typedef struct {
    char *pw_name, *pw_passwd; unsigned pw_uid, pw_gid; long pw_change; char *pw_class, *pw_gecos, *pw_dir, *pw_shell; long pw_expire;
} cb_passwd_t;
typedef struct { char *gr_name, *gr_passwd; unsigned gr_gid; char **gr_mem; } cb_group_t;
cb_passwd_t *getpwnam(const char *n);
cb_passwd_t *getpwuid(unsigned uid);
cb_group_t *getgrnam(const char *n);
cb_group_t *getgrgid(unsigned gid);

enum { CBIdentityClassUser = 1, CBIdentityClassGroup = 2 };

@interface CBIdentityAuthority : NSObject { int _kind; }
+ (instancetype)localIdentityAuthority;
+ (instancetype)managedIdentityAuthority;
+ (instancetype)defaultIdentityAuthority;
+ (instancetype)identityAuthorityWithCSIdentityAuthority:(void *)csAuthority;
- (id)localizedName;
- (void *)CSIdentityAuthority;
@end

@interface CBIdentity : NSObject {
@public
    id _posixName, _fullName, _uuid;
    unsigned _id;
    CBIdentityAuthority *_authority;
}
+ (instancetype)identityWithName:(id)name authority:(CBIdentityAuthority *)authority;
+ (instancetype)identityWithUniqueIdentifier:(id)uuid authority:(CBIdentityAuthority *)authority;
+ (instancetype)identityWithPersistentReference:(id)data;
+ (instancetype)identityWithCSIdentity:(void *)csIdentity;
- (CBIdentityAuthority *)authority;
- (id)uniqueIdentifier;
- (id)UUIDString;
- (id)fullName;
- (id)posixName;
- (id)emailAddress;
- (id)image;
- (id)persistentReference;
- (BOOL)isHidden;
- (id)aliases;
- (BOOL)isMemberOfGroup:(CBIdentity *)group;
- (void *)CSIdentity;
@end

@interface CBUserIdentity : CBIdentity
- (unsigned)posixUID;
- (id)certificate;
- (BOOL)isEnabled;
- (BOOL)authenticateWithPassword:(id)password;
@end

@interface CBGroupIdentity : CBIdentity
- (unsigned)posixGID;
- (id)members;
@end

@implementation CBIdentityAuthority
static CBIdentityAuthority *auth_of(int kind) {
    static CBIdentityAuthority *a[3];
    if (!a[kind]) { a[kind] = [[CBIdentityAuthority alloc] init]; a[kind]->_kind = kind; }
    return a[kind];
}
+ (instancetype)localIdentityAuthority { return auth_of(0); }
+ (instancetype)managedIdentityAuthority { return auth_of(1); }
+ (instancetype)defaultIdentityAuthority { return auth_of(2); }
+ (instancetype)identityAuthorityWithCSIdentityAuthority:(void *)csAuthority { return auth_of(2); }
- (id)localizedName { return _kind == 1 ? @"Managed" : _kind == 0 ? @"Local" : @"Default"; }
- (void *)CSIdentityAuthority { return 0; }
@end

/* UUID estável por classe e id (o formato FFFFEEEE-DDDD-CCCC-BBBB-AAAAxxxxxxxx do opendirectoryd para usuários locais) */
static id uuid_for(int user, unsigned n) {
    return [NSString stringWithFormat:@"%@-DDDD-CCCC-BBBB-AAAA%08X", user ? @"FFFFEEEE" : @"ABCDEFAB-CDEF-ABCD-EFAB", n];
}

static CBIdentity *user_identity(cb_passwd_t *pw, CBIdentityAuthority *a) {
    if (!pw || !pw->pw_name) return nil;
    CBUserIdentity *u = [[[CBUserIdentity alloc] init] autorelease];
    u->_posixName = [[NSString alloc] initWithUTF8String:pw->pw_name];
    u->_fullName = [[NSString alloc] initWithUTF8String:pw->pw_gecos && *pw->pw_gecos ? pw->pw_gecos : pw->pw_name];
    u->_id = pw->pw_uid;
    u->_uuid = [uuid_for(1, pw->pw_uid) retain];
    u->_authority = a ? a : [CBIdentityAuthority defaultIdentityAuthority];
    return u;
}
static CBIdentity *group_identity(cb_group_t *gr, CBIdentityAuthority *a) {
    if (!gr || !gr->gr_name) return nil;
    CBGroupIdentity *g = [[[CBGroupIdentity alloc] init] autorelease];
    g->_posixName = [[NSString alloc] initWithUTF8String:gr->gr_name];
    g->_fullName = [g->_posixName retain];
    g->_id = gr->gr_gid;
    g->_uuid = [uuid_for(0, gr->gr_gid) retain];
    g->_authority = a ? a : [CBIdentityAuthority defaultIdentityAuthority];
    return g;
}

@implementation CBIdentity
+ (instancetype)identityWithName:(id)name authority:(CBIdentityAuthority *)authority {
    const char *n = name ? [name UTF8String] : 0;
    if (!n || !*n) return nil;
    /* o getpwnam do mOS responde pelo usuário único: confere o nome curto ou o completo */
    cb_passwd_t *pw = getpwnam(n);
    if (pw && pw->pw_name && (!strcmp(pw->pw_name, n) || (pw->pw_gecos && !strcmp(pw->pw_gecos, n))))
        return (id)user_identity(pw, authority);
    cb_group_t *gr = getgrnam(n);
    if (gr && gr->gr_name && !strcmp(gr->gr_name, n)) return (id)group_identity(gr, authority);
    return nil;
}
+ (instancetype)identityWithUniqueIdentifier:(id)uuid authority:(CBIdentityAuthority *)authority {
    const char *s = uuid ? [[uuid description] UTF8String] : 0;
    if (!s || strlen(s) != 36) return nil;
    unsigned n = (unsigned)strtoul(s + 28, 0, 16);
    if (!strncmp(s, "FFFFEEEE", 8)) {
        cb_passwd_t *pw = getpwuid(n);
        return pw && pw->pw_uid == n ? (id)user_identity(pw, authority) : nil;
    }
    cb_group_t *gr = getgrgid(n);
    return gr && gr->gr_gid == n ? (id)group_identity(gr, authority) : nil;
}
+ (instancetype)identityWithPersistentReference:(id)data {
    if (!data || ![data respondsToSelector:@selector(bytes)]) return nil;
    id s = [[[NSString alloc] initWithData:data encoding:4] autorelease];
    return [self identityWithUniqueIdentifier:s authority:nil];
}
+ (instancetype)identityWithCSIdentity:(void *)csIdentity { return nil; }   /* o mOS não tem CSIdentity */
- (void)dealloc { [_posixName release]; [_fullName release]; [_uuid release]; [super dealloc]; }
- (CBIdentityAuthority *)authority { return _authority; }
- (id)uniqueIdentifier { return [[[NSClassFromString(@"NSUUID") alloc] initWithUUIDString:_uuid] autorelease]; }
- (id)UUIDString { return _uuid; }
- (id)fullName { return _fullName; }
- (id)posixName { return _posixName; }
- (id)emailAddress { return nil; }
- (id)image { return nil; }
- (id)persistentReference { return [_uuid dataUsingEncoding:4]; }
- (BOOL)isHidden { return _id < 500 && _id != 0 ? YES : NO; }
- (id)aliases { return [NSArray array]; }
- (BOOL)isMemberOfGroup:(CBIdentity *)group {
    if (![group isKindOfClass:[CBGroupIdentity class]]) return NO;
    cb_group_t *gr = getgrgid(group->_id);
    if (!gr) return NO;
    if ([self isKindOfClass:[CBUserIdentity class]]) {
        cb_passwd_t *pw = getpwuid(_id);
        if (pw && pw->pw_gid == gr->gr_gid) return YES;
    }
    const char *me = [_posixName UTF8String];
    for (char **m = gr->gr_mem; m && *m; m++) if (me && !strcmp(*m, me)) return YES;
    return NO;
}
- (void *)CSIdentity { return 0; }
- (BOOL)isEqual:(id)o { return [o isKindOfClass:[CBIdentity class]] && [_uuid isEqual:((CBIdentity *)o)->_uuid]; }
- (NSUInteger)hash { return [_uuid hash]; }
@end

@implementation CBUserIdentity
- (unsigned)posixUID { return _id; }
- (id)certificate { return nil; }
- (BOOL)isEnabled { return YES; }
- (BOOL)authenticateWithPassword:(id)password { return NO; }
@end

@implementation CBGroupIdentity
- (unsigned)posixGID { return _id; }
- (id)members {
    NSMutableArray *r = [NSMutableArray array];
    cb_group_t *gr = getgrgid(_id);
    for (char **m = gr ? gr->gr_mem : 0; m && *m; m++) {
        CBIdentity *u = [CBIdentity identityWithName:[NSString stringWithUTF8String:*m] authority:_authority];
        if (u) [r addObject:u];
    }
    return r;
}
@end
