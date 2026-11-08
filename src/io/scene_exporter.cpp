#include "gpr3d/io/scene_exporter.hpp"
#include "gpr3d/core/endian.hpp"
#include "gpr3d/geo/crs_transform.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace gpr3d::io {

namespace {

std::vector<const scene::Mesh3D*> collect_meshes(
    const scene::Scene3D& scene,
    bool include_ground) {
    std::vector<const scene::Mesh3D*> list;
    if (include_ground && !scene.ground_surface.empty()) {
        list.push_back(&scene.ground_surface);
    }
    for (const auto& hz : scene.horizons) {
        if (!hz.mesh.empty()) list.push_back(&hz.mesh);
    }
    for (const auto& cond : scene.conduits) {
        if (!cond.mesh.empty()) list.push_back(&cond.mesh);
    }
    for (const auto& an : scene.anomalies) {
        if (!an.mesh.empty()) list.push_back(&an.mesh);
    }
    return list;
}

void append_f32_le(std::vector<std::uint8_t>& buf, float val) {
    const std::size_t off = buf.size();
    buf.resize(off + 4);
    detail::write_f32_le(buf.data() + off, val);
}

void append_u32_le(std::vector<std::uint8_t>& buf, std::uint32_t val) {
    const std::size_t off = buf.size();
    buf.resize(off + 4);
    detail::write_u32_le(buf.data() + off, val);
}

}  // namespace

void SceneExporter3D::write_obj(
    const std::filesystem::path& obj_path,
    const scene::Scene3D& scene,
    const SceneExportOptions& opts) {
    auto mtl_path = obj_path;
    mtl_path.replace_extension(".mtl");

    std::ofstream out_obj(obj_path, std::ios::out | std::ios::trunc);
    if (!out_obj) {
        throw std::runtime_error("Cannot open OBJ file for writing: " + obj_path.string());
    }
    std::ofstream out_mtl(mtl_path, std::ios::out | std::ios::trunc);
    if (!out_mtl) {
        throw std::runtime_error("Cannot open MTL file for writing: " + mtl_path.string());
    }

    const bool use_utm = (opts.obj_coord_frame == CoordFrame3D::UTM) && scene.has_valid_crs;

    out_obj << "# gpr3d Reconstructed 3D Subsurface Scene\n";
    if (scene.has_valid_crs) {
        out_obj << std::fixed << std::setprecision(8)
                << "# ORIGIN_WGS84 lat=" << scene.origin_wgs84.latitude_deg
                << " lon=" << scene.origin_wgs84.longitude_deg
                << std::setprecision(3) << " alt=" << scene.origin_wgs84.altitude_m << "\n"
                << "# ORIGIN_UTM zone=" << scene.origin_utm.zone_string()
                << " epsg=" << scene.origin_utm.epsg_code()
                << " easting=" << scene.origin_utm.easting_m
                << " northing=" << scene.origin_utm.northing_m << "\n"
                << "# COORD_FRAME " << (use_utm ? "UTM_METERS" : "LOCAL_ENU_METERS") << "\n";
    }
    out_obj << "mtllib " << mtl_path.filename().string() << "\n\n";
    out_mtl << "# gpr3d Material Library\n\n";

    const auto meshes = collect_meshes(scene, opts.include_ground_surface);
    std::size_t v_offset = 1;  // Wavefront OBJ indices are 1-based

    out_obj << std::fixed << std::setprecision(4);
    out_mtl << std::fixed << std::setprecision(4);

    for (std::size_t m_idx = 0; m_idx < meshes.size(); ++m_idx) {
        const auto& m = *meshes[m_idx];
        const std::string mat_name = "mat_" + m.name;

        out_mtl << "newmtl " << mat_name << "\n"
                << "Ka " << (0.2f * m.color.r) << ' ' << (0.2f * m.color.g) << ' ' << (0.2f * m.color.b) << "\n"
                << "Kd " << m.color.r << ' ' << m.color.g << ' ' << m.color.b << "\n"
                << "Ks " << m.metallic << ' ' << m.metallic << ' ' << m.metallic << "\n"
                << "Ns " << (10.0f + 90.0f * (1.0f - m.roughness)) << "\n"
                << "d " << m.color.a << "\n"
                << "illum 2\n\n";

        out_obj << "o " << m.name << "\n"
                << "usemtl " << mat_name << "\n";

        for (const auto& v : m.vertices) {
            if (use_utm) {
                const Wgs84Coord w = geo::enu_to_wgs84({v.x, v.y, v.z}, scene.origin_wgs84);
                const UtmCoord u = geo::wgs84_to_utm(w, scene.origin_utm.zone);
                out_obj << "v " << u.easting_m << ' ' << u.northing_m << ' ' << u.elevation_m << "\n";
            } else {
                out_obj << "v " << v.x << ' ' << v.y << ' ' << v.z << "\n";
            }
        }

        const bool has_normals = (m.normals.size() == m.vertices.size());
        if (has_normals) {
            for (const auto& n : m.normals) {
                out_obj << "vn " << n.x << ' ' << n.y << ' ' << n.z << "\n";
            }
        }

        for (const auto& tri : m.triangles) {
            const std::size_t i0 = v_offset + tri[0];
            const std::size_t i1 = v_offset + tri[1];
            const std::size_t i2 = v_offset + tri[2];
            if (has_normals) {
                out_obj << "f " << i0 << "//" << i0 << ' '
                        << i1 << "//" << i1 << ' '
                        << i2 << "//" << i2 << "\n";
            } else {
                out_obj << "f " << i0 << ' ' << i1 << ' ' << i2 << "\n";
            }
        }
        out_obj << "\n";
        v_offset += m.vertices.size();
    }
}

