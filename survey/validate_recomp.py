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
FATAL_ERR = '#error "XenonRecomp: '                                        # :2442 (patch 0011)
# Opcodes whose case emits nothing on purpose (recompiler.cpp:593-1739);
# any other instruction with no code after its comment is a silent drop
# (Recompile() returned false, recompiler.cpp:2387 -> FATAL :2598).
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
        if line.startswith(FATAL_ERR):    # :2442, inside a body or (secondary entry) between them
            m = re.search(r'\b(?:0x)?([0-9A-F]{8})\b', line)
            markers['XenonRecomp FATAL (#error)'].append((int(m.group(1), 16) if m else 0, line[len(FATAL_ERR):-1]))
            continue
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

# --- 3b. code pointers built in code (ReCarbon) ------------------------------
# A code address built with lis + addi/ori that follows a blr, an unconditional
# b or padding is a function the game calls through a pointer; if it isn't a
# function start, the call aborts at runtime (cpu/indirect.cpp). NFS: Carbon
# 0x827C1A18 was one, swallowed by a leaf bound (survey/leaf_switch_bounds.py).
print('\n== code pointers built in code')
w = lambda a: struct.unpack('>I', img[a - BASE:a - BASE + 4])[0]
def uncond_exit(x):
    return x == 0x4E800020 or (x >> 26 == 18 and x & 3 == 0)
built = set()
for s0, e0 in CODE:
    for a in range(s0, e0, 4):
        x = w(a)
        if x >> 26 != 15 or (x >> 16) & 31:
            continue
        rd, hi = (x >> 21) & 31, (x & 0xFFFF) << 16
        for b in range(a + 4, min(a + 36, e0), 4):
            y = w(b)
            op = y >> 26
            if op in (14, 24) and (y >> 16) & 31 == rd:
                lo = y & 0xFFFF
                v = (hi + (lo - 0x10000 if op == 14 and lo & 0x8000 else lo)) & 0xFFFFFFFF
                if in_code(v) and v % 4 == 0 and in_code(v - 4) and (w(v - 4) == 0 or uncond_exit(w(v - 4))):
                    built.add(v)
                break
            if (y >> 21) & 31 == rd and op not in (36, 37, 32, 33):
                break
unmapped = sorted(built - map_addrs)
hard(not unmapped, f'{len(built)} entry-like code pointers, {len(unmapped)} not a function start'
     + (': ' + ' '.join(f'{a:08X}' for a in unmapped[:20]) if unmapped else ''))

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

# --- 5b. mid-asm hooks (ReCarbon M6) -----------------------------------------
# Every [[midasm_hook]] in config/recarbon.toml is called at its address in
# ppc/: XenonRecomp silently skips a hook whose address isn't an instruction
# of a recompiled function. The call is the first line after the hooked
# instruction's comment (after its code with after_instruction), so its
# address is the function's start + 4 * (comments so far - 1).
print('\n== mid-asm hooks')
import tomllib
want_hooks = {(h['name'], h['address']) for h in tomllib.load(open('config/recarbon.toml', 'rb')).get('midasm_hook', [])}
HOOK_RE = re.compile(r'^\t(?:if \()?(\w+)\(')
found_hooks = set()
names = {n for n, _ in want_hooks}
PUBLISH = 'RenderQueuePublish'
publish_funcs = {}                # start -> [asm] of a function with a render-queue hook (5c, 5d)
for path in sorted(glob.glob(os.path.join(PPC, 'ppc_recomp.*.cpp'))):
    start = count = None
    asm, hooked = [], False
    for line in open(path):
        m = re.match(r'^PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]+)\)', line)
        if m:
            if hooked:
                publish_funcs[start] = asm
            start, count = int(m.group(1), 16), 0
            asm, hooked = [], False
            continue
        if start is None:
            continue
        if line.startswith('\t// '):
            count += 1
            asm.append(line[4:].strip())
            continue
        h = HOOK_RE.match(line)
        if h and h.group(1) in names and count:
            found_hooks.add((h.group(1), start + 4 * (count - 1)))
            hooked |= h.group(1) in (PUBLISH, 'RenderQueueWindow', 'RenderQueueHandshake')
    if hooked:
        publish_funcs[start] = asm
for name, addr in sorted(want_hooks - found_hooks, key=lambda x: x[1]):
    print(f'  missing: {name} at {addr:08X}')
hard(want_hooks <= found_hooks, f'{len(want_hooks)} mid-asm hooks configured, {len(want_hooks & found_hooks)} called at their address')

