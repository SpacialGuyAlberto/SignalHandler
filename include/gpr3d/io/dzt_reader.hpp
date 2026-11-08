#pragma once

#include "gpr3d/io/reader.hpp"

#include <filesystem>

namespace gpr3d::io {

/// Reader and writer for GSSI RADAN (.dzt) Ground Penetrating Radar files.
/// Supports 8-bit, 16-bit, and 32-bit sample words, single and multi-block headers,
/// rhf_range time window, rhf_epsr permittivity, and rh_antname metadata.
class DztReader final : public RadarReader {
public:
    [[nodiscard]] BScan read(
        const std::filesystem::path& path,
        const ReadOptions& options = {}) const override;

    /// Write a single-channel 16-bit GSSI .dzt radargram file.
    static void write(const std::filesystem::path& path, const BScan& bscan);
};

}  // namespace gpr3d::io

