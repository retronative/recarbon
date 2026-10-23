// The Garage's model: ReCarbon's generated tables (controls.generated.json)
// joined with the hand-written actions overlay (controls.actions.json).
// Runs at build time; an overlay id the generated data doesn't know throws,
// which fails the build (PLAN §2).
import generated from '../data/controls.generated.json';
import overlay from '../data/controls.actions.json';
import { KEYBOARD } from '../data/keyboard-75';
import { site } from '../site.config';

export type Status = 'tested' | 'automated' | 'hand-verified' | 'source' | 'pending' | 'unverified' | 'not-mapped';
export type Mode = 'driving' | 'menu' | 'settings';
export type Device = 'keyboard' | 'controller';

export const STATUS: Record<Status, { label: string; icon: string; meaning: string }> = {
	tested: { label: 'Tested', icon: '✓', meaning: "Pinned by ReCarbon's unit tests." },
	automated: { label: 'Automated', icon: '⚙', meaning: 'Seen in scripted or virtual-controller runs, not yet by hand.' },
	'hand-verified': { label: 'Hand-verified', icon: '✋', meaning: 'Checked by a person in a hand test.' },
	source: { label: 'From source', icon: '‹›', meaning: "Read from ReCarbon's code or option text; not yet hand-tested." },
	pending: { label: 'Pending', icon: '◔', meaning: 'Expected from the option text; waiting for its hand test.' },
	unverified: { label: 'To verify', icon: '?', meaning: 'The control is bound, but what it does in the game is not known yet.' },
	'not-mapped': { label: 'Not mapped', icon: '∅', meaning: 'ReCarbon does not read this input (yet).' },
};

export const MODES: { id: Mode; label: string; gear: string; blurb: string }[] = [
	{ id: 'driving', label: 'Drive', gear: 'D', blurb: 'In a race or free-roam.' },
	{ id: 'menu', label: 'Menu', gear: 'M', blurb: "The game's own menus." },
	{ id: 'settings', label: 'Settings', gear: 'S', blurb: "ReCarbon's Settings overlay (F1)." },
];

type ControlId = (typeof generated.controls)[number]['id'];
type Control = (typeof generated.controls)[number];
interface Action {
	label?: string;
	status: Status;
	evidence: string;
}

const controls = new Map<string, Control>(generated.controls.map((c) => [c.id, c]));
const bindable = new Set<string>(generated.bindableKeys);
const keyIds = new Set(KEYBOARD.map((k) => k.id));

function check(ok: unknown, message: string): asserts ok {
	if (!ok) throw new Error(`controls: ${message}`);
}

for (const [id, modes] of Object.entries(overlay.actions)) {
	check(controls.has(id), `controls.actions.json names "${id}", which ReCarbon's key_map.h doesn't have`);
	for (const [mode, a] of Object.entries(modes as Record<string, Action>)) {
		check(mode === 'driving' || mode === 'menu', `${id}: unknown mode "${mode}"`);
		check(a.status in STATUS, `${id}.${mode}: unknown status "${a.status}"`);
		check(a.status !== 'unverified' || !a.label, `${id}.${mode}: an unverified action must not name a game action`);
	}
}
for (const c of generated.controls) for (const k of c.keys) if (k) check(keyIds.has(k), `default key "${k}" (${c.id}) is not on the keyboard drawing`);
for (const [id, s] of Object.entries(overlay.settings)) {
	const fromSource = (generated.ui as Record<string, unknown>)[id];
	check(fromSource || ('keys' in s && 'pad' in s), `controls.actions.json settings.${id} is in neither the generated ui block nor has its own keys`);
	check(s.status in STATUS, `settings.${id}: unknown status "${s.status}"`);
}

// --- Pad elements -----------------------------------------------------------

// The drawing's element ids are SDL3's gamepad names.
export const PAD_NAMES: Record<string, string> = {
	SOUTH: 'A (South)',
	EAST: 'B (East)',
	WEST: 'X (West)',
	NORTH: 'Y (North)',
	BACK: 'View (Back)',
	START: 'Menu (Start)',
	LEFT_SHOULDER: 'LB (left bumper)',
	RIGHT_SHOULDER: 'RB (right bumper)',
	LEFT_TRIGGER: 'LT (left trigger)',
	RIGHT_TRIGGER: 'RT (right trigger)',
	LEFT_STICK: 'Left stick',
	RIGHT_STICK: 'Right stick',
	DPAD_UP: 'D-pad up',
	DPAD_DOWN: 'D-pad down',
	DPAD_LEFT: 'D-pad left',
	DPAD_RIGHT: 'D-pad right',
	GUIDE: 'Guide (home) button',
	MISC1: 'Share / extra button',
	LEFT_PADDLE1: 'Left back paddle',
	RIGHT_PADDLE1: 'Right back paddle',
};

