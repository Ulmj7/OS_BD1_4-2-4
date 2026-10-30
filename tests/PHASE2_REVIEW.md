# Independent reproducibility and documentation review — 2026-10-28

Reviewed project: IV-2-4, F.CSM302 BD1, Kernel Data Structures. This is an
independent QA review of the supplied local deliverable, not an external team's
published peer review. No production code or submitted documentation was edited.

## Follow-up final status — 2026-10-28

The original observations below are retained as review history. Their current
status after the developer and tech-lead changes is:

| Finding | Final status | Evidence / owner |
| --- | --- | --- |
| 1: acquisition placeholder | **Open, user-side GitHub publication**; offline acquisition is now documented | The real repository URL still needs the user's publication step. Tech lead added supplied-folder/archive copying to Manual 8.2.1, matching the successful fresh-copy workflow; this is not a remaining production-code defect. |
| 2: Collatz example workflow/image | **Closed by tech lead** | Current Manual PDF explicitly uses the direct command box for start=0 before deletion; the caption now identifies step 3 and the preserved 112-node state. This matches the independently tested direct-command behavior. |
| 3: Ctrl+C engine cleanup | **Closed by developer; regression verified before/after** | Engine Popen now has `start_new_session=True`. New `ServerTests.test_group_sigint_shutdown_frees_live_nodes` fails with real child exit -2 against the saved original server and passes against the fixed server, including the fully sanitized suite. |
| 4: clipped diagram label | **Closed by tech lead** | The current Report PDF text contains the entire `Kbuild → .ko: энд ажиллуулаагүй` note; the SVG label was moved left with end alignment. |
| 5: UI script availability | **Closed by tech lead** | Current Report and Manual list both `tests/ui/` scripts, their installed-browser prerequisite and exclusion from `make test`. |

Regression details: the test starts an isolated real server with four live
colors, sends SIGINT to its process group, and verifies real engine exit 0,
server exit 0, no `LEAK` or traceback, and no surviving engine PID. A small
observer calls the original engine shutdown method unchanged and records its
child's actual `Popen.returncode`; it does not change signal handling or replace
the engine.

| Run | Actual result |
| --- | --- |
| Before: `python3 tests/test_server.py ServerTests.test_group_sigint_shutdown_frees_live_nodes` in `/tmp/kds-sigint-before-f_dufpfc/kernel-data-structures` | Exit 1; one test failed: `QA_ENGINE_EXIT=-2` instead of `QA_ENGINE_EXIT=0`. This copy uses the original server saved during Phase 2. |
| After: `make clean && make && make test` in the repository | Exit 0, about 2.22 s; four C suites pass; 9 protocol groups / 229 replies pass; **11 HTTP tests** pass (`Ran 11 tests in 0.325s`); `OK`; `All available tests passed.` No sanitizer diagnostics. |

Full final command output: `/tmp/kds-sigint-final-make-test.log`. An earlier
shared-tree run saw the sanitized engine path disappear mid-suite and returned
E_ENGINE/ENOENT for later restarts; the complete clean rerun above passed.

Documentation bookkeeping now needs **11** HTTP tests, **1,303** top-level test
source lines (`test_server.py`: 317), and **260** server source lines. The
initial 10-test / 1,217-line / 257-line measurements below describe the original
review snapshot. The tech lead owns updating those figures in submitted docs.

## Environment and first attempt

| Item | Actual environment |
| --- | --- |
| OS / architecture | Arch Linux, Linux 7.2.11-arch1-1, x86_64 |
| Compiler | gcc (GCC) 16.2.1 20260810 |
| Build | GNU Make 4.4.1 |
| Runtime | Python 3.14.8, standard library only |
| Browser | Brave 156.1.98.60, headless CDP |
| Fresh source | `/tmp/kds-qa-fresh-2sahcecn/kernel-data-structures` |
| Acquisition | Copied supplied repository, excluding `build`, `.git`, `__pycache__`; no downloads |
| Kernel headers | `/lib/modules/7.2.11-arch1-1/build` absent; real module build/load not attempted |

The first build/run attempt followed Manual 8.2.4 and README Quick Start without
source changes. The clone command remains a placeholder; local copying was the
explicitly authorized substitute. This is a fresh-directory test on the same
host, not proof of execution on different hardware or a different distribution.

## Build, run, and test evidence

