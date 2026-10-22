"""Structurally validate XenonRecomp's output in ppc/: function mapping vs
definitions, .pdata coverage, code-section coverage, and the markers
XenonRecomp leaves where translation failed. Exits 1 if a HARD check fails
(ReCarbon M1: failure markers and recompiler log errors are HARD too).

    python3 survey/validate_recomp.py [game/survey/recomp.log]

What XenonRecomp emits (tools/XenonRecomp/XenonRecomp/recompiler.cpp):
  ppc_func_mapping.cpp   "\t{ 0x%X, name }," for every image symbol, i.e. every
                         recompiled function plus secondary entries, import
                         thunks and __save*/__rest* helpers (no bodies).
  ppc_recomp.N.cpp       "PPC_FUNC_IMPL(__imp__name) {" + "PPC_WEAK_FUNC(name) {"
                         wrapper, or with XENON_RECOMP_USE_ALIAS (non-Apple
                         clang) '__attribute__((alias("__imp__name"))) PPC_WEAK_FUNC(name);'.
                         Every guest instruction gets exactly one "\t// mnemonic ops"
                         comment, so a body's instruction count is its size / 4.
"""
import bisect, collections, glob, os, re, struct, sys

from layout import BASE, SECTIONS, PDATA
IMG = 'game/survey/image.bin'
PPC = 'ppc'
img = open(IMG, 'rb').read()
CODE = [(s, e) for _, s, e, k in SECTIONS if k == 'code']

def in_code(a):
    return any(s <= a < e for s, e in CODE)

# recompiler.cpp emission patterns
MAP_RE = re.compile(r'^\t\{ 0x([0-9A-F]+), (\w+) \},$')                    # :2731
IMPL_RE = re.compile(r'^PPC_FUNC_IMPL\(__imp__(\w+)\) \{$')                # :2515
WEAK_RE = re.compile(r'^(?:__attribute__\(\(alias\("__imp__\w+"\)\)\) )?PPC_WEAK_FUNC\((\w+)\)')  # :2512/:2576
INSN_RE = re.compile(r'^\t// (\S+)(?: (.*))?$')                           # :266 and :2546
CALL_ERR_RE = re.compile(r'^\t// ERROR ([0-9A-F]+)$')                      # :402
SWITCH_ERR_RE = re.compile(r'^\t\t// ERROR: 0x([0-9A-F]+)$')               # :622
DEBUGTRAP = '__builtin_debugtrap();'                                       # :717 (blrl)
# Opcodes whose case emits nothing on purpose (recompiler.cpp:593-1739);
# any other instruction with no code after its comment is a silent drop
# (Recompile() returned false, recompiler.cpp:2387 -> log line :2559).
NOOPS = {'attn', 'cctpl', 'cctpm', 'db16cyc', 'dcbf', 'dcbst', 'dcbt', 'dcbtst', 'nop',
         'tdlgei', 'tdllei', 'twi', 'twlgei', 'twllei'}
# b/bl to __save*/__rest* also emit nothing with non_volatile_as_local (:390).
SILENT_BRANCHES = {'b', 'bl'}

fail = False
def hard(ok, msg):
    global fail
    print(('ok   ' if ok else 'FAIL ') + msg)
    fail |= not ok

# --- mapping -----------------------------------------------------------------
mapping = {}
for line in open(f'{PPC}/ppc_func_mapping.cpp'):
    m = MAP_RE.match(line.rstrip('\n'))
    if m:
        mapping[m.group(2)] = int(m.group(1), 16)
map_addrs = set(mapping.values())

# --- definitions + markers ---------------------------------------------------
cpps = sorted(glob.glob(f'{PPC}/ppc_recomp.*.cpp'), key=lambda p: int(p.split('.')[-2]))
impls = collections.Counter()
weaks = collections.Counter()
extent = {}                       # primary function name -> instruction count
secondary = set()
markers = collections.defaultdict(list)    # kind -> [(addr, insn)]

