#!/usr/bin/env python3
# -*- coding: utf-8 -*-

def get_content_s4_s7():
    return r"""
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
"""

print("Módulo S4-S7 reescrito limpiamente.")
