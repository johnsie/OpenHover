# Licensing

*An engineering summary, not legal advice. Have a lawyer review it before relying on it.*

## OpenHover's licence

OpenHover's **code and documentation** are licensed under **either** of

- the **MIT License** ([LICENSE-MIT](../LICENSE-MIT)), or
- the **Apache License, Version 2.0** ([LICENSE-APACHE](../LICENSE-APACHE)),

**at your option**. SPDX identifier: `MIT OR Apache-2.0`.

Why both: MIT is the simplest, most widely understood permissive licence and is compatible
with GPLv2 projects; Apache-2.0 adds an explicit patent grant and clear contribution terms
but cannot be combined with GPLv2-only code. Offering both lets every downstream developer
pick the one that fits, including for commercial and closed-source use.

### Contributions

Unless you state otherwise, any contribution you submit for inclusion is licensed as above,
under both licences, with no additional terms. By contributing you confirm you have the right
to license it that way (see the [clean-room policy](clean-room-policy.md): no licensed
HoverRace material).

### Game assets

Textures, models, sounds, music, and fonts are not "code". Created assets are licensed
**CC BY 4.0** (attribution) unless a file states **CC0** (public domain). Both allow
commercial use and modification. Each asset's author and licence are recorded in
`PROVENANCE.md`.

## What we may depend on

| Licence of a dependency | Allowed? | Notes |
| --- | --- | --- |
| MIT, BSD (2/3-clause), ISC, zlib/libpng, Boost, Apache-2.0, CC0, public domain | **Yes** | Keep the licence text and notices in the distribution. |
| MPL-2.0 | Yes, with care | File-level copyleft: changes to those files must be shared. |
| LGPL-2.1/3.0 (for example OpenAL Soft) | Only **dynamically linked** and replaceable | Do not link statically; ship the notice and how to relink. |
| GPL, AGPL | **No** | Would force the whole game under the GPL. |
| Proprietary SDKs, "non-commercial" or "no derivatives" licences | **No** | Not compatible with free redistribution. |
| Anything of unknown licence | **No** | Record the licence first (see `PROVENANCE.md`). |

Programs that are only *run* as separate processes (for example `curl` for downloads) are
not linked, so their licence does not affect OpenHover's.

Known and planned dependencies are all compatible: **SDL2** (zlib), **Dear ImGui** (MIT)
and its bundled **stb** headers (public domain / MIT).

## Keeping it right

- Every new dependency is added to `THIRD_PARTY_NOTICES.md` (name, version, licence, where
  it is used) in the same commit.
- Every new file is recorded in `PROVENANCE.md` (`scripts/check-provenance.py` enforces it).
- Source files should carry `SPDX-License-Identifier: MIT OR Apache-2.0`; new files do.
