# Repeatable QA for Kernel Data Structures

Run `make test` at the repository root. Its contract is to compile and execute
every `test_*.c`, then run `run_cases.py` and `test_server.py`, failing on a
nonzero exit. No packages, root privileges, real kernel loading, or network
services are needed. The HTTP tests use only an ephemeral IPv4 loopback port.

The tests compile the actual kernel module `.c` files against the shim; they do
not substitute test implementations. A successful userspace run does not prove
that a `.ko` builds or loads on a real kernel.

## Expected results and independent derivation

| Test ID | Input | Expected result | How derived |
| --- | --- | --- | --- |
| L01 | Empty sentinel, `INIT_LIST_HEAD`, entry and first-entry | Head points to itself; empty traversal executes zero times | Circular list definition |
| L02 | 5,000 operations, xorshift seed `0x0c011a72` | Add at head/tail/after, delete, and safe selective delete exactly match a plain pointer-array model after **every** operation | Separate array insertion/deletion with `memmove`; forward/reverse adjacency checks |
| L03 | Every allocated item, followed by final cleanup | Fresh bytes `0xA5`, stable serial; deleted links poisoned; allocs=frees, live=bytes=0 | Shim API and list-del contract |
| M01 | Actual color module init/exit | Exactly four printk lines, in order: violet `(138,43,226)`, red `(255,0,0)`, green `(0,255,0)`, blue `(0,0,255)`; no exit output | Correct RGB violet from brief; canonical primary-color fixture; literal expected output in `test_color_module.c` |
| M02 | Color failure after 0,1,2,3 successful allocations | `-ENOMEM`, exact `color: kmalloc failed\n`, all earlier allocations freed | One allocation per color; no traversal until all four exist |
| M03 | Color failure after 4 successful allocations | Successful init then leak-free exit | Four allocations suffice; injection triggers only on the **next** allocation |
| M04 | Actual Collatz module with untouched default | Same 24 values and printk lines as start=25 | Required default, independent recurrence below |
| M05 | Actual Collatz module with each reference start below | Every stored value and every printk byte matches independent Python reference; term count and peak match the brief | Python arbitrary-precision integer arithmetic, checked against independent reference table |
| M06 | Each reference with failure after `k=0..terms-1` | `-ENOMEM`, exact `collatz: kmalloc failed\n`; alloc/free deltas both k; empty list | One allocation per term; all **1,250** allocation positions swept |
| M07 | Each reference with failure after `terms` allocations | Success and zero leaks after exit | Boundary: no extra allocation is required |
| M08 | Collatz start 0 and -5 | `-EINVAL`, exact `collatz: start must be positive\n`; no allocations | Positive-start requirement |
| M09 | Named parameter `start`, unknown name, 2147483648 | Setter changes actual static `start`; unknown/outside int rejected | Shim module-parameter API, int32 bound |
| C01 | Every core command | Counts, actual fields, order, allocation IDs, effect IDs, and circular links agree | Direct inspection of public context and independent expected sequences |
| C02 | Every parser error class, empty command, 513-byte command | Named error code; exact previous state including memory counters | Public core header and grammar |
| C03 | Exactly 512 bytes: `state` plus 507 spaces | Success | Inclusive line-size boundary |
| C04 | Limit=3, full stack, four-color example, long Collatz replacement | `E_FULL`; previous nodes and IDs retained | Per-structure node cap and atomic-failure requirement |
| C05 | Core ENOMEM at every color-example/25-sequence allocation and single-node operation | `E_NOMEM`, old state and memory counters retained, no temporary live allocation | Public transactional error contract |
| C06 | Ten corruptions: broken next/prev, wrong count, poison, disconnected cycle, self-loop, bad head, excessive count | `kds_verify().ok=false`, nonempty detail, bounded walk; state emits `?` for unknown pointers; no crash/hang | Bidirectional circular-list invariants; 20-second process alarm bounds hangs |
| C07 | Destroy context with poisoned link | Registry cleanup releases all allocations | Bounded-destruction API guarantee |
| P01 `book_example` | Book example, traverse, clear/reset/help/check | Four nodes incl. corrected violet, traversal visits in order, clear frees four | Stated module example and command semantics |
| P02 `color_operations` | Head/tail/insert-after, search, indexed and multi-match deletion | Orders `[b,a]` → `[b,c,a]` → `[c,a]` → `[]`; all matches removed; subset filters conjunctive | Plain list reasoning, explicitly written RGB values |
| P03 `boundary_errors` | Empty structures, negative/OOB index, invalid RGB, missing/duplicate/unknown args, 600-byte line, invalid Collatz | Stable error codes, unchanged nodes/counters, usable final state | Grammar and core header; index=0 on empty color list is `E_RANGE`, empty pop/peek/dequeue is `E_EMPTY` |
| P04 `stack_queue` | Signed-int64 values, pushes/enqueues, peeks/removals | Stack LIFO and queue FIFO, including signed-int64 limits | Stack/queue definitions |
| P05 `max_nodes` | `#!args --max-nodes 3`, overflow and replacement attempts | `max_nodes=3`, `E_FULL`, original one-term sequence and three stack entries remain | Per-case CLI flags and node-cap semantics |
| P06 `allocation_failure` | Failure injection on single-node operation and mid-Collatz replacement, then off/reset | `E_NOMEM`, original RGB/Collatz survive, off permits allocation | Atomic-failure and injection contracts |
| P07 `collatz_reference` | All seven starts, traversal, peak search/delete including 2,974,984,576 | Full reference arrays match, 112-node and 525-node sequences; search/delete correct IDs | Independent Python recurrence and reference table |
| P08 `ignored_lines` | Empty/whitespace/comment lines between state/check | Two replies only | CLI skip-line contract |
| P09 `binary_garbage` | NUL, non-UTF8, control bytes, 10,000-byte line, 128 random binary lines (seed `0xBAD1`), final state | Process exits 0, stderr empty, each reply valid JSON; final state works | Robust-input requirement; malformed data never becomes a successful command |
| H01 | GET `/`, `app.js`, `style.css` | HTTP 200, Mongolian Cyrillic in page, correct HTML/JS/CSS MIME | Offline UI and static-serving requirements |
| H02 | Two commands and GET `/api/state`, unknown engine command | Persistent nodes match; HTTP 200 engine error passes through unchanged | Persistent engine JSON API |
| H03 | Invalid JSON/type/UTF8, missing/nonstring cmd, newline/CR, 513 bytes, wrong/missing MIME, 1 MiB body | HTTP 400 JSON error; server still handles state | HTTP trust-boundary requirements |
| H04 | Exactly 512-byte command | HTTP 200, successful engine reply | Inclusive size boundary |
| H05 | Enqueue then POST `/api/restart` | Empty structures, live=0 | Reset-by-engine-restart semantics |
| H06 | `..`, percent-encoded dots/slashes, backslash, NUL and absolute-path probes | HTTP 400/403/404; never exposes target file | Static root confinement |
| H07 | Inspect listening socket in Linux `/proc/net/tcp{,6}` | Only IPv4 127.0.0.1 on selected port | Explicit loopback-only binding |
| H08 | Kill child engine then GET state and enqueue | Fresh empty state, then successful command | Auto-restart contract |
| H09 | 24 concurrent enqueues with eight client threads | All replies valid; exactly values 0..23 present once | Lock must pair each request with its own engine response |
| H10 | Scan actual HTML/JS/CSS references | No remote CDN, font, script, stylesheet or fetch dependency | Offline UI contract; SVG namespace URI is not a dependency |
| H11 | SIGTERM server after tests | Terminates within 5s; engine PID gone; no LEAK or traceback | Clean shutdown requirement |
| H12 | Server starts with default SIGINT and four live colors; SIGINT to its foreground process group | Real engine exit 0, server exit 0, no LEAK/traceback, engine PID gone | Explicit child disposition isolates server behavior from an ignored SIGINT in the test launcher; exit observation calls the original shutdown method unchanged |
| H13 | Server starts with inherited ignored SIGINT and four live colors; group SIGINT | Same graceful server/engine exit 0 and no leaks | The server must install its own SIGINT handler even for background/nohup/CI launches; the launcher verifies SIGINT was initially ignored |

