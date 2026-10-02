# OpenHover roadmap

This is an original delivery plan for OpenHover. It uses the intended product direction of
HoverNet-style hovercraft racing as inspiration: fast controllable craft, competitive circuit
racing, distinct courses, accessible multiplayer, and a durable track ecosystem. It does not
reuse source, assets, track data, or text from HoverNet Classic or HoverRace.

Before a milestone adopts a specific legacy behaviour, record it as a black-box player-facing
requirement and test it under the [clean-room policy](clean-room-policy.md). Product research
may describe observable features; implementation work must remain independent.

## Product pillars

- **Responsive hover racing:** readable handling, boost management, collision feedback, and
  recovery that keeps players in the race.
- **Competitive racing:** checkpoints, laps, rivals, a clear race result, and fair course rules.
- **Courses with character:** original tracks built around alternate lines, elevation, hazards,
  boosts, and strong visual landmarks.
- **Race together:** local play first, then network racing built on deterministic simulation and
  replay validation.
- **Open creation:** documented original track formats, safe sharing, and clear provenance for
  every community contribution.

## Visual development plan

The current SDL/OpenGL presentation is a functional prototype, not a visual target. Do not treat
new HUD bars, colours, or isolated primitive shapes as a substitute for a coherent track scene.
Visual work proceeds in this order:

1. **Connected circuit geometry:** replace independent road strips with joined road meshes,
   consistent corners, continuous wall extrusion, reliable track-edge collision, and no visible
   gaps, floating markers, or overlapping panels.
2. **Renderer baseline:** establish original materials, directional lighting, controlled fog,
   terrain/sky separation, depth cues, and stable chase-camera framing before visual polish.
3. **Original modular assets:** author a coherent hovercraft family, road modules, wall modules,
   route markers, gates, boost pads, finish treatment, and landmark props. Every asset must have
   recorded origin and licence; reference images only inform high-level observable needs such as
   a readable skirted craft silhouette, enclosed cockpit, and visible propulsion hardware.
4. **Track art direction:** give each original course a distinct environment, palette, skyline,
   terrain treatment, landmarks, and route story. Road width, wall contrast, and turn cues must
   support speed and decision-making rather than decoration alone.
5. **Visual review gates:** capture desktop screenshots and gameplay clips for each track after
   every major rendering change. Fix geometry seams, weak silhouettes, unreadable turns, HUD
   occlusion, and poor contrast before adding the next layer of polish.

The goal is not to reproduce another game's artwork, layouts, or assets. It is to reach an
original, professional racing presentation with the same player-facing fundamentals: coherent
world scale, readable boundaries, visible route intent, and a craft that feels physically present.

## Milestone 0: Core race loop

**Goal:** make the current prototype consistently fun for a first-time player.

- Move simulation to a fixed timestep and add deterministic input recordings.
- Tune acceleration, turning, drift, boost drain, boost pads, collisions, and off-road recovery
  against player-facing tests.
- Add a finish/results screen with lap times, restart, and race settings.
- Add a persistent race HUD with a compact course map, next-gate direction, position, speed,
  lap state, and clearly separated resource/status meters.
- Add current-lap, best-lap, and sector/split timing so each run gives immediate performance
  feedback, including a concise finish summary.
- Make speed and route choice legible with broad road surfaces, high-contrast guard walls,
  corner-direction markers, and unambiguous wrong-way feedback.
- Support a stable low chase camera and optional camera-distance setting so the craft, road edges,
  and approaching corners remain readable at racing speed.
- Add original engine, boost, collision, checkpoint, and menu sound effects.
- Establish frame-time, startup-time, and gameplay regression checks.

**Exit criteria:** a three-lap race is playable with keyboard and gamepad, can be restarted
without restarting the program, and has deterministic smoke coverage for every race rule.

## Milestone 1: Tracks and race modes

**Goal:** turn the arena into a small racing game rather than a single demonstration circuit.

- Define a versioned, documented OpenHover track format with validation and provenance metadata.
- Build three original tracks: a beginner circuit, a technical circuit, and a high-speed course.
- Add track-select, lap-count, rival-count, and difficulty options.
- Add time trial, free practice, single race, and championship events.
- Add track features such as banked turns, jumps, shortcuts with trade-offs, boost routes, and
  recoverable hazards.
- Use contrasting original road, water, hazard, and restricted-route surface treatments to make
  safe lines, penalties, and alternate routes readable before the player reaches them.
- Add physical route control: strong guard walls, blocked-route panels, finish-zone markings,
  and clear entry/exit cues at every track junction.
- Give every track an original environmental identity through its skyline silhouettes, sky,
  landmark props, road treatment, lighting, and audio ambience without importing legacy art.
- Make each course readable at speed with a large always-visible course map, racer markers,
  next-gate direction, repeated turn markers, and a distinct finish-zone treatment.

**Exit criteria:** every mode completes cleanly on every bundled track, and track loading rejects
invalid or unrecorded content with actionable errors.

## Milestone 2: Competitive depth

**Goal:** make repeated races strategically interesting and legible.

- Add multiple AI personalities with tunable pace, overtaking, recovery, and boost use.
- Add starting grids, race position calculation, sector/split timing, and standings.
- Expose rival position and event state through the HUD and results screen without obscuring the
  driving view; use concise symbols, colour, and spatial layout rather than dense text.
- Add optional assist settings for steering, braking, and recovery.
- Add a small roster of original craft classes with distinct handling and readable silhouettes.
- Add replay recording and playback from deterministic input streams.
- Run structured playtests and maintain a public balancing changelog.

**Exit criteria:** an event with a full grid produces stable placements, competitive AI, and a
replay that agrees with the recorded result.

## Milestone 3: Local and online racing

**Goal:** support shared competitive play without compromising race integrity.

- Add split-screen local multiplayer with controller assignment and per-player HUDs.
- Design a versioned network protocol around inputs, simulation ticks, and authoritative results.
- Add private online lobbies, ready checks, disconnect handling, and race result synchronization.
- Add ghost sharing for time trials before ranked or public matchmaking.
- Add latency simulation and network regression tests before public online play.

**Exit criteria:** two players can finish a complete race locally and over a supported network path
with agreed results and recoverable disconnect behavior.

## Milestone 4: Creation and release

**Goal:** make OpenHover a maintainable, shareable game project.

- Ship a track editor or documented creator workflow with validation tooling.
- Add signed or hash-verified track packages, clear licences, and contributor provenance checks.
- Package Linux builds first, then add other supported platforms with controller and renderer
  verification.
- Add accessibility settings for input remapping, colour, camera motion, audio, and subtitles.
- Publish release notes, credits, third-party notices, and an asset provenance audit for every
  release.

**Exit criteria:** a new contributor can build the game, create a valid original track, and share
it with an unambiguous licence and provenance record.

## Ordering rules

- Finish a playable, tested core race before adding online features or creator tools.
- Prefer original implementations and observable behavior specifications over compatibility work.
- Add no dependency, asset, track, or document without satisfying the repository's licensing and
  provenance requirements.
- Revisit scope after each milestone using playtest evidence, stability data, and contributor
  capacity rather than a fixed calendar.