#!/usr/bin/env python3
# -*- coding: utf-8 -*-

def get_content_s0_s3():
    return r"""
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
"""

print("Módulo S0-S3 definido.")
