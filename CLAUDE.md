# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

mir2x is a C++26 reimplementation of the classic Legend of Mir 2 (v1.45) MMORPG, used to verify actor-model-based parallelism for game servers. Client (SDL3) and server (asio + FLTK monitor GUI) share a `common` static library; ~30 helper tools live under `tools/`.

## Build

Dependencies come from vcpkg manifest mode (`vcpkg.json`, overlay ports in `ports/`). The `build.py` helper bootstraps vcpkg (into `<build-dir>/vcpkg` unless `--vcpkg-prefix` points at an existing checkout), configures, builds, and installs. Builds are incremental; run from a dedicated build dir:

```sh
mkdir b_mir2x && cd b_mir2x
python3 /path/to/mir2x/build.py --c-compiler=gcc-16 --cxx-compiler=g++-16 --parallel=10
```

- Requires **GCC 16** on Linux (`CMAKE_CXX_STANDARD 26`; coroutine-based actor model). Windows is built from MSYS2 UCRT64 (triplet auto-detected as `x64-mingw-static`). 32-bit is rejected.
- Output: `<build-dir>/build` (CMake tree + `vcpkg_installed`), installed to `<build-dir>/install` (`install/server`, `install/client`, plus tool binaries).
- `--target <name>` builds a single CMake target; `--no-install` skips install. Note resource packing (asset conversion) only runs at **install** time, so a plain `--target server --no-install` won't refresh client/server resources.
- `--fresh` deletes `<build-dir>/build` entirely (rebuilds all vcpkg ports, re-clones resources) — only use for a real clean build.
- `--res-path=<dir>` reuses an existing `mir2x_res` checkout; otherwise CMake clones https://github.com/etorth/mir2x_res.git into `<build-dir>/build/assets/mir2x_res`.
- Sanitizers are Debug-only CMake options: `-DMIR2X_ENABLE_ASAN=ON`, `MIR2X_ENABLE_USAN`, `MIR2X_ENABLE_TSAN` (with `--build-type=Debug`). ccache is picked up automatically if installed.
- `--c-compiler/--cxx-compiler` internally enable `VCPKG_CHAINLOAD_TOOLCHAIN_FILE=cmake/Mir2xVcpkgChainload.cmake` so vcpkg ports and project targets use the same compiler.
- CI (`.github/workflows/build.yml`) builds on ubuntu-26.04 with gcc-16 and MSYS2 UCRT64, then verifies linkage with `ldd`/`readelf`; there is no test stage.

## Run

```sh
cd b_mir2x/install/server && ./server --auto-launch          # start monoserver (master mode, FLTK GUI)
cd b_mir2x/install/client && ./client --server-ip=localhost --auto-login=test:123456
```

- "Monoserver" = the single `server` binary in default master mode. `--slave --master-ip=... --master-port=...` runs a headless distributed map peer; `--actor-pool-thread=N` sizes the actor thread pool.
- `mir2x-server.sh <install-path>` / `mir2x-client.sh <install-path>` are gperftools CPU-profiling wrappers (LD_PRELOAD libprofiler, emit SVG via google-pprof); there are matching memory/CPU profiler scripts.

## Tests and lint

There is no unit-test suite. `tools/pyluacheck` syntax-checks Lua (`luac -p`) under `server/script/`. CI only verifies that installed binaries link.

## Architecture

### Actor model and coroutines (server)

The server is an actor system on C++20/26 coroutines (`common/src/corof.hpp`: `awaitable<T>` with symmetric transfer, `eval_poller<T>` for polled coroutines).

- `server/src/actorpool.hpp` — `ActorPool`: thread pool; one `MailboxBucket` per thread (97 sub-buckets each), CAS-locked mailboxes, work stealing. `runOneMailbox()` drives message dispatch and periodically injects `AM_METRONOME` for time-based processing.
- `server/src/actorpod.hpp` — `ActorPod`: per-actor message endpoint. **Request/response is `co_await m_actorPod->send(addr, mbuf)`** (caller coroutine suspends; resumed by `innHandler()` when the reply arrives, keyed by sequence id); `post()` is fire-and-forget. Message types: `ActorMsgPackType` enum + packed structs in `server/src/actormsg.hpp`.
- Actor hierarchy: `ServerObject` (`serverobject.hpp`, owns an `ActorPod`, pure-virtual `onActorMsg()`) → `CharObject` (position/map/view-list, movement protocol TRYLEAVE/ALLOWLEAVE/LEAVEOK/FINISHLEAVE) → `BattleObject` (HP/combat) → `Player` / `Monster`; plus `NPChar` (NPCs), `ServerMap` (grid map actor), `ServerGuard`, `Quest`.
- `ServiceCore` (`servicecore.hpp`, extends `PeerCore`) is the central coordinator: login, char creation, map loading, quest registration; it receives `AM_RECVPACKAGE` from `netdriver.hpp` (client connections) / `actornetdriver.hpp` (peer links) and dispatches into the actor graph.
- FLTK monitor GUI (`.fl` files in `server/src/` — mainwindow, actor/pod monitors, profiler, configure) is compiled by `fluid` via `fltk_parse_fl_file()` in the root CMakeLists and runs on the main thread, separate from actor-pool threads.

