// The published release, for the Download page and the installation guide.
// docs/download.md and docs/installation.md (the GitHub versions) carry the same values:
// change both at a release.
import { site } from '../site.config';

const version = '0.1.1';
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
		sha256: '7a43e99189cd809614c68c7dec0dc5dc046b0acd3f250a4c561123f590a55941',
	},
	thirdPartySource: {
		name: `ReCarbon-${version}-third-party-source.tar.xz`,
		sha256: '74926bf1918b9853aecbe000bec48c2830d49dd431496d53ff09d4d881ada6a8',
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
