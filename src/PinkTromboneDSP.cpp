// Port of chdh/pink-trombone-mod, commit 359c2d3b42b10280404c1650dc601902112b4c90.
#include "PinkTromboneDSP.hpp"
#include <algorithm>
#include <cmath>

namespace pink {
namespace {
constexpr double pi = 3.14159265358979323846264338327950288;
constexpr int noseStart = 17, bladeStart = 10, tipStart = 32, lipStart = 39;
double clamp(double x, double lo, double hi) noexcept { return std::min(hi, std::max(lo, x)); }
double lerp(double a, double b, double x) noexcept { return a * (1 - x) + b * x; }
double move(double x, double target, double up, double down) noexcept {
    return x < target ? std::min(x + up, target) : std::max(x - down, target);
}
constexpr int gradients[12][2] = {
    {1,1}, {-1,1}, {1,-1}, {-1,-1}, {1,0}, {-1,0},
    {1,0}, {-1,0}, {0,1}, {0,-1}, {0,1}, {0,-1}
};
constexpr int permutation[256] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,
    103,30,69,142,8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,
    26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,
    87,174,20,125,136,171,168,68,175,74,165,71,134,139,48,27,166,77,
    146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,
    40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
    18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
    52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,
    59,227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152,2,44,
    154,163,70,221,153,101,155,167,43,172,9,129,22,39,253,19,98,108,
    110,79,113,224,232,178,185,112,104,218,246,97,228,251,34,242,193,
    238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,107,49,192,
    214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,138,
    236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};
}

double finiteClamp(double value, double lo, double hi, double fallback) noexcept {
    return std::isfinite(value) ? clamp(value, lo, hi) : fallback;
}

Parameters::Parameters() noexcept { diameters.fill(-1); noseDiameters.fill(-1); }

SimplexNoise::SimplexNoise(uint32_t seed) noexcept {
    if (seed < 256) seed |= seed << 8;
    for (int i = 0; i < 256; ++i) {
        const int v = permutation[i] ^ ((i & 1) ? (seed & 255) : ((seed >> 8) & 255));
        perm[i] = perm[i + 256] = v;
    }
}

double SimplexNoise::two(double xin, double yin) const noexcept {
    const double f2 = 0.5 * (std::sqrt(3.0) - 1);
    const double g2 = (3 - std::sqrt(3.0)) / 6;
    const double s = (xin + yin) * f2;
    const double fi = std::floor(xin + s), fj = std::floor(yin + s);
    const double t = (fi + fj) * g2;
    const double x0 = xin - fi + t, y0 = yin - fj + t;
    const int i1 = x0 > y0 ? 1 : 0, j1 = x0 > y0 ? 0 : 1;
    const double x1 = x0 - i1 + g2, y1 = y0 - j1 + g2;
    const double x2 = x0 - 1 + 2 * g2, y2 = y0 - 1 + 2 * g2;
    // Floating reduction avoids an integer overflow even after very long runs.
    const int i = static_cast<int>(std::fmod(fi, 256) + 256) & 255;
    const int j = static_cast<int>(std::fmod(fj, 256) + 256) & 255;
    const auto corner = [](int hash, double x, double y) {
        double a = 0.5 - x * x - y * y;
        if (a < 0) return 0.0;
        a *= a;
        const auto& g = gradients[hash % 12];
        return a * a * (g[0] * x + g[1] * y);
    };
    return 70 * (corner(perm[i + perm[j]], x0, y0)
        + corner(perm[i + i1 + perm[j + j1]], x1, y1)
        + corner(perm[i + 1 + perm[j + 1]], x2, y2));
}

double SimplexNoise::one(double x) const noexcept { return two(x * 1.2, -x * 0.7); }

double Random::white() noexcept {
    state = state * 1664525u + 1013904223u;
    return 2 * (static_cast<double>(state) / 4294967296.0) - 1;
}

