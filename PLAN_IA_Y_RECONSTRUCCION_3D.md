# Plan de Implementación: Reconstrucción 3D de Alta Fidelidad por Redes Neuronales y Evolución Industrial de gpr3d

## 1. Visión General y Objetivo
Evolucionar `gpr3d` desde la aproximación geométrica analítica actual (cilindros y elipsoides paramétricos ajustados por física determinista) hacia un **motor híbrido Física + Deep Learning (Physics-Informed Neural Network / Neural Signed Distance Fields)** capaz de:
1. Reconstruir la **silueta volumétrica real** de objetos enterrados con sus irregularidades, codos, uniones en T, cajas de empalme, deformaciones mecánicas y aplastamientos.
2. Lograr **super-resolución sub-longitud de onda** para desacoplar reflectores contiguos que se solapan en la física clásica.
3. Facilitar la exportación e interoperabilidad directa con entornos industriales de ingeniería civil y telecomunicaciones (**BIM/IFC, CAD/DXF 3D, C-Scans / Depth-Slices**).

---

## 2. Arquitectura del Módulo de Inteligencia Artificial

```
                      [ B-Scan Acondicionado ]
                                 │
                 ┌───────────────┴───────────────┐
                 ▼                               ▼
       [ Rama Física C++20 ]           [ Extracción de Parches ]
     (Semblance, Fresnel, ASTM)       (ROIs centrados en hipérbolas)
                 │                               │
                 │                               ▼
                 │                   [ Modelo de IA (ONNX Runtime) ]
                 │                   ┌─────────────────────────────┐
                 │                   │ • Encoder 2D/3D Convolucional│
                 │                   │ • Physics Loss (Ecuac. Onda)│
                 │                   │ • Decodificador Neural SDF  │
                 │                   └──────────────┬──────────────┘
                 │                                  │
                 ▼                                  ▼
      [ Propiedades Dieléctricas ]       [ Campo Escalar SDF 3D ]
    (eps_r, sigma, Material ASTM)                   │
                 │                                  ▼
                 │                      [ Marching Cubes C++20 ]
                 │                      (Malla triangular arbitraria)
                 └──────────────┬───────────────────┘
                                ▼
               [ Objeto Semántico 3D Enriquecido ]
             (Geometría real + Textura PBR geológica)
```

### Componentes de IA:
1. **Representación de Geometría por SDF Neuronal (Signed Distance Field)**:
   * En lugar de forzar un cilindro de radio constante $r$, la red modela la función continua $f_\theta(\mathbf{x}) = d$, donde $d$ es la distancia signada a la superficie del objeto.
   * La superficie real del objeto corresponde al nivel cero ($f_\theta(\mathbf{x}) = 0$).
   * Permite topologías arbitrarias: uniones, bifurcaciones de fibra, tapas de registro y deformaciones.
2. **Motor de Inferencia en C++20 (`ONNX Runtime C++ API`)**:
   * Despliegue sin dependencias de Python en producción.
   * Ejecución ultra rápida en CPU (x86_64 AVX2/AVX-512) o aceleración por GPU (CUDA/DirectML) si está disponible.
3. **Pérdida Guiada por la Física (PINN - Physics-Informed)**:
   * El entrenamiento en simulación sintética (`gprMax` + `SyntheticGenerator`) incluye la ecuación de onda electromagnética para penalizar formas físicamente incompatibles con los frentes de onda observados.

---

## 3. Fases de Implementación

### Fase 1: Pipeline de Datos de Entrenamiento y Generación de Gemelos Digitales (Simulación FDTD)
* **Objetivo**: Crear un dataset balanceado de pares `(Radargrama GPR <-> Malla 3D Ground Truth)`.
* **Acciones**:
  1. Extender `gpr3d::sim::SyntheticGenerator` para simular geometrías no canónicas: tuberías curvadas, conductos aplastados, codos a $90^\circ$ y $45^\circ$, juntas de dilatación y piedras irregulares.
  2. Automatizar puente de exportación con `gprMax` (simulador FDTD electromagnético en 3D) para casos con dispersión dieléctrica compleja.
  3. Generar 20,000 pares de entrenamiento con variabilidad de suelos (asfalto, arcilla, arena, grava).

