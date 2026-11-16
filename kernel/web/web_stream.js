/* mOS — WHATWG Streams do motor web (implementação própria a partir da especificação): ReadableStream (padrão e
 * bytes, leitores padrão e BYOB, tee, pipeTo/pipeThrough, iteração assíncrona, from), WritableStream, TransformStream,
 * estratégias de fila, TextEncoderStream, TextDecoderStream, CompressionStream e DecompressionStream (o deflate é nativo:
 * web_zlib.c, recebido em Z). Embutido em web_stream_js.h por tools/gen_stream_js.py;
 * avaliado na primeira vez que a página usa um destes nomes. */
(function (G, Z) {
  'use strict';
  var P = Promise, resolved = function (v) { return P.resolve(v); }, rejected = function (e) { return P.reject(e); };
  var noop = function () {};
  function defer() { var d = {}; d.promise = new P(function (a, b) { d.resolve = a; d.reject = b; }); return d; }
  function handled(p) { p.then(noop, noop); return p; }
  function typeError(m) { return new TypeError(m); }
  function call(o, name, args, fallback) {
    var f = o == null ? undefined : o[name];
    if (f === undefined) return fallback;
    if (typeof f !== 'function') throw typeError(name + ' precisa ser uma função');
    return f.apply(o, args);
  }
  function promiseCall(o, name, args) {
    try { return resolved(call(o, name, args, undefined)); } catch (e) { return rejected(e); }
  }
  function strategyOf(s, defaultHWM) {
    s = s || {};
    var hwm = s.highWaterMark === undefined ? defaultHWM : Number(s.highWaterMark);
    if (hwm !== hwm || hwm < 0) throw new RangeError('highWaterMark inválido');
    var size = s.size;
    if (size !== undefined && typeof size !== 'function') throw typeError('size precisa ser uma função');
    return { hwm: hwm, size: size ? function (c) { return size(c); } : function () { return 1; } };
  }
  function abortErr() { try { return new DOMException('A operação foi abortada.', 'AbortError'); } catch (e) { var x = new Error('A operação foi abortada.'); x.name = 'AbortError'; return x; } }
  var K = typeof Symbol === 'function' ? Symbol('interno') : '__mos_stream';

  /* ------------------------------------------------------------ estratégias */
  function CountQueuingStrategy(init) { if (!init || init.highWaterMark === undefined) throw typeError('highWaterMark obrigatório'); this.highWaterMark = Number(init.highWaterMark); }
  CountQueuingStrategy.prototype.size = function () { return 1; };
  function ByteLengthQueuingStrategy(init) { if (!init || init.highWaterMark === undefined) throw typeError('highWaterMark obrigatório'); this.highWaterMark = Number(init.highWaterMark); }
  ByteLengthQueuingStrategy.prototype.size = function (chunk) { return chunk.byteLength; };

  /* ============================================================ ReadableStream */
  function ReadableStream(source, strategy) {
    if (!(this instanceof ReadableStream)) throw typeError("O construtor ReadableStream exige 'new'");
    source = source === undefined || source === null ? {} : source;
    var s = this[K] = { state: 'readable', reader: null, stored: undefined, disturbed: false, ctrl: null };
    var bytes = source.type !== undefined && String(source.type) === 'bytes';
    if (source.type !== undefined && !bytes) throw typeError("type precisa ser 'bytes' ou ausente");
    var st = strategyOf(strategy, bytes ? 0 : 1);
    if (bytes && strategy && strategy.size !== undefined) throw new RangeError('stream de bytes não aceita size');
    var c = s.ctrl = bytes ? new ReadableByteStreamController(K) : new ReadableStreamDefaultController(K);
    var ci = c[K] = { stream: this, queue: [], qsize: 0, hwm: st.hwm, size: st.size, started: false, pulling: false, pullAgain: false,
                      closeRequested: false, source: source, bytes: bytes, byob: [], autoAlloc: source.autoAllocateChunkSize };
    var r;
    try { r = call(source, 'start', [c], undefined); } catch (e) { throw e; }
    resolved(r).then(function () { ci.started = true; rsPullIfNeeded(ci); }, function (e) { rsError(ci, e); });
  }
  function rs(stream) { var s = stream && stream[K]; if (!s || s.ctrl === undefined) throw typeError('não é um ReadableStream'); return s; }
  function rsDesired(ci) {
    var s = ci.stream[K];
    if (s.state === 'errored') return null;
    if (s.state === 'closed') return 0;
    return ci.hwm - ci.qsize;
  }
  function rsShouldPull(ci) {
    var s = ci.stream[K];
    if (s.state !== 'readable' || ci.closeRequested || !ci.started) return false;
    var r = s.reader;
    if (r && r.reqs.length > 0) return true;
    return rsDesired(ci) > 0;
  }
  function rsPullIfNeeded(ci) {
    if (!rsShouldPull(ci)) return;
    if (ci.pulling) { ci.pullAgain = true; return; }
    ci.pulling = true;
    promiseCall(ci.source, 'pull', [ci.stream[K].ctrl]).then(function () {
      ci.pulling = false;
      if (ci.pullAgain) { ci.pullAgain = false; rsPullIfNeeded(ci); }
    }, function (e) { rsError(ci, e); });
  }
  function rsClose(stream) {
    var s = stream[K];
    s.state = 'closed';
    var r = s.reader;
    if (!r) return;
    if (r.reqs) { var q = r.reqs; r.reqs = []; q.forEach(function (d) { d.resolve(d.byob ? { value: d.view ? new d.view.constructor(d.view.buffer, d.view.byteOffset, 0) : undefined, done: true } : { value: undefined, done: true }); }); }
    r.closed.resolve(undefined);
  }
  function rsError(ci, e) {
    var s = ci.stream[K];
    if (s.state !== 'readable') return;
    ci.queue = []; ci.qsize = 0;
    s.state = 'errored'; s.stored = e;
    var r = s.reader;
    if (!r) return;
    var q = r.reqs; r.reqs = [];
    q.forEach(function (d) { d.reject(e); });
    handled(r.closed.promise);
    r.closed.reject(e);
  }
  function rsCancel(stream, reason) {
    var s = stream[K];
    s.disturbed = true;
    if (s.state === 'closed') return resolved(undefined);
    if (s.state === 'errored') return rejected(s.stored);
    rsClose(stream);
    var ci = s.ctrl[K];
    ci.queue = []; ci.qsize = 0;
    return promiseCall(ci.source, 'cancel', [reason]).then(function () { return undefined; });
  }
  /* entrega um pedaço: para um leitor esperando ou para a fila */
  function rsEnqueue(ci, chunk) {
    var s = ci.stream[K];
    var r = s.reader;
    if (r && r.reqs && r.reqs.length && !r.reqs[0].byob) { r.reqs.shift().resolve({ value: chunk, done: false }); }
    else if (r && r.reqs && r.reqs.length && r.reqs[0].byob) { byobFill(ci, chunk); }
    else {
      var size;
      try { size = ci.bytes ? chunk.byteLength : ci.size(chunk); } catch (e) { rsError(ci, e); throw e; }
      if (size !== size || size < 0 || size === Infinity) { var er = new RangeError('tamanho de pedaço inválido'); rsError(ci, er); throw er; }
      ci.queue.push({ v: chunk, s: size }); ci.qsize += size;
    }
    rsPullIfNeeded(ci);
  }

  function ReadableStreamDefaultController(k) { if (k !== K) throw typeError('Construtor ilegal'); }
  ReadableStreamDefaultController.prototype = {
    get desiredSize() { return rsDesired(this[K]); },
    close: function () {
      var ci = this[K];
      if (ci.closeRequested || ci.stream[K].state !== 'readable') throw typeError('o stream não pode ser fechado');
      ci.closeRequested = true;
      if (!ci.queue.length) rsClose(ci.stream);
    },
    enqueue: function (chunk) {
      var ci = this[K];
      if (ci.closeRequested || ci.stream[K].state !== 'readable') throw typeError('o stream não aceita mais pedaços');
      rsEnqueue(ci, chunk);
    },
    error: function (e) { rsError(this[K], e); }
  };

  /* bytes: os pedaços são views; o leitor BYOB recebe na view que trouxe */
  function ReadableByteStreamController(k) { if (k !== K) throw typeError('Construtor ilegal'); }
  ReadableByteStreamController.prototype = {
    get desiredSize() { return rsDesired(this[K]); },
    get byobRequest() {
      var ci = this[K], r = ci.stream[K].reader;
      if (!r || !r.reqs.length || !r.reqs[0].byob) return null;
      var d = r.reqs[0];
      return { view: d.view, respond: function (n) { byobRespond(ci, d, n); }, respondWithNewView: function (v) { d.view = v; byobRespond(ci, d, v.byteLength); } };
    },
    close: ReadableStreamDefaultController.prototype.close,
    enqueue: function (chunk) {
      if (!ArrayBuffer.isView(chunk)) throw typeError('enqueue precisa de uma view de bytes');
      if (chunk.byteLength === 0) throw typeError('pedaço vazio');
      var ci = this[K];
      if (ci.closeRequested || ci.stream[K].state !== 'readable') throw typeError('o stream não aceita mais pedaços');
      /* cópia: o stream é dono dos bytes */
      rsEnqueue(ci, new Uint8Array(chunk.buffer.slice(chunk.byteOffset, chunk.byteOffset + chunk.byteLength)));
    },
    error: function (e) { rsError(this[K], e); }
  };
  function byobRespond(ci, d, n) {
    var r = ci.stream[K].reader;
    var i = r.reqs.indexOf(d);
    if (i >= 0) r.reqs.splice(i, 1);
    d.resolve({ value: new d.view.constructor(d.view.buffer, d.view.byteOffset, n / (d.view.BYTES_PER_ELEMENT || 1)), done: false });
    rsPullIfNeeded(ci);
  }
  function byobFill(ci, chunk) {
    var r = ci.stream[K].reader, d = r.reqs.shift();
    var dst = new Uint8Array(d.view.buffer, d.view.byteOffset, d.view.byteLength);
    var src = chunk instanceof Uint8Array ? chunk : new Uint8Array(chunk.buffer, chunk.byteOffset, chunk.byteLength);
    var n = Math.min(dst.length, src.length);
    dst.set(src.subarray(0, n));
    if (n < src.length) { var rest = src.slice(n); ci.queue.unshift({ v: rest, s: rest.byteLength }); ci.qsize += rest.byteLength; }
    var el = d.view.BYTES_PER_ELEMENT || 1;
    d.resolve({ value: new d.view.constructor(d.view.buffer, d.view.byteOffset, Math.floor(n / el)), done: false });
  }

  function ReadableStreamDefaultReader(stream) {
    var s = rs(stream);
    if (s.reader) throw typeError('o stream já tem um leitor');
    var r = this[K] = { stream: stream, reqs: [], closed: defer() };
    s.reader = r;
    if (s.state === 'closed') r.closed.resolve(undefined);
    else if (s.state === 'errored') { handled(r.closed.promise); r.closed.reject(s.stored); }
  }
  function readerRead(r, byobView) {
    if (!r.stream) return rejected(typeError('leitor liberado'));
    var s = r.stream[K];
    s.disturbed = true;
    var ci = s.ctrl[K];
    if (s.state === 'errored') return rejected(s.stored);
    if (ci.queue.length) {
      var e = ci.queue.shift();
      ci.qsize -= e.s;
      if (byobView) {
        var d = defer(); d.byob = true; d.view = byobView;
        r.reqs.unshift(d);
        byobFill(ci, e.v);
      } else d = { promise: resolved({ value: e.v, done: false }) };
      if (ci.closeRequested && !ci.queue.length) rsClose(r.stream); else rsPullIfNeeded(ci);
      return d.promise;
    }
    if (s.state === 'closed') return resolved(byobView ? { value: new byobView.constructor(byobView.buffer, byobView.byteOffset, 0), done: true } : { value: undefined, done: true });
    var dd = defer();
    if (byobView) { dd.byob = true; dd.view = byobView; }
    r.reqs.push(dd);
    rsPullIfNeeded(ci);
    return dd.promise;
  }
  function readerRelease(r) {
    if (!r.stream) return;
    var s = r.stream[K];
    var e = typeError('o leitor foi liberado');
    r.reqs.forEach(function (d) { d.reject(e); }); r.reqs = [];
    if (s.state === 'readable') { handled(r.closed.promise); r.closed.reject(e); r.closed = defer(); handled(r.closed.promise); r.closed.reject(e); }
    s.reader = null; r.stream = null;
  }
  ReadableStreamDefaultReader.prototype = {
    get closed() { return this[K].closed.promise; },
    read: function () { return readerRead(this[K]); },
    releaseLock: function () { readerRelease(this[K]); },
    cancel: function (reason) { var r = this[K]; return r.stream ? rsCancel(r.stream, reason) : rejected(typeError('leitor liberado')); }
  };
  function ReadableStreamBYOBReader(stream) {
    var s = rs(stream);
    if (!s.ctrl[K].bytes) throw typeError('leitor BYOB só para streams de bytes');
    ReadableStreamDefaultReader.call(this, stream);
  }
  ReadableStreamBYOBReader.prototype = {
    get closed() { return this[K].closed.promise; },
    read: function (view) {
      if (!ArrayBuffer.isView(view) || view.byteLength === 0) return rejected(typeError('read precisa de uma view não vazia'));
      return readerRead(this[K], view);
    },
    releaseLock: function () { readerRelease(this[K]); },
    cancel: ReadableStreamDefaultReader.prototype.cancel
  };

  ReadableStream.prototype = {
    get locked() { return !!rs(this).reader; },
    cancel: function (reason) {
      var s = rs(this);
      if (s.reader) return rejected(typeError('o stream está travado por um leitor'));
      return rsCancel(this, reason);
    },
    getReader: function (opts) {
      var mode = opts && opts.mode;
      if (mode === undefined) return new ReadableStreamDefaultReader(this);
      if (String(mode) === 'byob') return new ReadableStreamBYOBReader(this);
      throw typeError("mode precisa ser 'byob'");
    },
    tee: function () {
      var src = this, reader = this.getReader(), canceled = [false, false], reasons = [], reading = false, done = false;
      var cancelAll = defer();
      var branches = [];
      function pull() {
        if (reading || done) return resolved();
        reading = true;
        return reader.read().then(function (r) {
          reading = false;
          if (r.done) { done = true; branches.forEach(function (b, i) { if (!canceled[i]) b[K].ctrl.close(); }); return; }
          branches.forEach(function (b, i) {
            if (canceled[i]) return;
            var v = r.value;
            if (i === 1 && b[K].ctrl[K].bytes) v = new Uint8Array(v);
            b[K].ctrl.enqueue(v);
          });
        }, function (e) { reading = false; done = true; branches.forEach(function (b) { b[K].ctrl.error(e); }); });
      }
      function mk(i) {
        return new ReadableStream({
          type: src[K].ctrl[K].bytes ? 'bytes' : undefined,
          pull: pull,
          cancel: function (reason) {
            canceled[i] = true; reasons[i] = reason;
            if (canceled[0] && canceled[1]) cancelAll.resolve(rsCancel(src, reasons));
            return cancelAll.promise;
          }
        }, { highWaterMark: 0 });
      }
      branches = [mk(0), mk(1)];
      reader.closed.then(noop, function (e) { branches.forEach(function (b) { try { b[K].ctrl.error(e); } catch (x) {} }); });
      return branches;
    },
    pipeTo: function (dest, opts) {
      var src = this;
      opts = opts || {};
      if (!(dest instanceof WritableStream)) return rejected(typeError('pipeTo precisa de um WritableStream'));
      if (rs(src).reader) return rejected(typeError('o stream de origem está travado'));
      if (ws(dest).writer) return rejected(typeError('o stream de destino está travado'));
      var preventClose = !!opts.preventClose, preventAbort = !!opts.preventAbort, preventCancel = !!opts.preventCancel, signal = opts.signal;
      var reader = src.getReader(), writer = dest.getWriter();
      var result = defer(), finished = false, shutting = false, pending = resolved(), currentRead = resolved();
      /* desliga depois que a leitura em curso foi escrita e as escritas pendentes terminaram */
      function settle() { return currentRead.then(function () { return pending; }).then(noop, noop); }
      function finish(err, isErr) {
        if (finished) return;
        finished = true;
        settle().then(function () {
          reader.releaseLock();
          writer.releaseLock();
          if (signal) signal.removeEventListener('abort', onAbort);
          if (isErr) result.reject(err); else result.resolve(undefined);
        });
      }
      function shutdown(action, err, isErr) {
        if (shutting) return;
        shutting = true;
        settle().then(function () { return action ? action() : undefined; })
          .then(function () { finish(err, isErr); }, function (x) { finish(x, true); });
      }
      function onAbort() {
        var e = signal.reason !== undefined ? signal.reason : abortErr();
        shutdown(function () {
          var acts = [];
          if (!preventAbort) acts.push(writer.abort(e));
          if (!preventCancel) acts.push(reader.cancel(e));
          return P.all(acts);
        }, e, true);
      }
      if (signal) {
        if (signal.aborted) { onAbort(); return result.promise; }
        signal.addEventListener('abort', onAbort);
      }
      reader.closed.then(function () {
        shutdown(preventClose ? null : function () { return writer.close(); }, undefined, false);
      }, function (e) {
        shutdown(preventAbort ? null : function () { return writer.abort(e); }, e, true);
      });
      writer.closed.then(noop, function (e) {
        shutdown(preventCancel ? null : function () { return reader.cancel(e); }, e, true);
      });
      (function loop() {
        if (shutting) return;
        writer.ready.then(function () {
          if (shutting) return;
          currentRead = reader.read().then(function (r) {
            if (r.done) return;
            pending = writer.write(r.value);
            handled(pending);
          });
          return currentRead.then(loop);
        }, noop).then(null, noop);
      })();
      return result.promise;
    },
    pipeThrough: function (pair, opts) {
      if (!pair || !(pair.writable instanceof WritableStream) || !(pair.readable instanceof ReadableStream)) throw typeError('pipeThrough precisa de {writable, readable}');
      if (rs(this).reader) throw typeError('o stream está travado');
      handled(this.pipeTo(pair.writable, opts));
      return pair.readable;
    },
    values: function (opts) {
      var reader = this.getReader(), preventCancel = !!(opts && opts.preventCancel), ended = false;
      var it = {
        next: function () {
          if (ended) return resolved({ value: undefined, done: true });
          return reader.read().then(function (r) { if (r.done) { ended = true; reader.releaseLock(); } return r; });
        },
        'return': function (v) {
          ended = true;
          if (!reader[K].stream) return resolved({ value: v, done: true });
          var p = preventCancel ? resolved() : reader.cancel(v);
          reader.releaseLock();   /* a especificação solta o leitor na hora, sem esperar o cancel */
          return p.then(function () { return { value: v, done: true }; });
        }
      };
      if (typeof Symbol === 'function' && Symbol.asyncIterator) it[Symbol.asyncIterator] = function () { return this; };
      return it;
    }
  };
  if (typeof Symbol === 'function' && Symbol.asyncIterator) ReadableStream.prototype[Symbol.asyncIterator] = ReadableStream.prototype.values;
  ReadableStream.from = function (iterable) {
    var it;
    if (iterable != null && typeof Symbol === 'function' && Symbol.asyncIterator && typeof iterable[Symbol.asyncIterator] === 'function') it = iterable[Symbol.asyncIterator]();
    else if (iterable != null && typeof iterable[Symbol.iterator] === 'function') it = iterable[Symbol.iterator]();
    else throw typeError('ReadableStream.from precisa de um iterável');
    return new ReadableStream({
      pull: function (c) {
        return resolved(it.next()).then(function (r) { if (r.done) c.close(); else return resolved(r.value).then(function (v) { c.enqueue(v); }); });
      },
      cancel: function (reason) { return typeof it['return'] === 'function' ? resolved(it['return'](reason)).then(noop) : undefined; }
    }, { highWaterMark: 0 });
  };

  /* ============================================================ WritableStream */
  function WritableStream(sink, strategy) {
    if (!(this instanceof WritableStream)) throw typeError("O construtor WritableStream exige 'new'");
    sink = sink === undefined || sink === null ? {} : sink;
    if (sink.type !== undefined) throw new RangeError('type não é suportado em WritableStream');
    var st = strategyOf(strategy, 1);
    var w = this[K] = { state: 'writable', stored: undefined, writer: null, queue: [], qsize: 0, hwm: st.hwm, size: st.size, sink: sink,
                        inflight: null, closeReq: null, inflightClose: null, pendingAbort: null, started: false, backpressure: false, ctrl: null };
    var ctrl = w.ctrl = new WritableStreamDefaultController(K);
    ctrl[K] = { w: w, abort: (typeof AbortController === 'function') ? new AbortController() : null };
    w.backpressure = w.hwm <= 0;
    var r;
    r = call(sink, 'start', [ctrl], undefined);
    resolved(r).then(function () { w.started = true; wsAdvance(w); }, function (e) { w.started = true; wsDealWithRejection(w, e); });
  }
  function ws(stream) { var w = stream && stream[K]; if (!w || w.sink === undefined) throw typeError('não é um WritableStream'); return w; }
  function wsDesired(w) { return w.state === 'errored' || w.state === 'erroring' ? null : w.state === 'closed' ? 0 : w.hwm - w.qsize; }
  function wsUpdateBackpressure(w) {
    var bp = wsDesired(w) <= 0;
    if (bp === w.backpressure) return;
    w.backpressure = bp;
    var wr = w.writer;
    if (!wr) return;
    if (bp) wr.ready = defer(); else wr.ready.resolve(undefined);
  }
  function wsDealWithRejection(w, e) { if (w.state === 'writable') { wsStartErroring(w, e); return; } wsFinishErroring(w); }
  function wsStartErroring(w, e) {
    w.state = 'erroring'; w.stored = e;
    var wr = w.writer;
    if (wr) { if (!w.backpressure) wr.ready = defer(); handled(wr.ready.promise); wr.ready.reject(e); }
    if (!w.inflight && !w.inflightClose && w.started) wsFinishErroring(w);
  }
  function wsFinishErroring(w) {
    w.state = 'errored';
    var e = w.stored;
    w.queue.forEach(function (q) { if (q.d) q.d.reject(e); });
    w.queue = []; w.qsize = 0;
    if (w.ctrl[K].abort) try { w.ctrl[K].abort.abort(e); } catch (x) {}
    var pa = w.pendingAbort;
    w.pendingAbort = null;
    if (pa) {
      if (pa.wasAlready) { pa.d.reject(e); wsRejectClose(w); }
      else promiseCall(w.sink, 'abort', [pa.reason]).then(function () { pa.d.resolve(undefined); wsRejectClose(w); }, function (x) { pa.d.reject(x); wsRejectClose(w); });
    } else wsRejectClose(w);
  }
  function wsRejectClose(w) {
    if (w.closeReq) { w.closeReq.reject(w.stored); w.closeReq = null; }
    var wr = w.writer;
    if (wr) { handled(wr.closed.promise); wr.closed.reject(w.stored); }
  }
  function wsAdvance(w) {
    if (!w.started || w.inflight || w.inflightClose) return;
    if (w.state === 'erroring') { wsFinishErroring(w); return; }
    if (!w.queue.length) return;
    var q = w.queue[0];
    if (q.close) {
      w.queue.shift();
      w.inflightClose = w.closeReq; w.closeReq = null;
      promiseCall(w.sink, 'close', []).then(function () {
        w.inflightClose.resolve(undefined); w.inflightClose = null;
        if (w.state === 'erroring') { w.stored = undefined; if (w.pendingAbort) { w.pendingAbort.d.resolve(undefined); w.pendingAbort = null; } }
        w.state = 'closed';
        if (w.writer) w.writer.closed.resolve(undefined);
      }, function (e) {
        w.inflightClose.reject(e); w.inflightClose = null;
        if (w.pendingAbort) { w.pendingAbort.d.reject(e); w.pendingAbort = null; }
        wsDealWithRejection(w, e);
      });
      return;
    }
    w.inflight = q.d;
    promiseCall(w.sink, 'write', [q.v, w.ctrl]).then(function () {
      w.inflight.resolve(undefined); w.inflight = null;
      w.queue.shift(); w.qsize -= q.s;
      if (w.state === 'writable' && !w.closeReq) wsUpdateBackpressure(w);
      wsAdvance(w);
    }, function (e) {
      w.inflight.reject(e); w.inflight = null;
      if (w.state === 'writable') { w.queue = []; w.qsize = 0; }
      wsDealWithRejection(w, e);
    });
  }
  function wsAbort(w, reason) {
    if (w.state === 'closed' || w.state === 'errored') return resolved(undefined);
    if (w.ctrl[K].abort) try { w.ctrl[K].abort.abort(reason); } catch (x) {}
    if (w.pendingAbort) return w.pendingAbort.d.promise;
    var was = w.state === 'erroring';
    var d = defer();
    w.pendingAbort = { d: d, reason: reason, wasAlready: was };
    if (!was) wsStartErroring(w, reason);
    return d.promise;
  }
  function wsClose(w) {
    if (w.state === 'closed' || w.state === 'errored') return rejected(typeError('o stream já está fechado'));
    if (w.closeReq || w.inflightClose) return rejected(typeError('close já pedido'));
    var d = defer();
    w.closeReq = d;
    if (w.writer && w.backpressure && w.state === 'writable') w.writer.ready.resolve(undefined);
    w.queue.push({ close: true, s: 0 });
    wsAdvance(w);
    return d.promise;
  }
  function WritableStreamDefaultController(k) { if (k !== K) throw typeError('Construtor ilegal'); }
  WritableStreamDefaultController.prototype = {
    error: function (e) { var w = this[K].w; if (w.state === 'writable') wsStartErroring(w, e); },
    get signal() { return this[K].abort ? this[K].abort.signal : undefined; }
  };
  function WritableStreamDefaultWriter(stream) {
    var w = ws(stream);
    if (w.writer) throw typeError('o stream já tem um escritor');
    var wr = this[K] = { stream: stream, ready: defer(), closed: defer() };
    w.writer = wr;
    if (w.state === 'writable') { if (!w.backpressure || w.closeReq) wr.ready.resolve(undefined); }
    else if (w.state === 'erroring') { handled(wr.ready.promise); wr.ready.reject(w.stored); }
    else if (w.state === 'closed') { wr.ready.resolve(undefined); wr.closed.resolve(undefined); }
    else { handled(wr.ready.promise); wr.ready.reject(w.stored); handled(wr.closed.promise); wr.closed.reject(w.stored); }
  }
  function writerOf(t) { var wr = t[K]; if (!wr || !wr.stream) throw typeError('escritor liberado'); return wr; }
  WritableStreamDefaultWriter.prototype = {
    get closed() { return this[K].closed.promise; },
    get ready() { return this[K].ready.promise; },
    get desiredSize() { var wr = writerOf(this); return wsDesired(wr.stream[K]); },
    write: function (chunk) {
      var wr = this[K];
      if (!wr.stream) return rejected(typeError('escritor liberado'));
      var w = wr.stream[K];
      var size;
      try { size = w.size(chunk); } catch (e) { if (w.state === 'writable') wsStartErroring(w, e); return rejected(e); }
      if (w.state === 'errored' || w.state === 'erroring') return rejected(w.stored);
      if (w.closeReq || w.inflightClose || w.state === 'closed') return rejected(typeError('o stream está fechando ou fechado'));
      var d = defer();
      w.queue.push({ v: chunk, s: size, d: d }); w.qsize += size;
      wsUpdateBackpressure(w);
      wsAdvance(w);
      return d.promise;
    },
    close: function () { var wr = this[K]; return wr.stream ? wsClose(wr.stream[K]) : rejected(typeError('escritor liberado')); },
    abort: function (reason) { var wr = this[K]; return wr.stream ? wsAbort(wr.stream[K], reason) : rejected(typeError('escritor liberado')); },
    releaseLock: function () {
      var wr = this[K];
      if (!wr.stream) return;
      var e = typeError('o escritor foi liberado');
      handled(wr.ready.promise); wr.ready.reject(e); wr.ready = defer(); handled(wr.ready.promise); wr.ready.reject(e);
      handled(wr.closed.promise); wr.closed.reject(e); wr.closed = defer(); handled(wr.closed.promise); wr.closed.reject(e);
      wr.stream[K].writer = null; wr.stream = null;
    }
  };
  WritableStream.prototype = {
    get locked() { return !!ws(this).writer; },
    getWriter: function () { return new WritableStreamDefaultWriter(this); },
    close: function () { var w = ws(this); return w.writer ? rejected(typeError('travado por um escritor')) : wsClose(w); },
    abort: function (reason) { var w = ws(this); return w.writer ? rejected(typeError('travado por um escritor')) : wsAbort(w, reason); }
  };

  /* ============================================================ TransformStream */
  function TransformStream(transformer, wStrategy, rStrategy) {
    if (!(this instanceof TransformStream)) throw typeError("O construtor TransformStream exige 'new'");
    transformer = transformer === undefined || transformer === null ? {} : transformer;
    if (transformer.readableType !== undefined || transformer.writableType !== undefined) throw new RangeError('readableType/writableType não são suportados');
    var self = this, rctrl = null, started = defer();
    var tc = new TransformStreamDefaultController(K);
    var t = tc[K] = { backpressure: true, bpChange: defer(), self: self };
    function readableCtrl() { return rctrl; }
    this.readable = new ReadableStream({
      start: function (c) { rctrl = c; return started.promise; },
      pull: function () { t.backpressure = false; var p = t.bpChange; t.bpChange = defer(); p.resolve(); return resolved(); },
      cancel: function (reason) { self.writable[K].state === 'writable' && wsStartErroring(self.writable[K], reason); return promiseCall(transformer, 'cancel', [reason]).then(noop); }
    }, rStrategy || { highWaterMark: 0 });
    t.rctrl = readableCtrl;
    /* transform que falha erra os dois lados (TransformStreamError) */
    function doTransform(chunk) {
      var p;
      if (typeof transformer.transform === 'function') p = promiseCall(transformer, 'transform', [chunk, tc]);
      else { try { tc.enqueue(chunk); p = resolved(); } catch (e) { p = rejected(e); } }
      return p.then(undefined, function (e) { try { rctrl.error(e); } catch (x) {} throw e; });
    }
    this.writable = new WritableStream({
      start: function () { return started.promise; },
      write: function (chunk) {
        if (t.backpressure) return t.bpChange.promise.then(function () { return doTransform(chunk); });
        return doTransform(chunk);
      },
      close: function () {
        return promiseCall(transformer, 'flush', [tc]).then(function () { try { rctrl.close(); } catch (e) {} }, function (e) { rctrl.error(e); throw e; });
      },
      abort: function (reason) { rctrl.error(reason); return promiseCall(transformer, 'cancel', [reason]).then(noop); }
    }, wStrategy);
    var r = call(transformer, 'start', [tc], undefined);
    resolved(r).then(function () { started.resolve(); }, function (e) { started.reject(e); });
  }
  function TransformStreamDefaultController(k) { if (k !== K) throw typeError('Construtor ilegal'); }
  TransformStreamDefaultController.prototype = {
    get desiredSize() { var c = this[K].rctrl(); return c ? c.desiredSize : 0; },
    enqueue: function (chunk) {
      var t = this[K], c = t.rctrl();
      c.enqueue(chunk);
      var bp = c.desiredSize <= 0;
      if (bp && !t.backpressure) { t.backpressure = true; }
    },
    error: function (e) { var t = this[K]; t.rctrl().error(e); var w = t.self.writable[K]; if (w.state === 'writable') wsStartErroring(w, e); },
    terminate: function () { var t = this[K]; try { t.rctrl().close(); } catch (e) {} var w = t.self.writable[K]; if (w.state === 'writable') wsStartErroring(w, typeError('o TransformStream terminou')); }
  };

  /* ============================================================ texto */
  function TextEncoderStream() {
    var enc = new TextEncoder(), pendingHigh = '';
    var ts = new TransformStream({
      transform: function (chunk, c) {
        var s = pendingHigh + String(chunk);
        pendingHigh = '';
        var last = s.charCodeAt(s.length - 1);
        if (last >= 0xD800 && last <= 0xDBFF) { pendingHigh = s.slice(-1); s = s.slice(0, -1); }
        if (s.length) c.enqueue(enc.encode(s));
      },
      flush: function (c) { if (pendingHigh) c.enqueue(new Uint8Array([0xEF, 0xBF, 0xBD])); }
    });
    this.readable = ts.readable; this.writable = ts.writable; this.encoding = 'utf-8';
  }
  function TextDecoderStream(label, opts) {
    var fatal = !!(opts && opts.fatal), ignoreBOM = !!(opts && opts.ignoreBOM);
    var dec = new TextDecoder(label === undefined ? 'utf-8' : label, { fatal: fatal });
    var carry = new Uint8Array(0), first = true;
    /* corta no último ponto de código completo; o resto vai para o próximo pedaço */
    function cut(b) {
      var end = b.length, i = end - 1, back = 0;
      while (i >= 0 && back < 4 && (b[i] & 0xC0) === 0x80) { i--; back++; }
      if (i < 0) return end;
      var lead = b[i], need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
      return end - i < need && lead >= 0xC0 && lead <= 0xF4 ? i : end;
    }
    var ts = new TransformStream({
      transform: function (chunk, c) {
        if (!ArrayBuffer.isView(chunk) && !(chunk instanceof ArrayBuffer)) throw typeError('TextDecoderStream precisa de bytes');
        var u = chunk instanceof ArrayBuffer ? new Uint8Array(chunk) : new Uint8Array(chunk.buffer, chunk.byteOffset, chunk.byteLength);
        var b = new Uint8Array(carry.length + u.length);
        b.set(carry); b.set(u, carry.length);
        var k = cut(b);
        carry = b.slice(k);
        var head = b.subarray(0, k);
        if (first && head.length) {
          first = false;
          if (!ignoreBOM && head.length >= 3 && head[0] === 0xEF && head[1] === 0xBB && head[2] === 0xBF) head = head.subarray(3);
        }
        var s = dec.decode(head);
        if (s) c.enqueue(s);
      },
      flush: function (c) { if (carry.length) { var s = dec.decode(carry); carry = new Uint8Array(0); if (s) c.enqueue(s); } }
    });
    this.readable = ts.readable; this.writable = ts.writable;
    this.encoding = dec.encoding; this.fatal = fatal; this.ignoreBOM = ignoreBOM;
  }

  /* stream de bytes a partir de um ArrayBuffer (Response.body, Blob.stream); onRead marca o corpo como usado */
  function fromBytes(buf, onRead) {
    var sent = false;
    return new ReadableStream({
      type: 'bytes',
      pull: function (c) {
        if (onRead && !onRead()) { c.error(typeError('o corpo já foi lido')); return; }
        if (!sent && buf && buf.byteLength) { sent = true; c.enqueue(new Uint8Array(buf)); }
        c.close();
      }
    });
  }

  /* todos os pedaços de um stream de bytes num ArrayBuffer (corpo em stream de Request/Response) */
  function collect(stream) {
    var reader = stream.getReader(), parts = [], total = 0;
    function step() {
      return reader.read().then(function (r) {
        if (r.done) {
          var out = new Uint8Array(total), off = 0;
          parts.forEach(function (p) { out.set(p, off); off += p.length; });
          return out.buffer;
        }
        var v = r.value;
        if (!(v instanceof Uint8Array)) throw typeError('o corpo em stream precisa entregar Uint8Array');
        parts.push(v); total += v.length;
        return step();
      });
    }
    return step();
  }
  function isStream(v) { return v instanceof ReadableStream; }

  /* CompressionStream / DecompressionStream: gzip, deflate (zlib) e deflate-raw */
  function zstream(compress, self, format) {
    var h = Z.init(String(format), compress);
    var ts = new TransformStream({
      transform: function (chunk, c) {
        var o = Z.push(h, chunk);
        if (o.length) c.enqueue(o);
      },
      flush: function (c) {
        var o = Z.end(h);
        if (o.length) c.enqueue(o);
      }
    });
    self.readable = ts.readable;
    self.writable = ts.writable;
  }
  function CompressionStream(format) {
    if (!(this instanceof CompressionStream)) throw typeError("Construtor CompressionStream precisa de 'new'");
    if (arguments.length < 1) throw typeError('CompressionStream: é preciso o formato');
    zstream(true, this, format);
  }
  function DecompressionStream(format) {
    if (!(this instanceof DecompressionStream)) throw typeError("Construtor DecompressionStream precisa de 'new'");
    if (arguments.length < 1) throw typeError('DecompressionStream: é preciso o formato');
    zstream(false, this, format);
  }

  var tags = { ReadableStream: ReadableStream, ReadableStreamDefaultReader: ReadableStreamDefaultReader, ReadableStreamBYOBReader: ReadableStreamBYOBReader,
               ReadableStreamDefaultController: ReadableStreamDefaultController, ReadableByteStreamController: ReadableByteStreamController,
               WritableStream: WritableStream, WritableStreamDefaultWriter: WritableStreamDefaultWriter,
               WritableStreamDefaultController: WritableStreamDefaultController, TransformStream: TransformStream,
               TransformStreamDefaultController: TransformStreamDefaultController, CountQueuingStrategy: CountQueuingStrategy,
               ByteLengthQueuingStrategy: ByteLengthQueuingStrategy, TextEncoderStream: TextEncoderStream, TextDecoderStream: TextDecoderStream,
               CompressionStream: CompressionStream, DecompressionStream: DecompressionStream };
  Object.keys(tags).forEach(function (n) {
    var f = tags[n];
    Object.defineProperty(f.prototype, 'constructor', { value: f, writable: true, configurable: true });
    if (typeof Symbol === 'function' && Symbol.toStringTag) Object.defineProperty(f.prototype, Symbol.toStringTag, { value: n, configurable: true });
  });
  ReadableStreamBYOBReader.prototype.constructor = ReadableStreamBYOBReader;
  return { tags: tags, fromBytes: fromBytes, collect: collect, isStream: isStream };
})
