#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Constructor del documento exhaustivo de replicación gpr3d (LaTeX y Markdown)."""

import os
import subprocess
import shutil

DOCS_DIR = "/home/luis/Documents/Projects/Glasfaser/SignalHandler/docs"
TEX_PATH = os.path.join(DOCS_DIR, "Guia_Replicacion_Paso_a_Paso_GPR3D.tex")
PDF_PATH = os.path.join(DOCS_DIR, "Guia_Replicacion_Paso_a_Paso_GPR3D.pdf")
MD_PATH = os.path.join(DOCS_DIR, "GUIA_REPLICACION_PASO_A_PASO.md")
ARTIFACT_PATH = "/home/luis/.gemini/antigravity/brain/3eb84fe1-79d3-4ab0-a519-bf37c63fc6ea/guia_replicacion_paso_a_paso_gpr3d.md"

def get_latex_header():
    return r"""\documentclass[11pt,a4paper]{article}
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
"""

def get_section_intro():
    return r"""
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
"""

print("Escribiendo módulo de inicialización...")

