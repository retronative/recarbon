import { defineCollection } from 'astro:content';
import { glob } from 'astro/loaders';
import { docsSchema } from '@astrojs/starlight/schema';
import { z } from 'astro/zod';
import { licensesLoader } from './lib/licenses.mjs';

export const collections = {
	// The repository's docs/, one source for GitHub and the site. Starlight's docsLoader()
	// only reads src/content/docs/; this is the same glob, from the repository root, so the
	// ids (and URLs) stay docs/<page>. docs/README.md is the GitHub index, not a page.
	docs: defineCollection({
		loader: glob({ base: '..', pattern: ['docs/**/[^_]*.{md,mdx}', '!docs/README.md'] }),
		schema: docsSchema(),
	}),
	// ReCarbon's own license files (COPYING, NOTICE, THIRD_PARTY_NOTICES.md, LICENSES/),
	// read from the repository root at build time: the site keeps no copies.
	licenses: defineCollection({
		loader: licensesLoader(),
		schema: z.object({ title: z.string(), description: z.string(), source: z.string(), order: z.number(), text: z.string().optional() }),
	}),
};
