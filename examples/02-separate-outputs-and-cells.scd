s.boot;

// Mouth left, nose right. These are the actual contributions to the mixed voice.
(
x = {
    var voice = PinkTrombone.ar(velum: 0.4, tongueIndex: 24, seed: 18);
    [voice[1], voice[2]] * 0.4
}.play;
)
x.free;

// A tract filter driven entirely by another oscillator.
(
x = {
    PinkTrombone.ar(
        glottisGain: 0, excitation: Saw.ar(90) * 0.15,
        tongueIndex: SinOsc.kr(0.2).range(12, 29), tongueDiameter: 2.2,
        autoWobble: 0
    )[0].dup
}.play;
)
x.free;

// Control every mouth cell directly. -1 restores the tongue-derived target.
(
SynthDef(\pinkIndividualCells, {
    var cells = NamedControl.kr(\cells, Array.fill(44, -1));
    var nose = NamedControl.kr(\nose, Array.fill(28, -1));
    var voice = PinkTrombone.ar(diameters: cells, noseDiameters: nose, seed: 27);
    Out.ar(0, (voice[0] * 0.3).dup);
}).add;
)
x = Synth(\pinkIndividualCells);
x.setn(\cells, Array.fill(44, { |i| 1.1 + (i * 0.3).sin * 0.4 }));
x.setn(\cells, Array.fill(44, -1));
x.free;

// Diagnostic channels are audio-rate; reduce to control rate to report geometry.
(
x = {
    var voice = PinkTrombone.ar(diagnostics: 1, tongueIndex: SinOsc.kr(0.3).range(12, 29));
    A2K.kr(voice[76 + 20]).poll(2, "actual mouth cell 20 diameter");
    (voice[0] * 0.3).dup
}.play;
)
x.free;
