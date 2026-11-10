#!/usr/bin/env python3
"""Diagramas adicionales para la guia de replicacion v2 (13 sprints).

Genera en docs/diagrams/:
  - roadmap_v2.pdf            : grafo de dependencias de los 13 sprints
  - dsp_orden_pipeline.pdf    : orden obligatorio del pipeline DSP y su motivo
  - porque_zero_padding.pdf   : convolucion circular vs lineal (artefacto real)
  - porque_dsp_traza.pdf      : efecto de dewow / time-zero / bandpass / SEC en una traza
"""
import os
import subprocess

os.environ.setdefault("MPLCONFIGDIR", "/tmp/mpl-gpr3d")
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "diagrams")
os.makedirs(OUT, exist_ok=True)


def render_dot(name: str, src: str) -> None:
    dot_path = os.path.join(OUT, f"{name}.dot")
    with open(dot_path, "w", encoding="utf-8") as f:
        f.write(src)
    subprocess.run(["dot", "-Tpdf", dot_path, "-o", os.path.join(OUT, f"{name}.pdf")], check=True)


# -----------------------------------------------------------------------------
# 1. Grafo de dependencias de los 13 sprints
# -----------------------------------------------------------------------------
render_dot("roadmap_v2", r"""digraph R {
  rankdir=TB; nodesep=0.25; ranksep=0.35;
  node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=9, penwidth=1.3];
  edge [color="#5F6368", arrowhead=vee, penwidth=1.1];

  S0  [label="S0  Infraestructura\nCMake, tests, CLI vacia", fillcolor="#ECEFF1", color="#607D8B"];
  S1  [label="S1  Nucleo binario\nendian.hpp, IBM float", fillcolor="#E8F0FE", color="#1A73E8"];
  S2  [label="S2  Tipos de dominio + simulador\ntypes.hpp, SyntheticGenerator", fillcolor="#E8F0FE", color="#1A73E8"];
  S3  [label="S3  Geodesia\nECEF, ENU, UTM, azimut, lever-arm", fillcolor="#E8F0FE", color="#1A73E8"];
  S4  [label="S4  GNSS\nNMEA GGA, sincronizacion de trayectoria", fillcolor="#E8F0FE", color="#1A73E8"];
  S5  [label="S5  Lectores de radar\nRadarReader, DZT, SEG-Y, dispatch", fillcolor="#E8F0FE", color="#1A73E8"];
  S6  [label="S6  FFT + Hilbert\nCooley-Tukey, senal analitica", fillcolor="#E6F4EA", color="#137333"];
  S7  [label="S7  Pipeline DSP\ndewow, t0, bandpass, fondo, SEC", fillcolor="#E6F4EA", color="#137333"];
  S8  [label="S8  Migracion + profundidad\nKirchhoff, TWT->z, nube de puntos", fillcolor="#E6F4EA", color="#137333"];
  S9  [label="S9  Inversion geofisica\nFresnel, semblanza, atenuacion, ASTM", fillcolor="#FEF7E0", color="#B06000"];
  S10 [label="S10 Objetos semanticos + mallas\nhorizontes, conductos, anomalias", fillcolor="#FEF7E0", color="#B06000"];
  S11 [label="S11 Exportadores\nOBJ/MTL, GLB, GeoJSON, PLY, HTML", fillcolor="#FCE8E6", color="#C5221F"];
  S12 [label="S12 Integracion\nCLI completa, graficos, datos reales", fillcolor="#FCE8E6", color="#C5221F"];

  S0 -> S1 -> S2;
  S2 -> S3 -> S4;
  S1 -> S5; S2 -> S5; S4 -> S5;
  S2 -> S6 -> S7;
  S5 -> S7;
  S7 -> S8; S3 -> S8;
  S6 -> S9; S7 -> S9; S8 -> S9;
  S8 -> S10; S9 -> S10;
  S10 -> S11; S3 -> S11;
  S11 -> S12; S5 -> S12;
}
""")

# -----------------------------------------------------------------------------
# 2. Orden del pipeline DSP con el motivo de cada posicion
# -----------------------------------------------------------------------------
render_dot("dsp_orden_pipeline", r"""digraph D {
  rankdir=LR; nodesep=0.2;
  node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=8.5, fillcolor="#FFFFFF", color="#137333", penwidth=1.2];
  edge [color="#137333", arrowhead=vee];

  RAW [label="Traza cruda\n(offset DC, wow)", fillcolor="#ECEFF1", color="#607D8B"];
  D1 [label="1. Dewow\nantes que t0: el DC\nsesga el umbral de\nprimer arribo"];
  D2 [label="2. Time-zero\nantes que filtros:\nfija z=0 real"];
  D3 [label="3. Bandpass\nquita ruido fuera\nde banda de antena"];
  D4 [label="4. Fondo\nquita onda directa\nhorizontal (ringing)"];
  D5 [label="5. Ganancia SEC\nal final: amplifica\nlo ya limpio"];
  D6 [label="6. Migracion\n(rama objetivos)"];
  D7 [label="7. Envolvente\nHilbert"];

  RAW -> D1 -> D2 -> D3 -> D4 -> D5 -> D6 -> D7;

  INV [label="Rama inversion:\ncopia SIN ganancia\n(amplitudes fisicas)", fillcolor="#FEF7E0", color="#B06000"];
  D3 -> INV [style=dashed, color="#B06000"];
}
""")

