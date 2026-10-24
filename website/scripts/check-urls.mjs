#!/usr/bin/env node
// The site's URL forms, on the preview server (dist/, as GitHub Pages serves it)
// and the dev server: "/recarbon" and every other page without its trailing
// slash redirect (301) to the slashed URL with the query kept; slashed URLs and
// files are served as they are. Needs a build (dist/) first.
//
//   node scripts/check-urls.mjs          (npm run check:urls)
import { dev } from 'astro';
import { fileURLToPath } from 'node:url';
import { startPreview } from './preview.mjs';

const ROOT = fileURLToPath(new URL('..', import.meta.url));
let failed = 0;

async function expect(origin, path, status, location) {
	const res = await fetch(origin + path, { redirect: 'manual' });
	const got = res.headers.get('location');
	const ok = res.status === status && (location === undefined || got === location);
	if (!ok) failed++;
	console.log(`${ok ? 'ok  ' : 'FAIL'} ${path} -> ${res.status}${got ? ` ${got}` : ''}${ok ? '' : `  (expected ${status}${location ? ` ${location}` : ''})`}`);
}

const shared = [
	['/recarbon', 301, '/recarbon/'],
	['/recarbon?x=1&y=two%20words', 301, '/recarbon/?x=1&y=two%20words'],
	['/recarbon/', 200],
	['/recarbon/?x=1', 200],
	['/recarbon/docs', 301, '/recarbon/docs/'],
	['/recarbon/docs/licenses/gpl-3-0?from=footer', 301, '/recarbon/docs/licenses/gpl-3-0/?from=footer'],
	['/recarbon/garage/', 200],
	['/recarbon/docs/controls/', 200],
	['/recarbon/favicon.svg', 200],
];

const preview = await startPreview({ port: 0 });
const p = `http://127.0.0.1:${preview.address().port}`;
console.log('preview (dist/)');
for (const c of shared) await expect(p, ...c);
await expect(p, '/recarbon/no-such-page', 404);
preview.close();

const server = await dev({ root: ROOT, logLevel: 'error', server: { host: '127.0.0.1', port: 0 } });
const d = `http://127.0.0.1:${server.address.port}`;
console.log('dev');
for (const c of shared) await expect(d, ...c);
await server.stop();

console.log(failed ? `check-urls: ${failed} failed` : 'check-urls: all passed');
process.exit(failed ? 1 : 0);
