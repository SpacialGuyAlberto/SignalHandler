#!/usr/bin/env python3
import os
import subprocess
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

OUTPUT_DIR = "/home/luis/Documents/Projects/Glasfaser/SignalHandler/docs/diagrams"
os.makedirs(OUTPUT_DIR, exist_ok=True)

# -----------------------------------------------------------------------------
# 1. ROADMAP DE SPRINTS (Graphviz)
# -----------------------------------------------------------------------------
dot_roadmap = """digraph Roadmap {
    rankdir=TB;
    compound=true;
    fontname="Helvetica";
    nodesep=0.4;
    ranksep=0.4;

    node [fontname="Helvetica", fontsize=10, shape=box, style="rounded,filled", penwidth=1.5];
    edge [color="#1A73E8", penwidth=1.5, arrowhead="vee"];

    subgraph cluster_fase1 {
        label = "FASE I: Fundamentos & Adquisición";
        fontname="Helvetica-Bold"; fontsize=11; color="#B0BEC5"; style="dashed,rounded"; fillcolor="#F8F9FA";
        S1 [label="Sprint 1: Núcleo, Memoria & Endianness\\n• Flotante IEEE 754 vs IBM 360\\n• Bitwise & std::endian\\n• Arens Kap. 1, 28 | Breymann Kap. 2, 3", fillcolor="#E8F0FE", color="#1A73E8"];
        S2 [label="Sprint 2: Geodesia WGS84, ECEF, ENU & UTM\\n• Matrices ortogonales & Lever-Arm\\n• Arens Kap. 7, 8 | Tipler Kap. 3 | Breymann Kap. 7", fillcolor="#E8F0FE", color="#1A73E8"];
        S3 [label="Sprint 3: Ingesta I/O GSSI RADAN & SEG-Y\\n• Parsing binario DZT 8/16/32-bit\\n• Clases AScan y BScan (RAII)\\n• Tipler Kap. 15 | Breymann Kap. 16", fillcolor="#E8F0FE", color="#1A73E8"];
        S1 -> S2 -> S3;
    }

    subgraph cluster_fase2 {
        label = "FASE II: Procesamiento de Señales (DSP)";
        fontname="Helvetica-Bold"; fontsize=11; color="#B0BEC5"; style="dashed,rounded"; fillcolor="#F8F9FA";
        S4 [label="Sprint 4: Acondicionamiento DSP & Filtros\\n• Dewow sustractivo & Time-Zero\\n• Butterworth pasabanda fase cero (FFT)\\n• Arens Kap. 18 | Tipler Kap. 28 | Breymann Kap. 11, 18", fillcolor="#E6F4EA", color="#137333"];
        S5 [label="Sprint 5: Transformada de Hilbert & Envolvente\\n• Señal analítica compleja z(t)\\n• Amplitud instantánea A(t) & Fase\\n• Arens Kap. 18 | Tipler Kap. 15 | Breymann Kap. 14", fillcolor="#E6F4EA", color="#137333"];
        S3 -> S4 -> S5;
    }

    subgraph cluster_fase3 {
        label = "FASE III: Inversión Geofísica & Geometría 3D";
        fontname="Helvetica-Bold"; fontsize=11; color="#B0BEC5"; style="dashed,rounded"; fillcolor="#F8F9FA";
        S6 [label="Sprint 6: Inversión de Suelo (Semblanza & Fresnel)\\n• Ajuste hiperbólico de velocidad v(x,t)\\n• Inversión multicapa de Fresnel R\\n• Base de datos litológica ASTM D6432\\n• Arens Kap. 28 | Tipler Kap. 30-32 | Breymann Kap. 13", fillcolor="#FEF7E0", color="#B06000"];
        S7 [label="Sprint 7: Migración Kirchhoff 2D & Mallas 3D\\n• Suma de Kirchhoff en apertura\\n• Triedro de Frenet-Serret para tubos 3D\\n• Arens Kap. 24, 25 | Tipler Kap. 15 | Breymann Kap. 7, 8", fillcolor="#FEF7E0", color="#B06000"];
        S5 -> S6 -> S7;
    }

    subgraph cluster_fase4 {
        label = "FASE IV: Visualización & Entrega";
        fontname="Helvetica-Bold"; fontsize=11; color="#B0BEC5"; style="dashed,rounded"; fillcolor="#F8F9FA";
        S8 [label="Sprint 8: glTF 2.0 Binary, GeoJSON & WebGL\\n• Serialización de buffers binarios .glb\\n• Geometrías GIS 3D georreferenciadas\\n• Dashboard Web interactivo Three.js\\n• Arens Kap. 7 | Tipler Kap. 31 | Breymann Kap. 16", fillcolor="#FCE8E6", color="#C5221F"];
        S7 -> S8;
    }
}
"""

