#!/usr/bin/env node
// Generates the public Progress page's data (src/data/progress.generated.json)
// from ReCarbon's roadmap and its recorded evidence, through the allowlist below.
//
//   node scripts/gen-progress.mjs           write src/data/progress.generated.json
//   node scripts/gen-progress.mjs --check   fail if the committed JSON differs
//
// Sources, read-only:
//   - reference/ROADMAP.md: every milestone's title and state (the authority).
//     A state below that ROADMAP's heading doesn't say fails the run.
//   - progress.json: the maintainers' local evidence record, never published
//     and not in the repository. Only what the allowlist names is taken from
//     it: criterion states by id, and the recent achievements' dates, order
//     and commit IDs. Every public sentence is written here, not copied.
// Commit links are kept only for commits on origin/main, so the site never
// links a commit GitHub doesn't have (regenerate after a push to add new ones).
// Without progress.json (a public clone, CI), --check validates the committed
// JSON against ROADMAP and the deny list and passes, unless --strict is given.
import { execFileSync } from 'node:child_process';
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const SRC = resolve(ROOT, process.env.RECARBON_SRC ?? '..');
const OUT = join(ROOT, 'src/data/progress.generated.json');
const args = new Set(process.argv.slice(2));
const MAX_ACHIEVEMENTS = 10;

// The five public states.
const STATES = ['completed', 'accepted', 'in-progress', 'deferred', 'unverified'];
// What a ROADMAP heading's state must say for each milestone state.
const HEADING = {
	completed: /^(done|Passed)\b/,
	accepted: /^Accepted\b.*limitations/,
	'in-progress': /^(In progress|Open)\b/,
};
// progress.json's task states, as public states.
const TASK_STATE = { Completed: 'completed', 'In Progress': 'in-progress', Pending: 'unverified' };

// ---- The allowlist ----------------------------------------------------------

// Each milestone: its public summary, and the criteria shown (by progress.json
// task id, with a public title). `state` on a criterion overrides progress.json's
// only where ROADMAP records it so (deferred items).
const MILESTONES = [
	{ id: 'M0', state: 'completed', summary: 'The Xbox 360 executable is verified, decrypted from the player\'s own disc and surveyed for recompilation.' },
	{ id: 'M1', state: 'completed', summary: 'All 72,864 functions translate to C++ with zero recompiler errors, warnings or unrecognized instructions; 242 instruction tests pass.' },
	{ id: 'M2', state: 'completed', summary: 'The translated code compiles and links into a native arm64 app, with every import defined and no unresolved symbols.' },
	{ id: 'M3', state: 'completed', summary: 'The runtime boots the game through kernel, file system and GPU bring-up to its first frame swap, in 10 of 10 consecutive launches.' },
	{ id: 'M4', state: 'completed', summary: 'The EA logos and the movies render through MoltenVK on Metal, with no unknown texture formats or dropped draws.' },
	{
		id: 'M5',
		state: 'accepted',
		summary: 'Music, movies and keyboard menus pass. Accepted with its open limitations, none of them recorded as passed.',
		criteria: [
			{ id: 'M5.5', title: 'Front-end music plays 5 minutes with no underruns' },
			{ id: 'M5.6', title: 'The intro movie plays at the correct speed' },
			{ id: 'M5.10', title: 'Default keyboard and controller mappings pinned by unit tests' },
			{ id: 'M5.9', title: 'Keyboard rebinding in Settings > Controls works and persists' },
			{ id: 'M5.7', title: 'Menus with a physical controller (the keyboard passed by hand)', state: 'deferred' },
			{ id: 'M5.3', title: 'Smoother movie frame pacing', state: 'deferred' },
		],
	},
	{
		id: 'M6',
		state: 'in-progress',
		summary: 'Saves, race loading and free-roam stability each have evidence. Open until the written new-profile route is run.',
		criteria: [
			{ id: 'M6.3', title: 'A new profile is created, saved, and reloads intact after a restart' },
			{ id: 'M6.4', title: 'A race or free-roam loads from the front end to control of the car' },
			{ id: 'M6.5', title: 'No deadlock or crash in 10 minutes of free-roam' },
			{ id: 'M6.6', title: 'The written new-profile route, with logs and the save recorded' },
		],
	},
	{
		id: 'M7',
		state: 'in-progress',
		summary: 'The current milestone. AI races and a full career race finish; controls, physics and frame pacing are still to verify.',
		criteria: [
			{ id: 'M7.3', title: 'AI opponents spawn, race and finish' },
			{ id: 'M7.4', title: 'A full career race finishes and its progress survives a restart' },
			{ id: 'M7.1', title: 'Steering, throttle, brake, handbrake, nitrous and camera on the keyboard' },
			{ id: 'M7.2', title: 'Vehicle physics consistent with the Xbox 360 version' },
			{ id: 'M7.5', title: 'A stable 30 fps or better over 10 minutes of racing' },
			{ id: 'M7.6', title: 'Audio in sync and no world geometry missing' },
			{ id: 'M7.7', title: 'The written test route, repeated 3 times' },
		],
	},
	{
		id: 'M8',
		state: 'in-progress',
		summary: 'Input profiles are built and tested; the app bundle check passes. A clean install on a fresh account is still to do.',
		criteria: [
			{ id: 'M8.7', title: 'Keyboard presets: WASD (the default) and Arrows' },
			{ id: 'M8.8', title: 'Separate keyboard and controller profiles that persist' },
			{ id: 'M8.9', title: 'Custom mappings, controller rebinding, switching and reset to default' },
			{ id: 'M8.10', title: 'Existing key bindings migrated into a profile, never dropped' },
			{ id: 'M8.12', title: 'Reset to default matches the M5 baseline' },
			{ id: 'M8.6', title: 'The unit tests run in ctest' },
			{ id: 'M8.5', title: 'The app bundle check (check_app.py) passes' },
			{ id: 'M8.11', title: 'Both presets drive the game, by hand' },
			{ id: 'M8.4', title: 'A clean install from a disc image on a fresh macOS account' },
			{ id: 'M8.1', title: 'The installer parametrized for Carbon' },
		],
	},
];

