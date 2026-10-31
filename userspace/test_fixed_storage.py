#!/usr/bin/env python3
"""Check fixed decoder ownership, reference bytes, corruption, and no heap symbols."""
import argparse
import lzma
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def main():
    """Run native decoder cases against independently encoded fixture bytes."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", required=True)
    parser.add_argument("--library", required=True)
    parser.add_argument("--nm", default="nm")
    args = parser.parse_args()
    symbols = subprocess.check_output([args.nm, "-u", args.library], text=True)
    assert not re.search(r"\b(?:malloc|calloc|realloc|free|aligned_alloc|xz_dec_init)$",
                         symbols, re.MULTILINE), symbols
    payload = bytes(range(256)) * 2048
    filters = [{"id": lzma.FILTER_LZMA2, "dict_size": 8 * 1024 * 1024}]
    cases = []
    for check in (lzma.CHECK_NONE, lzma.CHECK_CRC32, lzma.CHECK_CRC64, lzma.CHECK_SHA256):
        cases.append((f"check-{check}", lzma.compress(payload, check=check, filters=filters), payload, 0))
    valid = cases[1][1]
    cases += [("truncated", valid[:-1], payload, 8),
              ("bad-header", b"!" + valid[1:], payload, 5),
              ("bad-footer", valid[:-1] + b"!", payload, 7),
              ("oversized-dictionary", lzma.compress(payload, check=lzma.CHECK_CRC32,
                  filters=[{"id": lzma.FILTER_LZMA2, "dict_size": 16 * 1024 * 1024}]), payload, 4),
              ("concatenated", valid + valid, payload * 2, 0),
              ("valid-padding", valid + b"\0" * 4, payload, 0),
              ("bad-padding", valid + b"\0", payload, 7)]
    if shutil.which("xz"):
        blocks = subprocess.check_output(["xz", "--threads=1", "--check=crc32",
            "--lzma2=dict=8MiB", "--block-size=16KiB", "--stdout"], input=payload)
        cases.append(("multi-block", blocks, payload, 0))
    with tempfile.TemporaryDirectory(prefix="xz-fixed-storage-") as temporary:
        directory = Path(temporary)
        for name, compressed, expected, code in cases:
            source, reference = directory / "input.xz", directory / "expected"
            source.write_bytes(compressed)
            reference.write_bytes(expected)
            result = subprocess.run([args.driver, str(source), str(reference)], timeout=15)
            assert result.returncode == code, (name, result.returncode, code)
            print(f"PASS {name}", flush=True)


if __name__ == "__main__":
    main()
