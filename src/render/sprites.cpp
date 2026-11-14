#include "render/sprites.h"

#include <array>
#include <initializer_list>
#include <string_view>

namespace tg::render {
namespace {

using gfx::Rgb;

struct Pal {
    char key;
    Rgb c;
};

// Shared palette. '.' is transparent.
constexpr Pal kPalette[] = {
    {'k', {22, 22, 24}},    {'w', {230, 230, 225}}, {'g', {85, 98, 58}},    {'G', {130, 142, 88}},
    {'r', {255, 70, 40}},   {'s', {175, 125, 95}},  {'m', {115, 118, 128}}, {'M', {170, 175, 185}},
    {'b', {70, 48, 30}},    {'p', {110, 38, 58}},   {'P', {165, 68, 82}},   {'h', {225, 212, 182}},
    {'y', {255, 220, 60}},  {'t', {240, 240, 228}}, {'o', {255, 150, 30}},  {'c', {60, 200, 90}},
    {'B', {50, 90, 200}},   {'L', {110, 160, 255}}, {'d', {120, 25, 20}},   {'D', {170, 40, 30}},
    {'f', {255, 245, 180}},
};

Image build(std::initializer_list<std::string_view> rows) {
    Image img;
    img.h = static_cast<int>(rows.size());
    for (auto r : rows) img.w = std::max(img.w, static_cast<int>(r.size()));
    img.px.assign(static_cast<size_t>(img.w * img.h), Rgb{});
    img.opaque.assign(static_cast<size_t>(img.w * img.h), 0);
    int y = 0;
    for (auto r : rows) {
        for (int x = 0; x < static_cast<int>(r.size()); ++x) {
            for (const Pal& p : kPalette) {
                if (p.key == r[static_cast<size_t>(x)]) {
                    img.px[static_cast<size_t>(y * img.w + x)] = p.c;
                    img.opaque[static_cast<size_t>(y * img.w + x)] = 1;
                }
            }
        }
        ++y;
    }
    return img;
}

std::array<Image, static_cast<size_t>(SpriteId::Count)> makeAll() {
    std::array<Image, static_cast<size_t>(SpriteId::Count)> a;
    auto set = [&](SpriteId id, Image img) { a[static_cast<size_t>(id)] = std::move(img); };

    // Grunt: armoured trooper with a glowing visor and a sidearm.
    set(SpriteId::Grunt, build({
        "......kkkk......",
        ".....kGGGGk.....",
        "....kGggggGk....",
        "....kgrrrrgk....",
        "....kggggggk....",
        ".....kgggk......",
        "...kkGGGGGGkk...",
        "..kGGggggggGGk..",
        "..kGgggggggggk..",
        ".kGgkggggggkgGk.",
        ".kgk.kggggk.kgk.",
        ".kgk.kggggk.kgk.",
        ".ksk.kggggk.ksmm",
        "..k..kGGGGk..kmm",
        ".....kgkkgk.....",
        ".....kgkkgk.....",
        "....kggk.kggk...",
        "....kgk...kgk...",
        "....kgk...kgk...",
        "....kbk...kbk...",
        "...kbbk...kbbk..",
        "...kkkk...kkkk..",
    }));
    set(SpriteId::GruntFire, build({
        "......kkkk......",
        ".....kGGGGk.....",
        "....kGggggGk....",
        "....kgrrrrgk....",
        "....kggggggk....",
        ".....kgggk......",
        "...kkGGGGGGkk...",
        "..kGGggggggGGk..",
        "..kGgggggggggkkk",
        ".kGgkggggggkkmmM",
        ".kgk.kggggk.kmMfo",
        ".kgk.kggggk..kfyfo",
        ".ksk.kggggk...ofo",
        "..k..kGGGGk.......",
        ".....kgkkgk.....",
        ".....kgkkgk.....",
        "....kggk.kggk...",
        "....kgk...kgk...",
        "....kgk...kgk...",
        "....kbk...kbk...",
        "...kbbk...kbbk..",
        "...kkkk...kkkk..",
    }));
    set(SpriteId::GruntDead, build({
        "..........kkk.......",
        "....kkkkkkGrrk......",
        "..kkGGggggggggkk....",
        ".kDDggdggGGgggbbk...",
        "kDDdDDDggggkkbbbbk..",
        ".kkkkDDDDkkk..kkkk..",
    }));

    // Brute: hulking horned beast with yellow eyes.
    set(SpriteId::Brute, build({
        "..h..........h..",
        "..hh........hh..",
        "...hkkkkkkkkh...",
        "...kPPPPPPPPk...",
        "..kPPyyPPyyPPk..",
        "..kPPPPPPPPPPk..",
        "..kPPkttttkPPk..",
        "...kPPkkkkPPk...",
        ".kkPPPPPPPPPPkk.",
        "kPPPppppppppPPPk",
        "kPPppppppppppPPk",
        "kPpkppppppppkpPk",
        "kPpkppppppppkpPk",
        "kPpkppppppppkpPk",
        "khhkpppPPpppkhhk",
        "khhk.kppppk.khhk",
        ".kk..kppppk..kk.",
        ".....kppkppk....",
        "....kPpk.kpPk...",
        "....kppk.kppk...",
        "....kppk.kppk...",
        "...kPPPk.kPPPk..",
        "...kkkkk.kkkkk..",
    }));
    set(SpriteId::BruteFire, build({
        "..h..........h..",
        "..hh........hh..",
        "...hkkkkkkkkh...",
        "...kPPPPPPPPk...",
        "..kPPyyPPyyPPk..",
        "..kPPPPPPPPPPk..",
        "..kPPkttttkPPk..",
        "..kPkttooottkPk.",
        "..kPPktooottPPk.",
        "kPPPPkkooookPPPk",
        "kPPppppppppppPPk",
        "kPpkppppppppkpPk",
        "kPpkpppoypppkpPk",
        "kPpkppoyfyppkpPk",
        "khhkpppoyopppkhhk",
        "khhk.kppppk.khhk",
        ".kk..kppppk..kk.",
        ".....kppkppk....",
        "....kPpk.kpPk...",
        "....kppk.kppk...",
        "....kppk.kppk...",
        "...kPPPk.kPPPk..",
        "...kkkkk.kkkkk..",
    }));
    set(SpriteId::BruteDead, build({
        "..h...............h...",
        "...hkkkkkkkkkkkkkkh...",
        "..kPPyPPPPPPPPPPPyPk..",
        ".kDDPPppppppDDppPPPPk.",
        "kDDdDDppppDDddDDppphhk",
        ".kkkkkkkkkkkkkkkkkkkk.",
    }));

    set(SpriteId::Medkit, build({
        ".kkkkkkkkkk.",
        "kwwwwwwwwwwk",
        "kwwwwccwwwwk",
        "kwwwwccwwwwk",
        "kwwccccccwwk",
        "kwwccccccwwk",
        "kwwwwccwwwwk",
        "kwwwwccwwwwk",
        "kwwwwwwwwwwk",
        ".kkkkkkkkkk.",
    }));
    set(SpriteId::Ammo, build({
        "..k..k..k..k..",
        ".kyk.kyk.kyk..",
        ".kok.kok.kok..",
        "kkkkkkkkkkkkkk",
        "kbbbbbbbbbbbbk",
        "kbyyyyyyyyyybk",
        "kbbbbbbbbbbbbk",
        "kkkkkkkkkkkkkk",
    }));
    set(SpriteId::Armor, build({
        "..kk......kk..",
        ".kLBk....kBLk.",
        ".kBBBkkkkBBBk.",
        "kBBLBBBBBBLBBk",
        "kBBBBBLLBBBBBk",
        ".kBBBBLLBBBBk.",
        ".kBBBBBBBBBBk.",
        "..kBBBBBBBBk..",
        "..kBBBLLBBBk..",
        "...kkBBBBkk...",
        ".....kkkk.....",
    }));

    // First-person weapons, bottom-centre anchored.
    set(SpriteId::Pistol, build({
        "........kkkk........",
        ".......kMMMMk.......",
        ".......kmMMmk.......",
        ".......kmmmmk.......",
        "......kkmmmmkk......",
        "......kmmmmmmk......",
        ".....kkmmmmmmkk.....",
        ".....kmmkkkkmmk.....",
        "....kssmkbbkmssk....",
        "...kssssbbbbsssk....",
        "...ksssskbbksssk....",
        "..ksssssskksssssk...",
        "..kssssssssssssssk..",
        ".ksssssssssssssssk..",
        ".ksssssssssssssssk..",
    }));
    set(SpriteId::PistolFire, build({
        "......o.fyf.o.......",
        ".......ofyyfo.......",
        "......offfffo.......",
        ".......kfMMfk.......",
        ".......kMMMMk.......",
        ".......kmMMmk.......",
        ".......kmmmmk.......",
        "......kkmmmmkk......",
        "......kmmmmmmk......",
        ".....kkmmmmmmkk.....",
        ".....kmmkkkkmmk.....",
        "....kssmkbbkmssk....",
        "...kssssbbbbsssk....",
        "..ksssssskksssssk...",
        ".ksssssssssssssssk..",
    }));
    set(SpriteId::Scattergun, build({
        "..........kkkkkk..........",
        ".........kMMkkMMk.........",
        ".........kmmkkmmk.........",
        ".........kmmkkmmk.........",
        "........kkmmmmmmkk........",
        "........kmmmmmmmmk........",
        ".......kbbbbbbbbbbk.......",
        ".......kbbbbbbbbbbk.......",
        "......kbbbkmmmmkbbbk......",
        ".....ksbbbkkkkkkbbbsk.....",
        "....ksssbbbbbbbbbbsssk....",
        "...ksssssbbbbbbbbsssssk...",
        "..kssssssskbbbbksssssssk..",
        ".ksssssssssskkkssssssssk..",
        ".ksssssssssssssssssssssk..",
    }));
    set(SpriteId::ScattergunFire, build({
        ".......o.fyyyyf.o.........",
        "......ofyyfyyfyyfo........",
        ".......offffffffo.........",
        ".........kfMkkMfk.........",
        ".........kmmkkmmk.........",
        "........kkmmmmmmkk........",
        "........kmmmmmmmmk........",
        ".......kbbbbbbbbbbk.......",
        "......kbbbkmmmmkbbbk......",
        ".....ksbbbkkkkkkbbbsk.....",
        "....ksssbbbbbbbbbbsssk....",
        "...ksssssbbbbbbbbsssssk...",
        "..kssssssskbbbbksssssssk..",
        ".ksssssssssskkkssssssssk..",
        ".ksssssssssssssssssssssk..",
    }));
    return a;
}

}  // namespace

const Image& sprite(SpriteId id) {
    static const auto all = makeAll();
    return all[static_cast<size_t>(id)];
}

}  // namespace tg::render
