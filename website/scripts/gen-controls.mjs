#!/usr/bin/env node
// Generates the site's controls and settings data from ReCarbon's source, so
// nothing on the site is typed in by hand (docs/PLAN.md §2).
//
//   node scripts/gen-controls.mjs           write src/data/*.generated.json
//   node scripts/gen-controls.mjs --check   fail if the committed JSON differs
//
// The source tree is read-only. Point RECARBON_SRC at a checkout of ReCarbon;
// inside the ReCarbon repository (website/) it is the parent folder, and
// otherwise the sibling worker checkout this site was built against.
// With --check and no source tree present, it warns and passes, unless
// --strict is given (a site-only clone can't see ReCarbon's private source).
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { basename, dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const DEFAULT_SRC = '../recarbon-worker/src/b5993d05a948dd2796bf8b8fe540f003598836ad';
// Inside the ReCarbon repository (website/), the source is the parent folder.
const IN_REPO = existsSync(resolve(ROOT, '../runtime/hid/key_map.h')) ? '..' : null;
const SRC = resolve(ROOT, process.env.RECARBON_SRC ?? IN_REPO ?? DEFAULT_SRC);
const OUT = {
	controls: join(ROOT, 'src/data/controls.generated.json'),
	settings: join(ROOT, 'src/data/settings.generated.json'),
};
const args = new Set(process.argv.slice(2));

class SourceError extends Error {}

function fail(message) {
	throw new SourceError(message);
}

function read(rel) {
	const path = join(SRC, rel);
	if (!existsSync(path)) fail(`missing ${rel}`);
	return readFileSync(path, 'utf8');
}

// The text between `start` and the first `end` after it.
function block(text, start, end, what) {
	const i = text.indexOf(start);
	if (i < 0) fail(`${what}: "${start}" not found`);
	const j = text.indexOf(end, i + start.length);
	if (j < 0) fail(`${what}: no end after "${start}"`);
	return text.slice(i + start.length, j);
}

function expect(ok, what) {
	if (!ok) fail(what);
}

// Adjacent C string literals ("a" "b") as one string.
function cString(expr) {
	const parts = [...expr.matchAll(/"((?:[^"\\]|\\.)*)"/g)].map((m) => m[1].replace(/\\(.)/g, '$1'));
	return parts.join('');
}

function parseKeyNames(keyNamesH) {
	const body = block(keyNamesH, 'constexpr Key kKeys[] = {', '};', 'key_names.h kKeys');
	const keys = [...body.matchAll(/\{\s*(\d+),\s*"(\w+)"\s*\}/g)].map((m) => ({ code: Number(m[1]), token: m[2] }));
	expect(keys.length > 100, `key_names.h: only ${keys.length} keys parsed`);
	return keys;
}

