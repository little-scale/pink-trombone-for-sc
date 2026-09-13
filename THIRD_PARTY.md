# Attribution and licenses

The DSP is a C++ port of [chdh/pink-trombone-mod](https://github.com/chdh/pink-trombone-mod),
version 0.1.0, commit `359c2d3b42b10280404c1650dc601902112b4c90`.
Original Pink Trombone: Copyright 2017 Neil Thapen. Modularization and TypeScript
conversion: Christian d'Heureuse. The upstream source is MIT licensed and is
preserved under `vendor/pink-trombone-mod`, with its original copyright notice.
The standalone `src/PinkTromboneDSP.hpp` and `src/PinkTromboneDSP.cpp` port are
also available under the MIT terms in `LICENSE-DSP`.

Simplex noise derives from public-domain code by Stefan Gustavson, with
optimizations by Peter Eastman and JavaScript conversion by Joseph Gentle.
Their original attribution is retained in the vendored `NoiseGenerator.ts`.

The SuperCollider interface uses headers from SuperCollider 3.14.1, commit
`426edf6d8742e1cc3bd85b51ca0c4e595d37a903`. These are preserved under
`vendor/supercollider/include` with their original notices. See that directory's
`COPYING` and individual headers. The SuperCollider plugin adapter, language
class, and combined plugin distribution are supplied under GPL-3.0-or-later;
the full license is in `LICENSE`. The MIT upstream code retains its MIT license.

Source code, headers, build scripts, and tests sufficient to rebuild the plugins
are included in the source distribution. No third-party runtime library is
bundled into the plugin; macOS supplies the C++ runtime and system libraries.
