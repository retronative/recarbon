// An original 75% keyboard (site-only geometry, in key units): a dedicated
// F1–F12 row, a nav column, arrows and macOS modifiers. Ids are the key's
// settings.toml name where it has one (runtime/user/key_names.h); fn and
// the command keys have none, as ReCarbon can't bind them.

export interface KeyCap {
	id: string;
	label: string;
	name: string; // the accessible name
	x: number;
	y: number;
	w?: number;
	small?: boolean; // a word legend, set smaller
}

const row = (y: number, x0: number, ids: string[], label = (id: string) => id.toUpperCase()): KeyCap[] =>
	ids.map((id, i) => ({ id, label: label(id), name: label(id), x: x0 + i, y }));

const fKeys = (from: number, x0: number): KeyCap[] =>
	[0, 1, 2, 3].map((i) => ({ id: `f${from + i}`, label: `F${from + i}`, name: `F${from + i}`, x: x0 + i, y: 0, small: true }));

const named = (id: string, label: string, name: string, x: number, y: number, w = 1): KeyCap => ({ id, label, name, x, y, w, small: true });

const R1 = 1.25;
const R2 = 2.25;
const R3 = 3.25;
const R4 = 4.25;
const R5 = 5.25;

export const KEYBOARD_WIDTH = 16;
export const KEYBOARD_HEIGHT = 6.25;

export const KEYBOARD: KeyCap[] = [
	named('escape', 'esc', 'Escape', 0, 0),
	...fKeys(1, 1.25),
	...fKeys(5, 5.5),
	...fKeys(9, 9.75),
	named('delete', 'del ⌦', 'Delete (forward delete)', 15, 0),

	{ id: 'grave', label: '`', name: 'Grave accent', x: 0, y: R1 },
	...row(R1, 1, ['1', '2', '3', '4', '5', '6', '7', '8', '9', '0']),
	{ id: 'minus', label: '-', name: 'Minus', x: 11, y: R1 },
	{ id: 'equals', label: '=', name: 'Equals', x: 12, y: R1 },
	named('backspace', 'delete ⌫', 'Backspace (delete ⌫ on a Mac keyboard)', 13, R1, 2),
	named('home', 'home', 'Home', 15, R1),

	named('tab', 'tab', 'Tab', 0, R2, 1.5),
	...row(R2, 1.5, ['q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p']),
	{ id: 'left_bracket', label: '[', name: 'Left bracket', x: 11.5, y: R2 },
	{ id: 'right_bracket', label: ']', name: 'Right bracket', x: 12.5, y: R2 },
	{ id: 'backslash', label: '\\', name: 'Backslash', x: 13.5, y: R2, w: 1.5 },
	named('page_up', 'pg up', 'Page Up', 15, R2),

	named('caps_lock', 'caps', 'Caps Lock', 0, R3, 1.75),
	...row(R3, 1.75, ['a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l']),
	{ id: 'semicolon', label: ';', name: 'Semicolon', x: 10.75, y: R3 },
	{ id: 'apostrophe', label: "'", name: 'Apostrophe', x: 11.75, y: R3 },
	named('return', 'return', 'Return', 12.75, R3, 2.25),
	named('page_down', 'pg dn', 'Page Down', 15, R3),

	named('left_shift', 'shift ⇧', 'Left Shift', 0, R4, 2.25),
	...row(R4, 2.25, ['z', 'x', 'c', 'v', 'b', 'n', 'm']),
	{ id: 'comma', label: ',', name: 'Comma', x: 9.25, y: R4 },
	{ id: 'period', label: '.', name: 'Period', x: 10.25, y: R4 },
	{ id: 'slash', label: '/', name: 'Slash', x: 11.25, y: R4 },
	named('right_shift', 'shift', 'Right Shift', 12.25, R4, 1.75),
	{ id: 'up', label: '↑', name: 'Up arrow', x: 14, y: R4 },
	named('end', 'end', 'End', 15, R4),

	named('fn', 'fn', 'Fn (globe)', 0, R5),
	named('left_ctrl', 'control ⌃', 'Left Control', 1, R5),
	named('left_alt', 'option ⌥', 'Left Option (Alt)', 2, R5),
	named('left_gui', 'command ⌘', 'Left Command', 3, R5, 1.25),
	named('space', 'space', 'Space', 4.25, R5, 6.25),
	named('right_gui', '⌘', 'Right Command', 10.5, R5),
	named('right_alt', '⌥', 'Right Option (Alt)', 11.5, R5),
	{ id: 'left', label: '←', name: 'Left arrow', x: 13, y: R5 },
	{ id: 'down', label: '↓', name: 'Down arrow', x: 14, y: R5 },
	{ id: 'right', label: '→', name: 'Right arrow', x: 15, y: R5 },
];

// Mobile: the board in clusters, each zoomed to the screen's width (PLAN §6).
// Boxes are [x, y, w, h] in key units.
export const KEYBOARD_CLUSTERS = [
	{ id: 'left', label: 'Left hand', box: [0, 1.25, 7.25, 5] },
	{ id: 'right', label: 'Right hand', box: [6.75, 1.25, 7.25, 5] },
	{ id: 'arrows', label: 'Arrows & nav', box: [11.75, 0, 4.25, 6.25] },
	{ id: 'frow', label: 'F-row', box: [0, 0, 9.5, 1] },
] as const;
