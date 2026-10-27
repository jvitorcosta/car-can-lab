// Copied from spotify-pokemon-deck src/ui/termfont.cpp @ 9ced38d.
#include "termfont.h"
#include "../fonts/vt323_17.h"
#include "../fonts/vt323_19.h"
#include "../fonts/vt323_24.h"
#include "../fonts/vt323_36.h"

namespace termfont {

namespace {

const Font* const FONTS[N_FACES] = {&VT323_17, &VT323_19, &VT323_24, &VT323_36};

const Glyph* glyphOf(const Font& f, unsigned char c) {
    if (c < f.first || c > f.last) return nullptr;
    return &f.glyph[c - f.first];
}

// The tallest ink above the baseline of the characters first..last.
int above(const Font& ft, int first, int last) {
    int a = 0;
    for (int c = first; c <= last; c++) {
        const Glyph& g = ft.glyph[c - ft.first];
        if (g.height > 0 && -g.yOffset > a) a = -g.yOffset;
    }
    return a;
}

}  // namespace

const Font& font(Face f) {
    const int i = static_cast<int>(f);
    return *FONTS[i >= 0 && i < N_FACES ? i : 0];
}

int capHeight(Face f) {
    const Glyph* g = glyphOf(font(f), 'H');
    return g ? -g->yOffset : 0;
}

int ascent(Face f) {
    const Font& ft = font(f);
    return above(ft, ft.first, ft.last < 0x7E ? ft.last : 0x7E);
}

int accentAscent(Face f) {
    const Font& ft = font(f);
    return above(ft, ft.first, ft.last);
}

int descent(Face f) {
    const Font& ft = font(f);
    int d = 0;
    for (int c = ft.first; c <= ft.last; c++) {
        const Glyph& g = ft.glyph[c - ft.first];
        if (g.height > 0 && g.yOffset + g.height > d) d = g.yOffset + g.height;
    }
    return d;
}

int cell(Face f) {
    const Glyph* g = glyphOf(font(f), 'M');
    return g ? g->xAdvance : 0;
}

int advance(Face f, const char* s) {
    const Font& ft = font(f);
    int w = 0;
    for (; *s; s++) {
        const Glyph* g = glyphOf(ft, static_cast<unsigned char>(*s));
        if (g) w += g->xAdvance;
    }
    return w;
}

int width(const Font& f, const char* s) {
    int w = 0;
    for (; *s; s++) {
        const Glyph* g = glyphOf(f, static_cast<unsigned char>(*s));
        if (!g) continue;
        w += s[1] ? g->xAdvance : g->xOffset + g->width;
    }
    return w;
}

int width(Face f, const char* s) { return width(font(f), s); }

}  // namespace termfont
