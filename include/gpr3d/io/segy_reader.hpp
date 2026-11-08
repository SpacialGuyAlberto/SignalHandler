#pragma once

#include "gpr3d/io/reader.hpp"

#include <filesystem>

namespace gpr3d::io {

/// Reader and writer for SEG-Y (Rev 1/2) GPR radargram files (.sgy, .segy).
/// Supports big-endian and little-endian headers, IBM 360 float (format 1), int32 (format 2),
/// int16 (format 3), IEEE 754 float (format 5), int8 (format 8), and embedded trace coordinates.
class SegyReader final : public RadarReader {
public:
    [[nodiscard]] BScan read(
        const std::filesystem::path& path,
        const ReadOptions& options = {}) const override;

    /// Write a BScan (including WGS84 trace coordinates in arc-seconds) to standard IEEE SEG-Y.
    static void write(const std::filesystem::path& path, const BScan& bscan);
};

}  // namespace gpr3d::io

