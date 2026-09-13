// SuperCollider adapter; audio-thread state is allocated only with RTAlloc.
#include "SC_PlugIn.h"
#include "PinkTromboneDSP.hpp"
#include <cmath>
#include <new>

static InterfaceTable* ft;

struct PinkTrombone : Unit {
    pink::Engine* engine;
    pink::Parameters* params;
    int constrictionCount, turbulenceCount;
};

static double read(PinkTrombone* unit, int input, int sample, double lo, double hi, double fallback) {
    return pink::finiteClamp(IN(input)[INRATE(input) == calc_FullRate ? sample : 0], lo, hi, fallback);
}

static void readParameters(PinkTrombone* unit, int sample) {
    using namespace pink;
    auto& p = *unit->params;
    p.frequency = read(unit, Frequency, sample, 10, SAMPLERATE * 0.2, 140);
    p.tenseness = read(unit, Tenseness, sample, 0, 1, 0.6);
    p.gate = read(unit, Gate, sample, 0, 1, 0) > 0;
    p.alwaysVoice = read(unit, AlwaysVoice, sample, 0, 1, 1) > 0;
    p.autoWobble = read(unit, AutoWobble, sample, 0, 1, 1) > 0;
    p.vibratoAmount = read(unit, VibratoAmount, sample, 0, 1, 0.005);
    p.vibratoFrequency = read(unit, VibratoFrequency, sample, 0, 100, 6);
    p.aspiration = read(unit, Aspiration, sample, 0, 10, 1);
    p.frication = read(unit, Frication, sample, 0, 10, 1);
    p.excitation = read(unit, Excitation, sample, -100, 100, 0);
    p.glottisGain = read(unit, GlottisGain, sample, 0, 10, 1);
    p.glottalReflection = read(unit, GlottalReflection, sample, -0.999, 0.999, 0.75);
    p.lipReflection = read(unit, LipReflection, sample, -0.999, 0.999, -0.85);
    p.transientTrigger = read(unit, TransientTrigger, sample, -1, 1, 0);
    p.transientPosition = read(unit, TransientPosition, sample, 0, 43, 30);
    p.transientStrength = read(unit, TransientStrength, sample, 0, 10, 0.3);
    p.transientLife = read(unit, TransientLife, sample, 0, 10, 0.2);
    p.transientExponent = read(unit, TransientExponent, sample, 0, 10000, 200);
    p.reset = read(unit, Reset, sample, -1, 1, 0);
    p.aspirationNoise = read(unit, AspirationNoise, sample, -100, 100, 0);
    p.fricationNoise = read(unit, FricationNoise, sample, -100, 100, 0);
    p.externalNoise = static_cast<int>(read(unit, ExternalNoise, sample, 0, 3, 0));
    p.noiseModulator = read(unit, NoiseModulator, sample, -1, 1, -1);
    // Shape targets are consumed only at model boundaries, including a reset.
    if (unit->engine->atBoundary() || p.reset > 0) {
        p.tongueIndex = read(unit, TongueIndex, sample, 0, 43, 12.9);
        p.tongueDiameter = read(unit, TongueDiameter, sample, 2.05, 3.5, 2.43);
        p.velum = read(unit, Velum, sample, 0, 3, 0.01);
        p.movementSpeed = read(unit, MovementSpeed, sample, 0, 1000, 15);
        for (int i = 0; i < mouthSize; ++i) p.diameters[i] = read(unit, Diameters + i, sample, -1, 10, -1);
        for (int i = 0; i < noseSize; ++i) p.noseDiameters[i] = read(unit, NoseDiameters + i, sample, -1, 10, -1);
    }
    for (int j = 0; j < unit->constrictionCount + unit->turbulenceCount; ++j) {
        auto& point = j < unit->constrictionCount ? p.constrictions[j] : p.turbulence[j - unit->constrictionCount];
        point.index = read(unit, Points + 3 * j, sample, -1, 44, 30);
        point.diameter = read(unit, Points + 3 * j + 1, sample, -3, 10, 0.5);
        point.gate = read(unit, Points + 3 * j + 2, sample, 0, 1, 0);
    }
}

