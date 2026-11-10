#!/usr/bin/env python3
# -*- coding: utf-8 -*-

def get_content_s8_s12():
    return r"""
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
"""

print("Módulo S8-S12 definido.")

