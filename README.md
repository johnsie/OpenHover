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
verified downloader, each with tests. Build and test with:

```
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

## Process

- [Clean-room policy](docs/clean-room-policy.md): what may and may not go in.
- [Licensing](docs/licensing.md) and [third-party notices](THIRD_PARTY_NOTICES.md).
- [Provenance ledger](PROVENANCE.md): where every file came from. Run
  `scripts/check-provenance.py` before committing.

## Licence

Code and documentation are licensed under either the [MIT License](LICENSE-MIT) or the
[Apache License 2.0](LICENSE-APACHE), at your option (`MIT OR Apache-2.0`). Created game
assets are CC BY 4.0 (or CC0 where stated). Details, and which dependencies are acceptable:
[licensing](docs/licensing.md). Contributions are accepted under the same terms.