FilteredNoise::FilteredNoise(double frequency, double sampleRate, Random& random) noexcept {
    for (auto& x : data) x = random.white();
    const double w = 2 * pi * frequency / sampleRate;
    const double alpha = std::sin(w); // Q = 0.5, as upstream.
    b0 = alpha / (1 + alpha); b2 = -alpha / (1 + alpha);
    a1 = -2 * std::cos(w) / (1 + alpha); a2 = (1 - alpha) / (1 + alpha);
}

void FilteredNoise::reset() noexcept { index = 0; x1 = x2 = y1 = y2 = 0; }

double FilteredNoise::step(bool external, double input) noexcept {
    const double x = external ? input : data[index];
    index = (index + 1) & (noiseSize - 1);
    const double y = b0 * x + 0.0 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x; y2 = y1; y1 = y;
    return y;
}

Glottis::Glottis(double sr, Random& random, const SimplexNoise& simplex) noexcept
    : sampleRate(sr), noise(simplex), aspirationNoise(500, sr, random) { reset(); }

void Glottis::reset() noexcept {
    sampleCount = 0; intensity = 0; smoothFrequency = 140; timeInWaveform = 0; loudness = 1;
    newTenseness = oldTenseness = 0.6; newFrequency = oldFrequency = 140;
    aspirationNoise.reset(); setupWaveform(0);
}

void Glottis::adjust(const Parameters& p, int blockSize) noexcept {
    const double deltaTime = blockSize / sampleRate;
    const double delta = deltaTime * sampleRate / 512;
    const double newTime = sampleCount / sampleRate + deltaTime;
    intensity = clamp(intensity + ((p.gate || p.alwaysVoice) ? 0.13 : -0.05) * delta, 0, 1);
    if (intensity == 0) smoothFrequency = p.frequency;
    else if (p.frequency > smoothFrequency)
        smoothFrequency = std::min(smoothFrequency * (1 + 0.1 * delta), p.frequency);
    else if (p.frequency < smoothFrequency)
        smoothFrequency = std::max(smoothFrequency / (1 + 0.1 * delta), p.frequency);
    double vibrato = p.vibratoAmount * std::sin(2 * pi * newTime * p.vibratoFrequency);
    vibrato += 0.02 * noise.one(newTime * 4.07);
    vibrato += 0.04 * noise.one(newTime * 2.15);
    if (p.autoWobble) {
        vibrato += 0.2 * noise.one(newTime * 0.98);
        vibrato += 0.4 * noise.one(newTime * 0.5);
    }
    oldFrequency = newFrequency;
    newFrequency = std::max(10.0, smoothFrequency * (1 + vibrato));
    oldTenseness = newTenseness;
    newTenseness = std::max(0.0, p.tenseness + 0.1 * noise.one(newTime * 0.46)
        + 0.05 * noise.one(newTime * 0.36));
    if (!p.gate && p.alwaysVoice) newTenseness += (3 - p.tenseness) * (1 - intensity);
}

void Glottis::setupWaveform(double lambda) noexcept {
    const double frequency = lerp(oldFrequency, newFrequency, lambda);
    const double tenseness = lerp(oldTenseness, newTenseness, lambda);
    waveformLength = 1 / frequency;
    loudness = std::pow(std::max(0.0, tenseness), 0.25);
    const double rd = clamp(3 * (1 - tenseness), 0.5, 2.7);
    const double ra = -0.01 + 0.048 * rd, rk = 0.224 + 0.118 * rd;
    const double rg = (rk / 4) * (0.5 + 1.2 * rk) / (0.11 * rd - ra * (0.5 + 1.2 * rk));
    const double tp = 1 / (2 * rg);
    te = tp + tp * rk;
    epsilon = 1 / ra;
    shift = std::exp(-epsilon * (1 - te));
    delta = 1 - shift;
    const double rhsIntegral = ((1 / epsilon) * (shift - 1) + (1 - te) * shift) / delta;
    const double totalUpperIntegral = -(rhsIntegral - (te - tp) / 2);
    omega = pi / tp;
    const double s = std::sin(omega * te);
    const double y = -pi * s * totalUpperIntegral / (tp * 2);
    alpha = std::log(y) / (tp / 2 - te);
    e0 = -1 / (s * std::exp(alpha * te));
}

