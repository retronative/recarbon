// The published release, for the Download page and the installation guide.
// docs/download.md and docs/installation.md (the GitHub versions) carry the same values:
// change both at a release.
import { site } from '../site.config';

const version = '0.1.0';
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
		sha256: 'abc376fb04550842fe4a5a34d4feb75c8c9dd302a8b45db4f590be3018d60d9b',
	},
	thirdPartySource: {
		name: `ReCarbon-${version}-third-party-source.tar.xz`,
		sha256: 'a0e805b44a4724acc445e18049ed5662946376c4a94f52d8ef311704cc19e883',
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