# --- 5c. every publish into the render queue is fenced (ReCarbon) -------------
# A RenderQueuePublish hook sits before the store of the queue's write pointer
# (stw rA,20(rS), rS the queue state), which the count's increment follows
# (lwz rC,0(rS); addi rC,rC,1; stw rC,0(rS)); the inlined copies publish the
# same way (runtime/game/render_queue.cpp). The state's address comes from the
# hooked sites that build it in place (lis rT,hi; addi rS,rT,lo), so this is
# address-free; every other site in ppc/ that publishes to it must carry the
# hook too. A config entry lost would otherwise only show at runtime.
STW20_RE = re.compile(r'^stw r\d+,20\((r\d+)\)$')
def state_regs(asm, upto, hi=None, lo=None):
    """Registers set to a 32-bit constant by lis/addi in asm[:upto]: {reg: value}, or
    only those equal to (hi, lo) when given."""
    regs = {}
    for i, insn in enumerate(asm[:upto]):
        a = re.match(r'^addi (r\d+),(r\d+),(-?\d+)$', insn)
        if not a or (lo is not None and int(a.group(3)) != lo):
            continue
        lis = [l for l in asm[:i] if re.match(rf'^lis {a.group(2)},{hi if hi is not None else "-?[0-9]+"}$', l)]
        if lis:
            regs[a.group(1)] = ((int(lis[-1].split(',')[1]) << 16) + int(a.group(3))) & 0xFFFFFFFF
    return regs
def publishes(asm, i, rs):
    """asm[i] stores the write pointer at +20 of rs and the count's increment follows."""
    m = re.match(rf'^lwz (r\d+),0\({rs}\)$', asm[i + 1]) if i + 3 < len(asm) else None
    return bool(m) and asm[i + 2] == f'addi {m.group(1)},{m.group(1)},1' and asm[i + 3] == f'stw {m.group(1)},0({rs})'
states = set()
for start, asm in publish_funcs.items():
    for name, addr in found_hooks:
        i = (addr - start) // 4
        m = STW20_RE.match(asm[i]) if name == PUBLISH and 0 <= i < len(asm) else None
        if m and m.group(1) in state_regs(asm, i):
            states.add(state_regs(asm, i)[m.group(1)])
unfenced = []
for value in sorted(states):
    hi, lo = value >> 16, value & 0xFFFF
    lo -= 0x10000 if lo >= 0x8000 else 0
    hi += 1 if lo < 0 else 0
    hi -= 0x10000 if hi >= 0x8000 else 0
    for path in sorted(glob.glob(os.path.join(PPC, 'ppc_recomp.*.cpp'))):
        text = open(path).read()
        for body in text.split('PPC_FUNC_IMPL(__imp__sub_')[1:] if f',{lo}' in text else []:
            if f',{lo}' not in body:
                continue
            start = int(body[:body.index(')')], 16)
            asm = [l[4:].strip() for l in body.split('\n') if l.startswith('\t// ')]
            regs = state_regs(asm, len(asm), hi, lo)
            for i, insn in enumerate(asm):
                m = STW20_RE.match(insn)
                if m and m.group(1) in regs and publishes(asm, i, m.group(1)) and (PUBLISH, start + 4 * i) not in found_hooks:
                    unfenced.append(start + 4 * i)
for addr in unfenced:
    print(f'  unfenced publish: {addr:08X}')
hard(not unfenced and states, f'render queue state {", ".join(f"{s:08X}" for s in sorted(states)) or "not found"}: {len(unfenced)} publish sites without a RenderQueuePublish hook')

# --- 5d. the recording handshake is fenced on both sides (ReCarbon) -----------
# The recording thread opens a window by resetting the queue state (+0, +4, +8,
# +12) and then storing 1 to a handshake word the main thread polls; the main
# thread then reads +12, +8, +0 and +4 (runtime/game/render_queue.cpp). A
# RenderQueueWindow hook (release) sits on that store and a
# RenderQueueHandshake hook (acquire) at every place the main thread's poll
# leaves on a nonzero word. The word comes from the hooked store (lis rB,hi;
# stw rV,lo(rB), rV = li 1) after the reset of the 5c state, so this is
# address-free too; then every store of 1 to the word and every poll exit on
# nonzero (lwz rX,lo(rB); cmpwi crN,rX,0; then bne crN,T forward, or beq
# crN,back with its fall-through) must carry its hook. The recording thread's
# own wait (cmpwi rX,1: it waits for 0) is not a poll exit on nonzero.
WINDOW, HANDSHAKE = 'RenderQueueWindow', 'RenderQueueHandshake'
STORE_RE = re.compile(r'^(stw|stb) (r\d+),(-?\d+)\((r\d+)\)$')
def last_set(asm, upto, reg):
    """The last instruction in asm[:upto] that writes reg (stores and compares excluded)."""
    for insn in reversed(asm[:upto]):
        if re.match(rf'^(?!st|cmp)\S+ {reg},', insn):
            return insn
    return ''
def lis_value(asm, upto, reg):
    m = re.match(rf'^lis {reg},(-?\d+)$', last_set(asm, upto, reg))
    return int(m.group(1)) if m else None
def word_of(hi, lo):
    return ((hi << 16) + lo) & 0xFFFFFFFF
def func_of(addr):
    for start, asm in publish_funcs.items():
        if start <= addr < start + 4 * len(asm):
            return start, asm
    return None, None
