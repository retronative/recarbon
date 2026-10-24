# Deployment

**Target:** <https://retronative.github.io/recarbon/>, served by GitHub Pages from the public
`retronative/recarbon` repository, which will publish ReCarbon's runtime source and this website
together. **Nothing is deployed yet.** This file records the plan; every step that publishes
anything waits for explicit approval.

## Now

- The website is developed in this independent repository. It has no remote and is never pushed.
- `astro.config.mjs` builds for local use by default: `site` is `http://127.0.0.1:4321` and
  `base` is `/`. The GitHub Pages build sets `SITE_URL=https://retronative.github.io` and
  `BASE_PATH=/recarbon/` (`npm run build:pages`, and the workflow below); a custom domain later
  would set its own `SITE_URL` and `BASE_PATH=/`. The docs' links to
  <https://retronative.github.io/recarbon/> become links within whichever build it is.
- The local preview follows the build's base path: <http://127.0.0.1:4321/> for `npm run build`,
  <http://127.0.0.1:4321/recarbon/> for `npm run build:pages` (`npm run preview:pages`).
- `deploy/github-pages.yml` is the Pages workflow, kept as a **template outside
  `.github/workflows/`** so it can't run. It is manual-only (`workflow_dispatch`) even once copied.

No other public repository is created, and no repository is renamed.

## Later: integrating into `retronative/recarbon/website/`

1. Copy this repository's tracked files into `website/` of the ReCarbon repository (or import its
   history with `git subtree add --prefix=website <path-to-this-repo> main`, which keeps the
   commits). Leave out `node_modules/`, `dist/` and `.astro/`.
2. The generators then find ReCarbon's source in the parent folder (`website/..`) by themselves.
   Run `npm run gen:controls` once there and commit the result: the
   generated JSON's `source.commit` becomes `null` in-repo (the data's commit is the repository's
   own), so it must be regenerated once after the move.
3. Copy `deploy/github-pages.yml` to `.github/workflows/website.yml`, and in the repository's
   Settings > Pages choose "GitHub Actions" as the source.
4. Once publishing is approved, run the workflow by hand from the Actions tab. Enable its `push`
   trigger only after that first run is checked.
5. When the repository is public, set `REPO_PUBLIC = true` in `src/site.config.ts` (the release
   flag). The GitHub link in the header, the repository, issues and releases links in the footer,
   and "View source on GitHub" in the Developers docs then appear. While it is false they are
   hidden or shown disabled. `RECARBON_REPO_PUBLIC=1 npm run build` previews the public state
   without changing the flag.

The repository must be public for Pages on GitHub's free plan; the site is public either way.
