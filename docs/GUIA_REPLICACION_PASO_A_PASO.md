# Guía Técnica Exhaustiva de Replicación: Motor Geofísico y 3D `gpr3d`
**Manual de Implementación por Sprints Atómicos en C++20 con Justificación Física y Referencias Universitarias en Alemán**

> **Documento compilado en PDF:** [`Guia_Replicacion_Paso_a_Paso_GPR3D.pdf`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/docs/Guia_Replicacion_Paso_a_Paso_GPR3D.pdf)
> **Código fuente LaTeX:** [`Guia_Replicacion_Paso_a_Paso_GPR3D.tex`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/docs/Guia_Replicacion_Paso_a_Paso_GPR3D.tex)

---

\documentclass[11pt,a4paper]{article}
\usepackage[utf8]{inputenc}
\usepackage{amsmath,amssymb}
\usepackage{color}
\usepackage{graphicx}
\usepackage{hyperref}
\usepackage{url}

% Configuración de márgenes estándar A4
\setlength{\textwidth}{16.5cm}
\setlength{\textheight}{24.0cm}
\setlength{\oddsidemargin}{0.0cm}
\setlength{\evensidemargin}{0.0cm}
\setlength{\topmargin}{-1.2cm}
\setlength{\parindent}{0pt}
\setlength{\parskip}{6pt}

% Definición de colores
\definecolor{navyblue}{rgb}{0.05, 0.15, 0.45}
\definecolor{darkgreen}{rgb}{0.08, 0.40, 0.12}
\definecolor{darkred}{rgb}{0.55, 0.10, 0.10}
\definecolor{boxbg}{rgb}{0.96, 0.97, 0.99}

\hypersetup{
    colorlinks=true,
    linkcolor=navyblue,
    citecolor=darkgreen,
    urlcolor=navyblue,
    pdftitle={Guía Técnica Exhaustiva de Replicación: Motor Geofísico y 3D gpr3d},
    pdfauthor={Proyecto SignalHandler / gpr3d}
}

\renewcommand{\labelitemi}{$\bullet$}
\renewcommand{\labelitemii}{$-$}
\renewcommand{\labelitemiii}{$\ast$}
\renewcommand{\labelitemiv}{$\cdot$}

\begin{document}

% -----------------------------------------------------------------------------
% PORTADA
% -----------------------------------------------------------------------------
\begin{center}
    {\LARGE \textbf{Guía Técnica Exhaustiva de Replicación:\\[0.2cm] Motor Geofísico e Inversión Subterránea 3D (\texttt{gpr3d})}}\\[0.4cm]
    {\Large \textbf{Manual de Tareas de Implementación en C++20, Justificación Física y Referencias Universitarias en Alemán}}\\[0.3cm]
    \rule{\textwidth}{1.2pt}\\[0.2cm]
    \textbf{Proyecto:} \texttt{SignalHandler} (\textit{gpr3d}) \quad $\bullet$ \quad \textbf{Fecha:} Noviembre 2026 \quad $\bullet$ \quad \textbf{Estándar:} C++20 estricto\\
    \rule{\textwidth}{0.6pt}
\end{center}

\vspace{0.2cm}
\tableofcontents
\vspace{0.4cm}
\hrule
\vspace{0.4cm}

\section{Visión General, Metodología y Arquitectura del Sistema}

Esta guía técnica tiene como finalidad permitir la reconstrucción completa e independiente de la suite geofísica y de procesamiento de señales \texttt{gpr3d} (\texttt{SignalHandler}). A diferencia de resúmenes teóricos superficiales, este documento detalla:
\begin{enumerate}
    \item \textbf{Por qué hay que implementarlo:} Justificación física electromagnética y computacional de cada módulo, explicando qué fallos ocurren en los datos o en memoria si se omite o diseña incorrectamente.
    \item \textbf{Mapeo académico en alemán:} Localización precisa de temas en los libros estándar \textit{Mathematik} (Tilo Arens et al.), \textit{Physik für Wissenschaftler und Ingenieure} (Tipler \& Mosca) y \textit{C++ professionell anwenden} (Ulrich Breymann).
    \item \textbf{Tareas de implementación paso a paso:} Estructuras de datos, atributos, firmas de métodos, ecuaciones en pseudocódigo y código C++20 real.
    \item \textbf{Criterios de verificación unitaria:} Casos de prueba reproducibles con entradas numéricas y tolerancias de error $\epsilon$.
\end{enumerate}

\begin{figure}[htbp]
    \centering
    \includegraphics[width=0.95\textwidth]{diagrams/pipeline_arquitectura.pdf}
    \caption{Arquitectura en capas y flujo de transformaciones del motor geofísico \texttt{gpr3d}.}
    \label{fig:pipeline_arquitectura}
\end{figure}

\begin{figure}[htbp]
    \centering
    \includegraphics[width=0.88\textwidth]{diagrams/roadmap_v2.pdf}
    \caption{Grafo de dependencias técnicas de los 13 sprints de implementación progresiva.}
    \label{fig:roadmap_v2}
\end{figure}

\subsection{Tabla Sinóptica de los 13 Sprints de Implementación}
\begin{table}[htbp]
\centering
\footnotesize
\begin{tabular}{|c|p{4.0cm}|p{3.2cm}|p{2.9cm}|p{3.2cm}|}
\hline
\textbf{Sprint} & \textbf{Módulo y Objeto Técnico} & \textbf{\textit{Mathematik} (Arens)} & \textbf{\textit{Physik} (Tipler)} & \textbf{\textit{C++20} (Breymann)} \\
\hline
\textbf{0} & Infraestructura CMake y Sanitizers & -- & -- & Kap. 1 \& 21 (Toolchain, ASan) \\
\hline
\textbf{1} & Núcleo binario, Endianness, IBM 360 & Kap. 1 \& 28 (Numerik) & Kap. 1 (Messung) & Kap. 2, 3 (\texttt{<bit>}, Bitwise) \\
\hline
\textbf{2} & Tipos de Dominio y Simulador Ricker & Kap. 1 \& 28 (Matrizen) & Kap. 15 (Wellen \& TWT) & Kap. 7 \& 8 (RAII, Move) \\
\hline
\textbf{3} & Geodesia WGS84, ECEF, ENU, UTM & Kap. 7 \& 8 (Vektoren, Matrizen) & Kap. 3 (Kinematik 3D) & Kap. 7 (\texttt{std::optional}) \\
\hline
\textbf{4} & GNSS, NMEA GGA y Sincronización & Kap. 28 (Interpolation) & Kap. 3 (Trajektorie) & Kap. 16 (Stream-Parsing) \\
\hline
\textbf{5} & Lectores DZT, SEG-Y y MALA RD3 & Kap. 1 \& 28 & Kap. 15 (TWT) & Kap. 16 (Binäre E/A) \\
\hline
\textbf{6} & FFT Cooley-Tukey y Hilbert & Kap. 18 (Fourier, Hilbert) & Kap. 15 \& 30 (Energie) & Kap. 14 (\texttt{std::complex}) \\
\hline
\textbf{7} & Pipeline DSP (Dewow, t0, BP, Fondo, SEC) & Kap. 18 (Faltung) & Kap. 15 \& 28 (Filter) & Kap. 11 (\texttt{std::span}) \\
\hline
\textbf{8} & Migración Kirchhoff 2D y Nubes 3D & Kap. 24 \& 28 & Kap. 15 (Huygens) & Kap. 7 \& 8 (Point Cloud) \\
\hline
\textbf{9} & Inversión: Semblanza, Fresnel, ASTM & Kap. 28 (Ausgleichsrechnung) & Kap. 30--32 (Optik, EM) & Kap. 13 \& 14 (\texttt{nth\_element}) \\
\hline
\textbf{10} & Mallas 3D y Triedro Frenet-Serret & Kap. 24 \& 25 (Kurven 3D) & Kap. 15 (Wellenbeugung) & Kap. 7 \& 8 (Mallas indexadas) \\
\hline
\textbf{11} & Exportadores GLB 2.0, GeoJSON, HTML & Kap. 7 (Projektionen) & Kap. 31 (PBR-Optik) & Kap. 16 (Serialización binaria) \\
\hline
\textbf{12} & CLI, Gráficos Cairo y Datos Reales & Kap. 28 & Kap. 15 & Kap. 1 (CLI \& Integración) \\
\hline
\end{tabular}
\caption{Mapeo curricular exhaustivo de los 13 sprints de implementación.}
\end{table}

% -----------------------------------------------------------------------------
% SPRINT 0
% -----------------------------------------------------------------------------
\section{Sprint 0: Infraestructura C++20, CMake, Sanitizers y Entorno de Pruebas}

\subsection{1. Fundamentos y Mapeo Bibliográfico}
\begin{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Ulrich Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 1 (\textit{Entwicklungsumgebung und Compiler-Optionen})}: Configuración del estándar C++20 (\texttt{-std=c++20}), flags de advertencia estrictas (\texttt{-Wall -Wextra -Wpedantic -Wconversion}) y estructura modular con CMake.
        \item \textbf{Kapitel 21 (\textit{Fehlersuche und Code-Qualität})}: Detección temprana de errores de punteros, desbordamientos de búfer y conversiones con signo/sin signo mediante sanitizers de LLVM/GCC: AddressSanitizer (\texttt{-fsanitize=address}) y UndefinedBehaviorSanitizer (\texttt{-fsanitize=undefined}).
    \end{itemize}
\end{itemize}

\subsection{2. Justificación de Ingeniería: ¿Por qué hay que implementarlo?}
El procesamiento de georradar involucra la lectura de millones de muestras binarias crudas desde disco hacia búferes continuos de memoria, y la ejecución de convoluciones y transformadas FFT. Un solo error de desalineación de punteros, un acceso fuera de límites (\textit{out-of-bounds index}) o una conversión implícita de entero signado a no signado provocará:
\begin{enumerate}
    \item Corrupción silenciosa de datos geofísicos (valores fantasma en el espectro o en las coordenadas).
    \item Fallos de segmentación (\textit{segmentation faults}) en tiempo de ejecución difíciles de reproducir al procesar perfiles gigantescos de más de $10\,000$ trazas.
    \item Comportamiento indefinido (\textit{Undefined Behavior}) dependiente de la optimización del compilador (\texttt{-O2} vs \texttt{-O3}).
\end{enumerate}
Configurar un entorno estricto con sanitizers en el Sprint 0 garantiza que cualquier fallo de punteros se detecte de inmediato con un mensaje exacto de línea y llamada en la traza de pila (\textit{stack trace}).

