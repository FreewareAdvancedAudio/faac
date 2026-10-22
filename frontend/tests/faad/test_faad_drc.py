#!/usr/bin/env python3
# FAAD - Freeware Advanced Audio Decoder
# Copyright (C) 2026 Nils Schimmelmann
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

"""Generated fill elements; expectations derive only from octave gain formulas."""
import math
import os
import struct
import subprocess
import sys
import tempfile
import wave

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../tests")))
from audio_fixtures import make_wav
from drc_stream import drc, inject


def main():
    faac, probe = sys.argv[1:3]
    enabled, sbr = sys.argv[3:5]
    enabled = enabled == 'true'
    case = sys.argv[5]
    channels = min(2, int(sys.argv[6]))
    if case == 'excluded' and channels < 2:
        return sys.exit(77)
    if case in ('he', 'hetilt') and sbr != 'true':
        return sys.exit(77)
    with tempfile.TemporaryDirectory() as directory:
        src = os.path.join(directory, 'in.wav')
        make_wav(src, channels, secs=0.35)
        aac = os.path.join(directory, 'in.aac')
        subprocess.run([faac, '-a', '--object-type', 'lc', '-o', aac, src], check=True, capture_output=True)
        original = open(aac, 'rb').read()
        def decode(data, cut=0, boost=0):
            path = os.path.join(directory, 'case.aac')
            with open(path, 'wb') as f:
                f.write(data)
            result = subprocess.run([probe, path, str(cut), str(boost), '0', '1', '0'], capture_output=True)
            assert result.returncode == 0, result.returncode
            records = []
            pos = 0
            while pos < len(result.stdout):
                r = struct.unpack_from('=7I', result.stdout, pos)
                pos += 28
                pcm = result.stdout[pos:pos + r[2]]
                pos += r[2]
                assert r[0] == 0 and r[2] > 0, r
                records.append((r, pcm))
            assert len(records) >= 8
            return records
        def samples(records, channel=None):
            data = b''.join(p for _, p in records[3:])
            v = struct.unpack('=' + 'f' * (len(data) // 4), data)
            return v if channel is None else v[channel::channels]
        def ratio(actual, reference, expected, tolerance=.05):
            a, b = sum(x*x for x in actual), sum(x*x for x in reference)
            assert b > 1e-8
            db = 10 * math.log10(a / b)
            assert abs(db - expected) < tolerance, (db, expected)
        reference = decode(original)
        if case == 'hetilt' and sbr == 'true':
            # A longer projection reduces SBR startup history's effect on the HF level.
            # Keep the HF tone above the 11.025 kHz core but below
            # the less stable top SBR bands.
            tones = (500, 12000)
            with wave.open(src, 'wb') as w:
                w.setparams((channels, 2, 44100, 0, 'NONE', ''))
                w.writeframes(b''.join(struct.pack('<' + 'h'*channels, *([int(sum(6000*math.sin(2*math.pi*f*i/44100) for f in tones))]*channels)) for i in range(88200)))
            os.unlink(aac)
            subprocess.run([faac, '-a', '--object-type', 'he-aac-v1', '-b', str(32 * channels), '-o', aac, src], check=True, capture_output=True)
            he = open(aac, 'rb').read()
            base = samples(decode(he), 0)
            changed = samples(decode(inject(he, drc()), cut=127), 0)
            def energy(v, frequency):
                # Separate core and reconstructed HF levels so core energy cannot hide missing SBR gain.
                return sum(x*math.cos(2*math.pi*frequency*i/44100) for i,x in enumerate(v))**2 + sum(x*math.sin(2*math.pi*frequency*i/44100) for i,x in enumerate(v))**2
            for frequency in tones:
                assert energy(base, frequency) > 1e-8
                db = 10*math.log10(energy(changed, frequency)/energy(base, frequency))
                assert abs(db - (-20*math.log10(2) if enabled else 0)) < .5, (frequency, db)
            print('HE core and HF gain: PASS')
        if case == 'he' and sbr == 'true':
            os.unlink(aac)
            subprocess.run([faac, '-a', '--object-type', 'he-aac-v1', '-b', str(32 * channels), '-o', aac, src], check=True, capture_output=True)
            he = open(aac, 'rb').read()
            # SBR envelope reconstruction and startup history permit a looser level bound.
            ratio(samples(decode(inject(he, drc()), 127)), samples(decode(he)), -20*math.log10(2) if enabled else 0, .5)
            print('HE full-band gain: PASS')


if __name__ == '__main__':
    main()
