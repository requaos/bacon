# Meshtastic Beacon Module

A drop-in "welcome bot" module for [Meshtastic](https://meshtastic.org) firmware.
It greets nodes the first time they are heard on the local mesh and greets them
again if they have been away for more than three hours. Optimized for the
**RAKwireless WisMesh Tag (nRF52840)**, portable to other Meshtastic targets
(ESP32, RP2040, ...).

Instead of forking the (very large) upstream firmware repository, this repo
stays lightweight: `build.sh` clones upstream Meshtastic, injects the module
into the source tree, hooks it into module setup, and compiles it.

## Behavior

| Scenario | Action |
| --- | --- |
| Packet from a node we have never seen | Send it a DM: `Welcome to the local mesh! - Beacon` |
| Packet from a node seen within the last 3 hours | Nothing (clock is refreshed) |
| Packet from a node last seen more than 3 hours ago | Send it a DM: `Welcome back to the area! - Beacon` |

## Design constraints (nRF52840: 256 KB RAM, battery powered)

- **Event-driven execution** — no background timers or loops; all logic runs on
  the routing thread's receive path.
- **Deep-sleep compatible** — no `delay()` and no keep-awake locks, so the
  FreeRTOS PowerFSM can still drop the node into micro-amp sleep states.
- **Capacity-triggered LRU cache** — at most 250 tracked nodes (~9.6 KB of
  RAM); the oldest entry is evicted only when the 251st node arrives.
- **Anti-spam on boot** — the tracker is seeded from the node DB, so nodes
  already known to the device do not trigger a welcome flood after a reboot.

## Repository layout

```text
├── flake.nix            # Nix dev environment + GitHub Actions definition
├── build.sh             # Fetch, inject and compile upstream firmware
├── src/
│   ├── BeaconModule.h   # Module header
│   └── BeaconModule.cpp # Module implementation
├── README.md
└── LICENSE              # GPLv3 (matching Meshtastic upstream)
```

## Usage

### 1. Development environment

```shell
nix develop          # platformio, git, python3 + pre-commit hooks
```

(Any machine with `platformio`, `git` and `python3` also works without Nix.)

### 2. Build

```shell
./build.sh                     # defaults to the WisMesh Tag (rak_wismeshtag)
./build.sh rak4631             # any other PlatformIO env from upstream firmware
```

The firmware image ends up in `firmware_build/.pio/build/<env>/` as a
versioned UF2 file, e.g. `firmware-rak_wismeshtag-2.8.2.f7f66b4.uf2`
(depending on the upstream version at clone time). The first build also
downloads the ARM toolchain and framework packages into `~/.platformio`.

### 3. Flash the WisMesh Tag

1. Double-press the reset button to enter the UF2 bootloader.
2. Copy the `firmware-*.uf2` file from `firmware_build/.pio/build/rak_wismeshtag/`
   to the mounted USB drive.
3. To verify deep sleep, run the tag on battery (no USB attached) and check
   the current draw.

## CI

The GitHub Actions workflow (`.github/workflows/build.yaml`) is generated from
`flake.nix` by [actions.nix](https://github.com/nialov/actions.nix) — it is
written automatically by the pre-commit hook on your next `git commit` after
`nix develop`.

The dev shell installs the unwrapped `platformio-core`, which needs no user
namespaces — GitHub's Ubuntu 24.04+ runners restrict those via AppArmor, which
breaks the bubblewrap-sandboxed `platformio` package. On NixOS hosts (which
lack the FHS runtime for PlatformIO's downloaded toolchains) the shell hook
automatically prefers the bubblewrap-wrapped `platformio` instead.

## Implementation notes

- The module subclasses Meshtastic's `SinglePortModule` (the same pattern as
  upstream's `ReplyBotModule`) with `isPromiscuous = true` and a `wantPacket`
  override so it observes **every** over-the-air packet, not just texts
  addressed to this node (MQTT-sourced packets are ignored).
- Outgoing greetings are built with `allocDataPacket()` and sent through
  `service->sendToMesh()` — the current upstream send path
  (`MeshService::sendTextMessage()` no longer exists).
- Registration is injected into `setupModules()` in upstream
  `src/modules/Modules.cpp`, immediately before `routingModule = new
  RoutingModule();` (RoutingModule must remain the last module created).
- Timestamps use `millis()` deltas, which are wrap-safe in unsigned
  arithmetic.

## License

GPL-3.0 — matching the Meshtastic firmware this module is compiled into.