double Glottis::noiseModulator(const Parameters& p) const noexcept {
    const double voiced = 0.1 + 0.2 * std::max(0.0, std::sin(pi * 2 * timeInWaveform / waveformLength));
    return p.tenseness * intensity * voiced + (1 - p.tenseness * intensity) * 0.3;
}

double Glottis::step(const Parameters& p, double lambda) noexcept {
    const double time = sampleCount / sampleRate;
    if (timeInWaveform > waveformLength) {
        timeInWaveform -= waveformLength;
        setupWaveform(lambda);
    }
    const double t = timeInWaveform / waveformLength;
    const double wave = t > te ? (-std::exp(-epsilon * (t - te)) + shift) / delta
        : e0 * std::exp(alpha * t) * std::sin(omega * t);
    const double source = aspirationNoise.step(p.externalNoise & 1, p.aspirationNoise);
    const double aspiration1 = intensity * (1 - std::sqrt(p.tenseness)) * noiseModulator(p) * source;
    const double aspiration2 = aspiration1 * (0.2 + 0.02 * noise.one(time * 1.99));
    const double out = wave * intensity * loudness + p.aspiration * aspiration2;
    ++sampleCount;
    timeInWaveform += 1 / sampleRate;
    return out;
}

Tract::Tract(double sr, Random& random) noexcept : sampleRate(sr), fricationNoise(1000, sr, random) { reset(); }

double Tract::restDiameter(int i, double tongueIndex, double tongueDiameter) noexcept {
    if (i < 7) return 0.6;
    if (i < bladeStart) return 1.1;
    if (i >= lipStart) return 1.5;
    const double t = 1.1 * pi * (tongueIndex - i) / (tipStart - bladeStart);
    const double fixed = 2 + (tongueDiameter - 2) / 1.5;
    double curve = (1.5 - fixed + 1.7) * std::cos(t);
    if (i == bladeStart - 2 || i == lipStart - 1) curve *= 0.8;
    if (i == bladeStart || i == lipStart - 2) curve *= 0.94;
    return 1.5 - curve;
}

void Tract::shapeNose(bool open) noexcept {
    for (int i = 0; i < noseSize; ++i) {
        const double d = 2 * (static_cast<double>(i) / noseSize);
        const double diameter = i == 0 ? (open ? 0.4 : 0.01)
            : d < 1 ? 0.4 + 1.6 * d : 0.5 + 1.5 * (2 - d);
        noseDiameter[i] = std::min(diameter, 1.9);
    }
}

void Tract::noseReflections() noexcept {
    for (int i = 1; i < noseSize; ++i) {
        const double a = std::max(1E-6, noseDiameter[i-1] * noseDiameter[i-1]);
        const double b = std::max(1E-6, noseDiameter[i] * noseDiameter[i]);
        noseReflection[i] = (a - b) / (a + b);
    }
}

void Tract::reset() noexcept {
    sampleCount = 0; time = 0; transientCount = 0; pointCount = 0; lastObstruction = -1;
    droppedTransients = 0; customNose = false;
    right.fill(0); left.fill(0); reflection.fill(0); newReflection.fill(0);
    junctionLeft.fill(0); junctionRight.fill(0); maxAmplitude.fill(0);
    noseRight.fill(0); noseLeft.fill(0); noseReflection.fill(0);
    noseJunctionLeft.fill(0); noseJunctionRight.fill(0); noseMaxAmplitude.fill(0);
    points.fill(Point {});
    reflectionLeft = reflectionRight = reflectionNose = 0;
    newReflectionLeft = newReflectionRight = newReflectionNose = 0;
    fricationNoise.reset();
    shapeNose(true); noseReflections(); shapeNose(false);
    for (int i = 0; i < mouthSize; ++i) diameter[i] = target[i] = restDiameter(i, 12.9, 2.43);
}

