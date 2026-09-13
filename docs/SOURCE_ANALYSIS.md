# Source analysis and port decisions

Analyzed revision: [`359c2d3b42b10280404c1650dc601902112b4c90`](https://github.com/chdh/pink-trombone-mod/tree/359c2d3b42b10280404c1650dc601902112b4c90),
the repository's master HEAD when retrieved on 13 September 2026. The repository
identifies itself as version 0.1.0. The original sources are included unchanged
under `vendor/pink-trombone-mod/src` for review and regression tests.

## Modules and implementation coverage

| Upstream module | Responsibility | Port |
| --- | --- | --- |
| `Synthesizer.ts` | Blocks of at most 512 samples, two tract steps per sample, output sum/gain | `pink::Engine`; fixed configurable model cadence independent of host callback sizes |
| `Glottis.ts` | LF waveform, pitch glide, intensity attack/release, breath noise, vibrato/tenseness noise | `pink::Glottis`; every sound-generating method translated |
| `Tract.ts` | Oral/nasal traveling waves, reflections, three-way junction, frication, transient list, amplitude monitoring | `pink::Tract`; double precision, bounded preallocated event/point storage |
| `TractShaper.ts` | Tongue/rest diameters, nasal profile, slew rates, obstruction release detection | `pink::Tract::adjust`, `restDiameter`, `shapeNose` |
| `NoiseGenerator.ts` | Seeded 2D simplex noise and its 1D projection | `pink::SimplexNoise`; same permutation table, gradients, skewing and scaling; per-instance state |
| `Utils.ts` | Clamp/interpolation/movement, looping white noise, biquad bandpass | C++ math helpers, `pink::Random`, `pink::FilteredNoise` |
| `TractUi.ts` | Mouse/touch geometry, visual tract display, tongue constraints, constriction profile, nasal gestures | Direct tongue coordinates, point triples, exact cosine constriction reduction and negative-diameter nasal gestures; diagnostic outputs replace display state |
| `GlottisUi.ts` | Keyboard/pointer-to-frequency/tenseness mapping, touch state | Direct Hz/tenseness/gate parameters; UI coordinates are no longer required |
| `GuiUtils.ts`, `MainUi.ts` | Browser layout, canvas drawing, help, buttons, DOM events | Replaced by SC controls, examples and help; these do not implement audio synthesis |
| `AudioPlayer.ts` | Web Audio start/stop and 1024-frame callback | Replaced by SC Synth/server lifecycle; two 512-sample internal blocks are preserved by default |

“Complete” here means the complete DSP and articulatory model. It does not mean
embedding the original browser application, synthesizing text, or automatically
generating tongue gestures for words. Array capacities, valid input bounds, and
new extension behavior are explicitly described below and in PARAMETERS.md.

## Signal path

The glottis uses the Liljencrants–Fant flow-derivative model. Target frequency and
tenseness are smoothed/interpolated; each new cycle derives `Rd`, `Ra`, `Rk`,
`Rg`, `Tp`, `Te`, `epsilon`, `alpha`, and `E0`, with the same analytical formulas
and integral approximation as the TypeScript. LF amplitude is multiplied by the
intensity envelope and the fourth root of current tenseness. Aspiration passes
through the 500 Hz bandpass, the phase/tenseness noise modulator, and a slow simplex
amplitude variation before being added to the source.

Each output sample holds that glottal excitation across two waveguide updates.
The oral tract has 44 cells, with 43 ordinary junctions and a three-way nasal
connection at index 17. The nasal tract has 28 cells. Diameters squared determine
the area ratios and reflection coefficients; waves scatter at each junction.
The mouth loses a factor of 0.999 per substep. The upstream nose has no such loss
multiplier; this is preserved. Lip and nose endpoints share the −0.85 reflection
by default. Output summation uses the source's 0.125 gain across both substeps.

Constrictions inject 1000 Hz filtered noise according to their position, diameter,
100 ms attack/release envelope, and glottal noise modulator. Injection is split
between the next two cells according to the fractional position. Opening a
previously obstructed mouth while the velum is sufficiently closed creates a
decaying transient at the former obstruction. Both the ordering and timing of
these operations match the source, including the one-update-delayed obstruction
transition caused by testing the diameter before wall movement.

## Retained source quirks

- Tract lengths remain **44 and 28 at every server sample rate**. Therefore its
  physical/formant interpretation changes with sample rate. There is no hidden
  resampling or length compensation; choose 44.1 kHz when comparing the original
  at 44.1 kHz, and 48 kHz when comparing it at 48 kHz.
- Nasal internal reflections are initialized with a **0.4 open velum** and then
  held fixed, even while the actual velum changes. Only the three-way junction
  normally changes with the velum. Custom nose overrides deliberately enable
  recalculation; the default remains faithful.
- The three-way junction uses areas at `diameter[17]` and `diameter[18]`, as
  upstream, even though the incoming oral waves use indices 16 and 17.
- `autoWobble: 0` leaves two small simplex pitch terms active. Tenseness noise
  is also retained. Neither this flag nor `vibratoAmount: 0` makes a perfectly
  stationary oscillator.
- Attack/release are scaled against 512 samples, not a fixed time independent
  of sample rate. Pitch smoothing and the “always voice” onset also depend on
  model update size.
- Internal white noise repeats after 32768 consumptions. Frication consumes one
  value per active eligible point per tract substep. Points with no effective
  thinness/openness still consume noise after passing the source's initial checks.
- Changing tongue coordinates recomputes the rest target shape, the behavior
  performed by the browser's touch handler rather than by its core synthesizer.
- Tenseness is allowed to exceed 1 **internally** during the original attack;
  the `Rd` clamp handles this. Only the public target is restricted to 0–1.

## Real-time adaptations and extensions

The TypeScript allocates new area arrays each block and splices dynamic event
lists in the processing loop. This implementation uses fixed arrays, placement
construction in SC `RTAlloc` memory, and bounded list movement. No allocation,
file access, locks, clocks, or shared mutable DSP state occur during processing.
Noise permutation tables and random streams belong to each instance, so voices
are deterministic and safe for parallel server scheduling.

The main differences are:

1. A fixed initial model update size (default 512, range 1–512), rather than an
   externally supplied buffer that may shorten the final block. SC callback
   sizes do not affect this cadence. The comparison harness renders matching
   full model blocks.
2. A seeded 32-bit LCG replaces unspecified `Math.random` for the white buffers:
   `state = (1664525 × state + 1013904223) mod 2^32`; white output is
   `2 × state / 2^32 − 1`. Both buffers are initialized in the upstream order.
   The same integer seed initializes the source's simplex permutation algorithm.
3. Gates represent touch start/end times on the **audio sample clock**, removing
   dependence on browser event timestamps. A slot retains released coordinates
   for its tail; reuse restarts the slot. Separate slots allow overlapping tails.
4. There are at most 64 shape/turbulence constrictions and 64 additional noise-only
   points. Up to 256 transients coexist. An event overflow retires the oldest.
   These explicit limits replace unbounded browser arrays.
5. Nonfinite inputs fall back to defaults; bounded controls prevent invalid indices
   and physically explosive reflection coefficients. Entirely degenerate custom
   junctions can still excite an upstream numerical instability. If a boundary
   wave becomes nonfinite or exceeds 10^12, the model clears its acoustic state
   and emits zero for that sample. This is a numerical recovery mechanism, not a
   limiter; ordinary audio is unaffected and can exceed ±1.
6. Full reset, external excitation/noise, noise gains, adjustable boundary
   reflections/movement speed, custom nose shapes, manual transient triggering,
   separate lip/nose/glottis outputs, and diagnostic streams extend access to the
   model. They are documented separately from its default behavior.

The C++ core uses double precision, as JavaScript numbers do; SuperCollider input
and output wires are float32. Floating-point contraction is disabled. The test
reference uses identical float32 controls, including the newly exposed formerly
private reflection constants, so rounding differences are not mistaken for
algorithm changes. The source's `maxAmplitude` and `noseMaxAmplitude` arrays are
also compared, not only the final sound.

## Verification strategy

`tests/compare_upstream.mjs` executes the vendored TypeScript after erasing type
annotations, syntactically expanding one parameter-property constructor, and
removing type-only imports. It does not replace DSP methods with a second port.
It instruments glottis/tract calls to capture outputs and diagnostics. A seeded
`Math.random` supplies identical buffers, and both implementations receive the
same saved control schedule. All 148 channels are compared across eight cases.

`tests/language_test.scd` compiles the actual class, rejects malformed calls,
checks multichannel expansion and array controls, and writes offline scores.
`tests/server_test.py` renders those scores through the installed scsynth and
supernova, including Apple Silicon and Intel/Rosetta and several server block
sizes. `tests/dsp_tests.cpp` checks gates, repeatability, reset, allocation behavior,
output decomposition, rates/update sizes, and pathological point/transient loads.
See VALIDATION.md for measured results and the limitations of these checks.
