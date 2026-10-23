# ReCarbon website

The website for ReCarbon (RetroNative): a racing-styled landing page, the **Garage** controls
screen, and the documentation. Built with Astro and Starlight. The design and its open decisions
are in [`docs/PLAN.md`](docs/PLAN.md).

## Run it

```sh
npm install
npm run dev        # http://localhost:4321, live reload
npm run build      # checks the generated data against ReCarbon's source, then builds dist/
npm run preview    # serves dist/ on http://127.0.0.1:4321/recarbon/
npm run check      # type-checks the .astro and .ts files
```

## Data from ReCarbon's source

Nothing about the controls or the settings is typed in by hand.

| Script | Reads (ReCarbon, read-only) | Writes |
|---|---|---|
| `npm run gen:controls` | `runtime/user/settings.cpp`, `runtime/hid/{key_map.h,pad_map.h,input.cpp}`, `runtime/user/key_names.h`, `runtime/ui/{ui.cpp,settings_menu.cpp}`, `runtime/report/internal.h`, `tests/pad_map_test.cpp` | `src/data/controls.generated.json`, `src/data/settings.generated.json` |
| `npm run sync:licenses` | `LICENSES/*`, `NOTICE` | `src/content/docs/docs/licenses/*.md` |

`npm run build` runs both in `--check` mode first (`prebuild`) and **fails when the committed
files differ from the source**, or when the source no longer has a pattern the generator reads.
The generator also fails if `pad_map.h` disagrees with `pad_map_test.cpp`'s baseline.

The source is found at `RECARBON_SRC`, by default `../recarbon-worker/src/<commit>` (see the
scripts). Without a source tree (a site-only clone), the check warns and passes; add `--strict`
to make it fail instead.

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
- `src/content/docs/docs/`: the documentation.

## Rules

No EA or Need for Speed logos, artwork, music or footage, and no fabricated gameplay. Game names
appear only as plain factual references. Nothing links to game files. Fonts (Chakra Petch, Inter,
JetBrains Mono; SIL OFL) are served from the site itself.
