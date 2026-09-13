#include "PinkTromboneDSP.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

static pink::Parameters decode(const std::vector<double>& v, int nc, int nt) {
    pink::Parameters p;
    p.frequency=v[0]; p.tenseness=v[1]; p.tongueIndex=v[2]; p.tongueDiameter=v[3]; p.velum=v[4];
    p.gate=v[5]>0; p.alwaysVoice=v[6]>0; p.autoWobble=v[7]>0; p.vibratoAmount=v[8]; p.vibratoFrequency=v[9];
    p.aspiration=v[10]; p.frication=v[11]; p.excitation=v[12]; p.glottisGain=v[13];
    p.glottalReflection=v[14]; p.lipReflection=v[15]; p.movementSpeed=v[16];
    p.transientTrigger=v[17]; p.transientPosition=v[18]; p.transientStrength=v[19];
    p.transientLife=v[20]; p.transientExponent=v[21]; p.reset=v[22];
    p.aspirationNoise=v[28]; p.fricationNoise=v[29]; p.externalNoise=static_cast<int>(v[30]); p.noiseModulator=v[31];
    for(int i=0;i<44;++i) p.diameters[i]=v[32+i];
    for(int i=0;i<28;++i) p.noseDiameters[i]=v[76+i];
    p.constrictionCount=nc; p.turbulenceCount=nt;
    for(int i=0;i<nc+nt;++i) {
        auto& c=i<nc?p.constrictions[i]:p.turbulence[i-nc];
        c={v[104+3*i],v[105+3*i],v[106+3*i]};
    }
    return p;
}

int main(int argc, char** argv) {
    if(argc!=3) { std::cerr<<"usage: reference_render controls.bin output.bin\n"; return 2; }
    std::ifstream in(argv[1],std::ios::binary);
    std::ofstream out(argv[2],std::ios::binary);
    std::array<double,8> h {};
    in.read(reinterpret_cast<char*>(h.data()),sizeof(h));
    if(!in || !out || h[0]!=20260913 || h[5]<0 || h[5]>64 || h[6]<0 || h[6]>64 || h[3]<1 || h[3]>512) return 3;
    const int block=static_cast<int>(h[3]), frames=static_cast<int>(h[4]);
    const int nc=static_cast<int>(h[5]),nt=static_cast<int>(h[6]);
    auto engine=std::make_unique<pink::Engine>(h[1],static_cast<uint32_t>(h[2]),block);
    std::vector<double> row(104+3*(nc+nt));
    pink::Parameters p;
    for(int i=0;i<frames;++i) {
        if(i%block==0) {
            in.read(reinterpret_cast<char*>(row.data()),static_cast<std::streamsize>(row.size()*sizeof(double)));
            if(!in) return 4;
            p=decode(row,nc,nt);
        }
        const auto x=engine->step(p);
        std::array<double,148> data {};
        data[0]=x.mix; data[1]=x.lip; data[2]=x.nose; data[3]=x.glottis;
        const auto& t=engine->tractState();
        for(int j=0;j<44;++j) { data[4+j]=t.maxAmplitude[j]; data[76+j]=t.diameter[j]; }
        for(int j=0;j<28;++j) { data[48+j]=t.noseMaxAmplitude[j]; data[120+j]=t.noseDiameter[j]; }
        out.write(reinterpret_cast<const char*>(data.data()),sizeof(data));
    }
    return out ? 0 : 5;
}
