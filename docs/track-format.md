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
| `ground-height` | metres; one line per waypoint, in waypoint order (leave all out for a flat track) |

Waypoints and checkpoints are listed in driving order, starting with the finish.

Ground height: with `ground-height` lines the floor changes level in steps, never slopes. Each line
gives the height of the stretch of route from that waypoint to the next; the ground is level along it
and steps at the waypoint, across the road. The craft hovers 1.2 above the ground under it (bridges
and hazards measure from the ground too). A step down is a drop: the craft leaves the ground and falls
under gravity to the lower floor. A step up has to be jumped: the craft never rises by itself, so a
step of more than a quarter metre stops a craft that is not high enough above it (it bounces back). A
jump rises about 2 metres and stays up for just under a second, so how far it carries depends on
speed: about 34 metres at full speed, 22 metres at two thirds. Pits and gaps are only cleared with a
run-up, and a step up of more than about 2.1 metres cannot be jumped at all: that is a trap. A craft stopped by a step can use recovery, which puts it back on the
road at the higher ground for another run. Rivals jump steps and pits on their own; the player's
craft only jumps when the player presses jump.
Rules: one height per waypoint, heights within 60 metres of zero, no step within reach of the
starting grid, and where the route crosses itself both roads must be at the same height (the bridge
carries one over the other). Steps and drops can be any size. Tracks with no `ground-height` lines
are flat, as before; an older game that does not know the line refuses the track instead of racing it
flat. A file that
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

The **Mines** and **Hazards** buttons add optional obstacles: on straights of at least 110 units
that have no pad or bridge nearby, a third of the way along, the builder places a mine (radius
2.1) or a hazard zone (half the road wide, slowing a craft by 0.75 per second). With both on they
alternate; at most eight are placed. Red squares are mines and purple squares are hazard zones.

**Save Track** writes `<id>.ohtrack` to the `tracks` folder and adds the track to the local race
list immediately. The id comes from the name, and a name that is already used by another track is
refused. **Save and Drive** does the same and then starts a local race on the track so you can try
it; pause and choose Main Menu to return, and the editor keeps your points. Saving again after
more edits replaces the track you saved before.

## Converting room-based tracks

`OpenHoverTrkConvert` is a separate application that converts a track from another hover racing
game (a room-based `.trk` file) into an `.ohtrack`, and shows what it will write. **Only convert
tracks you have the rights to**: this repository's clean-room policy does not allow importing
anyone else's track data.

Start it with a file (`OpenHoverTrkConvert file.trk`) or drop a file onto its window. The window
shows every room it read, coloured by floor height, with the original starting positions as green
dots. A light-blue loop is the route it chose, a translucent band shows the real road width, and the
usual markers show the start, gates, bridges and pads of the track that will be written, with an
arrow for the direction of travel. The panel on the right lists what could not be carried over.

- Click a room to switch it off or on (for example a side lane the route should avoid); the loop is
  recalculated at once. `F` drives the loop the other way round, `R` switches every room back on,
  `N` renames the track, and `S` saves it into the game's own `tracks` folder. Every action also
  has a button at the bottom of the panel.
- The **boost pads**, **mines** and **hazard zones** buttons (`P`, `M`, `H`) choose which of those the
  builder adds to long straights, as in the track editor. Pads are on by default, mines and hazard
  zones off. Red squares are mines and purple squares are hazard zones.
- With `--out file.ohtrack` it runs without a window: `OpenHoverTrkConvert file.trk --out out.ohtrack
  [--name NAME] [--exclude ROOM]... [--reverse]`. Mines and hazards are only available in the window.

How it works: rooms that share part of an edge are linked; a loop through the start room is found
(never doubling straight back on itself); the loop's room centres and doorways become the route;
the road width follows the narrowest quarter of the doorways; and the track is finished by the same
builder as the track editor (start straight, four gates, bridges, pads) and then validated. Each room's floor height becomes the ground height of the route stretches through it (measured
from the start room, in steps of a quarter metre), and the ground steps at each doorway where the floor
does, at its real size. A room whose floor is lower than the road either side of it becomes a pit; if
its far wall is too high to climb out of, the converter warns that it is a trap. If the heights do not
fit the route (for example two crossing roads at different levels) the track is made flat with a
warning. The **ground
heights** button (`G`) switches this off.

Limits: it understands only the one layout it was written from and refuses anything else with a
reason. Ceilings and stacked rooms are not converted, side lanes and branches off the loop are left out,
and where the original road is wider than one lane the result may need a lane switched off or some
editing afterwards. The converted track starts on a long straight near the original start, which
can be a little way from it.
