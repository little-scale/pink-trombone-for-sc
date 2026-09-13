// Full Pink Trombone synthesis model. Copyright 2017 Neil Thapen (MIT upstream).
PinkTrombone : MultiOutUGen {
    *ar { arg freq = 140, tenseness = 0.6, tongueIndex = 12.9,
        tongueDiameter = 2.43, velum = 0.01, gate = 0, alwaysVoice = 1,
        autoWobble = 1, vibratoAmount = 0.005, vibratoFrequency = 6,
        aspiration = 1, frication = 1, excitation = 0, glottisGain = 1,
        glottalReflection = 0.75, lipReflection = -0.85, movementSpeed = 15,
        transientTrig = 0, transientPosition = 30, transientStrength = 0.3,
        transientLife = 0.2, transientExponent = 200, reset = 0,
        seed = 1, modelBlockSize = 512, diagnostics = 0,
        aspirationNoise = 0, fricationNoise = 0, externalNoise = 0,
        noiseModulator = -1, diameters, noseDiameters,
        constrictions = #[], turbulencePoints = #[];
        var args, count;
        if(seed.isNumber.not or: { modelBlockSize.isNumber.not } or: { diagnostics.isNumber.not }) {
            Error("PinkTrombone: seed, modelBlockSize and diagnostics must be initial numeric constants.").throw;
        };
        if(modelBlockSize < 1 or: { modelBlockSize > 512 } or: { modelBlockSize != modelBlockSize.asInteger }) {
            Error("PinkTrombone: modelBlockSize must be an integer from 1 to 512.").throw;
        };
        if(seed < 0 or: { seed > 16777215 } or: { seed != seed.asInteger }) {
            Error("PinkTrombone: seed must be an integer from 0 to 16777215.").throw;
        };
        if([0, 1].includes(diagnostics).not) {
            Error("PinkTrombone: diagnostics must be 0 or 1.").throw;
        };
        diameters = diameters ? Array.fill(44, -1);
        noseDiameters = noseDiameters ? Array.fill(28, -1);
        this.validateCells(diameters, 44, "diameters");
        this.validateCells(noseDiameters, 28, "noseDiameters");
        this.validatePoints(constrictions, "constrictions");
        this.validatePoints(turbulencePoints, "turbulencePoints");
        args = [\audio, freq, tenseness, tongueIndex, tongueDiameter, velum,
            gate, alwaysVoice, autoWobble, vibratoAmount, vibratoFrequency,
            aspiration, frication, excitation, glottisGain, glottalReflection,
            lipReflection, movementSpeed, transientTrig, transientPosition,
            transientStrength, transientLife, transientExponent, reset,
            seed, modelBlockSize, diagnostics, constrictions.size, turbulencePoints.size,
            aspirationNoise, fricationNoise, externalNoise, noiseModulator]
            ++ diameters ++ noseDiameters ++ constrictions.flat ++ turbulencePoints.flat;
        ^this.multiNewList(args)
    }

    *validateCells { arg values, size, name;
        if(values.isSequenceableCollection.not or: { values.size != size }) {
            Error("PinkTrombone: % must contain exactly % values.".format(name, size)).throw;
        };
        values.do { |value|
            if(value.isSequenceableCollection) {
                Error("PinkTrombone: % must be a flat array of signals or numbers.".format(name)).throw;
            };
        };
    }

    *validatePoints { arg points, name;
        if(points.isSequenceableCollection.not or: { points.size > 64 }) {
            Error("PinkTrombone: % must be an array of at most 64 triples.".format(name)).throw;
        };
        points.do { |point|
            this.validateCells(point, 3, name ++ " point [index, diameter, gate]");
        };
    }

    init { arg ... theInputs;
        inputs = theInputs;
        ^this.initOutputs(if(inputs[25] > 0, 148, 4), rate)
    }

    checkInputs {
        inputs.do { |input|
            if(input.rate == \demand) { ^"PinkTrombone accepts scalar, control and audio inputs only." };
        };
        ^nil
    }
}
