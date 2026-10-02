# OpenHover track format v1

OpenHover v1 tracks are original data packages with explicit provenance. A track defines a
closed ordered route, where the first waypoint is the finish gate and each later waypoint is a
checkpoint. Route segments connect each waypoint to the next, including the last back to the
finish.

Required fields are an identifier, display name, format version `1`, provenance metadata, at
least four waypoints, positive gate radii, and a positive road half-width. Boost pads are
optional but must have positive radii. The game validates these conditions before loading a
track.

Track provenance contains the author, licence, and source-asset origin. Each distributed package
must additionally record its content hash in package metadata before it can be accepted into an
OpenHover release. Built-in tracks are compiled definitions and therefore have no package hash.