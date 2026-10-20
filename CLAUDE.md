## Upstream-merge friendly

DJNote must stay easy to update to the next stable Mixxx (`upstream` remote). Wherever possible:

- Put DJNote logic in new files; touch upstream files only at small hook points.
- Never rename or repurpose upstream config keys, UI elements, skin attributes or controls; add new ones under DJNote-specific names.
- Prefer wrapping or observing upstream classes over editing them.

## Agent skills

### Issue tracker

Issues live in the private GitHub repo `jakkarudev/DJNote_Mixxx_issues` (not this clone's remotes); always pass `-R jakkarudev/DJNote_Mixxx_issues` to `gh`. See `docs/agents/issue-tracker.md`.

### Triage labels

Default five labels: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `GLOSSARY.md` + `docs/adr/` at the repo root. See `docs/agents/domain.md`.