void Tract::reduceTarget(double index, double d) noexcept {
    if (index < 2 || index >= mouthSize || d >= 3) return;
    const double width = index < 25 ? 10 : index >= tipStart ? 5 : 10 - 5 * (index - 25) / (tipStart - 25);
    for (int i = -static_cast<int>(std::ceil(width)) - 1; i < width + 1; ++i) {
        const int p = static_cast<int>(std::floor(index + 0.5)) + i;
        if (p < 0 || p >= mouthSize) continue;
        const double rel = std::abs(p - index) - 0.5;
        const double shrink = rel <= 0 ? 0 : rel > width ? 1 : 0.5 * (1 - std::cos(pi * rel / width));
        if (d < target[p]) target[p] = d + (target[p] - d) * shrink;
    }
}

void Tract::adjust(const Parameters& p, double deltaTime) noexcept {
    for (int i = 0; i < mouthSize; ++i)
        target[i] = p.diameters[i] >= 0 ? p.diameters[i] : restDiameter(i, p.tongueIndex, p.tongueDiameter);
    double velumTarget = p.velum;
    for (int i = 0; i < p.constrictionCount; ++i) {
        const auto& c = p.constrictions[i];
        if (c.gate <= 0) continue;
        // TractUi noseOffset is 0.8; negative touch diameters open the velum.
        if (c.index > noseStart && c.diameter < -0.8) velumTarget = 0.4;
        if (c.diameter >= -1.65) reduceTarget(c.index, std::max(0.0, c.diameter - 0.3));
    }
    const double amount = deltaTime * p.movementSpeed;
    int obstruction = -1;
    for (int i = 0; i < mouthSize; ++i) {
        if (diameter[i] <= 0) obstruction = i;
        const double slow = i < noseStart ? 0.6 : i >= tipStart ? 1
            : 0.6 + 0.4 * (i - noseStart) / (tipStart - noseStart);
        diameter[i] = move(diameter[i], target[i], slow * amount, 2 * amount);
    }
    if (lastObstruction > -1 && obstruction == -1 && noseDiameter[0] < 0.223)
        addTransient(lastObstruction, 0.3, 0.2, 200);
    lastObstruction = obstruction;
    // Restore defaults for cells whose override is removed. Preserve upstream's
    // fixed open-velum nasal reflections unless the user supplies a custom nose.
    const double oldVelum = noseDiameter[0];
    bool hasCustomNose = false;
    shapeNose(true);
    for (int i = 0; i < noseSize; ++i) {
        if (p.noseDiameters[i] >= 0) { noseDiameter[i] = p.noseDiameters[i]; hasCustomNose = true; }
    }
    if (hasCustomNose || customNose) noseReflections();
    customNose = hasCustomNose;
    if (p.noseDiameters[0] >= 0) velumTarget = p.noseDiameters[0];
    noseDiameter[0] = move(oldVelum, velumTarget, amount * 0.25, amount * 0.1);
}

void Tract::reflections() noexcept {
    for (int i = 1; i < mouthSize; ++i) {
        const double a = diameter[i-1] * diameter[i-1], b = diameter[i] * diameter[i];
        reflection[i] = newReflection[i];
        newReflection[i] = std::abs(a + b) > 1E-6 ? (a - b) / (a + b) : 1;
    }
    reflectionLeft = newReflectionLeft; reflectionRight = newReflectionRight; reflectionNose = newReflectionNose;
    const double velum = noseDiameter[0] * noseDiameter[0];
    const double an0 = diameter[noseStart] * diameter[noseStart];
    const double an1 = diameter[noseStart + 1] * diameter[noseStart + 1];
    const double sum = an0 + an1 + velum;
    newReflectionLeft = std::abs(sum) > 1E-6 ? (2 * an0 - sum) / sum : 1;
    newReflectionRight = std::abs(sum) > 1E-6 ? (2 * an1 - sum) / sum : 1;
    newReflectionNose = std::abs(sum) > 1E-6 ? (2 * velum - sum) / sum : 1;
}