\subsection{3. Tareas Detalladas de Implementación}
\begin{itemize}
    \item \textbf{Subtarea 0.1: Configuración de \texttt{CMakeLists.txt}:}
    Configura C++20 requerido, opciones condicionales de aceleración (\texttt{GPR3D\_USE\_FFTW3}, \texttt{GPR3D\_USE\_CAIRO}, \texttt{GPR3D\_USE\_OPENGL}) y flags de compilación para modo Debug y Release:
\begin{quote}
\small
\begin{verbatim}
cmake_minimum_required(VERSION 3.20)
project(gpr3d VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Opciones de bibliotecas opcionales
option(GPR3D_USE_FFTW3 "Aceleracion espectral con FFTW3" ON)
option(GPR3D_USE_CAIRO "Renderizado grafico con Cairo" ON)
option(GPR3D_BUILD_TESTS "Compilar suite de pruebas unitarias" ON)

# Flags de advertencia estrictas y sanitizers en Debug
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined -g")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")
endif()
\end{verbatim}
\end{quote}

    \item \textbf{Subtarea 0.2: Estructura física del repositorio:}
    Crea las carpetas estándar:
    \texttt{include/gpr3d/core/}, \texttt{include/gpr3d/io/}, \texttt{include/gpr3d/dsp/}, \texttt{include/gpr3d/geo/}, \texttt{include/gpr3d/scene/}, \texttt{src/}, \texttt{apps/} y \texttt{tests/}.
    
    \item \textbf{Subtarea 0.3: Harness básico de pruebas:}
    Crea un ejecutable \texttt{tests/test\_gpr3d.cpp} enlazado a la librería con una macro sencilla de aserción numérica:
\begin{quote}
\small
\begin{verbatim}
#include <iostream>
#include <cmath>
#include <cstdlib>

#define ASSERT_NEAR(a, b, eps) \
    if (std::abs((a) - (b)) > (eps)) { \
        std::cerr << "Fallo en linea " << __LINE__ << ": " << (a) << " != " << (b) << std::endl; \
        std::exit(EXIT_FAILURE); \
    }
\end{verbatim}
\end{quote}
\end{itemize}

\subsection{4. Prueba de Verificación Unitaria}
Ejecuta \texttt{cmake -B build -DCMAKE\_BUILD\_TYPE=Debug \&\& cmake --build build}. Verifica que compila con cero advertencias y que el ejecutable de pruebas termina con código de salida 0.

% -----------------------------------------------------------------------------
% SPRINT 1
% -----------------------------------------------------------------------------
\section{Sprint 1: Núcleo Binario, Decodificación Endianness y Flotante IBM 360}

\subsection{1. Fundamentos y Mapeo Bibliográfico}
\begin{itemize}
    \item \textbf{\textit{Mathematik} (Tilo Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 1 (\textit{Mengen und Zahlen})}: Sistemas posicionales binario y hexadecimal, complemento a dos, máscaras de bits y aritmética de precisión finita.
        \item \textbf{Kapitel 28 (\textit{Numerik})}: Estructura de números en punto flotante (signo, exponente sesgado y mantisa fraccionaria), sesgo (\textit{bias}) y resolución de máquina.
    \end{itemize}
    \item \textbf{\textit{Physik} (Tipler \& Mosca)}:
    \begin{itemize}
        \item \textbf{Kapitel 1 (\textit{Physikalische Größen und Messung})}: Escalas de tiempo en nanosegundos ($1\text{ ns} = 10^{-9}\text{ s}$) y frecuencias en megahertz ($1\text{ MHz} = 10^6\text{ Hz}$).
    \end{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 2 \& 3 (\textit{Datentypen, Bit-Operationen})}: Tipos de ancho fijo (\texttt{uint8\_t}, \texttt{uint16\_t}, \texttt{uint32\_t}, \texttt{int32\_t}), operadores bitwise (\texttt{<<}, \texttt{>>}, \texttt{\&}, \texttt{|}), y la biblioteca \texttt{<bit>} (\texttt{std::endian::native}).
    \end{itemize}
\end{itemize}

\subsection{2. Justificación de Ingeniería: ¿Por qué hay que implementarlo?}
\begin{enumerate}
    \item \textbf{Heterogeneidad de Endianness en Fabricantes de Georradar:}
    Los archivos GSSI RADAN (\texttt{.dzt}) guardan enteros en \textit{Little-Endian} (byte menos significativo primero), mientras que los archivos SEG-Y clásicos provenientes de cintas sísmicas o georradar industrial están en \textit{Big-Endian} (byte más significativo primero). Si un desarrollador intenta leer un entero de 16 bits directamente con \texttt{*(uint16\_t*)ptr}, el código fallará en procesadores con convención contraria o causará una violación de alineación (\textit{unaligned access}) en CPUs ARM.
    \item \textbf{Incompatibilidad de Punto Flotante IBM 360 frente a IEEE 754:}
    El estándar SEG-Y histórico almacena amplitudes en flotantes de IBM System/360. El formato IBM utiliza \textbf{base 16} (hexadecimal) con un sesgo de 64 en el exponente:
    \begin{equation}
        x = (-1)^s \cdot 16^{c - 64} \cdot \left(\sum_{i=1}^{24} b_{24-i} 2^{-i}\right)
    \end{equation}
    mientras que el estándar IEEE 754 moderno utiliza base 2 con sesgo 127 y un bit 1 implícito en la mantisa. Interpretar directamente los bits de un flotante IBM como flotante IEEE produce amplitudes erróneas de magnitudes absurdas ($\approx 10^{35}$) o valores \texttt{NaN}, destruyendo por completo cualquier cálculo posterior de energía o reflectancia.
\end{enumerate}

\subsection{3. Tareas Detalladas de Implementación}
Crea el archivo \texttt{include/gpr3d/core/endian.hpp}:
\begin{itemize}
    \item \textbf{Subtarea 1.1: Lectura segura de enteros Little-Endian y Big-Endian:}
\begin{quote}
\small
\begin{verbatim}
namespace gpr3d::detail {
    inline uint16_t read_u16_le(const uint8_t* p) noexcept {
        return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
    }
    inline uint32_t read_u32_le(const uint8_t* p) noexcept {
        return static_cast<uint32_t>(p[0]) |
               (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[3]) << 24);
    }
    inline int32_t read_i32_le(const uint8_t* p) noexcept {
        return static_cast<int32_t>(read_u32_le(p));
    }
    inline float read_f32_le(const uint8_t* p) noexcept {
        uint32_t u = read_u32_le(p);
        float f;
        std::memcpy(&f, &u, sizeof(float));
        return f;
    }
    inline uint16_t read_u16_be(const uint8_t* p) noexcept {
        return static_cast<uint16_t>((p[0] << 8) | p[1]);
    }
    inline uint32_t read_u32_be(const uint8_t* p) noexcept {
        return (static_cast<uint32_t>(p[0]) << 24) |
               (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8) |
               static_cast<uint32_t>(p[3]);
    }
    inline int32_t read_i32_be(const uint8_t* p) noexcept {
        return static_cast<int32_t>(read_u32_be(p));
    }
    inline float read_f32_be(const uint8_t* p) noexcept {
        uint32_t u = read_u32_be(p);
        float f;
        std::memcpy(&f, &u, sizeof(float));
        return f;
    }
}
\end{verbatim}
\end{quote}

    \item \textbf{Subtarea 1.2: Decodificador IBM 360 Float a IEEE 754:}
\begin{quote}
\small
\begin{verbatim}
inline double ibm_to_double(uint32_t ibm_bits) noexcept {
    if ((ibm_bits & 0x7FFFFFFF) == 0) return 0.0;
    const int sign = (ibm_bits & 0x80000000) ? -1 : 1;
    const int exponent = static_cast<int>((ibm_bits >> 24) & 0x7F) - 64;
    const uint32_t fraction = ibm_bits & 0x00FFFFFF;
    const double frac_val = static_cast<double>(fraction) / 16777216.0; // 2^24

    double factor = 1.0;
    if (exponent >= 0) {
        for (int i = 0; i < exponent; ++i) factor *= 16.0;
    } else {
        for (int i = 0; i < -exponent; ++i) factor /= 16.0;
    }
    return sign * frac_val * factor;
}
\end{verbatim}
\end{quote}

    \item \textbf{Subtarea 1.3: Escritura de enteros en Little/Big Endian:}
    Implementa \texttt{write\_u16\_le}, \texttt{write\_u32\_le}, \texttt{write\_u16\_be}, \texttt{write\_u32\_be} para permitir la exportación de archivos glTF y DZT sintetizados.
\end{itemize}

\subsection{4. Prueba de Verificación Unitaria}
En \texttt{tests/test\_endian.cpp}:
\begin{enumerate}
    \item \texttt{read\_u16\_le(\{0x34, 0x12\})} debe ser exactamente $4660$ (\texttt{0x1234}).
    \item \texttt{read\_u16\_be(\{0x12, 0x34\})} debe ser exactamente $4660$ (\texttt{0x1234}).
    \item \texttt{ibm\_to\_double(0x41100000)} debe retornar exactamente $1.0$ (signo $+$, exp $65-64=1$, fracc $1/16$, valor $16^1 \times 1/16 = 1.0$).
    \item \texttt{ibm\_to\_double(0xC2760000)} debe retornar exactamente $-118.0$.
\end{enumerate}

% -----------------------------------------------------------------------------
% SPRINT 2
% -----------------------------------------------------------------------------
\section{Sprint 2: Tipos de Dominio Geofísico y Simulador Ricker Sintético}