// Verified features: what ROADMAP's evidence records, and how it was checked
// (the Garage's evidence vocabulary). `task`, when given, must be Completed.
const FEATURES = [
	{ title: 'Recompiled to native code', text: '72,864 functions from the Xbox 360 executable, translated with 0 recompiler errors and structural validation passing.', evidence: 'automated', date: '2026-10-22', milestone: 'M1', task: 'M1.5' },
	{ title: 'Boots and renders', text: 'The game boots to its own screens; the EA logos and the movies render through MoltenVK on Metal.', evidence: 'automated', date: '2026-10-22', milestone: 'M4', task: 'M4.5' },
	{ title: 'Front-end music', text: 'Over 5 minutes with no underruns or gaps, in 3 of 3 runs.', evidence: 'automated', date: '2026-10-23', milestone: 'M5', task: 'M5.5' },
	{ title: 'Keyboard menus, audio and lip-sync', text: 'Keyboard navigation, rebinding that survives a restart, audio quality and movie lip-sync, checked by a person (hand test 2).', evidence: 'hand-verified', date: '2026-10-23', milestone: 'M5', task: 'M5.9' },
	{ title: 'Career saves', text: 'A new career save (286,764 B) is written and reloads after a restart with its hash unchanged.', evidence: 'automated', date: '2026-10-23', milestone: 'M6', task: 'M6.3' },
	{ title: '15 minutes of free-roam', text: 'Free-roam after the prologue chase, 15 minutes in a row with no crash or hang (hand test 3).', evidence: 'hand-verified', date: '2026-10-23', milestone: 'M6', task: 'M6.5' },
	{ title: 'A career race, won', text: 'Zone Heat 1 finished in 1st place against AI opponents; the cash and progress were still there after a restart (hand test 4).', evidence: 'hand-verified', date: '2026-10-23', milestone: 'M7', task: 'M7.4' },
	{ title: 'Keyboard and controller profiles', text: 'WASD and Arrows presets, custom profiles, controller button rebinding and migration of existing bindings, in scripted runs and unit tests.', evidence: 'automated', date: '2026-10-24', milestone: 'M8', task: 'M8.9' },
	{ title: 'Game actions on a virtual controller', text: 'In a Quick Race: RB changes the camera, LB looks back, the right stick shifts gears, the Left Trigger reverses and Start pauses.', evidence: 'automated', date: '2026-10-24', milestone: 'M8' },
	{ title: '48 unit tests', text: 'Every ctest entry passes on a MacBook Air M5 and an M1 Mac.', evidence: 'automated', date: '2026-10-24', milestone: 'M8', task: 'M8.6' },
];

