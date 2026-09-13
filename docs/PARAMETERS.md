# Parameter reference

`PinkTrombone.ar(...)` creates one native multi-output UGen. Positional argument
order is defined in `Classes/PinkTrombone.sc`; keyword arguments are recommended.
Ordinary inputs support `.ir`, `.kr`, and `.ar`. There is no `.kr` output method.

## Voice and anatomy

| Argument | Default | Range / meaning |
| --- | ---: | --- |
| `freq` | 140 | Target fundamental Hz; clipped to 10–0.2 × server sample rate. The LF cycle and source pitch glide remain active. |
| `tenseness` | 0.6 | 0–1; low is breathy, high is tense. This is the target, before simplex variation and attack adjustments. |
| `tongueIndex` | 12.9 | 0–43 cell coordinate. The browser's tongue control typically covers 12–29; direct controls allow a larger range. |
| `tongueDiameter` | 2.43 | 2.05–3.5, matching the browser's tongue-control bounds. |
| `velum` | 0.01 | 0–3 nasal entrance diameter; original closed/open targets are 0.01/0.4. |
| `gate` | 0 | Positive means the glottis is touched (`isTouched`). **To release with this gate, set `alwaysVoice: 0`.** Does not free the Synth. |
| `alwaysVoice` | 1 | Positive keeps the source sounding without a touch, including the original attack behavior. |
| `autoWobble` | 1 | Positive adds the two large simplex pitch-wobble terms. Small random pitch/tenseness motion is always retained. |
| `vibratoAmount` | 0.005 | 0–1 fractional sinusoidal pitch depth. |
| `vibratoFrequency` | 6 | 0–100 Hz sinusoidal vibrato rate. |
| `movementSpeed` | 15 | 0–1000; original wall movement speed, described upstream as cm/s. 0 freezes the current tract/velum shape. |
| `glottalReflection` | 0.75 | −0.999 to +0.999; glottal boundary reflection coefficient. |
| `lipReflection` | −0.85 | −0.999 to +0.999; reflection at both lip and nostril terminations. |

The default model updates every 512 samples. The source scales intensity attack
by `0.13 × blockSize/512` and release by `0.05 × blockSize/512`, and then clamps to
0–1. Consequently those envelope times depend on sample rate, as in the original.
Frequency/tenseness targets and the anatomical shape are used at model boundaries.
The glottal LF coefficients update at the next waveform cycle boundary.

## Excitation and noise

| Argument | Default | Range / meaning |
| --- | ---: | --- |
| `aspiration` | 1 | 0–10 multiplier for glottal aspiration. Does not turn off the internal noise clock. |
| `frication` | 1 | 0–10 multiplier for all turbulence sources. |
| `excitation` | 0 | External signal, −100 to +100; added to the glottal input of the waveguide, sample by sample. |
| `glottisGain` | 1 | 0–10 multiplier of the internal glottis entering the tract. Set 0 to process external excitation alone. Output 3 still gives the internal glottis. |
| `aspirationNoise` | 0 | Optional external white/excitation signal, −100 to +100, feeding the original 500 Hz/Q 0.5 bandpass. |
| `fricationNoise` | 0 | Optional external white/excitation signal, −100 to +100, feeding the original 1000 Hz/Q 0.5 bandpass at twice the server rate. |
| `externalNoise` | 0 | Integer bitmask: 0 = both internal; 1 = external aspiration; 2 = external frication; 3 = both external. |
| `noiseModulator` | −1 | Negative uses the glottis-derived turbulence modulation; 0–1 overrides it. Aspiration retains its own original glottal modulation. |

External samples are held for both tract substeps and for each turbulence point's
noise consumption within a substep. Internal frication noise is consumed once per
active point per substep, preserving the original shared-stream ordering.
With internal noise selected, the two separate 32768-sample white-noise buffers
loop indefinitely; they are not freshly randomized every block.

## Individual cells

`diameters` is a flat array of **44** numbers or signals. Its default is 44 values
of −1. A nonnegative value overrides that cell's oral target diameter (0–10); any
negative value selects the tongue/rest shape. Constrictions are applied after
these target overrides. Current diameters approach their targets at the original
asymmetric speed; these are targets, not instantaneous waveguide edits.

`noseDiameters` is a flat array of **28** numbers or signals, also defaulting to
−1. Nonnegative values override the nasal shape (0–10). Element 0 replaces the
velum target and retains its opening/closing slew. Elements 1–27 are applied at
the next model boundary. Removing an override restores the original nasal profile.
When any override is present, nasal reflection coefficients are recomputed from
the overridden target profile at model boundaries. Without overrides, the source's
fixed nasal reflections, computed once with an open 0.4 velum, are preserved.

Use named control arrays to change cells from a running Synth:

