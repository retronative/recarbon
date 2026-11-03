# ReCarbon. GPL-3.0-or-later (see COPYING).
#
# CFBundleVersion from the version alone, so the build number grows with the
# version and never depends on git history. ReCarbon releases are published by
# squash-merging a private branch: the old number, `git rev-list --count`, was
# 368 for v0.1.3 (its private branch) while the public main had 280 commits, and
# the build accepted for v0.1.4 counts 313 commits, so v0.1.4 would have looked
# older than v0.1.3 to anything that reads the number.
#
#   CFBundleVersion = MAJOR * 1000000 + MINOR * 10000 + PATCH * 100 + STAGE
#
# STAGE orders a version's pre-releases below the version itself, as Semantic
# Versioning does: alpha.N is N (1-32, a bare "alpha" 0), beta.N 33 + N (bare
# 33), rc.N 66 + N (bare 66), the release 99. MINOR and PATCH are at most 99.
# One integer: Apple allows up to three period-separated integers, and a
# dotted "0.1.4" would compare below 368 (0 < 368) where a whole number is
# compared. Every version from 0.1.0 up is above 368 (0.1.0 is 10099); v0.1.3,
# 368 when it shipped, would be 10399. A number at or below 368 (a version
# before 0.0.3) is refused: none ships.
#
# make_app.py writes it; check_app.py checks that an app's CFBundleVersion is
# this function of its CFBundleShortVersionString (tests/bundle_version_test.py).
import re

STAGES = {'alpha': 0, 'beta': 33, 'rc': 66}
RELEASE_STAGE = 99
MAX_STAGE_NUMBER = 32
# v0.1.3 shipped CFBundleVersion 368 (git rev-list --count of its commit).
LAST_COMMIT_COUNT_BUILD = 368

_VERSION = re.compile(r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-(alpha|beta|rc)(?:\.([1-9]\d*))?)?')


def bundle_version(version):
    """'0.1.4' -> '10499', '1.0.0-rc.2' -> '1000068'. ValueError for anything else."""
    m = _VERSION.fullmatch(str(version))
    if not m:
        raise ValueError(f'{version!r} is not MAJOR.MINOR.PATCH with an optional -alpha.N, -beta.N or -rc.N')
    major, minor, patch = (int(m.group(i)) for i in (1, 2, 3))
    if minor > 99 or patch > 99:
        raise ValueError(f'{version!r}: MINOR and PATCH must be at most 99')
    stage = RELEASE_STAGE
    if m.group(4):
        n = int(m.group(5) or 0)
        if n > MAX_STAGE_NUMBER:
            raise ValueError(f'{version!r}: a pre-release number must be at most {MAX_STAGE_NUMBER}')
        stage = STAGES[m.group(4)] + n
    number = major * 1000000 + minor * 10000 + patch * 100 + stage
    if number <= LAST_COMMIT_COUNT_BUILD:
        raise ValueError(f'{version!r}: {number} would sort below v0.1.3\'s build number {LAST_COMMIT_COUNT_BUILD}')
    return str(number)
