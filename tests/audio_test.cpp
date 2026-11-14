#include "audio/synth.h"
#include "check.h"

#include <cmath>

using namespace tg;

TEST(every_effect_synthesizes_and_stays_in_range) {
    for (int i = 0; i < static_cast<int>(game::Sfx::Count); ++i) {
        auto s = audio::synthesize(static_cast<game::Sfx>(i));
        CHECK(!s.empty());
        float peak = 0;
        for (float v : s) peak = std::max(peak, std::abs(v));
        CHECK(peak > 0.05f && peak <= 1.0f);
    }
}

TEST(wav_header_is_well_formed) {
    std::string w = audio::encodeWav({0.0f, 1.0f, -1.0f});
    CHECK(w.size() == 44 + 6);
    CHECK(w.compare(0, 4, "RIFF") == 0 && w.compare(8, 4, "WAVE") == 0 && w.compare(36, 4, "data") == 0);
    CHECK(static_cast<unsigned char>(w[44 + 2]) == 0xff && static_cast<unsigned char>(w[44 + 3]) == 0x7f);
}