function parseKeyOptions(settingsCpp) {
	const first = new Map();
	for (const m of settingsCpp.matchAll(/KeyOption\(Id::(\w+), "(\w+)", "([^"]+)", "([^"]*)", "(\w+)"\)/g))
		first.set(m[1], { id: m[1], setting: m[2], label: m[3], description: m[4], token: m[5] });
	const second = new Map();
	for (const m of settingsCpp.matchAll(/KeyOption2\(Id::(\w+), "(\w+)", "([^"]+)", "(\w+)"\)/g))
		second.set(m[1], { id: m[1], setting: m[2], label: m[3], token: m[4] });
	return { first, second };
}

function parseButtons(keyMapH) {
	const body = block(keyMapH, 'namespace button', '}', 'key_map.h button');
	const bits = {};
	for (const m of body.matchAll(/(\w+) = 0x([0-9A-Fa-f]+)/g)) bits[m[1]] = parseInt(m[2], 16);
	expect(Object.keys(bits).length === 14, `key_map.h: ${Object.keys(bits).length} button bits, expected 14`);
	return bits;
}

function parseKeyEntries(keyMapH) {
	const controlsBody = block(keyMapH, 'enum class Control : uint8_t', '};', 'key_map.h Control');
	const controlOrder = controlsBody
		.replace(/[{}\s]/g, '')
		.split(',')
		.filter((c) => c && c !== 'Count');
	const body = block(keyMapH, 'constexpr Entry kEntries[kControls] = {', '};', 'key_map.h kEntries');
	const re =
		/\{\s*Control::(\w+),\s*settings::Id::(\w+),\s*settings::Id::(\w+),\s*Role::(\w+)(?:,\s*(button::\w+|0))?(?:,\s*Axis::(\w+))?\s*\}/g;
	const entries = [...body.matchAll(re)].map((m) => ({
		control: m[1],
		id: m[2],
		id2: m[3],
		role: m[4],
		button: m[5] && m[5] !== '0' ? m[5].replace('button::', '') : null,
		axis: m[6] ?? (m[4].startsWith('Axis') ? 'LX' : null),
	}));
	expect(entries.length === controlOrder.length, `key_map.h: ${entries.length} entries for ${controlOrder.length} controls`);
	entries.forEach((e, i) => expect(e.control === controlOrder[i], `key_map.h: entry ${i} is ${e.control}, expected ${controlOrder[i]}`));
	return entries;
}

function parsePadMap(padMapH) {
	const body = block(padMapH, 'inline constexpr Entry kDefault[] = {', '};', 'pad_map.h kDefault');
	return [...body.matchAll(/\{\s*SDL_GAMEPAD_BUTTON_(\w+),\s*button::(\w+)\s*\}/g)].map((m) => ({ sdl: m[1], button: m[2] }));
}

function parsePadBaseline(padMapTest) {
	const body = block(padMapTest, 'constexpr Expected kBaseline[] = {', '};', 'pad_map_test.cpp kBaseline');
	return [...body.matchAll(/\{\s*SDL_GAMEPAD_BUTTON_(\w+),\s*0x([0-9A-Fa-f]+),\s*"([^"]+)"\s*\}/g)].map((m) => ({
		sdl: m[1],
		bit: parseInt(m[2], 16),
		what: m[3],
	}));
}

// The built-in keyboard presets (hid/profiles.h, ROADMAP M8): Default is the
// settings' defaults; WASD and Arrows replace some of its rows, as
// BuiltinMapping() does. Checked against profiles_test.cpp's full tables and
// its rules, so the C++ test stays the authority.
function parsePresets(profilesH, profilesTest, controls, bindable) {
	const names = block(profilesH, 'kBuiltinNames[] = {', '};', 'profiles.h kBuiltinNames');
	const presetNames = [...names.matchAll(/"(\w+)"/g)].map((m) => m[1]);
	expect(presetNames.join() === 'Default,WASD,Arrows', `profiles.h: built-in profiles are ${presetNames.join(', ')}`);
	const rows = (name) =>
		[...block(profilesH, `inline constexpr Row ${name}[] = {`, '};', `profiles.h ${name}`).matchAll(/\{\s*C::(\w+),\s*"(\w+)",\s*"(\w+)"\s*\}/g)].map(
			(m) => ({ control: m[1], keys: [m[2], m[3]] }),
		);
	const defaults = Object.fromEntries(controls.map((c) => [c.id, c.keys.map((k) => k ?? 'none')]));
	const apply = (presetRows) => {
		const m = structuredClone(defaults);
		for (const r of presetRows) {
			expect(r.control in m, `profiles.h: unknown control ${r.control}`);
			m[r.control] = r.keys;
		}
		return m;
	};
	const mappings = { Default: defaults, WASD: apply(rows('kWasdRows')), Arrows: apply(rows('kArrowsRows')) };

	const order = controls.map((c) => c.id);
	const table = (name) => {
		const t = [...block(profilesTest, `const Keys ${name}[] = {`, '};', `profiles_test.cpp ${name}`).matchAll(/\{\s*"(\w+)",\s*"(\w+)"\s*\}/g)];
		expect(t.length === order.length, `profiles_test.cpp ${name}: ${t.length} rows for ${order.length} controls`);
		return Object.fromEntries(t.map((m, i) => [order[i], [m[1], m[2]]]));
	};
	const tests = { Default: table('kDefaultTable'), WASD: table('kWasdTable'), Arrows: table('kArrowsTable') };
	for (const [name, m] of Object.entries(mappings))
		for (const id of order) {
			expect(m[id].join() === tests[name][id].join(), `${name} ${id}: profiles.h gives ${m[id]}, profiles_test.cpp ${tests[name][id]}`);
			for (const k of m[id]) expect(k === 'none' || bindable.has(k), `${name} ${id}: "${k}" is not in key_names.h`);
		}
	// The test's rules: no key on two controls in WASD and Arrows (22 keys
	// each); Default shares Left between steering and the D-pad by design.
	const repeated = (m) => {
		const keys = Object.values(m).flat().filter((k) => k !== 'none');
		return { bound: keys.length, repeats: [...new Set(keys.filter((k, i) => keys.indexOf(k) !== i))] };
	};
	for (const name of ['WASD', 'Arrows']) {
		const r = repeated(mappings[name]);
		expect(r.repeats.length === 0 && r.bound === 22, `${name}: ${r.bound} keys bound, repeated: ${r.repeats.join(', ') || 'none'}`);
	}
	expect(/RepeatedKey\(defaults\) == Code\("left"\)/.test(profilesTest), 'profiles_test.cpp: the Default sharing rule changed');

	return presetNames.map((name) => ({
		id: name.toLowerCase(),
		name,
		keys: Object.fromEntries(order.map((id) => [id, mappings[name][id].map((k) => (k === 'none' ? null : k))])),
		shared: repeated(mappings[name]).repeats,
	}));
}

