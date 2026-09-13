#include "PinkTromboneDSP.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>

static bool watchAllocations = false;
static size_t audioAllocations = 0;
void* operator new(std::size_t n) {
    if (watchAllocations) ++audioAllocations;
    if (void* p = std::malloc(n)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { std::free(p); }

static void require(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

int main() {
    using namespace pink;
    require(finiteClamp(NAN, 0, 1, 0.6) == 0.6, "NaN input fallback");
    require(finiteClamp(INFINITY, 0, 1, 0.6) == 0.6, "infinite input fallback");
    for (const double sr : {44100.0, 48000.0, 96000.0}) {
        for (const int block : {1, 17, 64, 512}) {
            Parameters p;
            auto a = std::make_unique<Engine>(sr, 42, block);
            auto b = std::make_unique<Engine>(sr, 42, block);
            double energy = 0;
            watchAllocations = true;
            for (int i = 0; i < 24000; ++i) {
                const auto x = a->step(p), y = b->step(p);
                require(std::isfinite(x.mix) && std::abs(x.mix) < 100, "finite default audio");
                require(x.mix == y.mix && x.glottis == y.glottis, "same seed gives identical instances");
                require(std::abs(x.mix - x.lip - x.nose) < 1E-12, "mix equals lip plus nose");
                energy += x.mix * x.mix;
            }
            require(energy > 0.1, "default voice is audible");
            a->reset(); b->reset();
            for (int i = 0; i < 2048; ++i) require(a->step(p).mix == b->step(p).mix, "repeatable reset");
            p.alwaysVoice = false; p.gate = false;
            for (int i = 0; i < 30000; ++i) a->step(p);
            require(a->glottisState().intensity == 0, "voice gate releases to zero intensity");
            watchAllocations = false;
        }
    }
    {
        Parameters p;
        p.constrictionCount = 64; p.turbulenceCount = 64;
        auto e = std::make_unique<Engine>(48000, 9, 1);
        Random random(88);
        bool sawDroppedTransients = false;
        watchAllocations = true;
        for (int i = 0; i < 100000; ++i) {
            if (i % 113 == 0) {
                p.frequency = 10 + 9500 * (random.white() + 1) / 2;
                p.tenseness = (random.white() + 1) / 2;
                p.tongueIndex = 43 * (random.white() + 1) / 2;
                p.tongueDiameter = 2.05 + 1.45 * (random.white() + 1) / 2;
                p.velum = 3 * (random.white() + 1) / 2;
                for (auto& d : p.diameters) d = random.white() > 0 ? 0 : 2;
                for (auto& c : p.constrictions) c = {22 * (random.white()+1), random.white(), random.white() > 0 ? 1.0 : 0.0};
                for (auto& c : p.turbulence) c = {22 * (random.white()+1), 0.5, random.white() > 0 ? 1.0 : 0.0};
            }
            p.transientTrigger = i % 2;
            const auto x = e->step(p);
            sawDroppedTransients |= e->tractState().droppedTransients > 0;
            require(std::isfinite(x.mix) && std::isfinite(x.glottis), "stress output remains finite");
        }
        watchAllocations = false;
        require(sawDroppedTransients, "transient overload exercises bounded pool");
    }
    require(audioAllocations == 0, "no heap allocations during processing or reset");
    std::cout << "PASS: determinism, reset, gates, output sums, 3 sample rates, 4 update sizes, "
        "128 simultaneous points, 100000-sample stress and allocation audit.\n";
}
