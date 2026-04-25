# Haruka Project — Documentation Coverage Tracker

## Goal
Track STL-style documentation coverage across the whole repository.

## Coverage Status
- [x] `src/core/components` (headers + implementation comments)
- [x] `src/core` (non-component headers documented)
- [x] `src/renderer` (public headers documented)
- [x] `src/io` (public headers documented)
- [x] `src/physics` (public headers documented)
- [x] `src/audio` (public headers documented)
- [x] `src/network` (public headers documented)
- [x] `src/database` (public headers documented)
- [x] `src/game` (public headers documented)
- [x] `src/editor` (public headers documented)
- [x] `src/test` (chat test utility documented)
- [x] root runtime docs alignment (`README`, build/runtime caveats)

## Per-Module Acceptance Criteria
1. Public classes/functions include STL-style contract docs.
2. Ownership/lifetime is explicit for shared/raw resources.
3. Error/fallback behavior is documented.
4. Serialization formats and defaults are documented.
5. Performance-sensitive paths include concise complexity notes.

## Notes
- Documentation pass is behavior-preserving.
- If a module has generated or external code, it is excluded unless explicitly requested.
