#!/usr/bin/env python3
"""Loopback-only HTTP bridge to one persistent, serialized kds_engine process."""
import argparse
import json
import mimetypes
import os
from pathlib import Path
import selectors
import signal
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import unquote, urlsplit

STATIC = Path(__file__).resolve().parent / "static"
ROOT = Path(__file__).resolve().parents[2]
MAX_BODY = 4096


class EngineError(Exception):
    pass


class Engine:
    def __init__(self, path, max_nodes, timeout=5):
        self.path = str(Path(path).resolve())
        self.max_nodes = max_nodes
        self.timeout = timeout
        self.lock = threading.Lock()
        self.process = None
        self.stderr = None
        self.pending = b""
        self._start()

    def _start(self):
        self.stderr = tempfile.TemporaryFile()
        try:
            self.process = subprocess.Popen(
                [self.path, "--max-nodes", str(self.max_nodes)],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.stderr,
                bufsize=0,
                # Own session: a terminal Ctrl+C must not reach the engine; the
                # server closes its stdin so it runs its EOF cleanup and leak check.
                start_new_session=True,
            )
        except OSError:
            self.stderr.close()
            self.stderr = None
            raise
        self.pending = b""

    def _stop(self):
        if self.process is None:
            return
        process = self.process
        try:
            process.stdin.close()
            process.wait(timeout=3)
        except (OSError, subprocess.TimeoutExpired):
            process.kill()
            process.wait()
        finally:
            process.stdout.close()
            self.stderr.seek(0)
            diagnostics = self.stderr.read().decode("utf-8", errors="replace")
            if diagnostics:
                print(diagnostics.rstrip(), file=sys.stderr)
            if process.returncode:
                print(f"engine exited with code {process.returncode}", file=sys.stderr)
            self.stderr.close()
            self.stderr = None
            self.process = None
            self.pending = b""

    def close(self):
        with self.lock:
            self._stop()

    def restart(self):
        with self.lock:
            self._stop()
            self._start()
            return self._command("state")

    def command(self, command):
        with self.lock:
            if self.process is None or self.process.poll() is not None:
                self._stop()
                self._start()
            try:
                return self._command(command)
            except (OSError, ValueError, EngineError) as exc:
                self._stop()
                try:
                    self._start()
                except OSError:
                    pass
                raise EngineError("Engine stopped or timed out; restarted with empty state. Retry the command.") from exc

    def _command(self, command):
        self.process.stdin.write(command.encode("utf-8") + b"\n")
        deadline = time.monotonic() + self.timeout
        with selectors.DefaultSelector() as selector:
            selector.register(self.process.stdout, selectors.EVENT_READ)
            while b"\n" not in self.pending:
                remaining = deadline - time.monotonic()
                if remaining <= 0 or not selector.select(remaining):
                    raise EngineError("Engine response timed out")
                chunk = os.read(self.process.stdout.fileno(), 65536)
                if not chunk:
                    raise EngineError("Engine closed stdout")
                self.pending += chunk
                if len(self.pending) > 64 * 1024 * 1024:
                    raise EngineError("Engine response too large")
        line, self.pending = self.pending.split(b"\n", 1)
        if not isinstance(json.loads(line), dict):
            raise EngineError("Invalid engine reply")
        return line + b"\n"


class Handler(BaseHTTPRequestHandler):
    server_version = "KDS/1"

    def setup(self):
        super().setup()
        self.connection.settimeout(2)

    def reply(self, status, body, content_type="application/json; charset=utf-8"):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy", "default-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; object-src 'none'; frame-ancestors 'none'; base-uri 'none'")
        self.end_headers()
        self.wfile.write(body)

    def error(self, status, code, message):
        self.reply(status, json.dumps({"ok": False, "error": {"code": code, "message": message}}).encode())

    def engine_command(self, command):
        try:
            self.reply(200, self.server.engine.command(command))
        except (EngineError, OSError) as exc:
            self.error(503, "E_ENGINE", str(exc))

    def do_GET(self):
        try:
            path = unquote(urlsplit(self.path).path, errors="strict")
        except (UnicodeError, ValueError):
            self.error(400, "E_SYNTAX", "Invalid URL")
            return
        if path == "/api/state":
            self.engine_command("state")
            return
        if path.startswith("/api/"):
            self.error(404, "E_NOT_FOUND", "Unknown endpoint")
            return
        if "\0" in path or "\\" in path or ".." in Path(path).parts:
            self.error(403, "E_PATH", "Path is not allowed")
            return
        target = (STATIC / ("index.html" if path == "/" else path.lstrip("/"))).resolve()
        if not target.is_relative_to(STATIC.resolve()):
            self.error(403, "E_PATH", "Path is not allowed")
            return
        try:
            if not target.is_file():
                self.error(404, "E_NOT_FOUND", "File not found")
                return
            body = target.read_bytes()
        except OSError:
            self.error(404, "E_NOT_FOUND", "File not found")
            return
        mime = mimetypes.guess_type(target.name)[0] or "application/octet-stream"
        if mime.startswith("text/") or target.suffix == ".js":
            mime += "; charset=utf-8"
        self.reply(200, body, mime)

    def do_POST(self):
        path = urlsplit(self.path).path
        if path not in ("/api/cmd", "/api/restart"):
            self.close_connection = True
            self.error(404, "E_NOT_FOUND", "Unknown endpoint")
            return
        origin = self.headers.get("Origin")
        if origin and origin != f"http://127.0.0.1:{self.server.server_port}":
            self.close_connection = True
            self.error(403, "E_ORIGIN", "Cross-origin requests are not allowed")
            return
        if path == "/api/restart":
            self.close_connection = True
            try:
                self.reply(200, self.server.engine.restart())
            except (EngineError, OSError) as exc:
                self.error(503, "E_ENGINE", str(exc))
            return
        if self.headers.get_content_type() != "application/json":
            self.close_connection = True
            self.error(400, "E_SYNTAX", "Content-Type must be application/json")
            return
        try:
            length = int(self.headers.get("Content-Length", "-1"))
            if length < 0 or length > MAX_BODY or self.headers.get("Transfer-Encoding"):
                raise ValueError("Invalid body size")
            self.connection.settimeout(2)
            body = self.rfile.read(length)
            if len(body) != length:
                raise ValueError("Incomplete body")
            payload = json.loads(body)
            command = payload.get("cmd") if isinstance(payload, dict) else None
            if not isinstance(command, str) or not command.strip() or command.lstrip().startswith("#"):
                raise ValueError("A nonempty cmd string is required")
            if len(command.encode("utf-8")) > 512 or any(ord(c) < 32 and c != "\t" for c in command):
                raise ValueError("cmd must be one line, at most 512 bytes")
        except (ValueError, UnicodeError, OSError) as exc:
            self.close_connection = True
            self.error(400, "E_SYNTAX", str(exc))
            return
        self.engine_command(command)

    def log_message(self, fmt, *args):
        print(f"{self.address_string()} {fmt % args}", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--max-nodes", type=int, default=2048)
    parser.add_argument("--engine", type=Path, default=ROOT / "build/kds_engine")
    args = parser.parse_args()
    if not 0 <= args.port <= 65535 or not 1 <= args.max_nodes <= 100000:
        parser.error("invalid port or max-nodes")
    engine = Engine(args.engine, args.max_nodes)
    try:
        server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    except BaseException:
        engine.close()
        raise
    server.engine = engine
    server.daemon_threads = False

    def stop(_signal, _frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, stop)
    print(f"http://127.0.0.1:{server.server_port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        engine.close()


if __name__ == "__main__":
    main()
