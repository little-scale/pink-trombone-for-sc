// Evaluate one parenthesized block at a time after installing the extension.
s.boot;

(
SynthDef(\pinkArticulation, { |out=0, freq=140, tense=0.6, tongueIndex=20,
    tongueDiameter=2.7, velum=0.01, consonant=0, closure=0, gate=1, amp=0.3|
    var voice = PinkTrombone.ar(
        freq: freq, tenseness: tense,
        tongueIndex: tongueIndex, tongueDiameter: tongueDiameter,
        velum: velum, gate: gate, alwaysVoice: 0, autoWobble: 0,
        constrictions: [[30, 0.5, consonant], [34, 0.1, closure]],
        seed: 9173
    );
    Out.ar(out, (voice[0] * amp).dup);
}).add;
)

x = Synth(\pinkArticulation);
x.set(\tongueIndex, 27, \tongueDiameter, 2.1); // narrow tongue shape
x.set(\tongueIndex, 15, \tongueDiameter, 3.2); // more open shape
x.set(\velum, 0.4); // nasal
x.set(\velum, 0.01, \consonant, 1); // fricative
x.set(\consonant, 0, \closure, 1); // close the tract
x.set(\closure, 0); // release creates an automatic plosive
x.set(\freq, 220, \tense, 0.85);
x.set(\gate, 0); // release the glottis; the Synth remains allocated
x.free;

// Continuous articulation with the original voice source.
(
x = {
    var voice = PinkTrombone.ar(
        freq: 140, autoWobble: 0,
        tongueIndex: SinOsc.kr(0.17).range(12, 29),
        tongueDiameter: SinOsc.kr(0.11).range(2.1, 3.4),
        velum: LFPulse.kr(0.35).range(0.01, 0.4),
        constrictions: [[30, 0.5, LFPulse.kr(2, 0, 0.2)]], seed: 12345
    );
    (voice[0] * 0.3).dup
}.play;
)
x.free;
