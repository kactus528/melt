// MeltEngine.h — framework-independent DSP for the "Melt" psychedelic breakdown effect.
// Everything the plugin does to audio lives here, so it can be tested without a DAW.
#pragma once
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <atomic>

namespace melt {

constexpr float kPi = 3.14159265358979f;

// ---------------------------------------------------------------- utilities
struct Rng {
    uint32_t s = 0x9E3779B9u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float)(s & 0xFFFFFF) / 16777216.0f; } // [0,1)
    float bi() { return next() * 2.0f - 1.0f; }                                                             // [-1,1)
};

// Smoothly wandering random value in [-1,1] (cosine-interpolated random targets).
struct Wander {
    float a = 0, b = 0, ph = 1;
    float tick(Rng& r, float rateHz, float sr) {
        ph += rateHz / sr;
        if (ph >= 1.0f) { ph -= std::floor(ph); a = b; b = r.bi(); }
        const float t = (1.0f - std::cos(ph * kPi)) * 0.5f;
        return a + (b - a) * t;
    }
};

struct OnePole { // parameter smoother
    float y = 0, k = 0.001f;
    void setTime(float seconds, float sr) { k = 1.0f - std::exp(-1.0f / std::max(1.0f, seconds * sr)); }
    float tick(float target) { y += (target - y) * k; return y; }
};

