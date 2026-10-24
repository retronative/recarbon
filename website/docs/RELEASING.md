# Releasing: versioned docs

Until ReCarbon's first tagged release there is one docs version, labelled by
`site.docsVersion` (`"alpha"`) in `src/site.config.ts`. Every docs page carries the alpha banner
(`src/components/starlight/Banner.astro`) and the header shows the version chip.

`starlight-versions` can't be enabled yet: it requires at least one archived version.

## At the first tagged release (e.g. 0.1)

1. Regenerate the data from the release's commit and commit it:
   ```sh
   RECARBON_SRC=<path to ReCarbon at the tag> npm run gen:controls
   ```
   Check the Known issues page and the measured figures (landing page, Performance) against the
   release's ROADMAP and FEASIBILITY.
2. Set `docsVersion` to the release (e.g. `"0.1"`), and update or remove the alpha banner text.
3. Build and check: `npm run build && npm run preview`.

## At the next release (when there is a version to archive)

1. `npm install --save-exact starlight-versions`.
2. In `astro.config.mjs`, add it to Starlight's plugins, archiving the version being replaced:
   ```js
   import starlightVersions from 'starlight-versions';
   // starlight({ plugins: [starlightVersions({ versions: [{ slug: '0.1' }] })], ... })
   ```
   Follow the plugin's own documentation for the exact archive step: it snapshots the current
   docs as that version and adds a version switcher. The docs here live under the `docs/` slug
   (`src/content/docs/docs/`), so check where it puts the archive before committing it.
3. Then update the current docs for the new release, and repeat step 1 above.
