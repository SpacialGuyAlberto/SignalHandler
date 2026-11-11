# gpr3d (`SignalHandler`)

[![C++20](https://img.shields.io/badge/Standard-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-lightgrey.svg)]()
[![Documentation](https://img.shields.io/badge/Docs-PDF%20%26%20LaTeX-green.svg)](docs/)

> **Language / Sprache:** [🇩🇪 Deutsch](#-deutsch) | [🇪🇸 Español](#-español)

---

# 🇩🇪 Deutsch

Modulare Hochleistungsbibliothek in **C++20** und CLI-Werkzeug (`gpr3d`) zur **Erfassung, Aufbereitung, geophysikalischen Interpretation und Transformation von Bodenradar-Signalen (GPR / Georadar)** in **georeferenzierte 3D-Punktwolken**, **semantische 3D-Netzobjekte (BIM/GIS)** und **interaktive Schwankungsdiagramme**.

---

## Inhaltsverzeichnis (Deutsch)
1. [Hauptmerkmale](#hauptmerkmale-de)
2. [Wissenschaftliche Grundlagen und Literatur](#wissenschaftliche-grundlagen-und-literatur-de)
3. [KI-Erweiterung: Neural SDF und PINN](#ki-erweiterung-neural-sdf-und-pinn-de)
4. [Kompilierung und Ausführung](#kompilierung-und-ausführung-de)
5. [CLI-Nutzung (`gpr3d`)](#cli-nutzung-gpr3d-de)
6. [C++20 SDK-Programmierbeispiel](#c20-sdk-programmierbeispiel-de)
7. [Projektstruktur](#projektstruktur-de)

---

<a name="hauptmerkmale-de"></a>
## Hauptmerkmale

### 1. Natives Lesen und Schreiben von Industrie- und GNSS-Formaten
- **Geophysikalische Standardformate:** SEG-Y (`.sgy` / `.segy`), GSSI RADAN (`.dzt` + `.dzg`) und MALÅ RAMAC (`.rd3` + `.rad` + `.cor`).
- **GNSS-Synchronisation:** Nahtlose Interpolation aus `.csv`, `.cor` und NMEA-0183 (`$GPGGA` / `$GNGGA`) mit automatischer Hebelarm-Korrektur (*Lever-Arm Correction*) zwischen Antennenzentrum und GNSS-Empfänger.
- **Geodätische Transformationen:** Hochpräzise Umwandlung von WGS84-Ellipsoidkoordinaten ($\varphi, \lambda, h$) in UTM-Gitterkoordinaten (WGS84 / GRS80).

### 2. Digitale Signalverarbeitung (DSP) und geophysikalische Pipeline
- **Zweigleisige Vorverarbeitung:**
  - *Dewow*-Filterung (Entfernung niederfrequenter Gleichspannungsdrifts).
  - Phasenloser Butterworth-Bandpassfilter im Frequenzbereich mittels `FFTW3`.
  - Automatische Nullzeitkorrektur (*Time-Zero Correction*) über die Hilbert-Einhüllende.
  - Hintergrundsubtraktion (*Background Removal*) und sphärische Divergenzkompensation (SEC-Gain).
  - **2D/3D-Kirchhoff-Diffraktionsmigration** zur räumlichen Fokussierung hyperbolischer Reflexionen.
- **Semantischer Signal-zu-3D-Objekt-Transformator (`SignalObjectTransformer`):**
  - **Zylindrische Leitungen (`ConduitObject3D`):** Automatische Erkennung migrierter Scheitelpunkte, Analyse der elektromagnetischen Phasenpolarität (*dielektrische Glasfaserschutzrohre* vs. *metallische Wasserleitungen*), 3D-Interpolation paralleler Scanlinien und Triangulation mittels Frenet-Serret-Begleitdreibein.
  - **Stratigraphische Schichthorizonte (`LayerHorizonObject3D`):** Verfolgung kontinuierlicher Schichtgrenzen durch lateralen Medianfilter und 3D-Bodenhorizont-Triangulation.
  - **Lokalisierte Hohlräume und Anomalien (`AnomalyObject3D`):** Georeferenzierte 3D-Ellipsoide.

### 3. Geophysikalische Bodeninversion und Materialklassifizierung
- **Geschwindigkeitsanalyse (*Neidell-Taner Semblance*):** Automatische Ermittlung der tatsächlichen Ausbreitungsgeschwindigkeit $v$ ($\text{m/ns}$) und relativen Dielektrizitätszahl $\varepsilon_r$ direkt aus den Beugungshyperbeln.
- **Fresnel-Mehrschichteninversion:** Schätzung dielektrischer Kontraste an Schichtgrenzen.
- **Dämpfung und Leitfähigkeit:** Berechnung der frequenzabhängigen Dämpfung $\alpha$ ($\text{dB/m}$) und scheinbaren Leitfähigkeit $\sigma$ ($\text{mS/m}$) über spektrale Schwerpunktsverschiebung (*Quan-Harris*).
- **Bayessche Klassifizierung nach ASTM D6432:** Automatische Zuweisung von PBR-Farben und lithologischen Profilen (*Asphalt*, *Beton*, *Schotter*, *Sand*, *Ton*, *PVC/HDPE*, *Metall*, *Kavität*).

### 4. 3D-Visualisierung und Exportformate
- **Industrieller Export:** Wavefront `.obj` + `.mtl`, Khronos glTF 2.0 binär (`.glb`), AutoCAD 3D `.dxf`, BIM IFC 4.0 (`.ifc`) und GIS 3D `.geojson`, Punktwolken in `.ply`, `.xyz` und `.csv`.
- **Schwankungsdiagramme (Cairo):** Hochauflösende 4-Panel-Grafiken in `.png` und Vektor-`.svg` (B-Scan, A-Scan mit Einhüllender, FFT-Leistungsspektrum und RMS-Energie).
- **Interaktives HTML5-Dashboard:** Eigenständige Three.js-Visualisierung mit synchronisierter virtueller Messsonde und Tiefenrevolver (*Depth-Slice*).
- **Nativer C++ Desktop-Viewer:** OpenGL 3.3+ und GLFW mit 60 FPS und Echtzeit-A-Scan-HUD.

---

<a name="wissenschaftliche-grundlagen-und-literatur-de"></a>
## Wissenschaftliche Grundlagen und Literatur

Die Implementierung basiert auf den anerkannten deutschsprachigen Standardlehrbüchern:

* **Mathematik:** Tilo Arens, Frank Hettlich et al., *Mathematik*, Springer Spektrum.
  - *Kapitel 21:* Komplexe Zahlen und Fouriertransformation ($e^{-i\omega t}$).
  - *Kapitel 24 & 25:* Vektoranalysis, implizite Flächen, Gradientenfeld $\nabla F$ und Frenet-Serret-Formeln.
  - *Kapitel 27:* Partielle Differentialgleichungen und nichtlineare Eikonal-Gleichung $\|\nabla \Phi\| = 1$.
* **Physik:** Paul A. Tipler, Gene Mosca, *Physik für Wissenschaftler und Ingenieure*, Springer Spektrum.
  - *Kapitel 15 & 20:* Wellenausbreitung, Maxwell-Gleichungen und Reflexion an dielektrischen Grenzflächen (Fresnel-Gleichungen).
* **Modernes C++:** Ulrich Breymann, *C++ professionell anwenden*, Carl Hanser Verlag.
  - *Kapitel 7, 8 & 11:* RAII, Null-Kopien-Speicherverwaltung mit `std::span` und sequenzielle Datenstrukturen.
  - *Kapitel 18:* C-Bibliotheksintegration und typsichere C++20-Schnittstellen.

Ausführliche Abhandlungen finden sich in den beigelegten Dokumenten:
- [`Fundamentos_Fisica_Matematica_Cpp_GPR3D.pdf`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/Fundamentos_Fisica_Matematica_Cpp_GPR3D.pdf)
- [`Guia_Replicacion_Paso_a_Paso_GPR3D.pdf`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/Guia_Replicacion_Paso_a_Paso_GPR3D.pdf)

---

<a name="ki-erweiterung-neural-sdf-und-pinn-de"></a>
## KI-Erweiterung: Neural SDF und PINN

Um reale, unregelmäßige Schadensbilder (mechanisch deformierte/gequetschte Rohre, $45^\circ$/$90^\circ$-Krümmer, T-Abzweige, Auskolkungen und Hohlräume) sub-millimetergenau zu rekonstruieren, verfügt das Projekt über eine Deep-Learning-Architektur:

* **Neural Signed Distance Fields (Neural SDF):** Kontinuierliche räumliche Darstellung $f_\theta(\mathbf{x}, \mathbf{z}) = d$.
* **Physics-Informed Neural Networks (PINN):** Durchsetzung der Eikonal-Bedingung $\|\nabla_\mathbf{x} f_\theta\| = 1$ im Verlustfunktional zur Eliminierung von Radarrauschen.
* **FDTD-Digitaler Zwilling mit `gprMax`:** Generierung prozeduraler CAD-Datensätze (15.000 synthetische Radargramme) zur Überwindung des Mangels an realen Grabungsdaten.
* **Autonomer C++20 ONNX Runtime Inferenzmotor:** Ausführung ohne Python-Laufzeitumgebung mit garantierter Latenz $< 0.5\text{ ms}$.
* **Robuste Fallback-Architektur:** Automatische Umschaltung auf das deterministische Frenet-Serret-Zylindermodell, falls das Vertrauensniveau der KI unter $50\%$ sinkt.

Details siehe: [`Plan_Redes_Neuronales_Reconstruccion_3D.pdf`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/Plan_Redes_Neuronales_Reconstruccion_3D.pdf).

---

<a name="kompilierung-und-ausführung-de"></a>
## Kompilierung und Ausführung

### Voraussetzungen
- GCC 11+ oder Clang 14+ mit voller C++20-Unterstützung.
- CMake $\ge 3.20$.
- Bibliotheken: `libfftw3-dev`, `libcairo2-dev`, `libglfw3-dev`, `libgl1-mesa-dev`.

```bash
# 1. Repository klonen und Build-Ordner erstellen
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# 2. Parallel kompilieren
cmake --build build -j$(nproc)

# 3. Testsuite ausführen
ctest --test-dir build --output-on-failure -V
```

---

<a name="cli-nutzung-gpr3d-de"></a>
## CLI-Nutzung (`gpr3d`)

### 1. Synthetische 3D-Befahrung mit Leitungen und Schichten simulieren
```bash
./build/gpr3d simulate \
  -o data/survey.sgy \
  --lines 3 --line-spacing 0.50 \
  --lat 52.520008 --lon 13.404954 --alt 34.5 \
  --epsr 9.0 --freq 400
```

### 2. Signale in 3D-Netzobjekte, GIS-Layer und Diagramme transformieren
```bash
./build/gpr3d transform \
  -i data/survey_line01.sgy \
  -i data/survey_line02.sgy \
  -i data/survey_line03.sgy \
  -o output/scene.glb \
  -o output/scene.obj \
  -o output/objects.geojson \
  -o output/fluctuations.png \
  -o output/fluctuations.svg \
  -o output/dashboard.html
```

### 3. Nativer interaktiver 3D-Echtzeit-Viewer
```bash
./build/gpr3d view \
  -i data/survey_line01.sgy \
  -i data/survey_line02.sgy \
  -i data/survey_line03.sgy
```
*(Steuerung: Maus ziehen zum Orbitieren/Schwenken, Scrollrad für Zoom, Pfeiltasten `Links`/`Rechts` zum Bewegen der Lasersonde mit A-Scan-Aktualisierung in Echtzeit, Zifferntasten `1`–`5` zum Umschalten der 3D-Ebenen).*

---

<a name="c20-sdk-programmierbeispiel-de"></a>
## C++20 SDK-Programmierbeispiel

```cpp
#include "gpr3d/io/reader.hpp"
#include "gpr3d/dsp/object_transformer.hpp"
#include "gpr3d/io/scene_exporter.hpp"
#include "gpr3d/viz/fluctuation_plotter.hpp"

int main() {
    // 1. Radardatei einlesen (DZT, RD3 oder SEG-Y)
    auto bscan = gpr3d::io::read_radar_file("messung.dzt");

    // 2. DSP-Filterung, Semblance-Geschwindigkeitsanalyse und 3D-Objekttransformation
    auto scene = gpr3d::dsp::SignalObjectTransformer::transform(bscan);

    // 3. Industrielle 3D-Modelle exportieren
    gpr3d::io::SceneExporter3D::write_glb("untergrund.glb", scene);
    gpr3d::io::SceneExporter3D::write_obj("untergrund.obj", scene);
    gpr3d::io::SceneExporter3D::write_geojson("leitungen.geojson", scene);

    // 4. Analytische Diagramme und Web-Dashboard generieren
    gpr3d::viz::FluctuationPlotter::write_png("signale.png", scene);
    gpr3d::viz::FluctuationPlotter::write_html_dashboard("dashboard.html", scene);
    
    return 0;
}
```

---

<a name="projektstruktur-de"></a>
## Projektstruktur

```
SignalHandler/
├── apps/               # CLI-Einstiegspunkte (gpr3d_cli.cpp)
├── include/gpr3d/      # C++20 Öffentliche API-Header
│   ├── core/           # Grundlegende Datentypen, Endianness-Handler
│   ├── dsp/            # FFT, Butterworth-Filter, Migration, Bodeninversion
│   ├── geo/            # WGS84/UTM Koordinatentransformation, ASTM-Materialdatenbank
│   ├── io/             # Parser für SEG-Y, DZT/DZG, RD3/RAD, GNSS und 3D-Exporteure
│   ├── scene/          # 3D-Szenendefinitionen (Conduit, Layer, Anomaly)
│   ├── sim/            # Synthetischer GPR-Datengenerator
│   └── viz/            # Cairo-Plotter und nativer OpenGL/GLFW Viewer
├── src/                # Implementierung der C++20 Module
├── docs/               # Technische Leitfäden, LaTeX-Quellen und Vektordiagramme
│   ├── diagrams/       # PDF/SVG Vektordiagramme der Architektur
│   ├── Fundamentos_Fisica_Matematica_Cpp_GPR3D.pdf
│   ├── Guia_Replicacion_Paso_a_Paso_GPR3D.pdf
│   └── Plan_Redes_Neuronales_Reconstruccion_3D.pdf
├── dashboard/          # Three.js Web-Dashboard
├── test/               # Unittests und Validierungsskripte
└── CMakeLists.txt      # Haupt-Build-Konfiguration
```

---
---

# 🇪🇸 Español

Librería modular de alto rendimiento en **C++20** y herramienta de línea de comandos (`gpr3d`) para **adquirir, acondicionar, interpretar geofísicamente y transformar señales de Georradar (GPR / GDR)** en **nubes de puntos 3D georreferenciadas**, **mallas semánticas 3D de ingeniería (BIM/GIS)** y **gráficos interactivos de fluctuación**.

---

## Índice General (Español)
1. [Características Principales](#características-principales-es)
2. [Fundamentos Científicos y Bibliografía](#fundamentos-científicos-y-bibliografía-es)
3. [Módulo de IA: Neural SDF y PINN](#módulo-de-ia-neural-sdf-y-pinn-es)
4. [Compilación y Pruebas](#compilación-y-pruebas-es)
5. [Uso de la CLI (`gpr3d`)](#uso-de-la-cli-gpr3d-es)
6. [Ejemplo con C++20 SDK API](#ejemplo-con-c20-sdk-api-es)
7. [Estructura del Proyecto](#estructura-del-proyecto-es)

---

<a name="características-principales-es"></a>
## Características Principales

### 1. Lectura y Escritura Nativa de Formatos Industriales y GNSS
- **Formatos Geofísicos Estándar:** SEG-Y (`.sgy` / `.segy`), GSSI RADAN (`.dzt` + `.dzg`) y MALÅ RAMAC (`.rd3` + `.rad` + `.cor`).
- **Sincronización GNSS:** Interpolación rigurosa desde archivos `.csv`, `.cor` y sentencias NMEA-0183 (`$GPGGA` / `$GNGGA`), incorporando corrección de brazo de palanca (*lever-arm*) entre el centro de fase de la antena y el receptor GNSS.
- **Transformación Geodésica:** Conversión precisa de coordenadas elipsoidales WGS84 ($\varphi, \lambda, h$) a coordenadas cartográficas proyectadas UTM (elipsoides WGS84 y GRS80).

### 2. Procesamiento Digital de Señales (DSP) y Pipeline Geofísico
- **Preprocesado de Doble Rama:**
  - Filtrado *Dewow* para supresión de componentes de continua y deriva instrumental.
  - Filtro paso banda Butterworth de fase nula en el dominio frecuencial con `FFTW3`.
  - Calibración automática de tiempo cero (*Time-Zero Correction*) mediante la envolvente analítica de Hilbert.
  - Sustracción de fondo (*Background Removal*) y corrección de divergencia esférica (ganancia SEC).
  - **Migración 2D/3D de Kirchhoff** para colapsar hipérbolas de difracción a su geometría espacial real.
- **Transformador Semántico de Señal a Objetos 3D (`SignalObjectTransformer`):**
  - **Conductos y Tuberías Cilíndricas (`ConduitObject3D`):** Detección de ápices migrados, discriminación automática de polaridad electromagnética (*conducto plástico/fibra óptica* frente a *tubería metálica/agua*), interpolación 3D entre pasadas paralelas y generación de mallas cilíndricas mediante el triedro móvil de Frenet-Serret.
  - **Horizontes Estratigráficos (`LayerHorizonObject3D`):** Trazado continuo de interfaces de suelo mediante filtrado de mediana lateral y triangulación 3D.
  - **Anomalías y Cavidades Localizadas (`AnomalyObject3D`):** Modelado elipsoidal 3D georreferenciado.

### 3. Inversión Geofísica de Suelos y Clasificación ASTM
- **Análisis de Semblanza de Velocidad (*Neidell-Taner*):** Cálculo automático de la velocidad de propagación $v$ ($\text{m/ns}$) y de la permitividad dieléctrica relativa $\varepsilon_r$ a partir de la curvatura hiperbólica.
- **Inversión de Reflectancia de Fresnel:** Estimación de contrastes dieléctricos en interfaces estratificadas.
- **Atenuación Espectral y Pérdidas:** Determinación de $\alpha$ ($\text{dB/m}$) y conductividad aparente $\sigma$ ($\text{mS/m}$) mediante el corrimiento espectral del centroide (*Quan-Harris*).
- **Clasificador Bayesiano ASTM D6432:** Asignación automática de propiedades litológicas (*Asfalto*, *Hormigón*, *Grava*, *Arena*, *Arcilla*, *PVC/HDPE*, *Acero*, *Cavidad con aire*) con materiales PBR y columna litológica estratigráfica.

### 4. Visualización 3D y Formatos de Exportación
- **Interoperabilidad CAD/BIM/GIS:** Wavefront `.obj` + `.mtl`, Khronos glTF 2.0 binario (`.glb`), AutoCAD 3D `.dxf`, BIM IFC 4.0 (`.ifc`) y GIS `.geojson` 3D, además de nubes de puntos `.ply`, `.xyz` y `.csv`.
- **Gráficos de Fluctuación (Cairo):** Renderizado de alta resolución en `.png` y `.svg` vectorial en 4 paneles (radargrama B-scan, traza A-scan con envolvente, espectro de amplitud FFT y energía RMS por traza).
- **Dashboard Web Interactivo:** Visor orbital Three.js con plano de corte horizontal interactivo (*Depth-Slice*) y sonda virtual sincronizada.
- **Visor Nativo de Escritorio en C++:** Renderizado 3D a 60 FPS con OpenGL 3.3+ y GLFW con HUD de trazas A-scan en tiempo real.

---

<a name="fundamentos-científicos-y-bibliografía-es"></a>
## Fundamentos Científicos y Bibliografía

El diseño riguroso del software se fundamenta en la literatura universitaria estándar de referencia alemana:

* **Matemáticas:** Tilo Arens, Frank Hettlich et al., *Mathematik*, Springer Spektrum.
  - *Kapitel 21:* Números complejos y transformada continua y discreta de Fourier ($e^{-i\omega t}$).
  - *Kapitel 24 & 25:* Geometría diferencial de superficies en el espacio, gradiente $\nabla F$ y fórmulas del triedro de Frenet-Serret.
  - *Kapitel 27:* Ecuaciones diferenciales en derivadas parciales y la formulación Eikonal $\|\nabla \Phi\| = 1$.
* **Física:** Paul A. Tipler, Gene Mosca, *Physik für Wissenschaftler und Ingenieure*, Springer Spektrum.
  - *Kapitel 15 & 20:* Ondas electromagnéticas, ecuaciones de Maxwell y reflexión en interfases dieléctricas (Fresnel).
* **C++ Profesional:** Ulrich Breymann, *C++ professionell anwenden*, Carl Hanser Verlag.
  - *Kapitel 7, 8 & 11:* Gestión RAII, vistas de memoria contigua con `std::span` y estructuras de datos eficientes para mallas poligonales.
  - *Kapitel 18:* Integración con librerías C nativas y diseño de interfaces en C++20.

Guías técnicas completas compiladas disponibles en el repositorio:
- [`Fundamentos_Fisica_Matematica_Cpp_GPR3D.pdf`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/Fundamentos_Fisica_Matematica_Cpp_GPR3D.pdf)
- [`Guia_Replicacion_Paso_a_Paso_GPR3D.pdf`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/Guia_Replicacion_Paso_a_Paso_GPR3D.pdf)

---

<a name="módulo-de-ia-neural-sdf-y-pinn-es"></a>
## Módulo de IA: Neural SDF y PINN

Para reconstruir patologías y accesorios de tuberías reales con precisión sub-milimétrica (tubos aplastados elípticamente por tráfico vehicular, codos a $45^\circ$/$90^\circ$, arquetas de empalme y socavones irregulares), `gpr3d` implementa una arquitectura híbrida de Deep Learning:

* **Neural Signed Distance Fields (Neural SDF):** Representación geométrica implícita continua $f_\theta(\mathbf{x}, \mathbf{z}) = d$.
* **Physics-Informed Neural Networks (PINN):** Regularización geométrica mediante penalización Eikonal $\|\nabla_\mathbf{x} f_\theta\| = 1$ en la función de coste.
* **Gemelos Digitales FDTD con `gprMax`:** Generación procedural de 15.000 pares sintéticos de entrenamiento para suplir la escasez de datos reales con verificación láser.
* **Motor Nativo de Inferencia C++20 con ONNX Runtime:** Inferencia en producción sin intérprete de Python ni bloqueos por GIL (latencia $< 0.5\text{ ms}$).
* **Arquitectura Resiliente de Respaldo (*Fallback*):** Conmutación transparente al cilindro determinista de Frenet-Serret si la confianza de la red desciende del $50\%$.

Plan de implementación detallado: [`Plan_Redes_Neuronales_Reconstruccion_3D.pdf`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/Plan_Redes_Neuronales_Reconstruccion_3D.pdf).

---

<a name="compilación-y-pruebas-es"></a>
## Compilación y Pruebas

### Requisitos del Sistema
- Compilador compatible con C++20 (GCC 11+ o Clang 14+).
- CMake $\ge 3.20$.
- Paquetes de desarrollo: `libfftw3-dev`, `libcairo2-dev`, `libglfw3-dev`, `libgl1-mesa-dev`.

```bash
# 1. Configurar el entorno de compilación optimizado
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# 2. Compilar el proyecto en paralelo
cmake --build build -j$(nproc)

# 3. Ejecutar la suite de pruebas
ctest --test-dir build --output-on-failure -V
```

---

<a name="uso-de-la-cli-gpr3d-es"></a>
## Uso de la CLI (`gpr3d`)

### 1. Simular un escaneo 3D multicarril con conductos y estratos
```bash
./build/gpr3d simulate \
  -o data/tramo.sgy \
  --lines 3 --line-spacing 0.50 \
  --lat 52.520008 --lon 13.404954 --alt 34.5 \
  --epsr 9.0 --freq 400
```

### 2. Transformar radargramas a mallas 3D, capas GIS y gráficos de fluctuación
```bash
./build/gpr3d transform \
  -i data/tramo_line01.sgy \
  -i data/tramo_line02.sgy \
  -i data/tramo_line03.sgy \
  -o salida/escena_3d.glb \
  -o salida/escena_3d.obj \
  -o salida/objetos_gis.geojson \
  -o salida/fluctuaciones.png \
  -o salida/fluctuaciones.svg \
  -o salida/dashboard_interactivo.html
```

### 3. Lanzar el visor 3D interactivo OpenGL/GLFW
```bash
./build/gpr3d view \
  -i data/tramo_line01.sgy \
  -i data/tramo_line02.sgy \
  -i data/tramo_line03.sgy
```
*(Controles del visor: arrastrar ratón para orbitar/desplazar, rueda para zoom, flechas `Izquierda`/`Derecha` para mover la sonda láser con actualización en vivo del A-scan, teclas `1`–`5` para encender/apagar capas 3D).*

---

<a name="ejemplo-con-c20-sdk-api-es"></a>
## Ejemplo con C++20 SDK API

```cpp
#include "gpr3d/io/reader.hpp"
#include "gpr3d/dsp/object_transformer.hpp"
#include "gpr3d/io/scene_exporter.hpp"
#include "gpr3d/viz/fluctuation_plotter.hpp"

int main() {
    // 1. Cargar archivo de radar (DZT de GSSI, RD3 de MALA o SEG-Y estándar)
    auto bscan = gpr3d::io::read_radar_file("perfil.dzt");

    // 2. Procesamiento geofísico, cálculo de velocidad y reconstrucción 3D
    auto scene = gpr3d::dsp::SignalObjectTransformer::transform(bscan);

    // 3. Exportar modelos 3D para ingeniería
    gpr3d::io::SceneExporter3D::write_glb("subsuelo.glb", scene);
    gpr3d::io::SceneExporter3D::write_obj("subsuelo.obj", scene);
    gpr3d::io::SceneExporter3D::write_geojson("redes.geojson", scene);

    // 4. Generar reportes gráficos analíticos y dashboard web
    gpr3d::viz::FluctuationPlotter::write_png("fluctuaciones.png", scene);
    gpr3d::viz::FluctuationPlotter::write_html_dashboard("dashboard.html", scene);

    return 0;
}
```

---

<a name="estructura-del-proyecto-es"></a>
## Estructura del Proyecto

```
SignalHandler/
├── apps/               # Puntos de entrada ejecutables CLI (gpr3d_cli.cpp)
├── include/gpr3d/      # Cabeceras públicas de la API en C++20
│   ├── core/           # Tipos de datos fundamentales, gestión de endianness
│   ├── dsp/            # FFT, filtros Butterworth, migración, inversión de suelos
│   ├── geo/            # Conversión geodésica WGS84/UTM, base de datos ASTM
│   ├── io/             # Parsers de SEG-Y, DZT/DZG, RD3/RAD, GNSS y exportadores 3D
│   ├── scene/          # Definición de entidades 3D (conductos, capas, anomalías)
│   ├── sim/            # Generador sintético de señales GPR
│   └── viz/            # Renderizado Cairo y visor nativo OpenGL/GLFW
├── src/                # Implementación de los módulos C++20
├── docs/               # Guías técnicas, fuentes LaTeX y diagramas vectoriales
│   ├── diagrams/       # Diagramas de arquitectura en formato PDF y SVG
│   ├── Fundamentos_Fisica_Matematica_Cpp_GPR3D.pdf
│   ├── Guia_Replicacion_Paso_a_Paso_GPR3D.pdf
│   └── Plan_Redes_Neuronales_Reconstruccion_3D.pdf
├── dashboard/          # Dashboard web interactivo en Three.js
├── test/               # Banco de pruebas unitarias y validación
└── CMakeLists.txt      # Script maestro de compilación con CMake
```

---

## Licencia

Este proyecto está distribuido bajo la Licencia MIT. Consulta el archivo `LICENSE` para más detalles.
