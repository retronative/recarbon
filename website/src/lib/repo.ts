import { site } from '../site.config';

// The repository's public links, or null while it is private.
export const repo = site.repo.public
	? {
			url: site.repo.url,
			issues: `${site.repo.url}/issues`,
			releases: `${site.repo.url}/releases`,
		}
	: null;
