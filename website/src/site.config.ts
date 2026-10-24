// The release flag for the source repository: true once
// github.com/retronative/recarbon is public.
const REPO_PUBLIC = true;

// Site-wide switches. Each open decision in docs/PLAN.md §10 is one value
// here, so settling it later is a one-line change.

export const site = {
	name: 'ReCarbon',
	org: 'RetroNative',
	tagline: 'Carbon. Native. Apple Silicon.', // copy to be approved (PLAN §5.4)

	// Release versioning: one "alpha" docs version until the first tagged
	// release, then starlight-versions (docs/RELEASING.md).
	docsVersion: 'alpha',

	// The source repository (D1: the runtime and this site, published together).
	// Its links stay hidden or disabled until REPO_PUBLIC (above) is true; nothing
	// asks GitHub at run time. RECARBON_REPO_PUBLIC=1 at build time previews
	// the public state without changing the flag.
	repo: {
		url: 'https://github.com/retronative/recarbon',
		public: REPO_PUBLIC || process.env.RECARBON_REPO_PUBLIC === '1',
	},

	// D4 unverified mappings: true shows game actions not yet hand-tested as
	// "to verify"; false hides them until hand test 5 and the controller test.
	showUnverifiedActions: true,

	// D8 footage: null keeps the abstract placeholder hero. When real ReCarbon
	// footage is approved, set the encoded sources (PLAN §5.4) and the caption.
	heroVideo: null as null | {
		poster: string;
		sources: { src: string; type: string; media?: string }[];
		caption: string; // e.g. "Captured in ReCarbon 0.1, MacBook Air M5"
	},

	// Release: null until there is a public download. The hero then shows
	// "Download: coming soon" instead of a Download button.
	downloadUrl: null as string | null,

	// D5 audio: the optional full video with sound. Null: no "Watch trailer"
	// button. Only set once its audio is licensed or original.
	trailer: null as null | { src: string; type: string; captions?: string },
};
