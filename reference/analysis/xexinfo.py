# Print an Xbox 360 XEX2 header summary (plaintext header fields only; no decryption).
# usage: python3 -I reference/analysis/xexinfo.py <default.xex>
import struct, sys, datetime

d = open(sys.argv[1], "rb").read()
u32 = lambda o: struct.unpack_from(">I", d, o)[0]
u16 = lambda o: struct.unpack_from(">H", d, o)[0]
assert d[:4] == b"XEX2", "not an XEX2"

flags, pe_off, sec_off, nhdr = u32(4), u32(8), u32(0x10), u32(0x14)
print("module flags %08X  PE data offset %X  security info %X  optional headers %d" % (flags, pe_off, sec_off, nhdr))

NAMES = {
    0x000002FF: "resource info", 0x000003FF: "file format", 0x000005FF: "delta patch",
    0x000080FF: "bounding path", 0x00010001: "original base", 0x00010100: "entry point",
    0x00010201: "image base", 0x000103FF: "import libraries", 0x00018002: "checksum/timestamp",
    0x000183FF: "original PE name", 0x000200FF: "static libraries", 0x00020104: "TLS info",
    0x00020200: "default stack size", 0x00020301: "fs cache size", 0x00020401: "default heap size",
    0x00028002: "page heap", 0x00030000: "system flags", 0x00040006: "execution info",
    0x00040201: "title workspace", 0x00040310: "game ratings", 0x00040404: "LAN key",
    0x000405FF: "Xbox 360 logo", 0x000406FF: "multidisc media ids", 0x000407FF: "alt title ids",
    0x00040801: "additional title memory", 0x00E10402: "exports by name",
}
hdr = {}
for i in range(nhdr):
    k, v = u32(0x18 + i * 8), u32(0x1C + i * 8)
    hdr[k] = v
    print("  %08X %-22s %08X" % (k, NAMES.get(k, "?"), v))

def ver(v):
    return "%d.%d.%d.%d" % (v >> 28, (v >> 24) & 0xF, (v >> 8) & 0xFFFF, v & 0xFF)

if 0x00040006 in hdr:
    o = hdr[0x00040006]
    media, version, base_version, title = u32(o), u32(o + 4), u32(o + 8), u32(o + 12)
    plat, exe_type, disc, ndisc = d[o + 16:o + 20]
    print("\ntitle id     %08X (%s-%d)" % (title, chr(title >> 24) + chr((title >> 16) & 0xFF), title & 0xFFFF))
    print("media id     %08X  version %s  base version %s  disc %d/%d" % (media, ver(version), ver(base_version), disc, ndisc))
if 0x000003FF in hdr:
    o = hdr[0x000003FF]
    enc, comp = u16(o + 4), u16(o + 6)
    print("file format  encryption %s  compression %s" % ({0: "none", 1: "normal"}.get(enc, enc),
          {0: "none", 1: "basic", 2: "normal (LZX)", 3: "delta"}.get(comp, comp)))
if 0x000183FF in hdr:
    o = hdr[0x000183FF]
    print("PE name      %s" % d[o + 4:o + u32(o)].split(b"\0")[0].decode())
if 0x00018002 in hdr:
    o = hdr[0x00018002]
    print("PE timestamp %s" % datetime.datetime.fromtimestamp(u32(o + 4), datetime.timezone.utc))
for k, label in ((0x00010100, "entry point "), (0x00010201, "image base  "), (0x00020200, "stack size  "),
                 (0x00020401, "heap size   "), (0x00030000, "system flags")):
    if k in hdr: print("%s %08X" % (label, hdr[k]))
if 0x00020104 in hdr:
    o = hdr[0x00020104]
    print("TLS          slots %d  raw data %08X  data size %X  raw size %X" % (u32(o), u32(o + 4), u32(o + 8), u32(o + 12)))

# Security info: image size, load address, region, allowed media, page descriptors.
s = sec_off
image_size, load_addr = u32(s + 4), u32(s + 0x110)
print("load address %08X  image size %08X  end %08X" % (load_addr, image_size, load_addr + image_size))
print("image flags  %08X  region %08X  allowed media %08X" % (u32(s + 0x10C), u32(s + 0x178), u32(s + 0x17C)))
npages = u32(s + 0x180)
kinds = {1: "code", 2: "data", 3: "read-only"}
page = 0x1000 if u32(s + 0x10C) & 0x10000000 else 0x10000  # image flag: 4 KB pages, else 64 KB
runs, addr = [], load_addr
for i in range(npages):
    info = u32(s + 0x184 + i * 24)
    size, kind = (info >> 4) * page, info & 0xF
    if runs and runs[-1][2] == kind and runs[-1][0] + runs[-1][1] == addr:
        runs[-1][1] += size
    else:
        runs.append([addr, size, kind])
    addr += size
print("page ranges (%d descriptors):" % npages)
for a, sz, k in runs:
    print("  %08X-%08X %-9s %7.2f MB" % (a, a + sz, kinds.get(k, k), sz / 2**20))

if 0x000200FF in hdr:
    o = hdr[0x000200FF]
    n = (u32(o) - 4) // 16
    print("\nstatic libraries (%d):" % n)
    for i in range(n):
        e = o + 4 + i * 16
        name = d[e:e + 8].rstrip(b"\0").decode()
        print("  %-8s %d.%d.%d.%d" % (name, u16(e + 8), u16(e + 10), u16(e + 12), u16(e + 14) & 0xFF))

if 0x000103FF in hdr:
    o = hdr[0x000103FF]
    strsz, nstr = u32(o + 4), u32(o + 8)
    names, p = [], o + 12
    while len(names) < nstr:
        end = d.index(b"\0", p)
        names.append(d[p:end].decode())
        p = end + 1
        while p < o + 12 + strsz and d[p] == 0: p += 1
    p = o + 12 + strsz
    total = 0
    print("\nimport libraries (%d):" % nstr)
    for _ in range(nstr):
        size = u32(p)
        lib_ver, lib_min = u32(p + 0x1C), u32(p + 0x20)
        name_idx, count = u16(p + 0x24), u16(p + 0x26)
        total += count
        print("  %-14s version %-14s min %-14s %4d import records" % (names[name_idx], ver(lib_ver), ver(lib_min), count))
        p += size
    print("  total import records: %d (variables + thunks; each function uses 2)" % total)