\subsection{1. Fundamentos y Mapeo Bibliográfico}
\begin{itemize}
    \item \textbf{\textit{Mathematik} (Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 18 (\textit{Fourier-Transformation})}: Función gaussiana y sus derivadas temporales.
        \item \textbf{Kapitel 28 (\textit{Numerik})}: Muestreo discreto temporal $t_j = j \cdot \Delta t$, discretización espacial de perfiles $x_i = i \cdot \Delta x$.
    \end{itemize}
    \item \textbf{\textit{Physik} (Tipler \& Mosca)}:
    \begin{itemize}
        \item \textbf{Kapitel 15 (\textit{Wellen})}: Propagación de pulsos, tiempo de viaje en dos sentidos (TWT) $t = 2z/v$, principio de superposición lineal y velocidad electromagnética $v = c_0 / \sqrt{\varepsilon_r}$.
    \end{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 7 \& 8 (\textit{Klassen, Move-Semantik})}: Objetos semánticos RAII, prevención de copias innecesarias con constructores por movimiento (\texttt{noexcept}), reserva previa de capacidad con \texttt{std::vector::reserve}.
    \end{itemize}
\end{itemize}

\subsection{2. Justificación de Ingeniería: ¿Por qué hay que implementarlo?}
\begin{enumerate}
    \item \textbf{Abstracción Agnóstica del Formato:} La lógica de procesamiento de señales (Dewow, Kirchhoff, Semblanza) no debe saber si los datos provinieron de un radar GSSI, MALA o SEG-Y. Se requiere una representación canónica en memoria (\texttt{BScan}) que contenga trazas verticales (\texttt{AScan}) y metadatos unificados.
    \item \textbf{Indispensabilidad de un Generador Sintético para Verificación:} En geofísica de campo, los datos reales contienen ruido aleatorio, acoplamientos y geometrías desconocidas a priori. Para demostrar rigurosamente que los algoritmos de migración e inversión dieléctrica funcionan, es obligatorio disponer de un generador analítico sintético que cree radargramas controlados con ondículas de Ricker:
    \begin{equation}
        \psi(t) = \left(1 - 2\pi^2 f_0^2 (t - t_0)^2\right) \exp\left(-\pi^2 f_0^2 (t - t_0)^2\right)
    \end{equation}
    donde se conocen exactamente las coordenadas del reflector $(x_0, z_0)$ y la permitividad del estrato $\varepsilon_r$.
\end{enumerate}

\subsection{3. Tareas Detalladas de Implementación}
\begin{itemize}
    \item \textbf{Subtarea 2.1: Definición de \texttt{include/gpr3d/core/types.hpp}:}
    Declara las constantes físicas fundamentales y los contenedores de datos:
\begin{quote}
\small
\begin{verbatim}
namespace gpr3d {
    inline constexpr double kSpeedOfLightMPerNs = 0.299792458; // c0 en m/ns

    struct AScan {
        std::size_t trace_id{0};
        double distance_along_track_m{0.0};
        std::optional<double> timestamp_sec;
        std::optional<Wgs84Coord> position_wgs84;
        std::optional<EnuCoord> position_enu;
        double heading_deg{0.0};
        std::vector<double> samples;
        std::vector<double> envelope;
    };

    struct SurveyMetadata {
        std::string source_format{"UNKNOWN"};
        double sample_interval_ns{0.1};
        double time_window_ns{50.0};
        double time_zero_ns{0.0};
        std::size_t samples_per_trace{0};
        double trace_spacing_m{0.02};
        double scans_per_second{50.0};
        double antenna_freq_mhz{400.0};
        double relative_permittivity{9.0};
    };

    struct BScan {
        SurveyMetadata metadata{};
        std::vector<AScan> traces{};
        [[nodiscard]] std::size_t num_traces() const noexcept { return traces.size(); }
        [[nodiscard]] std::size_t num_samples() const noexcept {
            return traces.empty() ? metadata.samples_per_trace : traces.front().samples.size();
        }
        [[nodiscard]] bool empty() const noexcept { return traces.empty(); }
    };
}
\end{verbatim}
\end{quote}

    \item \textbf{Subtarea 2.2: Implementación de \texttt{SyntheticGenerator}:}
    En \texttt{src/sim/synthetic\_generator.cpp}, implementa la generación de hipérbolas difractivas:
    Para cada traza en $x$, calcula la distancia euclidiana al blanco $R = \sqrt{(x - x_0)^2 + z_0^2}$, el tiempo doble $t_{\text{twt}} = 2R / v$, y evalúa la ondícula de Ricker sobre la malla de muestras temporales.
\end{itemize}

\subsection{4. Prueba de Verificación Unitaria}
Genera una encuesta sintética con $100$ trazas, $\Delta x = 0.05\text{ m}$, $\Delta t = 0.1\text{ ns}$, $\varepsilon_r = 9.0$ ($v = 0.10\text{ m/ns}$) y un conducto ubicado en $x = 2.5\text{ m}$, $z = 0.8\text{ m}$.
Comprueba que en la traza central ($x = 2.5\text{ m}$), el pico máximo de amplitud se ubica exactamente en la muestra correspondiente a $t_0 = 2 \times 0.8 / 0.10 = 16.0\text{ ns}$ (muestra $j = 160$).

% -----------------------------------------------------------------------------
% SPRINT 3
% -----------------------------------------------------------------------------
\section{Sprint 3: Geodesia Espacial (WGS84, ECEF, ENU, UTM y Lever-Arm)}

\subsection{1. Fundamentos y Mapeo Bibliográfico}
\begin{itemize}
    \item \textbf{\textit{Mathematik} (Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 7 \& 8 (\textit{Vektoren und Matrizen})}: Coordenadas cartesianas tridimensionales, producto cruz, matrices de rotación ortogonales ($R^{-1} = R^T$, $\det R = 1$) y cambio de sistema de coordenadas.
    \end{itemize}
    \item \textbf{\textit{Physik} (Tipler \& Mosca)}:
    \begin{itemize}
        \item \textbf{Kapitel 3 (\textit{Bewegung in 3D})}: Cinemática tridimensional, traslación de marcos de referencia y vectores de posición en marcos inerciales vs. móviles.
    \end{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 7 (\textit{Klassen})}: Estructuras de datos geométricas simples, métodos \texttt{noexcept} y constantes de \texttt{<numbers>} (\texttt{std::numbers::pi}).
    \end{itemize}
\end{itemize}

\subsection{2. Justificación de Ingeniería: ¿Por qué hay que implementarlo?}
\begin{enumerate}
    \item \textbf{No-Euclidianidad de Coordenadas Lat/Lon:}
    Las coordenadas GNSS angulares en grados $(\varphi, \lambda)$ son esféricas/elipsoidales y distorsionan las distancias métricas según la latitud geográfica. A $50^\circ\text{ N}$, $1^\circ$ de longitud equivale a $\approx 71\text{ km}$, mientras que $1^\circ$ de latitud equivale a $\approx 111\text{ km}$. Intentar calcular mallas 3D tubulares o distancias entre trazas directamente en grados produce cilindros aplastados y cálculos de velocidad completamente erróneos.
    \item \textbf{Necesidad de Coordenadas Métricas Locales ENU:}
    Los motores 3D (WebGL, Three.js, glTF) y los algoritmos de migración necesitan coordenadas métricas ortonormales euclidianas. El sistema plano tangente local ENU (East-North-Up) proporciona un origen métrico $(0,0,0)$ donde el eje X es Este, el eje Y es Norte y el eje Z es la elevación ortométrica vertical.
    \item \textbf{Corrección de Brazo de Palanca (\textit{Lever-Arm}):}
    En carritos de georradar, el receptor GPS se instala en un mástil elevado, desplazado longitudinalmente ($\Delta x_{\text{fwd}}$) y transversalmente ($\Delta y_{\text{right}}$) respecto a la antena de radar sobre el suelo. Si no se compensa este desplazamiento con el rumbo instantáneo ($\psi$), los perfiles adquiridos en pasadas de ida y vuelta quedarán desalineados hasta $2\text{ metros}$ entre sí.
\end{enumerate}

\begin{figure}[htbp]
    \centering
    \includegraphics[width=0.92\textwidth]{diagrams/geodesia_pipeline.pdf}
    \caption{Cadena de transformaciones geodésicas y corrección de brazo de palanca (Lever-Arm).}
    \label{fig:geodesia_pipeline_s3}
\end{figure}

\subsection{3. Tareas Detalladas de Implementación}
En \texttt{include/gpr3d/geo/crs\_transform.hpp} y \texttt{src/geo/crs\_transform.cpp}:
\begin{itemize}
    \item \textbf{Subtarea 3.1: Modelado de estructuras geodésicas:}
\begin{quote}
\small
\begin{verbatim}
struct Wgs84Coord { double latitude_deg{0.0}; double longitude_deg{0.0}; double altitude_m{0.0}; };
struct EcefCoord { double x_m{0.0}; double y_m{0.0}; double z_m{0.0}; };
struct EnuCoord { double east_m{0.0}; double north_m{0.0}; double up_m{0.0}; };
struct UtmCoord {
    double easting_m{500000.0};
    double northing_m{0.0};
    double elevation_m{0.0};
    int zone{32};
    bool is_northern{true};
};
\end{verbatim}
\end{quote}

    \item \textbf{Subtarea 3.2: Fórmulas del elipsoide WGS84 a ECEF:}
    Con $a = 6378137.0\text{ m}$, achatamiento $f = 1/298.257223563$, y primera excentricidad al cuadrado $e^2 = 2f - f^2$:
    \begin{equation}
        N(\varphi) = \frac{a}{\sqrt{1 - e^2 \sin^2\varphi}}
    \end{equation}
    \begin{equation}
        X = (N + h)\cos\varphi\cos\lambda, \quad Y = (N + h)\cos\varphi\sin\lambda, \quad Z = (N(1 - e^2) + h)\sin\varphi
    \end{equation}

    \item \textbf{Subtarea 3.3: ECEF a ENU local centrada en $(\varphi_0, \lambda_0, h_0)$:}
    Multiplicación matricial ortonormal:
    \begin{equation}
        \begin{bmatrix} E \\ N \\ U \end{bmatrix} =
        \begin{bmatrix}
        -\sin\lambda_0 & \cos\lambda_0 & 0 \\
        -\sin\varphi_0\cos\lambda_0 & -\sin\varphi_0\sin\lambda_0 & \cos\varphi_0 \\
        \cos\varphi_0\cos\lambda_0 & \cos\varphi_0\sin\lambda_0 & \sin\varphi_0
        \end{bmatrix}
        \begin{bmatrix} X - X_0 \\ Y - Y_0 \\ Z - Z_0 \end{bmatrix}
    \end{equation}

    \item \textbf{Subtarea 3.4: Proyección UTM de sexto orden de Krüger:}
    Implementa \texttt{wgs84\_to\_utm} para determinar automáticamente el huso UTM ($1..60$), el meridiano central $\lambda_0 = (6 \cdot \text{zone} - 183)^\circ$ y el falso Este ($500\,000\text{ m}$).

    \item \textbf{Subtarea 3.5: Corrección de brazo de palanca (\textit{Lever-Arm}):}
    \begin{equation}
        E_{\text{ant}} = E_{\text{gps}} + \Delta x \sin\psi + \Delta y \cos\psi, \qquad
        N_{\text{ant}} = N_{\text{gps}} + \Delta x \cos\psi - \Delta y \sin\psi
    \end{equation}
\end{itemize}

\subsection{4. Prueba de Verificación Unitaria}
Verifica las coordenadas del punto de referencia geodésico de Berlín:
$\varphi = 52.520008^\circ$, $\lambda = 13.404954^\circ$, $h = 34.5\text{ m}$.
\begin{enumerate}
    \item Al convertir a ECEF y retornar a WGS84 (\textit{round-trip}), la discrepancia en latitud y longitud debe ser inferior a $10^{-9}$ grados ($< 0.1\text{ mm}$ de error residual).
    \item Al calcular la proyección UTM, el huso debe ser exactamente \textbf{33N}, con $E \approx 391777.3\text{ m}$ y $N \approx 5820074.2\text{ m}$.
\end{enumerate}

% -----------------------------------------------------------------------------
% SPRINT 4
% -----------------------------------------------------------------------------
\section{Sprint 4: Receptores GNSS, NMEA-0183 (NMEA GGA) e Interpolación de Trayectoria}

\subsection{1. Fundamentos y Mapeo Bibliográfico}
\begin{itemize}
    \item \textbf{\textit{Mathematik} (Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 24 (\textit{Kurven im Raum})}: Longitud de arco acumulada $s_i = \sum_{k=1}^i \|\mathbf{x}_k - \mathbf{x}_{k-1}\|$, cálculo de rumbo tangencial continuo y curvatura planar.
        \item \textbf{Kapitel 28 (\textit{Numerik})}: Interpolación monótona de coordenadas espaciales frente a sellos de tiempo discretos.
    \end{itemize}
    \item \textbf{\textit{Physik} (Tipler \& Mosca)}:
    \begin{itemize}
        \item \textbf{Kapitel 3 (\textit{Kinematik})}: Estimación de velocidad instantánea de avance del carrito $v_{\text{cart}} = \Delta s / \Delta t$ y orientación azimutal respecto al Norte verdadero.
    \end{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 16 (\textit{String- und Stream-Verarbeitung})}: Tokenización eficiente de cadenas NMEA delimitadas por comas (\texttt{std::string\_view}, \texttt{std::stod}) y manejo de excepciones en conversiones numéricas.
    \end{itemize}
\end{itemize}

\subsection{2. Justificación de Ingeniería: ¿Por qué hay que implementarlo?}
\begin{enumerate}
    \item \textbf{Desincronización de Frecuencias de Muestreo GPR vs. GNSS:}
    Un radar de subsuelo adquiere pulsos a altas frecuencias (por ejemplo, $50\text{ a }100\text{ scans/segundo}$ o mediante una rueda odómetro con $1\text{ scan cada }2\text{ cm}$), mientras que un receptor GPS/GNSS entrega sentencias NMEA a $1\text{ Hz}$ o como máximo $10\text{ Hz}$. Esto significa que por cada posición GPS recibida se generan decenas o cientos de trazas de radar. Si no se construye un interpolador espacial robusto por longitud de arco, el $90\%$ de las trazas del radargrama quedarían huérfanas sin georreferenciación.
    \item \textbf{Peculiaridades de Archivos de Marcas (.dzg):}
    Equipos como GSSI RADAN almacenan las trazas en un archivo binario \texttt{.dzt} y las sentencias GNSS en un archivo de texto adjunto \texttt{.dzg}. El archivo \texttt{.dzg} intercala etiquetas propietarias como \texttt{GSSIS,1420} (indicando que la siguiente sentencia GPS corresponde exactamente a la traza número 1420). Un parser estricto debe extraer estas marcas para sincronizar exactamente la cinemática del levantamiento.
\end{enumerate}

\subsection{3. Tareas Detalladas de Implementación}
En \texttt{include/gpr3d/io/gps\_reader.hpp} y \texttt{include/gpr3d/geo/trajectory\_sync.hpp}:
\begin{itemize}
    \item \textbf{Subtarea 4.1: Parser de tramas NMEA-0183 GPGGA / GNGGA:}
    Extrae la hora UTC, latitud (formato \texttt{ddmm.mmmm} con hemisferio N/S), longitud (formato \texttt{dddmm.mmmm} con hemisferio E/W), calidad de fix ($1=\text{GPS}$, $2=\text{DGPS}$, $4=\text{RTK Fixed}$, $5=\text{RTK Float}$) y elevación sobre el geoide:
\begin{quote}
\small
\begin{verbatim}
inline double parse_nmea_coord(const std::string& val, char hem) {
    if (val.empty()) return 0.0;
    double raw = std::stod(val);
    double deg = std::floor(raw / 100.0);
    double min = raw - (deg * 100.0);
    double res = deg + (min / 60.0);
    if (hem == 'S' || hem == 'W') res = -res;
    return res;
}
\end{verbatim}
\end{quote}

    \item \textbf{Subtarea 4.2: Sincronización de trayectoria \texttt{synchronize\_trajectory}:}
    Itera sobre todas las trazas del \texttt{BScan}:
    \begin{enumerate}
        \item Si existen fijaciones GNSS con índice de traza explícito (\texttt{trace\_index}), realiza una interpolación lineal métrica entre pares consecutivos.
        \item Si no hay índices de traza pero sí marcas de tiempo, interpola por tiempo acumulado.
        \item Si solo hay distancia odrométrica, distribuye los puntos GNSS proporcionalmente a lo largo de la longitud de la trayectoria.
        \item Para cada traza interpolada, calcula el rumbo instantáneo del avance:
        \begin{equation}
            \psi_i = \operatorname{atan2}(E_{i+1} - E_{i-1}, \, N_{i+1} - N_{i-1})
        \end{equation}
        y aplica la corrección Lever-Arm de la antena.
    \end{enumerate}
\end{itemize}

\subsection{4. Prueba de Verificación Unitaria}
Configura 3 puntos GNSS:
$P_0 = (52.0^\circ, 13.0^\circ)$ en traza 0,
$P_1 = (52.001^\circ, 13.0^\circ)$ en traza 500,
$P_2 = (52.002^\circ, 13.0^\circ)$ en traza 1000.
Verifica que la traza intermedia 250 tiene latitud exactamente igual a $52.0005^\circ \pm 10^{-7\circ}$ y rumbo Norte constante ($0^\circ$).

% -----------------------------------------------------------------------------
% SPRINT 5
% -----------------------------------------------------------------------------
\section{Sprint 5: Decodificadores I/O de Georradar (GSSI DZT, SEG-Y y MALA RD3)}

\subsection{1. Fundamentos y Mapeo Bibliográfico}
\begin{itemize}
    \item \textbf{\textit{Mathematik} (Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 1 \& 28}: Lectura matricial continua $M \times N$, alineación de bytes y mapeo de memoria sin copias (\textit{zero-copy}).
    \end{itemize}
    \item \textbf{\textit{Physik} (Tipler \& Mosca)}:
    \begin{itemize}
        \item \textbf{Kapitel 15 (\textit{Wellen})}: Tiempo doble de viaje TWT, resolución temporal $\Delta t = T_{\text{range}} / N_{\text{samp}}$ y frecuencia de corte de Nyquist $f_{\text{Nyq}} = 1 / (2\Delta t)$.
    \end{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 16 (\textit{Binäre Dateiverarbeitung})}: Uso de \texttt{std::ifstream(path, std::ios::binary | std::ios::ate)}, validación de tamaño mínimo de archivo y patrones de fábrica polimórficos (\texttt{std::unique\_ptr<RadarReader>}).
    \end{itemize}
\end{itemize}

\subsection{2. Justificación de Ingeniería: ¿Por qué hay que implementarlo?}
\begin{enumerate}
    \item \textbf{Soporte de Formatos Propietarios Industriales:}
    Cada fabricante de GPR tiene un estándar cerrado. GSSI RADAN utiliza una cabecera binaria de $1024\text{ bytes}$ que puede extenderse a múltiplos de 1024 si se trata de un equipo multicanal (\texttt{rh\_data}). La profundidad de palabra puede ser de 8 bits (firmas de $0..255$), 16 bits sin signo (con sesgo en 32768) o 32 bits enteros signados. SEG-Y utiliza una cabecera textual EBCDIC de 3200 bytes, una cabecera binaria de 400 bytes y cabeceras de traza de 240 bytes con coordenadas escaladas. Si el software no normaliza todos los formatos a un \texttt{BScan} con amplitudes en flotante real centradas en cero, ningún filtro matemático posterior funcionará.
    \item \textbf{Filtrado de Marcadores de Hardware en Muestras 0 y 1:}
    Los digitalizadores de GSSI (SIR-20, SIR-3000) graban el contador secuencial de scans en la muestra temporal $j=0$, y en la muestra $j=1$ graban banderas de hardware (como \texttt{0xE4000000} cuando el operador presiona el botón de marca). Si estas dos muestras no se sustituyen por la amplitud temprana continua ($j=2$), representan un impulso artificial gigantesco que genera artefactos severos de dispersión en la FFT.
\end{enumerate}

\subsection{3. Tareas Detalladas de Implementación}
En \texttt{include/gpr3d/io/dzt\_reader.hpp}, \texttt{segy\_reader.hpp} y \texttt{reader.hpp}:
\begin{itemize}
    \item \textbf{Subtarea 5.1: Clase base abstracta y Factory Dispatcher:}
\begin{quote}
\small
\begin{verbatim}
class RadarReader {
public:
    virtual ~RadarReader() = default;
    [[nodiscard]] virtual BScan read(const std::filesystem::path& path,
                                    const ReadOptions& options = {}) const = 0;
};

BScan read_radar_file(const std::filesystem::path& path, const ReadOptions& options = {});
\end{verbatim}
\end{quote}

    \item \textbf{Subtarea 5.2: Lector de GSSI RADAN (\texttt{DztReader}):}
    \begin{enumerate}
        \item Lee los 1024 bytes de cabecera.
        \item Extrae \texttt{rh\_data} (offset a muestras), \texttt{rh\_nsamp} (muestras por traza), \texttt{rh\_bits} (8, 16 o 32), \texttt{rhf\_range} (ventana temporal ns), y \texttt{rhf\_spm} (scans por metro).
        \item Detecta el número de canales \texttt{rh\_nchan} para desempaquetar canales entrelazados.
        \item Itera sobre cada traza y normaliza según \texttt{rh\_bits}:
        \begin{equation}
            s_8 = \text{u8} - 128.0, \quad s_{16} = \text{u16\_le} - 32768.0, \quad s_{32} = \text{i32\_le}
        \end{equation}
        \item Reemplaza muestras 0 y 1 si superan $15 \times$ la amplitud temprana del pulso.
    \end{enumerate}

    \item \textbf{Subtarea 5.3: Lector de SEG-Y Industrial (\texttt{SegyReader}):}
    Lee la cabecera binaria de 400 bytes (offset 3200), extrae el código de formato de datos (código 1: IBM 360 float, código 5: IEEE 754 float, código 3: entero de 16 bits). Decodifica cada cabecera de traza de 240 bytes aplicando el factor de escala de coordenadas (bytes 68--70) a las posiciones X/Y (bytes 72--88).
\end{itemize}

\subsection{4. Prueba de Verificación Unitaria}
Abre el archivo de prueba real del repositorio \texttt{parking.nx15.dzt}:
\begin{enumerate}
    \item Comprueba que no lanza excepciones.
    \item Verifica que \texttt{bscan.num\_traces() > 1000} y \texttt{bscan.num\_samples() == 1024}.
    \item Verifica que $\Delta t = T_{\text{range}} / N_{\text{samp}} > 0.0\text{ ns}$ (típicamente entre $0.02\text{ ns}$ y $0.05\text{ ns}$).
\end{enumerate}

% -----------------------------------------------------------------------------
% SPRINT 6
% -----------------------------------------------------------------------------
\section{Sprint 6: Núcleo Espectral FFT y Transformada Analítica de Hilbert}

\subsection{1. Fundamentos y Mapeo Bibliográfico}
\begin{itemize}
    \item \textbf{\textit{Mathematik} (Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 15 \& 16 (\textit{Komplexe Zahlen})}: Álgebra en el plano complejo $\mathbb{C}$, módulo $|z| = \sqrt{a^2 + b^2}$, argumento $\operatorname{atan2}(b, a)$ y raíces de la unidad $e^{-j 2\pi / N}$.
        \item \textbf{Kapitel 18 (\textit{Fourier-Transformationen})}: Algoritmo rápido de Fourier (FFT), respuesta impulsiva del filtro de Hilbert $h(t) = 1/(\pi t)$, y operador de fase en cuadratura $H(f) = -j\operatorname{sgn}(f)$.
    \end{itemize}
    \item \textbf{\textit{Physik} (Tipler \& Mosca)}:
    \begin{itemize}
        \item \textbf{Kapitel 15 \& 30 (\textit{Wellen und Energie})}: Densidad de energía instantánea de la onda electromagnética proporcional al cuadrado de la envolvente $A(t)^2$, e independencia respecto a la fase instantánea.
    \end{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 14 (\textit{STL-Mathematik})}: Manejo de números complejos con \texttt{std::complex<double>}, y envoltura thread-safe con \texttt{std::mutex} para bibliotecas nativas de C (FFTW3).
    \end{itemize}
\end{itemize}

\subsection{2. Justificación de Ingeniería: ¿Por qué hay que implementarlo?}
\begin{enumerate}
    \item \textbf{La Ilusión de los Cruces por Cero en la Señal Cruda:}
    Las ondas de radar son pulsos bipolares o tripolares oscilantes con frecuencias dominantes de cientos de megahertz. Entre una cresta positiva y un valle negativo existe un cruce por cero donde la amplitud momentánea es cero. Si un algoritmo de detección busca reflectores en la señal cruda, interpretará erróneamente que en el cruce por cero no hay ningún objeto enterrado.
    \item \textbf{Inversión de Polaridad de Fase en Metales frente a Plásticos:}
    Cuando el radar choca contra un tubo de acero o un cable metálico (conductor eléctrico perfecto, PEC), el coeficiente de reflexión es $R \approx -1$, invirtiendo la fase $180^\circ$ (el primer pico es negativo). Cuando choca contra un tubo de polietileno (HDPE) vacío o una cavidad de aire, la onda no se desfasa ($R > 0$, pico positivo). La \textbf{envolvente instantánea de Hilbert}:
    \begin{equation}
        A(t) = |z(t)| = \sqrt{s(t)^2 + \tilde{s}(t)^2}
    \end{equation}
    es estrictamente positiva en todos los puntos y representa la energía electromagnética real del reflector sin depender de su polaridad de fase.
\end{enumerate}

\subsection{3. Tareas Detalladas de Implementación}
En \texttt{include/gpr3d/dsp/fft.hpp} y \texttt{src/dsp/fft.cpp}:
\begin{itemize}
    \item \textbf{Subtarea 6.1: Algoritmo Radix-2 Cooley-Tukey FFT in-place autónomo:}
    Permite compilar el software sin depender obligatoriamente de librerías externas.
    Implementa la permutación de inversión de bits (\textit{bit-reversal permutation}) seguida de los bucles mariposa (\textit{butterfly stages}) con factores de giro (\textit{twiddle factors}) $e^{\pm j 2\pi / \text{len}}$.
    
    \item \textbf{Subtarea 6.2: Wrapper acelerado con FFTW3:}
    Si se detecta la macro \texttt{GPR3D\_USE\_FFTW3}, ejecuta planes \texttt{fftw\_plan\_dft\_1d} protegidos por un \texttt{std::mutex} estático para garantizar seguridad multihilo (\textit{thread safety}).

    \item \textbf{Subtarea 6.3: Operador Analítico de Marple (1999) en Frecuencia:}
    Para una traza real de longitud $N$:
    \begin{enumerate}
        \item Calcula la FFT hacia adelante: $S = \text{fft\_forward}(s)$.
        \item Multiplica por el operador discreto:
        \begin{equation}
            H_k = \begin{cases} 1 & \text{si } k = 0 \text{ o } k = N/2 \\ 2 & \text{si } 1 \le k < N/2 \\ 0 & \text{si } N/2 < k < N \end{cases}
        \end{equation}
        \item Calcula la IFFT inversa: $z = \text{fft\_inverse}(S')$.
        \item La parte real de $z(t)$ es la señal original $s(t)$, y la parte imaginaria es su transformada de Hilbert $\tilde{s}(t)$.
        \item La envolvente es $A(t) = |z(t)|$ y la fase instantánea es $\phi(t) = \operatorname{atan2}(\operatorname{Im}(z), \operatorname{Re}(z))$.
    \end{enumerate}
\end{itemize}

\subsection{4. Prueba de Verificación Unitaria}
Evalúa la señal analítica teórica:
$s(t) = 3.5 \cos(2\pi \cdot 200\text{ MHz} \cdot t)$.
Su envolvente de Hilbert debe ser idénticamente igual a $3.5$ constante. Comprueba que para todas las muestras lejos de los bordes ($10\% < j < 90\%$), $|A_j - 3.5| < 10^{-5}$.

% -----------------------------------------------------------------------------
% SPRINT 7
% -----------------------------------------------------------------------------
\section{Sprint 7: Pipeline de Acondicionamiento DSP (Dewow, t0, BP, Fondo, SEC)}

\subsection{1. Fundamentos y Mapeo Bibliográfico}
\begin{itemize}
    \item \textbf{\textit{Mathematik} (Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 18 (\textit{Fourier-Transformationen})}: Teorema de convolución temporal, filtrado espectral y aliasing por convolución circular en la DFT.
    \end{itemize}
    \item \textbf{\textit{Physik} (Tipler \& Mosca)}:
    \begin{itemize}
        \item \textbf{Kapitel 15 \& 28 (\textit{Wechselströme und Filter})}: Deriva inductiva de baja frecuencia (\textit{wow}), alineación del acoplamiento dieléctrico aire-suelo y dispersión de energía geométrica y disipativa.
    \end{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 11 (\textit{std::span})}: Vistas continuas de memoria sin copias ni asignaciones dinámicas repetitivas.
    \end{itemize}
\end{itemize}

\subsection{2. Justificación de Ingeniería: ¿Por qué hay que implementarlo?}
El orden de las operaciones en el pipeline de georradar no es arbitrario; está gobernado estrictamente por la física del instrumento y las propiedades de la transformada de Fourier (ver Figura~\ref{fig:dsp_orden}):

\begin{figure}[htbp]
    \centering
    \includegraphics[width=0.95\textwidth]{diagrams/dsp_orden_pipeline.pdf}
    \caption{Orden obligatorio del pipeline de procesamiento DSP y bifurcación de la rama de inversión.}
    \label{fig:dsp_orden}
\end{figure}

\begin{enumerate}
    \item \textbf{¿Por qué Dewow debe ir ANTES de Time-Zero?:}
    El acoplamiento electromagnético inmediato entre las antenas transmisora y receptora satura el circuito y crea una deriva de baja frecuencia (\textit{inductive wow}) que desplaza la línea base. Si no se sustrae la media móvil en ventana corta, la detección del primer arribo de superficie (cruce por umbral del 15\% del pico) se disparará prematuramente en el ruido continuo, corrompiendo la referencia de cota cero del terreno.
    \item \textbf{¿Por qué Time-Zero debe usar un desvanecimiento coseno (\textit{Cosine-Taper})?:}
    Al alinear el terreno en $t=0\text{ ns}$, todas las muestras anteriores al primer arribo se descartan y la traza se rellena con ceros al final. Si la traza termina bruscamente en un valor no nulo, se genera un escalón artificial (\textit{Heaviside step}). Al aplicar la FFT en el siguiente paso, este escalón introduce altas frecuencias falsas (fenómeno de Gibbs) que contaminan el radargrama. Un desvanecimiento coseno de 16 muestras en la cola elimina este salto suavemente.
    \item \textbf{¿Por qué es indispensable el Zero-Padding al Doble ($2N$) en el Filtro Butterworth?:}
    El filtrado espectral mediante multiplicación en frecuencia $S(f) \cdot G(f)$ equivale a una \textbf{convolución circular} periódica. Si no se rellena con ceros hasta al menos el doble del tamaño de la traza ($2N$), la enorme energía de la onda directa de superficie en $t=0$ se "envuelve" circularmente y reaparece al final de la traza ($t = t_{\text{max}}$), simulando un reflector profundo inexistente (ver Figura~\ref{fig:porque_zero_padding}).
    \item \textbf{¿Por qué la Ganancia SEC debe ir al FINAL y mantenerse fuera de la Inversión?:}
    La compensación esférica y exponencial SEC:
    \begin{equation}
        g(t) = (1 + \beta t) \cdot \exp(\alpha t)
    \end{equation}
    amplifica las señales profundas miles de veces. Si se aplica antes de los filtros, amplificará el ruido de fondo y la deriva DC haciendo imposible su eliminación. Además, la rama de inversión geofísica (Sprint 9) debe trabajar obligatoriamente con trazas \textbf{sin ganancia} para poder medir coeficientes de reflexión físicos $R$ reales.
\end{enumerate}

\begin{figure}[htbp]
    \centering
    \includegraphics[width=0.88\textwidth]{diagrams/porque_zero_padding.pdf}
    \caption{Comparación entre convolución circular (sin padding) y lineal (con padding $2N$).}
    \label{fig:porque_zero_padding}
\end{figure}

\begin{figure}[htbp]
    \centering
    \includegraphics[width=0.88\textwidth]{diagrams/porque_dsp_traza.pdf}
    \caption{Transformación progresiva de una traza sintética a lo largo de las 4 etapas del pipeline DSP.}
    \label{fig:porque_dsp_traza}
\end{figure}

\subsection{3. Tareas Detalladas de Implementación}
En \texttt{src/dsp/signal\_processor.cpp}:
\begin{itemize}
    \item \textbf{Subtarea 7.1: Dewow con Sumas Prefijas en Complejidad $O(N)$:}
    Calcula el arreglo prefijo $P_{j+1} = P_j + s_j$ y resta la media local en ventana $[-W, +W]$ en tiempo constante por muestra:
\begin{quote}
\small
\begin{verbatim}
void SignalProcessor::apply_dewow(BScan& bscan, double window_ns) {
    const double dt = bscan.metadata.sample_interval_ns;
    const size_t half_w = (window_ns > 0.0 && dt > 0.0)
                              ? std::max<size_t>(1, static_cast<size_t>(std::lround(0.5 * window_ns / dt)))
                              : 0;
    std::vector<double> psum;
    for (auto& tr : bscan.traces) {
        const size_t n = tr.samples.size();
        if (n == 0) continue;
        psum.assign(n + 1, 0.0);
        for (size_t j = 0; j < n; ++j) psum[j + 1] = psum[j] + tr.samples[j];
        for (size_t j = 0; j < n; ++j) {
            const size_t a = (j > half_w) ? (j - half_w) : 0;
            const size_t b = std::min(n - 1, j + half_w);
            const double mean_loc = (psum[b + 1] - psum[a]) / static_cast<double>(b - a + 1);
            tr.samples[j] -= mean_loc;
        }
    }
}
\end{verbatim}
\end{quote}

    \item \textbf{Subtarea 7.2: Time-Zero con Cosine-Taper:}
    Calcula el promedio apilado de las primeras 40\% muestras, detecta el primer cruce que supere el $15\%$ del máximo global ($j_0$), desplaza a la izquierda y suaviza las últimas 16 muestras con una ventana coseno antes del relleno de ceros.

    \item \textbf{Subtarea 7.3: Filtro Butterworth Pasabanda con Zero-Padding a $2N$:}
    Calcula $N_{\text{fft}} = 2^{\lceil \log_2(2N) \rceil}$. Genera la función de ganancia real simétrica de orden $N_B = 4$:
    \begin{equation}
        G(f) = \frac{1}{\sqrt{1 + (f_{\text{low}}/f)^{2N_B}}} \cdot \frac{1}{\sqrt{1 + (f/f_{\text{high}})^{2N_B}}}
    \end{equation}
    Multiplica el espectro FFT complejo y aplica la IFFT inversa, conservando solo las primeras $N$ muestras reales.

    \item \textbf{Subtarea 7.4: Sustracción de Fondo (Background Removal):}
    Calcula la traza media espacial $\bar{s}_j = \frac{1}{M}\sum_{i=1}^M s_{i,j}$ y réstala de todas las trazas para suprimir el acoplamiento directo horizontal transmisor-receptor.

    \item \textbf{Subtarea 7.5: Ganancia SEC con Techo de Saturación (\textit{Gain Ceiling}):}
    Multiplica por $(1 + \beta t)\exp(\alpha t)$ con un límite superior de $60\text{ dB}$ para evitar amplificar el ruido térmico profundo.
\end{itemize}

\subsection{4. Prueba de Verificación Unitaria}
Aplica el pipeline DSP completo sobre una traza de prueba con deriva DC de $500.0$, señal útil en $400\text{ MHz}$ y ruido de alta frecuencia en $3000\text{ MHz}$.
Verifica que la deriva DC se reduce en más del $99.9\%$, la señal de $400\text{ MHz}$ mantiene su amplitud original $\pm 5\%$, y el ruido de $3000\text{ MHz}$ se atenúa en más de $30\text{ dB}$.

% -----------------------------------------------------------------------------
% SPRINT 8
% -----------------------------------------------------------------------------
\section{Sprint 8: Migración 2D de Kirchhoff y Nubes de Puntos 3D Georreferenciadas}

\subsection{1. Fundamentos y Mapeo Bibliográfico}
\begin{itemize}
    \item \textbf{\textit{Mathematik} (Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 24 (\textit{Flächenintegrale})}: Integrales de superficie y formulación de la integral de frontera de Helmholtz-Kirchhoff.
        \item \textbf{Kapitel 28 (\textit{Interpolation})}: Interpolación lineal sub-muestra entre muestras temporales discretas continuas.
    \end{itemize}
    \item \textbf{\textit{Physik} (Tipler \& Mosca)}:
    \begin{itemize}
        \item \textbf{Kapitel 15 (\textit{Wellenbeugung und Huygenssches Prinzip})}: Principio de Huygens: cada punto difractor en el subsuelo reemite como una fuente secundaria esférica. La migración representa el colapso espacial retropropagado de los frentes de onda difractados.
    \end{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 7 \& 8 (\textit{Klassen und Move-Semantik})}: Diseño eficiente de la estructura de nube de puntos \texttt{PointCloud3D} con reserva contigua para evitar millones de micro-asignaciones.
    \end{itemize}
\end{itemize}

\subsection{2. Justificación de Ingeniería: ¿Por qué hay que implementarlo?}
\begin{enumerate}
    \item \textbf{La Distorsión Hiperbólica de Tubos y Cavidades:}
    Debido a la apertura angular del cono de radiación de la antena electromagnética, un tubo enterrado de apenas $10\text{ cm}$ de diámetro no aparece como un punto en el radargrama, sino como una hipérbola gigante que abarca varios metros de ancho. Si intentamos generar una nube de puntos o una malla 3D directamente desde el radargrama crudo, obtendremos una superficie curva gigantesca en lugar de una tubería cilíndrica.
    \item \textbf{Colapso Coherente de Energía con Kirchhoff:}
    La migración de Kirchhoff suma coherentemente las amplitudes a lo largo de la hipérbola teórica de difracción:
    \begin{equation}
        t(x) = \frac{2}{v} \sqrt{(x - x_0)^2 + z_0^2}
    \end{equation}
    colapsando toda la energía dispersa de la hipérbola al punto focal real $(x_0, z_0)$ y suprimiendo las colas laterales mediante el factor de oblicuidad $\cos\theta = z_0/R$.
\end{enumerate}

\subsection{3. Tareas Detalladas de Implementación}
En \texttt{src/dsp/signal\_processor.cpp} y \texttt{src/dsp/interpreter.cpp}:
\begin{itemize}
    \item \textbf{Subtarea 8.1: Algoritmo de Migración 2D de Kirchhoff en Tiempo:}
    Para cada traza $i_0$ en $x_0$ y cada profundidad de muestra $j_0$ (tiempo $t_0 = j_0 \Delta t$, profundidad $z_0 = \frac{1}{2} v t_0$):
    \begin{enumerate}
        \item Determina el rango de trazas dentro de la apertura horizontal $[x_0 - L_{\text{ap}}, \, x_0 + L_{\text{ap}}]$.
        \item Para cada traza vecina $i$ en posición $x_i$, calcula la distancia oblicua $R = \sqrt{(x_i - x_0)^2 + z_0^2}$.
        \item Calcula el tiempo de viaje teórico $t_{\text{diff}} = 2R / v$ y su índice continuo $s_{\text{idx}} = t_{\text{diff}} / \Delta t$.
        \item Realiza interpolación lineal entre las muestras enteras $k = \lfloor s_{\text{idx}} \rfloor$ y $k+1$.
        \item Pondera por el factor de oblicuidad $\cos\theta = z_0 / R$ y suma a la muestra de salida.
    \end{enumerate}

    \item \textbf{Subtarea 8.2: Conversión TWT a Profundidad y Nube de Puntos Georreferenciada:}
    En \texttt{RadarInterpreter::transform\_to\_3d}, convierte cada muestra o pico de envolvente en un \texttt{Point3D} con coordenadas geográficas WGS84, proyección métrica UTM y coordenadas locales ENU, asignando la profundidad $z = \frac{1}{2} v \cdot \text{twt}$ como cota negativa bajo el terreno.
\end{itemize}

\subsection{4. Prueba de Verificación Unitaria}
Aplica la migración de Kirchhoff sobre el radargrama sintético del Sprint 2 (hipérbola centrada en $x = 2.5\text{ m}$, $z = 0.8\text{ m}$).
Verifica que tras la migración, la energía de las ramas de la hipérbola se reduce en más del $80\%$ y el pico máximo de la envolvente migrada se concentra a menos de $5\text{ cm}$ del centro verdadero $(x = 2.5, z = 0.8)$.

% -----------------------------------------------------------------------------
% SPRINT 9
% -----------------------------------------------------------------------------
\section{Sprint 9: Inversión Geofísica Subterránea (Semblanza, Fresnel y ASTM D6432)}

\subsection{1. Fundamentos y Mapeo Bibliográfico}
\begin{itemize}
    \item \textbf{\textit{Mathematik} (Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 28 (\textit{Ausgleichsrechnung})}: Métrica de semblanza de Neidell-Taner (energía apilada sobre energía individual normalizada), ajuste parabólico de 3 puntos alrededor de un máximo para resolución submalla, y partición en tiempo lineal $O(N)$ con \texttt{std::nth\_element}.
    \end{itemize}
    \item \textbf{\textit{Physik} (Tipler \& Mosca)}:
    \begin{itemize}
        \item \textbf{Kapitel 30, 31 \& 32 (\textit{Elektromagnetische Wellen und Reflexion})}: Coeficientes de reflexión de Fresnel en incidencia normal, impedancia de onda $\eta = \sqrt{\mu / \varepsilon}$, velocidad electromagnética $v = c_0 / \sqrt{\varepsilon_r}$, tangente de pérdidas $\tan\delta$, y corrimiento del centroide espectral (Quan-Harris).
    \end{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 13 \& 14 (\textit{STL-Algorithmen})}: Algoritmos de selección rápida y ordenación (\texttt{std::nth\_element}, \texttt{std::sort}, \texttt{std::clamp}).
    \end{itemize}
\end{itemize}

\subsection{2. Justificación de Ingeniería: ¿Por qué hay que implementarlo?}
\begin{enumerate}
    \item \textbf{El Peligro de Calibrar la Velocidad Manualmente:}
    Los operadores suelen asumir un valor fijo genérico de permitividad (p. ej. $\varepsilon_r = 9.0 \implies v = 0.10\text{ m/ns}$). Sin embargo, si el asfalto está seco ($\varepsilon_r = 4.0 \implies v = 0.15\text{ m/ns}$) o el terreno está empapado ($\varepsilon_r = 25.0 \implies v = 0.06\text{ m/ns}$), el error de profundidad supera el $100\%$, provocando que excavadoras rompan tuberías que estaban más cerca o más profundas de lo estimado. El \textbf{análisis de semblanza en hipérbolas} deduce la velocidad in situ a partir de la curvatura de la onda sin perforar el pavimento (Figura~\ref{fig:hiperbola_s9}).
    \item \textbf{Estratigrafía Multicapa de Fresnel en Carreteras:}
    El suelo bajo un pavimento no es uniforme; está formado por capas sucesivas (asfalto de rodadura, base granular de grava y subrasante arcillosa). Con la ley de Fresnel en incidencia normal:
    \begin{equation}
        R_k = \frac{\sqrt{\varepsilon_{r,k}} - \sqrt{\varepsilon_{r,k+1}}}{\sqrt{\varepsilon_{r,k}} + \sqrt{\varepsilon_{r,k+1}}} \implies \varepsilon_{r,k+1} = \varepsilon_{r,k} \left(\frac{1 - R_k}{1 + R_k}\right)^2
    \end{equation}
    es posible deducir la permitividad y el tipo de material de cada estrato hacia abajo sucesivamente (Figura~\ref{fig:fresnel_s9}).
    \item \textbf{Clasificación Automática de Materiales según ASTM D6432:}
    Los clientes y los sistemas GIS no necesitan solo valores numéricos abstractos de $\varepsilon_r$; necesitan saber si el estrato es \textit{Asphalt\_Dry}, \textit{Gravel\_Dry}, \textit{Concrete} o \textit{Clay\_Wet}, y si el conducto es metálico o polimérico (PVC/HDPE).
\end{enumerate}

\begin{figure}[htbp]
    \centering
    \includegraphics[width=0.72\textwidth]{diagrams/hiperbola_geometria.pdf}
    \caption{Geometría hiperbólica del tiempo de viaje TWT y escaneo de velocidad $v$.}
    \label{fig:hiperbola_s9}
\end{figure}

\begin{figure}[htbp]
    \centering
    \includegraphics[width=0.75\textwidth]{diagrams/fresnel_estratigrafia.pdf}
    \caption{Inversión estratigráfica multicapa de Fresnel.}
    \label{fig:fresnel_s9}
\end{figure}

\subsection{3. Tareas Detalladas de Implementación}
En \texttt{include/gpr3d/dsp/soil\_inversion.hpp}, \texttt{src/dsp/soil\_inversion.cpp} y \texttt{src/geo/material\_db.cpp}:
\begin{itemize}
    \item \textbf{Subtarea 9.1: Ajuste de Velocidad por Semblanza de Neidell-Taner:}
    Para un ápice $(x_0, t_0)$, evalúa una grilla de velocidades $v_g \in [0.05, 0.20]\text{ m/ns}$ con paso $\Delta v = 0.002\text{ m/ns}$.
    Calcula la semblanza $S(v)$ sumando amplitudes sobre la hipérbola en una semiventana temporal $w \in [-M, +M]$:
    \begin{equation}
        S(v) = \frac{\sum_{w=-M}^M \left[ \sum_{i=1}^K s_i(t_i(v) + w\Delta t) \right]^2}{K \sum_{w=-M}^M \sum_{i=1}^K \left[ s_i(t_i(v) + w\Delta t) \right]^2}
    \end{equation}
    Encuentra el índice óptimo $g^*$ y aplica refinamiento parabólico submalla de tres puntos:
    \begin{equation}
        \delta = \frac{1}{2} \frac{S_{g^*-1} - S_{g^*+1}}{S_{g^*-1} - 2S_{g^*} + S_{g^*+1}}, \qquad v^* = v_{g^*} + \delta \Delta v, \qquad \varepsilon_r = \left(\frac{c_0}{v^*}\right)^2
    \end{equation}

    \item \textbf{Subtarea 9.2: Estimador de Atenuación Quan-Harris:}
    Calcula el corrimiento hacia frecuencias bajas del centroide espectral entre ventanas someras y profundas para estimar la atenuación intrínseca $\alpha$ (dB/m), la pérdida tangente $\tan\delta$ y la conductividad aparente $\sigma$ (mS/m).

    \item \textbf{Subtarea 9.3: Base de Datos Litológica ASTM D6432 (\texttt{MaterialDatabase}):}
    Implementa el clasificador bayesiano \texttt{classify\_soil\_layer} que asigna probabilidades evaluando la distancia logarítmica respecto a los rangos permitidos de permitividad y conductividad.
\end{itemize}

\subsection{4. Prueba de Verificación Unitaria}
\begin{enumerate}
    \item Prueba analítica de Fresnel: Con $\varepsilon_{r,1} = 4.0$ y $R = -0.333333$, la función \texttt{invert\_fresnel\_permittivity} debe devolver exactamente $\varepsilon_{r,2} = 16.000 \pm 0.001$.
    \item Clasificador ASTM: Para $\varepsilon_r = 4.2$, $\sigma = 0.5\text{ mS/m}$ en superficie ($z=0\text{ m}$), el material debe ser clasificado como \texttt{Asphalt\_Dry} con probabilidad $> 35\%$.
\end{enumerate}

% -----------------------------------------------------------------------------
% SPRINT 10
% -----------------------------------------------------------------------------
\section{Sprint 10: Reconstrucción Semántica 3D y Generación de Mallas}

\subsection{1. Fundamentos y Mapeo Bibliográfico}
\begin{itemize}
    \item \textbf{\textit{Mathematik} (Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 24 \& 25 (\textit{Kurven und Flächen})}: Triedro móvil de Frenet-Serret $(\mathbf{T}, \mathbf{N}, \mathbf{B})$, vector tangente $\mathbf{T} = \mathbf{c}'(s)$, vector normal $\mathbf{N}$ y vector binormal $\mathbf{B} = \mathbf{T} \times \mathbf{N}$.
        Parametrización cilíndrica espacial:
        \begin{equation}
            \mathbf{r}(s, \theta) = \mathbf{c}(s) + r\left(\cos\theta \mathbf{N}(s) + \sin\theta \mathbf{B}(s)\right)
        \end{equation}
    \end{itemize}
    \item \textbf{\textit{Physik} (Tipler \& Mosca)}:
    \begin{itemize}
        \item \textbf{Kapitel 15}: Continuidad geométrica y consistencia espacial de reflectores enterrados.
    \end{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 7 \& 8}: Mallas triangulares indexadas con normales unitarias suaves y orientación antihoraria (CCW).
    \end{itemize}
\end{itemize}

\subsection{2. Justificación de Ingeniería: ¿Por qué hay que implementarlo?}
\begin{enumerate}
    \item \textbf{De Nubes Dispersas a Objetos CAD/BIM Continuos:}
    Los ingenieros civiles no pueden diseñar obras basándose en millones de puntos aislados de una nube de puntos. Necesitan objetos CAD/BIM semánticos sólidos: un conducto cilíndrico cerrado con radio y eje central, o superficies de estrato de pavimento trianguladas.
    \item \textbf{Por qué es Indispensable el Triedro de Frenet-Serret en Tubos 3D:}
    Al extruir un tubo a lo largo de una línea 3D con curvas, si se proyectan los vértices del cilindro usando un plano cartesiano estático fijo (p. ej. XY), cuando la tubería gira o cambia de pendiente vertical, el cilindro colapsa sobre sí mismo (\textit{gimbal lock}) y sufre torsiones bruscas. El marco local móvil $(\mathbf{T}, \mathbf{N}, \mathbf{B})$ se desplaza y gira suavemente acompañando a la curva, garantizando una sección transversal perfectamente circular en todo momento (Figura~\ref{fig:frenet_s10}).
\end{enumerate}

\begin{figure}[htbp]
    \centering
    \includegraphics[width=0.75\textwidth]{diagrams/frenet_serret_tubo.pdf}
    \caption{Triedro móvil de Frenet-Serret para la extrusión tubular cilíndrica de conductos 3D.}
    \label{fig:frenet_s10}
\end{figure}

\subsection{3. Tareas Detalladas de Implementación}
En \texttt{include/gpr3d/scene/objects\_3d.hpp} y \texttt{src/dsp/object\_transformer.cpp}:
\begin{itemize}
    \item \textbf{Subtarea 10.1: Definición de estructuras de escena 3D:}
    Define \texttt{Vec3}, \texttt{ColorRgba}, \texttt{Mesh3D}, \texttt{ConduitObject3D}, \texttt{LayerHorizonObject3D}, \texttt{AnomalyObject3D} y \texttt{Scene3D}.

    \item \textbf{Subtarea 10.2: Extrusión tubular \texttt{build\_tube\_mesh}:}
    Dada una secuencia de puntos del eje central $\mathbf{c}_i$, radio $r$, y $M$ sectores radiales:
    \begin{enumerate}
        \item Para cada punto, calcula el vector tangente $\mathbf{T}_i = \operatorname{norm}(\mathbf{c}_{i+1} - \mathbf{c}_{i-1})$.
        \item Elige un vector vertical auxiliar $\mathbf{u} = (0,0,1)$ (o $(0,1,0)$ si $\mathbf{T}$ es casi vertical).
        \item Calcula la normal $\mathbf{N} = \operatorname{norm}(\mathbf{T} \times \mathbf{u})$ y la binormal $\mathbf{B} = \mathbf{T} \times \mathbf{N}$.
        \item Genera los $M$ vértices del anillo circular:
        \begin{equation}
            \mathbf{v}_{i,j} = \mathbf{c}_i + r\left(\cos\theta_j \mathbf{N} + \sin\theta_j \mathbf{B}\right), \quad \theta_j = \frac{2\pi j}{M}
        \end{equation}
        \item Ensambla los triángulos entre anillos adyacentes con orientación antihoraria (CCW):
        $(v_{i,j}, \, v_{i+1,j}, \, v_{i+1,j+1})$ y $(v_{i,j}, \, v_{i+1,j+1}, \, v_{i,j+1})$.
    \end{enumerate}

    \item \textbf{Subtarea 10.3: Triangulación de Horizontes y Esferas de Anomalías:}
    Implementa \texttt{build\_surface\_from\_profiles} para tejer superficies de contacto entre perfiles paralelos, y \texttt{build\_ellipsoid\_mesh} para cavidades y bloques de roca.
\end{itemize}

\subsection{4. Prueba de Verificación Unitaria}
Para un eje recto de $5\text{ puntos}$ con radio $0.1\text{ m}$ y $16\text{ sectores}$:
Verifica que la malla tiene exactamente $5 \times 16 = 80\text{ vértices}$ y $4 \times 32 = 128\text{ triángulos}$, y que ninguna cara triangular tiene área nula.

% -----------------------------------------------------------------------------
% SPRINT 11
% -----------------------------------------------------------------------------
\section{Sprint 11: Exportadores e Interoperabilidad (glTF 2.0 Binary, GeoJSON 3D, HTML)}

\subsection{1. Fundamentos y Mapeo Bibliográfico}
\begin{itemize}
    \item \textbf{\textit{Mathematik} (Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 7 (\textit{Lineare Abbildungen})}: Transformaciones de coordenadas afines $4 \times 4$ y renderizado proyectivo.
    \end{itemize}
    \item \textbf{\textit{Physik} (Tipler \& Mosca)}:
    \begin{itemize}
        \item \textbf{Kapitel 31 (\textit{Optik})}: Modelos de materiales con renderizado basado en la física (PBR: rugosidad \textit{roughness}, metalicidad \textit{metallic} y albedo difuso).
    \end{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 16 (\textit{Binäre Dateiverarbeitung})}: Serialización estructurada de especificaciones abiertas (Khronos glTF 2.0 y GeoJSON RFC 7946).
    \end{itemize}
\end{itemize}

\subsection{2. Justificación de Ingeniería: ¿Por qué hay que implementarlo?}
\begin{enumerate}
    \item \textbf{El Estándar Khronos glTF 2.0 Binary (.glb) como Formato Universal:}
    Los formatos antiguos como OBJ o DXF no soportan materiales PBR modernos, o son formatos de texto gigantescos que tardan minutos en abrirse. El formato binario \texttt{.glb} empaqueta en un único archivo la descripción de nodos y materiales en JSON (\textit{Chunk 0}) junto con los búferes binarios directos de vértices y normales (\textit{Chunk 1}) alineados a 4 bytes, permitiendo que la GPU cargue la geometría en milisegundos sin necesidad de parsear texto.
    \item \textbf{Georreferenciación en Sistemas GIS con GeoJSON 3D:}
    Los organismos públicos y empresas de servicios públicos (agua, gas, electricidad, fibra óptica) gestionan sus redes en QGIS o ArcGIS. Un archivo GeoJSON 3D con cotas negativas en formato RFC 7946 permite superponer las tuberías detectadas directamente sobre el mapa municipal.
    \item \textbf{Cuadros de Mando Web Interactivos sin Instalación:}
    Entregar un archivo HTML interactivo con Three.js permite que clientes, directores de obra o topógrafos inspeccionen la escena 3D orbital y el radargrama B-scan sincronizado desde cualquier navegador web o smartphone sin instalar software adicional.
\end{enumerate}

\subsection{3. Tareas Detalladas de Implementación}
En \texttt{src/io/scene\_exporter.cpp}:
\begin{itemize}
    \item \textbf{Subtarea 11.1: Serializador Khronos glTF 2.0 Binary (.glb):}
    \begin{enumerate}
        \item Escribe la cabecera de 12 bytes: \texttt{magic = 0x46546C67} ("glTF"), \texttt{version = 2}, y \texttt{total\_length}.
        \item Rellena el Chunk 0 (JSON de escena, materiales geológicos PBR, nodos) con espacios ASCII (\texttt{0x20}) hasta un múltiplo exacto de 4 bytes. Escribe el encabezado del Chunk (\texttt{length}, tipo \texttt{0x4E4F534A} "JSON").
        \item Rellena el Chunk 1 (Búfer binario con vértices en float IEEE 754 y triángulos en uint32\_t) con ceros (\texttt{0x00}) hasta múltiplo de 4 bytes. Escribe el encabezado (\texttt{length}, tipo \texttt{0x004E4942} "BIN").
    \end{enumerate}

    \item \textbf{Subtarea 11.2: Serializador GeoJSON 3D (RFC 7946):}
    Escribe una \texttt{FeatureCollection} donde cada conducto es un \texttt{LineString} con coordenadas \texttt{[lon, lat, alt\_m]} y atributos de material clasificado, diámetro estimado y nivel de confianza.

    \item \textbf{Subtarea 11.3: Cuadro de mando interactivo HTML5 autónomo:}
    Genera un archivo HTML autocontenido con Three.js embebido, controles orbitales (\texttt{OrbitControls}), radargrama sincronizado y panel lateral con la columna litológica.
\end{itemize}

\subsection{4. Prueba de Verificación Unitaria}
Valida el archivo \texttt{.glb} generado usando un validador oficial Khronos o Blender:
La escena debe abrirse sin advertencias de alineación de búfer, con las normales orientadas hacia el exterior y los materiales geológicos diferenciados.

% -----------------------------------------------------------------------------
% SPRINT 12
% -----------------------------------------------------------------------------
\section{Sprint 12: Integración CLI, Analítica Gráfica y Validación Real}

\subsection{1. Fundamentos y Mapeo Bibliográfico}
\begin{itemize}
    \item \textbf{\textit{Mathematik} (Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 28}: Análisis estadístico de fluctuaciones de señal (media, desviación estándar, asimetría y curtosis).
    \end{itemize}
    \item \textbf{\textit{Physik} (Tipler \& Mosca)}:
    \begin{itemize}
        \item \textbf{Kapitel 15}: Interpretación integral de radargramas de campo reales.
    \end{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 1 (\textit{Kommandozeilen-Schnittstelle})}: Parsing de argumentos de línea de comandos (\texttt{argc}, \texttt{argv}) y manejo robusto de excepciones en el punto de entrada (\texttt{main}).
    \end{itemize}
\end{itemize}

\subsection{2. Justificación de Ingeniería: ¿Por qué hay que implementarlo?}
Una biblioteca geofísica sin herramientas de línea de comandos ni generadores de informes gráficos resulta inútil para la operación en campo. El CLI \texttt{gpr3d} consolida todos los sprints previos en una suite unificada con subcomandos claros:
\begin{itemize}
    \item \texttt{gpr3d inspect}: Diagnóstico geofísico rápido de cabeceras, velocidades e inversión litológica sin procesar todo el levantamiento.
    \item \texttt{gpr3d process}: Acondicionamiento de señal por lotes.
    \item \texttt{gpr3d transform}: Reconstrucción semántica 3D y exportación a GLB, GeoJSON y HTML.
    \item \texttt{gpr3d plot}: Renderizado de radargramas de alta resolución y analíticas de fluctuación con Cairo.
\end{itemize}

\subsection{3. Tareas Detalladas de Implementación}
En \texttt{apps/gpr3d\_cli.cpp} y \texttt{src/viz/fluctuation\_plotter.cpp}:
\begin{itemize}
    \item \textbf{Subtarea 12.1: CLI Principal y Subcomandos:}
    Implementa el despacho de comandos, validando argumentos y capturando excepciones con mensajes descriptivos.
    \item \textbf{Subtarea 12.2: Generador de Gráficos de Fluctuación con Cairo:}
    Crea radargramas en escala de grises o paleta geofísica, perfiles de energía instantánea de Hilbert y gráficos de atenuación en PNG y SVG vectorial.
    \item \textbf{Subtarea 12.3: Validación contra Archivos Reales del Repositorio:}
    Ejecuta el pipeline completo contra \texttt{parking.nx15.dzt} y \texttt{ds010\_A.DZT}.
\end{itemize}

\subsection{4. Prueba de Verificación Unitaria}
Ejecuta en consola:
\texttt{./build/gpr3d inspect parking.nx15.dzt}
Verifica que el informe en terminal reporta:
\begin{itemize}
    \item Frecuencia nominal de antena coherente con el modelo (p. ej. $1600\text{ MHz}$ para NX15).
    \item Detección de ápices hiperbólicos con velocidades calculadas entre $0.08\text{ m/ns}$ y $0.15\text{ m/ns}$.
    \item Clasificación litológica dominante acorde a pavimento urbano (\texttt{Asphalt\_Dry} y \texttt{Gravel\_Dry}).
\end{itemize}

% -----------------------------------------------------------------------------
% CONCLUSIONES
% -----------------------------------------------------------------------------
\section{Conclusiones y Hoja de Ruta de Especialización Profesional}

Siguiendo paso a paso los 13 sprints desarrollados en esta guía técnica, el desarrollador habrá replicado de forma autónoma una suite completa de software geofísico en C++20 moderno, adquiriendo dominio en:
\begin{enumerate}
    \item \textbf{Arquitectura de C++20 de Alto Rendimiento:} Abstracciones de coste cero, gestión de memoria RAII sin fugas, seguridad de tipos y serialización binaria de bajo nivel.
    \item \textbf{Procesamiento Digital de Señales (DSP):} Transformada de Fourier FFT, señal analítica de Hilbert, filtrado Butterworth de fase cero y compensación de atenuación.
    \item \textbf{Geofísica e Inversión Subterránea:} Análisis de semblanza en hipérbolas, inversión de reflectancias de Fresnel y clasificación litológica ASTM D6432.
    \item \textbf{Geodesia Espacial y Computación Gráfica 3D:} Transformaciones geodésicas (WGS84, ECEF, ENU, UTM), extrusión tubular con el triedro móvil de Frenet-Serret y empaquetado de escenas Khronos glTF 2.0 con materiales PBR.
\end{enumerate}

\begin{center}
    \vspace{0.8cm}
    \rule{0.6\textwidth}{0.8pt}\\[0.2cm]
    \textbf{Proyecto \texttt{SignalHandler} / \texttt{gpr3d}} $\bullet$ Noviembre 2026\\
    \textit{Documento técnico y académico de ingeniería inversa y desarrollo en C++20.}
\end{center}

\end{document}
