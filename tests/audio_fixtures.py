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

"""Small deterministic audio fixtures shared by CLI regression tests."""

import math
import struct
import wave


def make_wav(path, channels, rate=44100, secs=2, transient_center=False):
    freqs = (440, 660, 880, 1100, 1320, 1540, 1760, 1980)
    with wave.open(path, "wb") as w:
        w.setnchannels(channels)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(
            struct.pack("<" + "h" * channels,
                        *(28000 if transient_center and c == 2 and i % (rate // 5) == 0 else
                          int(8000 * math.sin(2 * math.pi * freqs[c % len(freqs)] * i / rate))
                          for c in range(channels)))
            for i in range(int(rate * secs))))
