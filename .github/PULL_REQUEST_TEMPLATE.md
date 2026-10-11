## What and why

<!-- What does this change, and what problem does it solve? Link the issue: "Fixes #123". -->

## How it was verified

<!-- How you ran it: headless (offscreen + imgui.ini), against tools/mock_engine, with the Haruka
backend, under ASan/TSan... -->

## Not verified

<!-- What you could not check, for example mouse interaction or the rpm package. -->

## Checklist

- [ ] Commits are signed off (`git commit -s`, see CONTRIBUTING.md)
- [ ] The core stays engine-free (`check_core_engine_free` passes)
- [ ] Contract changes bump their version (`HARUKA_EDITOR_BACKEND_API_VERSION` / `HARUKA_EDITOR_PLUGIN_API_VERSION`)
- [ ] Remote protocol changes are mirrored in `tools/mock_engine`
- [ ] Vendored code changes are recorded in `third_party/VENDORED.md`
