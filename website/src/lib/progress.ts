// The Progress page's data: src/data/progress.generated.json, written by
// scripts/gen-progress.mjs from ROADMAP.md and the recorded evidence through
// its allowlist. Checked again here, so a hand edit can't break the page's rules.
import data from '../data/progress.generated.json';
import { repo } from './repo';
import type { Status } from './controls';

export type ProgressState = 'completed' | 'accepted' | 'in-progress' | 'deferred' | 'unverified';

// `short`: the label on the milestone track, where space is tight.
export const PROGRESS_STATES: Record<ProgressState, { label: string; short: string; icon: string; meaning: string }> = {
	completed: { label: 'Completed', short: 'Completed', icon: '✓', meaning: 'Every criterion observed, as the roadmap records it.' },
	accepted: { label: 'Accepted · limits', short: 'Accepted', icon: '◐', meaning: 'Accepted with open limitations, none of them recorded as passed.' },
	'in-progress': { label: 'In progress', short: 'In progress', icon: '▸', meaning: 'Work and evidence under way; not passed yet.' },
	deferred: { label: 'Deferred', short: 'Deferred', icon: '‖', meaning: 'Set aside for later on purpose; not verified.' },
	unverified: { label: 'Not verified', short: 'Not verified', icon: '?', meaning: 'No evidence recorded yet.' },
};

export const MAX_ACHIEVEMENTS = 10;

interface Criterion {
	id: string;
	title: string;
	state: ProgressState;
}
export interface Milestone {
	id: string;
	title: string;
	state: ProgressState;
	date: string | null;
	summary: string;
	criteria: Criterion[];
}

const states = Object.keys(PROGRESS_STATES);
const check = (ok: boolean, what: string) => {
	if (!ok) throw new Error(`progress.generated.json: ${what}`);
};
for (const x of [...data.milestones, ...data.milestones.flatMap((m) => m.criteria), ...data.humanTests])
	check(states.includes(x.state), `"${x.title}": unknown state ${x.state}`);
for (const x of data.issues) check(x.state === 'open' || x.state === 'deferred', `issue "${x.title}": state ${x.state}`);
check(data.achievements.length <= MAX_ACHIEVEMENTS, `more than ${MAX_ACHIEVEMENTS} achievements`);
data.achievements.forEach((a, i) => check(i === 0 || a.date <= data.achievements[i - 1].date, `achievement ${i + 1} is out of order`));

export const MILESTONES = data.milestones as Milestone[];
export const CURRENT = MILESTONES.find((m) => m.id === data.currentMilestone)!;
export const UPDATED = data.updated;
export const FEATURES = data.features as { title: string; text: string; evidence: Status; date: string; milestone: string }[];
// An issue is open, or deferred where the roadmap defers it.
export const ISSUES = data.issues as { title: string; text: string; state: 'open' | 'deferred' }[];
export const HUMAN_TESTS = data.humanTests as { title: string; covers: string; state: ProgressState }[];
export const ACHIEVEMENTS = data.achievements.slice(0, MAX_ACHIEVEMENTS).map((a) => ({
	...a,
	commits: a.commits.map((c) => ({ ...c, url: repo ? `${repo.url}/commit/${c.sha}` : null })),
}));

// "2026-10-24" as "24 Oct 2026", fixed to UTC so the build machine's zone can't shift it.
export const formatDate = (iso: string) =>
	new Date(`${iso}T00:00:00Z`).toLocaleDateString('en-GB', { day: 'numeric', month: 'short', year: 'numeric', timeZone: 'UTC' });
