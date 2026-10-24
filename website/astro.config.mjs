// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';
import { fileURLToPath } from 'node:url';
import { licensePages } from './src/lib/licenses.mjs';
import { satteri } from '@astrojs/markdown-satteri';
import { docsMarkdownPlugin } from './src/lib/docs-markdown.mjs';
import { trailingSlashRedirect } from './src/lib/trailing-slash.mjs';

// Hosting (D1) and domain (D2): GitHub Pages of the public retronative/recarbon
// repository, which will hold the runtime and this site (in website/):
// https://retronative.github.io/recarbon/. SITE_URL and BASE_PATH override
// both, e.g. BASE_PATH=/ for a custom domain. See docs/DEPLOY.md.
const site = process.env.SITE_URL || 'https://retronative.github.io';
const base = process.env.BASE_PATH || '/recarbon/';

export default defineConfig({
	site,
	base,
	trailingSlash: 'always',
	// The docs are the repository's docs/ (src/content.config.ts), written to read on GitHub;
	// docsMarkdownPlugin turns their file links and alerts into the site's URLs and asides.
	markdown: { processor: satteri({ mdastPlugins: [docsMarkdownPlugin({ docsDir: fileURLToPath(new URL('../docs/', import.meta.url)), base, site })] }) },
	vite: {
		// The docs import the site's components as @site/... (they live outside src/).
		resolve: { alias: { '@site': fileURLToPath(new URL('./src', import.meta.url)) } },
		// The dev server may read the repository's docs/ and license files.
		server: { fs: { allow: ['..'] } },
	},
	integrations: [
		// "/recarbon" -> "/recarbon/" in dev, as GitHub Pages and scripts/preview.mjs do.
		trailingSlashRedirect(),
		starlight({
			title: 'ReCarbon',
			// Starlight's Markdown features (asides, heading links) for docs outside src/content/docs/.
			markdown: { processedDirs: ['../docs'] },
			description: 'ReCarbon: an unofficial native port of Need for Speed: Carbon (Xbox 360) to Apple Silicon Macs. Unofficial fan project.',
			// The title text next to it names the link (kept for screen readers when hidden), so the mark is decorative.
			logo: { src: './src/assets/recarbon-mark.svg', alt: '' },
			favicon: '/favicon.svg',
			customCss: ['./src/styles/fonts.css', './src/styles/tokens.css', './src/styles/starlight.css'],
			components: {
				Header: './src/components/starlight/Header.astro',
				Footer: './src/components/starlight/Footer.astro',
				Banner: './src/components/starlight/Banner.astro',
				MobileMenuFooter: './src/components/starlight/MobileMenuFooter.astro',
				// Dark-only (D7): the theme is always dark and there is no theme menu.
				ThemeProvider: './src/components/starlight/ThemeProvider.astro',
				ThemeSelect: './src/components/starlight/ThemeSelect.astro',
			},
			sidebar: [
				{
					label: 'Start',
					items: [
						{ label: 'Overview', slug: 'docs' },
						{ label: 'Download', slug: 'docs/download' },
						{ label: 'Installation', slug: 'docs/installation' },
						{ label: 'Supported hardware', slug: 'docs/hardware' },
					],
				},
				{
					label: 'Play',
					items: [
						{ label: 'Controls', slug: 'docs/controls' },
						{ label: 'Settings', slug: 'docs/settings' },
						{ label: 'Performance', slug: 'docs/performance' },
					],
				},
				{
					label: 'Help',
					items: [
						{ label: 'Troubleshooting', slug: 'docs/troubleshooting' },
						{ label: 'Known issues', slug: 'docs/known-issues' },
						{ label: 'FAQ', slug: 'docs/faq' },
					],
				},
				{
					label: 'Project',
					items: [
						{ label: 'Developers', slug: 'docs/developers' },
						{ label: 'Credits', slug: 'docs/credits' },
						{
							label: 'Licenses',
							collapsed: true,
							// Pages built from the repository's license files (src/pages/docs/licenses/).
							items: [{ label: 'Overview (GPL-3.0-or-later)', link: '/docs/licenses/' }, ...licensePages().map((p) => ({ label: p.title, link: `/docs/licenses/${p.id}/` }))],
						},
					],
				},
			],
		}),
	],
});