// ReCarbon's own UI: what opens Settings, turns its pages, clears and resets a
// Controls row, and closes it. Each pattern must still be in the source, or
// the generator fails and the site's Settings mode needs a look.
function parseUi(uiCpp, menuCpp, settingsH, internalH) {
	expect(/toggle = e\.key\.key == SDLK_F1;/.test(uiCpp), 'ui.cpp: F1 no longer toggles Settings');
	expect(/SDLK_COMMA && \(e\.key\.mod & SDL_KMOD_GUI\)/.test(uiCpp), 'ui.cpp: Cmd+, no longer toggles Settings');
	expect(
		/SDL_GAMEPAD_BUTTON_BACK\)\s*chord\.back = down;[\s\S]*SDL_GAMEPAD_BUTTON_START\)\s*chord\.start = down;/.test(uiCpp),
		'ui.cpp: the Back + Start chord changed',
	);
	const pageDelta = block(menuCpp, 'int PageDelta()', 'return delta;', 'settings_menu.cpp PageDelta');
	expect(/GamepadL1, false\) \|\| ImGui::IsKeyPressed\(ImGuiKey_Q/.test(pageDelta), 'PageDelta: LB / Q changed');
	expect(/GamepadR1, false\) \|\| ImGui::IsKeyPressed\(ImGuiKey_E/.test(pageDelta), 'PageDelta: RB / E changed');
	expect(/GamepadFaceUp, false\) \|\| ImGui::IsKeyPressed\(ImGuiKey_Backspace/.test(menuCpp), 'Controls page: Y / Backspace no longer clears');
	expect(/GamepadFaceLeft, false\) \|\| ImGui::IsKeyPressed\(ImGuiKey_Delete/.test(menuCpp), 'Controls page: X / Delete no longer resets');
	expect(/GamepadFaceRight, false\) \|\| ImGui::IsKeyPressed\(ImGuiKey_Escape/.test(menuCpp), 'Settings: B / Escape no longer closes');
	expect(/ImGuiConfigFlags_NavEnableKeyboard \| ImGuiConfigFlags_NavEnableGamepad/.test(uiCpp), 'ui.cpp: keyboard/gamepad navigation flags changed');

	const legend = menuCpp.match(/const char\* legend = s_category == Category::Controls \? ("[^"]*")\s*: ("[^"]*");/);
	expect(legend, 'settings_menu.cpp: footer legend not found');
	const tabs = block(settingsH, 'enum class Category : uint8_t {', '}', 'settings.h Category')
		.split(',')
		.map((s) => s.trim())
		.filter((s) => s && s !== 'Count');
	const hint = internalH.match(/kSaveBugReportHint = "([^"]+Cmd[^"]+)"/);
	expect(hint, 'report/internal.h: Save Bug Report hint (macOS) not found');

	return {
		open: {
			keys: ['f1'],
			macKeys: ['left_gui', 'comma'],
			pad: ['BACK', 'START'],
			note: 'F1 is the brightness key on Mac keyboards: press Fn+F1, or Cmd+,.',
			evidence: 'runtime/ui/ui.cpp (SDLK_F1, SDLK_COMMA with SDL_KMOD_GUI; the Back + Start chord)',
		},
		pages: { keys: ['q', 'e'], pad: ['LEFT_SHOULDER', 'RIGHT_SHOULDER'], evidence: 'runtime/ui/settings_menu.cpp PageDelta()' },
		clearSlot: { keys: ['backspace'], pad: ['NORTH'], evidence: 'runtime/ui/settings_menu.cpp (Controls row)' },
		resetControl: { keys: ['delete'], pad: ['WEST'], evidence: 'runtime/ui/settings_menu.cpp (Controls row)' },
		close: { keys: ['escape'], pad: ['EAST'], evidence: 'runtime/ui/settings_menu.cpp' },
		navigate: {
			keys: ['up', 'down', 'left', 'right'],
			pad: ['DPAD_UP', 'DPAD_DOWN', 'DPAD_LEFT', 'DPAD_RIGHT', 'LEFT_STICK'],
			evidence: 'runtime/ui/ui.cpp ImGuiConfigFlags_NavEnableKeyboard | NavEnableGamepad',
		},
		legend: { controls: cString(legend[1]), other: cString(legend[2]) },
		tabs,
		bugReportHint: hint[1],
	};
}

