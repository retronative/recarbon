import { defineRouteMiddleware } from '@astrojs/starlight/route-data';

// The sidebar's Installation entry links to the guide's first step; it stays marked as the
// current page on every step and on the all-steps page (/docs/installation/...).
export const onRequest = defineRouteMiddleware(({ locals, url }) => {
	const guide = `${import.meta.env.BASE_URL.replace(/\/?$/, '/')}docs/installation/`;
	if (!url.pathname.startsWith(guide)) return;
	type Entry = (typeof locals.starlightRoute.sidebar)[number];
	const mark = (entries: Entry[]) => {
		for (const e of entries) {
			if (e.type === 'group') mark(e.entries);
			else e.isCurrent = e.href.startsWith(guide);
		}
	};
	mark(locals.starlightRoute.sidebar);
});