with open(f"{OUTPUT_DIR}/roadmap_sprints.dot", "w") as f:
    f.write(dot_roadmap)
subprocess.run(["dot", "-Tpdf", f"{OUTPUT_DIR}/roadmap_sprints.dot", "-o", f"{OUTPUT_DIR}/roadmap_sprints.pdf"], check=True)

# -----------------------------------------------------------------------------
# 2. ARQUITECTURA DEL MOTOR GPR3D (Graphviz)
# -----------------------------------------------------------------------------
dot_arch = """digraph Arch {
    rankdir=LR;
    fontname="Helvetica";
    node [fontname="Helvetica", fontsize=9, shape=box, style="rounded,filled", penwidth=1.2];
    edge [color="#455A64", penwidth=1.2, arrowhead="vee"];

    subgraph cluster_input {
        label = "Archivos Crudos"; fontname="Helvetica-Bold"; fontsize=10; fillcolor="#ECEFF1"; color="#90A4AE"; style="rounded,filled";
        F1 [label="GSSI .DZT / .DZG", fillcolor="#FFFFFF", color="#607D8B"];
        F2 [label="SEG-Y / SGY", fillcolor="#FFFFFF", color="#607D8B"];
        F3 [label="MALA .RD3 / .RAD", fillcolor="#FFFFFF", color="#607D8B"];
    }

    subgraph cluster_io {
        label = "Capa I/O & Core"; fontname="Helvetica-Bold"; fontsize=10; fillcolor="#E8F0FE"; color="#1A73E8"; style="rounded,filled";
        IO [label="DztReader / SegyReader\\n(Zero-copy endianness)", fillcolor="#FFFFFF", color="#1A73E8"];
        CORE [label="Estructuras Maestras\\n• AScan (Muestras + GPS)\\n• BScan (Perfil 2D)", fillcolor="#FFFFFF", color="#1A73E8"];
        GEO [label="CrsTransformer\\nWGS84 -> ECEF -> ENU / UTM", fillcolor="#FFFFFF", color="#1A73E8"];
        IO -> CORE;
        GEO -> CORE;
    }

    subgraph cluster_dsp {
        label = "Capa DSP & Inversión"; fontname="Helvetica-Bold"; fontsize=10; fillcolor="#E6F4EA"; color="#137333"; style="rounded,filled";
        DSP [label="SignalProcessor\\n• Dewow\\n• Time-Zero\\n• Butterworth (FFTW3)", fillcolor="#FFFFFF", color="#137333"];
        HILB [label="HilbertTransformer\\n• Señal Analítica z(t)\\n• Envolvente |z(t)|", fillcolor="#FFFFFF", color="#137333"];
        INV [label="SoilInverter\\n• Semblanza v(x,t)\\n• Inversión Fresnel R\\n• MaterialDB (ASTM)", fillcolor="#FFFFFF", color="#137333"];
        MIG [label="ObjectTransformer\\n• Migración Kirchhoff\\n• Frenet-Serret 3D", fillcolor="#FFFFFF", color="#137333"];
        DSP -> HILB -> INV -> MIG;
    }

    subgraph cluster_export {
        label = "Capa Exportación"; fontname="Helvetica-Bold"; fontsize=10; fillcolor="#FEF7E0"; color="#B06000"; style="rounded,filled";
        GLB [label="SceneExporter3D\\nKhronos glTF 2.0 .glb", fillcolor="#FFFFFF", color="#B06000"];
        GIS [label="GeoJsonWriter\\nCapa vectorial GIS 3D", fillcolor="#FFFFFF", color="#B06000"];
        VIZ [label="FluctuationPlotter & Web\\nPNG Cairo + Three.js HTML", fillcolor="#FFFFFF", color="#B06000"];
    }

    F1 -> IO; F2 -> IO; F3 -> IO;
    CORE -> DSP;
    MIG -> GLB; MIG -> GIS; MIG -> VIZ;
}
"""

with open(f"{OUTPUT_DIR}/pipeline_arquitectura.dot", "w") as f:
    f.write(dot_arch)
subprocess.run(["dot", "-Tpdf", f"{OUTPUT_DIR}/pipeline_arquitectura.dot", "-o", f"{OUTPUT_DIR}/pipeline_arquitectura.pdf"], check=True)

