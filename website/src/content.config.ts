import { defineCollection } from 'astro:content';
import { docsLoader } from '@astrojs/starlight/loaders';
import { docsSchema } from '@astrojs/starlight/schema';
import { z } from 'astro/zod';
import { licensesLoader } from './lib/licenses.mjs';

export const collections = {
	docs: defineCollection({ loader: docsLoader(), schema: docsSchema() }),
	// ReCarbon's own license files (COPYING, NOTICE, THIRD_PARTY_NOTICES.md, LICENSES/),
	// read from the repository root at build time: the site keeps no copies.
	licenses: defineCollection({
		loader: licensesLoader(),
		schema: z.object({ title: z.string(), description: z.string(), source: z.string(), order: z.number(), text: z.string().optional() }),
	}),
};