// Known gameplay issues: ROADMAP's "Known issues for the alpha" (M7) and M5's
// deferred limitation. Each is open: `open`, or `deferred` where ROADMAP defers it.
const ISSUES = [
	{ title: 'Occasional stalls of about 1 s', text: 'The presenter waits for a free drawable while holding the GPU queue lock. Why no drawable is free isn\'t known yet.', state: 'open' },
	{ title: 'GPU-idle stalls of 250–600 ms', text: 'No lock wait is involved. The cause is unknown.', state: 'open' },
	{ title: 'Movie frame pacing', text: 'Movies play at the right speed, but 20–28% of frames show for 1 or 3 refreshes instead of 2. Smoothing is deferred to polish.', state: 'deferred' },
	{ title: 'Physical controller not yet tested', text: 'The virtual-controller and hot-unplug tests pass, but they don\'t show that a physical pad works.', state: 'deferred' },
	{ title: 'Key labels on non-Latin keyboard layouts', text: 'Under a layout such as Myanmar, Settings > Controls draws letter keys as \'?\'. Binding such a key is refused safely; gameplay input is unaffected.', state: 'open' },
];

// Pending human tests: reference/HUMAN_TEST_PLAN.md's sessions, by what they cover.
const HUMAN_TESTS = [
	{ title: 'Session 1: a new profile, both presets', covers: 'The new-profile route to a race load (M6.6); WASD and Arrows driving and switching without a restart (M8.11).', state: 'unverified' },
	{ title: 'Session 2: restart and hand test 5', covers: 'The profile reloading intact (M6.6), and which keys give handbrake, nitrous, Speedbreaker, camera and look back.', state: 'unverified' },
	{ title: 'Sessions 3–5: the M7 route, 3 times', covers: 'Controls, physics, 10 minutes of frame times, audio and picture, on a race of at least 10 minutes (M7.1, M7.2, M7.5–M7.7).', state: 'unverified' },
	{ title: 'Clean install on a fresh macOS account', covers: 'Installing from a disc image on a new user account (M8.4).', state: 'unverified' },
	{ title: 'Physical controller', covers: 'Menus and driving on a real pad, once one is available. Never recorded as passed before then.', state: 'deferred' },
];

// Recent achievements: each of progress.json's newest entries must match one
// of these by its opening words; an unmatched one fails the run, so nothing
// reaches the site unreviewed. The text shown is `text`.
const ACHIEVEMENTS = [
	{ starts: 'Public Progress page', text: 'This Progress page: milestone states checked against the roadmap, verified features, known gameplay issues, pending hand tests and recent work.' },
	{ starts: 'Keyboard presets merged', text: 'Keyboard presets merged: WASD is now the built-in Default that a new player starts on, beside Arrows. A player on the former WASD preset keeps those keys as a custom profile.' },
	{ starts: 'Boot alias "Yes" hang rechecked', text: 'A reported hang after creating a new profile at boot was rechecked on two Macs and didn\'t reproduce: the profile is saved and the main menu follows.' },
	{ starts: 'Virtual-controller gameplay check', text: 'Change Camera, Look Back, gear shifts, Reverse and Pause seen working in a scripted Quick Race on a virtual controller. Nitrous, Handbrake and Speedbreaker are still to verify.' },
	{ starts: 'Public source launch', text: 'ReCarbon\'s source and this website went public, with every page checked live: no broken links and no accessibility violations found by axe-core.' },
	{ starts: 'Public-source readiness', text: 'A fresh clone from GitHub rebuilds byte-identical generated code, and the instruction tests and the website build pass. A timing test was made reliable under load.' },
	{ starts: 'Resolution audit re-measured', text: 'Rendering resolution re-measured on the MacBook Air M5: 1x, 1.5x, 2x and 3x frame costs recorded, and Auto picks 2x. Ultrawide and scaled 3D in gameplay are unverified.' },
	{ starts: '25 SpeedBreaker v0.1.8 unit tests ported', text: '25 unit tests ported from SpeedBreaker v0.1.8 and adapted to Carbon, each checked with planted bugs: ctest grows from 20 to 48 entries, all passing on both Macs.' },
	{ starts: 'install_check.sh stops every process', text: 'The install check now stops every process it starts, on any exit, verified in five scenarios.' },
	{ starts: 'M8 B2 data', text: 'Keyboard and controller mappings became plain data, with the WASD and Arrows presets; no key drives two controls in Arrows.' },
	{ starts: 'Atomic settings writer', text: 'One atomic writer now saves both the settings and the input profiles; saving settings is unchanged.' },
	{ starts: 'Profile store', text: 'Profiles are written atomically and validated, existing key bindings are migrated, and a damaged profile file is left untouched.' },
	{ starts: 'Settings > Controls: Keyboard Profile row', text: 'Settings > Controls gained a Keyboard Profile row: switching applies at once, and editing a built-in preset makes a custom copy.' },
];

