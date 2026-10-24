// ReCarbon's docs (the repository's docs/) are written to read on GitHub: links
// to other pages are file links ("faq.md"), license links point at the
// repository's files ("../NOTICE"), links to the published site are absolute,
// and callouts are GitHub alerts ("> [!NOTE]", an optional bold first line
// being the title). This Sätteri mdast plugin turns them into the site's: page
// URLs under the base path, the license pages, and Starlight asides (each alert
// becomes the `:::note[Title]` directive Starlight's own plugin, which runs
// after this one, renders).
import { posix, relative, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { slug as licenseSlug } from './licenses.mjs';

const ASIDES = { NOTE: 'note', TIP: 'tip', IMPORTANT: 'note', WARNING: 'caution', CAUTION: 'danger' };

function siteUrl(target, root, hash) {
	let m;
	if ((m = target.match(/^docs\/(.+)\.mdx?$/))) return `${root}docs/${m[1] === 'index' ? '' : `${m[1].replace(/\/index$/, '')}/`}${hash}`;
	if (target === 'NOTICE') return `${root}docs/licenses/notice/${hash}`;
	if (target === 'COPYING') return `${root}docs/licenses/gpl-3-0/${hash}`;
	if (target === 'THIRD_PARTY_NOTICES.md') return `${root}docs/licenses/third-party-notices/${hash}`;
	if (target === 'LICENSES') return `${root}docs/licenses/${hash}`;
	if ((m = target.match(/^LICENSES\/(.+)$/))) return `${root}docs/licenses/${licenseSlug(m[1])}/${hash}`;
	return null;
}

// "> [!NOTE]\n> **Title**\n>\n> Body" (the blockquote's source) as ":::note[Title]\nBody\n:::".
function alertToDirective(source) {
	const lines = source.split('\n').map((l) => l.replace(/^\s*>\s?/, ''));
	const m = lines[0].match(/^\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]\s*$/);
	if (!m) return null;
	let body = lines.slice(1);
	let title = '';
	const t = body[0]?.match(/^\*\*(.+)\*\*\s*$/);
	if (t) {
		title = `[${t[1]}]`;
		body = body.slice(1);
	}
	while (body.length && !body[0].trim()) body.shift();
	return `:::${ASIDES[m[1]]}${title}\n${body.join('\n').trimEnd()}\n:::`;
}

export function docsMarkdownPlugin({ docsDir, base, site }) {
	const root = base.replace(/\/?$/, '/');
	const siteRoot = `${site.replace(/\/$/, '')}${root}`;
	docsDir = resolve(docsDir);
	return ({ fileURL }) => {
		const path = fileURL && fileURLToPath(fileURL);
		if (!path || !path.startsWith(docsDir + sep)) return null;
		const from = posix.join('docs', relative(docsDir, posix.dirname(path.split(sep).join('/'))).split(sep).join('/'));
		const mapUrl = (href) => {
			if (href.startsWith(siteRoot)) return root + href.slice(siteRoot.length);
			if (/^[a-z]+:/.test(href) || href.startsWith('#')) return href;
			const [url, frag] = href.split('#');
			const to = siteUrl(posix.normalize(posix.join(from, url)), root, frag ? `#${frag}` : '');
			if (!to) throw new Error(`${path}: docs link "${href}" is not a docs page, a license file or the site`);
			return to;
		};
		return {
			name: 'recarbon-docs',
			// The alert's source text is read by its position.
			options: { position: true },
			link(node, ctx) {
				const to = mapUrl(node.url);
				if (to !== node.url) ctx.setProperty(node, 'url', to);
			},
			blockquote(node, ctx) {
				const { start, end } = node.position ?? {};
				if (start?.offset == null || end?.offset == null) return;
				const directive = alertToDirective(ctx.source.slice(start.offset, end.offset));
				// The directive is parsed again after this pass, so its links are mapped here.
				if (directive) ctx.replaceNode(node, { raw: directive.replace(/\]\(([^)\s]+)\)/g, (all, url) => `](${mapUrl(url)})`) });
			},
		};
	};
}
