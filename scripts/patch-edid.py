#!/usr/bin/env python3
"""Patch dongle EDID by replacing DTD 4 with 1280x800@90 DTD from sdoled.

Layout of EDID 1.3/1.4 Block 0 (128 bytes):
  bytes  0-7   : header magic
  bytes  8-19  : vendor / product / serial
  bytes 20-53  : various
  bytes 54-71  : DTD 1
  bytes 72-89  : DTD 2
  bytes 90-107 : DTD 3
  bytes 108-125: DTD 4
  byte 126     : extension flag (count of extension blocks)
  byte 127     : block 0 checksum (sum of bytes 0..127 mod 256 == 0)

Block 1 (128 bytes) is the CTA-861 extension — unchanged.

We take the dongle EDID's existing Block 0 + Block 1 unchanged but replace
DTD 4 with the sdoled DTD 1 (which is 1280x800@90 native).

If a fresh build is requested, also bumps EDID version to 1.4 (byte 18=0x01,
byte 19=0x04) for completeness — gamescope/wlroots accept both.
"""
import sys
import struct
from pathlib import Path

DONGLE_EDID = Path("/tmp/dongle_edid.bin")
SDOLED_EDID = Path("/tmp/sdoled_edid.bin")
OUTPUT = Path("/tmp/edid_patched.bin")


def main() -> int:
    dongle = bytearray(DONGLE_EDID.read_bytes())
    sdoled = SDOLED_EDID.read_bytes()

    # Sanity
    assert len(dongle) >= 128, f"dongle EDID too short: {len(dongle)}"
    assert len(sdoled) >= 72, f"sdoled EDID too short: {len(sdoled)}"

    # Extract sdoled DTD 1 (bytes 54..71)
    sdoled_dtd1 = sdoled[54:72]
    print(f"sdoled DTD 1 ({len(sdoled_dtd1)} bytes): {sdoled_dtd1.hex()}")

    # Show what dongle currently has in DTD 4 (108..125)
    dongle_dtd4 = bytes(dongle[108:126])
    print(f"dongle DTD 4 (before): {dongle_dtd4.hex()}")

    # Replace dongle DTD 4 with sdoled DTD 1
    dongle[108:126] = sdoled_dtd1
    print(f"dongle DTD 4 (after):  {bytes(dongle[108:126]).hex()}")

    # Recompute Block 0 checksum (byte 127)
    # The sum of all 128 bytes of Block 0 must be 0 mod 256
    s = sum(dongle[0:127]) & 0xFF
    dongle[127] = (256 - s) & 0xFF
    print(f"new Block 0 checksum: 0x{dongle[127]:02x}")

    # Block 1 (CTA extension) unchanged — no checksum recompute needed
    OUTPUT.write_bytes(bytes(dongle))
    print(f"wrote {OUTPUT} ({len(dongle)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
