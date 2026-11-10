# Guía Técnica y Plan de Implementación: Reconstrucción 3D de Formas Reales mediante Redes Neuronales y Neural SDF en `gpr3d`

> **Proyecto:** `SignalHandler` (`gpr3d::ai`)  
> **Estándar:** C++20 / ONNX Runtime C++ API / PyTorch PINN / gprMax FDTD  
> **Documento PDF Oficial:** [`Guia_Implementacion_Redes_Neuronales_3D.pdf`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/Guia_Implementacion_Redes_Neuronales_3D.pdf)  
> **Fecha:** Noviembre 2026  

---

## Índice General

1. [Visión General: El Salto de Formas Paramétricas a Geometrías Reales](#1-visión-general)
2. [Fundamentos Matemáticos de Neural SDF y Mapeo Académico Alemán](#2-fundamentos-matemáticos-y-mapeo-académico)
3. [Sprint IA-0: Infraestructura de Inferencia Nativa C++20 con ONNX Runtime](#sprint-ia-0-infraestructura-de-inferencia-nativa-c20-con-onnx-runtime)
4. [Sprint IA-1: Generador Sintético de Gemelos Digitales FDTD con gprMax](#sprint-ia-1-generador-sintético-de-gemelos-digitales-fdtd-con-gprmax)
5. [Sprint IA-2: Red 1 (GPR-DetNet) - Detector 3D de Regiones de Interés (ROI)](#sprint-ia-2-red-1-gpr-detnet---detector-3d-de-regiones-de-interés-roi)
6. [Sprint IA-3: Red 2 (GPR-DeepSDF) - Decodificador Implícito con Regularización Eikonal (PINN)](#sprint-ia-3-red-2-gpr-deepsdf---decodificador-implícito-de-distancia-signada-con-regularización-eikonal-pinn)
7. [Sprint IA-4: Extractor de Mallas Poligonales Marching Cubes C++20 con Normales Analíticas Exactas](#sprint-ia-4-extractor-de-mallas-poligonales-marching-cubes-c20-con-normales-analíticas-exactas)
8. [Sprint IA-5: Red 3 (LayerTransformer) - Segmentación y Reconstrucción de Horizontes Estratigráficos](#sprint-ia-5-red-3-layertransformer---segmentación-y-reconstrucción-de-horizontes-estratigráficos)
9. [Sprint IA-6: Integración en SignalObjectTransformer y Arquitectura Resiliente de Fallback](#sprint-ia-6-integración-en-signalobjecttransformer-y-arquitectura-resiliente-de-fallback)
10. [Sprint IA-7: Generador de Cortes Horizontales C-Scan (Depth-Slices) y Visualización Interactiva Three.js](#sprint-ia-7-generador-de-cortes-horizontales-c-scan-depth-slices-y-visualización-interactiva-threejs)
11. [Sprint IA-8: Conectividad Industrial DXF/IFC 4.0, Reporte de Interferencias y Validación Real](#sprint-ia-8-conectividad-industrial-dxfifc-40-reporte-de-interferencias-y-validación-con-radargramas-reales)
12. [Plan de Acción, Cronograma y Matriz de Entregables](#plan-de-acción-cronograma-y-matriz-de-entregables)

---

## 1. Visión General: El Salto de Formas Paramétricas a Geometrías Reales

El motor actual de `gpr3d` genera objetos tridimensionales utilizando primitivas analíticas geométricas: cilindros paramétricos de radio uniforme $r$ extruidos a lo largo de ejes de tubería (triedro de Frenet-Serret) y elipsoides regulares para anomalías puntuales.

Aunque este enfoque es matemáticamente elegante y robusto para conductos nuevos y perfectos, en la **infraestructura subterránea real** los objetos presentan morfologías heterogéneas y complejas:

1. **Deformaciones mecánicas y aplastamientos:** Tuberías plásticas de PVC/HDPE o conductos de alcantarillado que han colapsado elípticamente bajo el peso del tráfico vehicular o compactación de zanjas.
2. **Accesorios e interferencias angulares:** Codos a $45^\circ$ y $90^\circ$, derivaciones en T, reducciones de diámetro, válvulas y arquetas prismáticas de telecomunicaciones.
3. **Cuerpos y cavidades irregulares:** Socavones producidos por fugas de agua, oquedades con superficies rugosas y bloques de roca erráticos que envuelven las canalizaciones.
4. **Límite de resolución clásico de Rayleigh:** Dos conductos de fibra óptica colocados a menos de $5\text{ cm}$ de distancia se solapan difractivamente en el radargrama, apareciendo como una única masa informe en la física clásica.

### El Principio de Cooperación Híbrida (Física Determinista + PINN)
La red neuronal **no** debe aprender la física desde cero (lo cual requeriría millones de parámetros y resultaría inestable). El motor en C++20 le proporciona a la IA:
- La velocidad real de propagación $v$ y permitividad $\varepsilon_r$ obtenidas por análisis de semblanza (Sprint 9).
- La posición inicial aproximada de los reflectores colapsados mediante migración de Kirchhoff (Sprint 8).
- La polaridad de fase y la clasificación litológica de material ASTM (Sprint 9).

Con esta información contextual, la red neuronal se especializa exclusivamente en resolver la **morfología volumétrica fina a escala sub-longitud de onda**, prediciendo un campo de distancia signada continuo $f_\theta(\mathbf{x}) = d$.

---

## 2. Fundamentos Matemáticos y Mapeo Académico

### Mapeo a Literatura Universitaria Estándar
- **Mathematik (Tilo Arens et al.)**:
  - *Kapitel 24 & 25 (Flächen im Raum und Vektoranalysis)*: Superficies implícitas $F(x, y, z) = 0$, gradiente $\nabla F = (\frac{\partial F}{\partial x}, \frac{\partial F}{\partial y}, \frac{\partial F}{\partial z})$, y vector normal unitario $\mathbf{n} = \nabla F / \|\nabla F\|$.
  - *Kapitel 27 (Partielle Differentialgleichungen)*: Ecuación Eikonal no lineal $\|\nabla \Phi(\mathbf{x})\| = 1$, propagación de frentes de onda y distancias euclidianas mínimas.
  - *Kapitel 28 (Numerische Optimierung)*: Descenso de gradiente estocástico (SGD), mínimos cuadrados no lineales y regularización de Tikhonov.
- **Physik für Wissenschaftler und Ingenieure (Tipler & Mosca)**:
  - *Kapitel 15 (Wellen und Beugung)*: Dispersión de ondas electromagnéticas en obstáculos comparables a la longitud de onda ($\lambda = v / f$).
  - *Kapitel 31 (Optik)*: Criterio de Rayleigh para resolución límite angular y espacial ($\Delta x_{\min} \approx 0.61 \lambda / \text{NA}$).
- **Literatura Fundamental de Deep Learning y Redes Guiadas por la Física (PINN)**:
  - *Goodfellow, Bengio & Courville (Deep Learning, MIT Press)*: Convoluciones 3D, funciones de activación continuas (GELU, Softplus, SIREN) y optimizador AdamW.
  - *Raissi, Perdikaris & Karniadakis (2019, Physics-Informed Neural Networks)*: Penalización de residuos de EDPs en la función de pérdida mediante diferenciación automática.
  - *Gropp et al. (2020, Implicit Geometric Regularization - IGR)*: Regularización geométrica implícita mediante la condición eikonal $\|\nabla_\mathbf{x} f_\theta(\mathbf{x})\| = 1$.
  - *Park et al. (2019, DeepSDF)*: Representación continua de formas 3D condicionadas por vectores latentes.
- **C++ professionell anwenden (Ulrich Breymann)**:
  - *Kapitel 7, 8 & 11*: Gestión de memoria continua en C++20 con `std::span`, tensores planos sin copias intermedias y estructuras de datos para mallas poligonales.
  - *Kapitel 18 (C-Bibliotheken)*: Integración del runtime de inferencia nativo `onnxruntime-cxx` con punteros inteligentes RAII y ejecución sin intérprete de Python.

### La Representación Geométrica por Neural SDF
En lugar de discretizar el espacio en cuadrículas cúbicas de vóxeles, una **Función de Distancia Signada Neuronal (Neural SDF)** modela el objeto como un campo continuo parametrizado por una red neuronal profunda $f_\theta$:
$$f_\theta : \mathbb{R}^3 \times \mathbb{R}^k \to \mathbb{R}, \qquad f_\theta(\mathbf{x}, \mathbf{z}) = d$$
donde $\mathbf{x} = (x, y, z)$ es un punto de consulta en metros, $\mathbf{z} \in \mathbb{R}^{128}$ es el vector latente que describe las características del objeto detectado en el radargrama, y $d$ es la distancia euclidiana signada:
$$d = \begin{cases} < 0 & \text{si } \mathbf{x} \text{ está en el interior del objeto (ej. núcleo del tubo)} \\ = 0 & \text{si } \mathbf{x} \text{ está sobre la superficie o pared física real} \\ > 0 & \text{si } \mathbf{x} \text{ está en el exterior (ej. suelo o estrato circundante)} \end{cases}$$

### Propiedades Matemáticas Clave
1. **Resolución Infinita:** Al ser una función continua $f_\theta$, no existe un tamaño mínimo de píxel/vóxel. Se puede evaluar la malla con cualquier paso arbitrario ($5\text{ mm}$ o $1\text{ mm}$).
2. **Topología Arbitraria sin Costuras:** Modela codos, derivaciones en T, arquetas o socavones irregulares sin desgarros de malla.
3. **Cálculo Analítico Exacto de Normales por Gradiente:** La normal unitaria exterior se obtiene directamente evaluando el gradiente del campo SDF:
   $$\mathbf{n}(\mathbf{x}) = \frac{\nabla_\mathbf{x} f_\theta(\mathbf{x})}{\|\nabla_\mathbf{x} f_\theta(\mathbf{x})\|}$$

---

## Sprint IA-0: Infraestructura de Inferencia Nativa C++20 con ONNX Runtime

### 1. Qué Estudiar en la Literatura Técnica
- **Breymann Kapitel 7 & 8**: Gestión RAII de entornos de ejecución nativos (`Ort::Env`, `Ort::Session`).
- **Breymann Kapitel 11**: Uso de `std::span<const float>` para referenciar tensores planos sin copias.
- **Breymann Kapitel 18**: Vinculación estática/dinámica con la API en C de `onnxruntime`.

### 2. Por Qué Hay que Implementarlo
En el entorno de producción (tablets de campo robustecidas, vehículos de inspección):
1. **Latencia y Determinismo:** Elimina el Global Interpreter Lock (GIL) y el recolector de basura de Python.
2. **Huella de Memoria:** Reduce el consumo de RAM de $>1.2\text{ GB}$ (PyTorch) a menos de $35\text{ MB}$ (ONNX nativo).
3. **Despliegue Cero Dependencias:** `gpr3d` se distribuye como un binario único ejecutable sin requerir Conda o pip.

### 3. Tareas Paso a Paso de Implementación
1. **Crear cabecera** [`include/gpr3d/ai/inference_engine.hpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/include/gpr3d/ai/inference_engine.hpp):
   - Estructura `TensorShape` con cálculo de elementos totales.
   - Clase `OnnxInferenceEngine` con método `run(std::span<const float>, TensorShape, ...)`.
   - Consulta de formas de entrada y salida (`get_input_shape`, `get_output_shape`).
2. **Implementar** [`src/ai/inference_engine.cpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/src/ai/inference_engine.cpp):
   - Inicialización RAII de `Ort::Env` y `Ort::SessionOptions`.
   - Creación de tensores zero-copy con `Ort::Value::CreateTensor<float>`.
   - Enlace multihilo con `SetIntraOpNumThreads(4)`.
3. **Configurar CMakeLists.txt**: añadir módulo `gpr3d_ai` enlazado con `onnxruntime`.

### 4. Prueba de Verificación Numérica
- Implementar `test/test_ai_inference_engine.cpp`.
- Evaluar una operación lineal sintética $y = 2.0x + 1.0$ sobre vector $\{1.0, 2.0, 3.0\}$.
- **Criterio:** Salida $\{3.0, 5.0, 7.0\}$ con error $< 10^{-6}$ y latencia $< 0.5\text{ ms}$.

---

## Sprint IA-1: Generador Sintético de Gemelos Digitales FDTD con gprMax

### 1. Qué Estudiar en la Literatura Técnica
- **Tipler & Mosca Kapitel 20**: Ecuaciones de Maxwell en el dominio del tiempo y discretización de Yee en 3D.
- **Arens Kapitel 27**: Condición de estabilidad Courant-Friedrichs-Lewy (CFL):
  $$\Delta t \le \frac{1}{c \sqrt{1/\Delta x^2 + 1/\Delta y^2 + 1/\Delta z^2}}$$
- **Documentación de gprMax**: Capas absorbentes PML y modelos fractales de rugosidad edáfica de Peplinski.

### 2. Por Qué Hay que Implementarlo
1. **Imposibilidad de Excavación Destructiva Continua:** No es factible excavar zanjas destructivas en vías públicas para escanear con láser 3D la forma de cada tubería real solo para etiquetar datos.
2. **Representatividad de Defectos Críticos:** Los casos más peligrosos (tuberías aplastadas por el tráfico, socavones oquedades, codos oblicuos) son raros en inspecciones rutinarias pero críticos de detectar.
3. **Gemelo Digital FDTD Riguroso:** Generar 15,000 pares de entrenamiento simulando la física electromagnética completa garantiza un dataset de entrenamiento supervisado masivo y exacto.

### 3. Tareas Paso a Paso de Implementación
1. **Desarrollar generador procedural CAD** `scripts/ai/procedural_cad_generator.py`:
   - Funciones `create_deformed_pipe(length, radius, crush_ratio, bend_angle)` y `create_junction_mesh()`.
   - Exportación de mallas STL/OBJ compatibles con gprMax.
2. **Automatizar simulaciones batch** `scripts/ai/gprmax_batch_runner.py`:
   - Generación de archivos de entrada `.in` con antenas Ricker de $800\text{ MHz}$, capas PML y suelos heterogéneos ($\varepsilon_r \in [3, 25]$, $\sigma \in [10^{-4}, 10^{-1}]\text{ S/m}$).
3. **Construir dataset unificado HDF5** `scripts/ai/dataset_builder.py` con volúmenes de radar y campos SDF de referencia.

### 4. Prueba de Verificación Numérica
- Simular un tubo de $\varnothing 160\text{ mm}$ a $0.80\text{ m}$ de profundidad en suelo con $\varepsilon_r = 9.0$.
- Tiempo de arribo teórico de doble vía: $v = c/\sqrt{9.0} = 0.10\text{ m/ns} \implies t = 2 \times 0.80 / 0.10 = 16.0\text{ ns}$.
- **Criterio:** Pico de reflexión principal en $t = 16.0 \pm 0.15\text{ ns}$.

---

## Sprint IA-2: Red 1 (GPR-DetNet) - Detector 3D de Regiones de Interés (ROI)

### 1. Qué Estudiar en la Literatura Técnica
- **Goodfellow Kapitel 9**: Convoluciones 3D para volúmenes $(x, y, t)$, 3D Max Pooling y Batch Normalization.
- **Focal Loss**: $\mathcal{L}_{\text{focal}}(p_t) = -\alpha_t (1 - p_t)^\gamma \log(p_t)$ para balancear el 95% de suelo homogéneo vacío frente a anomalías.
- **Arens Kapitel 25**: Cálculo de intersección volumétrica 3D IoU entre prismas rectangulares.

### 2. Por Qué Hay que Implementarlo
1. **Inviabilidad Computacional Global:** Evaluar un campo neuronal continuo SDF en todo el volumen ($20\text{m} \times 10\text{m} \times 2.5\text{m}$) requeriría evaluar la red $>2 \times 10^8$ veces.
2. **Estrategia Coarse-to-Fine:** GPR-DetNet procesa el volumen migrado en milisegundos y acota las cajas delimitadoras 3D $[x_{\min}, y_{\min}, z_{\min}, x_{\max}, y_{\max}, z_{\max}]$ con su clasificación de objeto.
3. **Reducción del 98.5% en Consultas:** El motor C++ solo evalúa Neural SDF dentro de las cajas detectadas, logrando reconstrucción interactiva en menos de $1.5\text{ segundos}$.

### 3. Tareas Paso a Paso de Implementación
1. **Definir arquitectura PyTorch** `scripts/ai/models/gpr_detnet.py`:
   - Bloques residuales 3D (`Conv3d` + `BatchNorm3d` + `GELU`).
   - Cabezal de clasificación (4 clases: Tubo, Unión/Codo, Arqueta, Cavidad/Oquedad).
   - Cabezal de regresión de cajas 3D (7 parámetros: centro $x,y,z$, dimensiones $w,h,d$ y ángulo de orientación $\theta_{\text{yaw}}$).
2. **Exportar a ONNX con perfil dinámico** `scripts/ai/export_detnet_onnx.py`.
3. **Implementar cliente C++20** [`include/gpr3d/ai/detnet_detector.hpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/include/gpr3d/ai/detnet_detector.hpp) y [`src/ai/detnet_detector.cpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/src/ai/detnet_detector.cpp) con filtrado Non-Maximum Suppression (NMS) 3D.

### 4. Prueba de Verificación Numérica
- Generar volumen sintético $64^3$ con reflector en $[0.4, 0.6]\text{ m} \times [0.8, 1.2]\text{ m} \times [0.5, 0.7]\text{ m}$.
- **Criterio:** Exactamente una caja detectada con clase `PIPE`, confianza $>85\%$ y error de centroide $< \pm 3\text{ cm}$.

---

## Sprint IA-3: Red 2 (GPR-DeepSDF) - Decodificador Implícito de Distancia Signada con Regularización Eikonal (PINN)

### 1. Qué Estudiar en la Literatura Técnica
- **Raissi et al. (2019, PINN)**: Diferenciación automática del grafo con `torch.autograd.grad`.
- **Gropp et al. (2020, IGR)**: Pérdida Eikonal:
  $$\mathcal{L}_{\text{eik}} = \frac{1}{N} \sum_{i=1}^N \left( \|\nabla_\mathbf{x} f_\theta(\mathbf{x}_i, \mathbf{z})\|_2 - 1 \right)^2$$
- **Park et al. (2019, DeepSDF)**: Decodificador MLP condicionado por vector latente $\mathbf{z} \in \mathbb{R}^{128}$.

### 2. Por Qué Hay que Implementarlo
1. **Superación del Límite de Vóxeles:** Memoria continua $O(1)$ sin celdas discretas cúbicas $O(N^3)$.
2. **Inmunidad al Ruido Radar:** Las redes no guiadas por la física producen superficies rugosas y con agujeros no físicos ante ecos espurios. La condición eikonal garantiza superficies lisas y matemáticamente consistentes.
3. **Morfología Continua de Objetos Reales:** Modela tubos aplastados, codos a $45^\circ$ o $90^\circ$ y derivaciones en T con fidelidad sub-milimétrica.

### 3. Tareas Paso a Paso de Implementación
1. **Implementar arquitectura DeepSDF** `scripts/ai/models/deep_sdf_decoder.py`:
   - 8 capas lineales con `Softplus(\beta=100)` y skip-connection en la capa 4.
2. **Bucle de entrenamiento PINN** `scripts/ai/train_deep_sdf.py`:
   - Pérdida combinada: $\mathcal{L} = \mathcal{L}_{\text{sdf}} + \lambda_{\text{eik}} \mathcal{L}_{\text{eik}}$ con $\lambda_{\text{eik}} = 0.1$.
3. **Exportar a ONNX y crear cliente C++20** [`include/gpr3d/ai/sdf_evaluator.hpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/include/gpr3d/ai/sdf_evaluator.hpp) y [`src/ai/sdf_evaluator.cpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/src/ai/sdf_evaluator.cpp).

### 4. Prueba de Verificación Numérica
- Evaluar modelo sobre conducto cilíndrico de radio teórico $r = 100\text{ mm}$ en el origen.
- Evaluar puntos: interior $(0,0,0) \implies -0.10\text{ m}$, pared $(0.1,0,0) \implies 0.00\text{ m}$, exterior $(0.15,0,0) \implies +0.05\text{ m}$.
- **Criterio:** Error de distancias $< 3\text{ mm}$ y $\|\nabla f_\theta\| = 1.0 \pm 0.04$.

---

## Sprint IA-4: Extractor de Mallas Poligonales Marching Cubes C++20 con Normales Analíticas Exactas

### 1. Qué Estudiar en la Literatura Técnica
- **Lorensen & Cline (1987)**: Tablas de 256 configuraciones cúbicas (`edgeTable`, `triTable`).
- **Arens Kapitel 24 & 25**: Interpolación lineal en aristas:
  $$\mathbf{P}_{\text{cruce}} = \mathbf{P}_1 + \frac{0 - d_1}{d_2 - d_1} (\mathbf{P}_2 - \mathbf{P}_1)$$
  y vector normal unitario exacto $\mathbf{n}(\mathbf{P}) = \nabla f_\theta / \|\nabla f_\theta\|$.
- **Breymann Kapitel 8**: Estructura contigua `MeshData3D` para vértices, normales e índices.

### 2. Por Qué Hay que Implementarlo
1. **Del Campo Continuo al Estándar Industrial:** Motores de renderizado (Three.js), formatos 3D (glTF/GLB) y CAD (AutoCAD DXF) exigen polígonos triangulares indexados.
2. **Normales Analíticas Suaves sin Ruido de Malla:** Calcular normales mediante el producto vectorial $(\mathbf{v}_2 - \mathbf{v}_1) \times (\mathbf{v}_3 - \mathbf{v}_1)$ amplifica el ruido. Calcularlas mediante el gradiente de la red por diferencias finitas centrales ($h = 10^{-4}\text{ m}$) produce un sombreado PBR perfecto sin necesidad de filtros laplacianos que encogen el volumen.

### 3. Tareas Paso a Paso de Implementación
1. **Definir estructuras y cabecera** [`include/gpr3d/ai/marching_cubes.hpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/include/gpr3d/ai/marching_cubes.hpp):
   - Estructura `MeshData3D` con buffers planos alineados.
   - Clase `MarchingCubesExtractor` con función de extracción adaptativa.
2. **Implementar extractor** [`src/ai/marching_cubes.cpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/src/ai/marching_cubes.cpp) con tablas de aristas e interpolación exacta de cruce por cero.

### 4. Prueba de Verificación Numérica
- Extraer malla de una esfera teórica $R = 0.25\text{ m}$ con paso de celda $\Delta = 0.01\text{ m}$.
- **Criterio:** Error máximo de radio en vértices $|r - 0.25| < 0.001\text{ m}$ ($1\text{ mm}$) y desviación angular media de normales $< 0.4^\circ$.

---

## Sprint IA-5: Red 3 (LayerTransformer) - Segmentación y Reconstrucción de Horizontes Estratigráficos

### 1. Qué Estudiar en la Literatura Técnica
- **Vaswani et al. (2017)**: Auto-atención y codificación posicional para secuencias espaciales.
- **Tipler Kapitel 15**: Reflexión en interfaces dieléctricas estratificadas de suelo.
- **Arens Kapitel 24**: Superficies de cota continua $z = h_k(x, y)$ y regularización laplaciana.

### 2. Por Qué Hay que Implementarlo
1. **Robustez ante Sombras y Difracciones:** El seguimiento clásico de líneas se rompe cuando una tubería cruza el estrato generando hipérbolas difractivas.
2. **Auto-Atención Global:** El Transformer interpola suavemente a través de zonas ocluidas o con sombras electromagnéticas.
3. **Restricción Física de No-Intersección:** La pérdida penaliza cruces geológicamente imposibles:
   $$\mathcal{L}_{\text{mono}} = \sum_{k=1}^{K-1} \max(0, \; h_k(x,y) - h_{k+1}(x,y) + \delta_{\min})$$

### 3. Tareas Paso a Paso de Implementación
1. **Crear modelo PyTorch** `scripts/ai/models/layer_transformer.py`.
2. **Implementar módulo C++20** [`include/gpr3d/ai/layer_reconstructor.hpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/include/gpr3d/ai/layer_reconstructor.hpp) y [`src/ai/layer_reconstructor.cpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/src/ai/layer_reconstructor.cpp).

### 4. Prueba de Verificación Numérica
- B-scan de 100 trazas con 2 capas teóricas a $0.25\text{ m}$ y $0.65\text{ m}$, con hueco ciego en trazas 40-55.
- **Criterio:** En el hueco, error de cota $< \pm 15\text{ mm}$ y separación $z_2 - z_1 \ge 0.35\text{ m}$ en todas las trazas.

---

## Sprint IA-6: Integración en SignalObjectTransformer y Arquitectura Resiliente de Fallback

### 1. Qué Estudiar en la Literatura Técnica
- **Breymann Kapitel 6 & 14**: Patrón Estrategia, seguridad ante excepciones y C++20 Concepts.
- **Arens Kapitel 24**: Geometría diferencial de curvas alabeadas.

### 2. Por Qué Hay que Implementarlo
1. **Seguridad Crítica en Obra Civil:** Una alucinación de la IA en una red de gas o media tensión puede causar accidentes mortales.
2. **Fallback Automático Transparente:**
   $$\text{Confianza} = 0.5 \cdot \text{Score}_{\text{DetNet}} + 0.5 \cdot (1 - \min(1.0, \text{ErrorEikonal}))$$
   Si $\text{Confianza} < 50\%$, el sistema conmuta instantáneamente al cilindro analítico determinista de Frenet-Serret (Sprint 10) sin interrumpir la operación.

### 3. Tareas Paso a Paso de Implementación
1. **Actualizar cabecera** [`include/gpr3d/geometry/signal_object_transformer.hpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/include/gpr3d/geometry/signal_object_transformer.hpp) con enum `ReconstructionMode { DeterministicOnly, NeuralSDFOnly, HybridAutoFallback }`.
2. **Actualizar lógica de transformación** [`src/geometry/signal_object_transformer.cpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/src/geometry/signal_object_transformer.cpp).

### 4. Prueba de Verificación Numérica
- Inyectar radargrama deliberadamente degradado ($	ext{SNR} = -10\text{ dB}$, confianza $\approx 22\%$).
- **Criterio:** Fallback limpio con `is_neural_reconstruction = false`, generando cilindro de respaldo sin excepciones.

---

## Sprint IA-7: Generador de Cortes Horizontales C-Scan (Depth-Slices) y Visualización Interactiva Three.js

### 1. Qué Estudiar en la Literatura Técnica
- **Arens Kapitel 24**: Interpolación bilineal en planos de corte $z = z_0$.
- **Three.js API**: Planos de textura semitransparentes, mapas de color viridis/jet y sliders dinámicos.

### 2. Por Qué Hay que Implementarlo
1. **Validación Visual por el Perito Geotécnico:** Los operadores exigen cotejar la malla 3D contra la rebanada horizontal de energía radar (C-Scan) a la misma profundidad.
2. **Exploración Estratigráfica Interactiva:** Desplazar un plano de corte continuo en Three.js cortando tuberías y estratos en tiempo real.

### 3. Tareas Paso a Paso de Implementación
1. **Crear rasterizador C++20** [`include/gpr3d/processing/cscan_rasterizer.hpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/include/gpr3d/processing/cscan_rasterizer.hpp) y [`src/processing/cscan_rasterizer.cpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/src/processing/cscan_rasterizer.cpp).
2. **Actualizar Three.js** [`dashboard/index.html`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/dashboard/index.html) añadiendo plano de corte con textura C-Scan y slider de elevación.

### 4. Prueba de Verificación Numérica
- Volumen sintético con reflector en $(30, 70, 45)$.
- **Criterio:** Píxel de brillo máximo en $(30, 70) \pm 0$ píxeles y canal alfa $255$.

---

## Sprint IA-8: Conectividad Industrial DXF/IFC 4.0, Reporte de Interferencias y Validación Real

### 1. Qué Estudiar en la Literatura Técnica
- **Norma ISO 16739-1:2018 (IFC 4.0)**: Entidades `IfcTriangulatedFaceSet`, `IfcFlowFitting` e `IfcDistributionFlowElement`.
- **AutoCAD DXF Reference**: Entidades 3D `MESH` y `3DFACE`.

### 2. Por Qué Hay que Implementarlo
1. **Entrega BIM Oficial:** Los clientes finales (empresas de gas, telecomunicaciones, municipios) exigen archivos `.ifc` y `.dxf` para Autodesk Revit, AutoCAD Civil 3D y QGIS.
2. **Detección Automática de Colisiones (Clash Detection):** Comprobación obligatoria por ley de distancias mínimas ($d \ge 0.30\text{ m}$) entre conducciones subterráneas antes de excavar.
3. **Validación Integral sobre Datos Reales:** Verificación de robustez sobre los radargramas de campo del repositorio: `parking.nx15.dzt` y `ds010_A.DZT`.

### 3. Tareas Paso a Paso de Implementación
1. **Exportador IFC 4.0** [`include/gpr3d/export/ifc_exporter.hpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/include/gpr3d/export/ifc_exporter.hpp) y [`src/export/ifc_exporter.cpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/src/export/ifc_exporter.cpp).
2. **Detector de interferencias** [`include/gpr3d/analysis/interference_detector.hpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/include/gpr3d/analysis/interference_detector.hpp) y [`src/analysis/interference_detector.cpp`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/src/analysis/interference_detector.cpp).
3. **Script de validación de campo** `scripts/ai/validate_real_dzt.py` sobre `parking.nx15.dzt`.

### 4. Prueba de Verificación Numérica
- Colisión sintética entre tubos a $0.22\text{ m}$ reportada como evento crítico con error de distancia $< 5\text{ mm}$.
- Ejecución completa sobre `parking.nx15.dzt`: generación de `parking_mesh.ifc` y `parking_mesh.dxf` con cabecera STEP válida y $>500$ triángulos sin valores `NaN`.

---

## Plan de Acción, Cronograma y Matriz de Entregables

| Fase / Sprint | Objetivo Principal y Entregables de Código | Archivos Clave C++ / Python | Criterio de Éxito |
| :--- | :--- | :--- | :--- |
| **Sprint IA-0** | Motor de inferencia autónomo en C++20 con ONNX Runtime sin dependencias de Python | `inference_engine.hpp`<br>`inference_engine.cpp` | Latencia $< 0.5\text{ ms}$, Cero copias (`std::span`) |
| **Sprint IA-1** | Generador sintético procedural CAD y simulaciones FDTD batch con gprMax | `procedural_cad.py`<br>`gprmax_batch.py` | 15,000 pares HDF5, Error de pico $< 0.15\text{ ns}$ |
| **Sprint IA-2** | Red 1 (GPR-DetNet) para detección y delimitación de cajas envolventes 3D | `gpr_detnet.py`<br>`detnet_detector.cpp` | IoU 3D $\ge 0.78$, Confianza $> 85\%$ |
| **Sprint IA-3** | Red 2 (GPR-DeepSDF) con decodificador continuo y regularización Eikonal PINN | `deep_sdf_decoder.py`<br>`sdf_evaluator.cpp` | $\|\nabla f_\theta\| \approx 1 \pm 0.04$, Error dist. $< 3\text{ mm}$ |
| **Sprint IA-4** | Extractor de mallas Marching Cubes C++20 con normales analíticas exactas | `marching_cubes.hpp`<br>`marching_cubes.cpp` | Error radio $< 1\text{ mm}$, Desv. normal $< 0.4^\circ$ |
| **Sprint IA-5** | Red 3 (LayerTransformer) para reconstrucción de horizontes estratigráficos | `layer_transformer.py`<br>`layer_reconstructor.cpp` | Capas sin cruces ($h_1 < h_2 < h_3$), MAE $< 15\text{ mm}$ |
| **Sprint IA-6** | Conmutador resiliente de fallback en `SignalObjectTransformer` | `signal_object_transformer.cpp` | Fallback transparente si Confianza $< 50\%$ |
| **Sprint IA-7** | Rebanadas horizontales C-Scan y visualizador interactivo con slider en Three.js | `cscan_rasterizer.cpp`<br>`dashboard/index.html` | Precisión de píxel $\pm 0$, opacidad continua |
| **Sprint IA-8** | Exportación industrial IFC 4.0/DXF, reporte de colisiones y validación real | `ifc_exporter.cpp`<br>`interference_detector.cpp` | Pipeline validado en `parking.nx15.dzt` |

---

### Documentación y Recursos Generados en el Repositorio
- Documento PDF Oficial Compilado: [`Guia_Implementacion_Redes_Neuronales_3D.pdf`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/Guia_Implementacion_Redes_Neuronales_3D.pdf) (30 páginas, formato A4)
- Código Fuente LaTeX: [`docs/Guia_Implementacion_Redes_Neuronales_3D.tex`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/docs/Guia_Implementacion_Redes_Neuronales_3D.tex)
- Diagramas Vectoriales:
  - Arquitectura General de IA: [`docs/diagrams/ai_arquitectura_reconstruccion.pdf`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/docs/diagrams/ai_arquitectura_reconstruccion.pdf)
  - Concepto Matemático de Neural SDF: [`docs/diagrams/neural_sdf_concepto.pdf`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/docs/diagrams/neural_sdf_concepto.pdf)
  - Flujo de Entrenamiento PINN + FDTD: [`docs/diagrams/pipeline_entrenamiento_ia.pdf`](file:///home/luis/Documents/Projects/Glasfaser/SignalHandler/docs/diagrams/pipeline_entrenamiento_ia.pdf)
