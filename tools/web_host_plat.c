/* mOS — teste de host das APIs de plataforma (kernel/web/web_plat.c, web_crypto.c): structuredClone, MessageChannel,
 * crypto.getRandomValues/randomUUID e crypto.subtle com vetores oficiais (FIPS 180, RFC 2202, 4231, 6070, 5869,
 * 8032, NIST SP 800-38A) e idas e voltas de ECDSA, ECDH, X25519, AES-GCM, wrapKey e formatos jwk/spki/pkcs8. */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

typedef struct { doc_t *d; js_page_t *js; } pg2_t;
static void tick(pg2_t *g, int n) { for (int i = 0; i < n; i++) js_page_tick(g->js); }
static pg2_t open2(void) {
    pg2_t g;
    const char *html = "<html><body><script>var r=[]; function hex(b){return Array.from(new Uint8Array(b),function(x){return (x<16?'0':'')+x.toString(16)}).join('')}"
                       "function unhex(h){var a=new Uint8Array(h.length/2); for(var i=0;i<a.length;i++) a[i]=parseInt(h.substr(2*i,2),16); return a}"
                       "function b64u(a){var s=''; a.forEach(function(x){s+=String.fromCharCode(x)}); return btoa(s).replace(/\\+/g,'-').replace(/\\//g,'_').replace(/=+$/,'')}"
                       "var te=new TextEncoder(), S=crypto.subtle;</script></body></html>";
    g.d = doc_parse(html, strlen(html), "http://teste/p.html");
    g.js = js_page_new2(g.d, "http://teste/p.html", 300, 200, NULL, NULL);
    tick(&g, 5);
    return g;
}
static void close2(pg2_t *g) { js_page_free(g->js); doc_free(g->d); }
static const char *run(pg2_t *g, const char *code) {
    static sbuf_t o;
    if (o.p) sb_free(&o);
    memset(&o, 0, sizeof o);
    int t = js_page_eval_value(g->js, code, (int)strlen(code), &o);
    if (t == JS_WK_EXCEPTION) { static char b[700]; snprintf(b, sizeof b, "EXC:%s", o.p ? o.p : ""); return b; }
    return o.p ? o.p : "";
}
#define EQ(code, want) do { const char *r_ = run(&g, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)
/* roda código assíncrono que empurra em r e confere r.join('|') depois de alguns ticks */
#define ASYNC(code, want) do { run(&g, "r=[];"); run(&g, code); tick(&g, 6); EQ("r.join('|')", want); } while (0)

WEB_HOST_CASE(plat_structuredclone) {
    pg2_t g = open2();
    EQ("var o={a:1,d:new Date(5),re:/x+/gi,m:new Map([[1,{z:2}]]),s:new Set(['q']),u:new Uint16Array([1,2]),n:[1,,3],e:new TypeError('ops')}; o.self=o; o.u2=new Uint8Array(o.u.buffer,2);"
       "var c=structuredClone(o); [c!==o, c.self===c, c.d.getTime(), c.re.source+c.re.flags, c.m.get(1).z, c.s.has('q'), c.u[1], c.u2.buffer===c.u.buffer, c.u.buffer!==o.u.buffer,"
       " 1 in c.n, c.n.length, c.e instanceof TypeError, c.e.message].join()", "true,true,5,x+gi,2,true,2,true,true,false,3,true,ops");
    EQ("try{structuredClone(function(){})}catch(e){e.name}", "DataCloneError");
    EQ("try{structuredClone({s:Symbol()})}catch(e){e.name+':'+(e instanceof DOMException)}", "DataCloneError:true");
    EQ("try{structuredClone(document.body)}catch(e){e.name}", "DataCloneError");
    EQ("var b=new Blob(['xy'],{type:'a/b'}); var cb=structuredClone(b); cb!==b && cb.size+cb.type", "2a/b");
    EQ("var ab=new ArrayBuffer(8); var t=structuredClone(ab,{transfer:[ab]}); ab.byteLength+','+t.byteLength", "0,8");
    EQ("class K{constructor(){this.v=1}}; var ck=structuredClone(new K()); (ck instanceof K)+','+ck.v", "false,1");
    EQ("var w=structuredClone(new String('s')); typeof w+w", "objects");
    close2(&g);
}

