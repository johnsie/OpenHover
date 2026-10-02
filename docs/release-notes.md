# Release Notes

Player-facing changes and known limitations. Each release names what changed for players, not
internal refactors. See the [roadmap](roadmap.md) for what is planned next.

## 0.1.16

- Online custom tracks now travel with the room. A host picks any of their tracks when creating a
  room; the game uploads it, the server checks it and holds it only while the room exists, and
  players who join download it automatically. Everyone races on a track whose SHA-256 hash matches
  the room's: the server will not ready a player, or start the race, until each player has
  verified the identical file. Downloaded tracks are saved in `tracks/downloaded` on your computer
  (up to 64) and appear in your track list. Server operators install nothing. Uploaded tracks must
  use plain-text names and stay within size and range limits (see
  [multiplayer](multiplayer.md#custom-tracks-online)). Championships still use the built-in
  tracks. The protocol is now version 7, so update the client and server together. This replaces
  the `--tracks` server folder from 0.1.15. `OpenHoverTrackCheck` is now included in the desktop
  and race-server packages.

## 0.1.15

- Tracks can now be written as text files and checked with the new `OpenHoverTrackCheck` tool;
  see `docs/track-format.md`. Put valid `.ohtrack` files in the `tracks` folder next to your
  settings (on Linux, `~/.local/share/OpenHover/OpenHover/tracks/`) and they appear after the
  built-in tracks in the local race setup. Invalid files are skipped with a reason printed to the
  terminal. Custom tracks can also be raced online, with the exact file checked: see below.
- Online custom tracks. A race server started with `--tracks <folder>` also hosts the valid
  custom tracks in that folder. Each track is identified by a SHA-256 hash of its canonical text,
  so players need the identical track, not just one with the same name. The server publishes the
  list with hashes; your game tells the server which custom tracks you have; and the server
  refuses to create, join, or switch to a room whose track you do not have byte-for-byte. A room
  on a track you lack shows "YOU NEED THIS TRACK FILE". Track ids and names must be plain text
  (letters, digits, spaces, `.`, `_`, `-`, up to 24 characters) to be hosted. Championships still
  use the built-in tracks. This changes the protocol (now version 6), so update the client and
  server together. Set `OPENHOVER_SERVER=host:port` to point the game at another server.
- Press **Alt** during a local race to show or hide your best-run ghost; the choice is saved.
- Recovering to the road now always faces you along the route. It used to aim at the next gate,
  which on a winding track could point into a wall or back the way you came. Local and online
  races share this behaviour.
- How To Play now shows your current key and gamepad bindings (including any remapping), the
  pause and Key Bindings shortcut, and points to the flashing wall arrows.
- All on-screen text now has a dark drop shadow, so it stays readable over bright road, sky, and
  walls. The fuel and missile bars moved below the lap-progress squares they used to overlap, and
  their labels no longer run into the bars.
- AI rivals now steer around craft ahead of them, including the player and each other, instead of
  driving through them, and ease off slightly when very close. Looking distance grows with speed.
  This applies to local and online races alike. A full grid of seven rivals still finishes every
  track, with a more natural spread of finishing times.

## 0.1.14

### Tracks

- All three tracks are much longer: a full-pace rival now takes about 60 seconds per lap (it was
  18 to 24 seconds). Harbor Loop gains chicanes on its long straights, and Glass Switchback and
  Velocity Ring gain extra boost pads, mines, and hazards. This changes built-in track content, so
  online compatibility moves to content version 2: update the client and server together.
- Large chevrons on both walls, flashing between yellow and magenta, point the way round the
  course, so you can always tell which direction to drive if you get turned around. Reduced
  camera motion keeps them steady yellow. Every race now starts on a straight with the
  first corner ahead, rather than a few metres from a corner.

### Controls and accessibility

- Keyboard keys for accelerate, brake, steer left/right, fire, and recover can be remapped from
  **Key Bindings** in the pause menu. Gamepad jump, fire, and recover buttons can be remapped
  there too. Arrow keys, Shift, and Start/B/A/D-pad menu navigation always keep working.
- A gamepad can now operate every menu, the pause menu, and the results panel: D-pad moves, A
  confirms, B goes back, Start pauses.
- New **Camera Motion** setting: Standard, Smooth (the camera follows turns with a short lag), or
  Reduced (no speed-based field-of-view change).
- The chase camera now rises with the craft over jumps and raised sections.
- A short practice flow teaches steering, checkpoints, boost pads, jumping, recovery, and weapons.

### Racing

- AI rivals have names drawn at random for every race from Anne Droid, Data, Optimus Prime,
  Davros, Tiktok, Kryten, Roomba, and Cooper; no two rivals in a race share a name.
  Results and the online leaderboard show them, and a rival win names the winner.
- The ghost craft now replays your best completed run for the track and craft class, not your
  last attempt. A faster finish replaces it and the results panel shows the ghost time you were racing ("GHOST BEST") and "NEW GHOST SAVED" when you beat it. Runs
  that used recovery never become a ghost, because inputs alone cannot reproduce them. Ghosts are
  saved between sessions and are discarded automatically when the built-in track content or
  handling physics changes.
- The local results panel adds a **Setup** action beside Restart/Next Event and Main Menu.

### Online

- If an established connection drops, the client reconnects automatically (1, 2, 4, 8, then 15
  seconds) and shows the countdown. A reconnect starts a new lobby session.
- Chat is rate limited, players can `/MUTE` and `/UNMUTE` others, and `/REPORT` sends a moderation
  report. The server limits simultaneous connections and oversized peers. See
  [multiplayer](multiplayer.md) for the exact limits.

### Known limitations

- A reconnect does not return you to your room or an aborted race.
- Online recovery uses F2 and online movement keys are arrows and Shift only; WASD remaps apply to
  local races.
- Gamepad steering and throttle (stick and triggers) cannot be remapped, and the race HUD does not
  yet show gamepad button prompts.
- Colour, contrast, and text-scale options are not available yet.
- The ghost replays inputs only, so a run disturbed by rival collisions or missile hits can drift
  from its recording.
- Rival names are shown in local results and the online leaderboard but not elsewhere.
