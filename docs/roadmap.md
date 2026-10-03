# OpenHover Roadmap

OpenHover is an original hover-racing game built around clear, satisfying races that players can
start quickly, understand immediately, and want to run again. This plan starts from the current
playable build, not from an empty prototype. It does not reuse source, assets, track data, or text
from HoverRace or HoverNet Classic; all future work follows the [clean-room policy](clean-room-policy.md).

## Current Baseline

The game already has a fixed-step hovercraft simulation; three original tracks; sealed route
crossings; laps, checkpoints, timing, positions, results, championships, AI rivals, craft classes,
boost pads, hazards, raised sections, recovery, driving assists, missiles, mines, replays, gamepad
input, and procedural audio. Local races and authoritative online races share the same core rules.
Online play includes a public room lobby, host settings, countdowns, interpolation, chat, race
results, and a deployed server.

This baseline is an inventory, not a claim that any system is finished. Every existing feature can
be improved when testing, play feedback, accessibility review, performance data, or online/offline
parity exposes a better player experience. The next work is not to add features indiscriminately;
it is to make the existing game easier to read, easier to control, more reliable with other
players, and more rewarding from the first race through a complete event.

## Product Rules

- **Drive first:** a player should reach a race in a few clear choices, with no unexplained state
  or hidden control.
- **Read the road:** road edges, gates, hazards, rivals, and the next decision must remain clear
  at racing speed. A route may never depend on a player guessing where they are allowed to drive.
- **Fast recovery:** a mistake should cost time and create tension, not leave the player confused,
  trapped, or waiting.
- **Useful feedback:** HUD, sound, camera, and results should answer what happened and what to do
  next without crowding the driving view.
- **Parity is mandatory:** online and offline racing must offer equivalent rules and player-facing
  feedback. Any exception needs documented player impact, follow-up work, and focused coverage.
- **Original and maintainable:** new code, assets, tracks, and dependencies require provenance and
  must strengthen the game rather than add a one-off demonstration.

## Milestone 1: Make Every Lap Feel Good

**Goal:** turn the current race into a polished, learnable driving experience.

- Review every track at racing speed for wall gaps, camera clipping, ambiguous turns, gate approach
  space, spawn safety, recovery placement, and route shortcuts that bypass intended progression.
- Replace disconnected road and wall pieces with continuous corner geometry where it improves
  collision reliability and makes the legal route obvious before the player reaches it.
- Tune acceleration, turning, drift, jump, boost pads, wall rebound, hazards, mines, and recovery
  using recorded runs and targeted playtests; publish the resulting tuning principles.
- Improve the chase camera: stable framing through turns and jumps, configurable distance and
  motion comfort, and no HUD or geometry obscuring the craft or next corner.
- Make the active route unmistakable with a compact course map, a next-gate cue, clear wrong-way
  feedback, and distinct visual treatment for finish, checkpoint, boost, hazard, and raised route.
- Add a short in-game controls and practice flow that teaches steering, boost pads, jumping,
  recovery, checkpoints, and weapons through play rather than a dense text screen.
- Audit keyboard and gamepad bindings; support remapping, sensible defaults, device prompts, and
  pause-menu access to controls and accessibility settings.

**Exit criteria:** a new player can finish every bundled track without outside help, understands
why a lap did or did not count, and can recover from an error within a few seconds.

## Milestone 2: Improve Race Flow And Competition

**Goal:** make setup, racing, results, and the next race feel like one coherent experience.

- Simplify local race setup around track, mode, laps, rivals, class, assists, and weapons; keep
  descriptions concise and expose only settings that change the selected mode.
- Give each craft class a genuinely distinct role, silhouette, handling explanation, and balanced
  use case. Validate its performance against AI and player recordings on every track.
- Improve AI racing lines, overtaking, avoidance, weapons, recovery, and difficulty scaling so a
  full grid feels competitive without relying on rubber-banding or erratic collisions.
- Expand results into a readable event summary: finishing order, lap and split improvement, class,
  championship points, rematch, next event, and return-to-setup actions.
- Make championship progression explain itself before and after every event, including standings,
  points changes, the next track, and a clear completion screen.
- Turn replay recording into a player feature with ghost selection, best-lap comparison, playback
  controls, and reliable invalidation when the relevant physics or track version changes.
- Add a small, deliberate audio pass for engine load, boost, landing, walls, gates, rival contact,
  results, and menu focus; include independent volume controls and visual alternatives.

**Exit criteria:** a player can run a complete local championship, understand each result, choose a
meaningful rematch or next action, and see a clear reason to improve their next lap.

## Milestone 3: Make Online Racing Trustworthy