void SceneExporter3D::write_glb(
    const std::filesystem::path& glb_path,
    const scene::Scene3D& scene,
    const SceneExportOptions& opts) {
    std::ofstream out(glb_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Cannot open GLB file for writing: " + glb_path.string());
    }

    const auto meshes = collect_meshes(scene, opts.include_ground_surface);
    std::vector<std::uint8_t> bin_chunk;

    std::ostringstream nodes_json;
    std::ostringstream meshes_json;
    std::ostringstream materials_json;
    std::ostringstream accessors_json;
    std::ostringstream views_json;
    std::ostringstream root_nodes_json;

    nodes_json << std::fixed << std::setprecision(6);
    meshes_json << std::fixed << std::setprecision(6);
    materials_json << std::fixed << std::setprecision(4);
    accessors_json << std::fixed << std::setprecision(5);

    for (std::size_t m_idx = 0; m_idx < meshes.size(); ++m_idx) {
        const auto& m = *meshes[m_idx];
        const std::size_t n_verts = m.vertices.size();
        const std::size_t n_indices = m.triangles.size() * 3;

        // 1. Pack POSITION (glTF right-handed Y-up: X = East, Y = Up, Z = -North)
        const std::size_t pos_byte_off = bin_chunk.size();
        float min_x = std::numeric_limits<float>::max();
        float min_y = std::numeric_limits<float>::max();
        float min_z = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float max_y = std::numeric_limits<float>::lowest();
        float max_z = std::numeric_limits<float>::lowest();

        for (const auto& v : m.vertices) {
            const float gx = static_cast<float>(v.x);
            const float gy = static_cast<float>(v.z);
            const float gz = static_cast<float>(-v.y);
            min_x = std::min(min_x, gx); max_x = std::max(max_x, gx);
            min_y = std::min(min_y, gy); max_y = std::max(max_y, gy);
            min_z = std::min(min_z, gz); max_z = std::max(max_z, gz);
            append_f32_le(bin_chunk, gx);
            append_f32_le(bin_chunk, gy);
            append_f32_le(bin_chunk, gz);
        }
        const std::size_t pos_byte_len = bin_chunk.size() - pos_byte_off;

        // 2. Pack NORMAL
        const std::size_t nrm_byte_off = bin_chunk.size();
        for (std::size_t k = 0; k < n_verts; ++k) {
            const auto n = (k < m.normals.size()) ? m.normals[k] : scene::Vec3{0.0, 0.0, 1.0};
            append_f32_le(bin_chunk, static_cast<float>(n.x));
            append_f32_le(bin_chunk, static_cast<float>(n.z));
            append_f32_le(bin_chunk, static_cast<float>(-n.y));
        }
        const std::size_t nrm_byte_len = bin_chunk.size() - nrm_byte_off;

        // 3. Pack INDICES (uint32)
        const std::size_t idx_byte_off = bin_chunk.size();
        for (const auto& tri : m.triangles) {
            append_u32_le(bin_chunk, tri[0]);
            append_u32_le(bin_chunk, tri[1]);
            append_u32_le(bin_chunk, tri[2]);
        }
        const std::size_t idx_byte_len = bin_chunk.size() - idx_byte_off;

        const std::string comma = (m_idx > 0) ? "," : "";
        const std::size_t base_view = m_idx * 3;
        const std::size_t base_acc = m_idx * 3;

        root_nodes_json << comma << m_idx;

        nodes_json << comma << "{\"name\":\"" << m.name << "\",\"mesh\":" << m_idx << "}";

        const bool translucent = (m.color.a < 0.99f);
        const bool double_sided = (m.kind == scene::SemanticKind::StratigraphicLayer ||
                                   m.kind == scene::SemanticKind::GroundSurface);

        materials_json << comma << "{\"name\":\"mat_" << m.name << "\","
                       << "\"pbrMetallicRoughness\":{\"baseColorFactor\":["
                       << m.color.r << "," << m.color.g << "," << m.color.b << "," << m.color.a << "],"
                       << "\"metallicFactor\":" << m.metallic << ","
                       << "\"roughnessFactor\":" << m.roughness << "},"
                       << "\"alphaMode\":\"" << (translucent ? "BLEND" : "OPAQUE") << "\","
                       << "\"doubleSided\":" << (double_sided ? "true" : "false") << "}";

        meshes_json << comma << "{\"name\":\"" << m.name << "\",\"primitives\":[{"
                    << "\"attributes\":{\"POSITION\":" << base_acc
                    << ",\"NORMAL\":" << (base_acc + 1) << "},"
                    << "\"indices\":" << (base_acc + 2) << ","
                    << "\"material\":" << m_idx << "}]}";

        views_json << comma
                   << "{\"buffer\":0,\"byteOffset\":" << pos_byte_off
                   << ",\"byteLength\":" << pos_byte_len << ",\"target\":34962},"
                   << "{\"buffer\":0,\"byteOffset\":" << nrm_byte_off
                   << ",\"byteLength\":" << nrm_byte_len << ",\"target\":34962},"
                   << "{\"buffer\":0,\"byteOffset\":" << idx_byte_off
                   << ",\"byteLength\":" << idx_byte_len << ",\"target\":34963}";

        accessors_json << comma
                       << "{\"bufferView\":" << base_view
                       << ",\"componentType\":5126,\"count\":" << n_verts
                       << ",\"type\":\"VEC3\",\"min\":[" << min_x << "," << min_y << "," << min_z
                       << "],\"max\":[" << max_x << "," << max_y << "," << max_z << "]},"
                       << "{\"bufferView\":" << (base_view + 1)
                       << ",\"componentType\":5126,\"count\":" << n_verts << ",\"type\":\"VEC3\"},"
                       << "{\"bufferView\":" << (base_view + 2)
                       << ",\"componentType\":5125,\"count\":" << n_indices << ",\"type\":\"SCALAR\"}";
    }

    while (bin_chunk.size() % 4 != 0) {
        bin_chunk.push_back(0x00);
    }

    std::ostringstream doc;
    doc << std::fixed << std::setprecision(8)
        << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"gpr3d SignalHandler\"},"
        << "\"scene\":0,"
        << "\"scenes\":[{\"name\":\"Subsurface_GPR_Scene\",\"nodes\":[" << root_nodes_json.str() << "]";
    if (scene.has_valid_crs) {
        doc << ",\"extras\":{\"crs_geodetic\":\"EPSG:4326\","
            << "\"crs_projected\":\"EPSG:" << scene.origin_utm.epsg_code() << "\","
            << "\"origin_lat_deg\":" << scene.origin_wgs84.latitude_deg << ","
            << "\"origin_lon_deg\":" << scene.origin_wgs84.longitude_deg << ","
            << std::setprecision(3)
            << "\"origin_alt_m\":" << scene.origin_wgs84.altitude_m << ","
            << "\"origin_utm_easting_m\":" << scene.origin_utm.easting_m << ","
            << "\"origin_utm_northing_m\":" << scene.origin_utm.northing_m << "}";
    }
    doc << "}],"
        << "\"nodes\":[" << nodes_json.str() << "],"
        << "\"meshes\":[" << meshes_json.str() << "],"
        << "\"materials\":[" << materials_json.str() << "],"
        << "\"accessors\":[" << accessors_json.str() << "],"
        << "\"bufferViews\":[" << views_json.str() << "],"
        << "\"buffers\":[{\"byteLength\":" << bin_chunk.size() << "}]}";

    std::string json_str = doc.str();
    while (json_str.size() % 4 != 0) {
        json_str.push_back(' ');  // glTF JSON chunk pads with 0x20 spaces
    }

    const bool has_bin = !bin_chunk.empty();
    const auto total_len = static_cast<std::uint32_t>(
        12 + 8 + json_str.size() + (has_bin ? (8 + bin_chunk.size()) : 0));

    std::uint8_t header[12]{};
    detail::write_u32_le(header + 0, 0x46546C67U);  // "glTF"
    detail::write_u32_le(header + 4, 2U);           // version 2
    detail::write_u32_le(header + 8, total_len);
    out.write(reinterpret_cast<const char*>(header), 12);

    std::uint8_t json_hdr[8]{};
    detail::write_u32_le(json_hdr + 0, static_cast<std::uint32_t>(json_str.size()));
    detail::write_u32_le(json_hdr + 4, 0x4E4F534AU);  // "JSON"
    out.write(reinterpret_cast<const char*>(json_hdr), 8);
    out.write(json_str.data(), static_cast<std::streamsize>(json_str.size()));

    if (has_bin) {
        std::uint8_t bin_hdr[8]{};
        detail::write_u32_le(bin_hdr + 0, static_cast<std::uint32_t>(bin_chunk.size()));
        detail::write_u32_le(bin_hdr + 4, 0x004E4942U);  // "BIN\0"
        out.write(reinterpret_cast<const char*>(bin_hdr), 8);
        out.write(reinterpret_cast<const char*>(bin_chunk.data()),
                  static_cast<std::streamsize>(bin_chunk.size()));
    }
}

