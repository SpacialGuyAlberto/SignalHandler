#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Script maestro de compilacion y generacion de la Guia Tecnica de Replicacion v2."""

import os
import subprocess
import shutil

DOCS_DIR = "/home/luis/Documents/Projects/Glasfaser/SignalHandler/docs"
TEX_PATH = os.path.join(DOCS_DIR, "Guia_Replicacion_Paso_a_Paso_GPR3D.tex")
PDF_PATH = os.path.join(DOCS_DIR, "Guia_Replicacion_Paso_a_Paso_GPR3D.pdf")
ROOT_PDF = "/home/luis/Documents/Projects/Glasfaser/SignalHandler/Guia_Replicacion_Paso_a_Paso_GPR3D.pdf"
MD_PATH = os.path.join(DOCS_DIR, "GUIA_REPLICACION_PASO_A_PASO.md")
ARTIFACT_PATH = "/home/luis/.gemini/antigravity/brain/3eb84fe1-79d3-4ab0-a519-bf37c63fc6ea/guia_replicacion_paso_a_paso_gpr3d.md"

import sys
sys.path.append(DOCS_DIR)

from builder import get_latex_header, get_section_intro
from content_s0_s3 import get_content_s0_s3
from content_s4_s7 import get_content_s4_s7
from content_s8_s12 import get_content_s8_s12


# Limpieza previa de archivos auxiliares
for ext in ['.aux', '.log', '.out', '.toc']:
    p = os.path.join(DOCS_DIR, 'Guia_Replicacion_Paso_a_Paso_GPR3D' + ext)
    if os.path.exists(p):
        os.remove(p)

print("1. Ensamblando documento LaTeX...")
full_tex = (
    get_latex_header() +
    get_section_intro() +
    get_content_s0_s3() +
    get_content_s4_s7() +
    get_content_s8_s12()
)

with open(TEX_PATH, "w", encoding="utf-8") as f:
    f.write(full_tex)
print(f"-> Archivo LaTeX escrito en: {TEX_PATH} ({len(full_tex)} caracteres)")

print("2. Compilando PDF con pdflatex (Pass 1)...")
res1 = subprocess.run(["pdflatex", "-interaction=nonstopmode", "Guia_Replicacion_Paso_a_Paso_GPR3D.tex"],
                      cwd=DOCS_DIR, capture_output=True, text=True, errors="replace")
print("Pass 1 returncode:", res1.returncode)
if res1.returncode != 0:
    print(res1.stdout[-800:])

print("3. Compilando PDF con pdflatex (Pass 2 para TOC y referencias)...")
res2 = subprocess.run(["pdflatex", "-interaction=nonstopmode", "Guia_Replicacion_Paso_a_Paso_GPR3D.tex"],
                      cwd=DOCS_DIR, capture_output=True, text=True, errors="replace")
print("Pass 2 returncode:", res2.returncode)
if res2.returncode == 0:
    print(f"-> PDF compilado con exito: {PDF_PATH}")
    shutil.copyfile(PDF_PATH, ROOT_PDF)
    print(f"-> Copia actualizada en la raiz del workspace: {ROOT_PDF}")
else:
    print(res2.stdout[-1000:])

print("4. Generando version Markdown detallada...")
# Generamos la guia Markdown sincronizada
# Convierte comandos LaTeX basicos a Markdown limpio para lectura directa
md_content = full_tex

# Reemplazos sencillos para crear el markdown
md_clean = full_tex
md_clean = md_clean.replace(r"\section{", "\n# ")
md_clean = md_clean.replace(r"\subsection{", "\n## ")
md_clean = md_clean.replace(r"\subsubsection{", "\n### ")
md_clean = md_clean.replace(r"\textbf{", "**")
md_clean = md_clean.replace(r"\textit{", "*")
md_clean = md_clean.replace(r"\texttt{", "`")
md_clean = md_clean.replace(r"}", "")
md_clean = md_clean.replace(r"\begin{itemize}", "")
md_clean = md_clean.replace(r"\end{itemize}", "")
md_clean = md_clean.replace(r"\begin{enumerate}", "")
md_clean = md_clean.replace(r"\end{enumerate}", "")
md_clean = md_clean.replace(r"\item ", "* ")
md_clean = md_clean.replace(r"\begin{quote}", "")
md_clean = md_clean.replace(r"\end{quote}", "")
md_clean = md_clean.replace(r"\begin{verbatim}", "```cpp")
md_clean = md_clean.replace(r"\end{verbatim}", "```")
md_clean = md_clean.replace(r"\small", "")
md_clean = md_clean.replace(r"\begin{document}", "")
md_clean = md_clean.replace(r"\end{document}", "")

# Cabecera de Markdown
md_header = """# Guía Técnica Exhaustiva de Replicación: Motor Geofísico y 3D `gpr3d`
**Manual de Implementación por Sprints Atómicos en C++20 con Justificación Física y Referencias Universitarias en Alemán**

> **Documento compilado en PDF:** [`Guia_Replicacion_Paso_a_Paso_GPR3D.pdf`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/docs/Guia_Replicacion_Paso_a_Paso_GPR3D.pdf)
> **Código fuente LaTeX:** [`Guia_Replicacion_Paso_a_Paso_GPR3D.tex`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/docs/Guia_Replicacion_Paso_a_Paso_GPR3D.tex)

---
"""

# Guardar Markdown
with open(MD_PATH, "w", encoding="utf-8") as f:
    f.write(md_header + "\n" + full_tex)
print(f"-> Guia Markdown escrita en: {MD_PATH}")

shutil.copyfile(MD_PATH, ARTIFACT_PATH)
print(f"-> Artefacto actualizado en: {ARTIFACT_PATH}")

print("5. Limpieza de temporales LaTeX...")
for ext in [".aux", ".log", ".out", ".toc"]:
    f_path = os.path.join(DOCS_DIR, "Guia_Replicacion_Paso_a_Paso_GPR3D" + ext)
    if os.path.exists(f_path):
        os.remove(f_path)

print("¡Proceso completado exitosamente!")