# -----------------------------------------------------------------------------
# 3. PIPELINE GEODÉSICO (Graphviz)
# -----------------------------------------------------------------------------
dot_geo = """digraph Geo {
    rankdir=LR;
    fontname="Helvetica";
    node [fontname="Helvetica", fontsize=9, shape=box, style="rounded,filled", penwidth=1.2];
    edge [color="#1A73E8", penwidth=1.2, arrowhead="vee"];

    WGS [label="WGS84 Geodésico\\n(φ: Lat, λ: Lon, h: Alt)\\nElipsoide a=6378137m", fillcolor="#E8F0FE", color="#1A73E8"];
    ECEF [label="ECEF Cartesiano\\n(X, Y, Z)\\nGeocéntrico Global", fillcolor="#E8F0FE", color="#1A73E8"];
    ENU [label="ENU Local Tangente\\n(East, North, Up)\\nOrigen local (φ₀, λ₀, h₀)", fillcolor="#E6F4EA", color="#137333"];
    UTM [label="Proyección UTM\\n(Zona, Banda, E_utm, N_utm)\\nTransversa de Mercator", fillcolor="#E6F4EA", color="#137333"];
    ANT [label="Corrección Lever-Arm\\nAntena GPR corregida\\ncon rumbo (heading) ψ", fillcolor="#FEF7E0", color="#B06000"];

    WGS -> ECEF [label=" N(φ), e²"];
    ECEF -> ENU [label=" Matriz R(φ₀, λ₀)"];
    WGS -> UTM [label=" Reducción Gauss-Krüger"];
    ENU -> ANT [label=" Δx sinψ + Δy cosψ"];
    UTM -> ANT [label=" Offset local"];
}
"""

with open(f"{OUTPUT_DIR}/geodesia_pipeline.dot", "w") as f:
    f.write(dot_geo)
subprocess.run(["dot", "-Tpdf", f"{OUTPUT_DIR}/geodesia_pipeline.dot", "-o", f"{OUTPUT_DIR}/geodesia_pipeline.pdf"], check=True)

# -----------------------------------------------------------------------------
# 4. HIPÉRBOLA DE DIFRACCIÓN & GEOMETRÍA (Matplotlib)
# -----------------------------------------------------------------------------
fig, ax = plt.subplots(figsize=(6.5, 3.2), dpi=300)
x0, z0, v = 5.0, 1.0, 0.10 # 5 m, 1 m de prof, 0.10 m/ns (epsr = 9)
x = np.linspace(2.0, 8.0, 300)
twt = 2.0 / v * np.sqrt((x - x0)**2 + z0**2) # en ns
t0 = 2.0 * z0 / v

ax.plot(x, twt, color="#1A73E8", linewidth=2.5, label=r"$t(x) = \sqrt{t_0^2 + \frac{4(x-x_0)^2}{v^2}}$")
ax.scatter([x0], [t0], color="#D93025", s=70, zorder=5, label=r"Ápice $(x_0, t_0)$")
ax.axvline(x0, color="#80868B", linestyle="--", alpha=0.7)
ax.axhline(t0, color="#80868B", linestyle="--", alpha=0.7)

ax.annotate(r"Ápice $(x_0, t_0)$" + f"\n$t_0 = {t0:.1f}$ ns, $z_0 = {z0:.1f}$ m",
            xy=(x0, t0), xytext=(x0 + 0.4, t0 - 2.5),
            arrowprops=dict(facecolor="#D93025", shrink=0.08, width=1, headwidth=6),
            fontsize=9, fontweight="bold", color="#B31412")

ax.set_title("Geometría de la Hipérbola de Difracción GPR", fontsize=11, fontweight="bold", pad=8)
ax.set_xlabel("Posición a lo largo del perfil $x$ [m]", fontsize=10)
ax.set_ylabel("Tiempo de Viaje TWT $t$ [ns]", fontsize=10)
ax.invert_yaxis() # Convención geofísica: tiempo hacia abajo
ax.grid(True, linestyle=":", alpha=0.6)
ax.legend(loc="upper right", fontsize=9, framealpha=0.9)
plt.tight_layout()
plt.savefig(f"{OUTPUT_DIR}/hiperbola_geometria.pdf")
plt.close()

# -----------------------------------------------------------------------------
# 5. ESTRATIGRAFÍA Y REFLEXIÓN MULTICAPA DE FRESNEL (Matplotlib)
# -----------------------------------------------------------------------------
fig, ax = plt.subplots(figsize=(6.5, 3.4), dpi=300)

layers = [
    {"name": "Aire (Superficie)", "epsr": 1.0, "z0": 0.0, "z1": 0.3, "color": "#E3F2FD"},
    {"name": "Capa 1: Asfalto de Rodadura", "epsr": 4.0, "z0": 0.3, "z1": 0.8, "color": "#78909C"},
    {"name": "Capa 2: Base Granular / Grava", "epsr": 6.5, "z0": 0.8, "z1": 1.5, "color": "#FFE082"},
    {"name": "Capa 3: Subrasante Arcillosa / Húmeda", "epsr": 16.0, "z0": 1.5, "z1": 2.2, "color": "#BCAAA4"},
]

for lay in layers:
    ax.fill_between([0, 10], lay["z0"], lay["z1"], color=lay["color"], alpha=0.85)
    z_mid = (lay["z0"] + lay["z1"]) / 2.0
    ax.text(0.5, z_mid, f"{lay['name']}\n$\\varepsilon_r = {lay['epsr']:.1f}$",
            va="center", ha="left", fontsize=9, fontweight="bold", color="#212121")