// The Settings menu's option table (settings.cpp kOptions), as macOS builds
// it. Platform constants are resolved for macOS here, explicitly: a new one
// fails the generator until it is added.
const MACOS_CONSTANTS = { kFullscreenHidden: false, kPresentScaleHidden: true, kFsr1Offered: true };

function parseEnumEntries(settingsCpp) {
	const tables = {};
	for (const m of settingsCpp.matchAll(/constexpr EnumEntry (k\w+Entries)\[\] = \{([\s\S]*?)\n\s*\};/g)) {
		const body = m[2].replace(/#if !\(defined\(__APPLE__\) && TARGET_OS_IOS\)\n|#endif\n/g, '');
		tables[m[1]] = [...body.matchAll(/\{\s*([^,]+),\s*"([^"]+)",\s*"([^"]+)"\s*\}/g)].map((e) => ({
			value: e[1].trim(),
			token: e[2],
			label: e[3],
		}));
	}
	return tables;
}

function resolveConstant(expr, what) {
	const name = expr.trim();
	expect(name in MACOS_CONSTANTS, `settings.cpp: ${what} uses ${name}; add it to MACOS_CONSTANTS`);
	return MACOS_CONSTANTS[name];
}

function parseSettings(settingsCpp) {
	const enums = parseEnumEntries(settingsCpp);
	const body = block(settingsCpp, 'constexpr Option kOptions[] = {', '\n        };', 'settings.cpp kOptions');
	const chunks = body.split(/\{ \.id = Id::/).slice(1);
	const options = [];
	for (const chunk of chunks) {
		const id = chunk.match(/^(\w+)/)[1];
		const field = (name) => chunk.match(new RegExp(`\\.${name} = ([^,]+?)(?:,|\\s*\\}|$)`, 'm'))?.[1]?.trim();
		const category = field('category').replace('Category::', '');
		if (category === 'Controls') continue;
		const type = field('type').replace('Type::', '');
		const hiddenExpr = field('hidden');
		const hidden = hiddenExpr === undefined ? false : hiddenExpr === 'true' ? true : resolveConstant(hiddenExpr, id);
		let description;
		const descMatch = chunk.match(/\.description = ([\s\S]*?),\n\s*\.env/);
		expect(descMatch, `settings.cpp: ${id} description not parsed`);
		const descExpr = descMatch[1].trim();
		const ternary = descExpr.match(/^(\w+)\s*\?\s*([\s\S]*?)\s*:\s*("[\s\S]*")$/);
		if (ternary) description = cString(resolveConstant(ternary[1], id) ? ternary[2] : ternary[3]);
		else description = cString(descExpr);
		const defExpr = field('defaultValue').replace(/^double\((.*)\)$/, '$1');
		const entriesName = field('entries');
		const option = {
			id,
			category,
			type,
			apply: field('apply').replace('Apply::', ''),
			table: cString(field('table')),
			key: cString(field('key')),
			label: cString(field('label')),
			description,
			env: field('env') && field('env') !== 'nullptr' ? cString(field('env')) : null,
		};
		if (type === 'Bool') option.default = defExpr === '1' ? 'On' : 'Off';
		else if (type === 'Enum') {
			const entries = enums[entriesName];
			expect(entries, `settings.cpp: ${id} entries ${entriesName} not parsed`);
			option.values = entries.map((e) => ({ token: e.token, label: e.label }));
			const def = entries.find((e) => e.value === defExpr || e.value === `int32_t(${defExpr})`);
			expect(def, `settings.cpp: ${id} default ${defExpr} is not one of ${entriesName}`);
			option.default = def.label;
			option.defaultToken = def.token;
		} else {
			const num = (s) => (s === undefined ? undefined : Number(s));
			option.default = num(defExpr);
			option.min = num(field('min'));
			option.max = num(field('max'));
			option.step = num(field('step'));
			const unit = field('unit');
			if (unit) option.unit = cString(unit);
			expect(Number.isFinite(option.default), `settings.cpp: ${id} default ${defExpr} is not a number`);
		}
		if (!hidden) options.push(option);
	}
	expect(options.length > 15, `settings.cpp: only ${options.length} options parsed`);
	return options;
}

function generate() {
	const settingsCpp = read('runtime/user/settings.cpp');
	const settingsH = read('runtime/user/settings.h');
	const keyMapH = read('runtime/hid/key_map.h');
	const padMapH = read('runtime/hid/pad_map.h');
	const inputCpp = read('runtime/hid/input.cpp');
	const keyNamesH = read('runtime/user/key_names.h');
	const uiCpp = read('runtime/ui/ui.cpp');
	const menuCpp = read('runtime/ui/settings_menu.cpp');
	const internalH = read('runtime/report/internal.h');
	const padMapTest = read('tests/pad_map_test.cpp');
	const profilesH = read('runtime/hid/profiles.h');
	const profilesTest = read('tests/profiles_test.cpp');

	const keyNames = parseKeyNames(keyNamesH);
	const bindable = new Set(keyNames.map((k) => k.token));
	const { first, second } = parseKeyOptions(settingsCpp);
	const bits = parseButtons(keyMapH);
	const entries = parseKeyEntries(keyMapH);

	const controls = entries.map((e) => {
		const a = first.get(e.id);
		const b = second.get(e.id2);
		expect(a, `settings.cpp: no KeyOption for ${e.id}`);
		expect(b, `settings.cpp: no KeyOption2 for ${e.id2}`);
		expect(b.setting === `${a.setting}_2`, `settings.cpp: ${e.id2} is "${b.setting}", expected "${a.setting}_2"`);
		for (const t of [a.token, b.token]) expect(t === 'none' || bindable.has(t), `settings.cpp: ${e.id} default "${t}" is not in key_names.h`);
		return {
			id: e.control,
			label: a.label,
			description: a.description,
			setting: a.setting,
			setting2: b.setting,
			keys: [a.token === 'none' ? null : a.token, b.token === 'none' ? null : b.token],
			role: e.role,
			...(e.button ? { button: e.button, bit: bits[e.button] } : {}),
			...(e.axis ? { axis: e.axis, direction: e.role === 'AxisPos' ? 1 : -1 } : {}),
		};
	});

	// The controller: the 14 buttons of pad_map.h, checked against the test's
	// own baseline so the C++ test stays the authority.
	const pad = parsePadMap(padMapH).map((p) => {
		const bit = bits[p.button];
		expect(bit !== undefined, `pad_map.h: unknown button ${p.button}`);
		const control = controls.find((c) => c.bit === bit);
		expect(control, `pad_map.h: no keyboard control has bit ${p.button}`);
		return { sdl: p.sdl, control: control.id };
	});
	const baseline = parsePadBaseline(padMapTest);
	expect(baseline.length === 14 && pad.length === 14, `pad map ${pad.length} / baseline ${baseline.length}, expected 14 each`);
	for (const want of baseline) {
		const got = pad.find((p) => p.sdl === want.sdl);
		expect(got, `pad_map.h lacks the baseline's ${want.sdl}`);
		const control = controls.find((c) => c.id === got.control);
		expect(control.bit === want.bit, `pad_map.h ${want.sdl} differs from pad_map_test (${want.what})`);
	}
	for (const axis of ['LEFTX', 'LEFTY', 'RIGHTX', 'RIGHTY', 'LEFT_TRIGGER', 'RIGHT_TRIGGER'])
		expect(inputCpp.includes(`SDL_GAMEPAD_AXIS_${axis}`), `input.cpp no longer reads SDL_GAMEPAD_AXIS_${axis}`);
	const padAxes = [
		{ sdl: 'LEFTX', controls: ['LeftStickLeft', 'LeftStickRight'] },
		{ sdl: 'LEFTY', controls: ['LeftStickUp', 'LeftStickDown'] },
		{ sdl: 'RIGHTX', controls: ['RightStickLeft', 'RightStickRight'] },
		{ sdl: 'RIGHTY', controls: ['RightStickUp', 'RightStickDown'] },
		{ sdl: 'LEFT_TRIGGER', controls: ['LeftTrigger'] },
		{ sdl: 'RIGHT_TRIGGER', controls: ['RightTrigger'] },
	];
	const deadzones = parseSettings(settingsCpp).filter((o) => o.table === 'input' && o.key.endsWith('deadzone'));

	const commit = basename(SRC).match(/^[0-9a-f]{40}$/) ? basename(SRC) : null;
	const sources = [
		'runtime/user/settings.cpp',
		'runtime/hid/key_map.h',
		'runtime/hid/pad_map.h',
		'runtime/hid/input.cpp',
		'runtime/user/key_names.h',
		'runtime/ui/ui.cpp',
		'runtime/ui/settings_menu.cpp',
		'tests/pad_map_test.cpp',
		'runtime/hid/profiles.h',
		'tests/profiles_test.cpp',
	];
	return {
		controls: {
			$comment: 'Generated by scripts/gen-controls.mjs from ReCarbon source. Do not edit.',
			source: { commit, files: sources },
			controls,
			presets: parsePresets(profilesH, profilesTest, controls, bindable),
			pad: { buttons: pad, axes: padAxes, deadzones: deadzones.map(({ key, label, default: d }) => ({ key, label, default: d })) },
			ui: parseUi(uiCpp, menuCpp, settingsH, internalH),
			bindableKeys: keyNames.map((k) => k.token),
		},
		settings: {
			$comment: 'Generated by scripts/gen-controls.mjs from runtime/user/settings.cpp (macOS build). Do not edit.',
			source: { commit, files: ['runtime/user/settings.cpp', 'runtime/user/settings.h'] },
			options: parseSettings(settingsCpp),
		},
	};
}

function serialize(data) {
	return JSON.stringify(data, null, '\t') + '\n';
}

function main() {
	if (!existsSync(SRC)) {
		const msg = `ReCarbon source not found at ${SRC} (set RECARBON_SRC)`;
		if (args.has('--check') && !args.has('--strict')) {
			console.warn(`gen-controls: WARNING: ${msg}; drift check skipped, the committed JSON is used as is.`);
			return 0;
		}
		console.error(`gen-controls: ${msg}`);
		return 1;
	}
	let data;
	try {
		data = generate();
	} catch (e) {
		if (!(e instanceof SourceError)) throw e;
		console.error(`gen-controls: the source no longer matches what this generator reads: ${e.message}`);
		return 1;
	}
	let drift = false;
	for (const [name, path] of Object.entries(OUT)) {
		const text = serialize(data[name]);
		if (args.has('--check')) {
			const committed = existsSync(path) ? readFileSync(path, 'utf8') : '';
			if (committed !== text) {
				console.error(`gen-controls: ${path.slice(ROOT.length + 1)} is out of date with ReCarbon's source. Run: npm run gen:controls`);
				drift = true;
			}
		} else {
			writeFileSync(path, text);
			console.log(`gen-controls: wrote ${path.slice(ROOT.length + 1)}`);
		}
	}
	if (!drift && args.has('--check')) console.log(`gen-controls: in sync with ${data.controls.source.commit ?? SRC}`);
	return drift ? 1 : 0;
}

process.exit(main());