**Goal:** make joining and completing an online race as understandable and dependable as local play.

- Improve lobby UX with connection state, server version, player presence, room capacity, host
  status, selected track preview, race settings, readiness, and clear join/leave/start feedback.
- Add protocol and content compatibility negotiation before joining a race; show an actionable
  update or missing-track message instead of accepting an incompatible client.
- Add reconnect and graceful disconnect behavior, including a clear race outcome, host migration
  rules, timeout feedback, and a return path to the lobby.
- Test latency, packet loss, delayed snapshots, reconnects, server restarts, and mixed input rates
  with automated integration tests and player-visible error states.
- Preserve parity for every shared race change: server authority, replication, interpolation,
  rendering, HUD feedback, audio cues, results, and focused offline/online coverage must land
  together.
- Provide a private-room path before matchmaking, then add lightweight invitations or room codes
  only after the core join flow is robust.
- Define an abuse and privacy baseline: name rules, chat reporting/muting, rate limits, transport
  security, server observability, and a documented compatibility policy.

**Exit criteria:** two players on normal home connections can create, join, race, finish, rematch,
and recover from a short interruption with agreed authoritative results and no unexplained state.

## Milestone 4: Give Tracks Identity And Depth

**Goal:** make each course memorable, readable, and worth replaying.

- Establish a cohesive original art direction for road surfaces, walls, terrain, sky, lighting,
  landmarks, gates, pads, hazards, and craft materials. Replace prototype primitives where they
  interfere with readability or sense of speed.
- Give each bundled track a distinct racing lesson and visual identity: an approachable flow track,
  a technical precision track, and a high-speed risk/reward track.
- Add alternate routes only when their entry, exit, risk, and reward are readable at speed and
  validated by timing data. Avoid shortcuts that create ambiguous lap validation.
- Use elevation, jumps, hazards, pads, and weapons deliberately, with safe sight lines and a
  recoverable outcome for every failed attempt.
- Floor height is in the track format (level stretches with drops to fall off and steps up to jump,
  drawn and validated the same way online and offline); review it on real tracks for sight
  lines and recoverable landings, and add editor controls for it. Follow it with
  **ceilings** (a low roof that limits jump height and is visible in the road geometry) and
  **stacked rooms** (one road passing over or under another at a different level, replacing the
  bridge-only crossings), including the room-based track converter reading those values. Each step
  needs a safe sight line, a recoverable failure, and a bump to the track content version.
- Add screenshot and gameplay-review gates for desktop and common low-resolution layouts; fix
  overlap, contrast, framing, and unreadable turn problems before expanding content.
- Build additional tracks only after the first three meet their readability and replayability
  targets. Each new track needs an authored route review, race recordings, and provenance.

**Exit criteria:** each bundled track is visually distinct, has a clear racing identity, and earns
repeat runs because players can see and learn its decisions.

## Milestone 5: Release-Ready Foundation And Creation

**Goal:** make the game safe to distribute, support, and extend without weakening the race.

- Bring the README, multiplayer guide, controls, and release notes in line with the implemented
  game; remove claims that describe already-shipped systems as future work.
- Complete Linux release polish, crash/error reporting, stable preferences, clean first-run setup,
  and verification on the supported controller and renderer combinations.
- Add accessibility options for remapping, colour and contrast, text scale, reduced camera motion,
  audio mixing, and non-audio race feedback.
- Evolve the documented track format and validation tools into a creator workflow. Validate route
  continuity, sealed crossings, gate placement, spawn safety, content provenance, and version
  compatibility before a track can be shared.
- Integrate the existing catalogue and verified downloader into player-facing track browsing only
  after clear source, licensing, update, compatibility, and offline behavior are in place.
- Maintain build, smoke, integration, performance, deployment, and provenance checks; publish
  concise release notes that name player-facing changes and known limitations.

**Exit criteria:** a first-time player can install, configure, play, and update the game without
manual intervention, while a contributor can build and validate an original track with clear rules.

## Planning Discipline

- Treat every shipped system as improvable. Preserve what works, but revisit controls, rules,
  presentation, performance, and flow whenever evidence shows that players are confused or a
  clearer, fairer, more accessible experience is possible.
- Fix confusion and reliability before adding breadth. A feature that obscures the route, breaks
  parity, or complicates race flow does not advance the roadmap.
- Every milestone item needs an observable player outcome, a focused validation plan, and a clear
  owner before implementation starts.
- Use playtest evidence, telemetry, recordings, and support issues to reprioritize quarterly.
- Keep changes small enough to verify locally and online; do not merge a shared race rule with one
  path unimplemented.