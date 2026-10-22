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

"""Provide an isolated path for sparse large-output PCM writer tests."""
import os
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="faad-large-output-") as directory:
    result = subprocess.run([sys.argv[1], os.path.join(directory, "output.pcm")])
    sys.exit(result.returncode)