```supercollider
SynthDef(\pinkCells, {
    var cells = NamedControl.kr(\cells, Array.fill(44, -1));
    Out.ar(0, PinkTrombone.ar(diameters: cells)[0].dup * 0.3);
}).add;
x = Synth(\pinkCells);
x.setn(\cells, Array.fill(44, 1.2));
```

## Constrictions and turbulence points

`constrictions` and `turbulencePoints` default to empty arrays. Each accepts up to
**64 triples** of `[index, diameter, gate]`; all three elements may be signals.
Their counts and ordering are fixed in the SynthDef. Each slot maintains its own
touch timing. A gate transition from nonpositive to positive starts a touch;
release starts its 100 ms frication tail, preserving the last active coordinates.
Reopening the same slot starts a new touch. Use a different slot if overlapping
the previous release tail is required.

`constrictions` changes the tract and creates turbulence. Its diameter is the
original **touch diameter**, not the resulting cell diameter. For example, 0.5
produces a 0.2 central target after the original 0.3 offset is subtracted. A touch
diameter of 0.3 or less can close the tract; 0.3–0.7 is the useful frication band.

Positions below 2 or at/above 44 do not constrict the tract. Positions are in cell
coordinates, with a cosine taper whose width decreases from 10 to 5 toward the
lips. A touch beyond index 17 with diameter below −0.8 opens the velum to 0.4.
Diameters below −1.65 have only the velum effect. These rules are copied from
`TractUi.handleTouches` and `reduceTargetDiametersByTouch`.

`turbulencePoints` injects noise with no shape or velum changes. Its diameter is
the same unshifted value used by the upstream turbulence function. No turbulence
is injected for diameter ≤0 or outside its thinness/openness band. A point at the
last cells can inject little/no noise because its injection sites lie past the lips.

```supercollider
PinkTrombone.ar(
    constrictions: [[30, 0.5, LFPulse.kr(2)], [22, 0.1, LFPulse.kr(0.7)]],
    turbulencePoints: [[35.25, 0.5, 1]]
)[0]
```

## Transients, reset, and configuration

| Argument | Default | Range / meaning |
| --- | ---: | --- |
| `transientTrig` | 0 | Rising edge starts a plosive event; detected every sample. |
| `transientPosition` | 30 | Oral cell 0–43, truncated to an integer. |
| `transientStrength` | 0.3 | 0–10 initial event strength. |
| `transientLife` | 0.2 | 0–10 seconds. |
| `transientExponent` | 200 | 0–10000; amplitude decays as `strength × 2^(−exponent × age)`. |
| `reset` | 0 | Rising edge clears acoustic, envelope, transient, and noise-filter state and rewinds the seeded noise streams. |
| `seed` | 1 | **Initial numeric constant**, integer 0–16777215. Separate voices should normally use separate seeds. |
| `modelBlockSize` | 512 | **Initial numeric constant**, integer 1–512. Independent of scsynth/supernova's block size. |
| `diagnostics` | 0 | **Initial numeric constant**, 0 for 4 outputs, 1 for 148 outputs. |

Automatic closure-release plosives are retained without needing `transientTrig`.
They use the source defaults (0.3 strength, 0.2 s lifetime, exponent 200). Manual
event settings affect manual triggers only. At most 256 simultaneous events are
stored; overload retires the oldest event. Reset is a full, repeatable restart;
the upstream browser's method called `reset()` only prepared block parameters.

Nonfinite inputs use a documented default from the tables; finite inputs outside
the supported ranges are clipped. Malformed array lengths and nonconstant
initialization arguments are rejected when building the SynthDef. Demand-rate
inputs are not accepted.

## Output channel map

All outputs are audio-rate signals. Indexing is zero based.

| Channels | Meaning |
| --- | --- |
| 0 | Mixed voice: `(lip1 + nose1 + lip2 + nose2) × 0.125`, matching the original synthesizer. |
| 1 | Mouth only: `(lip1 + lip2) × 0.125`. |
| 2 | Nose only: `(nose1 + nose2) × 0.125`. |
| 3 | Raw glottal source with aspiration, before `glottisGain`, external excitation, or vocal-tract filtering. |
| 4–47 | Oral `maxAmplitude[0..43]`, when diagnostics are enabled. |
| 48–75 | Nasal `noseMaxAmplitude[0..27]`. |
| 76–119 | Current oral `diameter[0..43]`. |
| 120–147 | Current `noseDiameter[0..27]`. |

`output[0]` equals `output[1] + output[2]` to floating-point rounding. Diagnostic
amplitudes use the source's peak tracking with a 0.9999 decay per tract substep.
Diameters and envelopes are useful for monitoring/modulation, not speaker feeds.
No limiter, normalization, DC blocker, or resampler is inserted. Apply those
explicitly after the UGen when desired. Signals can exceed ±1, particularly the
raw glottis and transients.
