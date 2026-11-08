# gpr3d (`SignalHandler`)

Librería modular en **C++20** y herramienta CLI (`gpr3d`) para **leer, acondicionar, interpretar y transformar señales de Georradar (GPR / GDR)** tanto a **nubes de puntos 3D georreferenciadas** como a **objetos semánticos 3D mallados** y **visualizaciones de fluctuaciones gráficas**.

---

## Características principales

1. **Lectura y escritura nativa de formatos estándar GPR y GNSS**:
   - **SEG-Y (`.sgy` / `.segy`)**, **GSSI RADAN (`.dzt` + `.dzg`)** y **MALA RAMAC (`.rd3` + `.rad` + `.cor`)**.
   - Sincronización e interpolación GNSS desde `.csv`, `.cor` y NMEA-0183 (`$GPGGA` / `$GNGGA`) con corrección de brazo de palanca (*lever-arm*).

2. **Procesamiento Digital de Señales (DSP) e Interpretación Semántica de Objetos 3D**:
   - Pipeline geofísico de doble rama: *Dewow*, corrección *Time-Zero*, pasabanda Butterworth de fase cero (`FFTW3`), sustracción de fondo, ganancia SEC, **migración 2D de Kirchhoff** y envolvente analítica de Hilbert.
   - **Transformador de Señales a Objetos 3D (`SignalObjectTransformer`)**:
     - **Conductos y tuberías cilíndricas (`ConduitObject3D`)**: detección de ápices migrados con clasificación automática de polaridad electromagnética (*ducto dieléctrico/fibra óptica* vs *tubería metálica/agua*), enlace 3D entre líneas paralelas y generación de mallas tubulares cilíndricas.
     - **Estratos / Horizontes estratigráficos (`LayerHorizonObject3D`)**: seguimiento robusto por filtro de mediana lateral y triangulación 3D de interfaces de pavimento/subsuelo.
     - **Anomalías subterráneas localizadas (`AnomalyObject3D`)**: volúmenes elipsoidales 3D georreferenciados.
   - Exportación de escenas 3D a **Wavefront `.obj` + `.mtl`**, **Khronos glTF 2.0 binario (`.glb`)** y **GIS `.geojson` 3D**, además de nubes `.ply`, `.csv` y `.xyz`.

3. **Motor Geofísico de Inversión y Clasificación de Suelos y Materiales (`SoilInverter` & `MaterialDatabase`)**:
   - **Análisis de velocidad por hipérbolas de difracción (*Neidell-Taner Semblance*)**: deducción automática de velocidad de onda real $v$ ($\text{m/ns}$) y permitividad relativa $\varepsilon_r$ independiente de la cabecera.
   - **Inversión multicapa de reflectancias de Fresnel**: estimación estratigráfica de contrastes dieléctricos ($\varepsilon_{r,1}, \varepsilon_{r,2}, \dots$) a través de cada horizonte detectado.
   - **Atenuación dependiente de la frecuencia y pérdidas**: cálculo de $\alpha$ ($\text{dB/m}$), $\tan\delta$ y conductividad aparente $\sigma$ ($\text{mS/m}$) combinando decaimiento de envolvente y corrimiento espectral de centroide (*Quan-Harris*).
   - **Clasificador bayesiano ASTM D6432 / GPR**: catalogación bilingüe (ES/EN) de pavimentos, suelos y servicios enterrados (*Asfalto seco*, *Hormigón seco/húmedo*, *Grava compactada*, *Arena seca/saturada*, *Limo*, *Arcilla húmeda*, *Roca consolidada*, *Agua dulce*, *Ducto polimérico PVC/PEAD*, *Tubería metálica*, *Cavidad con aire*) con colores PBR geológicos automáticos en mallas 3D y columna litológica en `.png`/`.svg`/`.html`.

4. **Fluctuaciones Gráficas y Visor Interactivo**:
   - **Gráficos estáticos de alta resolución (`.png` y `.svg` vectorial con Cairo)**: figura de 4 paneles con radargrama B-scan + superposición de objetos 3D, trazas *Wiggle* geofísicas, forma de onda A-scan $s(t)$ + envolvente $\pm|z(t)|$, densidad espectral FFT (dB) y energía RMS por traza.
   - **Dashboard interactivo autónomo `.html`**: escena 3D orbital en tiempo real sincronizada con sonda virtual sobre el B-scan, curva A-scan y espectro FFT sin dependencias externas.
   - **Visor nativo de escritorio C++ OpenGL + GLFW (`gpr3d view`)**: ventana 3D interactiva a 60 FPS con sonda láser 3D y HUD inferior en vivo de fluctuaciones B-scan *Wiggle* y A-scan.

---

## Compilación y Tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure -V
```

---

## Uso de la CLI (`gpr3d`)

### 1. Simular levantamiento 3D multicarril con conductos enterrados y estratos

```bash
./build/gpr3d simulate \
  -o datos/tramo.sgy \
  --lines 3 --line-spacing 0.50 \
  --lat 52.520008 --lon 13.404954 --alt 34.5 \
  --epsr 9.0 --freq 400
```

### 2. Transformar señales a Objetos 3D (`.obj`, `.glb`, `.geojson`) y Fluctuaciones Gráficas (`.png`, `.svg`, `.html`)

```bash
./build/gpr3d transform \
  -i datos/tramo_line01.sgy \
  -i datos/tramo_line02.sgy \
  -i datos/tramo_line03.sgy \
  -o salida/escena_3d.glb \
  -o salida/escena_3d.obj \
  -o salida/objetos_gis.geojson \
  -o salida/fluctuaciones.png \
  -o salida/fluctuaciones.svg \
  -o salida/dashboard_interactivo.html
```

### 3. Abrir el visor interactivo nativo OpenGL/GLFW en tiempo real

```bash
./build/gpr3d view \
  -i datos/tramo_line01.sgy \
  -i datos/tramo_line02.sgy \
  -i datos/tramo_line03.sgy
```

*(Controles en el visor: arrastrar ratón para orbitar/desplazar, rueda para zoom, flechas `Izquierda`/`Derecha` para desplazar la sonda láser de traza y actualizar el A-scan en vivo, teclas `1`–`5` para mostrar/ocultar capas 3D).*

---

## Ejemplo desde C++20 (API SDK)

```cpp
#include "gpr3d/io/reader.hpp"
#include "gpr3d/dsp/object_transformer.hpp"
#include "gpr3d/io/scene_exporter.hpp"
#include "gpr3d/viz/fluctuation_plotter.hpp"

int main() {
    auto bscan = gpr3d::io::read_radar_file("perfil.sgy");
    auto scene = gpr3d::dsp::SignalObjectTransformer::transform(bscan);

    gpr3d::io::SceneExporter3D::write_glb("subsuelo.glb", scene);
    gpr3d::io::SceneExporter3D::write_obj("subsuelo.obj", scene);
    gpr3d::io::SceneExporter3D::write_geojson("subsuelo.geojson", scene);

    gpr3d::viz::FluctuationPlotter::write_png("fluctuaciones.png", scene);
    gpr3d::viz::FluctuationPlotter::write_html_dashboard("dashboard.html", scene);
}
```
