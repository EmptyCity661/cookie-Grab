#!/usr/bin/env python
# 本地测试服务器：静态页 + /api/login（返回带 token 的 JSON）
import json
import os
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

BASE = os.path.dirname(os.path.abspath(__file__))


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def _send_json(self, obj, code=200):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path.startswith("/api/"):
            return self._send_json({"code": 1, "msg": "use POST"})
        path = self.path.split("?")[0]
        if path == "/":
            path = "/login.html"
        fp = os.path.join(BASE, path.lstrip("/"))
        if not os.path.isfile(fp):
            return self._send_json({"code": 404}, 404)
        with open(fp, "rb") as f:
            body = f.read()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        if length:
            self.rfile.read(length)
        if self.path == "/api/login":
            return self._send_json({
                "code": 0,
                "msg": "ok",
                "data": {
                    "access_token": "resp_access_token_9f8e7d6c5b4a",
                    "refresh_token": "resp_refresh_token_1a2b3c4d5e6f",
                    "expires_in": 7200,
                    "user": {"id": 1001, "name": "testuser", "token": "nested_token_ff00ee11dd22"}
                }
            })
        return self._send_json({"code": 404}, 404)


if __name__ == "__main__":
    port = 8899
    srv = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print("test server on http://127.0.0.1:%d/login.html" % port, flush=True)
    srv.serve_forever()
