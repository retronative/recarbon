// ReCarbon's license pages, read from the repository root (COPYING, NOTICE,
// THIRD_PARTY_NOTICES.md, LICENSES/) when the site is built, so the site keeps
// no copies of them. Plain JavaScript: astro.config.mjs builds the sidebar
// from the same list.
import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const WEBSITE = fileURLToPath(new URL('../..', import.meta.url));
// The repository this site lives in (website/'s parent), or RECARBON_SRC.
export const SRC = resolve(WEBSITE, process.env.RECARBON_SRC ?? '..');

export function slug(file) {
	return file
		.replace(/\.(txt|md)$/, '')
		.toLowerCase()
		.replace(/[^a-z0-9.]+/g, '-')
		.replace(/\./g, '-');
}

function read(rel) {
	const path = join(SRC, rel);
	if (!existsSync(path)) throw new Error(`licenses: ${path} not found (set RECARBON_SRC to a ReCarbon checkout)`);
	return readFileSync(path, 'utf8');
}

// Every page but the overview, in sidebar order.
export function licensePages() {
	const texts = readdirSync(join(SRC, 'LICENSES'))
		.filter((f) => /\.(txt|md)$/.test(f))
		.sort();
	return [
		{ id: 'third-party-notices', title: 'Third-party notices', source: 'THIRD_PARTY_NOTICES.md', markdown: true, description: 'What ReCarbon is built from and with, and the license of each component.' },
		{ id: 'notice', title: 'NOTICE', source: 'NOTICE', description: "ReCarbon's copyright and license notice." },
		...texts.map((f) => {
			const name = f.replace(/\.(txt|md)$/, '');
			return { id: slug(f), title: name, source: `LICENSES/${f}`, description: `License text: ${name}, from ReCarbon's LICENSES folder.` };
		}),
	];
}

// THIRD_PARTY_NOTICES.md links to files in the repository: license files go to
// their pages here; other paths (scripts, patches) stay as text.
function siteLinks(markdown, base) {
	const page = (id) => `${base}docs/licenses/${id ? `${id}/` : ''}`;
	return markdown.replace(/\[([^\]]+)\]\(([^)\s]+)\)/g, (all, text, target) => {
		if (/^[a-z]+:/.test(target) || target.startsWith('#')) return all;
		if (target === 'COPYING') return `[${text}](${page('gpl-3-0')})`;
		if (target === 'NOTICE') return `[${text}](${page('notice')})`;
		if (target === 'LICENSES' || target === 'LICENSES/') return `[${text}](${page('')})`;
		const m = target.match(/^LICENSES\/(.+)$/);
		if (m) return `[${text}](${page(slug(m[1]))})`;
		return text;
	});
}

/** @returns {import('astro/loaders').Loader} */
export function licensesLoader() {
	return {
		name: 'recarbon-licenses',
		load: async ({ store, config, renderMarkdown, parseData, watcher }) => {
			store.clear();
			const base = config.base.endsWith('/') ? config.base : `${config.base}/`;
			for (const [order, p] of licensePages().entries()) {
				const body = read(p.source);
				const data = await parseData({ id: p.id, data: { title: p.title, description: p.description, source: p.source, order, ...(p.markdown ? {} : { text: body }) } });
				store.set({ id: p.id, data, body, ...(p.markdown ? { rendered: await renderMarkdown(siteLinks(body, base)) } : {}) });
			}
			watcher?.add([join(SRC, 'LICENSES'), join(SRC, 'NOTICE'), join(SRC, 'THIRD_PARTY_NOTICES.md')]);
		},
	};
}
