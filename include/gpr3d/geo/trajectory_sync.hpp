#pragma once

#include "gpr3d/core/types.hpp"

#include <span>

namespace gpr3d::geo {

struct LeverArmOffset {
    double forward_m{0.0};  ///< Antenna offset forward of GNSS receiver along travel direction (m)
    double right_m{0.0};    ///< Antenna offset to the right of GNSS receiver (m)
    double up_m{0.0};       ///< Antenna vertical offset relative to GNSS phase center (m, usually negative)
};

/// Synchronize and interpolate a sequence of GNSS fixes onto all traces of a B-scan.
/// Supports trace-indexed fixes, timestamp-synchronized fixes, or proportional arc-length mapping.
/// Also computes per-trace heading, cumulative distance, and optional lever-arm correction.
void synchronize_trajectory(
    BScan& bscan,
    std::span<const GpsFix> gps_fixes,
    const LeverArmOffset& lever_arm = {});

/// Synthesize a straight-line georeferenced trajectory starting at `origin` along `azimuth_deg`
/// using the B-scan's `trace_spacing_m`. Useful when surveying along a known trench line without live GPS.
void assign_linear_trajectory(
    BScan& bscan,
    const Wgs84Coord& origin,
    double azimuth_deg,
    const LeverArmOffset& lever_arm = {});

/// Recompute per-trace headings and cumulative track distances from existing `position_wgs84` values.
void update_headings_and_distances(BScan& bscan, const LeverArmOffset& lever_arm = {});

}  // namespace gpr3d::geo

