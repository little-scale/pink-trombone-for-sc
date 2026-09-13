# Completion audit

Objective: implement an accurate emulation of the specified Pink Trombone source
as a signed macOS SuperCollider UGen, exposing its controls and audio outputs.
Audit date: 13 September 2026.

| Requirement | Evidence and outcome |
| --- | --- |
| Analyze the requested upstream source | Every vendored TypeScript file was byte-compared with the retrieved Git revision `359c2d3b42b10280404c1650dc601902112b4c90`. SOURCE_ANALYSIS.md maps every source module, signal-generating behavior, and retained quirk. |
| Accurately implement the complete sound model | `PinkTromboneDSP.cpp` implements the LF glottis, both filters/noise streams, simplex modulation, 44/28-cell waveguides, junctions, tongue shaping, constrictions, turbulence, transients, and monitoring. The actual upstream TypeScript is the reference oracle. Eight schedules and 148 channels pass at a 10⁻¹⁰ tolerance; measured maximum audio error is below 10⁻¹⁵. |
| Expose inputs as UGen parameters | `Classes/PinkTrombone.sc` and the adapter's `pink::Input` map agree, including variable point triples and fixed oral/nasal arrays. Language/SynthDef checks and server renders cover scalar, control, and audio inputs, array controls, and multichannel expansion. PARAMETERS.md lists the complete interface and timing. |
| Expose outputs at audio rate | Four primary outputs are mixed voice, mouth, nose, and glottis. Diagnostic mode exposes 148 audio-rate channels. Native server renders verify both layouts. The diagnostic streams retain each cell's envelope and diameter. |
| Work as a native macOS UGen | Universal arm64/x86_64 `.scx` files compile from the current tree and load in SuperCollider 3.14.1 scsynth and supernova. Native and Intel/Rosetta default renders match the float32 reference exactly. Nineteen server checks pass, including allocation failure in scsynth. |
| Supply signed binaries | Both architecture slices of both plugins pass `codesign --verify --strict --all-architectures`. The signatures are ad-hoc, as disclosed throughout the package. Developer ID signing and notarization are not claimed. |
| Deliver a usable and reviewable result | The binary ZIP contains plugins, the SC class, help, installer, examples/demo, documentation, and licenses. The separate source ZIP contains the implementation, pinned dependencies, build/sign/package scripts, and reproducible tests. Archives and current files are compared byte for byte. Checksum entries use portable filenames. |
| Respect real-time processing constraints | Code inspection, the allocation counter, stress tests, release tests, and native UndefinedBehaviorSanitizer checks pass. State belongs to each instance; processing contains no allocations, locks, or file/network access. Construction/destruction use SC's real-time allocator. |

The requested synthesis model is implemented. Browser drawing and event plumbing
are represented by explicit synthesizer controls and diagnostics; no browser UI
or text-to-speech layer was requested. Fixed real-time capacities, admissible
control ranges, seeded randomness, and numerical recovery are documented in
SOURCE_ANALYSIS.md and PARAMETERS.md and are not presented as unrestricted
equivalence for arbitrary invalid JavaScript state.

AddressSanitizer is an unavailable optional check on this Mac: an empty-program
probe reproduces its startup failure before application code. Native UBSan and
the other verification paths pass. See VALIDATION.md for the evidence and the
limits of offline, Rosetta, and local ad-hoc-signature testing.
