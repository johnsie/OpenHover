# Clean-room policy

OpenHover exists to be a hovercraft racing game that is **not tied to the HoverRace
licence**. That claim is only as good as the record behind it, so this project works
under these rules.

*This is an engineering policy, not legal advice. Have a lawyer review it, and the
licence of the original project, before relying on it.*

## 1. What may go in

A file, asset, or dependency may be added only if its origin is known, and all of these
hold:

- **Written here.** Created for OpenHover, by people (or tools) who were not copying
  from the licensed project; **or**
- **Carried over from the licensed project's repository, but written there after its
  import** and independent of it, and the maintainer confirms the ownership. Each such
  file is listed in `PROVENANCE.md` with where it came from; **or**
- **Third party under a permissive licence** (MIT, BSD, zlib, Apache-2.0, CC0, or similar; see [licensing](licensing.md))
  whose text is kept, whose version is recorded, and which the maintainer approved.

Everything is recorded in `PROVENANCE.md` **in the same commit that adds it**.
`scripts/check-provenance.py` fails if a file is not recorded.

## 2. What must never go in

- Source code, textures, models, sounds, track data, or documents from HoverRace or from
  the licensed "HoverNet Classic" project, other than the independent files recorded in
  `PROVENANCE.md`.
- The community track library (its licensing was never recorded) or conversions of it,
  until each track's rights are established.
- Code or assets of unknown origin, or under a licence that is more restrictive than the
  one chosen for OpenHover.
- Names, logos, or artwork that identify the licensed project.

## 3. Rebuilding what the original did

The game's behaviour is described by **what it does**, not how its source is written.

- Describe behaviour from playing it and from black-box measurements: recorded inputs and
  the outcomes they produce, speeds, timings, and rules a player can observe.
- Write each requirement as a test (an input recording and an expected result) before
  writing the code that satisfies it. The old game is an oracle for those tests, never a
  source of code.
- Do not read, paste, translate, or port the licensed source while writing a replacement.
  Where a stricter separation is needed (one person documents behaviour, another
  implements it), record who did which.

## 4. New assets

Textures, models, sounds, music, and fonts are created for OpenHover or taken from sources
with a recorded permissive licence. For created assets, record the author and the tool or
method; keep source files (not only exports) where practical.

## 5. AI tools

AI assistants may help write code and assets, under the same rules:

- Do not give the assistant licensed source or assets to read, translate, or imitate.
- An assistant that has previously worked on licensed code is not a clean implementer for
  replacing that code. Say which tool and version helped, in the ledger entry.
- The maintainer reviews generated work for similarity to known licensed material and
  confirms ownership before it is recorded.

## 6. Reviewing a change

Before merging, check that: every new file is in `PROVENANCE.md`; the origin and licence
columns are filled in; nothing came from the licensed project except through section 1; any
new dependency's licence is recorded; and `scripts/check-provenance.py` passes.

## 7. If something turns out to be tainted

Remove it, write a replacement under section 3 by someone who has not seen the removed
material, and record the incident and the fix in `PROVENANCE.md`. Do not rewrite history
to hide it.

## 8. Open decisions

- The licence is chosen (MIT OR Apache-2.0, see [licensing](licensing.md)); it still needs a lawyer's review.
- The project name and branding (the working name only describes the intent).
- Who counts as a clean implementer, and how strict section 3 must be, after legal advice.
