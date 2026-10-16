# Issue tracker: GitHub (separate repo)

Issues and specs for this repo live as GitHub issues in the **private repo `jakkarudev/DJNote_Mixxx_issues`**, not in this clone's remotes. Use the `gh` CLI for all operations.

**Always pass `-R jakkarudev/DJNote_Mixxx_issues`.** `gh` would otherwise infer the repo from `git remote -v`, which points at the code fork (`origin` = `jakkarudev/djnote_mixxx`, Issues disabled) or at upstream Mixxx (`upstream` = `mixxxdj/mixxx`). Never create, comment on, or label issues in either of those.

Because issues live in a different repo from the code, `#<n>` inside an issue refers to another issue in `DJNote_Mixxx_issues`. Reference code, commits, and PRs with full links, e.g. `jakkarudev/djnote_mixxx@<sha>` or `https://github.com/jakkarudev/djnote_mixxx/blob/djnote/src/...`.

## Conventions

- **Create an issue**: `gh issue create -R jakkarudev/DJNote_Mixxx_issues --title "..." --body "..."`. Use a heredoc for multi-line bodies.
- **Read an issue**: `gh issue view <number> -R jakkarudev/DJNote_Mixxx_issues --json number,title,body,labels,comments`.
- **List issues**: `gh issue list -R jakkarudev/DJNote_Mixxx_issues --state open --json number,title,body,labels,comments --jq '[.[] | {number, title, body, labels: [.labels[].name], comments: [.comments[].body]}]'` with appropriate `--label` and `--state` filters.
- **Make an issue a sub-issue of a parent**: `gh issue create -R jakkarudev/DJNote_Mixxx_issues --parent <parent> ...`, or `gh issue edit <parent> -R jakkarudev/DJNote_Mixxx_issues --add-sub-issue <child>` afterwards (`gh` 2.94+). Older `gh`: `gh api --method POST repos/jakkarudev/DJNote_Mixxx_issues/issues/<parent>/sub_issues -F sub_issue_id=<child-db-id>` (database id, as in **Blocking** below). Without sub-issues, put `Part of #<parent>` at the top of the child body.
- **Comment on an issue**: `gh issue comment <number> -R jakkarudev/DJNote_Mixxx_issues --body "..."`
- **Apply / remove labels**: `gh issue edit <number> -R jakkarudev/DJNote_Mixxx_issues --add-label "..."` / `--remove-label "..."`
- **Close**: `gh issue close <number> -R jakkarudev/DJNote_Mixxx_issues --comment "..."`

## Pull requests as a triage surface

**PRs as a request surface: no.** _(Set to `yes` if this repo treats external PRs as feature requests; `/triage` reads this flag.)_

When set to `yes`, PRs run through the same labels and states as issues, using the `gh pr` equivalents. Note that PRs live in the code repo (`jakkarudev/djnote_mixxx`), not the issues repo:

- **Read a PR**: `gh pr view <number> -R jakkarudev/djnote_mixxx --comments` and `gh pr diff <number> -R jakkarudev/djnote_mixxx` for the diff.
- **List external PRs for triage**: `gh api --paginate 'repos/jakkarudev/djnote_mixxx/pulls?state=open' --jq '.[] | select(.author_association | IN("OWNER","MEMBER","COLLABORATOR") | not) | {number, title, author: .user.login, author_association, labels: [.labels[].name]}'`.
- **Comment / label / close**: `gh pr comment`, `gh pr edit --add-label`/`--remove-label`, `gh pr close`, each with `-R jakkarudev/djnote_mixxx`.

## When a skill says "publish to the issue tracker"

Create a GitHub issue in `jakkarudev/DJNote_Mixxx_issues`.

## When a skill says "fetch the relevant ticket"

Read it as in **Read an issue** above.

## Wayfinding operations

Used by `/wayfinder`. The **map** is a single issue with **child** issues as tickets. All commands below take `-R jakkarudev/DJNote_Mixxx_issues`.

- **Map**: a single issue labelled `wayfinder:map`, holding the Notes / Decisions-so-far / Fog body. `gh issue create -R jakkarudev/DJNote_Mixxx_issues --label wayfinder:map`.
- **Child ticket**: an issue linked to the map as a GitHub sub-issue (see **Make an issue a sub-issue of a parent**). Where sub-issues aren't enabled, add the child to a task list in the map body and put `Part of #<map>` at the top of the child body. Labels: `wayfinder:<type>` (`research`/`prototype`/`grilling`/`task`). Once claimed, the ticket is assigned to the driving dev.
- **Blocking**: GitHub's **native issue dependencies**, the canonical, UI-visible representation. Add an edge with `gh api --method POST repos/jakkarudev/DJNote_Mixxx_issues/issues/<child>/dependencies/blocked_by -F issue_id=<blocker-db-id>`, where `<blocker-db-id>` is the blocker's numeric **database id** (`gh api repos/jakkarudev/DJNote_Mixxx_issues/issues/<n> --jq .id`, _not_ the `#number` or `node_id`). GitHub reports `issue_dependencies_summary.blocked_by` (open blockers only, the live gate). Where dependencies aren't available, fall back to a `Blocked by: #<n>, #<n>` line at the top of the child body. A ticket is unblocked when every blocker is closed.
- **Frontier query**: list the map's open children (`gh issue list -R jakkarudev/DJNote_Mixxx_issues --state open`, scoped to the map's sub-issues / task list), drop any with an open blocker (`issue_dependencies_summary.blocked_by > 0`, or an open issue in the `Blocked by` line) or an assignee; first in map order wins.
- **Claim**: `gh issue edit <n> -R jakkarudev/DJNote_Mixxx_issues --add-assignee @me`, the session's first write.
- **Resolve**: `gh issue comment <n> -R jakkarudev/DJNote_Mixxx_issues --body "<answer>"`, then `gh issue close <n> -R jakkarudev/DJNote_Mixxx_issues`, then append a context pointer (gist + link) to the map's Decisions-so-far.
