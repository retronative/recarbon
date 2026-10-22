"""Image layout of the surveyed XEX, read from xexsurvey's report.

    tools/survey/xexsurvey game/files/default.xex game/survey > game/survey/xexsurvey.txt

Replaces the hard-coded MW ranges SpeedBreaker's survey scripts carried
(ReCarbon change), so the scripts follow whichever executable was surveyed.
"""
import re

REPORT = 'game/survey/xexsurvey.txt'
_text = open(REPORT, encoding='latin1').read()

BASE = int(re.search(r'^base 0x([0-9A-F]+)', _text, re.M).group(1), 16)
SECTIONS = [(name, int(start, 16), int(end, 16), kind) for name, start, end, kind in re.findall(
    r'^\| (\S+) \| 0x([0-9A-F]+) \| 0x([0-9A-F]+) \| 0x[0-9A-F]+ \| (code|data) \|$', _text, re.M)]

def section(name):
    return next((s, e) for n, s, e, _ in SECTIONS if n == name)

_code = [(s, e) for _, s, e, k in SECTIONS if k == 'code']
TEXT = (min(s for s, _ in _code), max(e for _, e in _code))   # every code section, gaps are padding
PDATA = section('.pdata')
DATA = [section('.rdata'), section('.data')]