### Fase 2: Arquitectura del Modelo de Red Neuronal (GPR-ShapeNet)
* **Objetivo**: Diseñar y entrenar la red para inferencia de forma.
* **Acciones**:
  1. Diseñar arquitectura híbrida:
     * **Backbone**: U-Net 3D ligera o ResNet con atención espacial para codificar la nube de reflexiones migradas locales.
     * **Head**: MLP implícito (Neural Implicit Surface) que predice la ocupación o el valor SDF en coordenadas $(x, y, z)$.
  2. Entrenamiento supervisado en PyTorch con funciones de pérdida combinadas:
     * Pérdida de superficie de Chamfer / Hausdorff.
     * Pérdida Eikonal $\|\nabla f(\mathbf{x})\| = 1$ para regularizar el campo SDF.
  3. Exportación y cuantización del modelo a formato estándar `.onnx` (FP16/INT8).

### Fase 3: Integración del Motor de Inferencia en C++20 (`gpr3d::ai`)
* **Objetivo**: Integrar la inferencia en el flujo de compilación de CMake y CLI.
* **Archivos a crear**:
  * `include/gpr3d/ai/shape_reconstructor.hpp`
  * `src/ai/shape_reconstructor.cpp`
  * `src/ai/marching_cubes.cpp` (algoritmo Marching Cubes en C++20 para extraer la malla poligonal desde el SDF predicho).
* **Acciones**:
  1. Añadir dependencia opcional o modular de `onnxruntime` en `CMakeLists.txt`.
  2. Implementar `gpr3d::ai::ShapeReconstructor::predict_mesh_from_roi(...)`.
  3. Sustituir en `SignalObjectTransformer` el generador de tubos paramétricos por la malla resultante del modelo cuando `--enable-ai-reconstruction` esté activo.

### Fase 4: Reconstrucción Volumétrica Multilínea y Time-Slice / Depth-Slice (C-Scan)
* **Objetivo**: Permitir visualización en planta (vista superior como en planos de ingeniería civil).
* **Acciones**:
  1. Algoritmo de interpolación volumétrica 3D regular (Kriging / Thin-plate splines) para grillas de múltiples pasadas.
  2. Generación automática de planos horizontales (*Depth-Slices*) cada 5 cm o 10 cm de profundidad.
  3. Actualización de `FluctuationPlotter` y el dashboard HTML con un control deslizante (*slider*) para navegar los cortes en planta superpuestos en mapas georreferenciados.

### Fase 5: Conectividad Industrial (BIM / IFC y CAD DXF 3D)
* **Objetivo**: Exportar a formatos estándar de ingeniería y arquitectura.
* **Acciones**:
  1. Diseñar exportador DXF 3D (`SceneExporter3D::write_dxf`) con capas independientes para cada material y polilíneas 3D.
  2. Soporte preliminar IFC 4.0 (`IfcFlowFitting`, `IfcDistributionFlowElement`) para importación directa en Autodesk Revit y Bentley MicroStation.
  3. Generador de reportes PDF de interferencias de excavación (*Excavation Risk Report*).

---

## 4. Cronograma Estimado de Hitos

| Hito | Entregable Clave | Tiempo Estimado |
|---|---|---|
| **M1: Dataset & FDTD** | Generador de geometrías arbitrarias y dataset sintético de 20k perfiles | 2 semanas |
| **M2: Entrenamiento de Red** | Modelo ONNX entrenado con error medio de reconstrucción < 8 mm | 3 semanas |
| **M3: Integración C++ / Inferencia**| `gpr3d transform --ai-shape` generando mallas no paramétricas | 2 semanas |
| **M4: Depth-Slice & Dashboard** | Slider de corte horizontal en planta en el dashboard web interactivo | 1.5 semanas |
| **M5: BIM / DXF & Verificación** | Exportación DXF 3D y validación con los datasets reales del repo | 1.5 semanas |

---

## 5. Criterios de Aceptación y Validación
1. **Precisión Geométrica**: La malla reconstruida por IA debe reducir el error de posición de borde en un $\ge 35\%$ comparado con el cilindro estático en presencia de codos o deformaciones.
2. **Latencia de Inferencia**: El procesamiento de un B-scan de 2000 trazas no debe superar los 2.5 segundos en CPU estándar multinúcleo.
3. **Resiliencia**: Si el módulo de IA no encuentra confianza suficiente ($< 50\%$), el sistema debe hacer *fallback* automático a la aproximación física canónica (cilindro analítico) sin fallar el proceso.
