# Contributing to Haruka Editor

Thanks for your interest. This guide covers how to propose a change. The code and testing
standards are in [QUALITY.md](QUALITY.md).

## Before you start

- **Bugs:** open an issue with the bug report template. Say which backend was loaded (Haruka,
  remote, none) and include the editor log.
- **New panels:** consider writing a plugin (see [Writing a plugin](README.md#writing-a-plugin))
  before changing the core. Panels that only make sense for one engine belong in that engine's
  backend, not in the core.
- **Adapters for other engines** (Unreal, O3DE…) are welcome. Open an issue first; the remote
  protocol (`src/backends/remote/remote_protocol.h`) is the intended path.
- **Engine bugs** belong in the [engine repository](https://github.com/Andonigt04/haruka-cpp).
- **Security problems:** do not open a public issue. Follow [SECURITY.md](SECURITY.md).

## Setting up

```bash
git clone https://github.com/Andonigt04/haruka-cpp     # optional: a local engine next to the editor
git clone https://github.com/Andonigt04/haruka
cd haruka && ./build.sh
```

To work on the core without any engine, configure with `-DHARUKA_EDITOR_BACKEND_HARUKA=OFF`.

## Making a change

1. Fork the repository and create a branch from `main`.
2. Keep each pull request to one problem.
3. Follow [QUALITY.md](QUALITY.md). In particular, the core must stay engine-free, and contract
   changes must bump their version.
4. Check the editor starts and draws your panel without a window:

   ```bash
   SDL_VIDEODRIVER=offscreen HARUKA_EDITOR_OPEN=<your-panel> HARUKA_EDITOR_QUIT_AFTER=6 \
     XDG_CONFIG_HOME="$(mktemp -d)" ./build/bin/HarukaEditor
   ```

## Pull requests

Fill in the template. Say how you verified the change, and what you did **not** verify. For
example, "only tested offscreen, no mouse interaction" is useful information.

## Sign your commits (DCO)

This project uses the [Developer Certificate of Origin 1.1](https://developercertificate.org/).
Sign each commit with `git commit -s`. `git rebase --signoff` fixes existing commits.

## License of contributions

Contributions are accepted under the [Apache License 2.0](LICENSE), like the rest of the project.
Vendored third-party code keeps its own license; record its origin and version in
`third_party/VENDORED.md`.

## Language

Issues, pull requests and these guides are in English. Much of the code is commented in Spanish;
keep the language of the file you edit.
