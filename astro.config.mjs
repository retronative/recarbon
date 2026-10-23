// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

// Hosting (D1) and domain (D2) are open decisions: the site's URL and base
// path come from the environment so either choice needs no code change.
//   custom domain:     SITE_URL=https://recarbon.example        (no BASE_PATH)
//   GitHub Pages:      SITE_URL=https://<org>.github.io BASE_PATH=/recarbon-site
const site = process.env.SITE_URL || 'http://localhost:4321';
const base = process.env.BASE_PATH || '/';

export default defineConfig({
	site,
	base,
	// Docs Markdown links are relative ("../faq/"), so they work under any base path.
	trailingSlash: 'always',
	integrations: [
		starlight({
			title: 'ReCarbon',
			description: 'ReCarbon: a native Apple Silicon recompilation of an Xbox 360 racing classic. Unofficial fan project.',
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