# -----------------------------------------------------------------------------
# 3. Por que hace falta zero-padding: convolucion circular vs lineal
# -----------------------------------------------------------------------------
n = 256
dt = 0.1  # ns
t = np.arange(n) * dt
f0 = 0.4  # GHz -> 400 MHz
# Onda directa muy fuerte al inicio + reflector debil a 15 ns
def ricker(tc, f, amp):
    a = (np.pi * f * (t - tc)) ** 2
    return amp * (1 - 2 * a) * np.exp(-a)

trace = ricker(2.0, f0, 10.0) + ricker(15.0, f0, 0.6)

def lowpass_gain(nfft, fc_ghz=0.25, order=4):
    freqs = np.fft.fftfreq(nfft, d=dt)
    return 1.0 / np.sqrt(1.0 + (np.abs(freqs) / fc_ghz) ** (2 * order))

# Filtro largo (paso bajo estrecho => respuesta impulsiva larga) para exagerar el efecto
g_circ = lowpass_gain(n, 0.12, 8)
circ = np.real(np.fft.ifft(np.fft.fft(trace) * g_circ))
nfft = 2 * n
g_lin = lowpass_gain(nfft, 0.12, 8)
padded = np.zeros(nfft); padded[:n] = trace
lin = np.real(np.fft.ifft(np.fft.fft(padded) * g_lin))[:n]

fig, axes = plt.subplots(1, 2, figsize=(7.2, 2.7), dpi=200, sharey=True)
axes[0].plot(t, circ, color="#C5221F", lw=1.2)
axes[0].set_title("Sin zero-padding (convolucion circular)", fontsize=9)
axes[0].axvspan(22, 25.6, color="#C5221F", alpha=0.12)
axes[0].annotate("energia de la onda directa\n'envuelta' al final de la traza", xy=(24, circ[-15]),
                 xytext=(8, 4.5), fontsize=7, arrowprops=dict(arrowstyle="->", color="#C5221F"))
axes[1].plot(t, lin, color="#137333", lw=1.2)
axes[1].set_title("Con zero-padding a 2N (convolucion lineal)", fontsize=9)
for ax in axes:
    ax.set_xlabel("t [ns]", fontsize=8)
    ax.grid(ls=":", alpha=0.5)
    ax.tick_params(labelsize=7)
axes[0].set_ylabel("amplitud", fontsize=8)
plt.tight_layout()
plt.savefig(os.path.join(OUT, "porque_zero_padding.pdf"))
plt.close()

# -----------------------------------------------------------------------------
# 4. Efecto de cada etapa DSP sobre una traza sintetica
# -----------------------------------------------------------------------------
rng = np.random.default_rng(7)
n = 512
dt = 0.1
t = np.arange(n) * dt
delay = 3.0  # ns de cable/aire antes del suelo
wow = 1500.0 * np.exp(-t / 18.0) + 400.0  # deriva lenta + DC
alpha = 0.09  # Np/ns atenuacion
sig = (ricker(delay + 1.0, 0.5, 900.0)
       + ricker(delay + 12.0, 0.5, 220.0) * np.exp(-alpha * 12.0) * 6
       + ricker(delay + 30.0, 0.5, 220.0) * np.exp(-alpha * 30.0) * 6)
hf_noise = 25.0 * np.sin(2 * np.pi * 3.5 * t)  # ruido 3.5 GHz fuera de banda
raw = sig + wow + hf_noise + rng.normal(0, 4.0, n)

# dewow (media movil 4 ns)
w = int(round(2.0 / dt))
kern = np.ones(2 * w + 1) / (2 * w + 1)
mean_loc = np.convolve(np.pad(raw, w, mode="edge"), kern, mode="valid")
dew = raw - mean_loc
# time-zero por umbral 15 %
j0 = int(np.argmax(np.abs(dew) >= 0.15 * np.max(np.abs(dew))))
tz = np.zeros_like(dew); tz[: n - j0] = dew[j0:]
# bandpass 100-1250 MHz
nfft = 2 * n
fr = np.abs(np.fft.fftfreq(nfft, d=dt))
fr[0] = 1e-9
g = (1 / np.sqrt(1 + (0.1 / fr) ** 8)) * (1 / np.sqrt(1 + (fr / 1.25) ** 8))
pad = np.zeros(nfft); pad[:n] = tz
bp = np.real(np.fft.ifft(np.fft.fft(pad) * g))[:n]
# SEC
gain = np.minimum((1 + 0.08 * t) * np.exp(0.04 * t), 60)
sec = bp * gain

fig, axes = plt.subplots(4, 1, figsize=(7.2, 6.4), dpi=200, sharex=True)
panels = [
    (raw, "1) Cruda: offset DC + deriva 'wow' + ruido HF", "#607D8B"),
    (tz, "2) Tras dewow + time-zero: base en 0 y suelo en t=0", "#1A73E8"),
    (bp, "3) Tras Butterworth 100-1250 MHz: ruido 3.5 GHz eliminado", "#137333"),
    (sec, "4) Tras ganancia SEC: reflectores profundos visibles", "#B06000"),
]
for ax, (y, title, c) in zip(axes, panels):
    ax.plot(t, y, color=c, lw=0.9)
    ax.set_title(title, fontsize=8.5, loc="left")
    ax.grid(ls=":", alpha=0.5)
    ax.tick_params(labelsize=7)
axes[-1].set_xlabel("tiempo doble de viaje t [ns]", fontsize=8)
plt.tight_layout()
plt.savefig(os.path.join(OUT, "porque_dsp_traza.pdf"))
plt.close()

print("Diagramas v2 generados en", OUT)

