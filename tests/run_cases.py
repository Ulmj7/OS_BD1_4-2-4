#!/usr/bin/env python3
"""Black-box JSON protocol checks; fixtures are independent of engine output."""
import argparse
import json
import random
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def normalize(value):
    if isinstance(value, str):
        return re.sub(r"0x[0-9a-fA-F]+", "<addr>", value)
    if isinstance(value, list):
        return [normalize(v) for v in value]
    if isinstance(value, dict):
        return {k: normalize(v) for k, v in value.items()}
    return value


def structures(reply):
    return {s["name"]: s for s in reply["state"]["structures"]}


def assert_state(reply):
    state = reply["state"]
    groups = structures(reply)
    assert set(groups) == {"color", "collatz", "stack", "queue"}, groups.keys()
    ids = []
    for name, group in groups.items():
        nodes = group["nodes"]
        assert group["count"] == len(nodes), (name, group["count"], len(nodes))
        assert len(nodes) <= state["max_nodes"]
        assert group["invariant"]["ok"] is True, group["invariant"]
        assert isinstance(group["invariant"]["detail"], str)
        assert re.fullmatch(r"0x[0-9a-fA-F]+", group["head"]["addr"])
        assert group["head"]["next"] == (nodes[0]["id"] if nodes else "head")
        assert group["head"]["prev"] == (nodes[-1]["id"] if nodes else "head")
        for i, node in enumerate(nodes):
            assert node["index"] == i
            assert type(node["id"]) is int and node["id"] > 0
            assert re.fullmatch(r"0x[0-9a-fA-F]+", node["addr"])
            assert node["prev"] == (nodes[i - 1]["id"] if i else "head")
            assert node["next"] == (nodes[i + 1]["id"] if i + 1 < len(nodes) else "head")
            if name == "color":
                assert set(node["fields"]) == {"red", "green", "blue"}
                assert all(type(v) is int and 0 <= v <= 255 for v in node["fields"].values())
            else:
                assert set(node["fields"]) == {"value"}
                assert type(node["fields"]["value"]) is int
            ids.append(node["id"])
    assert len(ids) == len(set(ids)), "allocation IDs must be unique"
    mem = state["kmalloc"]
    assert mem["live"] == len(ids), mem
    assert mem["allocs"] - mem["frees"] == mem["live"], mem
    assert mem["live_bytes"] >= mem["live"], mem


def logical_state(reply):
    return {k: normalize({"count": v["count"], "head": v["head"], "nodes": v["nodes"]})
            for k, v in structures(reply).items()}


def check_reply(reply, expect, previous):
    assert type(reply["ok"]) is bool
    assert reply["ok"] == expect["ok"], {"ok": reply["ok"], "error": reply.get("error"), "expected": expect["ok"]}
    assert_state(reply)
    if reply["ok"]:
        for key in ("cmd", "msg", "ops", "log", "effect"):
            assert key in reply, key
        assert isinstance(reply["ops"], list) and isinstance(reply["log"], list)
        for key in ("added", "removed", "visited", "matched"):
            assert isinstance(reply["effect"][key], list), key
    else:
        assert reply["error"]["code"] in expect["error"].split("|"), (reply["error"], expect["error"])
        assert isinstance(reply["error"]["message"], str)
        if previous is not None:
            assert logical_state(reply) == logical_state(previous), "error mutated live nodes"
    for name, fields in expect.get("structures", {}).items():
        actual = [n["fields"] for n in structures(reply)[name]["nodes"]]
        assert actual == fields, (name, actual, fields)
    for name, count in expect.get("counts", {}).items():
        assert structures(reply)[name]["count"] == count
    for name, fields in expect.get("contains", {}).items():
        assert fields in [n["fields"] for n in structures(reply)[name]["nodes"]]
    for key, count in expect.get("effect_counts", {}).items():
        assert len(reply["effect"][key]) == count, (key, reply["effect"], count)
    for key, selection in expect.get("effect_indexes", {}).items():
        source = previous if key == "removed" else reply
        nodes = structures(source)[selection["structure"]]["nodes"]
        ids = [nodes[i]["id"] for i in selection["indexes"]]
        assert reply["effect"][key] == ids, (key, reply["effect"], ids)
    if "max_nodes" in expect:
        assert reply["state"]["max_nodes"] == expect["max_nodes"]


def run_case(path, engine):
    data = path.read_bytes()
    flags = []
    if data.startswith(b"#!args "):
        directive, data = data.split(b"\n", 1)
        flags = shlex.split(directive.decode()[7:])
    expected = json.loads(path.with_suffix(".expected.json").read_text())
    commands = [line.decode() for line in data.splitlines() if line.strip() and not line.lstrip().startswith(b"#")]
    result = subprocess.run([engine, *flags], input=data, capture_output=True, timeout=20)
    assert result.returncode == 0, (result.returncode, result.stderr.decode(errors="replace"))
    assert result.stderr == b"", result.stderr.decode(errors="replace")
    replies = result.stdout.splitlines()
    assert len(replies) == len(expected) == len(commands), (len(replies), len(expected), len(commands))
    previous = None
    for line_no, (raw, expect, cmd) in enumerate(zip(replies, expected, commands), 1):
        try:
            reply = json.loads(raw)
            check_reply(reply, expect, previous)
            if reply["ok"]:
                assert reply["cmd"] == cmd
            previous = reply
        except (AssertionError, KeyError, ValueError, TypeError) as exc:
            raise AssertionError(f"reply {line_no}, command {cmd!r}: {exc}") from exc
    return len(replies)


def run_garbage(engine):
    # Fixed arbitrary bytes exercise the input boundary, followed by a valid probe.
    rng = random.Random(0xBAD1)
    garbage = [b"?" + bytes(rng.randrange(32, 256) for _ in range(rng.randrange(1, 700))) for _ in range(128)]
    data = (b"\n# ignored\n\x00garbage\n\xff\xfe\xfd\n\x01\x02\x03\n"
            + b"x" * 10000 + b"\n" + b"\n".join(garbage) + b"\nstate\n")
    proc = subprocess.run([engine], input=data, capture_output=True, timeout=10)
    assert proc.returncode == 0, (proc.returncode, proc.stderr)
    assert proc.stderr == b"", proc.stderr
    replies = [json.loads(line) for line in proc.stdout.splitlines()]
    assert replies, "no final state reply"
    assert replies[-1]["ok"] is True and replies[-1]["cmd"] == "state"
    assert_state(replies[-1])
    assert all(r["ok"] is False for r in replies[:-1]), replies
    return len(replies)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", default="build/kds_engine")
    args = parser.parse_args()
    engine = str(Path(args.engine).resolve())
    passed = failed = replies = 0
    for case in sorted((ROOT / "tests/cases").glob("*.in")):
        try:
            count = run_case(case, engine)
            replies += count
            passed += 1
            print(f"PASS  {case.stem:24} {count:3} replies")
        except (AssertionError, OSError, subprocess.TimeoutExpired) as exc:
            failed += 1
            print(f"FAIL  {case.stem:24} {exc}")
    try:
        count = run_garbage(engine)
        passed += 1
        replies += count
        print(f"PASS  {'binary_garbage':24} {count:3} replies")
    except (AssertionError, KeyError, ValueError, TypeError, OSError, subprocess.TimeoutExpired) as exc:
        failed += 1
        print(f"FAIL  {'binary_garbage':24} {exc}")
    print(f"CASES: {passed} passed, {failed} failed; {replies} checked replies")
    return bool(failed)


if __name__ == "__main__":
    sys.exit(main())
