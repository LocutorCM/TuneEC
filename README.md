# TunEC

Plugin de corrección de pitch en tiempo real para **macOS** (AU / VST3 / Standalone) hecho con JUCE 8.
Motor de detección y corrección **original** (YIN + resíntesis por granos sincronizados al pitch), sin código de terceros.

## Controles

| Sección | Controles |
|---|---|
| Tuning | Input Type, Key, Scale (14 escalas + Custom), Retune Speed (0–400 ms), Flex Tune, Natural Vibrato, Humanize, Ignore Vibrato, Master Bypass |
| Formant & Pitch | Formant Correction, Throat Length, Transpose (±24 st), Detune (±100 cents) |
| MIDI | Target Notes via MIDI, Learn Scale from MIDI, MIDI Octaves (All / Match) |
| Vibrato | Shape, Rate, Onset Delay, Onset Rate, Variation, Pitch, Amplitude, Formant |

Monitor de pitch con traza de entrada/salida, nota detectada, cents y teclado de escala activa.

## Compatibilidad

- macOS **10.13 High Sierra en adelante** (mínimo que soporta JUCE 8), hasta el macOS más reciente.
- Binario universal: Intel (x86_64) y Apple Silicon (arm64).
- Formatos: Audio Unit (`.component`), VST3 y app Standalone.

## Compilar en GitHub

1. Sube todo el contenido de la carpeta a un repositorio (incluida `.github/`).
2. En **Actions** se ejecuta *Build TunEC (macOS)*. Descarga `TunEC-macOS.zip` en Artifacts. Para un release: `git tag v1.0.0 && git push --tags`.

## Instalar

- AU: `~/Library/Audio/Plug-Ins/Components/`
- VST3: `~/Library/Audio/Plug-Ins/VST3/`
- Los binarios llevan firma ad-hoc, no notarizada. Si macOS los bloquea: `xattr -dr com.apple.quarantine <archivo>` o clic derecho > Abrir. Para distribuir públicamente necesitas Developer ID + notarización.

## Compilar en local

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release
```

Salida en `build/TunEC_artefacts/Release/{AU,VST3,Standalone}`.

## Notas

- **AAX (Pro Tools)**: requiere SDK de Avid y firma PACE/iLok; define `AAX_SDK_PATH` para añadirlo.
- **Licencia**: JUCE se usa bajo AGPLv3/GPLv3 o licencia comercial de JUCE.
- Latencia reportada según *Input Type*: Soprano ~10 ms, Alto/Tenor ~17 ms, Bass ~35 ms.
- IDs propios (`Tnca/Tnc1`, `com.tunecaudio.TunEC`).
