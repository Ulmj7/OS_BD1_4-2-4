#!/usr/bin/env python3
"""HTTP boundary tests against the real stdlib server and persistent engine."""
import concurrent.futures
import http.client
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time
import unittest

sys.dont_write_bytecode = True
from run_cases import assert_state, logical_state, structures

ROOT = Path(__file__).resolve().parent.parent


class ServerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="kds-http-")
        cls.log_path = Path(cls.temp.name) / "server.log"
        cls.log = cls.log_path.open("wb")
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            cls.port = sock.getsockname()[1]
        cls.process = subprocess.Popen(
            ["python3", "-u", str(ROOT / "src/ui/server.py"), "--port", str(cls.port),
             "--engine", str(ROOT / "build/kds_engine")],
            cwd=ROOT, stdout=cls.log, stderr=cls.log,
        )
        try:
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                if cls.process.poll() is not None:
                    raise AssertionError(cls.log_path.read_text())
                try:
                    status, _, _ = cls.request("GET", "/api/state")
                    if status == 200:
                        return
                except (OSError, http.client.HTTPException):
                    pass
                time.sleep(0.05)
            raise AssertionError("server did not become ready within 8 seconds")
        except BaseException:
            cls.stop_server()
            cls.temp.cleanup()
            raise

    @classmethod
    def stop_server(cls):
        children = cls.engine_children()
        cls.process.terminate()
        try:
            cls.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            cls.process.kill()
            cls.process.wait(timeout=2)
            raise AssertionError("server did not shut down cleanly after SIGTERM")
        finally:
            cls.log.close()
        for pid in children:
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                continue
            raise AssertionError(f"engine PID {pid} survived server shutdown")

    @classmethod
    def tearDownClass(cls):
        try:
            cls.stop_server()
            assert cls.process.returncode == 0, cls.log_path.read_text(errors="replace")
            log = cls.log_path.read_text(errors="replace")
            assert "LEAK:" not in log, log
            assert "Traceback" not in log, log
        finally:
            cls.temp.cleanup()

    @classmethod
    def engine_children(cls):
        path = Path(f"/proc/{cls.process.pid}/task/{cls.process.pid}/children")
        return [int(pid) for pid in path.read_text().split()] if path.exists() else []

    @classmethod
    def request(cls, method, path, body=None, content_type=None):
        conn = http.client.HTTPConnection("127.0.0.1", cls.port, timeout=5)
        headers = {"Content-Type": content_type} if content_type else {}
        try:
            conn.request(method, path, body=body, headers=headers)
            response = conn.getresponse()
            return response.status, dict(response.getheaders()), response.read()
        finally:
            conn.close()

    def command(self, command):
        status, _, raw = self.request("POST", "/api/cmd", json.dumps({"cmd": command}), "application/json")
        self.assertEqual(status, 200, raw)
        reply = json.loads(raw)
        assert_state(reply)
        return reply

    def restart(self):
        status, _, raw = self.request("POST", "/api/restart", b"{}", "application/json")
        self.assertEqual(status, 200, raw)
        reply = json.loads(raw)
        self.assertTrue(reply["ok"], reply)
        return reply

    def setUp(self):
        self.restart()

    def test_home_and_static_mime(self):
        status, headers, raw = self.request("GET", "/")
        self.assertEqual(status, 200)
        self.assertIn("text/html", headers.get("Content-Type", ""))
        self.assertRegex(raw.decode(), r"[А-Яа-яӨөҮү]")
        for asset, mime in (("app.js", "javascript"), ("style.css", "text/css")):
            with self.subTest(asset=asset):
                status, headers, raw = self.request("GET", "/" + asset)
                self.assertEqual(status, 200)
                self.assertIn(mime, headers.get("Content-Type", ""))
                self.assertTrue(raw)

    def test_persistent_command_state_and_engine_error(self):
        self.assertTrue(self.command("color add_tail red=138 green=43 blue=226")["ok"])
        self.assertTrue(self.command("stack push value=42")["ok"])
        status, _, raw = self.request("GET", "/api/state")
        self.assertEqual(status, 200)
        state = json.loads(raw)
        assert_state(state)
        self.assertEqual(structures(state)["color"]["nodes"][0]["fields"], {"red": 138, "green": 43, "blue": 226})
        self.assertEqual(structures(state)["stack"]["nodes"][0]["fields"], {"value": 42})
        error = self.command("nonsense")
        self.assertFalse(error["ok"])
        self.assertEqual(error["error"]["code"], "E_UNKNOWN_CMD")
        self.assertEqual(logical_state(error), logical_state(state))

    def test_invalid_request_bodies(self):
        cases = [
            (b"{", "application/json"),
            (b"{}", "application/json"),
            (b"[]", "application/json"),
            (b"null", "application/json"),
            (b'{"cmd":42}', "application/json"),
            (b'{"cmd":""}', "application/json"),
            (b'{"cmd":"   "}', "application/json"),
            (b'{"cmd":"# comment"}', "application/json"),
            (json.dumps({"cmd": "state\nreset"}).encode(), "application/json"),
            (json.dumps({"cmd": "state\rreset"}).encode(), "application/json"),
            (json.dumps({"cmd": "x" * 513}).encode(), "application/json"),
            (b'{"cmd":"state"}', "text/plain"),
            (b'{"cmd":"state"}', None),
            (b"x" * (1024 * 1024), "application/json"),
            (b'\xff{"cmd":"state"}', "application/json"),
        ]
        for body, mime in cases:
            with self.subTest(body=body[:50], content_type=mime):
                status, headers, raw = self.request("POST", "/api/cmd", body, mime)
                self.assertEqual(status, 400, raw[:200])
                self.assertIn("application/json", headers.get("Content-Type", ""))
                self.assertIsInstance(json.loads(raw), dict)
        self.assertTrue(self.command("state")["ok"], "server remains usable after malformed input")

    def test_exact_512_character_command(self):
        self.assertTrue(self.command("state" + " " * 507)["ok"])

    def test_restart_resets_structures(self):
        self.command("queue enqueue value=99")
        self.restart()
        state = self.command("state")
        self.assertTrue(all(s["count"] == 0 for s in structures(state).values()))
        self.assertEqual(state["state"]["kmalloc"]["live"], 0)

    def test_path_traversal_is_blocked(self):
        paths = ["/../server.py", "/%2e%2e/server.py", "/%2e%2e%2fserver.py",
                 "/%2E%2E%2F%2E%2E%2Fshim/kshim.c", "/..%5cserver.py",
                 "//etc/passwd", "/%2fetc%2fpasswd", "/etc/passwd",
                 "/%00", "/%2e%2e/%2e%2e/%2e%2e/README.md"]
        for path in paths:
            with self.subTest(path=path):
                status, _, raw = self.request("GET", path)
                self.assertIn(status, (400, 403, 404), raw[:100])

    def test_listens_only_on_ipv4_loopback(self):
        matches = []
        for procfile in ("/proc/net/tcp", "/proc/net/tcp6"):
            for line in Path(procfile).read_text().splitlines()[1:]:
                fields = line.split()
                address, port = fields[1].split(":")
                if int(port, 16) == self.port and fields[3] == "0A":
                    matches.append(address)
        self.assertEqual(matches, ["0100007F"])

    def test_engine_death_auto_restart(self):
        self.command("stack push value=7")
        pids = self.engine_children()
        self.assertEqual(len(pids), 1, pids)
        os.kill(pids[0], signal.SIGKILL)
        time.sleep(0.1)
        status, _, raw = self.request("GET", "/api/state")
        self.assertEqual(status, 200, raw)
        state = json.loads(raw)
        self.assertTrue(state["ok"], state)
        assert_state(state)
        self.assertEqual(structures(state)["stack"]["count"], 0)
        self.assertTrue(self.command("queue enqueue value=5")["ok"])

    def test_concurrent_commands_are_serialized(self):
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            replies = list(pool.map(lambda n: self.command(f"queue enqueue value={n}"), range(24)))
        self.assertTrue(all(r["ok"] for r in replies))
        state = self.command("state")
        actual = [n["fields"]["value"] for n in structures(state)["queue"]["nodes"]]
        self.assertEqual(sorted(actual), list(range(24)))

    def test_group_sigint_shutdown_frees_live_nodes(self):
        # Observe the real child's exit status without changing signal handling.
        launcher = """
import runpy
import sys
server = runpy.run_path(sys.argv[1])
stop = server['Engine']._stop
def observed_stop(engine):
    child = engine.process
    stop(engine)
    if child is not None:
        print('QA_ENGINE_EXIT=' + str(child.returncode), flush=True)
server['Engine']._stop = observed_stop
sys.argv = sys.argv[1:]
server['main']()
"""
        with tempfile.TemporaryDirectory(prefix="kds-sigint-") as scratch:
            log_path = Path(scratch) / "server.log"
            with socket.socket() as sock:
                sock.bind(("127.0.0.1", 0))
                port = sock.getsockname()[1]

            def request(method, path, body=None):
                conn = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
                try:
                    headers = {"Content-Type": "application/json"} if body else {}
                    conn.request(method, path, body, headers)
                    response = conn.getresponse()
                    self.assertEqual(response.status, 200)
                    return json.loads(response.read())
                finally:
                    conn.close()

            with log_path.open("wb") as log:
                process = subprocess.Popen(
                    [sys.executable, "-u", "-c", launcher,
                     str(ROOT / "src/ui/server.py"), "--port", str(port),
                     "--engine", str(ROOT / "build/kds_engine")],
                    cwd=ROOT, stdout=log, stderr=log, start_new_session=True,
                )
                engine_pids = []
                try:
                    deadline = time.monotonic() + 8
                    while True:
                        self.assertIsNone(process.poll(), log_path.read_text())
                        try:
                            request("GET", "/api/state")
                            break
                        except (OSError, http.client.HTTPException):
                            self.assertLess(time.monotonic(), deadline, "SIGINT test server did not start")
                            time.sleep(0.02)
                    reply = request("POST", "/api/cmd", b'{"cmd":"color example"}')
                    self.assertTrue(reply["ok"], reply)
                    assert_state(reply)
                    self.assertEqual(reply["state"]["kmalloc"]["live"], 4)
                    children = Path(f"/proc/{process.pid}/task/{process.pid}/children")
                    engine_pids = [int(pid) for pid in children.read_text().split()]
                    self.assertEqual(len(engine_pids), 1)
                    # A terminal sends SIGINT to every process in its foreground group.
                    os.killpg(process.pid, signal.SIGINT)
                    process.wait(timeout=8)
                    output = log_path.read_text(errors="replace")
                    self.assertEqual(process.returncode, 0, output)
                    self.assertNotIn("LEAK", output)
                    self.assertNotIn("Traceback", output)
                    self.assertEqual(
                        [line for line in output.splitlines() if line.startswith("QA_ENGINE_EXIT=")],
                        ["QA_ENGINE_EXIT=0"], output,
                    )
                    for pid in engine_pids:
                        with self.assertRaises(ProcessLookupError, msg=f"engine PID {pid} survived SIGINT"):
                            os.kill(pid, 0)
                finally:
                    if process.poll() is None:
                        process.terminate()
                        try:
                            process.wait(timeout=5)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.wait(timeout=2)
                    for pid in engine_pids:
                        try:
                            os.kill(pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass

    def test_ui_has_no_remote_dependencies(self):
        remote = re.compile(r'''(?:src|href)\s*=\s*["']https?://|(?:fetch|import)\s*\(?\s*["']https?://''', re.I)
        for name in ("index.html", "app.js", "style.css"):
            text = (ROOT / "src/ui/static" / name).read_text()
            self.assertIsNone(remote.search(text), name)
            self.assertNotRegex(text, r"https?://(?:cdn\.|fonts\.|unpkg\.|cdnjs\.|jsdelivr\.)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
