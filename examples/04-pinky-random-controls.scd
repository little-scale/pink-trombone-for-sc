// First evaluate the SynthDef in 03-pinky-all-controls.scd.
// Every named SynthDef control appears below. Values are chosen once per note.
// gate stays positive to start Env.perc; out stays on the main stereo bus pair.
(
p = Pbind(
    \instrument, \pinky,

    \freq, Pwhite(90.0, 320.0, inf),
    \amp, Pwhite(0.05, 0.3, inf),
    \gate, Pwhite(1, 1, inf),
    \pan, Pwhite(-1.0, 1.0, inf),
    \out, Pwhite(0, 0, inf),

    \dur, Pwhite(0.5, 2.0, inf),
    \delta, Pwhite(0.15, 0.5, inf), // Event spacing, in addition to SynthDef controls.
    \attack, Pwhite(0.01, 0.15, inf),
    \decay, Pwhite(0.2, 1.5, inf),
    \curve, Pwhite(-10.0, -2.0, inf),

    \tenseness, Pwhite(0.4, 0.9, inf),
    \tongueIndex, Pwhite(12.0, 29.0, inf),
    \tongueDiameter, Pwhite(2.05, 3.5, inf),
    \velum, Pwhite(0.01, 0.4, inf),
    \alwaysVoice, Pwhite(0, 1, inf),
    \autoWobble, Pwhite(0, 1, inf),

    \vibratoAmount, Pwhite(0.0, 0.05, inf),
    \vibratoFrequency, Pwhite(3.0, 8.0, inf),
    \aspiration, Pwhite(0.1, 1.0, inf),
    \frication, Pwhite(0.2, 1.5, inf),
    \excitation, Pwhite(0.0, 0.3, inf), // Constant external excitation per note; 0 disables it.
    \glottisGain, Pwhite(0.5, 1.2, inf),
    \glottalReflection, Pwhite(0.6, 0.85, inf),
    \lipReflection, Pwhite(-0.9, -0.7, inf),
    \movementSpeed, Pwhite(5.0, 30.0, inf),

    \transientTrig, Pwhite(0, 1, inf),
    \transientPosition, Pwhite(0, 43, inf),
    \transientStrength, Pwhite(0.05, 0.35, inf),
    \transientLife, Pwhite(0.05, 0.3, inf)
).play;
)

p.stop;

// frication affects sound only when the UGen has active turbulence/constriction points.
// alwaysVoice has no effect while gate=1 keeps the glottis touched.
// freq supplies Hertz directly here, so degree is omitted.
