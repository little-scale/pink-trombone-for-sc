# PinkTrombone for SuperCollider

A native C++ UGen implementing the complete sound-generating model in
[chdh/pink-trombone-mod](https://github.com/chdh/pink-trombone-mod/tree/359c2d3b42b10280404c1650dc601902112b4c90/src),
including its articulatory controls. Builds for macOS 11 or newer on Apple Silicon
and Intel, for both **scsynth and supernova**. Built and tested with SuperCollider 3.14.1.

The package contains **ad-hoc-signed universal binaries**. They have verified code
signatures but are **not Developer ID signed or Apple notarized**. No Developer ID
identity was available on the build Mac. See [signing](docs/SIGNING.md) for the
distinction and the command for signing with your own identity.

## Install and play

Download the [macOS universal package](https://github.com/little-scale/pink-trombone-for-sc/raw/refs/heads/main/dist/PinkTromboneSC-1.0.0-macOS-universal.zip)
([SHA-256 checksums](dist/SHA256SUMS.txt)). Unzip it and copy its `PinkTrombone` folder to
the location returned by `Platform.userExtensionDir` in SuperCollider. On macOS:

```text
~/Library/Application Support/SuperCollider/Extensions/
```

Alternatively, run the included `install.command`; it preserves any previous
version outside the Extensions folder. Restart the interpreter and server.
The installer does not alter Gatekeeper or remove quarantine attributes.

```supercollider
s.boot;

// Outputs: [mixed voice, mouth, nose, raw glottal source].
// Select [0] for ordinary listening; the four outputs are not a stereo layout.
x = { LeakDC.ar(PinkTrombone.ar(freq: 140, autoWobble: 0)[0]).dup * 0.4 }.play;
x.free;
```

For many simultaneous voices, increase the server's real-time memory **before boot**:

```supercollider
s.options.memSize = 65536; // KiB; each voice uses roughly 0.55 MiB.
```

## DC offset and waveform asymmetry

For normal listening, use `LeakDC.ar` on the mixed voice output, **before the
amplitude envelope and panning**. This removes persistent DC bias while retaining
the voice waveform's natural positive/negative asymmetry. The UGen exposes the
original model's output without inserting a DC filter internally.

```supercollider
// Inside a SynthDef, with env, amp, pan and out already defined:
sig = PinkTrombone.ar(excitation: 0)[0];
sig = LeakDC.ar(sig, 0.995) * env * amp;
sig = Pan2.ar(sig, pan);
Out.ar(out, sig);
```

Keep `excitation: 0` when using only the internal voice. A constant such as
`excitation: 3` injects DC into the tract; this is an external excitation input,
not a voice intensity setting. For a percussive envelope, use
`Env.perc(attack, decay, 1, curve)`: the third argument is its peak level, and the
fourth is its curve. Putting a negative curve value in the third argument instead
inverts and scales the signal. Unequal positive and negative peaks alone do not
prove DC offset; DC is the signal's nonzero long-term average.

See [SuperCollider's LeakDC documentation](https://docs.supercollider.online/Classes/LeakDC.html).

## Playable controls

For a percussive `pinky` SynthDef with named voice controls, `LeakDC`, stereo
panning, and a `Pbind`, see [the complete example](examples/03-pinky-all-controls.scd).
Then try [randomizing every exposed SynthDef control with Pwhite](examples/04-pinky-random-controls.scd).

For little-scale's ten-voice randomized chords through stereo reverb, see
[the MiVerb example](examples/05-pinky-chords-and-reverb.scd). It requires the
separate **mi-UGens** plugin for `MiVerb`. The example includes server memory
setup, a stereo effects bus, voice-before-effect ordering, and cleanup. In this
version, `dur` sets the envelope's release time in seconds; the exposed `decay`
control is retained from the original patch but is unused.

```supercollider
(
SynthDef(\pinkVoice, { |out=0, freq=140, tongueIndex=20, tongueDiameter=2.7,
    velum=0.01, tense=0.6, consonant=0, gate=1, amp=0.3|
    var voice = PinkTrombone.ar(
        freq: freq, tenseness: tense, tongueIndex: tongueIndex,
        tongueDiameter: tongueDiameter, velum: velum,
        gate: gate, alwaysVoice: 0, autoWobble: 0,
        constrictions: [[30, 0.5, consonant]], seed: 1234
    );
    Out.ar(out, (LeakDC.ar(voice[0]) * amp).dup);
}).add;
)

x = Synth(\pinkVoice);
x.set(\tongueIndex, 27, \tongueDiameter, 2.1);
x.set(\velum, 0.4, \consonant, 1);
x.set(\consonant, 0, \gate, 0);
x.free;
```

All ordinary parameters accept scalar, control-rate, or audio-rate inputs. The
model preserves upstream smoothing: pitch and shape targets are consumed every
`modelBlockSize` samples, and LF coefficients change at glottal cycle boundaries.
The default is the original 512 samples, independent of the server's block size.
Set `modelBlockSize: 1` for per-sample target updates; this costs more CPU and
changes the source's block-dependent articulation and attack behavior. Audio
excitation, external noise, noise gains, triggers, and constriction gate edges are
processed every sample. Use `Lag` yourself when smoothing SuperCollider controls
is musically desirable.

## Inputs and outputs

See the complete [parameter reference](docs/PARAMETERS.md), the built-in
`PinkTrombone` help page, and [examples](examples/01-voice-and-articulation.scd).
The interface includes:

- Pitch, tenseness, touch/gate, always-voice, auto-wobble, and vibrato controls.
- Tongue position and diameter, velum opening, wall movement speed, and reflections.
- Up to 64 constriction triples and 64 independent turbulence triples. Each triple
  is `[index, diameter, gate]`; arrays retain their roles as individual cells/points.
- 44 individual oral diameter targets and 28 nasal diameter overrides.
- Automatic plosive releases and a manual transient trigger with position,
  strength, lifetime, and decay controls.
- External tract excitation, optional external aspiration/frication noise, and
  independent source/noise gains.
- Four audio outputs, or 148 audio-rate outputs with `diagnostics: 1`, including
  every cell's amplitude envelope and current diameter.

`seed`, `modelBlockSize`, `diagnostics`, and the array sizes are fixed when the
SynthDef is built. All other values can be signals. Use distinct seeds for
independent voices. `autoWobble: 0` disables only the large wobble terms; the source's
small simplex fluctuations remain, even with `vibratoAmount: 0`.

## Fidelity and scope

The LF glottis, simplex modulation, looping 32768-sample filtered noise sources,
44-cell oral tract, 28-cell nose, two tract substeps, reflection interpolation,
asymmetric wall movement, 100 ms fricative attack/release, transient decay, and
0.125 output summation gain are preserved. The browser canvas, mouse/touch event
machinery, and Web Audio player are replaced by SuperCollider controls and server
audio. This is a synthesizer, not a text-to-speech engine or a phoneme sequencer.

The original chooses random seeds from wall-clock time and `Math.random`; this
port uses a documented repeatable seed and independent per-instance state.
The test harness seeds the original identically and accounts for float32 UGen
input precision. See [source analysis](docs/SOURCE_ANALYSIS.md) for the complete
mapping, retained quirks, and explicit bounded real-time adaptations.

## Build, test, and sign

The required SuperCollider 3.14.1 headers and original TypeScript sources are
vendored and pinned, so compilation needs no network access or npm packages.
Install Apple Command Line Tools and CMake. Clone this repository and build:

```sh
git clone https://github.com/little-scale/pink-trombone-for-sc.git
cd pink-trombone-for-sc
./scripts/build_macos.sh
node tests/compare_upstream.mjs
/Applications/SuperCollider.app/Contents/MacOS/sclang -D --include-path "$PWD/Classes" "$PWD/tests/language_test.scd"
python3 tests/server_test.py
./scripts/package_macos.sh
```

The reference comparison requires Node 22.13 or later; server tests require the
SuperCollider application, Python 3, and Rosetta for their Intel-on-Apple-Silicon
checks. The language test compiles classes via an additional path without
installing anything. The server tests render offline and do not play sound.

For another header version, configure `-DSC_PATH=/path/to/supercollider-source`.
To check the core with memory/undefined-behavior sanitizers:

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DENABLE_SANITIZERS=ON -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

The C++ process loop does not allocate, take locks, access files, or share mutable
DSP state. Construction uses SuperCollider's real-time allocator; allocation
failure leaves a silent UGen that can be safely freed.

See [validation results](docs/VALIDATION.md). The DSP retains the original MIT
license; the SuperCollider distribution is GPL-3.0-or-later. See
[THIRD_PARTY.md](THIRD_PARTY.md), `LICENSE`, and `LICENSE-DSP`.
