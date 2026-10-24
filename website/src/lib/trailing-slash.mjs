// The site's URLs end in "/" (trailingSlash: 'always'). A request for a page
// without it, "/recarbon" or "/recarbon/docs?x=1", is redirected to the same
// URL with the slash, keeping the query, as GitHub Pages does for a folder.
// Astro's dev and preview servers answer 404 to it instead. Files (a path whose
// last part has an extension) are left alone.

// The redirect target for `url` (path and query, as a request has it), or null.
// `isPage(path)`, when given, says whether the slashed path is a page (the
// preview server checks dist/; GitHub Pages redirects folders only).
export function slashRedirect(url, base, isPage = () => true) {
	const root = base.replace(/\/?$/, '/');
	const q = url.indexOf('?');
	const path = q < 0 ? url : url.slice(0, q);
	const query = q < 0 ? '' : url.slice(q);
	if (path.endsWith('/') || /\.[a-z0-9]+$/i.test(path.split('/').pop())) return null;
	if (path !== root.slice(0, -1) && !path.startsWith(root)) return null;
	return isPage(`${path}/`) ? `${path}/${query}` : null;
}

// Connect middleware sending the redirect (301, the permanent one GitHub Pages sends).
export function slashRedirectMiddleware(base, isPage) {
	return (req, res, next) => {
		const to = (req.method === 'GET' || req.method === 'HEAD') && slashRedirect(req.url, base, isPage);
		if (!to) return next();
		res.statusCode = 301;
		res.setHeader('Location', to);
		res.end();
	};
}

// The dev server's redirect, as an Astro integration (astro.config.mjs). It has
// to come first in the dev server's middleware stack: Astro puts its own
// trailing-slash check (a 404 for "/recarbon/docs") and base-path handling at
// the front, from its Vite plugin's post hook. So this is a Vite plugin too,
// ordered after Astro's (enforce: 'post'), whose post hook goes in front of them.
export function trailingSlashRedirect() {
	return {
		name: 'recarbon-trailing-slash',
		hooks: {
			'astro:config:setup': ({ config, updateConfig }) => {
				const handle = slashRedirectMiddleware(config.base);
				const plugin = {
					name: 'recarbon-trailing-slash',
					apply: 'serve',
					enforce: 'post',
					configureServer: (server) => () => void server.middlewares.stack.unshift({ route: '', handle }),
				};
				updateConfig({ vite: { plugins: [plugin] } });
			},
		},
	};
}
