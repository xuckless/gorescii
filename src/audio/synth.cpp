#include "audio/synth.h"

#include <algorithm>
#include <cmath>

namespace tg::audio {
namespace {

constexpr float kTau = 6.2831853f;

struct Noise {
    uint32_t s = 0x12345678u;
    float next() {
        s = s * 1664525u + 1013904223u;
        return static_cast<float>(s >> 8) / 8388607.5f - 1.0f;
    }
};

int samplesFor(float seconds) { return static_cast<int>(seconds * kSampleRate); }

// Exponential decay envelope with a short attack to avoid clicks.
float env(float t, float attack, float decay) {
    float a = attack > 0 ? std::min(1.0f, t / attack) : 1.0f;
    return a * std::exp(-t / decay);
}

float square(float phase) { return std::fmod(phase, 1.0f) < 0.5f ? 1.0f : -1.0f; }

// Noise burst through a one-pole low-pass whose cutoff falls over time.
std::vector<float> blast(float seconds, float decay, float bright, float thumpHz, float thump) {
    std::vector<float> out(static_cast<size_t>(samplesFor(seconds)));
    Noise n;
    float lp = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float k = bright * std::exp(-t / (decay * 0.6f)) + 0.03f;
        lp += (n.next() - lp) * std::min(1.0f, k);
        float body = std::sin(kTau * thumpHz * t * (1.0f - t * 1.5f)) * thump;
        out[i] = (lp * 1.6f + body) * env(t, 0.002f, decay);
    }
    return out;
}

// Square-wave sweep from f0 to f1 Hz.
std::vector<float> sweep(float seconds, float f0, float f1, float decay, float noiseMix = 0) {
    std::vector<float> out(static_cast<size_t>(samplesFor(seconds)));
    Noise n;
    float phase = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        float f = f0 + (f1 - f0) * (t / seconds);
        phase += f / kSampleRate;
        float v = square(phase) * (1 - noiseMix) + n.next() * noiseMix;
        out[i] = v * 0.5f * env(t, 0.004f, decay);
    }
    return out;
}

// Sequence of short tones.
std::vector<float> notes(std::initializer_list<float> freqs, float each) {
    std::vector<float> out;
    float phase = 0;
    for (float f : freqs) {
        int n = samplesFor(each);
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / kSampleRate;
            phase += f / kSampleRate;
            float v = 0.6f * square(phase) + 0.4f * std::sin(kTau * phase);
            out.push_back(v * 0.45f * env(t, 0.003f, each * 0.7f));
        }
    }
    return out;
}

// Low motor hum with filtered noise: doors and lifts.
std::vector<float> rumble(float seconds, float hz) {
    std::vector<float> out(static_cast<size_t>(samplesFor(seconds)));
    Noise n;
    float lp = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        lp += (n.next() - lp) * 0.05f;
        float fade = std::min(1.0f, t / 0.05f) * std::min(1.0f, (seconds - t) / 0.15f);
        out[i] = (0.35f * square(hz * t) * 0.5f + lp * 2.0f) * 0.6f * fade;
    }
    return out;
}

void mixInto(std::vector<float>& dst, const std::vector<float>& src, float gain) {
    if (src.size() > dst.size()) dst.resize(src.size(), 0.0f);
    for (size_t i = 0; i < src.size(); ++i) dst[i] += src[i] * gain;
}

}  // namespace

std::vector<float> synthesize(game::Sfx id) {
    using game::Sfx;
    std::vector<float> s;
    switch (id) {
        case Sfx::Pistol: s = blast(0.25f, 0.06f, 0.9f, 140, 0.6f); break;
        case Sfx::Scattergun: s = blast(0.6f, 0.16f, 0.7f, 70, 0.9f); break;
        case Sfx::Click: s = sweep(0.04f, 1800, 1200, 0.01f, 0.5f); break;
        case Sfx::EnemyShot: s = blast(0.22f, 0.05f, 1.0f, 260, 0.4f); break;
        case Sfx::Fireball: s = sweep(0.45f, 120, 420, 0.25f, 0.6f); break;
        case Sfx::Claw: s = sweep(0.12f, 900, 300, 0.05f, 0.8f); break;
        case Sfx::Pain: s = sweep(0.18f, 420, 180, 0.08f, 0.15f); break;
        case Sfx::EnemyDeath:
            s = sweep(0.6f, 300, 60, 0.25f, 0.3f);
            mixInto(s, blast(0.4f, 0.12f, 0.4f, 50, 0.5f), 0.6f);
            break;
        case Sfx::PlayerHurt: s = sweep(0.16f, 160, 90, 0.08f, 0.2f); break;
        case Sfx::PlayerDeath: s = sweep(1.0f, 220, 40, 0.5f, 0.25f); break;
        case Sfx::Pickup: s = notes({660, 990}, 0.06f); break;
        case Sfx::Door: s = rumble(0.9f, 55); break;
        case Sfx::Lift: s = rumble(1.4f, 42); break;
        case Sfx::Win: s = notes({392, 494, 587, 784, 587, 784}, 0.11f); break;
        case Sfx::Count: break;
    }
    float peak = 0;
    for (float v : s) peak = std::max(peak, std::abs(v));
    if (peak > 0.95f)
        for (float& v : s) v *= 0.95f / peak;
    return s;
}

std::string encodeWav(const std::vector<float>& samples) {
    std::string out;
    auto u32 = [&](uint32_t v) {
        for (int i = 0; i < 4; ++i) out += static_cast<char>((v >> (8 * i)) & 0xff);
    };
    auto u16 = [&](uint16_t v) {
        out += static_cast<char>(v & 0xff);
        out += static_cast<char>(v >> 8);
    };
    uint32_t dataBytes = static_cast<uint32_t>(samples.size() * 2);
    out += "RIFF";
    u32(36 + dataBytes);
    out += "WAVEfmt ";
    u32(16);
    u16(1);  // PCM
    u16(1);  // mono
    u32(kSampleRate);
    u32(kSampleRate * 2);
    u16(2);
    u16(16);
    out += "data";
    u32(dataBytes);
    for (float v : samples) {
        auto q = static_cast<int16_t>(std::lround(std::clamp(v, -1.0f, 1.0f) * 32767.0f));
        u16(static_cast<uint16_t>(q));
    }
    return out;
}

}  // namespace tg::audio
