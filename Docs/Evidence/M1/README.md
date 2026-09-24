# M1 evidence (GRIDLANDS_BOOTSTRAP)

M1 was awarded **GREEN** by the operator on 2026-09-24.

- Engine: Unreal 5.8.3, changelist 58210709 (`++UE5+Release-5.8`, promoted build),
  prebuilt Linux binary; see `Tools/engine-pin.env`.
- Verified commit: `5b446c0fe62951ee278d0a0273bcd30f8e799068`.

The automation reports here are **sanitized** copies. Host details (machine
name, OS/kernel, CPU, absolute paths) were removed, but the test paths,
states, counts and warning entries are unchanged. Re-judge any of them with
`python3 Tools/lib/automation_report.py <file> Tools/required-tests.txt`.

| File | What it proves | Verdict |
|---|---|---|
| `06-first-build.excerpt.log` | first real C++ build: 15 actions, 0 errors, 0 warnings, ~36 s | PASS |
| `00-first-run-with-afs-warning.index.json` | first real run; 7/7 with an engine warning later traced to the AndroidFileServer plugin (now disabled) | PASS, 7 warnings |
| `01-tests-main-5b446c0.index.json` | 7/7 at the verified commit | PASS, 0 warnings |
| `02-tests-fresh-clone-5b446c0.index.json` | 7/7 from a fresh clone (`Tools/verify-fresh-clone.sh`), tracked inputs + pinned engine only, tree unchanged after the run | PASS |
| `05-zero-test-run.editor-excerpt.log` | filter `Gridlands.NoSuchTest`: engine matched no tests and wrote no report; `test.sh` returned FAIL | rejected |
| `03-partial-run-rejected.index.json` | a real run of 1 passing test is still rejected by the required-test minimum | rejected |
| `04-mutation.diff` + `04-mutation-player-authority.index.json` | granting `Player` the Latent->Detected transition makes exactly the 3 guarding tests fail | rejected, 3/7 fail |
