#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Compilador maestro de la Guia Tecnica y Plan de Implementacion de Redes Neuronales 3D."""

import os
import subprocess
import shutil
import re

DOCS_DIR = "/home/luis/Documents/Projects/Glasfaser/SignalHandler/docs"
TEX_PATH = os.path.join(DOCS_DIR, "Guia_Implementacion_Redes_Neuronales_3D.tex")
PDF_PATH = os.path.join(DOCS_DIR, "Guia_Implementacion_Redes_Neuronales_3D.pdf")
ROOT_PDF = "/home/luis/Documents/Projects/Glasfaser/SignalHandler/Guia_Implementacion_Redes_Neuronales_3D.pdf"
MD_PATH = os.path.join(DOCS_DIR, "GUIA_IMPLEMENTACION_REDES_NEURONALES_3D.md")
ARTIFACT_PATH = "/home/luis/.gemini/antigravity/brain/3eb84fe1-79d3-4ab0-a519-bf37c63fc6ea/plan_redes_neuronales_reconstruccion_3d.md"

import sys
sys.path.append(DOCS_DIR)

from ai_content_intro import get_ai_latex_header_and_intro
from ai_content_sprints_0_3 import get_ai_sprints_0_to_3
from ai_content_sprints_4_8 import get_ai_sprints_4_to_8

def assemble_latex():
    part1 = get_ai_latex_header_and_intro()
    part2 = get_ai_sprints_0_to_3()
    part3 = get_ai_sprints_4_to_8()
    return part1 + "\n" + part2 + "\n" + part3

def compile_pdf():
    tex_content = assemble_latex()
    with open(TEX_PATH, "w", encoding="utf-8") as f:
        f.write(tex_content)
    print(f"Archivo LaTeX generado: {TEX_PATH} ({len(tex_content)} caracteres)")

    # Compilar con pdflatex 2 pasadas
    for pass_num in (1, 2):
        print(f"Ejecutando pdflatex (Pasada {pass_num}/2)...")
        res = subprocess.run(
            ["pdflatex", "-interaction=nonstopmode", "-halt-on-error", os.path.basename(TEX_PATH)],
            cwd=DOCS_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            errors='replace'
        )
        if res.returncode != 0:
            print("ERROR al compilar LaTeX:")
            lines = res.stdout.splitlines()
            for line in lines[-50:]:
                print(line)
            return False

    if os.path.exists(PDF_PATH):
        pdf_size = os.path.getsize(PDF_PATH)
        print(f"PDF generado exitosamente: {PDF_PATH} ({pdf_size} bytes)")
        shutil.copy2(PDF_PATH, ROOT_PDF)
        print(f"Copiado a la raiz del proyecto: {ROOT_PDF}")
        return True
    return False

def generate_markdown(tex_content):
    md = "# Guía Técnica y Plan de Implementación: Reconstrucción 3D por Redes Neuronales y Neural SDF en gpr3d\n\n"
    md += "> **Proyecto:** `SignalHandler` (`gpr3d::ai`)  \n"
    md += "> **Estándar:** C++20 / ONNX Runtime C++ API / PyTorch PINN / gprMax FDTD  \n"
    md += "> **Documento PDF Oficial:** [`Guia_Implementacion_Redes_Neuronales_3D.pdf`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/Guia_Implementacion_Redes_Neuronales_3D.pdf)  \n\n"
    md += "---\n\n"

    # Conversion simplificada de LaTeX a Markdown
    text = tex_content
    # Quitar preambulo
    if r"\begin{document}" in text:
        text = text.split(r"\begin{document}")[1]
    if r"\end{document}" in text:
        text = text.split(r"\end{document}")[0]

    # Reemplazar secciones
    text = re.sub(r'\\section\{([^}]+)\}', r'\n## \1\n', text)
    text = re.sub(r'\\subsection\{([^}]+)\}', r'\n### \1\n', text)
    text = re.sub(r'\\subsubsection\{([^}]+)\}', r'\n#### \1\n', text)

    # Reemplazar entornos verbatim
    text = re.sub(r'\\begin\{verbatim\}', r'```cpp', text)
    text = re.sub(r'\\end\{verbatim\}', r'```', text)

    # Reemplazar estilos
    text = re.sub(r'\\textbf\{([^}]+)\}', r'**\1**', text)
    text = re.sub(r'\\textit\{([^}]+)\}', r'*\1*', text)
    text = re.sub(r'\\texttt\{([^}]+)\}', r'`\1`', text)

    # Listas
    text = re.sub(r'\\begin\{itemize\}', '', text)
    text = re.sub(r'\\end\{itemize\}', '', text)
    text = re.sub(r'\\begin\{enumerate\}', '', text)
    text = re.sub(r'\\end\{enumerate\}', '', text)
    text = re.sub(r'\\item\s*', r'- ', text)

    # Limpieza de caracteres de escape LaTeX
    text = text.replace(r'\_', '_')
    text = text.replace(r'\%', '%')
    text = text.replace(r'\&', '&')
    text = text.replace(r'\{', '{')
    text = text.replace(r'\}', '}')
    text = text.replace(r'\\', '')
    text = text.replace(r'\newpage', '\n---\n')

    # Reemplazar acentos latex
    text = text.replace(r"{\'a}", "á").replace(r"{\'e}", "é").replace(r"{\'i}", "í")
    text = text.replace(r"{\'o}", "ó").replace(r"{\'u}", "ú").replace(r"{\~n}", "ñ")
    text = text.replace(r'{\"a}', "ä").replace(r'{\"o}', "ö").replace(r'{\"u}', "ü")
    text = text.replace(r"{\'\i}", "í")

    md += text
    return md

if __name__ == "__main__":
    success = compile_pdf()
    if success:
        with open(TEX_PATH, "r", encoding="utf-8") as f:
            tex_content = f.read()
        md_content = generate_markdown(tex_content)
        with open(MD_PATH, "w", encoding="utf-8") as f:
            f.write(md_content)
        print(f"Documento Markdown generado: {MD_PATH} ({len(md_content)} caracteres)")
        with open(ARTIFACT_PATH, "w", encoding="utf-8") as f:
            f.write(md_content)
        print(f"Artifact actualizado: {ARTIFACT_PATH}")
        print("PROCESO COMPLETADO EXITOSAMENTE.")
    else:
        print("ERROR EN EL PROCESO DE COMPILACION.")
