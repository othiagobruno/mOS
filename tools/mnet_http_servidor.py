#!/usr/bin/env python3
"""Servidor HTTP/1.1 local para tools/mnet_client_test.c. Só escuta em 127.0.0.1, porta no argv[1].
Rotas: /ola, /redir/N (N saltos até /ola), /303 (POST vira GET), /307 (repete o POST), /gzip, /chunked,
/cookie/set, /cookie/get, /lento, /conta (conta conexões: prova keep-alive), /eco (devolve o corpo), /laco."""
import gzip, sys, threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

conexoes = 0
trava = threading.Lock()


class H(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def setup(self):
        global conexoes
        super().setup()
        with trava:
            conexoes += 1

    def log_message(self, *a):
        pass

    def body(self, code, data, ctype="text/plain", extra=()):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        for k, v in extra:
            self.send_header(k, v)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(data)

    def ler_corpo(self):
        n = int(self.headers.get("Content-Length") or 0)
        if self.headers.get("Transfer-Encoding", "").lower() == "chunked":
            out = b""
            while True:
                tam = int(self.rfile.readline().split(b";")[0], 16)
                if not tam:
                    self.rfile.readline()
                    return out
                out += self.rfile.read(tam)
                self.rfile.readline()
        return self.rfile.read(n) if n else b""

    def do_GET(self):
        self.rota()

    def do_HEAD(self):
        self.rota()

    def do_POST(self):
        self.rota()

    def rota(self):
        p = self.path
        corpo = self.ler_corpo() if self.command == "POST" else b""
        if p == "/ola":
            self.body(200, b"ola mundo")
        elif p.startswith("/redir/"):
            n = int(p.split("/")[2])
            alvo = "/ola" if n <= 1 else "/redir/%d" % (n - 1)
            self.body(302, b"vai", extra=[("Location", alvo)])
        elif p == "/laco":
            self.body(301, b"", extra=[("Location", "/laco")])
        elif p == "/303":
            self.body(303, b"", extra=[("Location", "/metodo")])
        elif p == "/307":
            self.body(307, b"", extra=[("Location", "/eco")])
        elif p == "/metodo":
            self.body(200, self.command.encode())
        elif p == "/eco":
            self.body(200, b"eco:" + self.command.encode() + b":" + corpo)
        elif p == "/gzip":
            d = b"comprimido " * 5000
            ae = self.headers.get("Accept-Encoding", "")
            if "gzip" in ae:
                self.body(200, gzip.compress(d), extra=[("Content-Encoding", "gzip")])
            else:
                self.body(200, d)
        elif p == "/chunked":
            self.send_response(200)
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            for i in range(50):
                pedaco = ("pedaco %d;" % i).encode()
                self.wfile.write(b"%x\r\n%s\r\n" % (len(pedaco), pedaco))
            self.wfile.write(b"0\r\nX-Fim: sim\r\n\r\n")
        elif p == "/cookie/set":
            self.body(302, b"", extra=[("Set-Cookie", "sessao=abc123; Path=/; HttpOnly"),
                                       ("Set-Cookie", "tema=escuro; Max-Age=3600"),
                                       ("Location", "/cookie/get")])
        elif p == "/cookie/get":
            self.body(200, (self.headers.get("Cookie") or "").encode())
        elif p == "/lento":
            import time
            time.sleep(3)
            self.body(200, b"tarde")
        elif p == "/conta":
            self.body(200, str(conexoes).encode())
        else:
            self.body(404, b"nao achei")


if __name__ == "__main__":
    srv = ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1])), H)
    srv.daemon_threads = True
    import os
    print("pronto %d" % os.getpid(), flush=True)
    srv.serve_forever()