### Lua scripting (sol2)

Game logic is Lua via sol2 (`common/src/luamodule.hpp` → `server/src/serverluamodule.hpp`). Two sources: `.lua` files embedded next to classes in `server/src/` (e.g. `player.lua`, `servermap.lua`, `quest.lua`), and the `server/script/` tree — `api/` (npc/player/quest modules), `map/` (per-map scripts), `npc/` (per-NPC dialogue/shop/teleport scripts, with shared includes in `npc/include/`), `quest/`, `run/` (startup loaders). C++ binds functions into Lua via `bindFunction()`.

### Network protocol (common)

Client↔server messages are `#pragma pack(1)` structs: `CMType` + structs in `common/src/clientmsg.hpp`, `SMType` in `common/src/servermsg.hpp`. `common/src/msgf.hpp` provides the head-code/attribute framing (fixed vs variable size, VLQ lengths, xor/compression flags); `common/src/serdesmsg.hpp` holds cereal-serialized payloads (`SDInitPlayer`, `SDSendPackage`, …) for complex bodies and peer-to-peer data. Adding a message = new enum value + packed struct + handler registration on both ends.

### Client (SDL3)

- `client/src/client.hpp` — `Client::mainLoop()`: poll SDL events → poll `NetIO` (`netio.hpp`, asio TCP with VLQ-framed packets and timeout-able response handlers) → update/draw the current `Process`.
- Scene state machine in `process.hpp`: `ProcessID` LOGO → SYNC → LOGIN → (CREATEACCOUNT/SELECTCHAR/CREATECHAR/CHANGEPASSWORD) → RUN (game world, `processrun.hpp`). Each process has a sibling `*net.cpp` (e.g. `processrunnet.cpp`) holding its network handlers; process switches are deferred via `requestProcess()`.
- Retained-mode UI: `widget.hpp` (`Widget`/`WidgetTreeNode` tree, lazy variant-valued layout properties); concrete panels are `*Board` classes (inventory, minimap, skill, guild, NPC chat, …) aggregated by `GUIManager` owned by `ProcessRun`.
- `sdldevice.hpp` — `SDLDevice` wraps window/renderer/ttf/mixer (BGM + 128 positioned SFX channels) and IME; `SDLDeviceHelper` RAII guards render state.
- Assets: globals created in `main.cpp` — `PNGTexDB`/`PNGTexOffDB` (ZSDB-backed texture caches, the latter with sprite offsets), `MapBinDB`, `FontexDB`, `EmojiDB`, `BGMusicDB`, `SoundEffectDB`.

### Assets and tools

Original Mir2 `.wil`/`.wix`/`.wzl`/`.zl` packages are decoded by `common/src/wilimagepackage.hpp` and converted during install into ZSDB/PNG/OGG by `tools/installres/src/install_res.py` (`common/src/zsdb.hpp` is the zstd container format). `tools/` groups into: asset converters (`pkgviewer`, `mapeditor`, `mapconverter`, `mapdbmaker`, `mapinfo`, the `*wil2png` family), data generators (`monstergen`, `npcgen`, `zsdbmaker`, `dropgen`, `*csv2cpp` stat-table compilers), audio/format decoders (`wtldecoder`, `sound2ogg`, `*listdecoder`), and utilities (`xmltran`, `pyluacheck`, `selectchardb`, `gif2emoji`…).

### Storage

Server SQLite databases (via SQLiteCpp, `server/src/dbpod.hpp`) are **generated at startup** by `Server::createDefaultDatabase()` — no `.db` files are checked in. Character/world data lives there; static game data (maps, items) comes from the installed resource files.

## Gotchas

- Root CMakeLists auto-generates `MIR2X_LOG_FILELIST` for `logProfiler()` — do **not** define it manually (FATAL_ERROR).
- Generated headers `buildconfig.hpp` / `logprof.hpp` are `configure_file` outputs under `<build>/config_file/`; edit the `.in` templates in `common/src/` instead.
- Source globs are recursive (`mir2x_list_source_recursive`): new `.cpp` files are picked up only after a CMake re-configure.
