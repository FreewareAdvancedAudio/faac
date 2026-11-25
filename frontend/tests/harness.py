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

"""Shared setup for the Python tests; import it before spawning anything.

Slow and cross-compiled targets differ from a developer machine in three ways
that every test must handle the same way:

* Wall-clock deadlines inside a test (a live pipe that must produce output in
  N seconds) are tuned for a fast machine. scaled() stretches them by the same
  multiplier Meson applies to the test's own time limit.
* In a cross build the frontends are foreign binaries. Meson starts C tests
  through its exe_wrapper, but a Python test spawns them itself, so this module
  prefixes the wrapper (for example "qemu-s390x -L /usr/s390x-linux-gnu") to
  every foreign ELF program started by path. Host programs such as ffmpeg
  keep running natively.
* Binary output of a program under test is in that program's byte order, which
  is not the Python host's in a cross build; use byteorder_of() with struct.

A test that hangs prints every thread's stack when Meson sends SIGTERM at the
time limit, so the buildd log shows where it was stuck.
"""
import faulthandler
import os
import shlex
import signal
import subprocess
import sys

TIMEOUT_SCALE = max(1, int(os.environ.get("FAAC_TEST_TIMEOUT_SCALE") or 1))
_WRAPPER = shlex.split(os.environ.get("MESON_EXE_WRAPPER", ""))


def shrink_pipe(*files):
    """Best-effort: give pipes a 4 KiB buffer, the smallest Linux allows.

    GNU/Hurd and other systems have much smaller pipe buffers than Linux's
    64 KiB, so a test that only works while a whole message fits in the pipe
    deadlocks there. Shrinking the pipe makes that fail on Linux too."""
    try:
        import fcntl
        for f in files:
            fcntl.fcntl(f.fileno(), fcntl.F_SETPIPE_SZ, 4096)
    except (ImportError, AttributeError, OSError, ValueError):
        pass


def scaled(seconds):
    """A deadline in seconds, stretched for slow targets."""
    return seconds * TIMEOUT_SCALE


def _elf_identity(path):
    """(class, byte order, machine) from an ELF header, or None if not ELF."""
    try:
        with open(path, "rb") as f:
            header = f.read(20)
    except OSError:
        return None
    if len(header) < 20 or header[:4] != b"\x7fELF" or header[5] not in (1, 2):
        return None
    order = "little" if header[5] == 1 else "big"
    return header[4], header[5], int.from_bytes(header[18:20], order)


def byteorder_of(path):
    """struct prefix ('<' or '>') for data written by the program at path."""
    identity = _elf_identity(path)
    if identity:
        return "<" if identity[1] == 1 else ">"
    return "<" if sys.byteorder == "little" else ">"


_HOST = _elf_identity(sys.executable)


def _is_foreign_binary(path):
    identity = _elf_identity(path)
    return identity is not None and _HOST is not None and identity != _HOST


if hasattr(signal, "SIGTERM") and hasattr(faulthandler, "register"):
    faulthandler.register(signal.SIGTERM, all_threads=True, chain=True)

if _WRAPPER:
    _popen_init = subprocess.Popen.__init__

    def _wrapped_init(self, args, *rest, **kw):
        if (isinstance(args, (list, tuple)) and args and os.sep in os.fspath(args[0])
                and _is_foreign_binary(args[0])):
            args = _WRAPPER + [os.fspath(a) for a in args]
        _popen_init(self, args, *rest, **kw)

    subprocess.Popen.__init__ = _wrapped_init