# Flechas de reflexión en interfases
interfaces = [
    {"z": 0.3, "r_label": r"$R_0 = \frac{1 - \sqrt{4}}{1 + \sqrt{4}} = -0.333$", "eps_next": 4.0},
    {"z": 0.8, "r_label": r"$R_1 = \frac{\sqrt{4} - \sqrt{6.5}}{\sqrt{4} + \sqrt{6.5}} \approx -0.121$", "eps_next": 6.5},
    {"z": 1.5, "r_label": r"$R_2 = \frac{\sqrt{6.5} - \sqrt{16}}{\sqrt{6.5} + \sqrt{16}} \approx -0.222$", "eps_next": 16.0},
]

for inter in interfaces:
    ax.axhline(inter["z"], color="#D93025", linewidth=1.5, linestyle="-")
    ax.text(6.0, inter["z"] - 0.04, inter["r_label"], fontsize=8.5, color="#B31412",
            bbox=dict(boxstyle="round,pad=0.2", facecolor="#FFFFFF", edgecolor="#D93025", alpha=0.9))

ax.set_ylim(2.2, 0.0) # Profundidad hacia abajo
ax.set_xlim(0, 10)
ax.set_title(r"Inversión Multicapa de Fresnel: $\varepsilon_{r,k+1} = \varepsilon_{r,k} \left(\frac{1 - R_k}{1 + R_k}\right)^2$",
             fontsize=11, fontweight="bold", pad=8)
ax.set_xlabel("Distancia transversal [m]", fontsize=10)
ax.set_ylabel("Profundidad estimada $z$ [m]", fontsize=10)
ax.set_xticks([])
plt.tight_layout()
plt.savefig(f"{OUTPUT_DIR}/fresnel_estratigrafia.pdf")
plt.close()

# -----------------------------------------------------------------------------
# 6. TRIEDRO DE FRENET-SERRET Y MALLAS TUBULARES 3D (Matplotlib)
# -----------------------------------------------------------------------------
fig = plt.figure(figsize=(6.5, 3.4), dpi=300)
ax = fig.add_subplot(111, projection="3d")

# Curva 3D
s = np.linspace(0, 2 * np.pi, 100)
xc = s
yc = np.sin(s) * 0.5
zc = -0.8 + np.cos(s) * 0.1
ax.plot(xc, yc, zc, color="#1A73E8", linewidth=2.5, label=r"Poligonal central $\mathbf{c}(s)$")

# Triedro en s = pi
s_idx = 50
p = np.array([xc[s_idx], yc[s_idx], zc[s_idx]])
T = np.array([1.0, 0.5 * np.cos(np.pi), -0.1 * np.sin(np.pi)])
T = T / np.linalg.norm(T)
N = np.array([0.0, -0.5 * np.sin(np.pi), -0.1 * np.cos(np.pi)])
if np.linalg.norm(N) > 1e-6:
    N = N / np.linalg.norm(N)
else:
    N = np.array([0.0, 1.0, 0.0])
B = np.cross(T, N)
B = B / np.linalg.norm(B)

ax.quiver(p[0], p[1], p[2], T[0]*0.8, T[1]*0.8, T[2]*0.8, color="#D93025", linewidth=2, label=r"Tangente $\mathbf{T}$")
ax.quiver(p[0], p[1], p[2], N[0]*0.5, N[1]*0.5, N[2]*0.5, color="#1E8E3E", linewidth=2, label=r"Normal $\mathbf{N}$")
ax.quiver(p[0], p[1], p[2], B[0]*0.5, B[1]*0.5, B[2]*0.5, color="#F9AB00", linewidth=2, label=r"Binormal $\mathbf{B}$")

# Anillo circular en p
theta = np.linspace(0, 2 * np.pi, 17)
radius = 0.2
ring = np.array([p + radius * (np.cos(th) * N + np.sin(th) * B) for th in theta])
ax.plot(ring[:, 0], ring[:, 1], ring[:, 2], color="#202124", linewidth=1.5, linestyle="--", label="Anillo $M=16$")

ax.set_title("Triedro de Frenet-Serret para Extrusión Tubular 3D", fontsize=10, fontweight="bold")
ax.set_xlabel("X (Este)", fontsize=8)
ax.set_ylabel("Y (Norte)", fontsize=8)
ax.set_zlabel("Z (Prof)", fontsize=8)
ax.legend(loc="upper right", fontsize=7.5)
plt.tight_layout()
plt.savefig(f"{OUTPUT_DIR}/frenet_serret_tubo.pdf")
plt.close()

print("All 6 vector PDF diagrams generated successfully in:", OUTPUT_DIR)

