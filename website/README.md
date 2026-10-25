# ReCarbon website

The website for ReCarbon (RetroNative): a racing-styled landing page, the **Garage** controls
screen, and the documentation. Built with Astro and Starlight. The design and its open decisions
are in [`docs/PLAN.md`](docs/PLAN.md).

## Run it

```sh
npm install
npm run dev        # http://localhost:4321/, live reload
npm run build      # checks the generated data against ReCarbon's source, then builds dist/ for "/"
npm run preview    # serves that dist/ on http://127.0.0.1:4321/, as GitHub Pages does
npm run check:urls # after a build: unslashed URLs ("/docs") redirect, in preview and dev
npm run check      # type-checks the .astro and .ts files

# The GitHub Pages build, for https://retronative.github.io/recarbon/:
npm run build:pages          # SITE_URL and BASE_PATH=/recarbon/ set
npm run preview:pages        # http://127.0.0.1:4321/recarbon/
BASE_PATH=/recarbon/ npm run check:urls
```

## Data from ReCarbon's source

Nothing about the controls or the settings is typed in by hand.

The license pages (`/docs/licenses/`) are built from the repository's `COPYING`, `NOTICE`,
`THIRD_PARTY_NOTICES.md` and `LICENSES/` each time the site is built (`src/lib/licenses.mjs`), so
the site keeps no copies of them.

| Script | Reads (ReCarbon, read-only) | Writes |
|---|---|---|
| `npm run gen:controls` | `runtime/user/settings.cpp`, `runtime/hid/{key_map.h,pad_map.h,input.cpp}`, `runtime/user/key_names.h`, `runtime/ui/{ui.cpp,settings_menu.cpp}`, `runtime/report/internal.h`, `tests/pad_map_test.cpp` | `src/data/controls.generated.json`, `src/data/settings.generated.json` |
| `npm run gen:progress` | `reference/ROADMAP.md`, and the maintainers' local evidence record (`progress.json`, not in the repository) through the script's allowlist | `src/data/progress.generated.json` |

`npm run build` runs it in `--check` mode first (`prebuild`) and **fails when the committed
files differ from the source**, or when the source no longer has a pattern the generator reads.
The generator also fails if `pad_map.h` disagrees with `pad_map_test.cpp`'s baseline.

The Progress page (`/progress/`) shows only what `scripts/gen-progress.mjs` allowlists: each
milestone's state must match its ROADMAP heading, every sentence is written in the script (none
is copied from the evidence record), at most 10 achievements are kept, newest first, and commit
links are kept only for commits on `origin/main`. A deny list (private paths, internal and legal
topics) fails the run. Without the evidence record, `--check` validates the committed JSON
against ROADMAP and passes. Regenerate after a push, so new commits get their links.

The source is the ReCarbon repository this site lives in (`website/`'s parent folder), or
`RECARBON_SRC` for another checkout. Without a source tree (a site-only clone), the check warns
and passes; add `--strict` to make it fail instead.

What each control **does in the game** is hand-written in `src/data/controls.actions.json`, with
a status and its evidence for every action. The build fails on a control id the generated data
doesn't have, or on an unverified action that names a game action. Never upgrade a status there
without the evidence that does it.

## Decisions (docs/PLAN.md §10)

Each is one switch, so settling it later is a small change:

| Decision | Where |
|---|---|
| D1 hosting, D2 domain | **Decided:** https://retronative.github.io/recarbon/, from the public `retronative/recarbon` repository (website in `website/`). Defaults in `astro.config.mjs`, overridable with `SITE_URL` / `BASE_PATH`. GitHub links: `site.repo` and the `REPO_PUBLIC` release flag in `src/site.config.ts`. Deployment is prepared, not enabled: [`docs/DEPLOY.md`](docs/DEPLOY.md) |
| D7 light theme | **Decided:** dark-only, docs included. Starlight's theme provider and menu are overridden in `src/components/starlight/` |
| D4 unverified mappings | `site.showUnverifiedActions` (shown as "to verify" today) |
| D5 audio | `site.trailer`: null means no sound-on video |
| D8 footage | `site.heroVideo`: null keeps the abstract placeholder hero |
| Release versioning | `site.docsVersion`, and [`docs/RELEASING.md`](docs/RELEASING.md) |

Docs Markdown links are relative (`../faq/`), so they work under any base path. Custom pages use
`url()` from `src/lib/url.ts`.

## Layout

- `src/pages/index.astro`, `src/pages/garage.astro`: the custom pages, on `src/layouts/RaceLayout.astro`.
- `src/components/garage/`: the keyboard and controller drawings (original SVG), the Garage, the text tables.
- `src/lib/controls.ts`: joins the generated data and the actions overlay; validates both.
- `src/components/starlight/`: Starlight overrides (header, footer, banner, mobile menu).
- `src/styles/tokens.css`: the palette and motion tokens, shared by everything.
- `../docs/` (the repository's `docs/`): the documentation, one source for GitHub and the site.
  It is loaded from there (`src/content.config.ts`); `src/lib/docs-markdown.mjs` turns its
  GitHub-style file links and alerts into the site's URLs and asides. After changing that plugin,
  clear Astro's content cache (`rm -rf .astro node_modules/.astro`): cached `.md` pages aren't
  rendered again otherwise.
- `src/lib/licenses.mjs`, `src/pages/docs/licenses/`: the license pages, built from the
  repository's license files.
- `src/pages/docs/download/` and `src/pages/docs/installation/[step].astro`: the Download page and
  the five-step installation guide (one static page per step), with the release's values in
  `src/data/release.ts`. `docs/download.md` is their GitHub version and is not built as a page;
  `docs/installation.md` is, as the guide's all-steps page (`/docs/installation/`).
  `src/starlightRouteData.ts` keeps the sidebar's Installation entry current on all of them.

## Rules

No EA or Need for Speed logos, artwork, music or footage, and no fabricated gameplay. Game names
appear only as plain factual references. Nothing links to game files. Fonts (Chakra Petch, Inter,
JetBrains Mono; SIL OFL) are served from the site itself.