export const PAD_UNMAPPED = ['GUIDE', 'MISC1', 'LEFT_PADDLE1', 'RIGHT_PADDLE1'];

const axisElement: Record<string, string> = {
	LEFTX: 'LEFT_STICK',
	LEFTY: 'LEFT_STICK',
	RIGHTX: 'RIGHT_STICK',
	RIGHTY: 'RIGHT_STICK',
	LEFT_TRIGGER: 'LEFT_TRIGGER',
	RIGHT_TRIGGER: 'RIGHT_TRIGGER',
};

function padElementsOf(id: string): string[] {
	const button = generated.pad.buttons.find((b) => b.control === id);
	if (button) return [button.sdl];
	const axis = generated.pad.axes.find((a) => a.controls.includes(id));
	check(axis, `no pad input drives ${id}`);
	return [axisElement[axis.sdl]];
}

// --- Keys ---------------------------------------------------------------------

const keyCap = new Map(KEYBOARD.map((k) => [k.id, k]));
export function keyLabel(id: string): string {
	return keyCap.get(id)?.label ?? id;
}
export function keyName(id: string): string {
	return keyCap.get(id)?.name ?? id;
}
export function isBindable(id: string): boolean {
	return bindable.has(id);
}

// --- Callouts -----------------------------------------------------------------

// The Xbox controls as the Garage groups them: one callout plate each.
const CLUSTERS: { id: string; title: string; glyph: string; controls: ControlId[] }[] = [
	{ id: 'rt', title: 'Right trigger', glyph: 'RT', controls: ['RightTrigger'] },
	{ id: 'lt', title: 'Left trigger', glyph: 'LT', controls: ['LeftTrigger'] },
	{ id: 'lsx', title: 'Left stick ← →', glyph: 'LS', controls: ['LeftStickLeft', 'LeftStickRight'] },
	{ id: 'lsy', title: 'Left stick ↑ ↓', glyph: 'LS', controls: ['LeftStickUp', 'LeftStickDown'] },
	{ id: 'a', title: 'A (South)', glyph: 'A', controls: ['A'] },
	{ id: 'b', title: 'B (East)', glyph: 'B', controls: ['B'] },
	{ id: 'x', title: 'X (West)', glyph: 'X', controls: ['X'] },
	{ id: 'y', title: 'Y (North)', glyph: 'Y', controls: ['Y'] },
	{ id: 'lb', title: 'Left bumper', glyph: 'LB', controls: ['LB'] },
	{ id: 'rb', title: 'Right bumper', glyph: 'RB', controls: ['RB'] },
	{ id: 'dpad', title: 'D-pad', glyph: 'D', controls: ['DpadUp', 'DpadDown', 'DpadLeft', 'DpadRight'] },
	{ id: 'start', title: 'Menu (Start)', glyph: '≡', controls: ['Start'] },
	{ id: 'back', title: 'View (Back)', glyph: '⧉', controls: ['Back'] },
	{ id: 'rs', title: 'Right stick', glyph: 'RS', controls: ['RightStickUp', 'RightStickDown', 'RightStickLeft', 'RightStickRight'] },
	{ id: 'lsc', title: 'Left stick click', glyph: 'L3', controls: ['LeftStickClick'] },
	{ id: 'rsc', title: 'Right stick click', glyph: 'R3', controls: ['RightStickClick'] },
];
check(
	CLUSTERS.flatMap((c) => c.controls).sort().join() === [...controls.keys()].sort().join(),
	'the Garage clusters must cover every control in key_map.h exactly once',
);

export interface Line {
	control: string;
	label: string; // the control's own name, from settings.cpp
	action?: string;
	keys: (string | null)[];
	setting: string;
	setting2: string;
}

export interface Callout {
	id: string;
	mode: Mode;
	title: string;
	glyph: string;
	action: string; // what it does, or "Action to verify"
	status: Status;
	evidence: string;
	detail?: string;
	keys: string[]; // keyboard keys to light, by settings.toml name
	keyGroups: string[][]; // as shown: alternatives, each a key or a chord
	pad: string[]; // pad elements to light
	padChord?: boolean;
	lines: Line[]; // the controls behind it (empty in Settings mode)
}

