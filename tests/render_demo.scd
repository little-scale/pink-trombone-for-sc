// Build a 10-second offline demonstration score, without playing sound.
(
var root = thisProcess.nowExecutingPath.dirname.dirname;
var def;
try {
    def = SynthDef(\pinkDemo, {
        var tongue = EnvGen.kr(Env([12.9, 27, 17, 24, 20], [2, 2, 2, 2]));
        var diameter = EnvGen.kr(Env([2.43, 2.1, 3.2, 2.3, 2.7], [2, 2, 2, 2]));
        var nose = EnvGen.kr(Env([0.01, 0.01, 0.4, 0.4, 0.01], [3, 0.5, 2, 0.5]));
        var touch = LFPulse.kr(2.4, 0, 0.22) * (Sweep.kr > 5);
        var closure = LFPulse.kr(1.5, 0, 0.15) * (Sweep.kr > 7);
        var envelope = EnvGen.kr(Env([0, 1, 1, 0], [0.05, 9.5, 0.4]), doneAction: 2);
        var voice = PinkTrombone.ar(freq: 145, tenseness: 0.65,
            tongueIndex: tongue, tongueDiameter: diameter, velum: nose,
            autoWobble: 0, constrictions: [[30, 0.5, touch], [34, 0.1, closure]], seed: 6001);
        Out.ar(0, (LeakDC.ar(voice[0]) * envelope * 0.55).dup);
    });
    Score([
        [0, [\d_recv, def.asBytes]], [0, [\s_new, \pinkDemo, 1000, 0, 0]],
        [10, [\c_set, 0, 0]]
    ]).write(root +/+ "build/demo.osc");
    "PASS: demo score generated".postln;
    0.exit;
} { |error| error.reportError; 1.exit };
)
