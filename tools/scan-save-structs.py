# SPDX-FileCopyrightText: 2026 Haliax
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Scan a Satisfactory .sav: decompress body chunks, list property/struct tag
# strings relevant to SCIM save-parser compatibility triage.
import re
import sys
import zlib

MAGIC = b"\xc1\x83\x2a\x9e"


def decompress_body(data: bytes) -> bytes:
    out = []
    pos = 0
    while True:
        idx = data.find(MAGIC, pos)
        if idx < 0:
            break
        # Chunk header: magic(4) pad(4) ver(4) then sizes; zlib stream starts
        # at first 0x78 byte after header. Scan a small window.
        window = data[idx + 8 : idx + 96]
        zstart = -1
        for off, byte in enumerate(window):
            if byte == 0x78:
                zstart = idx + 8 + off
                break
        if zstart < 0:
            pos = idx + 4
            continue
        try:
            dec = zlib.decompressobj()
            out.append(dec.decompress(data[zstart : zstart + 4 * 1024 * 1024]))
            consumed = len(data[zstart : zstart + 4 * 1024 * 1024]) - len(dec.unused_data)
            pos = zstart + max(consumed, 1)
        except zlib.error:
            pos = idx + 4
    return b"".join(out)


def main() -> int:
    path = sys.argv[1]
    with open(path, "rb") as fh:
        raw = fh.read()
    body = decompress_body(raw)
    print(f"raw={len(raw)} decompressed={len(body)}")

    probes = [
        b"SoftClassPath",
        b"SoftObjectPath",
        b"SoftObjectProperty",
        b"StructuralNodeId",
        b"StructuralComponentKey",
        b"StructuralEndpointOverrides",
        b"PlayerEndpointOverrides",
        b"ComponentDefaultIds",
        b"NextStructureDefaultIdIndex",
        b"PCSwatchEntry",
        b"PaintFinishPath",
        b"PaintFinish",
        b"StoreSchema",
        b"PCSwatchStore",
        b"StructuralPower",
        b"PipelineColor",
        b"Topograph",
    ]
    for probe in probes:
        count = body.count(probe)
        print(f"{probe.decode():40} {count}")

    # Context dump for each SoftClassPath hit: preceding property name string.
    for match in re.finditer(re.escape(b"SoftClassPath"), body):
        start = max(0, match.start() - 160)
        ctx = body[start : match.end() + 60]
        printable = re.findall(rb"[ -~]{5,}", ctx)
        print("HIT:", b" | ".join(printable).decode(errors="replace"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