function controlCallouts(mode: 'driving' | 'menu'): Callout[] {
	const out: Callout[] = [];
	for (const cluster of CLUSTERS) {
		const acts = cluster.controls.map((id) => (overlay.actions as Record<string, Record<string, Action>>)[id]?.[mode]);
		if (acts.every((a) => !a)) continue;
		const statuses = new Set(acts.map((a) => a?.status));
		check(statuses.size === 1, `${cluster.id} (${mode}): its controls must share one status`);
		const first = acts[0]!;
		if (first.status === 'unverified' && !site.showUnverifiedActions) continue;
		const lines: Line[] = cluster.controls.map((id, i) => {
			const c = controls.get(id)!;
			return { control: id, label: c.label, action: acts[i]?.label, keys: c.keys, setting: c.setting, setting2: c.setting2 };
		});
		const labels = [...new Set(lines.map((l) => l.action).filter((l): l is string => !!l))];
		const verbs = new Set(labels.map((l) => l.split(' ')[0]));
		// "Steer left" + "Steer right" read as "Steer left / right".
		const action =
			labels.length === 0
				? 'Action to verify'
				: verbs.size === 1 && labels.length > 1
					? `${[...verbs][0]} ${labels.map((l) => l.split(' ').slice(1).join(' ')).join(' / ')}`
					: labels.join(' / ');
		const keys = [...new Set(lines.flatMap((l) => l.keys).filter((k): k is string => !!k))];
		out.push({
			id: `${mode}-${cluster.id}`,
			mode,
			title: cluster.title,
			glyph: cluster.glyph,
			action,
			status: first.status,
			evidence: first.evidence,
			keys,
			keyGroups: keys.map((k) => [k]),
			pad: [...new Set(cluster.controls.flatMap(padElementsOf))],
			lines,
		});
	}
	return out;
}

type UiBinding = { keys: string[]; macKeys?: string[]; pad: string[]; evidence: string };

function settingsCallouts(): Callout[] {
	return Object.entries(overlay.settings).map(([id, s]) => {
		const ui = (generated.ui as unknown as Record<string, UiBinding | undefined>)[id];
		const keys: string[] = ui?.keys ?? (s as { keys: string[] }).keys;
		const pad: string[] = ui?.pad ?? (s as { pad: string[] }).pad;
		const macKeys = ui?.macKeys;
		return {
			id: `settings-${id}`,
			mode: 'settings' as const,
			title: s.label,
			glyph: id === 'open' ? 'F1' : '',
			action: s.label,
			status: s.status as Status,
			evidence: ui?.evidence ?? (s as { evidence: string }).evidence,
			detail: s.detail,
			keys: [...keys, ...(macKeys ?? [])],
			keyGroups: [...keys.map((k) => [k]), ...(macKeys ? [macKeys] : [])],
			pad,
			padChord: id === 'open',
			lines: [],
		};
	});
}

export const CALLOUTS: Record<Mode, Callout[]> = {
	driving: controlCallouts('driving'),
	menu: controlCallouts('menu'),
	settings: settingsCallouts(),
};

// Per key: its state in each mode, for the drawing's highlights.
export type KeyState = 'active' | 'unverified' | 'bound' | 'unused';
export function keyStates(id: string): Record<Mode, KeyState> {
	const bound = generated.controls.some((c) => c.keys.includes(id));
	const state = (mode: Mode): KeyState => {
		const hit = CALLOUTS[mode].find((c) => c.keys.includes(id));
		if (hit) return hit.status === 'unverified' ? 'unverified' : 'active';
		return bound && mode !== 'settings' ? 'bound' : 'unused';
	};
	return { driving: state('driving'), menu: state('menu'), settings: state('settings') };
}

export function padStates(element: string): Record<Mode, KeyState> {
	const state = (mode: Mode): KeyState => {
		const hit = CALLOUTS[mode].find((c) => c.pad.includes(element));
		if (hit) return hit.status === 'unverified' ? 'unverified' : 'active';
		return PAD_UNMAPPED.includes(element) ? 'unused' : 'bound';
	};
	return { driving: state('driving'), menu: state('menu'), settings: state('settings') };
}

export function controlsOfKey(id: string): { label: string; setting: string; slot: 1 | 2 }[] {
	return generated.controls
		.filter((c) => c.keys.includes(id))
		.map((c) => (c.keys[0] === id ? { label: c.label, setting: c.setting, slot: 1 as const } : { label: c.label, setting: c.setting2, slot: 2 as const }));
}

export const BINDING = overlay.binding as Record<string, { status: Status; evidence: string }>;
export const SOURCE = generated.source;
export const UI = generated.ui;
