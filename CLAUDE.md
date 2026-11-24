# x64dbg-automate

x64dbg plugin that exposes a ZMQ-based automation server for remote control of the debugger.

## Build

**You must use the VS2022 cmake.** The system PATH does not have cmake. Use:

```bash
"/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build build64 --config Release
```

If you need to regenerate the build system:

```bash
"/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" -B build64 -G "Visual Studio 17 2022" -A x64
```

Output: `build64/Release/x64dbg-automate.dp64`

## Build/install/package scripts

Prefer these over manual cmake + copy steps:

- `build-install-64.cmd` / `build-install-32.cmd` (thin wrappers over `build-install.cmd 64|32`) — configure, build Release, and copy the plugin (`.dp64`/`.dp32` + `libzmq-mt-4_3_5.dll`) into `<X64DBG_RELEASE_DIR>\x64\plugins\` (or `x32`). cmake is found via vswhere, so they work from any shell.
- Machine-specific settings (`X64DBG_RELEASE_DIR`, optional `CMAKE_BIN` / `CMAKE_GENERATOR`) live in `local.cmd` (gitignored; template `local.cmd.example`) or env vars — never hardcode local paths in checked-in files. Without `X64DBG_RELEASE_DIR` the scripts build but skip the install.
- `package-release.ps1` — local fallback only (releases come from CI, below): runs both build-install scripts, then zips `build32/Release` and `build64/Release` into `release{32,64}-<version>.zip` using the version hardcoded at the top of the script.

## Releasing

Release artifacts are built by CI (`.github/workflows/build.yml`, `windows-2022` / VS 2022), not locally. Every push to `main` and every PR builds both bitnesses; the zips are downloadable from the workflow run.

1. If the wire protocol changed, bump `XAUTO_COMPAT_VERSION` in `src/xauto_cmd.h` (codename convention: see pyclient) and commit.
2. Tag and push: `git tag v<semver>-<codename>` then `git push origin v<semver>-<codename>`. The tag's codename must match `XAUTO_COMPAT_VERSION`, or CI fails the build.
3. CI creates a **draft** GitHub release for the tag with `release{32,64}-<semver>-<codename>.zip` and auto-generated notes. Re-running the workflow refreshes a draft's assets but refuses to touch a published release.
4. Run the e2e smoke test below against the draft's zips (`gh release download <tag>` works on drafts), then publish the draft.

## Release e2e smoke test

Once CI has drafted the GitHub release here (see Releasing) and the matching pyclient is on PyPI, verify the release artifacts work together before publishing the draft — using only published artifacts, nothing from the local working trees. **Both bitnesses ship, so both get exercised** — see step 5.

1. **Pristine x64dbg** — copy `build64/_deps/x64dbg-src/release` (the extracted snapshot, no config/plugins) to a temp dir. The snapshot contains both `x32/` and `x64/`, so one copy serves both legs.
2. **Plugin from GitHub** — download `release64-<version>.zip` and `release32-<version>.zip` from the release. Copy `Release/x64dbg-automate.dp64` + `Release/libzmq-mt-4_3_5.dll` from the 64-bit zip into `<temp>/x64/plugins/`, and `Release/x64dbg-automate.dp32` + its `libzmq-mt-4_3_5.dll` from the 32-bit zip into `<temp>/x32/plugins/`. The two zips carry different `libzmq` builds — do not cross them.
3. **Client from PyPI** — `python -m venv <temp>/venv`, then `pip install x64dbg-automate`; confirm `pip show` reports the new version (proves PyPI publish is live).
4. **Happy path, 64-bit** — with the venv python: `start_session` against `<temp>/x64/x64dbg.exe` (the session start asserts the compat handshake), then exercise whatever surface the release changed and `terminate_session`.
5. **Happy path, 32-bit** — repeat step 4 against `<temp>/x32/x32dbg.exe`. Do not skip this leg because 64-bit passed: bitness-conditional code can fail silently on x32dbg with x64dbg perfectly healthy, so a green 64-bit run is not evidence about 32-bit.
6. **Compat gate** — `pip install x64dbg-automate==<previous version>` in the same venv and confirm `start_session` raises `Incompatible x64dbg plugin and client versions ...` (only meaningful when the protocol codename changed).
7. **Cleanup** — kill any orphaned `x64dbg`/`x32dbg` processes spawned from the temp dir (the rejection test can leak one), delete the temp dir.

For a broader pre-tag check against the dev install rather than published artifacts, the pyclient suite is bitness-parameterized: run it once with `TEST_BITNESS=64` and once with `TEST_BITNESS=32`.

## Project structure

- `src/plugin.cpp` — Plugin callbacks, menu UI, event publishing
- `src/xauto_server.cpp` / `.h` — ZMQ server, session management, command handling
- `src/pluginmain.cpp` / `.h` — Plugin entry points and SDK boilerplate
- x64dbg plugin SDK is fetched via CMake into `build64/_deps/x64dbg-src/pluginsdk/`

## Git

Keep all commits authored solely as the repo owner (Darius Houle) — do not add `Co-Authored-By` or other attribution trailers.

Do not add Claude/AI attribution to any GitHub text on this project — commit messages, PR titles and descriptions, PR/issue comments, and issue bodies. This includes the `🤖 Generated with Claude Code` footer and any `Co-Authored-By: Claude` line. All such text must read as authored solely by the repo owner.

## Settings

Stored in x64dbg.ini under `[XAutomate]`. Key settings:
- `Mode` — `"local"` (default) or `"remote"`
- `BindAddress` — Bind address for remote mode (e.g. `0.0.0.0`)
- `ReqRepPort` / `PubSubPort` — Fixed ports for remote mode (0 = random, local only)
