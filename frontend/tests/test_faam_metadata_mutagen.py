"""
FAAM - Freeware Advanced Audio Muxer
Copyright (C) 2026 Nils Schimmelmann

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.
"""

"""Optional interoperability check against an independent MP4 implementation."""
import pathlib
import subprocess
import sys
import tempfile

try:
    from mutagen.mp4 import MP4, MP4FreeForm, AtomDataType
except ImportError:
    print("Mutagen is not installed; skipping interoperability checks")
    sys.exit(77)

faam, fixture = sys.argv[1:3]
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    song = root / "interop.m4a"
    subprocess.run([fixture, "--fixture", str(song)], check=True)
    tags = MP4(song).tags
    assert tags["\xa9lyr"] == ["First line\nDeuxième ligne\n日本語\n"]
    assert tags["tmpo"] == [120]
    assert len(tags["covr"]) == 2
    assert tags["----:org.example:Identifier"] == [b"first", b"second"]

    source = root / "lyrics.txt"
    lyrics = "Unicode 🎵\n第二行\n"
    source.write_bytes(lyrics.encode("utf-8"))
    subprocess.run([faam, "tag", str(song), "--lyrics-file", str(source), "--bpm", "135",
                    "--work", "Symphony", "--movement", "Allegro", "--movement-number", "2",
                    "--movement-total", "4", "--show-work-movement"], check=True, capture_output=True)
    external = MP4(song)
    assert external.tags["\xa9lyr"] == [lyrics]
    assert external.tags["tmpo"] == [135]
    assert external.tags["\xa9wrk"] == ["Symphony"]
    assert external.tags["\xa9mvn"] == ["Allegro"]
    assert external.tags["\xa9mvi"] == [2] and external.tags["\xa9mvc"] == [4]
    assert external.tags["shwm"] == [1]
    # Mutagen writes multiple data children in a parent and opaque freeform data.
    external.tags["\xa9lyr"] = ["external first", "external second"]
    external.tags["----:org.example:Binary"] = [MP4FreeForm(b"\x00\xff\x80", dataformat=AtomDataType.IMPLICIT)]
    external.save()
    info = subprocess.run([faam, "info", str(song)], check=True, capture_output=True).stdout.decode()
    assert "Lyrics: external first" in info and "Lyrics: external second" in info
    subprocess.run([faam, "tag", str(song), "--title", "Changed"], check=True, capture_output=True)
    tags = MP4(song).tags
    assert tags["\xa9lyr"] == ["external first", "external second"]
    assert bytes(tags["----:org.example:Binary"][0]) == b"\x00\xff\x80"
    assert tags["\xa9nam"] == ["Changed"] and len(tags["covr"]) == 2
    subprocess.run([faam, "tag", str(song), "--remove", "lyrics"], check=True, capture_output=True)
    assert "\xa9lyr" not in MP4(song).tags
    # A genre is read back as one value: Mutagen folds a numeric gnre atom into the same key.
    for genre, expected in (("Rock", "Rock"), ("17", "Rock"), ("191", "Psybient"), ("Not A Genre", "Not A Genre")):
        subprocess.run([faam, "tag", str(song), "--genre", genre], check=True, capture_output=True)
        assert MP4(song).tags["\xa9gen"] == [expected]
print("Mutagen bidirectional interoperability passed")