static void PinkTrombone_next(PinkTrombone* unit, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        readParameters(unit, i);
        const auto out = unit->engine->step(*unit->params);
        if (!std::isfinite(out.mix) || !std::isfinite(out.glottis) || std::abs(out.mix) > 1E12) {
            // Recover from numerical overload without poisoning downstream UGens.
            unit->engine->reset();
            for (uint32 j = 0; j < unit->mNumOutputs; ++j) OUT(j)[i] = 0;
            continue;
        }
        OUT(0)[i] = static_cast<float>(out.mix);
        OUT(1)[i] = static_cast<float>(out.lip);
        OUT(2)[i] = static_cast<float>(out.nose);
        OUT(3)[i] = static_cast<float>(out.glottis);
        if (unit->mNumOutputs == pink::diagnosticOutputs) {
            const auto& tract = unit->engine->tractState();
            for (int j = 0; j < pink::mouthSize; ++j) {
                OUT(4 + j)[i] = static_cast<float>(tract.maxAmplitude[j]);
                OUT(76 + j)[i] = static_cast<float>(tract.diameter[j]);
            }
            for (int j = 0; j < pink::noseSize; ++j) {
                OUT(48 + j)[i] = static_cast<float>(tract.noseMaxAmplitude[j]);
                OUT(120 + j)[i] = static_cast<float>(tract.noseDiameter[j]);
            }
        }
    }
}

static void PinkTrombone_Ctor(PinkTrombone* unit) {
    unit->engine = nullptr; unit->params = nullptr;
    SETCALC(ClearUnitOutputs);
    if (unit->mNumInputs < pink::baseInputs || (unit->mNumOutputs != 4 && unit->mNumOutputs != pink::diagnosticOutputs)) {
        Print("PinkTrombone: invalid input/output layout; recompile the class library.\n");
        ClearUnitOutputs(unit, 1); return;
    }
    unit->constrictionCount = static_cast<int>(read(unit, pink::ConstrictionCount, 0, 0, pink::maxPoints, 0));
    unit->turbulenceCount = static_cast<int>(read(unit, pink::TurbulenceCount, 0, 0, pink::maxPoints, 0));
    if (unit->mNumInputs != static_cast<uint32>(pink::baseInputs + 3 * (unit->constrictionCount + unit->turbulenceCount))) {
        Print("PinkTrombone: invalid point array layout.\n"); ClearUnitOutputs(unit, 1); return;
    }
    void* parameterMemory = RTAlloc(unit->mWorld, sizeof(pink::Parameters));
    ClearUnitIfMemFailed(parameterMemory);
    unit->params = new (parameterMemory) pink::Parameters();
    unit->params->constrictionCount = unit->constrictionCount;
    unit->params->turbulenceCount = unit->turbulenceCount;
    void* engineMemory = RTAlloc(unit->mWorld, sizeof(pink::Engine));
    ClearUnitIfMemFailed(engineMemory);
    const auto seed = static_cast<uint32_t>(read(unit, pink::Seed, 0, 0, 16777215, 1));
    const int block = static_cast<int>(read(unit, pink::ModelBlockSize, 0, 1, 512, 512));
    unit->engine = new (engineMemory) pink::Engine(SAMPLERATE, seed, block);
    SETCALC(PinkTrombone_next);
    // Do not advance the model in the constructor: preserve the first real sample.
    ClearUnitOutputs(unit, 1);
}

static void PinkTrombone_Dtor(PinkTrombone* unit) {
    if (unit->engine) { unit->engine->~Engine(); RTFree(unit->mWorld, unit->engine); }
    if (unit->params) { unit->params->~Parameters(); RTFree(unit->mWorld, unit->params); }
}

PluginLoad(PinkTrombone) {
    ft = inTable;
    DefineDtorCantAliasUnit(PinkTrombone);
}
