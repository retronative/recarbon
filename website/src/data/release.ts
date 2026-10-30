// The published release, for the Download page and the installation guide.
// docs/download.md and docs/installation.md (the GitHub versions) carry the same values:
// change both at a release.
import { site } from '../site.config';

const version = '0.1.3';
const tag = `v${version}`;
const assets = `${site.repo.url}/releases/download/${tag}`;
const dmg = `ReCarbon-${version}-macos.dmg`;

export const release = {
	version,
	tag,
	page: `${site.repo.url}/releases/tag/${tag}`,
	source: `${site.repo.url}/tree/${tag}`,
	dmg: {
		name: dmg,
		url: `${assets}/${dmg}`,
		sha256Url: `${assets}/${dmg}.sha256`,
		sha256: 'b0cb142a90d5fe86ee5bc90234c78a824a1a4ff240fe8ebc5f261913e446a071',
	},
	thirdPartySource: {
		name: `ReCarbon-${version}-third-party-source.tar.xz`,
		sha256: '2277ed03badeb2da9402e2c186a2ad6d54c255f9a7284d5051b2d50885e784cb',
	},
};

// The installation guide's steps (src/pages/docs/installation/[step].astro), in order.
export const installSteps = [
	{ slug: 'step-1', title: 'Download ReCarbon', short: 'Download' },
	{ slug: 'step-2', title: 'Drag ReCarbon to Applications', short: 'Drag to Applications' },
	{ slug: 'step-3', title: 'If macOS blocks ReCarbon', short: 'Allow it once' },
	{ slug: 'step-4', title: 'Choose your disc image', short: 'Choose your disc' },
	{ slug: 'step-5', title: 'Play', short: 'Play' },
] as const;

export type InstallStep = (typeof installSteps)[number];