void Tract::updatePoints(const Parameters& p) noexcept {
    pointCount = p.constrictionCount + p.turbulenceCount;
    for (int i = 0; i < pointCount; ++i) {
        const auto& c = i < p.constrictionCount ? p.constrictions[i] : p.turbulence[i - p.constrictionCount];
        auto& point = points[i];
        const bool alive = c.gate > 0;
        if (alive && !point.alive) { point.start = time; point.end = -1; point.active = true; }
        if (!alive && point.alive) point.end = time;
        // A released touch keeps its last coordinates throughout the noise tail.
        if (alive) { point.index = c.index; point.diameter = c.diameter; }
        point.alive = alive;
        if (!alive && point.end >= 0 && time - point.end >= 0.1) point.active = false;
    }
}

void Tract::addTransient(int position, double strength, double life, double exponent) noexcept {
    if (transientCount == maxTransients) {
        // Bounded real-time storage. On overload, retire the oldest event.
        for (int i = 1; i < transientCount; ++i) transients[i - 1] = transients[i];
        --transientCount; ++droppedTransients;
    }
    transients[transientCount++] = {time, life, strength, exponent, std::max(0, std::min(43, position))};
}

void Tract::processTransients() noexcept {
    for (int i = transientCount - 1; i >= 0; --i) {
        const auto& trans = transients[i];
        const double age = time - trans.start;
        if (age > trans.life) {
            for (int j = i + 1; j < transientCount; ++j) transients[j - 1] = transients[j];
            --transientCount; continue;
        }
        const double amplitude = trans.strength * std::pow(2.0, -trans.exponent * age);
        right[trans.position] += amplitude / 2;
        left[trans.position] += amplitude / 2;
    }
}

void Tract::turbulence(const Parameters& p, double modulator) noexcept {
    for (int j = 0; j < pointCount; ++j) {
        const auto& point = points[j];
        if (!point.active || point.index < 2 || point.index > mouthSize || point.diameter <= 0) continue;
        const double intensity = point.end < 0 ? clamp((time - point.start) / 0.1, 0, 1)
            : clamp(1 - (time - point.end) / 0.1, 0, 1);
        if (intensity <= 0) continue;
        const double noise = 0.66 * fricationNoise.step(p.externalNoise & 2, p.fricationNoise) * intensity * modulator;
        const int i = static_cast<int>(std::floor(point.index));
        const double delta = point.index - i;
        const double thinness = clamp(8 * (0.7 - point.diameter), 0, 1);
        const double openness = clamp(30 * (point.diameter - 0.3), 0, 1);
        const double noise0 = p.frication * noise * (1 - delta) * thinness * openness;
        const double noise1 = p.frication * noise * delta * thinness * openness;
        if (i + 1 < mouthSize) { right[i+1] += noise0 / 2; left[i+1] += noise0 / 2; }
        if (i + 2 < mouthSize) { right[i+2] += noise1 / 2; left[i+2] += noise1 / 2; }
    }
}

