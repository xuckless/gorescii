#pragma once
// CPU take on the AcerolaFX ASCII shader: per-cell luminance picks a fill
// glyph; Difference-of-Gaussians + Sobel edges, voted per cell, pick line
// glyphs that follow silhouettes; cell colour comes from the source image.
#include <string>
#include <vector>

#include "gfx/cells.h"
#include "gfx/framebuffer.h"

namespace tg::gfx {

struct AsciiSettings {
    // Glyphs from darkest to brightest (UTF-8 allowed, one code point each).
    std::u32string ramp = U" .:coPO?@#";
    // Edge glyphs: vertical, horizontal, '/' diagonal, '\' diagonal.
    char32_t edgeGlyphs[4] = {U'|', U'_', U'/', U'\\'};

    float exposure = 1.25f;    // luminance multiplier before quantising
    float attenuation = 0.9f;  // gamma on luminance (<1 brightens mids)

    bool edges = true;
    float dogSigma = 1.0f;      // inner Gaussian
    float dogK = 1.6f;          // outer sigma = sigma * k
    float dogTau = 1.0f;        // weight of the outer blur
    float dogThreshold = 0.05f;
    bool depthEdges = true;
    float depthThreshold = 0.08f;  // relative depth jump that counts as an edge
    bool surfaceEdges = true;      // edges where Framebuffer::surface changes
    // Edge pixels a cell needs, as a fraction of cellW + cellH (a straight line
    // across a 4x8 cell covers 4 or 8 pixels depending on its direction).
    float edgeCoverage = 0.3f;

    // Colour: 0 = monochrome glyphs in `tint`, 1 = full image colour.
    float colorBlend = 1.0f;
    Rgb tint{255, 255, 255};
    // Pushes colours towards full brightness so dark cells aren't darkened twice.
    float colorNormalize = 0.6f;
    // Background as a fraction of the cell colour (0 = black terminal background).
    float backgroundBlend = 0.0f;
};

// Edge presets. Lines: texture detail (panel seams, tiles, mortar) is drawn
// in line glyphs too, the busier AcerolaFX look. Clean: only geometry gets
// lines and texture stays as fill. Off: fill glyphs only.
enum class EdgeStyle { Lines, Clean, Off };
void applyEdgeStyle(AsciiSettings& s, EdgeStyle style);
const char* edgeStyleName(EdgeStyle style);

class AsciiShader {
public:
    AsciiSettings settings;

    // `fb` must be cellW*grid.cols by cellH*grid.rows. Writes every cell.
    void apply(const Framebuffer& fb, int cellW, int cellH, CellGrid& grid);

    // Edge direction bucket for a gradient (y grows downward); -1 = no edge.
    static int directionBucket(float gx, float gy);

private:
    void computeEdges(const Framebuffer& fb);
    std::vector<float> lum_, blurA_, blurB_, tmp_, dog_, logDepth_;
    std::vector<signed char> dir_;  // per pixel bucket or -1
    std::vector<float> kernel_;
};

}  // namespace tg::gfx