// Never in the output: private paths, test homes, internal-only states and
// the release and legal topics this page leaves to its dedicated pages.
const DENY = [/\/Users\//, /~\//, /game\/m3/, /home-\w/, /not pushed/i, /\bW7\b/, /legal/i, /licen[cs]/i, /\bLGPL\b/, /\brisk/i, /WORKLOG/, /progress\.json/, /\.local\b/, /private/i, /source offer/i, /user decision/i];

// ---- Generation --------------------------------------------------------------

class SourceError extends Error {}
const fail = (message) => {
	throw new SourceError(message);
};
const expect = (ok, what) => ok || fail(what);

function read(rel) {
	const path = join(SRC, rel);
	if (!existsSync(path)) fail(`missing ${rel}`);
	return readFileSync(path, 'utf8');
}

// ROADMAP's headings: "## M0 — Title *(state text)*".
function roadmapMilestones(text) {
	const out = new Map();
	for (const m of text.matchAll(/^## (M\d+) — (.+?) \*\((.+?)\)\*\s*$/gm)) out.set(m[1], { title: m[2], status: m[3] });
	expect(out.size >= MILESTONES.length, `ROADMAP.md: ${out.size} milestone headings parsed`);
	return out;
}

function checkMilestones(milestones, roadmap) {
	expect(milestones.length === roadmap.size, `ROADMAP.md has ${roadmap.size} milestones, the allowlist ${milestones.length}`);
	for (const m of milestones) {
		const r = roadmap.get(m.id);
		expect(r, `${m.id} is not in ROADMAP.md`);
		expect(STATES.includes(m.state) && HEADING[m.state], `${m.id}: "${m.state}" is not a milestone state`);
		expect(HEADING[m.state].test(r.status), `${m.id}: ROADMAP says "${r.status}", not ${m.state}`);
		expect(m.title === r.title, `${m.id}: title "${m.title}" is not ROADMAP's "${r.title}"`);
	}
}

const git = (...a) => execFileSync('git', ['-C', SRC, ...a], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] }).trim();
function publicCommit(short) {
	try {
		const sha = git('rev-parse', '--verify', '--quiet', `${short}^{commit}`);
		git('merge-base', '--is-ancestor', sha, 'origin/main');
		return { sha, short: sha.slice(0, 7), time: Number(git('show', '-s', '--format=%ct', sha)) };
	} catch {
		return null;
	}
}

function generate(progress, roadmap) {
	const tasks = new Map(progress.milestones.flatMap((m) => m.tasks.map((t) => [t.id, t.status])));
	const milestones = MILESTONES.map((m) => {
		const r = roadmap.get(m.id);
		// The date it passed or was accepted; an open milestone's has none.
		const date = m.state === 'in-progress' ? null : (r.status.match(/\d{4}-\d{2}-\d{2}/)?.[0] ?? null);
		const criteria = (m.criteria ?? []).map((c) => {
			const status = tasks.get(c.id);
			expect(status in TASK_STATE, `${c.id}: progress.json state "${status}"`);
			// A deferred criterion is one ROADMAP keeps open: never deferred over a pass.
			if (c.state) expect(c.state === 'deferred' && status !== 'Completed', `${c.id}: ${c.state} over "${status}"`);
			return { id: c.id, title: c.title, state: c.state ?? TASK_STATE[status] };
		});
		return { id: m.id, title: r.title, state: m.state, date, summary: m.summary, criteria };
	});
	for (const f of FEATURES) if (f.task) expect(tasks.get(f.task) === 'Completed', `feature "${f.title}": ${f.task} is "${tasks.get(f.task)}"`);

	const entries = progress.recentAchievements.slice(0, MAX_ACHIEVEMENTS);
	const achievements = entries.map((a, i) => {
		const allowed = ACHIEVEMENTS.find((x) => a.text.startsWith(x.starts));
		expect(allowed, `achievement ${i + 1} (${a.date}) is not in the allowlist: "${a.text.slice(0, 60)}…"`);
		if (i > 0) expect(a.date <= entries[i - 1].date, `achievement ${i + 1}: ${a.date} after a ${entries[i - 1].date} one (not newest first)`);
		const commits = [...new Set(a.text.match(/\b[0-9a-f]{7,40}\b/g) ?? [])].map(publicCommit).filter(Boolean);
		return { date: a.date, text: allowed.text, commits };
	});
	// Same-day entries: each one's newest public commit is no newer than the one's before it.
	const newest = (a) => Math.max(0, ...a.commits.map((c) => c.time));
	let last = Infinity;
	for (const a of achievements) {
		if (!newest(a)) continue;
		expect(newest(a) <= last, `"${a.text.slice(0, 40)}…" has a newer commit than the achievement listed before it`);
		last = newest(a);
	}
	return {
		updated: progress.updated,
		currentMilestone: progress.currentMilestone,
		milestones,
		features: FEATURES.map(({ task, ...f }) => f),
		issues: ISSUES,
		humanTests: HUMAN_TESTS,
		achievements: achievements.map((a) => ({ date: a.date, text: a.text, commits: a.commits.map(({ sha, short }) => ({ sha, short })) })),
	};
}

// What the committed JSON must meet with or without progress.json.
function validate(data, roadmap) {
	checkMilestones(data.milestones, roadmap);
	expect(data.milestones.some((m) => m.id === data.currentMilestone && m.state === 'in-progress'), `current milestone ${data.currentMilestone} is not in progress`);
	for (const x of [...data.milestones.flatMap((m) => m.criteria), ...data.humanTests]) expect(STATES.includes(x.state), `"${x.title}": unknown state ${x.state}`);
	for (const x of data.issues) expect(['open', 'deferred'].includes(x.state), `issue "${x.title}": state ${x.state}`);
	expect(data.achievements.length <= MAX_ACHIEVEMENTS, `${data.achievements.length} achievements, more than ${MAX_ACHIEVEMENTS}`);
	for (let i = 1; i < data.achievements.length; i++) expect(data.achievements[i].date <= data.achievements[i - 1].date, `achievement ${i + 1} is out of order`);
	const text = JSON.stringify(data);
	for (const d of DENY) expect(!d.test(text), `the output matches the deny list (${d})`);
}

function main() {
	const roadmap = roadmapMilestones(read('reference/ROADMAP.md'));
	const progressPath = join(SRC, 'progress.json');
	if (!existsSync(progressPath)) {
		if (!args.has('--check') || args.has('--strict')) fail('missing progress.json (the local evidence record)');
		validate(JSON.parse(readFileSync(OUT, 'utf8')), roadmap);
		console.warn('gen-progress: no progress.json here; the committed data was checked against ROADMAP.md only');
		return;
	}
	const data = generate(JSON.parse(readFileSync(progressPath, 'utf8')), roadmap);
	validate(data, roadmap);
	const text = JSON.stringify(data, null, '\t') + '\n';
	if (args.has('--check')) {
		const committed = existsSync(OUT) ? readFileSync(OUT, 'utf8') : '';
		if (committed !== text) fail('src/data/progress.generated.json is out of date: run node scripts/gen-progress.mjs');
		console.log('gen-progress: in sync');
		return;
	}
	writeFileSync(OUT, text);
	console.log(`gen-progress: ${data.milestones.length} milestones, ${data.achievements.length} achievements (${data.achievements.reduce((n, a) => n + a.commits.length, 0)} commit links) -> ${OUT}`);
}

try {
	main();
} catch (e) {
	if (!(e instanceof SourceError)) throw e;
	console.error(`gen-progress: ${e.message}`);
	process.exit(1);
}