for path in cpps:
    lines = open(path, encoding='latin1').read().split('\n')
    name = None
    for i, line in enumerate(lines):
        if name is None:
            m = IMPL_RE.match(line)
            if m:
                name, n, addr, last = m.group(1), 0, mapping.get(m.group(1)), None
                impls[name] += 1
                continue
            m = WEAK_RE.match(line)
            if m:
                weaks[m.group(1)] += 1
            continue
        if line == '}':
            if name not in secondary:
                extent[name] = n
            name = None
            continue
        if n == 0 and line.startswith('\tgoto loc_'):     # :2628, secondary entry
            secondary.add(name)
        if name in secondary or addr is None:
            continue                                      # body duplicates its parent
        if CALL_ERR_RE.match(line):
            markers['branch/call to non-function (// ERROR X)'].append((ia, last))
        elif m := SWITCH_ERR_RE.match(line):
            markers['switch case outside function (// ERROR: 0xX)'].append((ia, f'{last}-> 0x{m.group(1)}'))
        elif line == DEBUGTRAP:
            markers['blrl -> __builtin_debugtrap()'].append((ia, last))
        else:
            m = INSN_RE.match(line)
            if not m:
                continue
            ia, last = addr + 4 * n, line[4:]
            n += 1
            mnem = m.group(1)
            if mnem == '.long':
                if m.group(2) != '0x0':
                    markers['undecodable (// .long X)'].append((ia, last))
                continue
            nxt = lines[i + 1]
            if (nxt == '}' or nxt.startswith('loc_') or INSN_RE.match(nxt) and not CALL_ERR_RE.match(nxt)) \
                    and mnem not in NOOPS and mnem not in SILENT_BRANCHES:
                markers['silently dropped (comment only)'].append((ia, last))
            if mnem == 'attn':
                markers['attn (emitted as nothing)'].append((ia, last))

# --- 1. counts ---------------------------------------------------------------
total = sum(os.path.getsize(p) for p in glob.glob(f'{PPC}/*'))
print(f'== counts\n{len(cpps)} ppc_recomp.*.cpp, ppc/ {total / 1e6:.1f} MB, '
      f'{len(mapping)} mapping entries, {sum(impls.values())} PPC_FUNC_IMPL, '
      f'{sum(weaks.values())} PPC_WEAK_FUNC, {len(secondary)} secondary entries')

# --- 2. mapping <-> definitions ----------------------------------------------
print('\n== mapping vs definitions')
HELPER = re.compile(r'^__(save|rest)(gprlr|fpr|vmx)_\d+$')
undefined = [n for n in mapping if n not in impls]
helpers = [n for n in undefined if HELPER.match(n)]
imports = [n for n in undefined if not HELPER.match(n) and not n.startswith('sub_')]
missing = sorted((mapping[n], n) for n in undefined if n.startswith('sub_'))
print(f'without a body (expected): {len(helpers)} __save/__rest helpers, {len(imports)} import thunks')
hard(not missing, f'{len(missing)} sub_ mapping entries with no definition'
     + ''.join(f'\n       {a:08X} {n}' for a, n in missing[:20]))
extra = sorted(n for n in impls if n not in mapping)
hard(not extra, f'{len(extra)} definitions not in the mapping ' + ' '.join(extra[:20]))
dup = sorted(n for n, c in impls.items() if c != 1)
hard(not dup, f'{len(dup)} functions defined more than once ' + ' '.join(dup[:20]))
noweak = sorted(n for n in impls if weaks[n] != impls[n])
hard(not noweak, f'{len(noweak)} PPC_FUNC_IMPL without a matching PPC_WEAK_FUNC ' + ' '.join(noweak[:20]))

# --- 3. .pdata starts --------------------------------------------------------
print('\n== .pdata')
pdata = set()
for a in range(PDATA[0], PDATA[1], 8):
    b, _ = struct.unpack('>II', img[a - BASE:a - BASE + 8])
    if b and in_code(b):
        pdata.add(b)
lost = sorted(pdata - map_addrs)
hard(not lost, f'{len(pdata)} .pdata starts in code, {len(lost)} not in the mapping'
     + (': ' + ' '.join(f'{a:08X}' for a in lost[:20]) if lost else ''))

