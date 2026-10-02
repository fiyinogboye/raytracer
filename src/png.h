#pragma once
// Minimal dependency-free PNG writer (8-bit RGB, uncompressed "stored" deflate
// blocks). Files are larger than a real encoder's, but it needs no libpng/stb.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace png_detail {
inline uint32_t crc32(uint32_t crc, const uint8_t* buf, size_t len) {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (size_t i = 0; i < len; ++i) crc = table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

inline void be32(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back(v >> 24); o.push_back(v >> 16); o.push_back(v >> 8); o.push_back(v);
}

inline void chunk(std::vector<uint8_t>& o, const char type[4], const std::vector<uint8_t>& data) {
    be32(o, static_cast<uint32_t>(data.size()));
    std::vector<uint8_t> td(type, type + 4);
    td.insert(td.end(), data.begin(), data.end());
    o.insert(o.end(), td.begin(), td.end());
    be32(o, crc32(0, td.data(), td.size()));
}
}  // namespace png_detail

inline bool writePNG(const std::string& path, int w, int h, const std::vector<uint8_t>& rgb) {
    using namespace png_detail;

    // Raw scanlines, each prefixed with filter byte 0.
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(h) * (w * 3 + 1));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgb.begin() + static_cast<size_t>(y) * w * 3,
                   rgb.begin() + static_cast<size_t>(y + 1) * w * 3);
    }

    // zlib stream made of stored blocks.
    std::vector<uint8_t> z = {0x78, 0x01};
    for (size_t off = 0; off < raw.size(); off += 65535) {
        size_t n = std::min<size_t>(65535, raw.size() - off);
        bool last = off + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(n & 0xFF); z.push_back(n >> 8);
        z.push_back(~n & 0xFF); z.push_back((~n >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + off, raw.begin() + off + n);
    }
    uint32_t a = 1, b = 0;
    for (uint8_t byte : raw) { a = (a + byte) % 65521; b = (b + a) % 65521; }
    be32(z, (b << 16) | a);

    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    be32(ihdr, w); be32(ihdr, h);
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});  // 8-bit, RGB, deflate, no filter, no interlace
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    return true;
}
