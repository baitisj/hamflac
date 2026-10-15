// hamflac: WAV <-> FLAC from the command line, through the HamFLAC library.
//
//   hamflac encode in.wav out.flac [--blocksize N] [--max-lpc-order N] [--no-sync]
//   hamflac decode in.flac out.wav [--from FRAME] [--count N]
//   hamflac info in.flac
//
// encode takes 16-bit (or wider/float, converted by dr_wav) mono or stereo
// WAV and writes 16-bit FLAC; decode writes 16-bit PCM WAV, optionally a
// range of sample frames; info prints STREAMINFO and checks the MD5.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "hamflac/hamflac.hpp"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

namespace {

int usage() {
    std::fprintf(stderr,
                 "usage: hamflac encode in.wav out.flac [--blocksize N] [--max-lpc-order N] [--no-sync]\n"
                 "       hamflac decode in.flac out.wav [--from FRAME] [--count N]\n"
                 "       hamflac info in.flac\n");
    return 2;
}

std::string hex(const std::array<std::uint8_t, 16>& d) {
    static const char* x = "0123456789abcdef";
    std::string s;
    for (auto b : d) {
        s += x[b >> 4];
        s += x[b & 15];
    }
    return s;
}

int encode(const std::string& in, const std::string& out, const hamflac::EncodeOptions& opt) {
    drwav wav;
    if (!drwav_init_file(&wav, in.c_str(), nullptr)) {
        std::fprintf(stderr, "hamflac: cannot read %s as WAV\n", in.c_str());
        return 1;
    }
    const int ch = wav.channels;
    const int fs = static_cast<int>(wav.sampleRate);
    const std::uint64_t total = wav.totalPCMFrameCount;
    if (ch != 1 && ch != 2) {
        drwav_uninit(&wav);
        std::fprintf(stderr, "hamflac: %s has %d channels; HamFLAC encodes 1 or 2\n", in.c_str(), ch);
        return 1;
    }
    std::uint64_t at = 0;
    // The encoder pulls frames in order, a block at a time.
    const hamflac::Source src = [&](std::uint64_t first, std::size_t n, std::int16_t* p) {
        if (first != at) return false;
        const auto got = drwav_read_pcm_frames_s16(&wav, n, p);
        at += got;
        return got == n;
    };
    hamflac::EncodeStats st;
    try {
        st = hamflac::encode_file(out, fs, ch, total, src, opt);
    } catch (...) {
        drwav_uninit(&wav);
        throw;
    }
    drwav_uninit(&wav);
    const double raw = static_cast<double>(total) * ch * 2;
    std::printf("%s: %llu frames, %d ch, %d Hz -> %llu bytes (%.3f of 16-bit PCM)\n", out.c_str(),
                static_cast<unsigned long long>(total), ch, fs, static_cast<unsigned long long>(st.bytes),
                raw > 0 ? static_cast<double>(st.bytes) / raw : 0.0);
    return 0;
}

int decode(const std::string& in, const std::string& out, std::uint64_t from, std::uint64_t count) {
    hamflac::Reader r(in);
    const auto& info = r.info();
    const std::vector<std::int16_t> x = r.read(from, count);
    drwav_data_format fmt{};
    fmt.container = drwav_container_riff;
    fmt.format = DR_WAVE_FORMAT_PCM;
    fmt.channels = static_cast<drwav_uint32>(info.channels);
    fmt.sampleRate = static_cast<drwav_uint32>(info.sample_rate);
    fmt.bitsPerSample = 16;
    drwav wav;
    if (!drwav_init_file_write(&wav, out.c_str(), &fmt, nullptr)) {
        std::fprintf(stderr, "hamflac: cannot write %s\n", out.c_str());
        return 1;
    }
    const std::uint64_t frames = x.size() / static_cast<std::size_t>(info.channels);
    const auto wrote = drwav_write_pcm_frames(&wav, frames, x.data());
    drwav_uninit(&wav);
    if (wrote != frames) {
        std::fprintf(stderr, "hamflac: short write to %s\n", out.c_str());
        return 1;
    }
    return 0;
}

int info(const std::string& in) {
    hamflac::Reader r(in);
    const auto& i = r.info();
    std::printf("channels %d\nsample_rate %d\nbits_per_sample %d\nframes %llu\nmd5 %s\n", i.channels,
                i.sample_rate, i.bits_per_sample, static_cast<unsigned long long>(i.frames), hex(i.md5).c_str());
    const auto x = r.read(0, i.frames);
    const auto md5 = hamflac::pcm16_md5(x);
    const bool none = i.md5 == std::array<std::uint8_t, 16>{};
    if (none) {
        std::printf("md5 check: none stored\n");
        return 0;
    }
    std::printf("md5 check: %s\n", md5 == i.md5 ? "ok" : "MISMATCH");
    return md5 == i.md5 ? 0 : 1;
}

std::uint64_t number(const char* s) { return std::strtoull(s, nullptr, 10); }

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) return usage();
    const std::string cmd = argv[1];
    try {
        if (cmd == "info" && argc == 3) return info(argv[2]);
        if (argc < 4) return usage();
        if (cmd == "encode") {
            hamflac::EncodeOptions opt;
            for (int i = 4; i < argc; ++i) {
                const std::string a = argv[i];
                if (a == "--no-sync") opt.sync = false;
                else if (a == "--blocksize" && i + 1 < argc) opt.blocksize = static_cast<int>(number(argv[++i]));
                else if (a == "--max-lpc-order" && i + 1 < argc) opt.max_lpc_order = static_cast<int>(number(argv[++i]));
                else return usage();
            }
            return encode(argv[2], argv[3], opt);
        }
        if (cmd == "decode") {
            std::uint64_t from = 0, count = ~0ull;
            for (int i = 4; i < argc; ++i) {
                const std::string a = argv[i];
                if (a == "--from" && i + 1 < argc) from = number(argv[++i]);
                else if (a == "--count" && i + 1 < argc) count = number(argv[++i]);
                else return usage();
            }
            return decode(argv[2], argv[3], from, count);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "hamflac: %s\n", e.what());
        return 1;
    }
    return usage();
}
