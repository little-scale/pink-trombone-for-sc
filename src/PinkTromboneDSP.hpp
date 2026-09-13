// Pink Trombone DSP port. Original: Copyright 2017 Neil Thapen (MIT).
// Modular TypeScript version: Christian d'Heureuse. See THIRD_PARTY.md.
#pragma once

#include <array>
#include <cstdint>

namespace pink {
constexpr int mouthSize = 44;
constexpr int noseSize = 28;
constexpr int maxPoints = 64;
constexpr int maxTransients = 256;
constexpr int noiseSize = 32768;
constexpr int baseInputs = 104;
constexpr int diagnosticOutputs = 148;

enum Input {
    Frequency, Tenseness, TongueIndex, TongueDiameter, Velum,
    Gate, AlwaysVoice, AutoWobble, VibratoAmount, VibratoFrequency,
    Aspiration, Frication, Excitation, GlottisGain, GlottalReflection,
    LipReflection, MovementSpeed, TransientTrigger, TransientPosition,
    TransientStrength, TransientLife, TransientExponent, Reset, Seed,
    ModelBlockSize, Diagnostics, ConstrictionCount, TurbulenceCount,
    AspirationNoise, FricationNoise, ExternalNoise, NoiseModulator,
    Diameters = 32, NoseDiameters = 76, Points = 104
};

struct PointControl {
    double index = 30, diameter = 0.5, gate = 0;
};

struct Parameters {
    double frequency = 140, tenseness = 0.6;
    double tongueIndex = 12.9, tongueDiameter = 2.43, velum = 0.01;
    bool gate = false, alwaysVoice = true, autoWobble = true;
    double vibratoAmount = 0.005, vibratoFrequency = 6;
    double aspiration = 1, frication = 1, excitation = 0, glottisGain = 1;
    double glottalReflection = 0.75, lipReflection = -0.85, movementSpeed = 15;
    double transientTrigger = 0, transientPosition = 30, transientStrength = 0.3;
    double transientLife = 0.2, transientExponent = 200, reset = 0;
    double aspirationNoise = 0, fricationNoise = 0, noiseModulator = -1;
    int externalNoise = 0, constrictionCount = 0, turbulenceCount = 0;
    std::array<double, mouthSize> diameters;
    std::array<double, noseSize> noseDiameters;
    std::array<PointControl, maxPoints> constrictions {}, turbulence {};
    Parameters() noexcept;
};

struct Output { double mix, lip, nose, glottis; };

class SimplexNoise {
public:
    explicit SimplexNoise(uint32_t seed = 1) noexcept;
    double one(double x) const noexcept;
    double two(double x, double y) const noexcept;
private:
    std::array<int, 512> perm {};
};

// A repeatable, independent replacement for JavaScript's unspecified Math.random.
class Random {
public:
    explicit Random(uint32_t seed) noexcept : state(seed) {}
    double white() noexcept;
private:
    uint32_t state;
};

class FilteredNoise {
public:
    FilteredNoise(double frequency, double sampleRate, Random& random) noexcept;
    double step(bool external, double input) noexcept;
    void reset() noexcept;
private:
    std::array<double, noiseSize> data {};
    int index = 0;
    double b0, b2, a1, a2, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
};

class Glottis {
public:
    Glottis(double sampleRate, Random& random, const SimplexNoise& noise) noexcept;
    void reset() noexcept;
    void adjust(const Parameters& p, int blockSize) noexcept;
    double step(const Parameters& p, double lambda) noexcept;
    double noiseModulator(const Parameters& p) const noexcept;
    double intensity = 0;
private:
    double sampleRate;
    const SimplexNoise& noise;
    FilteredNoise aspirationNoise;
    uint64_t sampleCount = 0;
    double smoothFrequency = 140, timeInWaveform = 0, loudness = 1;
    double newTenseness = 0.6, oldTenseness = 0.6, newFrequency = 140, oldFrequency = 140;
    double waveformLength = 0, alpha = 0, e0 = 0, epsilon = 0, shift = 0, delta = 0, te = 0, omega = 0;
    void setupWaveform(double lambda) noexcept;
};

class Tract {
public:
    Tract(double sampleRate, Random& random) noexcept;
    void reset() noexcept;
    void adjust(const Parameters& p, double deltaTime) noexcept;
    void updatePoints(const Parameters& p) noexcept;
    void addTransient(int position, double strength, double life, double exponent) noexcept;
    void reflections() noexcept;
    Output step(const Parameters& p, double glottal, double modulator, double lambda) noexcept;
    std::array<double, mouthSize> diameter {}, target {}, maxAmplitude {};
    std::array<double, noseSize> noseDiameter {}, noseMaxAmplitude {};
    uint64_t droppedTransients = 0;
private:
    struct Point {
        double index = 30, diameter = 0.5, start = 0, end = -1;
        bool active = false, alive = false;
    };
    struct Transient {
        double start = 0, life = 0, strength = 0, exponent = 0;
        int position = 0;
    };
    double sampleRate, time = 0;
    uint64_t sampleCount = 0;
    FilteredNoise fricationNoise;
    std::array<double, mouthSize> right {}, left {}, reflection {}, newReflection {}, junctionRight {};
    std::array<double, mouthSize + 1> junctionLeft {};
    std::array<double, noseSize> noseRight {}, noseLeft {}, noseReflection {}, noseJunctionRight {};
    std::array<double, noseSize + 1> noseJunctionLeft {};
    std::array<Point, maxPoints * 2> points {};
    std::array<Transient, maxTransients> transients {};
    int pointCount = 0, transientCount = 0, lastObstruction = -1;
    double reflectionLeft = 0, reflectionRight = 0, reflectionNose = 0;
    double newReflectionLeft = 0, newReflectionRight = 0, newReflectionNose = 0;
    bool customNose = false;
    static double restDiameter(int i, double tongueIndex, double tongueDiameter) noexcept;
    void shapeNose(bool open) noexcept;
    void noseReflections() noexcept;
    void reduceTarget(double index, double diameter) noexcept;
    void processTransients() noexcept;
    void turbulence(const Parameters& p, double modulator) noexcept;
};

class Engine {
public:
    Engine(double sampleRate, uint32_t seed, int modelBlockSize) noexcept;
    Output step(const Parameters& p) noexcept;
    void reset() noexcept;
    const Tract& tractState() const noexcept { return tract; }
    const Glottis& glottisState() const noexcept { return glottis; }
    bool atBoundary() const noexcept { return phase == 0; }
private:
    double sampleRate;
    int blockSize, phase = 0;
    bool initialized = false;
    double previousReset = 0, previousTrigger = 0;
    SimplexNoise noise;
    Random random;
    Glottis glottis;
    Tract tract;
};

double finiteClamp(double value, double lo, double hi, double fallback) noexcept;
} // namespace pink
