// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

// Hosting (D1) and domain (D2): GitHub Pages of the public retronative/recarbon
// repository, which will hold the runtime and this site (in website/):
// https://retronative.github.io/recarbon/. SITE_URL and BASE_PATH override
// both, e.g. BASE_PATH=/ for a custom domain. See docs/DEPLOY.md.
const site = process.env.SITE_URL || 'https://retronative.github.io';
const base = process.env.BASE_PATH || '/recarbon/';

export default defineConfig({
	site,
	base,
	// Docs Markdown links are relative ("../faq/"), so they work under any base path.
	trailingSlash: 'always',
	integrations: [
		starlight({
			title: 'ReCarbon',
			description: 'ReCarbon: an unofficial native port of Need for Speed: Carbon (Xbox 360) to Apple Silicon Macs. Unofficial fan project.',
			logo: { src: './src/assets/recarbon-mark.svg', alt: 'ReCarbon' },
			favicon: '/favicon.svg',
			customCss: ['./src/styles/fonts.css', './src/styles/tokens.css', './src/styles/starlight.css'],
			components: {
				Header: './src/components/starlight/Header.astro',
				Footer: './src/components/starlight/Footer.astro',
				Banner: './src/components/starlight/Banner.astro',
				MobileMenuFooter: './src/components/starlight/MobileMenuFooter.astro',
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
						{ label: 'Licenses', collapsed: true, items: [{ autogenerate: { directory: 'docs/licenses' } }] },
					],
				},
			],
		}),
	],
});
