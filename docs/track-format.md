# OpenHover track format v1

OpenHover v1 tracks are original data packages with explicit provenance. A track defines a
closed ordered route. The first waypoint is the finish gate. Route segments connect each
waypoint to the next, including the last back to the finish. Each track separately declares
four ordered race checkpoints at intentional course landmarks; they control lap validation and
are not inferred from route geometry. Each checkpoint and finish line spans the full road width
from wall to wall.

Required fields are an identifier, display name, format version `1`, provenance metadata, at
least four waypoints, exactly four checkpoints, positive gate radii, and a positive road
half-width. Boost pads are optional but must have positive radii. The game validates these
conditions before loading a track.

Validation also checks route geometry: consecutive waypoints must be distinct, every checkpoint
must lie within the road half-width of the route, and checkpoints must follow the route in driving
order starting from the finish. A checkpoint placed exactly on the finish point counts as the end
of the lap. Boost pads, hazard zones, and mines must also lie on the road, and wherever the route crosses
itself a driveable raised section must cover the crossing point. The nine-slot starting grid at the finish line must lie on the road
and stay at least two units clear of mines and hazard zones. A track that fails any check is
rejected with a message naming the problem.

Track provenance contains the author, licence, and source-asset origin. Each distributed package
must additionally record its content hash in package metadata before it can be accepted into an
OpenHover release. Built-in tracks are compiled definitions and therefore have no package hash.

## Track file text format

Tracks can be written as plain text and checked with `OpenHoverTrackCheck <file> [...]`, which
prints `ok` or the first problem for each file and exits non-zero if any file fails. The first
line is `openhover-track 1`. After that, one directive per line; blank lines and lines starting
with `#` are ignored.

| Directive | Arguments |
| --- | --- |
| `id`, `name`, `author`, `license`, `origin` | the rest of the line (text) |
| `road-half-width` | width |
| `atmosphere`, `road-colour`, `wall-colour` | red green blue, each 0 to 1 |
| `waypoint`, `checkpoint`, `boost-pad`, `mine` | x y radius |
| `hazard` | x y radius speed-loss-per-second |
| `raised` | x y half-length half-width heading clear-height driveable (0 or 1) |

Waypoints and checkpoints are listed in driving order, starting with the finish. A file that
parses can still fail validation; the checker runs both, so every rule above applies.

## Playing a custom track

Copy a valid `.ohtrack` file into the `tracks` folder inside OpenHover's per-user data folder (on
Linux, `~/.local/share/OpenHover/OpenHover/tracks/`; SDL reports the equivalent folder on other
systems). At startup the game loads every file ending in `.ohtrack`, in name order, up to 32
files of at most 1 MiB each. A file is skipped, with the reason printed to standard error, if it
does not parse, fails validation, or reuses an `id` or `name` already used by a built-in or
earlier custom track. Custom tracks appear after the built-in ones in the local race track
list. They are not available in championships. To race one online, host a room with it: the
room carries the track and other players download it automatically; see
[multiplayer](multiplayer.md#custom-tracks-online). Tracks downloaded that way are saved in
`tracks/downloaded` and appear in your own track list.

## Track editor

The main menu's **Track Editor** builds a track without writing a file. Click the map to add corner
points in driving order (the grid snaps to 10 units); drag a point to move it; right click a point
to delete it; Backspace or **Undo Point** removes the last one. **Narrower** and **Wider** change the
road. Click the **Name** button to type a name (letters, digits, space, `.`, `_`, `-`, up to 24).

From the points, the editor builds a complete track exactly as a hand-written one would be, then
validates it with the normal rules: it starts the lap at the middle of the longest side so the grid
has a straight to sit on, picks four corners spread around the lap as checkpoints, adds a driveable
bridge where two roads cross (crossings shallower than 25 degrees are refused), and places boost
pads in the middle of straights longer than 80 units. The status line shows whether the track is
valid, its length, and a rough lap time at about 30 m/s; the green square is the start, yellow
squares are checkpoints, orange squares are bridges, and blue dots are pads.

**Save Track** writes `<id>.ohtrack` to the `tracks` folder and adds the track to the local race
list immediately. The id comes from the name, and a name that is already used by another track is
refused. **Save and Drive** does the same and then starts a local race on the track so you can try
it; pause and choose Main Menu to return, and the editor keeps your points. Saving again after
more edits replaces the track you saved before.