words, bad_window = set(), []
for name, addr in sorted(found_hooks):
    if name != WINDOW:
        continue
    start, asm = func_of(addr)
    i = (addr - start) // 4 if asm else -1
    m = STORE_RE.match(asm[i]) if asm else None
    hi = lis_value(asm, i, m.group(4)) if m and m.group(1) == 'stw' else None
    reset = set()
    for value in states:
        shi, slo = value >> 16, value & 0xFFFF
        slo -= 0x10000 if slo >= 0x8000 else 0
        shi += 1 if slo < 0 else 0
        shi -= 0x10000 if shi >= 0x8000 else 0
        regs = state_regs(asm, i, shi, slo) if asm else {}
        for insn in asm[:i] if asm else []:
            s = STORE_RE.match(insn)
            if s and s.group(4) in regs:
                reset.add(int(s.group(3)))
    if hi is None or last_set(asm, i, m.group(2)) != f'li {m.group(2)},1' or not {0, 4, 8, 12} <= reset:
        bad_window.append(addr)
    else:
        words.add(word_of(hi, int(m.group(3))))
for addr in bad_window:
    print(f'  {WINDOW} at {addr:08X} is not a store of 1 after the queue state reset')
missing, exits, stores = [], set(), 0
for word in sorted(words):
    lo = word & 0xFFFF
    lo -= 0x10000 if lo >= 0x8000 else 0
    hi = ((word - lo) >> 16) & 0xFFFF
    hi -= 0x10000 if hi >= 0x8000 else 0
    for path in sorted(glob.glob(os.path.join(PPC, 'ppc_recomp.*.cpp'))):
        text = open(path).read()
        if f',{lo}(r' not in text:
            continue
        for body in text.split('PPC_FUNC_IMPL(__imp__sub_')[1:]:
            if f',{lo}(r' not in body:
                continue
            start = int(body[:body.index(')')], 16)
            asm = [l[4:].strip() for l in body.split('\n') if l.startswith('\t// ')]
            for i, insn in enumerate(asm):
                s = STORE_RE.match(insn)
                if s and s.group(1) == 'stw' and int(s.group(3)) == lo and lis_value(asm, i, s.group(4)) == hi \
                        and last_set(asm, i, s.group(2)) == f'li {s.group(2)},1':
                    stores += 1
                    if (WINDOW, start + 4 * i) not in found_hooks:
                        missing.append((WINDOW, start + 4 * i))
                l = re.match(rf'^lwz (r\d+),{lo}\((r\d+)\)$', insn)
                if not l or lis_value(asm, i, l.group(2)) != hi or i + 2 >= len(asm):
                    continue
                c = re.match(rf'^cmpwi (cr\d),{l.group(1)},0$', asm[i + 1])
                b = re.match(r'^(bne|beq) (cr\d),0x([0-9a-f]+)$', asm[i + 2]) if c else None
                if not b or b.group(2) != c.group(1):
                    continue
                exit_ = int(b.group(3), 16) if b.group(1) == 'bne' else start + 4 * (i + 3)
                if exit_ > start + 4 * i:
                    exits.add(exit_)
                    if (HANDSHAKE, exit_) not in found_hooks:
                        missing.append((HANDSHAKE, exit_))
for name, addr in sorted(set(missing)):
    print(f'  missing: {name} at {addr:08X}')
stray = sorted(a for n, a in found_hooks if n == HANDSHAKE and a not in exits)
for addr in stray:
    print(f'  {HANDSHAKE} at {addr:08X} is not where a poll of the handshake exits on nonzero')
hard(len(words) == 1 and exits and not bad_window and not missing and not stray,
     f'render queue handshake {", ".join(f"{w:08X}" for w in sorted(words)) or "not found"}: {stores} recording starts, '
     f'{len(exits)} poll exits, {len(set(missing)) + len(stray)} without their fence')

# --- 6. recompiler log -------------------------------------------------------
if len(sys.argv) > 1:
    print(f'\n== log {sys.argv[1]}')
    LOG = [  # fmt::println strings in recompiler.cpp (FATAL: Recompiler::Fatal, patch 0011)
        ('switch case outside function (:623)', r'^FATAL: switch case at ([0-9A-F]+) jumps outside its function'),
        ('unrecognized instruction (:2598)', r'^FATAL: unrecognized instruction at 0x([0-9A-F]+): (\S+)'),
        ('unable to decode (:2588)', r'^FATAL: unable to decode instruction [0-9A-F]+ at ([0-9A-F]+)'),
        ('jump table without switch entry (:2593)', r'^FATAL: switch jump table at ([0-9A-F]+) with no switch table entry'),
        ('record form without CR write (:2611)', r'^FATAL: record form (\S+) at 0x([0-9A-F]+) writes no CR field'),
        ('RC bit without comparison (:2429)', r'^(\S+) at ([0-9A-F]+) has RC bit enabled but no comparison'),
        ('secondary entry outside functions (:2809)', r'^FATAL: secondary entry ([0-9A-F]+) is not inside any function'),
        ('fatal problem count (main.cpp:47)', r'^XenonRecomp: ([0-9]+) fatal problem\(s\)'),
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