| Step | Exact command / input | Expected | Actual |
| --- | --- | --- | --- |
| Build | `make` | Engine and library; exit 0 | PASS; `build/kds_engine`, `build/libkds.a`; no warnings; about 0.25 s |
| Main tests | `make test` | Sanitized implementation; 4 C suites, 9 cases / 229 replies, 10 HTTP tests | PASS; ASan+UBSan flags on shim/core/main and test TUs; all expected lines; HTTP `Ran 10 tests in 0.261s`, `OK`; `All available tests passed.`; about 1.72 s |
| Run | `make run` | Local server on 8080 | PASS; printed `http://127.0.0.1:8080`; UI connected and served assets |
| Direct engine | `printf 'color example\ncolor delete index=1\nstate\n' \| build/kds_engine` | Four colors, then three; correct accounting | PASS; first state 4/0/4/128 B; then IDs 1,3,4 and 4/1/3/96 B |
| Limit case | `build/kds_engine --max-nodes 3 < tests/cases/max_nodes.in` | Ten replies, full errors preserve old nodes | PASS; exit 0, empty stderr |
| Normal engine cases | `python3 tests/run_cases.py --engine build/kds_engine` | 9 groups, 229 replies | PASS; all nine groups |
| References | `python3 tests/reference_cases.py` | Independent fixture generation | PASS; all seven table checkpoints; deterministic generation previously checked |
| Alternate configuration | `python3 src/ui/server.py --port 9000 --max-nodes 50` | Listen on 9000; per-structure limit 50 | PASS; state limit 50; Collatz 27 rejected as `E_FULL`; SIGTERM exit 0 |
| Extra 1: int64 / EOF | Push -9223372036854775808, then 9223372036854775807, then 9223372036854775808; final `state` without newline | Exact endpoints; overflow error; final reply | PASS; exact two values in LIFO order, `E_RANGE`, four replies, exit 0 / empty stderr |
| Extra 2: HTTP body boundary | `curl --data-binary @... -H 'Content-Type: application/json'` with valid JSON padded to exactly 4096 / 4097 bytes | HTTP 200 / 400 | PASS; statuses 200 / 400 |
| Extra 3: symlink confinement | Real server Handler, temporary static root containing symlink to an outside sentinel file | Regular file 200, escape 403 | PASS; `/ok.txt` 200, `/escape.txt` and `/../outside.txt` 403; no sentinel disclosure; no source tree changed |
| CLI option limits | Engine `--max-nodes 1`, `100000`, `0`, `100001`, `-1`, `abc` | Bounds accepted; invalid options rejected | PASS; first two accepted, others exit 2 |
| Stop as documented | Ctrl+C in `make run` terminal | Server stops, engine performs EOF cleanup | Server stopped, but engine exited by SIGINT (`-2`), bypassing its cleanup; see finding 3 |
| Clean | `make clean` | Build/cache/module remnants removed | PASS; fresh copy left without `build/`; both 8080 and 9000 stopped |

The complete outputs of 41 CLI/curl/reference commands are recorded in
`/tmp/kds-qa-fresh-2sahcecn/manual-command-results.json`. Literal Manual 8.2.6
curl commands returned the documented JSON errors; server access output also
confirmed statuses 200/400/403/404, including `E_ORIGIN` and `--path-as-is`.

## Documentation and UI cross-check

All numerical reference claims checked against actual runs matched:

| Claim | Actual evidence |
| --- | --- |
| ENOMEM positions | Module tests exercised 1,250 Collatz positions + 4 color positions; every failed init cleaned up; exact allocation boundary succeeded |
| Collatz 27 | 112 nodes, 111 steps, peak 9232 at index 77; 2,688 live bytes; peak deletion leaves 111 |
| Collatz 837799 | 525 nodes, peak 2,974,984,576 |
| INT_MAX start | 451 nodes, peak 1,235,346,792,567,892 |
| Other reference starts | 1/15/25/97 produce 1/18/24/119 terms and documented peaks |
| Color example | Correct violet; exact four printk lines; 128 B before indexed deletion, 96 B after |
| Error table | Invalid ranges, numeric input, missing/duplicate/unknown arguments, unknown command, missing match, empty structures, long line, limits and ENOMEM all match |
| Source line counts | 86 shim Linux headers (list.h 38), 106 shim core, 107 modules/Kbuild, 406 engine, 257 server, 514 UI, 73 Makefile, 1,217 top-level C/Python/header test lines; all match Report 5.1 |
| PDF metadata | Manual 9 pages; Report 19 pages |

Brave ran both preserved developer scripts successfully. Independent Manual
form checks additionally reproduced example 1 exactly: search red=255 selected
#2, the observed animated visit sequence was #1→#2→#3→#4, indexed deletion left
#1/#3/#4, and reset released all nodes. Example 2 produced the documented
112-node sequence, peak search/deletion, and 525-node sequence; its start=0 step
has the documentation problem below. Example 3 stack/queue forms, example 4 URL,
and example 5 ENOMEM command-box sequence all worked as written.

The color-example and dark Collatz screenshots were independently reproduced
and visually compared; graphs, colors, labels, arcs and counters match the
submitted images, apart from addresses/time. DOM checks confirmed 112 nodes /
226 arrows, real search/delete submissions, history locks, zoom, keyboard and
pointer pan, theme toggle, exact int64 display, 320/390 px without page overflow,
and no JavaScript exceptions. The previously developer-only Report 8.6 claims
are now independently supported by these runs.

