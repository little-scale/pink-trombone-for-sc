// Run with --include-path pointing at ../Classes. Writes scores without playing sound.
(
var root = thisProcess.nowExecutingPath.dirname.dirname;
var build = root +/+ "build";
var defs, valid, expectError;
try {
    defs = [
        SynthDef(\pinkDefault, { Out.ar(0, PinkTrombone.ar(seed: 12039)) }),
        SynthDef(\pinkAudio, {
            var x = PinkTrombone.ar(
                freq: SinOsc.ar(8).range(100, 220),
                tongueIndex: SinOsc.ar(3).range(12, 29),
                velum: SinOsc.ar(2).range(0.01, 0.4),
                modelBlockSize: 1,
                constrictions: [[30, 0.5, LFPulse.ar(4)], [24, 0.1, LFPulse.kr(3)]],
                turbulencePoints: [[34, 0.5, LFPulse.kr(2)]],
                transientTrig: Impulse.ar(13), reset: Impulse.ar(1),
                excitation: SinOsc.ar(200) * 0.02, seed: 32);
            Out.ar(0, x)
        }),
        SynthDef(\pinkDiagnostics, {
            var x = PinkTrombone.ar(diagnostics: 1, seed: 42);
            if(x.size != 148) { Error("Wrong diagnostic channel count").throw };
            Out.ar(0, x)
        }),
        SynthDef(\pinkArrays, {
            var x = PinkTrombone.ar(
                freq: [110, 220],
                diameters: NamedControl.kr(\diameters, Array.fill(44, 1.2)),
                noseDiameters: NamedControl.kr(\noseDiameters, Array.fill(28, -1)));
            if(x.size != 2 or: { x[0].size != 4 }) { Error("Multichannel expansion failed").throw };
            Out.ar(0, x.sum)
        }),
        SynthDef(\pinkExternalNoise, {
            Out.ar(0, PinkTrombone.ar(aspirationNoise: WhiteNoise.ar, fricationNoise: WhiteNoise.ar,
                externalNoise: 3, constrictions: [[30, 0.5, 1]], alwaysVoice: 0))
        }),
        SynthDef(\pinkInvalidNumbers, {
            var bad = \bad.kr(0.6);
            Out.ar(0, PinkTrombone.ar(freq: bad, tenseness: bad,
                excitation: bad, tongueIndex: bad,
                constrictions: [[bad, bad, 1]]))
        })
    ];
    valid = 0;
    expectError = { |func|
        var caught = false;
        try { func.value } { caught = true };
        if(caught.not) { Error("Invalid input was accepted").throw };
        valid = valid + 1;
    };
    expectError.value({ SynthDef(\badCells, { PinkTrombone.ar(diameters: [1,2,3]) }) });
    expectError.value({ SynthDef(\badPoints, { PinkTrombone.ar(constrictions: [[30, 0.5]]) }) });
    expectError.value({ SynthDef(\badSize, { PinkTrombone.ar(modelBlockSize: 0) }) });
    expectError.value({ SynthDef(\badSeed, { PinkTrombone.ar(seed: -1) }) });
    expectError.value({ SynthDef(\badDiagnostic, { PinkTrombone.ar(diagnostics: 2) }) });
    defs.do { |def| def.writeDefFile(build) };
    Score([
        [0.0, [\d_recv, defs[0].asBytes]],
        [0.0, [\s_new, \pinkDefault, 1000, 0, 0]],
        [1.0, [\n_free, 1000]]
    ]).write(build +/+ "default.osc");
    defs.copyRange(1, 5).do { |def|
        Score([
            [0.0, [\d_recv, def.asBytes]],
            [0.0, [\s_new, def.name, 1000, 0, 0]],
            [0.25, [\n_set, 1000, \bad, inf]],
            [0.5, [\n_set, 1000, \bad, 0/0]],
            [1.0, [\n_free, 1000]]
        ]).write(build +/+ (def.name ++ ".osc"))
    };
    ("PASS: " ++ defs.size ++ " SynthDefs, audio/control/scalar inputs, expansion, diagnostics and " ++ valid ++ " validation failures.").postln;
    0.exit;
} { |error|
    error.reportError;
    1.exit;
};
)