## Collatz reference table

The brief supplies these independent checkpoints. Terms include the starting
value and final 1; steps = terms - 1. `reference_cases.py` uses Python integers
and `n // 2 if n % 2 == 0 else 3*n+1`, verifies these checkpoints, then writes
the complete expected arrays in `reference_collatz.h` and the JSON fixtures.
It never executes or imports production code. Regenerate with
`python3 tests/reference_cases.py`.

| Start | Terms | Steps | Peak |
| ---: | ---: | ---: | ---: |
| 1 | 1 | 0 | 1 |
| 15 | 18 | 17 | 160 |
| 25 | 24 | 23 | 88 |
| 27 | 112 | 111 | 9,232 |
| 97 | 119 | 118 | 9,232 |
| 837799 | 525 | 524 | 2,974,984,576 |
| 2147483647 | 451 | 450 | 1,235,346,792,567,892 |

## Protocol fixtures

Each `.in` has a companion `.expected.json`, with one expectation per actual
command. Blank/comments do not have expectations. An optional first line
`#!args --max-nodes N` supplies per-case engine flags. The runner compares
structured fields, order, counts and effect IDs, not human-readable prose;
hex addresses are normalized for unchanged-state comparison. Every reply also
gets independent pointer-resolution, index, invariant, ID-uniqueness and
allocation-accounting checks. Each subprocess must exit 0 with **empty stderr**.

Where the brief permits either classification, a fixture lists error codes
with `|`. Core tests use the exact documented classification. Search misses
are `E_NOT_FOUND`; an empty CLI line is skipped, while an empty core command
is `E_SYNTAX` and an empty HTTP command is HTTP 400.

The initial automated scope is userspace API/protocol/HTTP testing. Browser interaction,
visual graph accuracy, real kernel compilation/loading, and the manual-only
fresh-copy workflow require separate verification. Path traversal probes do
not create a symlink inside `src/ui/static`, because the test only needs a temporary static root.

## Optional Brave/Chromium UI checks

The CDP checks are preserved in `tests/ui/browser_check.py` and
`tests/ui/interactions.py`, with portable browser discovery, an isolated
temporary browser profile, and an ephemeral debug port. They use Python's
standard library and require an already installed Brave/Chromium; no downloads
or packages are needed. They are separate from `make test`.

Start `make run` in one terminal, then run these sequentially in another:

```sh
python3 tests/ui/browser_check.py
python3 tests/ui/interactions.py
```

Both scripts restart the local engine and replace its state. The first captures
seven screenshots in `/tmp/kds-ui-check`; the second checks actual form
submissions, validation, zoom/pan, theme, exact int64 display and history locks.
Set `KDS_URL` for a different local server URL, `KDS_BROWSER` for the installed
browser executable, and `KDS_OUTPUT` for screenshots/logs. Stop the server with
Ctrl+C when finished. The scripts launch a disposable headless browser with
`--no-sandbox`; they are intended only for this project's trusted local UI.
