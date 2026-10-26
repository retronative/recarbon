#!/usr/bin/env node
// axe-core (WCAG 2.0/2.1/2.2 A and AA) on the built site, at 1440 and 390 px, in the system's
// Google Chrome through playwright-core (no browser is downloaded). Serves dist/ with
// scripts/preview.mjs on 127.0.0.1:4472, under the base the build was made for (BASE_PATH, or
// read from dist/index.html). Fails on any serious or critical violation; the others are listed.
//
//   npm run build:pages && npm run test:a11y          (A11Y_VERBOSE=1 lists every node)
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { chromium } from 'playwright-core';

const dist = new URL('../dist/', import.meta.url);
process.env.BASE_PATH ||= readFileSync(new URL('index.html', dist), 'utf8').match(/"(\/[^"]*?)_astro\//)?.[1] ?? '/';
const { startPreview } = await import('./preview.mjs');
const BASE = process.env.BASE_PATH.replace(/\/?$/, '/');
const PORT = 4472;
const AXE = readFileSync(createRequire(import.meta.url).resolve('axe-core/axe.min.js'), 'utf8');
const TAGS = ['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa'];
const WIDTHS = [1440, 390];
const PAGES = [
	'', 'docs/', 'docs/download/', 'docs/installation/',
	...[1, 2, 3, 4, 5].map((n) => `docs/installation/step-${n}/`),
	'docs/known-issues/', 'docs/controls/', 'progress/', 'garage/',
];

const server = await startPreview({ port: PORT });
const browser = await chromium.launch({ channel: 'chrome' });
const failures = [];
try {
	for (const width of WIDTHS) {
		const context = await browser.newContext({ viewport: { width, height: 900 } });
		const page = await context.newPage();
		for (const path of PAGES) {
			const where = `/${path} @${width}px`;
			const res = await page.goto(`http://127.0.0.1:${PORT}${BASE}${path}`, { waitUntil: 'load' });
			if (res?.status() !== 200) {
				failures.push(`${where}: HTTP ${res?.status()}`);
				continue;
			}
			await page.evaluate(() => document.fonts.ready);
			await page.addScriptTag({ content: AXE });
			const violations = await page.evaluate(async (tags) => {
				const r = await /** @type {any} */ (window).axe.run(document, { runOnly: { type: 'tag', values: tags } });
				return r.violations.map((v) => ({ id: v.id, impact: v.impact, nodes: v.nodes.map((n) => n.target.join(' ')) }));
			}, TAGS);
			const serious = violations.filter((v) => v.impact === 'serious' || v.impact === 'critical');
			console.log(`${where}: ${serious.length} serious/critical, ${violations.length - serious.length} other`);
			for (const v of violations) {
				const line = `${where}: ${v.id} (${v.impact}) at ${(process.env.A11Y_VERBOSE ? v.nodes : v.nodes.slice(0, 3)).join(', ')}`;
				if (serious.includes(v)) failures.push(line);
				else console.log(`  ${line}`);
			}
		}
		await context.close();
	}
} finally {
	await browser.close();
	server.close();
}

if (failures.length) {
	console.error(`\na11y: ${failures.length} serious/critical finding(s)\n${failures.map((f) => `  - ${f}`).join('\n')}`);
	process.exit(1);
}
console.log(`\na11y: ${PAGES.length} pages x ${WIDTHS.length} widths, 0 serious/critical violations`);
