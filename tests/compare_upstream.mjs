// Executes the actual pinned TypeScript DSP, with Math.random made reproducible.
// Node >= 22.13, no npm dependencies. Type erasure does not change DSP equations.
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { stripTypeScriptTypes } from 'node:module';
import { execFileSync } from 'node:child_process';

const root = path.resolve(import.meta.dirname, '..');
const build = path.resolve(process.argv[2] || path.join(root, 'build'));
const scratch = path.join(build, 'reference');
fs.mkdirSync(scratch, { recursive: true });
const upstream = path.join(root, 'vendor/pink-trombone-mod/src');
for (const name of ['Synthesizer.ts','Glottis.ts','Tract.ts','TractShaper.ts','TractUi.ts','GuiUtils.ts','Utils.ts','NoiseGenerator.ts']) {
    // Node 26 strips types but no longer transforms parameter properties. Expand
    // this one constructor syntactically; its arithmetic is left untouched.
    const source = fs.readFileSync(path.join(upstream, name), 'utf8').replace(
        'constructor(public x: number, public y: number, public z: number) {}',
        'constructor(x: number, y: number, z: number) { this.x=x; this.y=y; this.z=z; }')
        .replace(/, (Transient|TurbulencePoint)(?=\})/g, '')
        .replace(/import \{AppTouch\} from "\.\/GuiUtils";/g, '');
    const text = stripTypeScriptTypes(source, { mode: 'strip' })
        .replace(/from "\.\/(\w+)"/g, 'from "./$1.mjs"');
    fs.writeFileSync(path.join(scratch, name.replace('.ts', '.mjs')), text);
}
const { Synthesizer } = await import(pathToFileURL(path.join(scratch, 'Synthesizer.mjs')));
const { TractUi } = await import(pathToFileURL(path.join(scratch, 'TractUi.mjs')));
const noise = await import(pathToFileURL(path.join(scratch, 'NoiseGenerator.mjs')));

export function defaults(nc = 0, nt = 0) {
    return [140, .6, 12.9, 2.43, .01, 0, 1, 1, .005, 6, 1, 1, 0, 1, .75, -.85,
        15, 0, 30, .3, .2, 200, 0, 1, 512, 0, nc, nt, 0, 0, 0, -1,
        ...Array(72).fill(-1), ...Array.from({length:nc+nt}, () => [30,.5,0]).flat()];
}

const cases = [
    {name:'default_44100', sr:44100, block:512, duration:0.9},
    {name:'default_48000', sr:48000, block:512, duration:0.9},
    {name:'default_96000', sr:96000, block:512, duration:0.5},
    {name:'articulation_64', sr:48000, block:64, duration:0.9, moving:true},
    {name:'articulation_1', sr:44100, block:1, duration:0.15, moving:true},
    {name:'closures_fricatives', sr:48000, block:512, duration:1.4, nc:2, nt:2, points:true},
    {name:'individual_diameters', sr:48000, block:128, duration:0.8, cells:true},
    {name:'voice_gate', sr:48000, block:256, duration:0.9, gate:true},
];
const report = [];

