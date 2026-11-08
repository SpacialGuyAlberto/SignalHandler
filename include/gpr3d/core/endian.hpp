#pragma once

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace gpr3d::detail {

[[nodiscard]] inline std::uint16_t bswap16(std::uint16_t v) noexcept {
    return static_cast<std::uint16_t>((v >> 8U) | (v << 8U));
}

[[nodiscard]] inline std::uint32_t bswap32(std::uint32_t v) noexcept {
    return ((v & 0x000000FFU) << 24U) |
           ((v & 0x0000FF00U) << 8U)  |
           ((v & 0x00FF0000U) >> 8U)  |
           ((v & 0xFF000000U) >> 24U);
}

[[nodiscard]] inline std::uint16_t read_u16_le(const std::uint8_t* p) noexcept {
    std::uint16_t v{};
    std::memcpy(&v, p, sizeof(v));
    if constexpr (std::endian::native == std::endian::big) {
        v = bswap16(v);
    }
    return v;
}

[[nodiscard]] inline std::int16_t read_i16_le(const std::uint8_t* p) noexcept {
    return static_cast<std::int16_t>(read_u16_le(p));
}

[[nodiscard]] inline std::uint32_t read_u32_le(const std::uint8_t* p) noexcept {
    std::uint32_t v{};
    std::memcpy(&v, p, sizeof(v));
    if constexpr (std::endian::native == std::endian::big) {
        v = bswap32(v);
    }
    return v;
}

[[nodiscard]] inline std::int32_t read_i32_le(const std::uint8_t* p) noexcept {
    return static_cast<std::int32_t>(read_u32_le(p));
}

[[nodiscard]] inline float read_f32_le(const std::uint8_t* p) noexcept {
    return std::bit_cast<float>(read_u32_le(p));
}

[[nodiscard]] inline std::uint16_t read_u16_be(const std::uint8_t* p) noexcept {
    std::uint16_t v{};
    std::memcpy(&v, p, sizeof(v));
    if constexpr (std::endian::native == std::endian::little) {
        v = bswap16(v);
    }
    return v;
}

[[nodiscard]] inline std::int16_t read_i16_be(const std::uint8_t* p) noexcept {
    return static_cast<std::int16_t>(read_u16_be(p));
}

[[nodiscard]] inline std::uint32_t read_u32_be(const std::uint8_t* p) noexcept {
    std::uint32_t v{};
    std::memcpy(&v, p, sizeof(v));
    if constexpr (std::endian::native == std::endian::little) {
        v = bswap32(v);
    }
    return v;
}

[[nodiscard]] inline std::int32_t read_i32_be(const std::uint8_t* p) noexcept {
    return static_cast<std::int32_t>(read_u32_be(p));
}

[[nodiscard]] inline float read_f32_be(const std::uint8_t* p) noexcept {
    return std::bit_cast<float>(read_u32_be(p));
}

inline void write_u16_le(std::uint8_t* p, std::uint16_t v) noexcept {
    if constexpr (std::endian::native == std::endian::big) {
        v = bswap16(v);
    }
    std::memcpy(p, &v, sizeof(v));
}

inline void write_i16_le(std::uint8_t* p, std::int16_t v) noexcept {
    write_u16_le(p, static_cast<std::uint16_t>(v));
}

inline void write_u32_le(std::uint8_t* p, std::uint32_t v) noexcept {
    if constexpr (std::endian::native == std::endian::big) {
        v = bswap32(v);
    }
    std::memcpy(p, &v, sizeof(v));
}

inline void write_f32_le(std::uint8_t* p, float v) noexcept {
    write_u32_le(p, std::bit_cast<std::uint32_t>(v));
}

inline void write_u16_be(std::uint8_t* p, std::uint16_t v) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        v = bswap16(v);
    }
    std::memcpy(p, &v, sizeof(v));
}

inline void write_i16_be(std::uint8_t* p, std::int16_t v) noexcept {
    write_u16_be(p, static_cast<std::uint16_t>(v));
}

