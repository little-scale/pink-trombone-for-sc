// Ten-voice randomized PinkTrombone chords through MiVerb.
// Based on little-scale's example. Requires the separate mi-UGens plugin (MiVerb).
// Evaluate each numbered block separately. Set memSize before boot; if the server
// is already running, quit it first so the new memory setting takes effect.
// This replaces the pinky SynthDef from example 03: dur now sets envelope release
// in seconds. decay remains exposed for compatibility but is unused here.
// ! ~rep produces ten values per event, expanded into ten voices with a short strum.
// excitation is a constant external input per voice; LeakDC removes its DC bias.

// 1. Boot, allocate the stereo effects bus, and send both SynthDefs.
(
s.options.memSize = 8192 * 32;
s.waitForBoot {
    ~verb = Bus.audio(s, numChannels: 2);
    SynthDef(\verb, { arg in = 0, out = 0, fx = 0.9, time = 0.5;
        Out.ar(out, MiVerb.ar(In.ar(in, 2), time, fx));
    }).add;
    SynthDef(\pinky, {
        arg freq = 140, amp = 0.05, gate = 1, pan = 0, out = 0,
            dur = 0.5, attack = 0.1, decay = 1, curve = -10,
            tenseness = 0.6, tongueIndex = 4, tongueDiameter = 2,
            velum = 0.01, alwaysVoice = 1, autoWobble = 0,
            vibratoAmount = 0.02, vibratoFrequency = 5,
            aspiration = 0.5, frication = 1, excitation = 3,
            glottisGain = 1, glottalReflection = 0.75, lipReflection = -0.85,
            movementSpeed = 15, transientTrig = 0, transientPosition = 30,
            transientStrength = 0.3, transientLife = 1;
        var sig, env;

        env = EnvGen.ar(
            Env.perc(attack, dur, 1, curve),
            gate,
            doneAction: 2
        );

        sig = PinkTrombone.ar(
            freq: freq,
            tenseness: tenseness,
            tongueIndex: tongueIndex,
            tongueDiameter: tongueDiameter,
            velum: velum,
            gate: gate,
            alwaysVoice: alwaysVoice,
            autoWobble: autoWobble,
            vibratoAmount: vibratoAmount,
            vibratoFrequency: vibratoFrequency,
            aspiration: aspiration,
            frication: frication,
            excitation: excitation,
            glottisGain: glottisGain,
            glottalReflection: glottalReflection,
            lipReflection: lipReflection,
            movementSpeed: movementSpeed,
            transientTrig: transientTrig,
            transientPosition: transientPosition,
            transientStrength: transientStrength,
            transientLife: transientLife
        )[0];

        sig = LeakDC.ar(sig) * env * amp;
        sig = Pan2.ar(sig, pan);
        Out.ar(out, sig);
    }).add;
    s.sync;
    "Ready: evaluate block 2 to play.".postln;
};
)

// 2. Play. The voice group runs before the reverb so both bus channels reach it.
(
~rep = 10;
~pinkyVoices = Group.head(s);
v = Synth.after(~pinkyVoices, \verb, [\in, ~verb, \out, 0, \fx, 0.4]);
p = Pbind(
    \instrument, \pinky,
    \group, ~pinkyVoices,
    \degree, Pwhite(0, 7 ! ~rep, inf),
    \strum, Pwhite(0.0, 0.01, inf),
    \octave, Pwhite(3, 5 ! ~rep, inf),
    \amp, Pwhite(0.5, 0.5 ! ~rep, inf),
    \attack, Pwhite(0.001, 0.1 ! ~rep, inf),
    \pan, Pwhite(-1.0, 1.0 ! ~rep, inf),
    \dur, Pwhite(0.05, 1.25 ! ~rep, inf),
    \delta, Pseq(Pgeom(1, 1.3, 8).asStream.all.normalizeSum.reverse, inf) * 2,
    \decay, Pwhite(0.1, 5 ! ~rep, inf),
    \curve, Pwhite(-10.0, -1.0 ! ~rep, inf),
    \tenseness, Pwhite(0.4, 0.9 ! ~rep, inf),
    \tongueIndex, Pwhite(12.0, 29.0 ! ~rep, inf),
    \tongueDiameter, Pwhite(2.05, 3.5 ! ~rep, inf),
    \velum, Pwhite(0.01, 0.4 ! ~rep, inf),
    \alwaysVoice, Pwhite(0, 1 ! ~rep, inf),
    \autoWobble, Pwhite(0, 1 ! ~rep, inf),
    \vibratoAmount, Pwhite(0.0, 0.05 ! ~rep, inf),
    \vibratoFrequency, Pwhite(3.0, 8.0 ! ~rep, inf),
    \aspiration, Pwhite(0.1, 1.0 ! ~rep, inf),
    \frication, Pwhite(0.2, 1.5 ! ~rep, inf),
    \excitation, Pwhite(0.0, 0.4 ! ~rep, inf),
    \glottisGain, Pwhite(0.5, 1.2 ! ~rep, inf),
    \glottalReflection, Pwhite(0.6, 0.85 ! ~rep, inf),
    \lipReflection, Pwhite(-0.9, -0.7 ! ~rep, inf),
    \out, ~verb
).play;
)

// 3. Stop notes and release this example's nodes and bus before running it again.
(
p.stop;
~pinkyVoices.freeAll;
v.free;
~pinkyVoices.free;
~verb.free;
)
