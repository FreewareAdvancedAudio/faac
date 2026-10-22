# FAAC - Freeware Advanced Audio Coder
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

"""End-to-end FAAC input checks for RF64, pipes, and PCM bounds."""
import os
import struct
import subprocess
import sys
import tempfile

faac, max_channels = sys.argv[1:]
fmt = struct.pack('<4sIHHIIHH', b'fmt ', 16, 1, 2, 48000, 192000, 4, 16)


def rf64(data_size, table=b'', count=0, extra=b'', wave_format=fmt):
    riff_size = 12 + 8 + 28 + len(table) + len(extra) + len(wave_format) + 8 + data_size + (data_size & 1) - 8
    riff_size += sum(8 + struct.unpack_from('<Q', table, 12 * i + 4)[0] for i in range(count))
    align = struct.unpack_from('<H', wave_format, 20)[0]
    ds = struct.pack('<QQQI', riff_size, data_size, data_size // align, count) + table
    return b'RF64\xff\xff\xff\xffWAVEds64' + struct.pack('<I', len(ds)) + ds + extra + wave_format + b'data\xff\xff\xff\xff'


with tempfile.TemporaryDirectory(prefix='faac-rf64-') as d:
    path = os.path.join(d, 'input.wav')
    # Encode identical PCM from RIFF/RF64 and stdin. Trailing chunks aren't audio.
    for channels in (1, 2, 6):
        if channels > int(max_channels): continue
        for bits, tag in ((16, 1), (24, 1), (32, 1), (32, 3)):
            align = channels * (bits // 8)
            simple = struct.pack('<4sIHHIIHH', b'fmt ', 16, tag, channels, 48000,
                                 48000 * align, align, bits)
            pcm = bytes(align * 4096)
            body = b'WAVE' + simple + b'data' + struct.pack('<I', len(pcm)) + pcm
            regular = b'RIFF' + struct.pack('<I', len(body)) + body
            large = rf64(len(pcm), wave_format=simple) + pcm + b'JUNK\x04\0\0\0abcd'
            results = []
            for i, data in enumerate((regular, large, large)):
                with open(path, 'wb') as f:
                    f.write(data)
                out = os.path.join(d, str(i) + '.aac')
                command = [faac, '--overwrite', '-q', '2', '--object-type=lc', '-o', out, '-' if i == 2 else path]
                subprocess.run(command, input=data if i == 2 else None, capture_output=True, check=True)
                results.append(open(out, 'rb').read())
            assert results[0] == results[1] == results[2], 'RF64 encoded PCM differs'
    # --ignorelength remains an explicit escape hatch for inaccurate headers.
    mono = struct.pack('<4sIHHIIHH', b'fmt ', 16, 1, 1, 48000, 96000, 2, 16)
    pcm = bytes(8192)
    full = rf64(len(pcm), wave_format=mono) + pcm
    shortened = rf64(2, wave_format=mono) + pcm
    expected = None
    streaming = b'RIFF\xff\xff\xff\xffWAVE' + mono + b'data\xff\xff\xff\xff' + pcm
    for i, data in enumerate((full, shortened, streaming)):
        with open(path, 'wb') as f: f.write(data)
        out = os.path.join(d, 'ignore' + str(i) + '.aac')
        command = [faac, '--object-type=lc', '-o', out, path]
        if i == 1: command.insert(1, '--ignorelength')
        subprocess.run(command, check=True, capture_output=True)
        encoded = open(out, 'rb').read()
        if expected is None: expected = encoded
        else: assert encoded == expected, '--ignorelength did not bypass the data limit'
    for broken in (large[:12] + large[48:], large[:16] + struct.pack('<I', 27) + large[20:],
                   large[:44] + struct.pack('<I', 999) + large[48:]):
        with open(path, 'wb') as f:
            f.write(broken)
        result = subprocess.run([faac, '-o', os.path.join(d, 'bad.aac'), path], capture_output=True)
        assert result.returncode != 0, 'malformed RF64 accepted'
print('RF64 input: ok')
