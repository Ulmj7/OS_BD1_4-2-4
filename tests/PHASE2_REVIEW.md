# Technical verification notes — 2026-10-28

This file records repeatability checks on the local IV-2-4 deliverable. It is a
technical test record, not evidence of an external peer review or of individual
authorship. The team assignments are in the Report appendix.

## Results

- A clean copy built with `make`; four C suites, nine protocol case groups
  (229 replies), and the HTTP suite passed under `make test`. Sanitizers reported
  no errors. The current HTTP suite has 12 tests.
- `make run` served the local UI. Curl checks covered successful commands,
  malformed input, origin validation, and file path confinement.
- The two optional Brave/Chromium scripts in `tests/ui/` checked the graph,
  controls, history, error states, and 320/390 px layouts. They are outside
  `make test` and require an installed browser.
- The module tests covered 1,250 Collatz and four color allocation failure
  positions. Collatz start=27 produced 112 terms with peak 9232; start=837799
  produced 525 terms with peak 2,974,984,576.
- A clean-copy Ctrl+C check exposed an engine shutdown bug: the engine received
  SIGINT with the server and exited before cleanup. `start_new_session=True` in
  `src/ui/server.py` isolates the engine; the regression in `tests/test_server.py`
  now checks engine exit 0 and no surviving child process.
- The Manual's invalid Collatz input example was corrected to use the direct
  command field; native browser form validation blocks zero before submission.

## Scope

The 2026-10-28 copy lacked matching kernel headers, so that check did not
compile or load modules. The later build and team member's real-kernel run are
documented separately in Report §§8.7–8.8. This review does not establish
behavior on another machine or Linux distribution.
