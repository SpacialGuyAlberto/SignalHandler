#pragma once

#include "gpr3d/io/reader.hpp"

#include <filesystem>

namespace gpr3d::io {

/// Reader and writer for MALA GeoScience RAMAC (.rd3 + .rad + optional .cor) GPR files.
class Rd3Reader final : public RadarReader {
public:
    [[nodiscard]] BScan read(
        const std::filesystem::path& path,
        const ReadOptions& options = {}) const override;

    /// Write a BScan as <stem>.rd3 (16-bit binary traces), <stem>.rad (ASCII header),
    /// and <stem>.cor (MALA GPS coordinate file) if traces carry WGS84 coordinates.
    static void write(const std::filesystem::path& rd3_path, const BScan& bscan);
};

}  // namespace gpr3d::io

