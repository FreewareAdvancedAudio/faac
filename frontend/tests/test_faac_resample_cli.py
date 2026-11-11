#!/usr/bin/env python3
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
"""Check FAAC's resampling policy: High quality above 48 kHz, and low-rate stereo HE-AAC.

Usage: test_faac_resample_cli.py <faac> <faad> <true|false>

The last argument says whether the build can code stereo HE-AAC.
"""

import math
import os
import struct
import subprocess
import sys
import tempfile
import wave


faac, faad = map(os.path.abspath, sys.argv[1:3])
he_stereo = sys.argv[3] == "true"
cases = ((64000, 48000), (88200, 44100), (96000, 48000))

with tempfile.TemporaryDirectory() as tmp:
    for input_rate, output_rate in cases:
        source = os.path.join(tmp, f"input-{input_rate}.wav")
        encoded = os.path.join(tmp, f"output-{input_rate}.aac")
        decoded = os.path.join(tmp, f"decoded-{input_rate}.wav")
        with wave.open(source, "wb") as wav:
            wav.setnchannels(1)
            wav.setsampwidth(2)
            wav.setframerate(input_rate)
            pcm = b"".join(
                struct.pack("<h", int(12000 * math.sin(2 * math.pi * 440 * i / input_rate)))
                for i in range(input_rate)
            )
            wav.writeframes(pcm)

        result = subprocess.run(
            [faac, "--rate", "auto", "--object-type", "lc", "-b", "128",
             "-a", "--overwrite", "-o", encoded, source],
            capture_output=True,
            text=True,
        )
        output = result.stdout + result.stderr
        if result.returncode:
            print(output, file=sys.stderr)
            raise SystemExit(f"FAAC failed unexpectedly for {input_rate} Hz input")

        expected = f"Resampling input: {input_rate} Hz -> {output_rate} Hz (FAAR High)"
        if expected not in output:
            raise SystemExit(f"missing High-profile report: {output}")

        decoded_result = subprocess.run(
            [faad, "--quiet", "--overwrite", "-o", decoded, encoded],
            capture_output=True,
            text=True,
        )
        if decoded_result.returncode:
            print(decoded_result.stderr, file=sys.stderr)
            raise SystemExit(f"FAAD could not decode the {input_rate} Hz test stream")
        with wave.open(decoded, "rb") as wav:
            if (wav.getframerate(), wav.getnchannels()) != (output_rate, 1):
                raise SystemExit(
                    f"unexpected decoded format for {input_rate} Hz input: "
                    f"{wav.getframerate()} Hz, {wav.getnchannels()} channels"
                )

    if he_stereo:
        # Stereo HE-AAC drops to 32 kHz through 5 kbps per channel and keeps 48 kHz above.
        source = os.path.join(tmp, "input-stereo-48000.wav")
        with wave.open(source, "wb") as wav:
            wav.setnchannels(2)
            wav.setsampwidth(2)
            wav.setframerate(48000)
            wav.writeframes(b"".join(
                struct.pack("<hh", int(12000 * math.sin(2 * math.pi * 440 * i / 48000)),
                            int(9000 * math.sin(2 * math.pi * 660 * i / 48000)))
                for i in range(48000)
            ))
        for bitrate, output_rate in ((8, 32000), (12, 48000)):
            encoded = os.path.join(tmp, f"he-{bitrate}.aac")
            decoded = os.path.join(tmp, f"he-{bitrate}.wav")
            result = subprocess.run(
                [faac, "--rate", "auto", "--object-type", "he-v1", "-b", str(bitrate),
                 "-a", "--overwrite", "-o", encoded, source],
                capture_output=True,
                text=True,
            )
            output = result.stdout + result.stderr
            if result.returncode:
                print(output, file=sys.stderr)
                raise SystemExit(f"FAAC failed unexpectedly at {bitrate} kbps")
            if ("Resampling input: 48000 Hz -> 32000 Hz" in output) != (output_rate == 32000):
                raise SystemExit(f"unexpected resampling report at {bitrate} kbps: {output}")
            if "Object type: HE-AAC v1" not in output:
                raise SystemExit(f"profile changed at {bitrate} kbps: {output}")
            decoded_result = subprocess.run(
                [faad, "--quiet", "--overwrite", "-o", decoded, encoded],
                capture_output=True,
                text=True,
            )
            if decoded_result.returncode:
                print(decoded_result.stderr, file=sys.stderr)
                raise SystemExit(f"FAAD could not decode the {bitrate} kbps stream")
            with wave.open(decoded, "rb") as wav:
                if (wav.getframerate(), wav.getnchannels()) != (output_rate, 2):
                    raise SystemExit(
                        f"unexpected decoded format at {bitrate} kbps: "
                        f"{wav.getframerate()} Hz, {wav.getnchannels()} channels"
                    )

print("Resampling above 48 kHz uses FAAR High and decodes at the selected rate")
if he_stereo:
    print("Low-rate stereo HE-AAC decodes at 32 kHz and keeps 48 kHz above 5 kbps per channel")
