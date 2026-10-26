#!/usr/bin/env node
// Serves dist/ under the base path the way GitHub Pages serves it: a folder
// without its trailing slash is redirected (301, query kept) to the slashed
// URL, a folder serves its index.html, and anything missing gets 404.html.
// `astro preview` answers 404 to "/docs" instead, and has no hook for a
// static site's redirects.
//
//   node scripts/preview.mjs [--port N] [--host H]     (npm run preview)
import { createServer } from 'node:http';
import { existsSync, readFileSync, statSync } from 'node:fs';
import { extname, join, normalize, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { slashRedirect } from '../src/lib/trailing-slash.mjs';

const ROOT = resolve(fileURLToPath(new URL('..', import.meta.url)));
const DIST = join(ROOT, 'dist');
// The base the build was made for: "/" locally, BASE_PATH=/recarbon/ for a GitHub Pages build.
const BASE = (process.env.BASE_PATH || '/').replace(/\/?$/, '/');
const TYPES = {
	'.html': 'text/html; charset=utf-8', '.css': 'text/css; charset=utf-8', '.js': 'text/javascript; charset=utf-8',
	'.mjs': 'text/javascript; charset=utf-8', '.json': 'application/json', '.svg': 'image/svg+xml', '.png': 'image/png',
	'.jpg': 'image/jpeg', '.avif': 'image/avif', '.webp': 'image/webp', '.ico': 'image/x-icon', '.woff2': 'font/woff2', '.woff': 'font/woff',
	'.xml': 'application/xml', '.txt': 'text/plain; charset=utf-8', '.wasm': 'application/wasm', '.pf_meta': 'application/octet-stream',
};

function file(urlPath) {
	if (!urlPath.startsWith(BASE)) return null;
	const rel = decodeURIComponent(urlPath.slice(BASE.length));
	const path = normalize(join(DIST, rel));
	if (!path.startsWith(DIST)) return null;
	if (existsSync(path) && statSync(path).isDirectory()) return existsSync(join(path, 'index.html')) ? join(path, 'index.html') : null;
	return existsSync(path) && statSync(path).isFile() ? path : null;
}

export function startPreview({ port = 4321, host = '127.0.0.1' } = {}) {
	if (!existsSync(DIST)) throw new Error('preview: no dist/; run npm run build first');
	const server = createServer((req, res) => {
		const to = (req.method === 'GET' || req.method === 'HEAD') && slashRedirect(req.url, BASE, (p) => file(p) !== null);
		if (to) {
			res.writeHead(301, { Location: to });
			return res.end();
		}
		const path = file(req.url.split('?')[0]);
		const status = path ? 200 : 404;
		const served = path ?? join(DIST, '404.html');
		res.writeHead(status, { 'Content-Type': TYPES[extname(served)] ?? 'application/octet-stream' });
		res.end(req.method === 'HEAD' ? undefined : readFileSync(served));
	});
	return new Promise((ok) => server.listen(port, host, () => ok(server)));
}

if (import.meta.url === `file://${process.argv[1]}`) {
	const arg = (name, fallback) => {
		const i = process.argv.indexOf(name);
		return i > 0 ? process.argv[i + 1] : fallback;
	};
	const server = await startPreview({ port: Number(arg('--port', 4321)), host: arg('--host', '127.0.0.1') });
	const { address, port } = server.address();
	console.log(`preview: http://${address}:${port}${BASE}`);
}
