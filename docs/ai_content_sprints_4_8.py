#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Contenido de Sprints IA-4 a IA-8 para la Guia Tecnica de Redes Neuronales 3D."""

def get_ai_sprints_4_to_8():
    return r"""
% =============================================================================
% SPRINT IA-4: EXTRACTOR MARCHING CUBES C++20 CON NORMALES ANALITICAS
% =============================================================================
\section{Sprint IA-4: Extractor de Mallas Poligonales Marching Cubes C++20 con Normales Anal{\'\i}ticas Exactas}

\subsection{1. Qu{\'e} Estudiar en la Literatura T{\'e}cnica}
\begin{itemize}
    \item \textbf{Lorensen \& Cline (1987, \textit{Marching Cubes: A High Resolution 3D Surface Construction Algorithm})}:
    \begin{itemize}
        \item Indexaci{\'o}n de los 8 v{\'e}rtices de cada celda c{\'u}bica (patr{\'o}n binario de 8 bits $\implies 256$ configuraciones posibles).
        \item Tablas de aristas intersectadas (\texttt{edgeTable}) y de conectividad de tri{\'a}ngulos (\texttt{triTable}).
    \end{itemize}
    \item \textbf{\textit{Mathematik} (Tilo Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 24 \& 25 (\textit{Fl{\"a}chen im Raum und Gradienten})}: Interpolaci{\'o}n lineal a lo largo de las aristas del cubo para ubicar el conjunto de nivel cero exacto $f_\theta(\mathbf{x}) = 0$:
        \begin{equation}
            \mathbf{P}_{\text{cruce}} = \mathbf{P}_1 + \frac{0 - d_1}{d_2 - d_1} (\mathbf{P}_2 - \mathbf{P}_1)
        \end{equation}
        donde $d_1 = f_\theta(\mathbf{P}_1)$ y $d_2 = f_\theta(\mathbf{P}_2)$.
        \item C{\'a}lculo del vector normal unitario exacto evaluando el gradiente anal{\'\i}tico del campo:
        \begin{equation}
            \mathbf{n}(\mathbf{P}) = \frac{\nabla f_\theta(\mathbf{P})}{\|\nabla f_\theta(\mathbf{P})\|_2}
        \end{equation}
    \end{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Ulrich Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 8 (\textit{Sequenzielle Datenstrukturen})}: Dise{\~n}o de \texttt{MeshData3D} con b{\'u}feres contiguos alineados para v{\'e}rtices, normales y coordenadas de textura compatibles con GPUs modernas.
    \end{itemize}
\end{itemize}

\subsection{2. Por Qu{\'e} Hay que Implementarlo}
Una representaci{\'o}n impl{\'\i}cita Neural SDF es una funci{\'o}n matem{\'a}tica pura en memoria; para exportarla a est{\'a}ndares de ingenier{\'\i}a (glTF/GLB, AutoCAD DXF, Three.js) es indispensable convertirla a una \textbf{malla de pol{\'\i}gonos triangulares}:
\begin{enumerate}
    \item \textbf{Superaci{\'o}n del Ruido de Normales de Malla Discreta:} La mayor{\'\i}a de los extractores de mallas calculan los vectores normales promediando el producto vectorial $(\mathbf{v}_2 - \mathbf{v}_1) \times (\mathbf{v}_3 - \mathbf{v}_1)$ de las caras adyacentes. En geometr{\'\i}as reconstruidas a partir de se{\~n}ales de radar con ruido, este m{\'e}todo amplifica la rugosidad num{\'e}rica y genera artefactos de sombreado rugoso no f{\'\i}sico.
    \item \textbf{Ventaja {\'U}nica de las Redes Neuronales SDF:} Dado que la red $f_\theta$ es una funci{\'o}n continua y diferenciable, podemos evaluar el gradiente exacto $\nabla f_\theta$ mediante diferencias finitas centrales con paso infinitesimal $h = 10^{-4}\text{ m}$:
    \begin{equation}
        \frac{\partial f_\theta}{\partial x} \approx \frac{f_\theta(x+h, y, z) - f_\theta(x-h, y, z)}{2h}
    \end{equation}
    Esto produce normales perfectamente suaves (\textit{smooth normals}) y matem{\'a}ticamente exactas, eliminando por completo la necesidad de filtros de suavizado laplaciano posteriores que deforman las dimensiones de las tuber{\'\i}as.
\end{enumerate}

\subsection{3. Tareas Paso a Paso de Implementaci{\'o}n}

\begin{enumerate}
    \item \textbf{Definici{\'o}n de estructuras de malla y cabecera del extractor}:
    Crear el archivo \texttt{include/gpr3d/ai/marching\_cubes.hpp}:
\begin{verbatim}
#pragma once
#include <vector>
#include <array>
#include <functional>
#include "gpr3d/geometry/geometry_types.hpp"

namespace gpr3d::ai {

struct BoundingBox3D {
    Vector3D min_pt;
    Vector3D max_pt;
};

struct MeshData3D {
    std::vector<Vector3D> vertices;
    std::vector<Vector3D> normals;
    std::vector<uint32_t> indices;
};

class MarchingCubesExtractor {
public:
    using SDFFunction = std::function<float(const Vector3D&)>;

    // Extrae la malla triangular evaluando el campo SDF en una cuadricula adaptativa
    static MeshData3D extract(
        const SDFFunction& sdf,
        const BoundingBox3D& bbox,
        float cell_size,
        float iso_level = 0.0f
    );

private:
    static Vector3D compute_normal(const SDFFunction& sdf, const Vector3D& p, float h = 1e-4f);
    static Vector3D interpolate_vertex(
        float iso_level,
        const Vector3D& p1, const Vector3D& p2,
        float val1, float val2
    );
};

} // namespace gpr3d::ai
\end{verbatim}

    \item \textbf{Implementaci{\'o}n del algoritmo con tablas est{\'a}ticas de indexaci{\'o}n}:
    Crear el archivo \texttt{src/ai/marching\_cubes.cpp} conteniendo \texttt{edgeTable[256]} y \texttt{triTable[256][16]}, iterando sobre los v{\'o}xeles de la caja delimitadora, evaluando el signo de los 8 v{\'e}rtices, interpolando las aristas activas y calculando las normales con la funci{\'o}n anal{\'\i}tica \texttt{compute\_normal}.
\end{enumerate}

\subsection{4. Prueba de Verificaci{\'o}n Num{\'e}rica}
Implementar \texttt{test/test\_marching\_cubes.cpp}:
\begin{enumerate}
    \item Definir un campo anal{\'\i}tico de una esfera perfecta de radio $R = 0.25\text{ m}$ centrada en el origen: $f(x, y, z) = \sqrt{x^2 + y^2 + z^2} - 0.25$.
    \item Ejecutar \texttt{MarchingCubesExtractor::extract} en la caja $[-0.35, 0.35]^3\text{ m}$ con paso de celda $\Delta = 0.01\text{ m}$ ($1\text{ cm}$).
    \item Para cada v{\'e}rtice $\mathbf{v}$ generado, calcular su distancia al origen $r = \|\mathbf{v}\|$ y el {\'a}ngulo entre la normal generada $\mathbf{n}(\mathbf{v})$ y la normal te{\'o}rica $\mathbf{v} / \|\mathbf{v}\|$.
    \item \textbf{Criterio de Aprobaci{\'o}n}: El error m{\'a}ximo de radio debe ser $|r - 0.25| < 0.001\text{ m}$ ($1\text{ mm}$), y la desviaci{\'o}n angular media de las normales debe ser inferior a $0.4^\circ$, validando la fidelidad poligonal del extractor.
\end{enumerate}

\newpage

% =============================================================================
% SPRINT IA-5: RED 3 (LAYERTRANSFORMER) - ESTRATIGRAFIA CONTINUA
% =============================================================================
\section{Sprint IA-5: Red 3 (LayerTransformer) - Segmentaci{\'o}n y Reconstrucci{\'o}n de Horizontes Estratigr{\'a}ficos}

\subsection{1. Qu{\'e} Estudiar en la Literatura T{\'e}cnica}
\begin{itemize}
    \item \textbf{Vaswani et al. (2017, \textit{Attention is All You Need})}:
    \begin{itemize}
        \item Mecanismo de auto-atenci{\'o}n escalada (\textit{Scaled Dot-Product Attention}) y codificaci{\'o}n posicional senoidal para secuencias temporales y espaciales.
    \end{itemize}
    \item \textbf{\textit{Physik f{\"u}r Wissenschaftler und Ingenieure} (Tipler \& Mosca)}:
    \begin{itemize}
        \item \textbf{Kapitel 15 (\textit{Reflexion an Grenzfl{\"a}chen})}: Interfaces de transici{\'o}n diel{\'e}ctrica entre capas de suelo (asfalto $\varepsilon_{r1} \approx 4$, base de grava $\varepsilon_{r2} \approx 9$, arcilla saturada $\varepsilon_{r3} \approx 20$).
    \end{itemize}
    \item \textbf{\textit{Mathematik} (Tilo Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 24 (\textit{Parametrische Fl{\"a}chen})}: Superficies de elevaci{\'o}n geol{\'o}gica continuas $z = h_k(x, y)$ y regularizaci{\'o}n de suavidad mediante m{\'\i}nima curvatura (laplaciano de la superficie).
    \end{itemize}
\end{itemize}

\subsection{2. Por Qu{\'e} Hay que Implementarlo}
El suelo nunca es un bloque homog{\'e}neo; est{\'a} formado por capas superpuestas de pavimento, base y estratos naturales:
\begin{enumerate}
    \item \textbf{Vulnerabilidad de los Detectores Cl{\'a}sicos de Picos:} El algoritmo tradicional de seguimiento de l{\'\i}neas de reflectores se interrumpe cada vez que una tuber{\'\i}a o una piedra cruza el horizonte, generando hip{\'e}rbolas de difracci{\'o}n que enmascaran la interfaz continua de la capa.
    \item \textbf{Auto-Atenci{\'o}n para Continuidad Espacial de Largo Alcance:} La arquitectura Transformer aprende correlaciones globales entre trazas distantes del radargrama. Si la reflexi{\'o}n de una capa desaparece puntualmente debido a la sombra electromagn{\'e}tica de una tuber{\'\i}a met{\'a}lica, la red reconstruye de forma l{\'o}gica la superficie continua de la capa a trav{\'e}s de la zona de sombra.
    \item \textbf{Restricci{\'o}n F{\'\i}sica de No-Intersecci{\'o}n (Monoton{\'\i}a Geol{\'o}gica):} Los estratos geol{\'o}gicos se depositan secuencialmente por gravedad. Una capa superior $k$ jam{\'a}s puede penetrar f{\'\i}sicamente por debajo de la capa inferior $k+1$. Para hacer cumplir esto, se introduce la penalizaci{\'o}n en la p{\'e}rdida:
    \begin{equation}
        \mathcal{L}_{\text{mono}} = \sum_{k=1}^{K-1} \max\left(0, \; h_k(x, y) - h_{k+1}(x, y) + \delta_{\min}\right)
    \end{equation}
    garantizando que las capas nunca se crucen en el modelo 3D.
\end{enumerate}

\subsection{3. Tareas Paso a Paso de Implementaci{\'o}n}

\begin{enumerate}
    \item \textbf{Modelo PyTorch de LayerTransformer}:
    Crear el archivo \texttt{scripts/ai/models/layer\_transformer.py} con codificador de auto-atenci{\'o}n bidimensional sobre perfiles B-scan y cabezal de regresi{\'o}n continua de cotas de profundidad $h_k(x)$.

    \item \textbf{M{\'o}dulo C++20 de reconstrucci{\'o}n estratigr{\'a}fica}:
    Crear \texttt{include/gpr3d/ai/layer\_reconstructor.hpp} y \texttt{src/ai/layer\_reconstructor.cpp} que convierte los perfiles de cota predichos en mallas de superficies poligonales \texttt{MeshData3D} coloreadas por material de suelo seg{\'u}n la litolog{\'\i}a clasificada en el Sprint 9.
\end{enumerate}

\subsection{4. Prueba de Verificaci{\'o}n Num{\'e}rica}
Implementar \texttt{test/test\_layer\_reconstructor.cpp}:
\begin{enumerate}
    \item Generar un B-scan sint{\'e}tico de 100 trazas con dos capas continuas a profundidades te{\'o}ricas $z_1 = 0.25\text{ m}$ y $z_2 = 0.65\text{ m}$, introduciendo un hueco con se{\~n}al nula en las trazas 40 a 55 para simular una oclusi{\'o}n severa.
    \item Ejecutar la inferencia con \texttt{LayerReconstructor}.
    \item \textbf{Criterio de Aprobaci{\'o}n}: En la regi{\'o}n de oclusi{\'o}n (trazas 40-55), la superficie predicha debe interpolar suavemente con un error absoluto m{\'a}ximo de profundidad inferior a $\pm 15\text{ mm}$, y la separaci{\'o}n $z_2(x) - z_1(x)$ debe ser estrictamente positiva ($\ge 0.35\text{ m}$) en todas las 100 trazas.
\end{enumerate}

\newpage

% =============================================================================
% SPRINT IA-6: INTEGRACION EN SIGNALOBJECTTRANSFORMER Y FALLBACK RESILIENTE
% =============================================================================
\section{Sprint IA-6: Integraci{\'o}n en SignalObjectTransformer y Arquitectura Resiliente de Fallback}

\subsection{1. Qu{\'e} Estudiar en la Literatura T{\'e}cnica}
\begin{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Ulrich Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 6 (\textit{Klassen und Polymorphie})}: Patr{\'o}n Estrategia (\textit{Strategy Pattern}) para selecci{\'o}n din{\'a}mica o configurable de motores de geometr{\'\i}a.
        \item \textbf{Kapitel 14 (\textit{Robuste Programmierung und Exception-Safety})}: Dise{\~n}o de sistemas tolerantes a fallos donde los m{\'o}dulos cr{\'\i}ticos nunca lanzan excepciones no controladas hacia el hilo principal de renderizado.
    \end{itemize}
\end{itemize}

\subsection{2. Por Qu{\'e} Hay que Implementarlo}
En aplicaciones cr{\'\i}ticas de ingenier{\'\i}a civil y seguridad de excavaci{\'o}n (evitar perforar conducciones de gas a presi{\'o}n o cables el{\'e}ctricos de media tensi{\'o}n):
\begin{enumerate}
    \item \textbf{Peligro de Alucinaciones de Redes Neuronales:} En condiciones de campo adversas (suelos extremadamente h{\'u}medos, arcillas salinas con atenuaci{\'o}n r{\'a}pida o interferencias electromagn{\'e}ticas de l{\'\i}neas el{\'e}ctricas), las redes neuronales pueden producir predicciones de baja confianza o formas degeneradas.
    \item \textbf{Arquitectura Resiliente de Fallback Autom{\'a}tico:}
    El sistema debe monitorizar de forma continua dos m{\'e}tricas objetivas de control de calidad:
    \begin{equation}
        \text{Confianza} = 0.5 \cdot \text{Score}_{\text{DetNet}} + 0.5 \cdot \left(1 - \min(1.0, \, \text{ErrorEikonal})\right)
    \end{equation}
    Si la confianza cae por debajo del $50\%$, el pipeline \textbf{desactiva transparentemente la reconstrucci{\'o}n por IA} y activa el algoritmo determinista geom{\'e}trico clásico (triedro de Frenet-Serret del Sprint 10), registrando una advertencia en el informe de auditor{\'\i}a t{\'e}cnica sin detener la operaci{\'o}n de la aplicaci{\'o}n.
\end{enumerate}

\subsection{3. Tareas Paso a Paso de Implementaci{\'o}n}

\begin{enumerate}
    \item \textbf{Actualizaci{\'o}n de la interfaz en SignalObjectTransformer}:
    Editar \texttt{include/gpr3d/geometry/signal\_object\_transformer.hpp}:
\begin{verbatim}
enum class ReconstructionMode {
    DeterministicOnly,   // Cilindros y elipsoides analiticos de Frenet-Serret
    NeuralSDFOnly,       // Malla de alta fidelidad por Neural SDF
    HybridAutoFallback   // IA preferente con fallback determinista si confianza < 0.50
};

struct ReconstructionConfig {
    ReconstructionMode mode = ReconstructionMode::HybridAutoFallback;
    float confidence_threshold = 0.50f;
    float eikonal_tolerance = 0.25f;
    float marching_cubes_resolution = 0.01f; // 1 cm
};
\end{verbatim}

    \item \textbf{Implementaci{\'o}n del conmutador de modo y auditor{\'\i}a}:
    Actualizar \texttt{src/geometry/signal\_object\_transformer.cpp} para ejecutar la l{\'o}gica de decisi{\'o}n, etiquetando cada objeto 3D con un campo booleano \texttt{is\_neural\_reconstruction} y guardando la puntuaci{\'o}n de calidad num{\'e}rica.
\end{enumerate}

\subsection{4. Prueba de Verificaci{\'o}n Num{\'e}rica}
Implementar \texttt{test/test\_hybrid\_transformer\_fallback.cpp}:
\begin{enumerate}
    \item Inyectar un radargrama deliberadamente corrupto con ruido blanco gaussiano extremo ($\text{SNR} = -10\text{ dB}$) donde la red neuronal genera una confianza estimada de apenas el $22\%$.
    \item Ejecutar \texttt{SignalObjectTransformer::transform} en modo \texttt{HybridAutoFallback}.
    \item \textbf{Criterio de Aprobaci{\'o}n}: El sistema debe completar la ejecuci{\'o}n sin excepciones ni cuelgues, devolver un objeto de tuber{\'\i}a con \texttt{is\_neural\_reconstruction = false}, y verificar que la geometr{\'\i}a del conducto coincide con el cilindro param{\'e}trico determinista de respaldo.
\end{enumerate}

\newpage

% =============================================================================
% SPRINT IA-7: CORTES HORIZONTALES C-SCAN Y VISUALIZACION THREE.JS
% =============================================================================
\section{Sprint IA-7: Generador de Cortes Horizontales C-Scan (Depth-Slices) y Visualizaci{\'o}n Interactiva Three.js}

\subsection{1. Qu{\'e} Estudiar en la Literatura T{\'e}cnica}
\begin{itemize}
    \item \textbf{\textit{Mathematik} (Tilo Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 24 (\textit{Rasterung und Interpolation})}: Interpolaci{\'o}n bilineal sobre planos de corte $z = z_0$ a partir de una rejilla tridimensional de radar $V(x, y, z)$.
    \end{itemize}
    \item \textbf{Three.js API y Shader Materials}:
    \begin{itemize}
        \item Creaci{\'o}n de planos de textura semitransparentes (\texttt{MeshBasicMaterial} con canal alfa), mapas de calor de gradiente de color falso (Paleta Viridis / Jet geof{\'\i}sica) y enlaces de control interactivo mediante deslizadores en la interfaz HTML.
    \end{itemize}
\end{itemize}

\subsection{2. Por Qu{\'e} Hay que Implementarlo}
\begin{enumerate}
    \item \textbf{Validaci{\'o}n Cruzada por el Ingeniero Geot{\'e}cnico:} En la pr{\'a}ctica pericial y forense, ning{\'u}n ingeniero civil certifica un replanteo bas{\'a}ndose {\'u}nicamente en una malla 3D opaca. Necesita inspeccionar visualmente la rebanada horizontal de energ{\'\i}a radar (\textit{C-Scan / Depth Slice}) a la profundidad exacta del objeto para comprobar que la tuber{\'\i}a 3D reconstruida se alinea milim{\'e}tricamente con la huella de energ{\'\i}a del radargrama.
    \item \textbf{Navegaci{\'o}n Intuitiva en el Subsuelo:} Disponer de un deslizador en el navegador web que permita cortar el terreno de $0\text{ a }3\text{ metros}$ de profundidad en tiempo real, observando c{\'o}mo las mallas 3D emergen y se sumergen en las l{\'a}minas de corte de radar, proporciona una comprensi{\'o}n espacial instant{\'a}nea para operadores no expertos.
\end{enumerate}

\subsection{3. Tareas Paso a Paso de Implementaci{\'o}n}

\begin{enumerate}
    \item \textbf{Desarrollo del rasterizador C-Scan en C++20}:
    Crear \texttt{include/gpr3d/processing/cscan\_rasterizer.hpp} y \texttt{src/processing/cscan\_rasterizer.cpp}:
\begin{verbatim}
#pragma once
#include <vector>
#include <string>
#include "gpr3d/processing/grid3d.hpp"

namespace gpr3d::processing {

struct DepthSliceImage {
    int width;
    int height;
    float depth_meters;
    std::vector<uint8_t> rgba_pixels; // Imagen lista para textura Three.js
};

class CScanRasterizer {
public:
    static DepthSliceImage generate_slice(
        const Grid3D<float>& migrated_grid,
        float target_depth,
        float velocity = 0.10f
    );
};

} // namespace gpr3d::processing
\end{verbatim}

    \item \textbf{Integraci{\'o}n en el Dashboard Web Three.js}:
    Actualizar \texttt{dashboard/index.html} con un plano m{\'o}vil de visualizaci{\'o}n acoplado al visor 3D:
\begin{verbatim}
// Creacion del plano de corte horizontal interactivo C-Scan
const sliceGeometry = new THREE.PlaneGeometry(surveyWidth, surveyLength);
const sliceMaterial = new THREE.MeshBasicMaterial({
    map: sliceTexture,
    transparent: true,
    opacity: 0.75,
    side: THREE.DoubleSide
});
const sliceMesh = new THREE.Mesh(sliceGeometry, sliceMaterial);
sliceMesh.rotation.x = -Math.PI / 2;
scene.add(sliceMesh);

// Evento de deslizador de profundidad (Depth Slider)
document.getElementById('depthSlider').addEventListener('input', (e) => {
    const depth = parseFloat(e.target.value);
    sliceMesh.position.y = -depth;
    document.getElementById('depthValue').innerText = depth.toFixed(2) + ' m';
});
\end{verbatim}
\end{enumerate}

\subsection{4. Prueba de Verificaci{\'o}n Num{\'e}rica}
Implementar \texttt{test/test\_cscan\_rasterizer.cpp}:
\begin{enumerate}
    \item Construir un volumen sint{\'e}tico de $100 \times 100 \times 100$ con un reflector puntual de alta energ{\'\i}a situado en la celda $(30, 70, 45)$.
    \item Extraer la rebanada C-Scan a la cota correspondiente a la capa 45.
    \item \textbf{Criterio de Aprobaci{\'o}n}: El p{\'\i}xel con m{\'a}ximo valor de brillo en la imagen RGBA devuelta debe ubicarse exactamente en las coordenadas $(x=30, y=70) \pm 0$ p{\'\i}xeles, y su canal alfa debe ser $255$.
\end{enumerate}

\newpage

% =============================================================================
% SPRINT IA-8: CONECTIVIDAD INDUSTRIAL IFC 4.0/DXF, INTERFERENCIAS Y VALIDACION
% =============================================================================
\section{Sprint IA-8: Conectividad Industrial DXF/IFC 4.0, Reporte de Interferencias y Validaci{\'o}n con Radargramas Reales}

\subsection{1. Qu{\'e} Estudiar en la Literatura T{\'e}cnica}
\begin{itemize}
    \item \textbf{Norma Internacional ISO 16739-1:2018 (\textit{Industry Foundation Classes - IFC 4.0})}:
    \begin{itemize}
        \item Definici{\'o}n de esquemas de representaci{\'o}n geom{\'e}trica mediante facetas triangulares: \texttt{IfcTriangulatedFaceSet} y asignaci{\'o}n de clases de ingenier{\'\i}a civil: \texttt{IfcFlowFitting} (codos y derivaciones de tuber{\'\i}a) e \texttt{IfcDistributionFlowElement} (conductos principales).
    \end{itemize}
    \item \textbf{Especificaci{\'o}n Oficial de AutoCAD DXF (Autodesk)}:
    \begin{itemize}
        \item C{\'oacute}digos de grupo para la entidad \texttt{MESH} tridimensional: n{\'u}mero de v{\'e}rtices (grupo 71), n{\'u}mero de caras (grupo 72) y lista ordenada de conectividad de v{\'e}rtices (grupo 90).
    \end{itemize}
\end{itemize}

\subsection{2. Por Qu{\'e} Hay que Implementarlo}
\begin{enumerate}
    \item \textbf{Entrega de Valor en el Flujo BIM Real:} Los ayuntamientos y empresas constructoras no pueden integrar capturas de pantalla o visores experimentales en sus sistemas de informaci{\'o}n geogr{\'a}fica corporativos (GIS/BIM). Exigen archivos est{\'a}ndar \texttt{.ifc} y \texttt{.dxf} que se abran sin conversi{\'o}n en Autodesk Revit, Bentley OpenRoads o QGIS.
    \item \textbf{Detecci{\'o}n Autom{\'a}tica de Interferencias Cr{\'\i}ticas (Clash Detection):} Antes de iniciar una excavaci{\'o}n con retroexcavadora en una calle urbana, la legislaci{\'o}n exige un certificado de distancias m{\'\i}nimas de seguridad entre conductos de gas y canalizaciones de fibra {\'o}ptica ($d \ge 0.30\text{ m}$). El software debe calcular autom{\'a}ticamente las distancias euclidianas m{\'\i}nimas entre todas las mallas generadas por la IA y alertar de posibles colisiones.
    \item \textbf{Cierre del Ciclo de Vida del Software sobre Datos Reales de Campo:} Todo el pipeline (desde la lectura cruda de \texttt{parking.nx15.dzt} y \texttt{ds010\_A.DZT}, pasando por filtros DSP, migraci{\'o}n, detecci{\'o}n 3D por DetNet y Neural SDF) debe validarse sobre radargramas de campo reales para asegurar su robustez ante el ruido del mundo real.
\end{enumerate}

\subsection{3. Tareas Paso a Paso de Implementaci{\'o}n}

\begin{enumerate}
    \item \textbf{Exportador nativo IFC 4.0 en C++20}:
    Crear \texttt{include/gpr3d/export/ifc\_exporter.hpp} y \texttt{src/export/ifc\_exporter.cpp} para serializar mallas \texttt{MeshData3D} en formato STEP (ISO 10303-21) con cabecera est{\'a}ndar IFC 4.0.

    \item \textbf{Motor de detecci{\'o}n de interferencias y c{\'a}lculo de distancias m{\'\i}nimas}:
    Crear \texttt{include/gpr3d/analysis/interference\_detector.hpp} y \texttt{src/analysis/interference\_detector.cpp}:
\begin{verbatim}
#pragma once
#include <vector>
#include "gpr3d/geometry/geometry_types.hpp"
#include "gpr3d/ai/marching_cubes.hpp"

namespace gpr3d::analysis {

struct ClashEvent {
    std::string object_id_a;
    std::string object_id_b;
    float minimum_distance_meters;
    Vector3D closest_point_a;
    Vector3D closest_point_b;
    bool is_critical; // True si distancia < margen de seguridad (ej. 0.30 m)
};

class InterferenceDetector {
public:
    static std::vector<ClashEvent> check_clashes(
        const std::vector<ai::MeshData3D>& meshes,
        const std::vector<std::string>& object_ids,
        float safety_margin = 0.30f
    );
};

} // namespace gpr3d::analysis
\end{verbatim}

    \item \textbf{Script de validaci{\'o}n integral sobre datos reales}:
    Crear \texttt{scripts/ai/validate\_real\_dzt.py} para cargar \texttt{parking.nx15.dzt}, procesar la traza con el pipeline h{\'\i}brido y exportar el informe t{\'e}cnico completo de interferencias y mallas IFC.
\end{enumerate}

\subsection{4. Prueba de Verificaci{\'o}n Num{\'e}rica}
Implementar \texttt{test/test\_real\_pipeline\_ai.cpp}:
\begin{enumerate}
    \item Cargar el archivo de radargrama real del repositorio: \texttt{parking.nx15.dzt}.
    \item Ejecutar el pipeline de extremo a extremo:
    \begin{itemize}
        \item Filtro IIR Bandpass y correcci{\'o}n Time-Zero.
        \item Migraci{\'o}n de Kirchhoff 3D sobre las trazas.
        \item Detecci{\'o}n de ROI con \texttt{GPRDetNet} y reconstrucci{\'o}n morfol{\'o}gica con \texttt{GPRDeepSDF}.
        \item Extracci{\'o}n de malla por \texttt{MarchingCubesExtractor}.
        \item Generaci{\'o}n de archivos de salida \texttt{output/parking\_mesh.ifc} y \texttt{output/parking\_mesh.dxf}.
    \end{itemize}
    \item \textbf{Criterio de Aprobaci{\'o}n}:
    \begin{itemize}
        \item Se deben generar los archivos IFC y DXF sin ning{\'u}n fallo de segmentaci{\'o}n.
        \item El archivo IFC generado debe comenzar con la cabecera v{\'a}lida \texttt{ISO-10303-21;} y contener al menos una entidad \texttt{IFCTRIANGULATEDFACESET}.
        \item La malla resultante debe contener m{\'a}s de $500$ tri{\'a}ngulos con {\'a}rea total no nula y sin v{\'e}rtices con valores \texttt{NaN} o infinitos.
    \end{itemize}
\end{enumerate}

\newpage

% =============================================================================
% SECCION: PLAN DE ACCION Y CRONOGRAMA DE ENTREGABLES
% =============================================================================
\section{Plan de Acci{\'o}n, Cronograma y Matriz de Entregables}

Para asegurar un avance met{\'o}dico e impecable, la implementaci{\'o}n de las redes neuronales 3D se organiza en la matriz de fases mostrada en el Cuadro~\ref{tab:cronograma_ia}.

\begin{table}[htbp]
\centering
\small
\begin{tabular}{|l|p{6.8cm}|p{3.8cm}|p{2.8cm}|}
\hline
\textbf{Fase / Sprint} & \textbf{Objetivo Principal y Entregables de C{\'o}digo} & \textbf{Archivos Clave C++ / Python} & \textbf{Criterio de {\'E}xito} \\ \hline
\textbf{Sprint IA-0} & Motor de inferencia aut{\'o}nomo en C++20 con ONNX Runtime sin dependencias de Python & \texttt{inference\_engine.hpp} \newline \texttt{inference\_engine.cpp} & Latencia $< 0.5\text{ ms}$, Cero copias (\texttt{std::span}) \\ \hline
\textbf{Sprint IA-1} & Generador sint{\'e}tico procedural CAD y simulaciones FDTD batch con gprMax & \texttt{procedural\_cad.py} \newline \texttt{gprmax\_batch.py} & 15,000 pares HDF5, Error de pico $< 0.15\text{ ns}$ \\ \hline
\textbf{Sprint IA-2} & Red 1 (GPR-DetNet) para detecci{\'o}n y delimitaci{\'o}n de cajas envolventes 3D & \texttt{gpr\_detnet.py} \newline \texttt{detnet\_detector.cpp} & IoU 3D $\ge 0.78$, Confianza $> 85\%$ \\ \hline
\textbf{Sprint IA-3} & Red 2 (GPR-DeepSDF) con decodificador continuo y regularizaci{\'o}n Eikonal PINN & \texttt{deep\_sdf\_decoder.py} \newline \texttt{sdf\_evaluator.cpp} & $\|\nabla f_\theta\| \approx 1 \pm 0.04$, Error dist. $< 3\text{ mm}$ \\ \hline
\textbf{Sprint IA-4} & Extractor de mallas Marching Cubes C++20 con normales anal{\'\i}ticas exactas & \texttt{marching\_cubes.hpp} \newline \texttt{marching\_cubes.cpp} & Error radio $< 1\text{ mm}$, Desv. normal $< 0.4^\circ$ \\ \hline
\textbf{Sprint IA-5} & Red 3 (LayerTransformer) para reconstrucci{\'o}n de horizontes estratigr{\'a}ficos & \texttt{layer\_transformer.py} \newline \texttt{layer\_reconstructor.cpp} & Capas sin cruces ($h_1 < h_2 < h_3$), MAE $< 15\text{ mm}$ \\ \hline
\textbf{Sprint IA-6} & Conmutador resiliente de fallback en \texttt{SignalObjectTransformer} & \texttt{signal\_object\_transformer.cpp} & Fallback transparente si Confianza $< 50\%$ \\ \hline
\textbf{Sprint IA-7} & Rebanadas horizontales C-Scan y visualizador interactivo con slider en Three.js & \texttt{cscan\_rasterizer.cpp} \newline \texttt{dashboard/index.html} & Precisi{\'o}n de p{\'\i}xel $\pm 0$, opacidad continua \\ \hline
\textbf{Sprint IA-8} & Exportaci{\'o}n industrial IFC 4.0/DXF, reporte de colisiones y validaci{\'o}n real & \texttt{ifc\_exporter.cpp} \newline \texttt{interference\_detector.cpp} & Pipeline validado en \texttt{parking.nx15.dzt} \\ \hline
\end{tabular}
\caption{Cronograma de implementaci{\'o}n del subsistema de Inteligencia Artificial en \texttt{gpr3d}.}
\label{tab:cronograma_ia}
\end{table}

\vspace{0.4cm}
\begin{center}
\textbf{--- FIN DE LA GU{\'I}A T{\'E}CNICA Y PLAN DE IMPLEMENTACI{\'O}N IA 3D ---}
\end{center}

\end{document}
"""

print("Módulo de Sprints IA-4 a IA-8 definido.")

