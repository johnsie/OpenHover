# OpenHover Project Guidelines

## Online And Offline Parity

Online racing and offline racing must provide equivalent gameplay rules and player-facing
features. When a change adds or changes an offline race feature, implement the corresponding
authoritative server behavior, client replication, rendering, HUD feedback, and results behavior
in the same change. Treat any intentional difference as an exception: document its reason,
player impact, and follow-up work before merging.

Validate parity with focused coverage for both paths whenever a shared race rule, track effect,
craft behavior, race mode, countdown, assist, weapon, or result changes.