# M.Y.E. plugins

Open-source audio effects for sound design, by Carlos Ángel Luppi (M.Y.E. — martillo · yunque · estribo).
Built with JUCE (VST3 / AU / Standalone). Mac builds: see Releases (`latest`).

| Plugin | Qué es |
|---|---|
| **Estribo** | Delay estéreo con modos de carácter (Limpio, Cinta, Analog 1–4, LoFi), ping-pong, freeze, modulación, auto-pan |
| **Martillo** | Compresor óptico clásico con énfasis HF y válvula |
| **Yunque** | Saturación / forja: Válvula, Cinta, Transformador, Fragua |
| **Tímpano** | Puntos de escucha: a través de algo, por un aparato, en el cuerpo |

Versiones y cambios: `CHANGELOG.md`. Plan: `ROADMAP.md`.

## Build
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
Offline DSP test: add `-DMYE_BUILD_TESTS=ON` and run `MyeDspTest`.

## Tools
- `tools/freesound-mcp`: MCP server to search, audition and download Freesound sounds from Claude (see its README).
