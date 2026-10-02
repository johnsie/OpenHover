# Release Notes

Player-facing changes and known limitations. Each release names what changed for players, not
internal refactors. See the [roadmap](roadmap.md) for what is planned next.

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
