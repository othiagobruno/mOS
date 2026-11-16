/* mOS — teste de host de ArrayBuffer, typed arrays, DataView e TextEncoder/TextDecoder (kernel/web/js_typed.c). */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

static const char *run(js_page_t *p, const char *code) {
    static sbuf_t o;
    if (o.p) sb_free(&o);
    memset(&o, 0, sizeof o);
    int t = js_page_eval_value(p, code, (int)strlen(code), &o);
    if (t == JS_WK_EXCEPTION) { static char b[600]; snprintf(b, sizeof b, "EXC:%s", o.p ? o.p : ""); return b; }
    return o.p ? o.p : "";
}

#define EQ(code, want) do { const char *r_ = run(p, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)

WEB_HOST_CASE(typed_arrays) {
    doc_t *d = doc_parse("<html></html>", 13, "http://teste/t.html");
    js_page_t *p = js_page_new2(d, "http://teste/t.html", 100, 100, NULL, NULL);
    EQ("var u=new Uint8Array(4); u[0]=257; u[1]=-1; u[2]=3.9; u[9]=5; [u[0],u[1],u[2],u[3],u[9],u.length].join()", "1,255,3,0,,4");
    EQ("var c=new Uint8ClampedArray([300,-5,1.5,2.5,0.5]); Array.from(c).join()", "255,0,2,2,0");
    EQ("var i16=new Int16Array([40000,-40000]); i16.join()", "-25536,25536");
    EQ("var f=new Float32Array([1.1]); f[0]===Math.fround(1.1)", "true");
    EQ("new Float64Array([NaN,Infinity])[0]!==new Float64Array([NaN])[0]", "true");
    EQ("var b=new ArrayBuffer(8); var a=new Uint32Array(b); a[0]=0x11223344; var v=new Uint8Array(b); [v[0],v[3],b.byteLength].join()", "68,17,8");
    EQ("var s=new Uint8Array([1,2,3,4,5]); var sub=s.subarray(1,4); sub[0]=99; [s[1], sub.length, sub.byteOffset].join()", "99,3,1");
    EQ("var sl=new Uint8Array([1,2,3]).slice(-2); sl[0]=7; Array.from(sl).join()", "7,3");
    EQ("var m=new Int8Array([1,2,3]).map(function(x){return x*100}); m instanceof Int8Array && m.join()", "100,-56,44");
    EQ("Array.from(new Int32Array([5,1,4,-2]).sort()).join()", "-2,1,4,5");
    EQ("new Float64Array([3,NaN,1]).sort().join()", "1,3,NaN");
    EQ("var t=new Uint16Array(3); t.set([7,8],1); t.join()", "0,7,8");
    EQ("var t2=new Uint8Array(4); t2.fill(9,1,3); t2.join() + '|' + t2.reverse().join()", "0,9,9,0|0,9,9,0");
    EQ("[...new Uint8Array([4,5])].join() + ' ' + Object.keys(new Uint8Array(3)).join()", "4,5 0,1,2");
    EQ("var e=[]; for (var [k,v] of new Int8Array([7,8]).entries()) e.push(k+':'+v); e.join()", "0:7,1:8");
    EQ("new Uint8Array([1,2,3]).indexOf(2) + ',' + new Uint8Array([1,2,3]).includes(3) + ',' + new Uint8Array([1,2,3]).reduce(function(a,b){return a+b})", "1,true,6");
    EQ("Object.prototype.toString.call(new Float32Array(1)) + ' ' + new Uint8Array(2).toString()", "[object Float32Array] 0,0");
    EQ("Uint8Array.BYTES_PER_ELEMENT + Float64Array.BYTES_PER_ELEMENT + new Int16Array(1).BYTES_PER_ELEMENT", "11");
    EQ("Int16Array.of(1,2).join() + '/' + Uint8Array.from([1,2,3], function(x){return x*2}).join()", "1,2/2,4,6");
    EQ("var big=new BigInt64Array(2); big[0]=-5n; big[1]=2n**63n; String(big[0]) + ',' + String(big[1])", "-5,-9223372036854775808");
    EQ("var ub=new BigUint64Array([-1n]); String(ub[0])", "18446744073709551615");
    EQ("try { new BigInt64Array(1)[0] = 1; 'não' } catch (e) { e instanceof TypeError }", "true");
    EQ("var dv=new DataView(new ArrayBuffer(8)); dv.setUint16(0, 0x1234); dv.setUint16(2, 0x1234, true); dv.setFloat32(4, 1.5);"
       "[dv.getUint8(0), dv.getUint8(2), dv.getFloat32(4), dv.getInt16(0, true), dv.byteLength].join()", "18,52,1.5,13330,8");
    EQ("try { new DataView(new ArrayBuffer(2)).getUint32(0); 'não' } catch (e) { e instanceof RangeError }", "true");
    EQ("ArrayBuffer.isView(new Uint8Array(1)) && ArrayBuffer.isView(new DataView(new ArrayBuffer(1))) && !ArrayBuffer.isView([])", "true");
    EQ("var ab=new ArrayBuffer(6); new Uint8Array(ab).set([1,2,3,4,5,6]); Array.from(new Uint8Array(ab.slice(2,-1))).join()", "3,4,5");
    EQ("Array.from(new TextEncoder().encode('aé€😀')).join()", "97,195,169,226,130,172,240,159,152,128");
    EQ("new TextDecoder().decode(new TextEncoder().encode('Olá, 世界 😀'))", "Olá, 世界 😀");
    EQ("new TextDecoder().decode(new Uint8Array([0xEF,0xBB,0xBF,0x41,0xFF,0x42,0xE2,0x82]))", "A\xEF\xBF\xBD" "B\xEF\xBF\xBD");
    EQ("try { new TextDecoder('utf-8', {fatal:true}).decode(new Uint8Array([0xC0])); 'não' } catch (e) { e instanceof TypeError }", "true");
    EQ("JSON.stringify(new Uint8Array([1,2]))", "{\"0\":1,\"1\":2}");
    EQ("try { new Uint32Array(new ArrayBuffer(6)); 'não' } catch (e) { e instanceof RangeError }", "true");
    EQ("var x=new Uint8Array(1e6); x[999999]=1; x.length + x[999999]", "1000001");
    js_page_free(p);
    doc_free(d);
}
