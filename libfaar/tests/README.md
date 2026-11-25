# FAAR verification

The library tests live in this directory and are registered by its local
`meson.build`. They cover the public streaming API, signal quality controls,
rate-matrix invariants, DFT filtering and shared spectra, prefilter setup, and
the complex FFT. Shared-workspace tests cover serialized channel-pair processing
at padded and High quality settings, verify that overlap histories and
streaming queues remain independent, and check workspace lifetime across
processing and close.
`frontend/tests/test_faar_cli.py` covers command-line and file-format behavior.
The public API is also exercised by `tests/fuzz/fuzz_faar_resampler.c` through the
repository fuzz runner.

Run the production checks from a configured build directory:

```sh
meson test -C build
```

The fuzz target can be built and run with:

```sh
tests/fuzz/run.sh /tmp/faac-fuzz 10 fuzz_faar_resampler
```

## High precision checks

The `high_stopband` Meson group checks High's 150 dB rejection with one, two,
three, and eight channels, including loud neighboring channels.
`test_faar_float2` compares the compensated FFT against the double FFT and
checks inverse round trips and phase arithmetic.

## Reusable filter plans

`test_faar_design` compares plan-backed streams with independent normal opens
across dedicated, generic, cascaded, half-band and equal-rate routes. It checks
prepared streaming, shaped dither, independent sample histories, and closing
the caller's plan reference before processing streams.
`test_faar_design_alloc` injects every setup/clone allocation failure for High,
cascaded Good and generic Fast plans, verifies cleanup, then reuses the plan.
On pthread platforms, `test_faar_design` also opens, processes and closes
independent streams concurrently from one plan.

Both programs are long on slow targets, so Meson registers each as eight tests
(`test_faar_design_0` ... `_7`, `test_faar_design_alloc_0` ... `_7`) that run a
slice of the checks. Run without arguments, a program still runs everything.
With `<shard> <shards>` it runs the checks whose index is `shard` modulo
`shards`; `tests/shard.h` explains the convention, which `test_sbr_pairing` and
`test_faac_drc profile_curves` share. Adding a route or case needs no change in
`meson.build`, and changing the shard count never leaves a check unrun.
