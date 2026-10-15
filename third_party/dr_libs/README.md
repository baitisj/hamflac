# dr_libs (vendored)

From https://github.com/mackron/dr_libs, branch `master`. Public domain /
MIT-0 dual licence; see `LICENSE`, the licence block from the headers.
Both unmodified.

- `dr_flac.h` v0.13.4, fetched 2026-10-14, sha256
  `111144e778f55738db6851cb226015c419e00d04b916a09506d4856d9cff945c`.
  HamFLAC's reader (`src/hamflac.cpp` is the only file that defines
  `DR_FLAC_IMPLEMENTATION`), built with its CRC checks on: they are what
  makes its seek without a SEEKTABLE (a binary search over frame headers)
  safe on files from encoders that write none.
- `dr_wav.h`, fetched 2026-07-28: WAV in and out for `tools/hamflac.cpp`
  and the command-line test. The library itself does not use it.
