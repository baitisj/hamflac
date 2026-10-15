// Writes a short 16-bit stereo WAV (band-limited noise and a tone) for the
// command-line round-trip test.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::mt19937 g(7);
    std::normal_distribution<double> nd(0.0, 1.0);
    const std::size_t n = 4000 * 30;
    std::vector<std::int16_t> x(2 * n);
    double lp[2] = {0, 0};
    for (std::size_t i = 0; i < n; ++i)
        for (int c = 0; c < 2; ++c) {
            lp[c] = 0.9 * lp[c] + 0.1 * nd(g);
            const double v = 3000.0 * lp[c] + 500.0 * std::sin(0.3 * i + c);
            x[2 * i + c] = static_cast<std::int16_t>(std::lround(v));
        }
    drwav_data_format fmt{};
    fmt.container = drwav_container_riff;
    fmt.format = DR_WAVE_FORMAT_PCM;
    fmt.channels = 2;
    fmt.sampleRate = 4000;
    fmt.bitsPerSample = 16;
    drwav wav;
    if (!drwav_init_file_write(&wav, argv[1], &fmt, nullptr)) return 1;
    const bool ok = drwav_write_pcm_frames(&wav, n, x.data()) == n;
    drwav_uninit(&wav);
    return ok ? 0 : 1;
}
