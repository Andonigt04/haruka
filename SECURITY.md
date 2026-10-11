# Security policy

## Reporting a vulnerability

**Please do not report security problems in public issues, pull requests or discussions.**

Report them privately through GitHub:
**[Report a vulnerability](https://github.com/Andonigt04/haruka/security/advisories/new)**
(the *Security* tab, then *Report a vulnerability*).

Include the affected commit, the kind of issue, steps or a file that reproduce it, and the impact
you expect. Engine problems go to the
[engine repository](https://github.com/Andonigt04/haruka-cpp/security/advisories/new).

## What to expect

This project is maintained by one person, so responses are best effort. The intent is to
acknowledge a report within a week, keep you informed while it is fixed, and credit you in the
advisory unless you prefer otherwise. Please allow reasonable time for a fix before disclosing.

## Supported versions

Only the latest commit on `main` receives security fixes.

## Scope and trust model

**Backends and plugins are native code that the editor loads into its own process.** Loading a
`.so` from `bin/backends/`, `bin/plugins/`, `~/.local/share/haruka-editor/plugins/` or the paths in
`HARUKA_EDITOR_BACKEND` / `HARUKA_EDITOR_PLUGINS` runs that code with your permissions. That is
by design and not a vulnerability. Only install plugins you trust. A way to make the editor load a
library **without** the user placing it in one of those locations would be a vulnerability.

In scope:

| Area | Where | Why it matters |
|---|---|---|
| Remote protocol | `src/backends/remote/` | parses frames from another process: lengths, JSON, raw binary |
| Project and document files | `src/panels/project_browser.*`, backends | opening a project from someone else |
| Engine discovery | `src/engine_sdk.*` | reads `haruka-engine.json` and runs the engine's build |
| Asset import | `src/panels/asset_importer.*` | files from disk |
