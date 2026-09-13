# Validation results

Validated on 13 September 2026 on an Apple Silicon Mac running macOS 26.6.1,
with Apple Clang 17.0.0 and SuperCollider 3.14.1 (`426edf6`). Release binaries
contain Apple Silicon and Intel slices, target macOS 11+, and use SC plugin API 3.
Intel execution was tested through Rosetta on this Mac, not on separate Intel hardware.

## Numerical comparison with the original

The pinned TypeScript and C++ port received identical seeded noise and float32
controls. Eight cases compared **330,647 audio frames and all 148 channels**:
48,935,756 scalar comparisons. The largest absolute error was **3.73035 × 10⁻¹⁴**
over all channels and **9.15934 × 10⁻¹⁶** over the four audio channels.

| Case | Sample rate | Model block size | Frames |
| --- | ---: | ---: | ---: |
| Default voice | 44,100 | 512 | 39,936 |
| Default voice | 48,000 | 512 | 43,520 |
| Default voice | 96,000 | 512 | 48,128 |
| Moving pitch/tongue/tenseness/velum | 48,000 | 64 | 43,200 |
| Per-sample model updates | 44,100 | 1 | 6,615 |
| Multiple closures, fricatives, nasal gestures, and releases | 48,000 | 512 | 67,584 |
| Individual oral cells and closure release | 48,000 | 128 | 38,400 |
| Glottal gate attack/release | 48,000 | 256 | 43,264 |

These results validate the model for the tested schedules; they are not a proof
of every possible control trajectory. The noise is intentionally seeded in both
implementations. Unseeded browser sessions will naturally differ. The comparison
includes the original mathematical quirks documented in SOURCE_ANALYSIS.md.
Detailed measurements: [upstream-comparison.json](upstream-comparison.json).

The completion audit strengthened the oracle to take channel 0 directly from
the original `Synthesizer.synthesize` output buffer, including its exact summation
order and gain, and to reject nonfinite reference samples. All cases still pass.

## SuperCollider integration

Actual **scsynth and supernova offline renders passed** on both arm64 and x86_64.
For each server, the default voice was compared over 43,520 frames × four channels
at 48 kHz. The native server block sizes were 16, 64, and 512, with an additional
Intel/Rosetta render at block size 64. **Every compared float32 sample was identical
to the reference**, across all eight server/architecture/block combinations.

Additional one-second renders passed on both servers:

- Audio-rate pitch, tongue, velum, triggers, reset and excitation; multiple mixed-rate points.
- All 148 diagnostic channels, including finite/nonnegative shape and envelope data.
- Named oral/nasal control arrays and ordinary multichannel voice expansion.
- External aspiration and frication noise.
- Infinity and NaN control inputs, verifying finite output and continued rendering.

A deliberately undersized **256 KiB scsynth real-time pool** also verified the
allocation-failure path: silent output, no crash, and safe destruction. Supernova's
pool behavior did not force an allocation failure at the same setting, so no
equivalent supernova exhaustion claim is made.

The language tests compiled six SynthDefs and exercised five expected validation
failures. The actual SCDoc parser and HTML renderer accepted the help page.
Detailed integration results: [server-tests.json](server-tests.json).

## Core robustness

Release and **native UndefinedBehaviorSanitizer** test runs passed. Checks cover:

- Seeded reproducibility, repeated reset, and zero intensity after gate release.
- The equality of mixed output and the lip/nose decomposition.
- 44.1, 48, and 96 kHz; internal update sizes 1, 17, 64, and 512.
- A heap-allocation counter proving no C++ heap allocations during processing/reset.
- 100,000 stressed samples with 64 constrictions, 64 independent turbulence points,
  repeated complete closures, and deliberately overflowing the 256-event transient pool.
- Finite output and numerical recovery under those pathological conditions.

**AddressSanitizer could not run on this Mac's current toolchain/runtime.** Its
runtime hung in shadow-memory initialization before the test entered `main`.
A separate empty-program probe reproduced the arm64 hang; the Intel ASan probe
failed before the test program ran. A one-second native stack sample located the
arm64 wait inside ASan initialization called from macOS dyld. This is recorded
as an unavailable check, not an ASan pass. The project retains an ASan build option
for use with a working compiler/runtime combination.

To reproduce the successful native UB check:

```sh
cmake -S . -B build-ubsan -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DENABLE_SANITIZERS=ON -DSANITIZERS=undefined -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-ubsan --parallel
ctest --test-dir build-ubsan --output-on-failure
```

## Package and demonstration

Both universal `.scx` files are ad-hoc signed and their signatures are verified
across all architecture slices during packaging. Packaging includes the class,
help, examples, this analysis, licenses, and a separate complete source archive.
Developer ID signing and notarization were not performed because no signing
identity was installed.

`examples/PinkTrombone-demo.wav` is a 48 kHz stereo offline render, approximately
10 seconds, demonstrating moving tongue shapes, nasal opening, frication, and
closure releases. Peak absolute amplitude is about **0.18016**. The demonstration
applies output gain, an envelope, and `LeakDC`; these are outside the UGen. This
was rendered and checked numerically, not auditioned through the user's speakers.

Offline correctness and a bounded processing loop do not establish performance
for every live polyphonic session or hardware configuration. No interactive
audio-device, long-duration soak, notarization, or other-Mac installation test
is claimed.
