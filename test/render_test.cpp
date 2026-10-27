// Offline test: run a synthetic drum + chord loop through the engine with Melt on,
// write before/after WAVs and print per-second level stats.
#include "../Source/MeltEngine.h"
#include <cstdio>
#include <cstring>

static void writeWav(const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
    FILE* f = std::fopen(path, "wb");
    const int n = (int)L.size(); const int dataBytes = n * 4;
    auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); w32(36 + dataBytes); std::fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(1); w16(2); w32(sr); w32(sr * 4); w16(4); w16(16);
    std::fwrite("data", 1, 4, f); w32(dataBytes);
    for (int i = 0; i < n; ++i) {
        int16_t a = (int16_t)std::clamp(L[i] * 32767.0f, -32768.0f, 32767.0f);
        int16_t b = (int16_t)std::clamp(R[i] * 32767.0f, -32768.0f, 32767.0f);
        std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
    }
    std::fclose(f);
}

int main(int argc, char** argv) {
    const int sr = 48000, seconds = 24;
    const int n = sr * seconds;
    std::vector<float> L(n), R(n);
    melt::Rng rng;
    const float beat = sr * 0.5f; // 120 bpm
    const float chord[4] = { 220.0f, 261.63f, 329.63f, 392.0f };
    for (int i = 0; i < n; ++i) {
        const float t = (float)i / sr;
        const float pb = std::fmod((float)i, beat) / sr;          // time since beat
        const float p8 = std::fmod((float)i, beat * 0.5f) / sr;    // time since 8th
        float kick = std::sin(2 * melt::kPi * (50 + 120 * std::exp(-pb * 30)) * pb) * std::exp(-pb * 8);
        float hat = rng.bi() * std::exp(-p8 * 60) * 0.25f;
        const bool snareBeat = ((int)(i / beat) % 2) == 1;
        float snare = snareBeat ? rng.bi() * std::exp(-pb * 18) * 0.4f : 0.0f;
        float pad = 0;
        for (float fq : chord) pad += std::sin(2 * melt::kPi * fq * t) * 0.07f;
        L[i] = 0.6f * kick + hat + snare + pad;
        R[i] = 0.6f * kick + 0.8f * hat + snare + pad;
    }
    writeWav(argc > 1 ? argv[1] : "dry.wav", L, R, sr);

    melt::Engine e; e.prepare(sr); e.setBpm(120);
    melt::Params p; p.meltOn = true; p.meltSec = 14.0f;
    const int block = 256;
    float* chs[2];
    int nanCount = 0;
    for (int s = 0; s < n; s += block) {
        const int m = std::min(block, n - s);
        if (s >= sr * 18) p.meltOn = false; // release at 18 s
        chs[0] = L.data() + s; chs[1] = R.data() + s;
        e.process(chs, 2, m, p);
    }
    for (int sec = 0; sec < seconds; ++sec) {
        double sum = 0; float pk = 0;
        for (int i = sec * sr; i < (sec + 1) * sr; ++i) {
            if (!std::isfinite(L[i]) || !std::isfinite(R[i])) ++nanCount;
            sum += L[i] * L[i] + R[i] * R[i]; pk = std::max(pk, std::max(std::fabs(L[i]), std::fabs(R[i])));
        }
        std::printf("t=%2ds  rms=%.3f  peak=%.3f\n", sec, std::sqrt(sum / (2.0 * sr)), pk);
    }
    std::printf("non-finite samples: %d\n", nanCount);
    writeWav(argc > 2 ? argv[2] : "melted.wav", L, R, sr);
    return 0;
}