for (const test of cases) {
    const {sr,block} = test, nc = test.nc || 0, nt = test.nt || 0, seed = 12039;
    const frames = Math.ceil(sr * test.duration / block) * block;
    const controls = [];
    for (let n = 0; n < frames; n += block) {
        const t = n / sr, v = defaults(nc,nt);
        v[23] = seed; v[24] = block;
        if (test.moving) {
            v[0] = 90 + 200 * (0.5 + 0.5 * Math.sin(t * 9));
            v[1] = .15 + .8 * (0.5 + 0.5 * Math.sin(t * 11));
            v[2] = 20 + 8 * Math.sin(t * 8); v[3] = 2.7 + .6 * Math.sin(t * 7);
            v[4] = .2 + .19 * Math.sin(t * 6); v[5] = 1; v[7] = 0;
        }
        if (test.gate) { v[6] = 0; v[5] = (t > .06 && t < .35) || t > .7 ? 1 : 0; }
        if (test.cells) {
            for (let i = 0; i < 44; ++i) v[32+i] = .8 + .4 * Math.cos(i*.4 + t*10);
            if (t > .15 && t < .3) v[32+30] = 0;
            if (t > .45) v[32+30] = -1;
        }
        if (test.points) {
            v[104]=30; v[105]=t < .45 ? .1 : .5; v[106]=t > .12 && t < .9 ? 1:0;
            v[107]=22; v[108]=t > .8 ? -1 : .55; v[109]=t > .65 && t < 1.1 ? 1:0;
            v[110]=35.25; v[111]=.49; v[112]=t > .4 && t < .8 ? 1:0;
            v[113]=12.9; v[114]=.59; v[115]=t > .5 && t < 1 ? 1:0;
        }
        controls.push(v.map(Math.fround)); // Match SC's input wire precision.
    }
    let randomState = seed;
    const originalRandom = Math.random;
    Math.random = () => {randomState=(Math.imul(randomState,1664525)+1013904223)>>>0; return randomState/4294967296;};
    noise.setSeed(seed);
    const synth = new Synthesizer(sr);
    Math.random = originalRandom;
    const tract = synth.tract, glottis = synth.glottis, shaper = synth.tractShaper;
    const ui = new TractUi(tract, shaper);
    const points = Array.from({length:nc+nt},()=>({alive:false, active:false,position:30,diameter:.5,startTime:0,endTime:NaN}));
    function apply(v) {
        glottis.targetFrequency=v[0]; glottis.targetTenseness=v[1];
        shaper.tongueIndex=v[2]; shaper.tongueDiameter=v[3]; shaper.velumTarget=v[4];
        glottis.isTouched=v[5]>0; glottis.alwaysVoice=v[6]>0; glottis.autoWobble=v[7]>0;
        glottis.vibratoAmount=v[8]; glottis.vibratoFrequency=v[9];
        tract.glottalReflection=v[14]; tract.lipReflection=v[15]; shaper.movementSpeed=v[16];
        for(let i=0;i<44;++i) shaper.targetDiameter[i]=v[32+i]>=0?v[32+i]:shaper.getRestDiameter(i);
        for(let i=0;i<nc+nt;++i) {
            const c=points[i], alive=v[106+3*i]>0;
            if(alive&&!c.alive) {c.startTime=tract.time;c.endTime=NaN;c.active=true;}
            if(!alive&&c.alive) c.endTime=tract.time;
            if(alive) {c.position=v[104+3*i];c.diameter=v[105+3*i];}
            c.alive=alive;
            if(i<nc&&alive) {
                if(c.position>17&&c.diameter<-.8) shaper.velumTarget=.4;
                if(c.diameter>=-1.65) ui.reduceTargetDiametersByTouch(c.position,Math.max(0,c.diameter-.3));
            }
        }
        tract.turbulencePoints=points.filter(p=>p.active);
    }
    apply(controls[0]); synth.reset();
    const expected = new Float64Array(frames * 148);
    let sample = 0, substep = 0, lip1 = 0, nose1 = 0, glottal = 0;
    const originalGlottisStep = glottis.step.bind(glottis);
    glottis.step = lambda => { glottal=originalGlottisStep(lambda);return glottal; };
    const originalTractStep = tract.step.bind(tract);
    tract.step = (g,lambda) => {
        const result=originalTractStep(g,lambda);
        const lip=tract.right[43], nose=tract.noseRight[27];
        if(substep++ % 2 === 0) {lip1=lip;nose1=nose;}
        else {
            const offset=sample++*148;
            // Channel zero is filled from Synthesizer.synthesize's real output,
            // below, so the oracle also verifies its summation order and gain.
            expected[offset+1]=(lip1+lip)*.125; expected[offset+2]=(nose1+nose)*.125; expected[offset+3]=glottal;
            expected.set(tract.maxAmplitude,offset+4);expected.set(tract.noseMaxAmplitude,offset+48);
            expected.set(tract.diameter,offset+76);expected.set(tract.noseDiameter,offset+120);
        }
        return result;
    };
    const buffer=new Float64Array(block);
    for(const row of controls) {
        apply(row);
        const firstSample=sample;
        synth.synthesize(buffer);
        for(let i=0;i<block;++i) expected[(firstSample+i)*148]=buffer[i];
    }
    const header=[20260913,sr,seed,block,frames,nc,nt,1];
    const controlFile=path.join(scratch,test.name+'.controls.bin');
    const outputFile=path.join(scratch,test.name+'.cpp.bin');
    const payload=new Float64Array(header.length+controls.length*controls[0].length);
    payload.set(header);
    controls.forEach((v,i)=>payload.set(v,header.length+i*v.length));
    fs.writeFileSync(controlFile,Buffer.from(payload.buffer));
    execFileSync(path.join(build,'reference_render'),[controlFile,outputFile]);
    const bytes=fs.readFileSync(outputFile);
    const actual=new Float64Array(bytes.buffer,bytes.byteOffset,bytes.length/8);
    let maxError=0, maxAudioError=0, sumError=0, maxChannel=0, maxFrame=0;
    if(actual.length!==expected.length) throw Error('Output length mismatch');
    for(let i=0;i<actual.length;++i) {
        if(!Number.isFinite(actual[i])) throw Error(`Nonfinite ${test.name} ${i}`);
        if(!Number.isFinite(expected[i])) throw Error(`Nonfinite upstream reference ${test.name} ${i}`);
        const error=Math.abs(actual[i]-expected[i]);
        if(error>maxError) {maxError=error;maxChannel=i%148;maxFrame=Math.floor(i/148);}
        if(i%148<4) {maxAudioError=Math.max(maxAudioError,error);sumError+=error*error;}
    }
    const result={name:test.name,sampleRate:sr,modelBlockSize:block,frames,channels:148,
        maxAbsoluteError:maxError,maxAudioError,audioRMSError:Math.sqrt(sumError/(frames*4)),maxChannel,maxFrame,pass:maxError<1e-10};
    report.push(result);
    console.log(JSON.stringify(result));
    // Retain a compact four-channel upstream reference for server integration.
    if(test.name==='default_48000') {
        const four=new Float32Array(frames*4);
        for(let i=0;i<frames;++i)for(let j=0;j<4;++j)four[i*4+j]=expected[i*148+j];
        fs.writeFileSync(path.join(scratch,'default_48000.upstream.f32'),Buffer.from(four.buffer));
    }
    fs.unlinkSync(outputFile);
}
fs.writeFileSync(path.join(build,'upstream-comparison.json'),JSON.stringify({
    upstreamCommit:'359c2d3b42b10280404c1650dc601902112b4c90',
    method:'Pinned TypeScript, unmodified DSP, deterministic Math.random, float32 controls, float64 computation, 148 outputs',cases:report
},null,2)+'\n');
if(report.some(x=>!x.pass)) process.exitCode=1;
