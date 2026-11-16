// Servidor HTTP/2 com TLS para tools/mnet_h2mux_test.c (multiplexação no cliente mnet).
// node tools/mnet_h2_servidor.js <dir-dos-certificados> <porta> <max-fluxos>  — imprime "pronto" quando ouve.
//   GET /lento?ms=N      espera N ms e responde; x-conn = número da conexão, x-fluxo = id do fluxo
//   GET /grande?n=N      N bytes, byte i = (i * 7 + 3) & 255
//   POST /eco            devolve o corpo; x-tamanho = bytes recebidos
//   GET /goaway          responde e manda GOAWAY (a conexão não aceita fluxos novos)
//   GET /contagem        x-conns = conexões abertas até agora
'use strict';
const http2 = require('http2');
const fs = require('fs');
const [dir, port, maxs] = process.argv.slice(2);
let conns = 0;
const srv = http2.createSecureServer({
  key: fs.readFileSync(dir + '/srv.key'), cert: fs.readFileSync(dir + '/srv.pem'),
  settings: { maxConcurrentStreams: Number(maxs) || 100 }, allowHTTP1: false,
});
srv.on('session', (s) => { s.mosId = ++conns; });
srv.on('stream', (st, h) => {
  const u = new URL(h[':path'], 'https://localhost');
  const base = { ':status': 200, 'x-conn': String(st.session.mosId), 'x-fluxo': String(st.id) };
  if (u.pathname === '/lento') {
    setTimeout(() => { if (!st.destroyed) { st.respond(base); st.end('ok ' + st.id); } }, Number(u.searchParams.get('ms')) || 0);
  } else if (u.pathname === '/grande') {
    const n = Number(u.searchParams.get('n'));
    const b = Buffer.alloc(n);
    for (let i = 0; i < n; i++) b[i] = (i * 7 + 3) & 255;
    st.respond({ ...base, 'content-length': String(n) });
    st.end(b);
  } else if (u.pathname === '/eco') {
    const parts = [];
    st.on('data', (d) => parts.push(d));
    st.on('end', () => { const b = Buffer.concat(parts); st.respond({ ...base, 'x-tamanho': String(b.length) }); st.end(b); });
  } else if (u.pathname === '/goaway') {
    st.respond(base); st.end('tchau');
    const ses = st.session;
    setTimeout(() => { try { ses.goaway(0); } catch (e) { /* já fechou */ } }, 50);
  } else if (u.pathname === '/contagem') {
    st.respond({ ...base, 'x-conns': String(conns) }); st.end();
  } else { st.respond({ ':status': 404 }); st.end(); }
  st.on('error', () => {});
});
srv.listen(Number(port), '127.0.0.1', () => console.log('pronto'));
