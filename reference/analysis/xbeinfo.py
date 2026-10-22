# Print an original-Xbox XBE header summary (no decryption; header fields only).
# usage: python3 -I reference/analysis/xbeinfo.py <default.xbe>
import struct, sys, datetime

d = open(sys.argv[1], "rb").read()
u32 = lambda o: struct.unpack_from("<I", d, o)[0]
assert d[:4] == b"XBEH"
base = u32(0x104)
def va(addr): return addr - base  # header region is mapped at base

print("base        %08X" % base)
print("hdr size    %08X  image size %08X" % (u32(0x108), u32(0x10C)))
print("timestamp   %s" % datetime.datetime.utcfromtimestamp(u32(0x114)))
ep = u32(0x128); kt = u32(0x158)
for kind, ek, kk in (("retail", 0xA8FC57AB, 0x5B6D40B6), ("debug", 0x94859D4B, 0xEFB1F152)):
    print("%-6s entry %08X  kthunk %08X" % (kind, ep ^ ek, kt ^ kk))
print("stack %X  TLS %08X  init flags %08X" % (u32(0x130), u32(0x12C), u32(0x124)))

c = va(u32(0x118))
title = d[c + 0xC:c + 0xC + 80].decode("utf-16le").rstrip("\0")
print("cert time   %s" % datetime.datetime.utcfromtimestamp(u32(c + 4)))
print("title id    %08X (%s-%03d)" % (u32(c + 8), d[c + 11:c + 12].decode() + d[c + 10:c + 11].decode(), u32(c + 8) & 0xFFFF))
print("title       %s" % title)
print("media %08X region %08X ratings %08X disc %d version %08X" % (u32(c + 0x9C), u32(c + 0xA0), u32(c + 0xA4), u32(c + 0xA8), u32(c + 0xAC)))

ns, sh = u32(0x11C), va(u32(0x120))
secs = []
print("\nsections (%d):" % ns)
for i in range(ns):
    o = sh + i * 56
    fl, vaddr, vsz, raw, rsz, nm = struct.unpack_from("<6I", d, o)
    n = d[va(nm):d.index(b"\0", va(nm))].decode()
    secs.append((vaddr, vsz, raw, rsz))
    print("  %-10s va %08X vsz %08X raw %08X rsz %08X flags %X" % (n, vaddr, vsz, raw, rsz, fl))

def file_off(addr):
    for vaddr, vsz, raw, rsz in secs:
        if vaddr <= addr < vaddr + rsz: return raw + addr - vaddr
    return va(addr)

nl, lv = u32(0x160), va(u32(0x164))
print("\nXDK libraries (%d):" % nl)
for i in range(nl):
    o = lv + i * 16
    name = d[o:o + 8].rstrip(b"\0").decode()
    maj, mi, bld, fl = struct.unpack_from("<4H", d, o + 8)
    print("  %-8s %d.%d.%d flags %04X" % (name, maj, mi, bld, fl))

for kind, kk in (("retail", 0x5B6D40B6), ("debug", 0xEFB1F152)):
    o = file_off(kt ^ kk)
    if 0 < o < len(d) and u32(o) & 0x80000000:
        ords = []
        while u32(o): ords.append(u32(o) & 0x7FFFFFFF); o += 4
        print("\nkernel imports (%s key): %d ordinals" % (kind, len(ords)))
        print("  " + " ".join(map(str, ords)))
        break
