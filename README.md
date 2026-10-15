# HamFLAC

A small FLAC encoder for 16-bit audio, in one C++20 source file, with no
dependency on libFLAC. It comes with a reader (the vendored, single-header
[dr_flac](https://github.com/mackron/dr_libs)) and a `hamflac` command-line
tool.

It was written for [SSTVAE](https://github.com/arodland/SSTVAE)'s QRSSTVAE
mode, an ultra-slow picture mode for HF whose listener keeps 48 hours of
4 kHz I/Q passband audio on disk and packs each finished hour into FLAC.
Anything that records receiver audio for a long time has the same need:
lossless, compact, seekable files, written from inside the program, with
nothing extra to install. That covers WSPR and FT8 archivers, SDR
recorders, beacon monitors and grabbers.

## What it writes

Ordinary FLAC: libFLAC, `flac -t`, ffmpeg and dr_flac all decode it sample
for sample, and the tests check that.

- 16-bit PCM, mono or stereo, any sample rate FLAC allows, a fixed block
  size (4096 by default).
- Each subframe is whichever is smallest by exact size: CONSTANT (silence
  costs almost nothing), VERBATIM, FIXED orders 0-4, or LPC up to order 12.
  LPC is found as libFLAC finds it: tukey(0.5) and partial_tukey(2)
  windows, Levinson-Durbin, coefficients quantized with error feedback.
- Partitioned Rice residuals, with each partition's parameter searched
  exactly and an escape to raw bits wherever that is cheaper.
- Stereo tries left/right, left/side, right/side and mid/side.
- STREAMINFO carries the real MD5 of the samples and the real frame sizes.
  A SEEKTABLE (a point every 16 frames) makes partial reads fast.
- It is a FLAC "subset" stream, and every prediction sum fits in 32 bits,
  so decoders that predict in 32 bits are safe.

On eight simulated hours of HF passband recordings (weak signals over band
noise, 4 kHz I/Q), files came out 0.90-0.97x the size of libFLAC's default
level 5, encoding at 0.4-1.5 s of one core per hour of stereo audio.

The reader opens any FLAC file of up to 16 bits per sample, seeks to a
sample (with the seek table, or by dr_flac's CRC-checked search when there
is none), and treats a frame that fails its CRC as an error instead of
silently skipping it.

## Using it from CMake

```cmake
include(FetchContent)
FetchContent_Declare(hamflac GIT_REPOSITORY https://github.com/baitisj/HamFLAC GIT_TAG main)
FetchContent_MakeAvailable(hamflac)
target_link_libraries(my_app PRIVATE HamFLAC::hamflac)
```

`add_subdirectory(HamFLAC)` works too. Tools and tests build only when
HamFLAC is the top-level project (`HAMFLAC_BUILD_TOOLS`,
`HAMFLAC_BUILD_TESTS`).

```cpp
#include "hamflac/hamflac.hpp"

// Encode from memory (interleaved samples).
std::string bytes = hamflac::encode(samples, /*channels=*/2, /*sample_rate=*/4000);

// Or stream a long recording to a file, a block at a time.
hamflac::encode_file("hour.flac", 4000, 2, total_frames,
    [&](std::uint64_t first, std::size_t n, std::int16_t* out) {
        return read_my_samples(first, n, out);   // false aborts the encode
    });

// Read a range back.
hamflac::Reader r("hour.flac");
std::vector<std::int16_t> x = r.read(/*first=*/120000, /*n=*/4000);
```

`encode_file` fsyncs the file before it returns, so a caller can safely
delete its source; set `EncodeOptions::sync = false` to skip that. It
polls an optional `std::atomic<bool>` to cancel. File names are UTF-8 on
every platform. See `include/hamflac/hamflac.hpp` for the rest.

## The command-line tool

```
hamflac encode in.wav out.flac [--blocksize N] [--max-lpc-order N] [--no-sync]
hamflac decode in.flac out.wav [--from FRAME] [--count N]
hamflac info in.flac
```

`info` prints STREAMINFO and checks the stored MD5 against the decoded
samples.

## Building and testing

```
cmake -S . -B build -G Ninja
ninja -C build
ctest --test-dir build --output-on-failure
```

The tests take a couple of seconds. `test_flac` and `test_flac_lpc` check
round trips, a strict decoder of their own (every field, 32-bit
prediction), every LPC order and precision, CRCs, MD5, seeking, damaged
frames, and a seeded fuzz. When libFLAC is installed (found through
pkg-config), `test_libflac` decodes a fuzzed corpus through it with its MD5
check on. `cli_roundtrip` runs the tool, and also `flac -t` and ffmpeg
when they are installed.

## Limits

16 bits per sample, one or two channels, and a fixed block size. The
reader also refuses files of more than 16 bits per sample.

## License

Artistic License 2.0 (see `LICENSE`), the same as SSTVAE, where this code
started. The vendored dr_libs headers are public domain or MIT-0; see
`NOTICE`.