# --- 4. coverage -------------------------------------------------------------
print('\n== coverage (extent = 4 * instruction comments; import thunks 16 bytes)')
spans = [(mapping[n], mapping[n] + 4 * c) for n, c in extent.items() if n in mapping]
spans = sorted(spans + [(mapping[n], mapping[n] + 16) for n in imports])   # 16-byte thunks, xex.cpp:339
code_bytes = sum(e - s for s, e in CODE)
covered, gaps = 0, []
for s, e in CODE:
    pos = s
    for a, b in spans:
        a, b = max(a, pos), min(b, e)
        if b <= a:
            continue
        if a > pos:
            gaps.append((pos, a))
        covered += b - a
        pos = b
    if pos < e:
        gaps.append((pos, e))
by_addr = {a: n for n, a in mapping.items()}
big = [(a, b) for a, b in gaps if b - a > 64 and any(img[a - BASE:b - BASE])]
print(f'{covered} / {code_bytes} code bytes covered ({100 * covered / code_bytes:.2f}%), '
      f'{len(gaps)} gaps, {len(big)} non-padding gaps > 64 bytes')
for a, b in big[:20]:
    print(f'  {a:08X}..{b:08X} {b - a:6d} bytes  {by_addr.get(a, "")}')

# --- 5. failure markers ------------------------------------------------------
print('\n== translation-failure markers in generated code')
if not markers:
    print('none')
for kind, items in sorted(markers.items()):
    mn = collections.Counter(insn.split()[0] for _, insn in items)
    print(f'{len(items):6d}  {kind}  [{", ".join(f"{k} {v}" for k, v in mn.most_common(8))}]')
    for a, insn in items[:20]:
        print(f'          {a:08X}  {insn}')
# A complete translation (ROADMAP M1) leaves no failure marker.
hard(not markers, f'{sum(map(len, markers.values()))} translation-failure markers')

# --- 6. recompiler log -------------------------------------------------------
if len(sys.argv) > 1:
    print(f'\n== log {sys.argv[1]}')
    LOG = [  # fmt::println strings in recompiler.cpp
        ('switch case outside function (:623)', r'^ERROR: Switch case at ([0-9A-F]+) is trying to jump outside function'),
        ('unrecognized instruction (:2559)', r'^Unrecognized instruction at 0x([0-9A-F]+): (\S+)'),
        ('unable to decode (:2549)', r'^Unable to decode instruction [0-9A-F]+ at ([0-9A-F]+)'),
        ('jump table without switch entry (:2555)', r'^Found a switch jump table at ([0-9A-F]+) with no switch table entry'),
        ('RC bit without comparison (:2396)', r'^(\S+) at ([0-9A-F]+) has RC bit enabled but no comparison'),
        ('secondary entry outside functions (:2758)', r'^ERROR: secondary entry ([0-9A-F]+) is not inside any function'),
        ('patch file error (:48-86)', r'^ERROR: Unable to (apply|load) the patch file'),
        ('progress', r'^Recompiling functions\.\.\.'),
    ]
    found, other = collections.defaultdict(list), []
    for line in open(sys.argv[1], encoding='latin1'):
        line = line.rstrip('\n')
        for kind, rx in LOG:
            m = re.match(rx, line)
            if m:
                found[kind].append(m)
                break
        else:
            if line.strip():
                other.append(line)
    for kind, _ in LOG:
        ms = found.get(kind)
        if not ms or kind == 'progress':
            continue
        print(f'{len(ms):6d}  {kind}', end='')
        if kind.startswith('unrecognized'):
            print('  [' + ', '.join(f'{k} {v}' for k, v in collections.Counter(m.group(2) for m in ms).most_common()) + ']', end='')
        print('\n          ' + ' '.join(m.group(1) for m in ms[:10]))
    print(f'{len(other):6d}  other lines' + ''.join(f'\n          {l[:100]}' for l in other[:10]))
    errors = sum(len(ms) for kind, ms in found.items() if kind != 'progress') + len(other)
    hard(errors == 0, f'{errors} recompiler log errors/warnings')

sys.exit(1 if fail else 0)
