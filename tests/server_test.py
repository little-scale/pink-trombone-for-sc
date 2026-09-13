#!/usr/bin/env python3
"""Offline tests of actual SuperCollider servers. Standard library only."""
import array
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
SC = Path(os.environ.get("SUPERCOLLIDER_APP", "/Applications/SuperCollider.app")) / "Contents/Resources"


def read_float_wav(path):
    data = Path(path).read_bytes()
    assert data[:4] in (b"RIFF", b"RF64") and data[8:12] == b"WAVE"
    offset = 12
    channels = rate = bits = 0
    samples = None
    while offset + 8 <= len(data):
        tag, size = struct.unpack_from("<4sI", data, offset)
        body = data[offset + 8:offset + 8 + size]
        if tag == b"fmt ":
            fmt, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", body)
            assert fmt in (3, 65534) and bits == 32, (fmt, bits)
        if tag == b"data":
            samples = array.array("f")
            samples.frombytes(body)
            if sys.byteorder != "little":
                samples.byteswap()
        offset += 8 + size + size % 2
    assert samples is not None
    return channels, rate, samples


def render(server, score, channels=4, block=64, arch="arm64", memory=32768):
    output = BUILD / f"{score}-{server}-{arch}-{block}-{memory}k.wav"
    command = ["arch", f"-{arch}", str(SC / server), "-N", str(BUILD / f"{score}.osc"), "_",
               str(output), "48000", "WAV", "float", "-o", str(channels), "-i", "0",
               "-z", str(block), "-m", str(memory), "-w", "1024", "-D", "0",
               "-U", str(SC / "plugins") + ":" + str(BUILD / "plugins")]
    result = subprocess.run(command, capture_output=True, text=True, timeout=90)
    log = result.stdout + result.stderr
    (BUILD / f"{output.stem}.log").write_text(log)
    if result.returncode or "FAILURE" in log or "exception" in log.lower() or "ERROR" in log:
        raise RuntimeError(f"{command}\n{log}")
    actual_channels, sr, samples = read_float_wav(output)
    assert actual_channels == channels and sr == 48000
    assert all(math.isfinite(x) for x in samples), output
    return samples, output, log


def main():
    reference = array.array("f")
    reference.frombytes((BUILD / "reference/default_48000.upstream.f32").read_bytes())
    if sys.byteorder != "little":
        reference.byteswap()
    report = []
    for server in ("scsynth", "supernova"):
        for arch, block in (("arm64", 16), ("arm64", 64), ("arm64", 512), ("x86_64", 64)):
            samples, output, _ = render(server, "default", block=block, arch=arch)
            assert len(samples) >= len(reference)
            max_error = max(abs(a-b) for a, b in zip(samples, reference))
            assert max_error < 1e-6, (server, arch, block, max_error)
            report.append(dict(server=server, architecture=arch, serverBlockSize=block,
                               framesCompared=len(reference)//4, maxAudioError=max_error, passTest=True))
            print(json.dumps(report[-1]), flush=True)
        for score, channels in (("pinkAudio", 4), ("pinkDiagnostics", 148), ("pinkArrays", 4),
                                ("pinkExternalNoise", 4), ("pinkInvalidNumbers", 4)):
            samples, output, _ = render(server, score, channels=channels)
            energy = sum(x*x for i,x in enumerate(samples) if i % channels == 0)
            assert energy > 1e-7, (score, energy)
            assert max(abs(x) for x in samples) < 100, (score, "unexpected amplitude")
            if channels == 148:
                assert all(x >= 0 for i,x in enumerate(samples) if i % channels >= 4)
            report.append(dict(server=server, scenario=score, channels=channels, frames=len(samples)//channels,
                               peak=max(abs(x) for x in samples), passTest=True))
            print(json.dumps(report[-1]), flush=True)
    samples, _, log = render("scsynth", "default", memory=256)
    assert all(x == 0 for x in samples) and "alloc" in log.lower()
    report.append(dict(server="scsynth", scenario="allocation_failure", realTimeMemoryKiB=256,
                       result="silent output and safe destruction", passTest=True))
    print(json.dumps(report[-1]), flush=True)
    (BUILD / "server-tests.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
