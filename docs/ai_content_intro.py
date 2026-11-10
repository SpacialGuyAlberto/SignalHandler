#!/usr/bin/env python3
# -*- coding: utf-8 -*-

def get_ai_latex_header_and_intro():
    return r"""\documentclass[11pt,a4paper]{article}
\usepackage[utf8]{inputenc}
\usepackage{amsmath,amssymb}
\usepackage{color}
\usepackage{graphicx}
\usepackage{hyperref}
\usepackage{url}

% Configuracion de margenes estandar A4
\setlength{\textwidth}{16.5cm}
\setlength{\textheight}{24.0cm}
\setlength{\oddsidemargin}{0.0cm}
\setlength{\evensidemargin}{0.0cm}
\setlength{\topmargin}{-1.2cm}
\setlength{\parindent}{0pt}
\setlength{\parskip}{6pt}

% Definicion de colores para enlaces y destacados
\definecolor{navyblue}{rgb}{0.05, 0.15, 0.45}
\definecolor{darkgreen}{rgb}{0.08, 0.40, 0.12}
\definecolor{darkred}{rgb}{0.55, 0.10, 0.10}
\definecolor{boxbg}{rgb}{0.96, 0.97, 0.99}

\hypersetup{
    colorlinks=true,
    linkcolor=navyblue,
    citecolor=darkgreen,
    urlcolor=navyblue,
    pdftitle={Plan y Guia Tecnica: Reconstruccion 3D por Redes Neuronales en gpr3d},
    pdfauthor={Proyecto SignalHandler / gpr3d - IA Team}
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
    {\LARGE \textbf{Plan y Gu{\'\i}a T{\'e}cnica de Implementaci{\'o}n:\\[0.2cm] Reconstrucci{\'o}n 3D de Formas Reales mediante Redes Neuronales y Neural SDF en \texttt{gpr3d}}}\\[0.4cm]
    {\Large \textbf{De la Aproximaci{\'o}n Determinista a la Morfolog{\'\i}a de Alta Fidelidad por Deep Learning y Physics-Informed Neural Networks}}\\[0.3cm]
    \rule{\textwidth}{1.2pt}\\[0.2cm]
    \textbf{Proyecto:} \texttt{SignalHandler} (\textit{gpr3d::ai}) \quad $\bullet$ \quad \textbf{Fecha:} Noviembre 2026 \quad $\bullet$ \quad \textbf{Est{\'a}ndar:} C++20 / ONNX Runtime / PyTorch\\
    \rule{\textwidth}{0.6pt}
\end{center}

\vspace{0.2cm}
\tableofcontents
\vspace{0.4cm}
\hrule
\vspace{0.4cm}

% -----------------------------------------------------------------------------
% SECCION 1: VISION GENERAL Y PARADIGMA HIBRIDO
% -----------------------------------------------------------------------------
\section{Visi{\'o}n General: El Salto de Formas Param{\'e}tricas a Geometr{\'\i}as Reales}

El motor actual de \texttt{gpr3d} genera objetos tridimensionales utilizando primitivas anal{\'\i}ticas geom{\'e}tricas: cilindros param{\'e}tricos de radio uniforme $r$ extruidos a lo largo de ejes de tuber{\'\i}a (triedro de Frenet-Serret) y elipsoides regulares para anomal{\'\i}as puntuales.

Aunque este enfoque es matem{\'a}ticamente elegante y robusto para conductos nuevos y perfectos, en la \textbf{infraestructura subterr{\'a}nea real} los objetos presentan morfolog{\'\i}as heterog{\'e}neas y complejas:
\begin{enumerate}
    \item \textbf{Deformaciones mec{\'a}nicas y aplastamientos:} Tuber{\'\i}as pl{\'a}sticas de PVC/HDPE o conductos de alcantarillado que han colapsado el{\'\i}pticamente bajo el peso del tr{\'a}fico vehicular.
    \item \textbf{Accesorios e interferencias angulares:} Codos a $45^\circ$ y $90^\circ$, derivaciones en T, reducciones de di{\'a}metro, v{\'a}lvulas y cajas de empalme prism{\'a}ticas de telecomunicaciones.
    \item \textbf{Cuerpos y cavidades irregulares:} Socavones producidos por fugas de agua, oquedades con superficies rugosas y bloques de roca err{\'a}ticos que envuelven las canalizaciones.
    \item \textbf{L{\'\i}mite de resoluci{\'o}n cl{\'a}sico de Rayleigh:} Dos conductos de fibra {\'o}ptica colocados a menos de $5\text{ cm}$ de distancia se solapan difractivamente en el radargrama, apareciendo como una {\'u}nica masa informe en la f{\'\i}sica cl{\'a}sica.
\end{enumerate}

Para resolver estos desaf{\'\i}os sin sacrificar la rigurosidad f{\'\i}sica, se dise{\~n}a el \textbf{Paradigma H{\'\i}brido F{\'\i}sica Determinista + Deep Learning (PINN + Neural SDF)}, ilustrado en la Figura~\ref{fig:ai_arquitectura}.

\begin{figure}[htbp]
    \centering
    \includegraphics[width=0.95\textwidth]{diagrams/ai_arquitectura_reconstruccion.pdf}
    \caption{Arquitectura del subsistema de Inteligencia Artificial de \texttt{gpr3d} y su acoplamiento con la f{\'\i}sica determinista.}
    \label{fig:ai_arquitectura}
\end{figure}

\subsection{El Principio de Cooperaci{\'o}n H{\'\i}brida}
La red neuronal \textbf{no} debe aprender la f{\'\i}sica desde cero (lo cual requerir{\'\i}a millones de par{\'a}metros y resultar{\'\i}a inestable). El motor en C++20 le proporciona a la IA:
\begin{itemize}
    \item La velocidad real de propagaci{\'o}n $v$ y permitividad $\varepsilon_r$ obtenidas por an{\'a}lisis de semblanza (Sprint 9).
    \item La posici{\'o}n inicial aproximada de los reflectores colapsados mediante migraci{\'o}n de Kirchhoff (Sprint 8).
    \item La polaridad de fase y la clasificaci{\'o}n litol{\'o}gica de material ASTM (Sprint 9).
\end{itemize}
Con esta informaci{\'o}n contextual, la red neuronal se especializa exclusivamente en resolver la \textbf{morfolog{\'\i}a volum{\'e}trica fina a escala sub-longitud de onda}, prediciendo un campo de distancia signada continuo $f_\theta(\mathbf{x}) = d$.

\newpage

% -----------------------------------------------------------------------------
% SECCION 2: FUNDAMENTOS MATEMATICOS Y MAPEO BIBLIOGRAFICO
% -----------------------------------------------------------------------------
\section{Fundamentos Matem{\'a}ticos de Neural SDF y Mapeo Acad{\'e}mico}

\subsection{1. Mapeo a Literatura Est{\'a}ndar Universitaria}
\begin{itemize}
    \item \textbf{\textit{Mathematik} (Tilo Arens et al.)}:
    \begin{itemize}
        \item \textbf{Kapitel 24 \& 25 (\textit{Fl{\"a}chen im Raum und Vektoranalysis})}: Superficies impl{\'\i}citas $F(x, y, z) = 0$, gradiente $\nabla F = (\frac{\partial F}{\partial x}, \frac{\partial F}{\partial y}, \frac{\partial F}{\partial z})$, y vector normal unitario $\mathbf{n} = \nabla F / \|\nabla F\|$.
        \item \textbf{Kapitel 27 (\textit{Partielle Differentialgleichungen})}: Ecuaci{\'o}n Eikonal no lineal $\|\nabla \Phi(\mathbf{x})\| = 1$, propagaci{\'o}n de frentes de onda y geometr{\'\i}a de distancias euclidianas m{\'\i}nimas a conjuntos cerrados.
        \item \textbf{Kapitel 28 (\textit{Numerische Optimierung})}: M{\'e}todo de descenso de gradiente estoc{\'a}stico (SGD), aproximaci{\'o}n por m{\'\i}nimos cuadrados no lineales y regularizaci{\'o}n de Tikhonov.
    \end{itemize}
    \item \textbf{\textit{Physik f{\"u}r Wissenschaftler und Ingenieure} (Tipler \& Mosca)}:
    \begin{itemize}
        \item \textbf{Kapitel 15 (\textit{Wellen und Beugung})}: Dispersi{\'o}n de ondas electromagn{\'e}ticas en obst{\'a}culos de dimensiones comparables a la longitud de onda ($\lambda = v / f$).
        \item \textbf{Kapitel 31 (\textit{Optik})}: Criterio de Rayleigh para resoluci{\'o}n l{\'\i}mite angular y espacial ($\Delta x_{\min} \approx 0.61 \lambda / \text{NA}$).
    \end{itemize}
    \item \textbf{Literatura Fundamental de Deep Learning y Redes Guiadas por la F{\'\i}sica (PINN)}:
    \begin{itemize}
        \item \textbf{Goodfellow, Bengio \& Courville (\textit{Deep Learning}, MIT Press)}: Convoluciones 3D, funciones de activaci{\'o}n no lineales (ReLU, Sine/SIREN, GELU) y optimizador AdamW con regularizaci{\'o}n de pesos.
        \item \textbf{Raissi, Perdikaris \& Karniadakis (2019, \textit{Physics-Informed Neural Networks}, J. Comp. Phys.)}: Penalizaci{\'o}n de residuos de ecuaciones diferenciales parciales (EDP) en la funci{\'o}n de p{\'e}rdida mediante diferenciaci{\'o}n autom{\'a}tica.
        \item \textbf{Gropp, Yariv, Haim, Atzmon \& Lipman (2020, \textit{Implicit Geometric Regularization for Learning Shapes}, NeurIPS)}: Regularizaci{\'o}n geom{\'e}trica impl{\'\i}cita mediante la condici{\'o}n eikonal $\|\nabla_\mathbf{x} f_\theta(\mathbf{x})\| = 1$.
        \item \textbf{Park, Florence, Straub, Newcombe \& Lovegrove (2019, \textit{DeepSDF: Learning Continuous Signed Distance Functions}, CVPR)}: Representaci{\'o}n continua de formas 3D condicionadas por c{\'o}digos latentes.
    \end{itemize}
    \item \textbf{\textit{C++ professionell anwenden} (Ulrich Breymann)}:
    \begin{itemize}
        \item \textbf{Kapitel 7, 8 \& 11}: Gesti{\'o}n de memoria contigua en C++20 con \texttt{std::span}, construcci{\'o}n de tensores planos sin copias intermedias y estructuras de datos para mallas triangulares.
        \item \textbf{Kapitel 18 (\textit{C-Bibliotheken})}: Integraci{\'o}n del runtime de inferencia nativo \texttt{onnxruntime-cxx} con punteros inteligentes RAII y ejecuci{\'o}n sin int{\'e}rprete de Python.
    \end{itemize}
\end{itemize}

\subsection{2. La Representaci{\'o}n Geom{\'e}trica por Neural SDF}
En lugar de discretizar el espacio en cuadr{\'\i}culas de p{\'\i}xeles o p{\'o}xeles volum{\'e}tricos (v{\'o}xeles), una \textbf{Funci{\'o}n de Distancia Signada Neuronal (Neural SDF)} modela el objeto como un campo continuo parametrizado por una red neuronal profunda $f_\theta$:
\begin{equation}
    f_\theta : \mathbb{R}^3 \times \mathbb{R}^k \to \mathbb{R}, \qquad f_\theta(\mathbf{x}, \mathbf{z}) = d
\end{equation}
donde $\mathbf{x} = (x, y, z)$ es un punto de consulta en el espacio tridimensional en metros, $\mathbf{z} \in \mathbb{R}^{128}$ es el vector latente que describe las caracter{\'\i}sticas del objeto detectado en el radargrama, y $d$ es la distancia euclidiana signada con la convenci{\'o}n (ver Figura~\ref{fig:neural_sdf}):
\begin{equation}
    d = \begin{cases}
        < 0 & \text{si } \mathbf{x} \text{ se encuentra en el interior del objeto (ej. n{\'u}cleo del tubo)} \\
        = 0 & \text{si } \mathbf{x} \text{ se encuentra sobre la superficie o pared f{\'\i}sica real} \\
        > 0 & \text{si } \mathbf{x} \text{ se encuentra en el exterior (ej. suelo o estrato circundante)}
    \end{cases}
\end{equation}

\begin{figure}[htbp]
    \centering
    \includegraphics[width=0.78\textwidth]{diagrams/neural_sdf_concepto.pdf}
    \caption{Curvas de nivel del campo escalar continuo predicho por Neural SDF. La superficie f{\'\i}sica real se extrae evaluando el conjunto de nivel cero $f_\theta(\mathbf{x}) = 0$.}
    \label{fig:neural_sdf}
\end{figure}

\subsection{3. Propiedades Matem{\'a}ticas Clave del Neural SDF}
\begin{enumerate}
    \item \textbf{Resoluci{\'o}n Infinita:} Al ser una funci{\'o}n continua $f_\theta$, no existe un tama{\~n}o m{\'\i}nimo de p{\'\i}xel. Se puede evaluar la malla poligonal con cualquier nivel de detalle arbitrario (paso de $5\text{ mm}$ o $1\text{ mm}$).
    \item \textbf{Topolog{\'\i}a Arbitraria sin Costuras:} A diferencia de los tubos param{\'e}tricos que solo pueden modelar cilindros simples no bifurcados, un conjunto de nivel cero $f_\theta(\mathbf{x}) = 0$ puede representar de forma nativa codos, uniones en T, cajas de registro huecas o zanjas irregulares sin costuras matem{\'a}ticas ni desgarros de malla.
    \item \textbf{C{\'a}lculo Anal{\'\i}tico Exacto de Normales por Gradiente:}
    La normal unitaria exterior a la superficie en cualquier punto de la pared se obtiene directamente evaluando el gradiente del campo SDF:
    \begin{equation}
        \mathbf{n}(\mathbf{x}) = \frac{\nabla_\mathbf{x} f_\theta(\mathbf{x})}{\|\nabla_\mathbf{x} f_\theta(\mathbf{x})\|}
    \end{equation}
    lo que produce un sombreado suave (\textit{smooth shading}) perfecto para renderizado PBR en motores 3D sin tener que promediar tri{\'a}ngulos vecinos ruidosos.
\end{enumerate}

\begin{figure}[htbp]
    \centering
    \includegraphics[width=0.92\textwidth]{diagrams/pipeline_entrenamiento_ia.pdf}
    \caption{Flujo de simulaci{\'o}n electromagn{\'e}tica FDTD, entrenamiento con p{\'e}rdidas f{\'\i}sicas PINN y exportaci{\'o}n a ONNX C++20.}
    \label{fig:pipeline_entrenamiento}
\end{figure}
"""

print("Módulo de introducción y teoría de IA definido.")

