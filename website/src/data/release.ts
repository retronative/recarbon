// The published release, for the Download page and the installation guide.
// docs/download.md and docs/installation.md (the GitHub versions) carry the same values:
// change both at a release.
import { site } from '../site.config';

const version = '0.1.5';
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
		sha256: '0a7ee90bb4612d3f38e28db423c367c3c32c0155489d8eeaf40017c067c18d26',
	},
	thirdPartySource: {
		name: `ReCarbon-${version}-third-party-source.tar.xz`,
		sha256: '721bf2a475454c112a39b3e40a9dd504e9a3e06842fd760510086199a5e56ae1',
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
