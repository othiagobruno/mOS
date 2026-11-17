/* mOS — Service Workers e Cache API do motor web (implementação própria a partir da especificação). Roda em dois
 * lados: na janela (navigator.serviceWorker, ServiceWorkerRegistration, ServiceWorker, fetch passando pelo worker que
 * controla a página) e no worker (ServiceWorkerGlobalScope: install/activate/fetch/message, clients, skipWaiting,
 * registration). caches existe nos dois. O registro, o ciclo de vida, a ponte entre as páginas e o armazenamento do
 * cache ficam em web_sw.c (H). Embutido em web_sw_js.h por tools/gen_sw_js.py. */
(function (G, H, isWorker) {
  'use strict';
  var P = Promise;
  function typeError(m) { return new TypeError(m); }
  function domErr(name, m) { try { return new DOMException(m, name); } catch (e) { var x = new Error(m); x.name = name; return x; } }
  function tag(f, n) {
    Object.defineProperty(f.prototype, 'constructor', { value: f, writable: true, configurable: true });
    if (typeof Symbol === 'function' && Symbol.toStringTag) Object.defineProperty(f.prototype, Symbol.toStringTag, { value: n, configurable: true });
  }
  /* alvo de eventos simples (on<tipo> + addEventListener) */
  function Emitter() {}
  Emitter.prototype = {
    addEventListener: function (t, f) { if (!f) return; var l = this.__l || (this.__l = {}); (l[t] || (l[t] = [])).indexOf(f) < 0 && l[t].push(f); },
    removeEventListener: function (t, f) { var l = this.__l && this.__l[t]; if (l) { var i = l.indexOf(f); if (i >= 0) l.splice(i, 1); } },
    dispatchEvent: function (e) {
      var h = this['on' + e.type];
      try { Object.defineProperty(e, 'target', { value: this, configurable: true }); Object.defineProperty(e, 'currentTarget', { value: this, configurable: true }); } catch (x) {}
      if (typeof h === 'function') h.call(this, e);
      var l = this.__l && this.__l[e.type];
      if (l) l.slice().forEach(function (f) { typeof f === 'function' ? f.call(this, e) : f.handleEvent(e); }, this);
      return true;
    }
  };
  function mkEvent(type, props) {
    var e = new Event(type);
    for (var k in props) Object.defineProperty(e, k, { value: props[k], configurable: true, enumerable: true });
    return e;
  }
  function headerList(h) { var out = []; h.forEach(function (v, k) { out.push([k, v]); }); return out; }
  function reqUrl(r) { return r instanceof Request ? r.url : new URL(String(r), G.location ? G.location.href : undefined).href; }

  /* ============================================================ Cache API */
  function respData(res) {
    return res.arrayBuffer().then(function (b) {
      return { url: res.url, status: res.status, statusText: res.statusText, headers: headerList(res.headers), body: b, type: res.type };
    });
  }
  function respFrom(d) {
    var r = new Response(d.status === 204 || d.status === 304 ? null : d.body, { status: d.status, statusText: d.statusText, headers: d.headers });
    try { Object.defineProperty(r, 'url', { value: d.url, configurable: true }); } catch (e) {}
    return r;
  }
  function stripSearch(u, ignore) { if (!ignore) return u; var x = new URL(u); x.search = ''; return x.href; }
  function Cache(k, name) { if (k !== Cache) throw typeError('Construtor ilegal'); this.__n = name; }
  Cache.prototype = {
    match: function (req, opt) { return this.matchAll(req, opt).then(function (l) { return l[0]; }); },
    matchAll: function (req, opt) {
      var n = this.__n, ig = !!(opt && opt.ignoreSearch);
      return P.resolve().then(function () {
        var keys = req === undefined ? H.eKeys(n) : [reqUrl(req)];
        var out = [];
        if (req !== undefined && ig) {
          var want = stripSearch(keys[0], true);
          keys = H.eKeys(n).filter(function (k) { return stripSearch(k, true) === want; });
        }
        if (req instanceof Request && req.method !== 'GET' && !(opt && opt.ignoreMethod)) return out;
        keys.forEach(function (k) { var d = H.eGet(n, k); if (d) out.push(respFrom(d)); });
        return out;
      });
    },
    put: function (req, res) {
      var n = this.__n;
      return P.resolve().then(function () {
        if (req instanceof Request && req.method !== 'GET') throw typeError('Cache.put: só requisições GET');
        if (!(res instanceof Response)) throw typeError('Cache.put: é preciso um Response');
        if (res.status === 206) throw typeError('Cache.put: resposta parcial (206) não entra no cache');
        if (res.bodyUsed) throw typeError('Cache.put: o corpo do Response já foi lido');
        var u = reqUrl(req);
        if (!/^https?:/.test(u)) throw typeError('Cache.put: só http(s)');
        return respData(res).then(function (d) { H.ePut(n, u, d); });
      });
    },
    add: function (req) { return this.addAll([req]); },
    addAll: function (reqs) {
      var self = this;
      return P.all(Array.prototype.map.call(reqs, function (r) {
        return fetch(r).then(function (res) {
          if (!res.ok) throw typeError('Cache.addAll: resposta ' + res.status + ' para ' + res.url);
          return [r, res];
        });
      })).then(function (pairs) { return P.all(pairs.map(function (p) { return self.put(p[0], p[1]); })); }).then(function () {});
    },
    delete: function (req, opt) {
      var n = this.__n;
      return P.resolve().then(function () {
        var u = reqUrl(req), ig = !!(opt && opt.ignoreSearch), hit = false;
        H.eKeys(n).forEach(function (k) { if (stripSearch(k, ig) === stripSearch(u, ig)) { H.eDel(n, k); hit = true; } });
        return hit;
      });
    },
    keys: function (req) {
      var n = this.__n;
      return P.resolve().then(function () {
        var l = H.eKeys(n);
        if (req !== undefined) { var u = reqUrl(req); l = l.filter(function (k) { return k === u; }); }
        return l.map(function (k) { return new Request(k); });
      });
    }
  };
  tag(Cache, 'Cache');
  function CacheStorage(k) { if (k !== CacheStorage) throw typeError('Construtor ilegal'); }
  CacheStorage.prototype = {
    open: function (name) { name = String(name); return P.resolve().then(function () { H.cOpen(name); return new Cache(Cache, name); }); },
    has: function (name) { name = String(name); return P.resolve().then(function () { return H.cHas(name); }); },
    delete: function (name) { name = String(name); return P.resolve().then(function () { return H.cDelete(name); }); },
    keys: function () { return P.resolve().then(function () { return H.cKeys(); }); },
    match: function (req, opt) {
      if (opt && opt.cacheName) return this.has(opt.cacheName).then(function (h) { return h ? new Cache(Cache, String(opt.cacheName)).match(req, opt) : undefined; });
      var names = H.cKeys();
      return names.reduce(function (p, n) {
        return p.then(function (r) { return r || new Cache(Cache, n).match(req, opt); });
      }, P.resolve(undefined));
    }
  };
  tag(CacheStorage, 'CacheStorage');
  var caches = new CacheStorage(CacheStorage);

  var tags = { Cache: Cache, CacheStorage: CacheStorage, caches: caches };

  /* ============================================================ lado da janela */
  if (!isWorker) {
    var workers = {}, regs = {}, container, pending = {};
    function ServiceWorker(k, rid, kind) {
      if (k !== ServiceWorker) throw typeError('Construtor ilegal');
      this.__r = rid; this.__k = kind;
      var info = H.info(rid);
      this.scriptURL = info ? info.script : '';
      this.state = info ? info.state : 'redundant';
      this.onstatechange = null; this.onerror = null;
    }
    ServiceWorker.prototype = Object.create(Emitter.prototype);
    ServiceWorker.prototype.postMessage = function (msg) {
      if (arguments.length < 1) throw typeError('postMessage: é necessário 1 argumento');
      H.postToWorker(this.__r, structuredClone(msg));
    };
    tag(ServiceWorker, 'ServiceWorker');
    function workerFor(rid) { return workers[rid] || (workers[rid] = new ServiceWorker(ServiceWorker, rid)); }

    function ServiceWorkerRegistration(k, rid) {
      if (k !== ServiceWorkerRegistration) throw typeError('Construtor ilegal');
      this.__r = rid;
      this.onupdatefound = null;
      sync(this);
    }
    ServiceWorkerRegistration.prototype = Object.create(Emitter.prototype);
    ServiceWorkerRegistration.prototype.unregister = function () { var r = this.__r; return P.resolve().then(function () { return H.unregister(r); }); };
    ServiceWorkerRegistration.prototype.update = function () { var self = this; return P.resolve().then(function () { H.update(self.__r); return self; }); };
    tag(ServiceWorkerRegistration, 'ServiceWorkerRegistration');
    function sync(reg) {
      var i = H.info(reg.__r) || { scope: reg.scope || '', state: 'redundant' };
      reg.scope = i.scope;
      var w = workerFor(reg.__r);
      reg.installing = i.state === 'installing' ? w : null;
      reg.waiting = i.state === 'installed' ? w : null;
      reg.active = i.state === 'activating' || i.state === 'activated' ? w : null;
      reg.updateViaCache = 'imports';
      reg.navigationPreload = { enable: function () { return P.resolve(); }, disable: function () { return P.resolve(); }, getState: function () { return P.resolve({ enabled: false, headerValue: 'true' }); } };
    }
    function regFor(rid) { return regs[rid] || (regs[rid] = new ServiceWorkerRegistration(ServiceWorkerRegistration, rid)); }

    function ServiceWorkerContainer(k) {
      if (k !== ServiceWorkerContainer) throw typeError('Construtor ilegal');
      var self = this, readyP;
      this.oncontrollerchange = null; this.onmessage = null; this.onmessageerror = null;
      this.__ready = new P(function (res) { readyP = res; });
      this.__readyRes = readyP;
      Object.defineProperty(this, 'controller', { get: function () { var c = H.controller(); return c < 0 ? null : workerFor(c); }, configurable: true });
      Object.defineProperty(this, 'ready', { get: function () { return self.__ready; }, configurable: true });
      checkReady(this);
    }
    ServiceWorkerContainer.prototype = Object.create(Emitter.prototype);
    ServiceWorkerContainer.prototype.register = function (url, opt) {
      var self = this;
      return P.resolve().then(function () {
        var base = G.location.href, su = new URL(String(url), base).href;
        var scope = opt && opt.scope !== undefined ? new URL(String(opt.scope), base).href : new URL('./', su).href;
        if (opt && opt.type === 'module') throw typeError('register: workers de módulo ainda não são aceitos neste motor');
        var rid = H.register(su, scope);
        var reg = regFor(rid);
        sync(reg);
        checkReady(self);
        return reg;
      });
    };
    ServiceWorkerContainer.prototype.getRegistration = function (url) {
      return P.resolve().then(function () {
        var u = new URL(url === undefined ? '' : String(url), G.location.href).href, best = null, bl = -1;
        H.list().forEach(function (rid) { var i = H.info(rid); if (i && u.indexOf(i.scope) === 0 && i.scope.length > bl) { best = rid; bl = i.scope.length; } });
        return best === null ? undefined : regFor(best);
      });
    };
    ServiceWorkerContainer.prototype.getRegistrations = function () { return P.resolve().then(function () { return H.list().map(regFor); }); };
    ServiceWorkerContainer.prototype.startMessages = function () {};
    tag(ServiceWorkerContainer, 'ServiceWorkerContainer');
    function checkReady(c) {
      if (c.__readyDone) return;
      var u = G.location.href;
      H.list().forEach(function (rid) {
        var i = H.info(rid);
        if (!c.__readyDone && i && u.indexOf(i.scope) === 0 && (i.state === 'activating' || i.state === 'activated')) { c.__readyDone = true; c.__readyRes(regFor(rid)); }
      });
    }
    function getContainer() { return container || (container = new ServiceWorkerContainer(ServiceWorkerContainer)); }

    /* fetch da página controlada passa pelo evento fetch do worker */
    var origFetch = G.fetch;
    function swFetch(input, init) {
      var ctl = H.controller();
      var req;
      try { req = new Request(input, init); } catch (e) { return P.reject(e); }
      if (ctl < 0 || !/^https?:/.test(req.url)) return origFetch.call(G, req);
      var backup = req.clone();
      var body = req.method === 'GET' || req.method === 'HEAD' ? P.resolve(null) : req.arrayBuffer();
      return body.then(function (b) {
        return new P(function (res, rej) {
          var pid = H.fetch(ctl, { url: req.url, method: req.method, headers: headerList(req.headers), body: b, mode: req.mode,
                                   credentials: req.credentials, cache: req.cache, redirect: req.redirect, referrer: req.referrer });
          pending[pid] = { res: res, rej: rej, req: backup };
        });
      });
    }
    G.fetch = swFetch;

    /* entradas que web_sw.c entrega a esta janela */
    G['\x01swin'] = function (kind, a, b) {
      if (kind === 'resp') {
        var p = pending[a];
        if (!p) return;
        delete pending[a];
        if (b === null) origFetch.call(G, p.req).then(p.res, p.rej);                 /* sem respondWith: vai para a rede */
        else if (typeof b === 'string') p.rej(typeError('fetch: ' + b));             /* respondWith rejeitado ou inválido */
        else p.res(respFrom(b));
      } else if (kind === 'state') {
        var reg = regs[a], w = workers[a], i = H.info(a);
        var old = w ? w.state : null;
        if (w && i) w.state = i.state;
        if (reg) { var hadInstalling = !!reg.installing; sync(reg); if (!hadInstalling && reg.installing) reg.dispatchEvent(mkEvent('updatefound', {})); }
        if (w && old !== w.state) w.dispatchEvent(mkEvent('statechange', {}));
        if (container) checkReady(container);
      } else if (kind === 'controller') {
        if (container) container.dispatchEvent(mkEvent('controllerchange', {}));
      } else if (kind === 'message') {
        var c = getContainer();
        c.dispatchEvent(mkEvent('message', { data: a, origin: H.origin(), source: H.controller() < 0 ? null : workerFor(H.controller()), ports: [], lastEventId: '' }));
      }
    };
    tags.ServiceWorker = ServiceWorker;
    tags.ServiceWorkerRegistration = ServiceWorkerRegistration;
    tags.ServiceWorkerContainer = ServiceWorkerContainer;
    tags.container = getContainer;
    return tags;
  }

  /* ============================================================ lado do worker */
  function ExtendableEvent(type, init) {
    var e = new Event(type, init), list = [];
    Object.defineProperty(e, 'waitUntil', { value: function (p) { list.push(P.resolve(p)); }, configurable: true });
    Object.defineProperty(e, '__wait', { value: list });
    return e;
  }
  function Client(d) { this.id = d.id; this.url = d.url; this.type = 'window'; this.frameType = 'top-level'; }
  Client.prototype.postMessage = function (msg) { H.postToClient(this.id, structuredClone(msg)); };
  tag(Client, 'Client');
  var clients = {
    get: function (id) { return P.resolve().then(function () { var f = H.clients().filter(function (c) { return c.id === id; })[0]; return f ? new Client(f) : undefined; }); },
    matchAll: function (opt) {
      return P.resolve().then(function () {
        var all = !!(opt && opt.includeUncontrolled);
        return H.clients().filter(function (c) { return all || c.controlled; }).map(function (c) { return new Client(c); });
      });
    },
    claim: function () { return P.resolve().then(function () { H.claim(); }); },
    openWindow: function () { return P.reject(domErr('InvalidAccessError', 'clients.openWindow precisa de um gesto do usuário')); }
  };
  G.clients = clients;
  G.skipWaiting = function () { return P.resolve().then(function () { H.skipWaiting(); }); };
  G.registration = { get scope() { return H.scope(); }, unregister: function () { return P.resolve(H.unregisterSelf()); },
                     update: function () { return P.resolve(); } };
  G.oninstall = null; G.onactivate = null; G.onfetch = null; G.onmessage = null;

  function lifecycle(type) {
    var e = ExtendableEvent(type);
    var ok = true;
    try { G.dispatchEvent(e); } catch (x) { ok = false; }
    P.all(e.__wait).then(function () { H.done(type, ok); }, function () { H.done(type, false); });
  }
  G['\x01swin'] = function (kind, a, b) {
    if (kind === 'install' || kind === 'activate') { lifecycle(kind); return; }
    if (kind === 'message') {
      var me = ExtendableEvent('message');
      Object.defineProperty(me, 'data', { value: a }); Object.defineProperty(me, 'source', { value: b ? new Client(b) : null });
      Object.defineProperty(me, 'origin', { value: H.origin() }); Object.defineProperty(me, 'ports', { value: [] });
      G.dispatchEvent(me);
      return;
    }
    if (kind === 'fetch') {
      var fid = a, d = b, responded = false, sent = false;
      var req = new Request(d.url, { method: d.method, headers: d.headers, body: d.body && d.method !== 'GET' && d.method !== 'HEAD' ? d.body : undefined,
                                     mode: d.mode === 'navigate' ? 'same-origin' : d.mode, credentials: d.credentials, cache: d.cache, redirect: d.redirect });
      var fe = ExtendableEvent('fetch');
      Object.defineProperty(fe, 'request', { value: req });
      Object.defineProperty(fe, 'clientId', { value: '' });
      Object.defineProperty(fe, 'resultingClientId', { value: '' });
      Object.defineProperty(fe, 'preloadResponse', { value: P.resolve(undefined) });
      Object.defineProperty(fe, 'respondWith', { value: function (r) {
        if (responded) throw domErr('InvalidStateError', 'respondWith já foi chamado');
        responded = true;
        P.resolve(r).then(function (res) {
          if (!(res instanceof Response)) throw typeError('respondWith precisa de um Response');
          return respData(res);
        }).then(function (data) { sent = true; H.respond(fid, data); }, function (err) { if (!sent) H.respond(fid, String(err && err.message || err)); });
      } });
      try { G.dispatchEvent(fe); } catch (x) {}
      if (!responded) H.respond(fid, null);
    }
  };
  tags.Client = Client;
  tags.ExtendableEvent = ExtendableEvent;
  return tags;
})
