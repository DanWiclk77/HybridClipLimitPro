# Loudness Catalyst - VST3 Implementation Blueprint

## Arquitectura del Procesamiento (DSP)

Para el VST3 real en C++ (utilizando JUCE), el flujo en `processBlock` debe seguir este orden estricto para maximizar el loudness sin distorsión audible:

### 1. Clipper Stage (Standard Clip Analysis)
- **Oversampling:** Usar un filtro polifásico (polyphase filter) de 4x o 8x antes del clipping para evitar aliasing en las altas frecuencias.
- **Soft Clip Function:** Implementar una curva sigmoide configurable.
  ```cpp
  float applyClipper(float input, float knee) {
      if (abs(input) > 0.9f) {
           // Soft saturation logic
           return std::tanh(input * (1.0f + knee));
      }
      return input;
  }
  ```

### 2. Limiter Stage (Pro-L 2 Analysis)
- **Look-ahead:** Buffer circular de 0.5ms a 5.0ms para anticipar los transitorios.
- **TP (True Peak):** Interpolación Whittaker-Shannon para detectar picos entre muestras (inter-sample peaks).
- **Gain Envelope:** Ataque rápido, pero con un decaimiento (release) dependiente del programa (Adaptive Release).

## Innovación: "Target -6 LUFS" AI Algorithm

La función **Smart Target** que has solicitado funciona midiendo estadísticamente el audio de entrada y ajustando recursivamente:
1. **Input Gain:** Incrementa el volumen hasta que el RMS se acerque al objetivo.
2. **Clip Threshold:** Aplana los picos más agresivos para ganar un extra de 2-3 dB de "headroom" virtual.
3. **Limiter Ceiling:** Fija el techo final (ej. -0.1 dBTP).

---

## Estructura del Repositorio sugerida:
- `/Source`: Archivos .cpp y .h de JUCE.
- `/Resources`: Gráficos, fuentes.
- `/JuceLibraryCode`: SDK de JUCE.
- `/.github/workflows`: Los archivos YAML que te he proporcionado para compilar automáticamente en la nube.

## Ventajas Competitivas:
1. **Cero Latencia Opcional:** Modo para tracking vs Modo Master con Look-ahead.
2. **Visualización Espectral de GR:** Mostrar qué frecuencias están siendo más limitadas (similar a Pro-L2).
3. **Optimización SIMD:** Procesamiento paralelo en procesadores modernos para carga de CPU mínima.