struct Delay {
    std::vector<float> buf; int w = 0; int mask = 0;
    void init(int minSize) { int n = 16; while (n < minSize) n <<= 1; buf.assign((size_t)n, 0.0f); mask = n - 1; w = 0; }
    void clear() { std::fill(buf.begin(), buf.end(), 0.0f); w = 0; }
    int size() const { return mask + 1; }
    void push(float x) { buf[(size_t)w] = x; w = (w + 1) & mask; }
    // Read 'd' samples into the past (d >= 3 for the cubic to stay inside written data).
    float read(float d) const {
        d = std::clamp(d, 3.0f, (float)(mask - 3));
        float rp = (float)w - d; if (rp < 0) rp += (float)(mask + 1);
        const int i = (int)rp; const float f = rp - (float)i;
        const float y0 = buf[(size_t)((i - 1) & mask)], y1 = buf[(size_t)(i & mask)];
        const float y2 = buf[(size_t)((i + 1) & mask)], y3 = buf[(size_t)((i + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
};

// Topology-preserving state-variable low-pass.
struct Svf {
    float ic1 = 0, ic2 = 0;
    float lp(float x, float cutoff, float q, float sr) {
        const float g = std::tan(kPi * std::min(cutoff, 0.45f * sr) / sr);
        const float k = 1.0f / q;
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        return v2;
    }
    void reset() { ic1 = ic2 = 0; }
};

struct Allpass { // Schroeder allpass for diffusion
    Delay d; float g = 0.6f; int len = 100;
    void init(int n) { len = n; d.init(n + 8); }
    float tick(float x) {
        const float del = d.read((float)len);
        const float v = x + g * del;
        d.push(v);
        return del - g * v;
    }
};

inline float softLimit(float x) { // transparent below 0.8, smooth ceiling at 1.0
    const float a = std::fabs(x);
    if (a <= 0.8f) return x;
    return std::copysign(0.8f + 0.2f * std::tanh((a - 0.8f) / 0.2f), x);
}

// ---------------------------------------------------------------- parameters
struct Params {
    float chaos   = 0.0f;  // master macro 0..1
    float warble  = 0.7f;  // tape wow / flutter
    float dive    = 0.6f;  // pitch sag / tape-stop
    float stutter = 0.6f;  // buffer repeats
    float reverse = 0.5f;  // reversed grains
    float crush   = 0.4f;  // bit + sample-rate reduction
    float filter  = 0.5f;  // wandering resonant low-pass
    float smear   = 0.6f;  // diffuse runaway feedback wash
    float mix     = 1.0f;  // dry/wet
    float outDb   = 0.0f;  // output gain
    bool  meltOn  = false; // auto-ramp chaos up to 1
    float meltSec = 8.0f;  // time for the auto-ramp to reach full chaos
};

// ---------------------------------------------------------------- engine
class Engine {
public:
    void prepare(double sampleRate) {
        sr = (float)sampleRate;
        for (int c = 0; c < 2; ++c) {
            hist[c].init((int)(sr * 4.0f));
            slice[c].assign((size_t)(sr * 2.0f), 0.0f);
            wob[c].init((int)(sr * 0.1f));
            pitch[c].init((int)(sr * 0.25f));
            smearLine[c].init((int)(sr * 1.0f));
            const float apMs[3] = { 4.7f + c * 0.6f, 11.3f + c * 0.9f, 17.9f + c * 1.3f };
            for (int i = 0; i < 3; ++i) ap[c][i].init((int)(apMs[i] * 0.001f * sr));
        }
        const float smoothSec = 0.03f;
        for (auto* s : { &sChaos, &sWarble, &sDive, &sStut, &sRev, &sCrush, &sFilt, &sSmear, &sMix, &sOut })
            s->setTime(smoothSec, sr);
        sEnv.setTime(0.002f, sr);
        reset();
    }

    void reset() {
        for (int c = 0; c < 2; ++c) {
            hist[c].clear(); wob[c].clear(); pitch[c].clear(); smearLine[c].clear();
            for (auto& a : ap[c]) a.d.clear();
            svf[c].reset(); holdVal[c] = 0; smearFb[c] = 0; hpState[c] = 0; lpState[c] = 0; dcX[c] = dcY[c] = 0;
            std::fill(slice[c].begin(), slice[c].end(), 0.0f);
        }
        active = false; sEnv.y = 0; countdown = 0; ramp = 0; holdCount = 0; pitchPhase = 0; lfoPhase = 0;
    }

    void setBpm(double b) { if (b > 20.0 && b < 400.0) bpm = (float)b; }
    float currentChaos() const { return chaosOut.load(); }

    // Process in place. 'nCh' is 1 or 2.
    void process(float* const* ch, int nCh, int n, const Params& p) {
        const float dt = 1.0f / sr;
        const float outGainTarget = std::pow(10.0f, p.outDb / 20.0f);
        const float beat = sr * 60.0f / bpm; // samples per quarter note
        float c = 0;

        for (int i = 0; i < n; ++i) {
            // ---- macro + melt ramp
            if (p.meltOn) ramp = std::min(1.0f, ramp + dt / std::max(0.25f, p.meltSec));
            else          ramp = std::max(0.0f, ramp - dt / 0.6f);
            const float macro = std::max(p.chaos, ramp * ramp * (3.0f - 2.0f * ramp));
            c = sChaos.tick(macro);
            const float cc = std::pow(c, 1.2f);
            const float eWarb = sWarble.tick(p.warble) * cc;
            const float eDive = sDive.tick(p.dive) * cc;
            const float eStut = sStut.tick(p.stutter) * cc;
            const float eRev  = sRev.tick(p.reverse) * cc;
            const float eCrus = sCrush.tick(p.crush) * cc;
            const float eFilt = sFilt.tick(p.filter) * cc;
            const float eSmea = sSmear.tick(p.smear) * cc;
            const float mix   = sMix.tick(p.mix);
            const float gOut  = sOut.tick(outGainTarget);

            // ---- shared modulators
            const float wWow   = wanderWow.tick(rng, 0.7f + 3.0f * c, sr);
            const float wPitch = wanderPitch.tick(rng, 0.15f + 0.6f * c, sr);
            const float wFilt  = wanderFilt.tick(rng, 0.2f + 2.5f * c, sr);
            const float wSmear = wanderSmear.tick(rng, 0.08f + 0.3f * c, sr);
            lfoPhase += (0.8f + 5.0f * c) * dt; if (lfoPhase >= 1) lfoPhase -= 1;
            const float lfo = std::sin(2.0f * kPi * lfoPhase);

            // ---- stutter / reverse scheduler (runs on a 1/16-note grid)
            if (--countdown <= 0) {
                countdown = std::max(1, (int)(beat * 0.25f));
                if (!active) maybeTrigger(beat, c, eStut, eRev, eDive);
            }

            // pitch-shift ratio for the "dive" stage: sags 0..-19 semitones, wandering
            const float semis = -eDive * 19.0f * (0.55f + 0.45f * wPitch);
            const float ratio = std::exp2(semis / 12.0f);
            const float win = 0.07f * sr;
            pitchPhase += (1.0f - ratio) / win; pitchPhase -= std::floor(pitchPhase);
            const float ph2 = std::fmod(pitchPhase + 0.5f, 1.0f);
            const float g1 = 1.0f - std::fabs(2.0f * pitchPhase - 1.0f);
            const float pitchBlend = std::min(1.0f, eDive * 3.0f);

            // warble delay (tape wow + flutter)
            const float wobDepth = eWarb * 0.012f * sr;
            const float wobD = 0.015f * sr + wobDepth * (0.6f * lfo + 0.4f * wWow);

            // crush settings
            const float bits = 16.0f - eCrus * 13.0f * (0.85f + 0.15f * wFilt);
            const float qStep = std::exp2(-(bits - 1.0f));
            const int   hold = 1 + (int)(eCrus * eCrus * 40.0f * (0.6f + 0.4f * std::fabs(wFilt)));
            const bool  takeSample = (++holdCount >= hold);
            if (takeSample) holdCount = 0;

            // filter settings
            const float cutoff = 18000.0f * std::exp2(-eFilt * 7.0f * (0.55f + 0.45f * wFilt));
            const float q = 0.707f + eFilt * 7.0f;

            // smear settings
            const float smearD = (0.09f + 0.22f * (0.5f + 0.5f * wSmear)) * sr;
            const float fb = std::min(1.03f, eSmea * 1.08f);

            // stutter playback envelope (crossfade between live input and grain)
            const float env = sEnv.tick(active ? 1.0f : 0.0f);
            float grain[2] = { 0, 0 };
            if (active) readGrain(grain);

            float sm[2] = { smearFb[0], smearFb[1] };
            for (int k = 0; k < 2; ++k) {
                const int src = std::min(k, nCh - 1);
                const float dry = ch[src][i];
                hist[k].push(dry);

                float x = dry + (grain[k] - dry) * env;

                // warble
                wob[k].push(x);
                const float xw = wob[k].read(wobD);

                // dive (two-tap granular pitch shift)
                pitch[k].push(xw);
                const float ps = g1 * pitch[k].read(4.0f + pitchPhase * win)
                               + (1.0f - g1) * pitch[k].read(4.0f + ph2 * win);
                x = xw + (ps - xw) * pitchBlend;

                // crush
                if (takeSample) holdVal[k] = (eCrus > 0.001f) ? std::round(x / qStep) * qStep : x;
                x = (eCrus > 0.001f) ? holdVal[k] : x;

                // wandering resonant low-pass
                x = svf[k].lp(x, cutoff, q, sr);

                // smear: diffuse, modulated, cross-fed feedback delay with saturation
                const float other = sm[1 - k];
                float in = x + fb * std::tanh(0.6f * sm[k] + 0.4f * other);
                for (auto& a : ap[k]) in = a.tick(in);
                // band-limit the loop so it melts instead of booming or fizzing
                hpState[k] += 0.004f * (in - hpState[k]);
                float loop = in - hpState[k];
                lpState[k] += 0.35f * (loop - lpState[k]);
                loop = lpState[k];
                smearLine[k].push(loop);
                const float tap = smearLine[k].read(smearD * (k ? 1.13f : 1.0f));
                smearFb[k] = tap;
                x = x + eSmea * 0.9f * tap;

                // DC block, mix, output
                const float y = x - dcX[k] + 0.995f * dcY[k]; dcX[k] = x; dcY[k] = y;
                float out = dry + (y - dry) * mix;
                out = softLimit(out * gOut);
                if (!std::isfinite(out)) { reset(); out = 0; }
                if (k < nCh) ch[k][i] = out;
            }
        }
        chaosOut.store(c);
    }

private:
    void maybeTrigger(float beat, float c, float eStut, float eRev, float eDive) {
        const float r = rng.next();
        const float pStut = eStut * 0.65f;
        const float pRev  = eRev * 0.35f;
        const int avail = (int)slice[0].size();
        if (r < pStut) {
            static const float divs[5] = { 1.0f, 0.5f, 0.25f, 0.125f, 0.0625f }; // 1/4 .. 1/64
            const int idx = std::clamp((int)(rng.next() * 2.0f + c * 3.0f), 0, 4);
            len = std::clamp((int)(beat * divs[idx]), 64, avail);
            repeatsLeft = 2 + (int)(rng.next() * (2.0f + 6.0f * c));
            rev = rng.next() < eRev;
            altRev = rng.next() < eRev * 0.5f;
            rate = (rng.next() < c * 0.25f) ? (rng.next() < 0.5f ? 2.0f : 0.5f) : 1.0f;
            rateMul = (rng.next() < eDive * 0.7f) ? 0.78f + 0.17f * rng.next() : 1.0f; // tape-stop per repeat
        } else if (r < pStut + pRev) {
            len = std::clamp((int)(beat * (rng.next() < 0.5f ? 1.0f : 0.5f)), 64, avail);
            repeatsLeft = 1; rev = true; altRev = false; rate = 1.0f; rateMul = 1.0f;
        } else {
            return;
        }
        // capture the most recent 'len' samples as the slice
        for (int k = 0; k < 2; ++k) {
            const Delay& h = hist[k];
            for (int j = 0; j < len; ++j) slice[k][(size_t)j] = h.buf[(size_t)((h.w - len + j) & h.mask)];
        }
        pos = 0; active = true;
    }

    void readGrain(float* out) {
        const float fadeLen = std::max(16.0f, std::min(0.003f * sr, len * 0.25f));
        const float fpos = rev ? (float)(len - 1) - pos : pos;
        const int i0 = std::clamp((int)fpos, 0, len - 1);
        const int i1 = std::min(i0 + 1, len - 1);
        const float f = fpos - (float)i0;
        const float w = std::min({ 1.0f, pos / fadeLen, ((float)len - pos) / fadeLen });
        for (int k = 0; k < 2; ++k)
            out[k] = (slice[k][(size_t)i0] + (slice[k][(size_t)i1] - slice[k][(size_t)i0]) * f) * std::max(0.0f, w);
        pos += rate;
        if (pos >= (float)len) {
            pos = 0;
            rate *= rateMul;
            if (altRev) rev = !rev;
            if (--repeatsLeft <= 0 || rate < 0.08f) active = false;
        }
    }

    float sr = 48000, bpm = 120;
    Rng rng;
    Wander wanderWow, wanderPitch, wanderFilt, wanderSmear;
    OnePole sChaos, sWarble, sDive, sStut, sRev, sCrush, sFilt, sSmear, sMix, sOut, sEnv;
    Delay hist[2], wob[2], pitch[2], smearLine[2];
    Allpass ap[2][3];
    Svf svf[2];
    std::vector<float> slice[2];
    float holdVal[2] {}, smearFb[2] {}, hpState[2] {}, lpState[2] {}, dcX[2] {}, dcY[2] {};
    int holdCount = 0, countdown = 0;
    float ramp = 0, pitchPhase = 0, lfoPhase = 0;
    // grain state
    bool active = false, rev = false, altRev = false;
    int len = 0, repeatsLeft = 0;
    float pos = 0, rate = 1, rateMul = 1;
    std::atomic<float> chaosOut { 0.0f };
};

} // namespace melt
