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