Output Tract::step(const Parameters& p, double glottal, double modulator, double lambda) noexcept {
    processTransients(); turbulence(p, modulator);
    junctionRight[0] = left[0] * p.glottalReflection + glottal;
    junctionLeft[mouthSize] = right[mouthSize - 1] * p.lipReflection;
    for (int i = 1; i < mouthSize; ++i) {
        const double r = lerp(reflection[i], newReflection[i], lambda);
        const double w = r * (right[i-1] + left[i]);
        junctionRight[i] = right[i-1] - w; junctionLeft[i] = left[i] + w;
    }
    const int i = noseStart;
    double r = lerp(reflectionLeft, newReflectionLeft, lambda);
    junctionLeft[i] = r * right[i-1] + (1 + r) * (noseLeft[0] + left[i]);
    r = lerp(reflectionRight, newReflectionRight, lambda);
    junctionRight[i] = r * left[i] + (1 + r) * (right[i-1] + noseLeft[0]);
    r = lerp(reflectionNose, newReflectionNose, lambda);
    noseJunctionRight[0] = r * noseLeft[0] + (1 + r) * (left[i] + right[i-1]);
    for (int j = 0; j < mouthSize; ++j) {
        right[j] = junctionRight[j] * 0.999; left[j] = junctionLeft[j+1] * 0.999;
        maxAmplitude[j] = std::max(maxAmplitude[j] * 0.9999, std::abs(right[j] + left[j]));
    }
    const double lip = right[mouthSize - 1];
    noseJunctionLeft[noseSize] = noseRight[noseSize - 1] * p.lipReflection;
    for (int j = 1; j < noseSize; ++j) {
        const double w = noseReflection[j] * (noseRight[j-1] + noseLeft[j]);
        noseJunctionRight[j] = noseRight[j-1] - w; noseJunctionLeft[j] = noseLeft[j] + w;
    }
    for (int j = 0; j < noseSize; ++j) {
        noseRight[j] = noseJunctionRight[j]; noseLeft[j] = noseJunctionLeft[j+1];
        noseMaxAmplitude[j] = std::max(noseMaxAmplitude[j] * 0.9999, std::abs(noseRight[j] + noseLeft[j]));
    }
    const double nose = noseRight[noseSize - 1];
    ++sampleCount; time = sampleCount / sampleRate;
    return {lip + nose, lip, nose, glottal};
}

Engine::Engine(double sr, uint32_t seed, int modelBlockSize) noexcept
    : sampleRate(finiteClamp(sr, 8000, 768000, 48000)), blockSize(std::max(1, std::min(512, modelBlockSize))),
      noise(seed), random(seed), glottis(sampleRate, random, noise), tract(2 * sampleRate, random) {}

void Engine::reset() noexcept {
    phase = 0; initialized = false; previousTrigger = 0;
    glottis.reset(); tract.reset();
}

Output Engine::step(const Parameters& p) noexcept {
    if (p.reset > 0 && previousReset <= 0) reset();
    previousReset = p.reset;
    tract.updatePoints(p);
    if (!initialized) {
        glottis.adjust(p, 0); tract.adjust(p, 0); tract.reflections(); initialized = true;
    }
    if (phase == 0) {
        glottis.adjust(p, blockSize); tract.adjust(p, blockSize / sampleRate); tract.reflections();
    }
    if (p.transientTrigger > 0 && previousTrigger <= 0)
        tract.addTransient(static_cast<int>(p.transientPosition), p.transientStrength, p.transientLife, p.transientExponent);
    previousTrigger = p.transientTrigger;
    const double lambda1 = static_cast<double>(phase) / blockSize;
    const double lambda2 = (phase + 0.5) / blockSize;
    const double glottal = glottis.step(p, lambda1);
    const double modulator = p.noiseModulator >= 0 ? p.noiseModulator : glottis.noiseModulator(p);
    const double excitation = glottal * p.glottisGain + p.excitation;
    const auto a = tract.step(p, excitation, modulator, lambda1);
    const auto b = tract.step(p, excitation, modulator, lambda2);
    if (!std::isfinite(a.mix) || !std::isfinite(b.mix) || !std::isfinite(glottal)
        || std::abs(a.lip) > 1E12 || std::abs(a.nose) > 1E12
        || std::abs(b.lip) > 1E12 || std::abs(b.nose) > 1E12) {
        // Upstream can become unstable with degenerate/custom closed junctions.
        // Normal synthesis is unchanged; pathological states restart silently.
        reset();
        return {0, 0, 0, 0};
    }
    phase = (phase + 1) % blockSize;
    // Same two-substep sum and gain as Synthesizer.synthesizeBlock.
    return {(a.mix + b.mix) * 0.125, (a.lip + b.lip) * 0.125, (a.nose + b.nose) * 0.125, glottal};
}
} // namespace pink
