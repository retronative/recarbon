#!/usr/bin/env node
// The site's URL forms, on the preview server (dist/, as GitHub Pages serves it)
// and the dev server: every page without its trailing slash ("/docs", and the
// base itself, "/recarbon", for a GitHub Pages build) redirects (301) to the
// slashed URL with the query kept; slashed URLs and files are served as they
// are. Run it after a build, with the BASE_PATH the build had:
//
//   npm run build && npm run check:urls
//   npm run build:pages && BASE_PATH=/recarbon/ npm run check:urls
import { dev } from 'astro';
import { fileURLToPath } from 'node:url';
import { startPreview } from './preview.mjs';

const ROOT = fileURLToPath(new URL('..', import.meta.url));
const B = (process.env.BASE_PATH || '/').replace(/\/?$/, '/');
let failed = 0;

async function expect(origin, path, status, location) {
	// As a browser asks for a page; the dev server redirects only those.
	const res = await fetch(origin + path, { redirect: 'manual', headers: { accept: path.includes('@') ? '*/*' : 'text/html' } });
	const got = res.headers.get('location');
	const ok = res.status === status && (location === undefined || got === location);
	if (!ok) failed++;
	console.log(`${ok ? 'ok  ' : 'FAIL'} ${path} -> ${res.status}${got ? ` ${got}` : ''}${ok ? '' : `  (expected ${status}${location ? ` ${location}` : ''})`}`);
}

const shared = [
	// The base without its slash ("/recarbon"); at "/" there is no such URL.
	...(B === '/' ? [] : [
		[B.slice(0, -1), 301, B],
		[`${B.slice(0, -1)}?x=1&y=two%20words`, 301, `${B}?x=1&y=two%20words`],
	]),
	[B, 200],
	[`${B}?x=1`, 200],
	[`${B}docs`, 301, `${B}docs/`],
	[`${B}docs?x=1&y=two%20words`, 301, `${B}docs/?x=1&y=two%20words`],
	[`${B}docs/licenses/gpl-3-0?from=footer`, 301, `${B}docs/licenses/gpl-3-0/?from=footer`],
	[`${B}garage/`, 200],
	[`${B}docs/controls/`, 200],
	[`${B}favicon.svg`, 200],
];

const preview = await startPreview({ port: 0 });
const p = `http://127.0.0.1:${preview.address().port}`;
console.log('preview (dist/)');
for (const c of shared) await expect(p, ...c);
await expect(p, `${B}no-such-page`, 404);
preview.close();

const server = await dev({ root: ROOT, base: B, logLevel: 'error', server: { host: '127.0.0.1', port: 0 } });
const d = `http://127.0.0.1:${server.address.port}`;
console.log('dev');
for (const c of shared) await expect(d, ...c);
// The dev server's own requests have no extension and are never redirected:
// its client script, as a page loads it.
const client = (await (await fetch(`${d}${B}`)).text()).match(/src="([^"]*@vite\/client)"/)?.[1];
if (!client) failed++, console.log('FAIL no @vite/client script on the dev page');
else await expect(d, client, 200);
await server.stop();

console.log(failed ? `check-urls: ${failed} failed` : 'check-urls: all passed');
process.exit(failed ? 1 : 0);
