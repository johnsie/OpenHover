# OpenHover

A hovercraft racing game, written from scratch and free of the HoverRace licence.

**Status: just started.** This repository is new and intentionally empty. It has no
history from, and contains no code or assets copied from, the licensed HoverNet /
HoverRace project (kept separately as "HoverNet Classic").

## Ground rules

- Nothing is added unless its origin and licence are known and recorded.
- Code, textures, models and sounds are created new, or come from sources whose
  licences are written down in the provenance ledger.
- No licensed HoverRace source or asset is copied in, and the licensed project's code
  is not consulted while writing replacements.

## What is here so far

Small, independent building blocks that were first written in the licensed project but use
none of its code: SHA-256, a gzip decoder, wire-format helpers, a track catalogue, and a
verified downloader, each with tests. The project also has an original 3D SDL2/OpenGL test
arena with hovercraft controls, checkpoints, and lap timing. Build and test with:

![Harbor Loop race start](assets/screenshots/harbor-loop-gameplay.png)

```
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

Run `./build/OpenHover` to drive the arena. Select **Play Local Game** from the main menu, choose
the original track, lap count, and weapons setting, then select **Start Race**. A three-light
countdown begins before the race. Use `W`/`S` or Shift/Down to accelerate and reverse, `A`/`D` or
Left/Right to steer, Ctrl to boost, Up to jump, and Escape to quit. Boost energy recharges when
not in use. A connected gamepad uses its left stick to steer, triggers to accelerate and brake,
A to boost, and B to jump. Pass checkpoints in order, then cross the finish zone to complete a
lap. Complete the selected number of laps to finish; press `R` to restart. The rivals follow the
same course; the first craft to finish wins.

## Process

- [Clean-room policy](docs/clean-room-policy.md): what may and may not go in.
- [Roadmap](docs/roadmap.md): staged plan for the playable game and its original feature set.
- [Track format](docs/track-format.md): v1 route and provenance requirements.
- [Licensing](docs/licensing.md) and [third-party notices](THIRD_PARTY_NOTICES.md).
- [Provenance ledger](PROVENANCE.md): where every file came from. Run
  `scripts/check-provenance.py` before committing.

## Licence

Code and documentation are licensed under either the [MIT License](LICENSE-MIT) or the
[Apache License 2.0](LICENSE-APACHE), at your option (`MIT OR Apache-2.0`). Created game
assets are CC BY 4.0 (or CC0 where stated). Details, and which dependencies are acceptable:
[licensing](docs/licensing.md). Contributions are accepted under the same terms.