inline void write_u32_be(std::uint8_t* p, std::uint32_t v) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        v = bswap32(v);
    }
    std::memcpy(p, &v, sizeof(v));
}

inline void write_i32_be(std::uint8_t* p, std::int32_t v) noexcept {
    write_u32_be(p, static_cast<std::uint32_t>(v));
}

inline void write_f32_be(std::uint8_t* p, float v) noexcept {
    write_u32_be(p, std::bit_cast<std::uint32_t>(v));
}

/// Convert a 32-bit IBM System/360 hexadecimal float word (host byte order) to double.
/// Format: 1 sign bit, 7-bit base-16 exponent (excess-64), 24-bit fraction.
[[nodiscard]] inline double ibm_to_double(std::uint32_t ibm) noexcept {
    const std::uint32_t mantissa = ibm & 0x00FFFFFFU;
    if (mantissa == 0U) {
        return 0.0;
    }
    const bool negative = (ibm & 0x80000000U) != 0U;
    const int exp16 = static_cast<int>((ibm >> 24U) & 0x7FU) - 64;
    // Value = (-1)^sign * (mantissa / 2^24) * 16^exp16 = mantissa * 2^(4*exp16 - 24)
    const double val = std::ldexp(static_cast<double>(mantissa), 4 * exp16 - 24);
    return negative ? -val : val;
}

/// Decode EBCDIC text buffer if EBCDIC markers (e.g. 0xC3 = 'C' at start of SEG-Y header) are detected.
[[nodiscard]] inline std::string decode_segy_text_header(const std::uint8_t* buf, std::size_t len) {
    // Standard CP037/EBCDIC-US lookup table
    static constexpr char kEbcdicToAscii[256] = {
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','[','.','<','(','+','!',
        '&',' ',' ',' ',' ',' ',' ',' ',' ',' ',']','$','*',')',';','^',
        '-','/',' ',' ',' ',' ',' ',' ',' ',' ','|',',','%','_','>','?',
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','`',':','#','@','\'','=',
        '"','a','b','c','d','e','f','g','h','i',' ',' ',' ',' ',' ',' ',
        ' ','j','k','l','m','n','o','p','q','r',' ',' ',' ',' ',' ',' ',
        ' ','~','s','t','u','v','w','x','y','z',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        '{','A','B','C','D','E','F','G','H','I',' ',' ',' ',' ',' ',' ',
        '}','J','K','L','M','N','O','P','Q','R',' ',' ',' ',' ',' ',' ',
        '\\',' ','S','T','U','V','W','X','Y','Z',' ',' ',' ',' ',' ',' ',
        '0','1','2','3','4','5','6','7','8','9',' ',' ',' ',' ',' ',' '
    };

    // Detect EBCDIC: SEG-Y headers begin with "C 1 " or "C01 "; in EBCDIC 'C' is 0xC3 and space is 0x40
    std::size_t ebcdic_score = 0;
    std::size_t ascii_score = 0;
    for (std::size_t i = 0; i < len && i < 160; ++i) {
        const std::uint8_t b = buf[i];
        if (b == 0x40 || (b >= 0xC1 && b <= 0xE9) || (b >= 0xF0 && b <= 0xF9)) {
            ++ebcdic_score;
        }
        if (b == 0x20 || (b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z') || (b >= '0' && b <= '9')) {
            ++ascii_score;
        }
    }

    const bool is_ebcdic = (ebcdic_score > ascii_score);
    std::string out;
    out.reserve(len + len / 80 + 2);
    for (std::size_t i = 0; i < len; ++i) {
        char c = is_ebcdic ? kEbcdicToAscii[buf[i]] : static_cast<char>(buf[i]);
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            c = ' ';
        }
        out.push_back(c);
        if ((i + 1) % 80 == 0 && c != '\n') {
            out.push_back('\n');
        }
    }
    return out;
}

}  // namespace gpr3d::detail