Evidence: `/tmp/kds-ui-check/` (seven screenshots),
`/tmp/kds-qa-fresh-2sahcecn/manual-ui/results.json` and screenshots,
`/tmp/kds-qa-fresh-2sahcecn/sigint-repro.log`, and
`/tmp/kds-report-page5.png`.

Two evidence-backed strengths are broad independent arithmetic/allocation
failure coverage and a graph that actually reflects engine IDs, pointer links,
accounting, errors and historical snapshots. No numerical result was copied
from a production snapshot to create an expectation.

## Initial findings (status updated above)

### 1. Major — Manual-only acquisition is incomplete

**Where:** Manual 8.2.1, README Quick Start, report front matter / 5.7.

**Repro:** Start with only the Manual and attempt its clone step.
**Expected:** A concrete repository URL or downloadable source archive.
**Actual:** `<GitHub холбоос>` / `[GitHub холбоос]` placeholders; no published
repository or release is supplied. No network clone was attempted in this
review. **Impact:** A stranger cannot acquire the deliverable using the Manual
alone. This is already acknowledged by the documents, but remains a handoff
blocker. **Suggested fix:** Supply the real approved repository/release or an
archive path and acquisition instructions; replace placeholders before handoff.

### 2. Minor — Collatz example's invalid-input step and screenshot do not follow its workflow

**Where:** Manual 8.1.7 example 2, steps 3–4 and its error-image caption;
`docs/src/manual.html:160` / `:163`; validation in `app.js:updateFields`.

**Repro:** Run 27, search 9232, delete 9232, select run, enter start=0, submit
the normal operation form. **Expected:** The promised red E_RANGE message and
the pictured state. **Actual:** Native min=1 validation prevents submission;
history is unchanged and the message still says the previous delete succeeded.
There are already 111 nodes after step 3, while the error picture shows 112.
The raw command box correctly returns E_RANGE and preserves the 111-node state.
**Impact:** The documented user workflow does not reproduce the promised error
or screenshot. **Suggested fix:** Explicitly use the direct command box for
`collatz run start=0`; move this error check before peak deletion to match the
112-node image, or replace the screenshot/caption with the 111-node result.

### 3. Minor — Terminal Ctrl+C bypasses engine cleanup and leak checking

**Where:** Manual 8.1.5; `src/ui/server.py:41` engine subprocess creation.

**Repro:** `make run`, create colors, then press Ctrl+C in that terminal.
Also independently reproduced by starting the server in its own process group
and sending SIGINT to that group with four live nodes.
**Expected:** Parent closes engine stdin, engine destroys lists and exits 0.
**Actual:** Terminal SIGINT reaches the engine too; it exits -2. The real terminal
printed `^Cengine exited with code -2`; the isolated reproduction captured the
same child exit in `sigint-repro.log`. **Impact:** The advertised shutdown path
skips `kds_destroy()` and the final leak diagnostic. The OS still reclaims memory;
no persistent host leak was observed. SIGTERM to the parent alone is clean.
**Suggested fix:** Isolate the engine from terminal signals (for example,
`start_new_session=True` in its Popen), then let the parent close stdin; add a
process-group SIGINT regression test.

### 4. Minor — Report architecture diagram clips its kernel-build note

**Where:** Report PDF page 5, Figure 2; `docs/src/report.html:239`.

**Repro:** Read/render page 5. **Expected:** Full note
`Kbuild → .ko: энд ажиллуулаагүй`. **Actual:** The right edge cuts the note at
`энд ажиллу...`. **Impact:** A useful tested-versus-untested qualification is
partly unreadable in the diagram (it remains explicit elsewhere).
**Suggested fix:** Move the label left, wrap it, or enlarge its SVG viewBox.

### 5. Suggestion — Update UI automation availability in the documents

**Where:** Report 8.6 / 9.2 / 9.3, Manual 8.2.9.

The optional scripts are now in `tests/ui/browser_check.py` and
`tests/ui/interactions.py`, with configurable browser/server/output, an
ephemeral debug port and isolated temporary profile. Both passed against the
fresh server. `tests/README.md` documents the already-installed Brave/Chromium
requirement and commands; they intentionally remain separate from `make test`.
Update the current statements that UI scripts are absent from the repository.

## Verdict

The supplied source copy builds and passes all main tests with the documented
tools, including the new group-SIGINT regression. The tech lead has corrected
the reviewed documentation issues and supplied an offline acquisition route.
A stranger who receives the source can follow the Manual for normal
build/run/tests; publishing and filling the GitHub URL remains a user-side
handoff task. Real kernel execution and a genuinely different
computer/distribution remain unverified; the documents disclose those limits.