WEB_HOST_CASE(plat_messagechannel) {
    pg2_t g = open2();
    ASYNC("var ch=new MessageChannel(); ch.port2.onmessage=function(e){r.push('2:'+e.data.k+':'+(e.data!==msg)+':'+(e instanceof MessageEvent))};"
          "var msg={k:'v'}; ch.port1.postMessage(msg); msg.k='mudou';"
          "ch.port1.addEventListener('message',function(e){r.push('1:'+e.data)}); ch.port2.postMessage('antes do start'); ch.port1.start();",
          "2:v:true:true|1:antes do start");
    ASYNC("var c2=new MessageChannel(); c2.port2.onmessage=function(e){r.push(e.data)}; c2.port1.close(); c2.port1.postMessage('perdida');"
          "r.push(c2.port1 instanceof MessagePort);", "true");
    EQ("try{new MessagePort()}catch(e){e instanceof TypeError}", "true");
    close2(&g);
}

WEB_HOST_CASE(plat_crypto_basico) {
    pg2_t g = open2();
    EQ("var u=crypto.getRandomValues(new Uint8Array(32)); u.some(function(x){return x})+','+(crypto.getRandomValues(u)===u)", "true,true");
    EQ("try{crypto.getRandomValues(new Float32Array(2))}catch(e){e.name}", "TypeMismatchError");
    EQ("try{crypto.getRandomValues(new Uint8Array(65537))}catch(e){e.name}", "QuotaExceededError");
    EQ("/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(crypto.randomUUID())+','+(crypto.randomUUID()!==crypto.randomUUID())", "true,true");
    ASYNC("var abc=te.encode('abc'); Promise.all(['SHA-1','SHA-256','sha-384','SHA-512'].map(function(h){return S.digest(h,abc)})).then(function(v){v.forEach(function(x){r.push(hex(x).slice(0,16))})});",
          "a9993e364706816a|ba7816bf8f01cfea|cb00753f45a35e8b|ddaf35a193617aba");
    ASYNC("S.digest('MD5',te.encode('x')).catch(function(e){r.push(e.name)})", "NotSupportedError");
    EQ("Object.prototype.toString.call(crypto.subtle)+' '+(crypto.subtle instanceof SubtleCrypto)", "[object SubtleCrypto] true");
    close2(&g);
}

