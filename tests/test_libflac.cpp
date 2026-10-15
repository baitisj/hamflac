// HamFLAC's streams through the reference decoder, libFLAC: every stream
// decodes sample for sample, with libFLAC's own MD5 check of the decoded
// samples against STREAMINFO on, and no error callback. Built only when
// CMake finds libFLAC (HAMFLAC_LIBFLAC_TESTS).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include <FLAC/stream_decoder.h>

#include "check.hpp"
#include "hamflac/hamflac.hpp"

namespace {

struct Ctx {
    const std::string* bytes;
    std::size_t pos = 0;
    std::vector<std::int16_t> out;
    unsigned channels = 0;
    int errors = 0;
};

FLAC__StreamDecoderReadStatus on_read(const FLAC__StreamDecoder*, FLAC__byte buf[], size_t* n, void* u) {
    auto* c = static_cast<Ctx*>(u);
    const std::size_t k = std::min(*n, c->bytes->size() - c->pos);
    if (k == 0) {
        *n = 0;
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }
    std::memcpy(buf, c->bytes->data() + c->pos, k);
    c->pos += k;
    *n = k;
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderWriteStatus on_write(const FLAC__StreamDecoder*, const FLAC__Frame* f,
                                        const FLAC__int32* const buf[], void* u) {
    auto* c = static_cast<Ctx*>(u);
    c->channels = f->header.channels;
    for (unsigned i = 0; i < f->header.blocksize; ++i)
        for (unsigned ch = 0; ch < f->header.channels; ++ch) c->out.push_back(static_cast<std::int16_t>(buf[ch][i]));
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void on_error(const FLAC__StreamDecoder*, FLAC__StreamDecoderErrorStatus s, void* u) {
    ++static_cast<Ctx*>(u)->errors;
    std::fprintf(stderr, "libFLAC error: %s\n", FLAC__StreamDecoderErrorStatusString[s]);
}

void through_libflac(const std::vector<std::int16_t>& x, int ch, const hamflac::EncodeOptions& opt,
                     const std::string& what) {
    const std::string bytes = hamflac::encode(x, ch, 8000, opt);
    Ctx c;
    c.bytes = &bytes;
    FLAC__StreamDecoder* d = FLAC__stream_decoder_new();
    FLAC__stream_decoder_set_md5_checking(d, true);
    const bool init = FLAC__stream_decoder_init_stream(d, on_read, nullptr, nullptr, nullptr, nullptr, on_write,
                                                       nullptr, on_error, &c) == FLAC__STREAM_DECODER_INIT_STATUS_OK;
    const bool ok = init && FLAC__stream_decoder_process_until_end_of_stream(d);
    const bool md5 = init && FLAC__stream_decoder_finish(d);   // false: MD5 mismatch
    FLAC__stream_decoder_delete(d);
    hamflac::check::is_true(ok, what + ": libFLAC decodes");
    hamflac::check::is_true(md5, what + ": libFLAC's MD5 check");
    hamflac::check::is_true(c.errors == 0, what + ": no libFLAC errors");
    hamflac::check::is_true(c.out == x, what + ": samples");
}

}  // namespace

int main() {
    std::mt19937 g(20261015);
    std::normal_distribution<double> nd(0.0, 1.0);
    for (int trial = 0; trial < 60; ++trial) {
        const int ch = 1 + trial % 2;
        const std::size_t n = 1 + g() % 40000;
        std::vector<std::int16_t> x(n * ch);
        const int kind = trial % 6;
        double lp[2] = {0, 0};
        const double level = std::pow(10.0, -0.5 * (g() % 9));
        for (std::size_t i = 0; i < n; ++i)
            for (int c = 0; c < ch; ++c) {
                double v = 0;
                switch (kind) {
                    case 0: v = 0; break;                                                  // silence
                    case 1: v = 32767.0 * level * nd(g); break;                            // white noise
                    case 2: v = 30000.0 * level * std::sin(0.01 * (c + 1) * i); break;     // sine
                    case 3: v = ((i / 37) % 2 ? 32767.0 : -32768.0); break;                // full-scale square
                    case 4: lp[c] = 0.97 * lp[c] + 0.03 * nd(g); v = 30000.0 * level * lp[c]; break;  // band-limited
                    default: v = (i > n / 3 && i < n / 2) ? 8000.0 * nd(g) : 0.0; break;   // a burst
                }
                x[i * ch + c] = static_cast<std::int16_t>(std::clamp(std::nearbyint(v), -32768.0, 32767.0));
            }
        hamflac::EncodeOptions opt;
        if (trial % 5 == 4) opt.max_lpc_order = static_cast<int>(1 + g() % 12);
        if (trial % 7 == 6) opt.blocksize = static_cast<int>(16 + g() % 4593);
        if (trial % 11 == 10) opt.stereo_decorrelation = false;
        through_libflac(x, ch, opt, "trial " + std::to_string(trial));
    }
    return hamflac::check::report("test_libflac");
}
