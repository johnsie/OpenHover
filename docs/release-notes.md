# Release Notes

Player-facing changes and known limitations. Each release names what changed for players, not
internal refactors. See the [roadmap](roadmap.md) for what is planned next.

## 0.1.20

- The track editor can now add **mines** and **hazard zones**. Two toggle buttons, Mines and
  Hazards, scatter them along long straights (clear of the start, the bridges, and the boost pads);
  the preview shows mines in red and hazards in purple, and the built track is validated and can be
  hosted online like any other.
- The pause menu now has the **HUD Text** setting too (between Camera Motion and Menu Volume), so
  the accessibility options (camera motion, HUD text size, assists, volume, key bindings) can all
  be changed without leaving a race. The pause menu rows are a little closer together to make room.

## 0.1.19

- The online race HUD no longer draws the leaderboard on top of the course map: the leaders list
  now sits below the map and the place readout, and an unused panel behind the map is gone.
- New **HUD Text** setting (Normal or Large) on the Settings screen. Large makes the race
  readouts bigger: speed, distance to the next gate, lap, time, lap time, best, split, and place in
  local races, and the equivalent lap, time, speed, and lap-time readouts in online races. The
  choice is saved. Other text (menus, chat, the leaderboard) is unchanged.

## 0.1.18

- Small windows and layout fixes. On a window about 600 pixels tall (for example 1024 by 600) the
  main menu no longer overlaps the title or the key hints, the local race setup rows tighten up so
  Weapons and Back clear the hints, How To Play no longer runs its last two lines together, the
  multiplayer lobby's action buttons and status are no longer clipped, and the Host Race panel stays
  on screen. The Settings screen is redesigned to match the other menus: it fixes camera distance
  choices that overlapped each other ("STANDARD" and "FAR") and the Back row sitting under the key
  hints, at every window size. Normal-size windows are otherwise unchanged. To check a size
  yourself, start the game with `OPENHOVER_WINDOW=1024x600`.
- Crash reporting. If the game crashes it writes a small plain-text report (game version, what
  went wrong, and a stack trace on Linux; no personal information) to `crash.log` in its data
  folder. On the next start the report is kept as `last-crash.log` there, printed to the terminal,
  and the main menu says the game closed unexpectedly, so you have something to send with a bug
  report. (The race server does not write one yet; its log is in the service journal.)
- Each rival name now drives its own craft class, so rivals look and handle differently: Optimus
  Prime, Tiktok and Cooper drive Sprint craft, Data and Kryten drive Control craft, and Anne
  Droid, Davros and Roomba drive Balanced craft. It is the same in local and online races.
- AI rivals no longer get lost. Besides recovering when stuck in one spot, a rival that makes no
  forward progress along the road for eight seconds (bouncing between walls, circling a corner)
  now puts itself back on the road and re-aims at the next waypoint ahead. In about 750 test races
  with weapons, none ended with a rival unable to finish; before the fix roughly one race in sixty
  had one.
- Craft classes are rebalanced. Measured with the AI driver on every track, **Control** had been
  25 to 30 percent slower than Balanced, which made it a trap rather than a choice. Control now
  has a higher top speed (33) and acceleration, and Sprint a slightly lower top speed (38), so the
  AI laps about 4 to 9 percent faster in Sprint and 7 to 12 percent slower in Control than in
  Balanced, with Control keeping its sharper turning for human drivers. An automated test keeps the
  classes within those bands. Saved ghosts from earlier versions are discarded once, because the
  handling they were recorded with changed.
- Championship standings now break ties fairly. Competitors level on points are ordered by where
  they finished the latest event, so finishing last no longer ranks you level with fourth place
  (the panel previously showed "4TH" after a last-place finish). The top-three line follows the
  same order.
- The championship results panel now explains itself: the event number, the top three by points
  using the rivals' names, your own place and points with what this event added, and what comes
  next (the next track by name, the final results, or "series complete"). The panel is taller to
  make room.

## 0.1.17

- AI rivals now use weapons: on Standard and Expert difficulty a rival fires a missile at any craft
  lined up ahead of it within about 55 metres, whether that is you or another rival (Relaxed
  rivals never fire). Missiles recharge as usual, and nothing changes when weapons are off. This
  applies to local and online races alike.
- AI rivals now recover on their own. A rival that stays stuck in one place for three seconds, for
  example wedged against a wall after a collision or a missile hit, puts itself back on the road
  facing the right way. Previously such a rival could sit there for twenty seconds or more.
- While a ghost races alongside you, the HUD shows how far you are ahead of or behind it along the
  road, for example "12 M AHEAD OF GHOST" (green) or "40 M BEHIND GHOST" (orange). It follows the
  road across laps and at bridges, so it never jumps where two roads cross.
- **Alt** now cycles the ghost between your best run, your last run (this session, on the same
  track and craft), and off, and shows which is active; the choice is saved. Best and last switch
  at the next race start, off takes effect at once. The results panel names the ghost you raced.
- New **Track Editor** on the main menu. Click on the map to place corner points (drag to move, right
  click to delete, Backspace or Undo to remove the last one). The editor turns your points into a
  complete track as you go: a start straight on the longest side, four checkpoints, a bridge at
  every crossing, and boost pads on long straights, and it shows the result, an estimated lap
  time, and any problem (for example "ADD AT LEAST 5 POINTS") live. Pick a name and road width and
  press Save; the track appears in Play Local Game and can be hosted online like any other custom
  track. See [track format](track-format.md#track-editor). **Save and Drive** saves the track and
  starts a local race on it at once; saving again after more edits replaces your earlier version.

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
- Colour and contrast options are not available yet; HUD text can be made larger, but menu and chat text cannot.
- The ghost replays inputs only, so a run disturbed by rival collisions or missile hits can drift
  from its recording.
- Rival names are shown in local results and the online leaderboard but not elsewhere.
