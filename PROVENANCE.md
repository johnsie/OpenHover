# Provenance ledger

Every file in this repository is listed here with where it came from. A change is not
complete until its files are recorded (see `docs/clean-room-policy.md`);
`scripts/check-provenance.py` enforces it.

**Status labels**

- **Original:** created for OpenHover.
- **Carried over:** copied unmodified from the licensed project's repository ("HoverNet
  Classic", commit `2c597eb`, 2 October 2026). Each was written there *after* the HoverRace
  import, uses no HoverRace code, types, or headers (it builds here with the standard
  library alone), and is independent of it. **Ownership and licence are to be confirmed by
  the maintainer;** most were written with AI assistance (Claude), which is recorded in the
  notes. They still carry the old product name in places; rename them under OpenHover.

| Path | Origin | Authors | Licence status | Notes |
| --- | --- | --- | --- | --- |
| `README.md` | Original | Maintainer, with AI assistance (Claude) | OpenHover licence (not yet chosen) | |
| `.gitignore` | Original | Maintainer, with AI assistance (Claude) | OpenHover licence (not yet chosen) | |
| `PROVENANCE.md` | Original | Maintainer, with AI assistance (Claude) | OpenHover licence (not yet chosen) | this ledger |
| `CMakeLists.txt` | Original | Maintainer, with AI assistance (Claude) | OpenHover licence (not yet chosen) | |
| `docs/clean-room-policy.md` | Original | Maintainer, with AI assistance (Claude) | OpenHover licence (not yet chosen) | needs legal review |
| `scripts/check-provenance.py` | Original | Maintainer, with AI assistance (Claude) | OpenHover licence (not yet chosen) | |
| `src/util/Sha256.h` | Carried over: `NetTarget/LinuxClient/Sha256.h` (added 2026-10-01, `86e942b`) | Maintainer, with AI assistance (Claude) | to be confirmed | standard SHA-256 (FIPS 180-4); library only |
| `src/util/Sha256.cpp` | Carried over: `NetTarget/LinuxClient/Sha256.cpp` (`86e942b`) | Maintainer, with AI assistance (Claude) | to be confirmed | |
| `src/util/Gunzip.h` | Carried over: `NetTarget/LinuxClient/Gunzip.h` (`86e942b`) | Maintainer, with AI assistance (Claude) | to be confirmed | gzip/DEFLATE decoder (RFC 1951/1952) |
| `src/util/Gunzip.cpp` | Carried over: `NetTarget/LinuxClient/Gunzip.cpp` (`86e942b`) | Maintainer, with AI assistance (Claude) | to be confirmed | |
| `src/util/WireFormat.h` | Carried over: `NetTarget/Util/WireFormat.h` (added 2026-09-28, `4ea517d`) | Maintainer, with AI assistance (Claude) | to be confirmed | fixed-width little-endian helpers; namespace `HoverNetWire` to rename |
| `src/content/TrackCatalog.h` | Carried over: `NetTarget/LinuxClient/TrackCatalog.h` (added 2026-10-01, `d3121dc`) | Maintainer, with AI assistance (Claude) | to be confirmed | track list and safe-name resolution; `.trk` suffix and "community" wording are Classic-specific |
| `src/content/TrackCatalog.cpp` | Carried over: `NetTarget/LinuxClient/TrackCatalog.cpp` (`d3121dc`) | Maintainer, with AI assistance (Claude) | to be confirmed | |
| `src/content/TrackDownloader.h` | Carried over: `NetTarget/LinuxClient/TrackDownloader.h` (`86e942b`) | Maintainer, with AI assistance (Claude) | to be confirmed | verified on-demand downloads |
| `src/content/TrackDownloader.cpp` | Carried over: `NetTarget/LinuxClient/TrackDownloader.cpp` (`86e942b`) | Maintainer, with AI assistance (Claude) | to be confirmed | default download URL points at the Classic project's releases: replace |
| `tests/TrackCatalogSmoke.cpp` | Carried over: `NetTarget/LinuxClient/TrackCatalogSmoke.cpp` (`d3121dc`) | Maintainer, with AI assistance (Claude) | to be confirmed | test |
| `tests/TrackDownloadSmoke.cpp` | Carried over: `NetTarget/LinuxClient/TrackDownloadSmoke.cpp` (`86e942b`) | Maintainer, with AI assistance (Claude) | to be confirmed | test |
| `scripts/make-release-checksums.sh` | Carried over: `scripts/make-release-checksums.sh` (added 2026-10-01, `eddc478`) | Maintainer, with AI assistance (Claude) | to be confirmed | writes `SHA256SUMS` |
| `packaging/debian/reproducible.sh` | Carried over: `packaging/debian/reproducible.sh` (`eddc478`) | Maintainer, with AI assistance (Claude) | to be confirmed | reproducible-build helper |

## Not carried over, on purpose

Everything else in the Classic repository stays there. It falls into one of: HoverRace
source, assets, or tracks (licensed or of unrecorded origin); code written against the
HoverRace engine's types (which cannot be separated from it); third-party code with its own
licence; documents that describe the original's internals; and files specific to the
Classic product. The Classic repository keeps a full per-file classification
(`docs/provenance-audit.csv`, produced by `scripts/audit-provenance.py`) so any further
file can be reviewed individually before it is considered.

## Incidents

None recorded.
