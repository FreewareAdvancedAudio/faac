# Tests

This directory holds what the tests share. Each library keeps its own tests next
to its code (`libfaac/tests`, `libfaad/tests`, `libfaam/tests`, `libfaar/tests`,
`common/tests`), the frontend tests are in `frontend/tests`, and `fuzz/` has the
fuzz harnesses and their corpus generator (see `fuzz/README.md`).

| File | Purpose |
|------|---------|
| `ftest.h` | Minimal checks for C tests that never use `assert()`, so `NDEBUG` cannot disable them |
| `shard.h` | Splits one long test into several Meson tests that each run a slice |
| `test_abi_layout.c` | Compile-time checks of every public struct's layout |
| `check_symbol_collisions.py` | No global symbol is defined in two of the static archives |
| `audio_fixtures.py`, `faam_test_helpers.h` | Shared fixtures |

Run everything with:

```sh
meson test -C build --print-errorlogs
```

## Time limits on slow targets

The limits are tuned for a current x86-64 or arm64 machine. The
`test-timeout-multiplier` build option scales every one: 0 (the default) picks 1
on fast little-endian CPUs and 10 on big-endian, uncommon, GNU/Hurd and emulated
targets. It is applied through a default Meson test setup, so a plain
`meson test` honours it, and it is exported to the tests as
`FAAC_TEST_TIMEOUT_SCALE`.

Two things keep a slow run short and diagnosable:

- **Shards.** A test that is one long loop takes optional trailing arguments,
  `<shard> <shards>`, and runs the items whose index is `shard` modulo `shards`
  (see `shard.h`). `meson.build` registers one test per shard. Run without
  arguments, the program still runs everything. Every item belongs to exactly one
  shard for any shard count, so adding items needs no change in `meson.build`.
  Bad arguments exit 2, and a shard with nothing to do because a build has few
  channels exits 77 (skipped). Make failures name the case they were running.
- **Start order.** Meson starts tests in declaration order, so the longest tests
  would otherwise begin last and finish alone. Give a test
  `priority: long_test_priority` (about a minute under qemu on m68k) or
  `medium_test_priority` (about half a minute); both are defined in the
  top-level `meson.build`.

## Testing on other architectures

The suite is expected to pass on big-endian and unusual targets. To check one
without the hardware, cross-compile and run under `qemu-user`. Meson runs the C
tests through the `exe_wrapper`, and the Python tests prefix the same wrapper to
every frontend they spawn (`../frontend/tests/harness.py`):

```sh
meson setup build-m68k --cross-file m68k.txt   # exe_wrapper = ['qemu-m68k', '-L', '/usr/m68k-linux-gnu']
meson test -C build-m68k --print-errorlogs
```

CI does this for s390x, m68k and hppa (`.github/workflows/master.yml`).

## Writing tests that survive these targets

- Decode binary output in the byte order of the program that wrote it
  (`harness.byteorder_of()`), never `=` or the host's order.
- Express wall-clock deadlines with `harness.scaled()` so slow targets get the same
  multiplier as the test's own time limit.
- A test that needs files over 4 GiB must probe for sparse-file support and skip
  (exit status 77) when it is missing, as on GNU/Hurd.
- Never write to a child's stdin from the thread that must also read its output.
  GNU/Hurd's pipes hold far less than Linux's 64 KiB, so keep pipe sizes in mind;
  `harness.shrink_pipe()` reproduces that locally.
- A Python test imports `harness` before spawning anything, which routes foreign
  binaries through the exe wrapper and dumps every thread's stack if the test is
  killed at its time limit.
- Public structs must not depend on the target's alignment of `int`: add explicit
  padding and extend `test_abi_layout.c`.
