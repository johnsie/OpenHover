# Provenance ledger

Every file in this repository is listed here with where it came from. A change is not
complete until its files are recorded (see `docs/clean-room-policy.md`);
`scripts/check-provenance.py` enforces it.

**Status labels**

- **Original:** created for OpenHover.
- **Carried over:** copied unmodified from the licensed project's repository ("HoverNet
  Classic", commit `2c597eb`, 2 October 2026). Each was written there *after* the HoverRace
  import, uses no HoverRace code, types, or headers (it builds here with the standard
  library alone), and is independent of it. **The maintainer must confirm ownership before
  the licence applies to them;** most were written with AI assistance (Claude), which is recorded in the
  notes. They still carry the old product name in places; rename them under OpenHover.

## Design Research Records

- **2026-10-02:** `johnsie/HoverNet`, revision `f1aea2c855b54e5c59ae20116efede3a5abf53cc`,
  GrokkSoft HoverRace SourceCode License v1.0. Consulted checkpoint and finish-line source
  declarations plus race-gate validation. Design question: how should OpenHover make lap
  completion unambiguous with a small number of gates? High-level finding: use an explicit,
  ordered sequence of authored race gates that is separate from the visual route geometry.
  No source, identifiers, tests, data, assets, or file structure were copied.

| Path | Origin | Authors | Licence status | Notes |
| --- | --- | --- | --- | --- |
| `README.md` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | |
| `assets/screenshots/harbor-loop-gameplay.png` | Original | Maintainer, with AI assistance (GitHub Copilot) | CC BY 4.0 | Screenshot captured from the OpenHover runtime |
| `.gitignore` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | |
| `.github/copilot-instructions.md` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | project-wide development instructions |
| `PROVENANCE.md` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | this ledger |
| `CMakeLists.txt` | Original | Maintainer, with AI assistance (Claude, GitHub Copilot) | MIT OR Apache-2.0 | SDL2/OpenGL interactive executable |
| `src/AudioFeedback.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | Procedural SDL audio feedback API |
| `src/AudioFeedback.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | Runtime-generated menu, boost, impact, and checkpoint sounds |
| `LICENSE-MIT` | Original | The standard MIT text, with the project copyright line | licence text | MIT licence text itself |
| `LICENSE-APACHE` | Third party: the Apache Software Foundation, https://www.apache.org/licenses/LICENSE-2.0.txt | Apache Software Foundation | the licence text itself (Apache-2.0) | Apache-2.0 licence text, unmodified (SHA-256 `cfc7749b`) |
| `docs/licensing.md` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | needs legal review |
| `docs/roadmap.md` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | clean-room product roadmap |
| `docs/track-format.md` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | original track format documentation |
| `THIRD_PARTY_NOTICES.md` | Original | Maintainer, with AI assistance (Claude, GitHub Copilot) | MIT OR Apache-2.0 | SDL2 and system OpenGL notices |
| `docs/clean-room-policy.md` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | needs legal review |
| `scripts/check-provenance.py` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | |
| `src/util/Sha256.h` | Carried over: `NetTarget/LinuxClient/Sha256.h` (added 2026-10-01, `86e942b`) | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 once ownership is confirmed | standard SHA-256 (FIPS 180-4); library only |
| `src/util/Sha256.cpp` | Carried over: `NetTarget/LinuxClient/Sha256.cpp` (`86e942b`) | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 once ownership is confirmed | |
| `src/util/Gunzip.h` | Carried over: `NetTarget/LinuxClient/Gunzip.h` (`86e942b`) | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 once ownership is confirmed | gzip/DEFLATE decoder (RFC 1951/1952) |
| `src/util/Gunzip.cpp` | Carried over: `NetTarget/LinuxClient/Gunzip.cpp` (`86e942b`) | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 once ownership is confirmed | |
| `src/util/WireFormat.h` | Carried over: `NetTarget/Util/WireFormat.h` (added 2026-09-28, `4ea517d`) | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 once ownership is confirmed | fixed-width little-endian helpers; namespace `HoverNetWire` to rename |
| `src/content/TrackCatalog.h` | Carried over: `NetTarget/LinuxClient/TrackCatalog.h` (added 2026-10-01, `d3121dc`) | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 once ownership is confirmed | track list and safe-name resolution; `.trk` suffix and "community" wording are Classic-specific |
| `src/content/TrackCatalog.cpp` | Carried over: `NetTarget/LinuxClient/TrackCatalog.cpp` (`d3121dc`) | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 once ownership is confirmed | |
| `src/content/TrackDownloader.h` | Carried over: `NetTarget/LinuxClient/TrackDownloader.h` (`86e942b`) | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 once ownership is confirmed | verified on-demand downloads |
| `src/content/TrackDownloader.cpp` | Carried over: `NetTarget/LinuxClient/TrackDownloader.cpp` (`86e942b`) | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 once ownership is confirmed | default download URL points at the Classic project's releases: replace |
| `tests/TrackCatalogSmoke.cpp` | Carried over: `NetTarget/LinuxClient/TrackCatalogSmoke.cpp` (`d3121dc`) | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 once ownership is confirmed | test |
| `tests/TrackDownloadSmoke.cpp` | Carried over: `NetTarget/LinuxClient/TrackDownloadSmoke.cpp` (`86e942b`) | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 once ownership is confirmed | test |
| `scripts/make-release-checksums.sh` | Carried over: `scripts/make-release-checksums.sh` (added 2026-10-01, `eddc478`) | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 once ownership is confirmed | writes `SHA256SUMS` |
| `packaging/debian/reproducible.sh` | Carried over: `packaging/debian/reproducible.sh` (`eddc478`) | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 once ownership is confirmed | reproducible-build helper |
| `src/game/Hovercraft.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | hovercraft simulation API |
| `src/game/Hovercraft.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic 2D hovercraft simulation |
| `src/main.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | SDL2/OpenGL 3D test arena |
| `tests/HovercraftSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | hovercraft simulation test |
| `src/game/Championship.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | championship event progression API |
| `src/game/Championship.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic championship scoring and progression |
| `tests/ChampionshipSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | championship progression test |
| `src/game/RouteGuidance.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | route-direction guidance API |
| `src/game/RouteGuidance.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic wrong-way detection |
| `tests/RouteGuidanceSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | route guidance test |
| `src/game/SteeringAssist.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | steering assistance API |
| `src/game/SteeringAssist.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | route-aware player steering assist |
| `tests/SteeringAssistSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | steering assistance test |
| `src/game/RecoveryAssist.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | route recovery assistance API |
| `src/game/RecoveryAssist.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic route recovery |
| `tests/RecoveryAssistSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | route recovery test |
| `src/game/InputRecording.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic input recording API |
| `src/game/InputRecording.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic input recording |
| `tests/InputRecordingSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | input recording test |
| `src/game/HazardZone.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | hazard zone API |
| `src/game/HazardZone.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | time-scaled hazard slow zones |
| `tests/HazardZoneSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | hazard zone test |
| `src/game/CraftClass.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | original hovercraft class API |
| `src/game/CraftClass.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | original hovercraft handling presets |
| `tests/CraftClassSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | hovercraft class test |
| `src/game/Race.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | checkpoint and lap progression API |
| `src/game/Race.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | checkpoint and lap progression |
| `tests/RaceSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | race progression test |
| `src/game/RacePosition.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic race position API |
| `src/game/RacePosition.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic race position calculation |
| `tests/RacePositionSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | race position test |
| `src/game/RivalController.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic rival steering API |
| `src/game/RivalController.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | checkpoint-following rival controller |
| `tests/RivalControllerSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | rival controller test |
| `src/game/RaceStart.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | race start countdown API |
| `src/game/RaceStart.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | race start countdown |
| `tests/RaceStartSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | race start countdown test |
| `src/game/Course.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | course boundary and projection API |
| `src/game/Course.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | course boundary and recovery projection |
| `tests/CourseSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | course boundary test |
| `src/game/RacerCollision.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | racer collision API |
| `src/game/RacerCollision.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic racer collision resolution |
| `tests/RacerCollisionSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | racer collision test |
| `src/game/BoostPad.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | boost-pad API |
| `src/game/BoostPad.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic boost-pad refill |
| `tests/BoostPadSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | boost-pad test |
| `src/game/FixedStepClock.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic simulation clock API |
| `src/game/FixedStepClock.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | fixed-step simulation clock |
| `tests/FixedStepClockSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | fixed-step simulation clock test |
| `src/game/TrackDefinition.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | track definition API |
| `src/game/TrackDefinition.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | original built-in track definitions and validation |
| `tests/TrackDefinitionSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | track definition test |
| `src/game/RaceMode.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | selectable race modes API |
| `src/game/RaceMode.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | race mode behavior |
| `tests/RaceModeSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | race mode test |
| `src/game/WallCollision.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | course wall collision API |
| `src/game/WallCollision.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic course wall rebound |
| `tests/WallCollisionSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | course wall collision test |
| `src/game/RaisedSection.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | raised track-section collision API |
| `src/game/RaisedSection.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | airborne-clearable raised-section collision |
| `tests/RaisedSectionSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | raised-section collision test |
| `src/game/LapTiming.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | lap timing API |
| `src/game/LapTiming.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | current, last, and best lap timing |
| `tests/LapTimingSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | lap timing test |
| `tests/AudioFeedbackSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | audio feedback preference test |

| `src/game/Missile.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic race missile API |
| `src/game/Missile.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | accelerated bouncing missile and spin-out hits |
| `tests/MissileSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | missile movement and impact test |
| `src/game/Mine.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deterministic race mine API |
| `src/game/Mine.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | mine placement and spin-out hits |
| `tests/MineSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | mine placement and impact test |
| `src/game/PracticeGuide.h` | Original | Maintainer, with AI assistance (OpenAI Codex) | MIT OR Apache-2.0 | event-driven in-game practice lesson API |
| `src/game/PracticeGuide.cpp` | Original | Maintainer, with AI assistance (OpenAI Codex) | MIT OR Apache-2.0 | deterministic practice lesson progression |
| `tests/PracticeGuideSmoke.cpp` | Original | Maintainer, with AI assistance (OpenAI Codex) | MIT OR Apache-2.0 | practice lesson progression test |
| `src/game/KeyBindings.h` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | remappable keyboard binding API |
| `src/game/KeyBindings.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | binding defaults, conflict swap, validation, persistence text |
| `tests/KeyBindingsSmoke.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | key binding behavior test |
| `src/game/PadBindings.h` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | remappable gamepad binding API |
| `src/game/PadBindings.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | pad defaults, swap, validation, persistence text |
| `tests/PadBindingsSmoke.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | gamepad binding behavior test |
| `src/game/PadMenuInput.h` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | gamepad-to-menu-key mapping API |
| `src/game/PadMenuInput.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | D-pad, A, B and Start menu mapping |
| `tests/PadMenuInputSmoke.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | gamepad menu mapping test |
| `src/game/GhostLibrary.h` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | best-run ghost store API |
| `src/game/GhostLibrary.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | best ghost selection and versioned persistence |
| `tests/GhostLibrarySmoke.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | ghost selection and persistence test |
| `tests/ImpairedNetworkSmoke.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | lossy, delayed, mixed-rate input stream test |
| `tests/TrackLapTimeSmoke.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | lap duration bounds test for built-in tracks |
| `docs/release-notes.md` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | player-facing release notes |
| `src/net/ReconnectPolicy.h` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | client reconnect scheduling API |
| `src/net/ReconnectPolicy.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | backoff schedule and give-up rules |
| `tests/ReconnectPolicySmoke.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | reconnect scheduling test |
| `src/game/RivalNames.h` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | AI rival display name API |
| `src/game/RivalNames.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | rival name roster |
| `tests/RivalNamesSmoke.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | rival name roster test |
| `src/game/CameraRig.h` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | chase camera motion comfort API |
| `src/game/CameraRig.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | heading smoothing and speed-zoom policy |
| `tests/CameraRigSmoke.cpp` | Original | Maintainer, with AI assistance (Claude) | MIT OR Apache-2.0 | camera comfort behavior test |
| `src/net/Lobby.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | multiplayer lobby protocol and state API |
| `src/net/Protocol.h` | Original | Maintainer, with AI assistance (OpenAI Codex) | MIT OR Apache-2.0 | shared protocol and built-in content compatibility versions |
| `src/net/Lobby.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | multiplayer room, presence, settings, and chat state |
| `src/net/TcpLobbyClient.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | cross-platform TCP lobby client API |
| `src/net/TcpLobbyClient.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | cross-platform TCP lobby client implementation |
| `src/net/AuthoritativeRace.h` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | authoritative online race API |
| `src/net/AuthoritativeRace.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | server-side race simulation and snapshots |
| `src/server/main.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | dedicated lobby and race server entry point |
| `tests/LobbySmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | lobby state and protocol test |
| `tests/LobbyClientSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | TCP lobby and race integration test |
| `tests/AuthoritativeRaceSmoke.cpp` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | authoritative race simulation test |
| `docs/multiplayer.md` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | multiplayer use, protocol, deployment, and operations guide |
| `.github/workflows/package.yml` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | GitHub Actions release packaging workflow |
| `.gitlab-ci.yml` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | race server build and deployment pipeline |
| `packaging/debian/openhover-deploy` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | restricted race server deployment wrapper |
| `packaging/debian/openhover-gitlab-runner.sudoers` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | deployment wrapper authorization |
| `packaging/debian/postinst` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | Debian package post-installation script |
| `packaging/debian/postrm` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | Debian package post-removal script |
| `packaging/debian/prerm` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | Debian package pre-removal script |
| `packaging/systemd/openhover-raceserver.service` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | dedicated race server systemd unit |
| `scripts/install-race-server.sh` | Original | Maintainer, with AI assistance (GitHub Copilot) | MIT OR Apache-2.0 | race server package installation helper |

## Not carried over, on purpose

Everything else in the Classic repository stays there. It falls into one of: HoverRace
source, assets, or tracks (licensed or of unrecorded origin); code written against the
HoverRace engine's types (which cannot be separated from it); third-party code with its own
licence; documents that describe the original's internals; and files specific to the
Classic product. The Classic repository keeps a full per-file classification
(`docs/provenance-audit.csv`, produced by `scripts/audit-provenance.py`) so any further
file can be reviewed individually before it is considered.

## Incidents

None recorded.