WEB_HOST_CASE(plat_crypto_simetrico) {
    pg2_t g = open2();
    /* RFC 4231 caso 2 e RFC 2202 caso 2 */
    ASYNC("S.importKey('raw',te.encode('Jefe'),{name:'HMAC',hash:'SHA-256'},true,['sign','verify']).then(function(k){ r.push(k.type+':'+k.algorithm.name+':'+k.algorithm.hash.name+':'+k.algorithm.length+':'+k.usages.join('+'));"
          " return S.sign('HMAC',k,te.encode('what do ya want for nothing?')).then(function(s){ r.push(hex(s)); return S.verify('HMAC',k,s,te.encode('what do ya want for nothing?')) }).then(function(ok){ r.push(ok); return S.exportKey('jwk',k) })"
          " .then(function(j){ r.push(j.kty+':'+j.alg+':'+j.k) }) });",
          "secret:HMAC:SHA-256:32:sign+verify|5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843|true|oct:HS256:SmVmZQ");
    ASYNC("S.importKey('raw',te.encode('Jefe'),{name:'HMAC',hash:'SHA-1'},false,['sign']).then(function(k){ return S.sign('HMAC',k,te.encode('what do ya want for nothing?')) }).then(function(s){ r.push(hex(s)) })",
          "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
    /* RFC 6070 (PBKDF2-HMAC-SHA1), PBKDF2-SHA256 e RFC 5869 caso 1 (HKDF-SHA256) */
    ASYNC("S.importKey('raw',te.encode('password'),'PBKDF2',false,['deriveBits']).then(function(k){ return Promise.all([S.deriveBits({name:'PBKDF2',hash:'SHA-1',salt:te.encode('salt'),iterations:1},k,160),"
          " S.deriveBits({name:'PBKDF2',hash:'SHA-256',salt:te.encode('salt'),iterations:1},k,256)]) }).then(function(v){ r.push(hex(v[0])); r.push(hex(v[1])) })",
          "0c60c80f961f0e71f3a9b524af6012062fe037a6|120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    ASYNC("var ikm=new Uint8Array(22).fill(11); S.importKey('raw',ikm,'HKDF',false,['deriveBits']).then(function(k){ return S.deriveBits({name:'HKDF',hash:'SHA-256',salt:unhex('000102030405060708090a0b0c'),info:unhex('f0f1f2f3f4f5f6f7f8f9')},k,336) })"
          ".then(function(b){ r.push(hex(b)) })",
          "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865");
    /* AES-GCM (vetor do zero) e AES-CTR (NIST SP 800-38A F.5.1) */
    ASYNC("S.importKey('raw',new Uint8Array(16),'AES-GCM',false,['encrypt','decrypt']).then(function(k){ return S.encrypt({name:'AES-GCM',iv:new Uint8Array(12)},k,new Uint8Array(0)).then(function(c){ r.push(hex(c));"
          " return S.encrypt({name:'AES-GCM',iv:new Uint8Array(12),additionalData:te.encode('ad')},k,te.encode('segredo')) }).then(function(c){ return S.decrypt({name:'AES-GCM',iv:new Uint8Array(12),additionalData:te.encode('ad')},k,c)"
          " .then(function(p){ r.push(new TextDecoder().decode(p)); var bad=new Uint8Array(c); bad[0]^=1; return S.decrypt({name:'AES-GCM',iv:new Uint8Array(12),additionalData:te.encode('ad')},k,bad) }) }) }).catch(function(e){ r.push(e.name) })",
          "58e2fccefa7e3061367f1d57a4e7455a|segredo|OperationError");
    ASYNC("S.importKey('raw',unhex('2b7e151628aed2a6abf7158809cf4f3c'),{name:'AES-CTR'},false,['encrypt']).then(function(k){ return S.encrypt({name:'AES-CTR',counter:unhex('f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff'),length:64},k,"
          " unhex('6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51')) }).then(function(c){ r.push(hex(c)) })",
          "874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff");
    /* geração, deriveKey, wrapKey/unwrapKey e erros */
    ASYNC("S.generateKey({name:'AES-GCM',length:256},true,['encrypt','decrypt','wrapKey','unwrapKey']).then(function(wk){ return S.generateKey({name:'HMAC',hash:'SHA-512'},true,['sign']).then(function(hk){ r.push('hmac:'+hk.algorithm.length);"
          " var iv=crypto.getRandomValues(new Uint8Array(12)); return S.wrapKey('jwk',hk,wk,{name:'AES-GCM',iv:iv}).then(function(w){ return S.unwrapKey('jwk',w,wk,{name:'AES-GCM',iv:iv},{name:'HMAC',hash:'SHA-512'},true,['sign']) })"
          " .then(function(k2){ return Promise.all([S.exportKey('raw',hk),S.exportKey('raw',k2)]) }).then(function(v){ r.push('embrulho:'+(hex(v[0])===hex(v[1]))) }) }) });"
          "S.importKey('raw',te.encode('pw'),'PBKDF2',false,['deriveKey']).then(function(pk){ return S.deriveKey({name:'PBKDF2',hash:'SHA-256',salt:te.encode('s'),iterations:2},pk,{name:'AES-GCM',length:128},true,['encrypt']) })"
          ".then(function(dk){ r.push('derivada:'+dk.algorithm.name+dk.algorithm.length) });",
          "hmac:1024|derivada:AES-GCM128|embrulho:true");
    ASYNC("S.importKey('raw',new Uint8Array(24),'AES-GCM',false,['encrypt']).catch(function(e){r.push(e.name)});"
          "S.generateKey({name:'AES-GCM',length:128},false,['sign']).catch(function(e){r.push(e.name)});"
          "S.importKey('raw',new Uint8Array(16),'AES-GCM',false,['encrypt']).then(function(k){ return S.exportKey('raw',k) }).catch(function(e){r.push(e.name)});"
          "S.importKey('raw',te.encode('k'),{name:'HMAC',hash:'SHA-256'},false,['verify']).then(function(k){ return S.sign('HMAC',k,te.encode('x')) }).catch(function(e){r.push(e.name)});",
          "NotSupportedError|SyntaxError|InvalidAccessError|InvalidAccessError");
    close2(&g);
}

WEB_HOST_CASE(plat_crypto_assimetrico) {
    pg2_t g = open2();
    /* RFC 8032 teste 1 (Ed25519, mensagem vazia) por JWK */
    ASYNC("var d=unhex('9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60'), x=unhex('d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a');"
          "S.importKey('jwk',{kty:'OKP',crv:'Ed25519',d:b64u(d),x:b64u(x)},'Ed25519',true,['sign']).then(function(k){ return S.sign('Ed25519',k,new Uint8Array(0)) }).then(function(s){ r.push(hex(s).slice(0,32)+'..'+hex(s).slice(-16));"
          " return S.importKey('raw',x,'Ed25519',true,['verify']).then(function(pk){ return Promise.all([S.verify('Ed25519',pk,s,new Uint8Array(0)), S.verify('Ed25519',pk,s,new Uint8Array(1)), S.exportKey('spki',pk)]) }) })"
          ".then(function(v){ r.push(v[0]+','+v[1]+','+hex(v[2]).slice(0,24)) })",
          "e5564300c360ac729086e2cc806e828a..655141438e7a100b|true,false,302a300506032b6570032100");
    /* ECDSA P-256 e P-384: gera, assina, verifica, exporta e reimporta (spki, pkcs8, jwk) */
    ASYNC("['P-256','P-384'].forEach(function(c){ S.generateKey({name:'ECDSA',namedCurve:c},true,['sign','verify']).then(function(kp){ var m=te.encode('mensagem');"
          " return S.sign({name:'ECDSA',hash:'SHA-256'},kp.privateKey,m).then(function(s){ return Promise.all([S.verify({name:'ECDSA',hash:'SHA-256'},kp.publicKey,s,m), S.verify({name:'ECDSA',hash:'SHA-256'},kp.publicKey,s,te.encode('outra')),"
          "  S.exportKey('spki',kp.publicKey).then(function(sp){ return S.importKey('spki',sp,{name:'ECDSA',namedCurve:c},true,['verify']) }).then(function(pk2){ return S.verify({name:'ECDSA',hash:'SHA-256'},pk2,s,m) }),"
          "  S.exportKey('pkcs8',kp.privateKey).then(function(p8){ return S.importKey('pkcs8',p8,{name:'ECDSA',namedCurve:c},true,['sign']) }).then(function(k3){ return S.exportKey('jwk',k3) }).then(function(j){ return j.crv+':'+!!j.d }),"
          "  s.byteLength]) }) }).then(function(v){ r.push(c+':'+v.join()) }) });",
          "P-256:true,false,true,P-256:true,64|P-384:true,false,true,P-384:true,96");
    /* ECDH e X25519: os dois lados chegam ao mesmo segredo */
    ASYNC("Promise.all([S.generateKey({name:'ECDH',namedCurve:'P-256'},false,['deriveBits']),S.generateKey({name:'ECDH',namedCurve:'P-256'},false,['deriveBits'])]).then(function(p){"
          " return Promise.all([S.deriveBits({name:'ECDH',public:p[1].publicKey},p[0].privateKey,256),S.deriveBits({name:'ECDH',public:p[0].publicKey},p[1].privateKey,256)]) }).then(function(s){ r.push('ecdh:'+(hex(s[0])===hex(s[1]))+':'+s[0].byteLength) });"
          "Promise.all([S.generateKey('X25519',true,['deriveKey','deriveBits']),S.generateKey('X25519',true,['deriveBits'])]).then(function(p){"
          " return Promise.all([S.deriveBits({name:'X25519',public:p[1].publicKey},p[0].privateKey,null),S.deriveBits({name:'X25519',public:p[0].publicKey},p[1].privateKey,null),"
          "  S.deriveKey({name:'X25519',public:p[1].publicKey},p[0].privateKey,{name:'AES-GCM',length:256},false,['encrypt'])]) }).then(function(s){ r.push('x25519:'+(hex(s[0])===hex(s[1]))+':'+s[2].algorithm.length) });",
          "ecdh:true:32|x25519:true:256");
    /* RSA: verificação PKCS#1 v1.5 e PSS de assinaturas geradas pelo OpenSSL (chave de teste de 1024 bits) */
    ASYNC("var spki=unhex('30819f300d06092a864886f70d010101050003818d0030818902818100a53ea5f12f6b3d7500167eba61c929bdd415d3aa686b0b512e814a8045e57b95c566e7b3381fffff144c8f0ea9e9c82297945610dd6b722f06b56bf85d6c41728f59fe7bc5dffd983be14fa98ba9db872b68c6b4ced5b42f8f36a342bc9eec6f184887081bdaafa9ac6a417a48dd2cdb9a9bc96bd7627fcd4434fb57aee6b09f0203010001'), m=te.encode('mensagem');"
          "Promise.all([S.importKey('spki',spki,{name:'RSASSA-PKCS1-v1_5',hash:'SHA-256'},true,['verify']),S.importKey('spki',spki,{name:'RSA-PSS',hash:'SHA-256'},true,['verify'])]).then(function(k){"
          " return Promise.all([S.verify('RSASSA-PKCS1-v1_5',k[0],unhex('39a47dc61efa9b8613a9115c772f6baf3973ea24ccd4c5ff7c7213cdeb89f05761010b5ade9df3905c26e335bdf74d1ea0eb7344b9e81c7a3b6ee688bf2431317e2a9bb7876dcf4a21ab965476a7321cd6ee20bbaa689983ca7880e8c7aec1981aad59576a418baa74bd5c4a9b26907c358e02e00c2fbddac3ef72dacaead408'),m), S.verify('RSASSA-PKCS1-v1_5',k[0],unhex('39a47dc61efa9b8613a9115c772f6baf3973ea24ccd4c5ff7c7213cdeb89f05761010b5ade9df3905c26e335bdf74d1ea0eb7344b9e81c7a3b6ee688bf2431317e2a9bb7876dcf4a21ab965476a7321cd6ee20bbaa689983ca7880e8c7aec1981aad59576a418baa74bd5c4a9b26907c358e02e00c2fbddac3ef72dacaead408'),te.encode('x')),"
          "  S.verify({name:'RSA-PSS',saltLength:32},k[1],unhex('842f76a68d0e2361dde077102ea9f6f0547d0c34a2c1ce0b676cb3a3f077d6ccfd3a410a6c2fca4541cb0ee35355f5455767bf18f9f3143e4c206637d4688eaa8cee555dc6a85daef89e190d57335ac50302d31f81795cf909d1dfde25da8a5bcc9edb00536895a8eaffcb139486ed114b13ae314a4a3fa6df51cb4760407433'),m), k[0].algorithm.modulusLength, S.exportKey('jwk',k[0]).then(function(j){return j.kty+j.e})]) }).then(function(v){ r.push(v.join()) })",
          "true,false,true,1024,RSAAQAB");
    /* RSA: só verificação; geração recusada com mensagem clara */
    ASYNC("S.generateKey({name:'RSASSA-PKCS1-v1_5',modulusLength:2048,publicExponent:new Uint8Array([1,0,1]),hash:'SHA-256'},true,['sign','verify']).catch(function(e){r.push(e.name)})", "NotSupportedError");
    close2(&g);
}
