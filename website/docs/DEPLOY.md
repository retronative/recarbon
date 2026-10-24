# Deployment

**Target:** <https://retronative.github.io/recarbon/>, served by GitHub Pages from the public
`retronative/recarbon` repository, which holds ReCarbon's runtime source, its docs and this
website together. Every step that publishes anything waits for the maintainer's approval.

## Now

- `astro.config.mjs` builds for local use by default: `site` is `http://127.0.0.1:4321` and
  `base` is `/`. The GitHub Pages build sets `SITE_URL=https://retronative.github.io` and
  `BASE_PATH=/recarbon/` (`npm run build:pages`, and the workflow below); a custom domain later
  would set its own `SITE_URL` and `BASE_PATH=/`. The docs' links to
  <https://retronative.github.io/recarbon/> become links within whichever build it is.
- The local preview follows the build's base path: <http://127.0.0.1:4321/> for `npm run build`,
  <http://127.0.0.1:4321/recarbon/> for `npm run build:pages` (`npm run preview:pages`).
- `.github/workflows/website.yml` (at the repository root) builds and deploys the site to GitHub
  Pages (Settings > Pages > Source: GitHub Actions). It runs on a push to `main` that changes the
  site or what it is built from, or by hand.

## In `retronative/recarbon`

The site lives in `website/` of the ReCarbon repository, and its docs are the repository's `docs/`.

1. The generators read ReCarbon's source from the parent folder (`website/..`); the license pages
   are built from the repository's license files.
2. It deployed first by hand (Actions, "Website"); after that site was checked, the workflow's
   `push` trigger was enabled.
3. `REPO_PUBLIC` in `src/site.config.ts` shows the GitHub links (header, footer, "View source")
   once the repository is public. `RECARBON_REPO_PUBLIC=1 npm run build` previews that state.
