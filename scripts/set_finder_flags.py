#!/usr/bin/env python3
"""set_finder_flags.py: turns on the Finder's hasBundle and hasCustomIcon
flags in a Retro68 build, so the Finder shows the app's own icon.

    python3 scripts/set_finder_flags.py <build dir> <app name>

Retro68 writes the type and creator but leaves the Finder flags at 0, and
without hasBundle the Finder never reads the app's BNDL. hasCustomIcon makes
it use the app's ICN# -16455 straight away, even on a Mac (or file server)
that already knows the creator. The flags go into every copy the build makes:

  <name>.APPL  the FinderInfo extended attributes (macOS, Basilisk II)
  %<name>.ad   the AppleDouble header
  <name>.bin   the MacBinary header, and its CRC
  <name>.dsk   the app's catalog record in the HFS disk image

Running it again changes nothing.
"""
import os
import re
import struct
import subprocess
import sys

FLAGS = 0x2000 | 0x0400  # hasBundle | hasCustomIcon


def or_flags(buf, at):
    """ORs FLAGS into the big-endian Finder flags at buf[at:at+2]"""
    struct.pack_into(">H", buf, at, struct.unpack_from(">H", buf, at)[0] | FLAGS)


def patch_xattrs(path):
    for attr in ("com.apple.FinderInfo", "org.BasiliskII.FinderInfo"):
        r = subprocess.run(["xattr", "-px", attr, path], capture_output=True, text=True)
        if r.returncode != 0:
            continue
        info = bytearray(bytes.fromhex(r.stdout))
        if len(info) < 10:
            continue
        or_flags(info, 8)
        subprocess.run(["xattr", "-wx", attr, info.hex(), path], check=True)


def patch_appledouble(path):
    data = bytearray(open(path, "rb").read())
    count = struct.unpack_from(">H", data, 24)[0]
    for i in range(count):
        entry, offset, _ = struct.unpack_from(">III", data, 26 + i * 12)
        if entry == 9:  # Finder Info: type, creator, flags
            or_flags(data, offset + 8)
    open(path, "wb").write(data)


def crc16(data):
    """CRC-16/XMODEM, as MacBinary II uses for its header"""
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else crc << 1
            crc &= 0xFFFF
    return crc


def patch_macbinary(path):
    data = bytearray(open(path, "rb").read())
    had_crc = struct.unpack_from(">H", data, 124)[0] == crc16(data[:124])
    data[73] |= FLAGS >> 8      # the flags' high byte; the low one is at 101
    data[101] |= FLAGS & 0xFF
    if had_crc:
        struct.pack_into(">H", data, 124, crc16(data[:124]))
    open(path, "wb").write(data)


def patch_disk(path, type_creator):
    """The catalog's file record: record type 2, reserved, flags, file type 0,
    then the Finder info (type, creator, Finder flags, ...)."""
    data = bytearray(open(path, "rb").read())
    found = [m.start() for m in re.finditer(rb"\x02\x00.\x00" + re.escape(type_creator), data, re.S)]
    if len(found) != 1:
        sys.exit(f"{path}: found {len(found)} catalog records for {type_creator!r}, expected 1")
    or_flags(data, found[0] + 12)
    open(path, "wb").write(data)


def main():
    build, name = sys.argv[1], sys.argv[2]
    appl = os.path.join(build, name + ".APPL")
    patch_xattrs(appl)
    patch_appledouble(os.path.join(build, "%" + name + ".ad"))
    binary = os.path.join(build, name + ".bin")
    patch_macbinary(binary)
    header = open(binary, "rb").read(128)
    disk = os.path.join(build, name + ".dsk")
    if os.path.exists(disk):
        patch_disk(disk, header[65:73])


if __name__ == "__main__":
    main()
