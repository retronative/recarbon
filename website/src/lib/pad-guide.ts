// The controller guide's labels: for each button group ("slot"), its action
// in a mode, shortened for a label beside the drawing. Shared by the drawing
// (Controller.astro, which also places them) and the list under it on phones
// (PadGuideList.astro). Everything comes from the controls model.
import { CALLOUTS, STATUS, type Callout, type Mode } from './controls';

// ax, ay: where a slot's callout line meets its button, in the guide canvas.
export type Slot = { slot: string; els: string[]; ax: number; ay: number; chip: [string, string] };
export const LEFT: Slot[] = [
	{ slot: 'LT', els: ['LEFT_TRIGGER'], ax: 372, ay: 84, chip: ['LT', 'L2'] },
	{ slot: 'LB', els: ['LEFT_SHOULDER'], ax: 352, ay: 130, chip: ['LB', 'L1'] },
	{ slot: 'LS', els: ['LEFT_STICK'], ax: 379, ay: 208, chip: ['LS', 'LS'] },
	{ slot: 'DPAD', els: ['DPAD_UP', 'DPAD_DOWN', 'DPAD_LEFT', 'DPAD_RIGHT'], ax: 438, ay: 281, chip: ['✛', '✛'] },
];
export const RIGHT: Slot[] = [
	{ slot: 'RT', els: ['RIGHT_TRIGGER'], ax: 708, ay: 84, chip: ['RT', 'R2'] },
	{ slot: 'RB', els: ['RIGHT_SHOULDER'], ax: 728, ay: 130, chip: ['RB', 'R1'] },
	{ slot: 'RS', els: ['RIGHT_STICK'], ax: 637, ay: 284, chip: ['RS', 'RS'] },
];
export const FACE: Slot[] = [
	{ slot: 'SOUTH', els: ['SOUTH'], ax: 727, ay: 208, chip: ['A', '✕'] },
	{ slot: 'EAST', els: ['EAST'], ax: 727, ay: 208, chip: ['B', '○'] },
	{ slot: 'WEST', els: ['WEST'], ax: 727, ay: 208, chip: ['X', '□'] },
	{ slot: 'NORTH', els: ['NORTH'], ax: 727, ay: 208, chip: ['Y', '△'] },
];
export const TOP: Slot[] = [
	{ slot: 'BACK', els: ['BACK'], ax: 505, ay: 196, chip: ['⧉', '⧉'] },
	{ slot: 'START', els: ['START'], ax: 575, ay: 196, chip: ['≡', '≡'] },
];

// Shorter names beside the drawing for ReCarbon's own Settings keys, where
// the list's name is too long or the two buttons of one key differ.
const SETTINGS_SHORT: Record<string, Record<string, string>> = {
	open: { BACK: 'Hold Both', START: 'Settings' },
	pages: { LB: 'Previous Tab', RB: 'Next Tab' },
	navigate: { default: 'Navigate' },
	activate: { default: 'Set Key' },
};
const QUALIFIER: Record<string, string> = { lsx: '← →', lsy: '↑ ↓', lsc: 'press', rsc: 'press' };
const VERIFIED = new Set(['tested', 'automated', 'hand-verified']);

type Entry = { id: string; text: string; status: Callout['status']; qual: string };
function entries(mode: Mode, slot: Slot): Entry[] {
	return CALLOUTS[mode]
		.filter((c) => c.pad.some((p) => slot.els.includes(p)))
		.map((c) => {
			const key = c.id.slice(mode.length + 1);
			const short = mode === 'settings' ? (SETTINGS_SHORT[key]?.[slot.slot] ?? SETTINGS_SHORT[key]?.default) : undefined;
			return {
				id: c.id,
				text: short ?? (c.status === 'unverified' ? 'to verify' : c.action),
				status: c.status,
				qual: QUALIFIER[c.cluster ?? ''] ?? '',
			};
		});
}
// One label: the best-known entry as its line, the others as small notes.
export function label(mode: Mode, slot: Slot) {
	const all = entries(mode, slot);
	if (!all.length) return null;
	const main = all.find((e) => e.status !== 'unverified') ?? all[0];
	const subs = all.filter((e) => e !== main).map((e) => `${e.qual ? `${e.qual}: ` : ''}${e.text}`);
	return { main, subs, ids: all.map((e) => e.id).join(' ') };
}
export const chipW = (chip: [string, string]) => Math.max(26, Math.max(chip[0].length, chip[1].length) * 9 + 12);
// A small mark on game actions that are expected but not yet hand-tested.
// ReCarbon's own Settings keys aren't game actions, so they carry none.
export const markFor = (mode: Mode) => (s: Callout['status']) =>
	mode === 'settings' || VERIFIED.has(s) || s === 'unverified' ? '' : STATUS[s].icon;


// The slots in reading order, for a list: left side, top, right side.
export const READING_ORDER: Slot[] = [...LEFT, ...TOP, RIGHT[0], RIGHT[1], ...FACE, RIGHT[2]];