void SceneExporter3D::write_geojson(
    const std::filesystem::path& geojson_path,
    const scene::Scene3D& scene) {
    std::ofstream out(geojson_path, std::ios::out | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Cannot open GeoJSON file for writing: " + geojson_path.string());
    }

    out << "{\n  \"type\": \"FeatureCollection\",\n";
    if (scene.has_valid_crs) {
        out << std::fixed << std::setprecision(8)
            << "  \"metadata\": {\n"
            << "    \"crs_geodetic\": \"EPSG:4326\",\n"
            << "    \"crs_projected\": \"EPSG:" << scene.origin_utm.epsg_code() << "\",\n"
            << "    \"utm_zone\": \"" << scene.origin_utm.zone_string() << "\",\n"
            << "    \"origin_lat_deg\": " << scene.origin_wgs84.latitude_deg << ",\n"
            << "    \"origin_lon_deg\": " << scene.origin_wgs84.longitude_deg << ",\n"
            << std::setprecision(3)
            << "    \"origin_alt_m\": " << scene.origin_wgs84.altitude_m << "\n"
            << "  },\n";
    }
    out << "  \"features\": [\n";

    bool first_feat = true;
    auto emit_comma = [&]() {
        if (!first_feat) {
            out << ",\n";
        }
        first_feat = false;
    };

    // 1. Conduits as 3D LineStrings
    for (const auto& cond : scene.conduits) {
        if (cond.centerline_wgs84.empty()) continue;
        emit_comma();
        out << "    {\n"
            << "      \"type\": \"Feature\",\n"
            << "      \"properties\": {\n"
            << "        \"object_class\": \"conduit\",\n"
            << "        \"label\": \"" << cond.label << "\",\n"
            << "        \"polarity\": \"" << scene::to_string(cond.polarity) << "\",\n"
            << std::fixed << std::setprecision(4)
            << "        \"mean_depth_m\": " << cond.mean_depth_m << ",\n"
            << "        \"estimated_radius_m\": " << cond.estimated_radius_m << ",\n"
            << "        \"estimated_diameter_mm\": " << (cond.estimated_radius_m * 2000.0) << ",\n"
            << "        \"peak_intensity\": " << cond.peak_intensity << ",\n"
            << "        \"confidence\": " << cond.confidence << "\n"
            << "      },\n"
            << "      \"geometry\": {\n"
            << "        \"type\": \"LineString\",\n"
            << "        \"coordinates\": [";
        for (std::size_t k = 0; k < cond.centerline_wgs84.size(); ++k) {
            const auto& w = cond.centerline_wgs84[k];
            if (k > 0) out << ", ";
            out << "[" << std::setprecision(8) << w.longitude_deg << ", "
                << w.latitude_deg << ", "
                << std::setprecision(3) << w.altitude_m << "]";
        }
        out << "]\n      }\n    }";
    }

    // 2. Stratigraphic Horizons as 3D LineStrings
    for (const auto& hz : scene.horizons) {
        if (hz.profile_wgs84.empty()) continue;
        emit_comma();
        out << "    {\n"
            << "      \"type\": \"Feature\",\n"
            << "      \"properties\": {\n"
            << "        \"object_class\": \"stratigraphic_horizon\",\n"
            << "        \"label\": \"" << hz.label << "\",\n"
            << std::fixed << std::setprecision(4)
            << "        \"mean_depth_m\": " << hz.mean_depth_m << ",\n"
            << "        \"dip_slope_m_per_m\": " << hz.dip_slope_m_per_m << ",\n"
            << "        \"mean_intensity\": " << hz.mean_intensity << "\n"
            << "      },\n"
            << "      \"geometry\": {\n"
            << "        \"type\": \"LineString\",\n"
            << "        \"coordinates\": [";
        for (std::size_t k = 0; k < hz.profile_wgs84.size(); ++k) {
            const auto& w = hz.profile_wgs84[k];
            if (k > 0) out << ", ";
            out << "[" << std::setprecision(8) << w.longitude_deg << ", "
                << w.latitude_deg << ", "
                << std::setprecision(3) << w.altitude_m << "]";
        }
        out << "]\n      }\n    }";
    }

    // 3. Localized Anomalies as 3D Points with bounding ellipsoid metadata
    for (const auto& an : scene.anomalies) {
        emit_comma();
        out << "    {\n"
            << "      \"type\": \"Feature\",\n"
            << "      \"properties\": {\n"
            << "        \"object_class\": \"subsurface_anomaly\",\n"
            << "        \"label\": \"" << an.label << "\",\n"
            << "        \"polarity\": \"" << scene::to_string(an.polarity) << "\",\n"
            << std::fixed << std::setprecision(4)
            << "        \"depth_m\": " << an.depth_m << ",\n"
            << "        \"radius_x_m\": " << an.radii_m.x << ",\n"
            << "        \"radius_y_m\": " << an.radii_m.y << ",\n"
            << "        \"radius_z_m\": " << an.radii_m.z << ",\n"
            << "        \"peak_intensity\": " << an.peak_intensity << ",\n"
            << "        \"utm_easting_m\": " << an.center_utm.easting_m << ",\n"
            << "        \"utm_northing_m\": " << an.center_utm.northing_m << "\n"
            << "      },\n"
            << "      \"geometry\": {\n"
            << "        \"type\": \"Point\",\n"
            << "        \"coordinates\": ["
            << std::setprecision(8) << an.center_wgs84.longitude_deg << ", "
            << an.center_wgs84.latitude_deg << ", "
            << std::setprecision(3) << an.center_wgs84.altitude_m << "]\n"
            << "      }\n    }";
    }

    out << "\n  ]\n}\n";
}

void SceneExporter3D::write_auto(
    const std::filesystem::path& path,
    const scene::Scene3D& scene,
    const SceneExportOptions& opts) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (ext == ".obj") {
        write_obj(path, scene, opts);
    } else if (ext == ".glb") {
        write_glb(path, scene, opts);
    } else if (ext == ".geojson" || ext == ".json") {
        write_geojson(path, scene);
    } else {
        throw std::runtime_error(
            "Unsupported 3D scene extension '" + ext + "' (expected .obj, .glb, or .geojson)");
    }
}

}  // namespace gpr3d::io

