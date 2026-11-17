self.addEventListener('install', function (e) {
  e.waitUntil(caches.open('v1').then(function (c) { return c.addAll(['dado.txt']); }).then(function () { return self.skipWaiting(); }));
});
self.addEventListener('activate', function (e) { e.waitUntil(self.clients.claim()); });
self.addEventListener('fetch', function (e) {
  var u = new URL(e.request.url);
  if (u.pathname.endsWith('/virtual')) e.respondWith(new Response('do worker:' + e.request.method, { headers: { 'x-sw': '1' } }));
  else if (u.pathname.endsWith('/dado.txt')) e.respondWith(caches.match(e.request));
  else if (u.pathname.endsWith('/quebrado')) e.respondWith(Promise.reject(new Error('não')));
});
self.addEventListener('message', function (e) { e.source.postMessage('eco:' + e.data + ':' + registration.scope); });
