#pragma once
// Procedural sound effects rendered to 16-bit mono WAV bytes.
#include <cstdint>
#include <string>
#include <vector>

#include "game/sound.h"

namespace tg::audio {

constexpr int kSampleRate = 22050;

// Samples in -1..1 for one effect.
std::vector<float> synthesize(game::Sfx id);
// A complete RIFF/WAVE file.
std::string encodeWav(const std::vector<float>& samples);

}  // namespace tg::audio
