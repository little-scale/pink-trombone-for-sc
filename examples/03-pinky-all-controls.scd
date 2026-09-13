// Evaluate the SynthDef block first, then the Pbind block.
// Every PinkTrombone input from the supplied patch is a named SynthDef control.
// excitation=3 is preserved from that patch; use 0 for internal-voice-only synthesis.
// tongueDiameter=2 is preserved; the UGen clips it to its minimum of 2.05.
// transientLife is independent of decay, with the same initial value of 1 second.
(
SynthDef(\pinky, {
    arg freq = 140, amp = 0.05, gate = 1, pan = 0, out = 0,
        dur = 0.5, attack = 0.05, decay = 1, curve = -10,
        tenseness = 0.6, tongueIndex = 4, tongueDiameter = 2,
        velum = 0.01, alwaysVoice = 1, autoWobble = 0,
        vibratoAmount = 0.02, vibratoFrequency = 5,
        aspiration = 0.5, frication = 1, excitation = 3,
        glottisGain = 1, glottalReflection = 0.75, lipReflection = -0.85,
        movementSpeed = 15, transientTrig = 0, transientPosition = 30,
        transientStrength = 0.3, transientLife = 1;
    var sig, env;

    env = EnvGen.ar(
        Env.perc(attack, decay, 1, curve),
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
)

// dur/delta schedule pattern events; this percussive envelope lasts attack + decay.
(
p = Pbind(
    \instrument, \pinky,
    \degree, Pseq([0, 3, 5, 7, 9], inf),
    \dur, 0.5,
    \delta, 0.25,
    \amp, 0.05
).play;
)

p.stop;
