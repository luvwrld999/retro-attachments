#pragma once
#include <SDL.h>
#include <cmath>
#include <map>
#include <sstream>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "assets.hpp"
#include "stb_image.h"
struct Color {
  unsigned char r, g, b, a = 255;
};
struct Theme {
  std::string name;
  Color bg, panel, card, accent, text, muted;
};
inline const Theme themes[] = {{"Midnight Blue",
                                {10, 15, 29},
                                {17, 24, 42},
                                {23, 34, 55},
                                {100, 207, 255},
                                {239, 246, 255},
                                {151, 169, 193}},
                               {"OLED Black",
                                {0, 0, 0},
                                {10, 10, 12},
                                {21, 21, 25},
                                {227, 232, 242},
                                {250, 250, 250},
                                {165, 165, 178}},
                               {"Retro CRT",
                                {6, 17, 13},
                                {12, 29, 21},
                                {17, 42, 30},
                                {130, 244, 148},
                                {222, 255, 225},
                                {133, 176, 146}},
                               {"Neon Purple",
                                {17, 10, 29},
                                {28, 17, 45},
                                {43, 26, 64},
                                {203, 151, 255},
                                {248, 236, 255},
                                {176, 152, 199}}};
struct Painter {
  SDL_Renderer *r;
  SDL_Texture *font = nullptr;
  stbtt_bakedchar chars[224]{};
  Painter(SDL_Renderer *renderer) : r(renderer) {
    std::vector<unsigned char> bitmap(2048 * 2048);
    stbtt_BakeFontBitmap(font_data, 0, 40, bitmap.data(), 2048, 2048, 32, 224,
                         chars);
    std::vector<unsigned char> rgba(2048 * 2048 * 4, 255);
    for (size_t i = 0; i < bitmap.size(); ++i)
      rgba[i * 4 + 3] = bitmap[i];
    font = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32,
                             SDL_TEXTUREACCESS_STATIC, 2048, 2048);
    SDL_UpdateTexture(font, nullptr, rgba.data(), 2048 * 4);
    SDL_SetTextureBlendMode(font, SDL_BLENDMODE_BLEND);
  }
  ~Painter() { SDL_DestroyTexture(font); }
  void color(Color c) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a); }
  void box(float x, float y, float w, float h, Color c, int radius = 0) {
    color(c);
    if (!radius) {
      SDL_FRect q{x, y, w, h};
      SDL_RenderFillRectF(r, &q);
      return;
    }
    radius = std::min(radius, (int)h / 2);
    for (int dy = 0; dy < (int)h; ++dy) {
      float inset = 0;
      if (dy < radius)
        inset =
            radius -
            std::sqrt(float(radius * radius - (radius - dy) * (radius - dy)));
      else if (dy >= h - radius)
        inset = radius -
                std::sqrt(float(radius * radius -
                                (dy - (h - radius)) * (dy - (h - radius))));
      SDL_RenderDrawLineF(r, x + inset, y + dy, x + w - inset, y + dy);
    }
  }
  static Color mix(Color a, Color b, float amount) {
    amount = std::clamp(amount, 0.f, 1.f);
    return {static_cast<unsigned char>(a.r + (b.r - a.r) * amount),
            static_cast<unsigned char>(a.g + (b.g - a.g) * amount),
            static_cast<unsigned char>(a.b + (b.b - a.b) * amount),
            static_cast<unsigned char>(a.a + (b.a - a.a) * amount)};
  }
  void gradient(int x, int y, int w, int h, Color top, Color bottom) {
    for (int i = 0; i < h; ++i) {
      color(mix(top, bottom, float(i) / std::max(1, h - 1)));
      SDL_RenderDrawLine(r, x, y + i, x + w - 1, y + i);
    }
  }
  void ring(int x, int y, int radius, float progress, Color accent, Color track,
            int thickness = 4) {
    progress = std::clamp(progress, 0.f, 1.f);
    for (int i = 0; i < 96; ++i) {
      color(i < progress * 96 ? accent : track);
      float a = (i / 96.f) * 6.2831853f - 1.5707963f,
            b = ((i + 1) / 96.f) * 6.2831853f - 1.5707963f;
      for (int line = 0; line < thickness; ++line) {
        float r = radius - line;
        SDL_RenderDrawLineF(this->r, x + std::cos(a) * r, y + std::sin(a) * r,
                            x + std::cos(b) * r, y + std::sin(b) * r);
      }
    }
  }
  void pill(const std::string &value, int x, int y, Color ink, Color fill,
            int size = 15) {
    int w = int(width(value, size)) + 24;
    box(x, y, w, size + 16, fill, (size + 16) / 2);
    text(value, x + 12, y + 5, size, ink);
  }
  void label(std::string value, float x, float y, float size, Color ink,
             float maxw) {
    if (width(value, size) > maxw) {
      while (!value.empty() && width(value + "...", size) > maxw) {
        std::size_t i = value.size() - 1;
        while (i > 0 && (static_cast<unsigned char>(value[i]) & 0xc0) == 0x80)
          --i;
        value.resize(i);
      }
      value += "...";
    }
    text(value, x, y, size, ink, maxw);
  }
  void cartridge(int x, int y, int w, int h, Color accent, Color background,
                 int seed) {
    box(x, y, w, h, mix(background, accent, .18f), 10);
    // Original geometric placeholder, distinct from downloaded game artwork.
    for (int i = 0; i < 5; ++i) {
      int sx = x + 8 + ((seed * 13 + i * 23) % (std::max(1, w - 20)));
      int sy = y + 8 + ((seed * 7 + i * 17) % (std::max(1, h - 20)));
      box(sx, sy, 4, 4, mix(background, accent, .38f), 1);
    }
    int cw = w * 2 / 3, ch = h * 2 / 3, cx = x + (w - cw) / 2,
        cy = y + (h - ch) / 2;
    box(cx, cy, cw, ch, mix(background, accent, .4f), 5);
    box(cx + 5, cy + 5, cw - 10, ch * 3 / 5, mix(background, accent, .12f), 3);
    ring(cx + cw / 2, cy + ch / 3, std::max(5, cw / 6), 1, accent,
         mix(background, accent, .12f), 2);
    for (int i = 0; i < 4; ++i)
      box(cx + 7 + i * (cw - 14) / 4, cy + ch - 10, 3, 6, accent, 1);
  }
  void outline(float x, float y, float w, float h, Color c) {
    color(c);
    for (int i = 0; i < 2; ++i) {
      SDL_FRect q{x - i, y - i, w + 2 * i, h + 2 * i};
      SDL_RenderDrawRectF(r, &q);
    }
  }
  static std::vector<int> codes(const std::string &s) {
    std::vector<int> out;
    for (size_t i = 0; i < s.size();) {
      unsigned char c = s[i++];
      int code = c;
      if (c >= 192 && c < 224 && i < s.size())
        code = ((c & 31) << 6) | (s[i++] & 63);
      else if (c >= 224) {
        while (i < s.size() && ((unsigned char)s[i] & 192) == 128)
          ++i;
        code = '?';
      }
      if (code < 32 || code > 255)
        code = '?';
      out.push_back(code);
    }
    return out;
  }
  float width(const std::string &s, float size) {
    float w = 0;
    for (int c : codes(s))
      w += chars[c - 32].xadvance * size / 40;
    return w;
  }
  void text(std::string s, float x, float y, float size, Color c,
            float maxw = 2000) {
    SDL_SetTextureColorMod(font, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(font, c.a);
    float px = 0, py = 0;
    float scale = size / 40;
    for (int code : codes(s)) {
      auto &ch = chars[code - 32];
      if (px * scale + ch.xadvance * scale > maxw)
        break;
      stbtt_aligned_quad q;
      stbtt_GetBakedQuad(chars, 2048, 2048, code - 32, &px, &py, &q, 1);
      SDL_Rect source{int(q.s0 * 2048), int(q.t0 * 2048),
                      int((q.s1 - q.s0) * 2048), int((q.t1 - q.t0) * 2048)};
      SDL_FRect dest{x + q.x0 * scale, y + size + q.y0 * scale,
                     (q.x1 - q.x0) * scale, (q.y1 - q.y0) * scale};
      SDL_RenderCopyF(r, font, &source, &dest);
    }
  }
  void wrap(const std::string &s, float x, float y, float size, Color c,
            float maxw, int lines = 3) {
    std::string line, word;
    std::istringstream stream(s);
    int n = 0;
    while (stream >> word) {
      if (!line.empty() && width(line + " " + word, size) > maxw) {
        text(line, x, y + n * (size + 7), size, c, maxw);
        line.clear();
        if (++n >= lines)
          return;
      }
      if (!line.empty())
        line += " ";
      line += word;
    }
    if (n < lines)
      text(line, x, y + n * (size + 7), size, c, maxw);
  }
  void bar(int x, int y, int w, float fraction, Color accent, Color track) {
    box(x, y, w, 5, track, 2);
    box(x, y, w * std::clamp(fraction, 0.f, 1.f), 5, accent, 2);
  }
  SDL_Texture *texture(const std::string &bytes) {
    int w = 0, h = 0, n = 0;
    auto *pixels = stbi_load_from_memory((const unsigned char *)bytes.data(),
                                         bytes.size(), &w, &h, &n, 4);
    if (!pixels)
      return nullptr;
    auto *t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32,
                                SDL_TEXTUREACCESS_STATIC, w, h);
    if (t) {
      SDL_UpdateTexture(t, nullptr, pixels, w * 4);
      SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    }
    stbi_image_free(pixels);
    return t;
  }
  void image(SDL_Texture *t, int x, int y, int w, int h) {
    if (!t)
      return;
    int iw, ih;
    SDL_QueryTexture(t, nullptr, nullptr, &iw, &ih);
    float scale = std::min(float(w) / iw, float(h) / ih);
    SDL_FRect d{x + (w - iw * scale) / 2, y + (h - ih * scale) / 2, iw * scale,
                ih * scale};
    SDL_RenderCopyF(r, t, nullptr, &d);
  }
};
