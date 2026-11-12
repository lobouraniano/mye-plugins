# M.Y.E. plugins — hoja de ruta (decidido con Carlos, 2026-11-12)

## Reglas
- Versiones A.B.C por componente: A = versión definitiva (hoy 0), B = actualización sustancial, C = cambios menores. Cada cambio sube versión y se anota en CHANGELOG.md.
- Corrección pendiente: Eco "0.2.0" debió ser 0.1.1 (fix mono→estéreo + UI). Si existe el release `eco-v0.2.0`, borrarlo/renombrar.
- Open-source; Mac (VST3 + AU, universal). Compila GitHub Actions → release `latest` + release versionado por plugin.
- Nada de código de Waves/terceros: se diseña por oído y documentación pública.
- Arquitecturas cómodas: pocos controles, agrupados en secciones con nombre.

## Trilogía M.Y.E. (oído medio: el martillo golpea, el yunque forja, el estribo transmite)
### Estribo 0.2.0 = Eco renombrado (delay con alma de H-Delay)
Secciones: TIEMPO (tiempo ms/nota, sync, división, offset R, ping-pong) · REPETICIONES (feedback hasta auto-oscilación, tono, low cut) · CARÁCTER (un selector Limpio/Cinta/Analog 1–4/LoFi + una perilla de intensidad; wow/flutter/ruido/zumbido/resolución viven dentro de cada modo) · MOVIMIENTO (mod rate/depth tipo chorus en repeticiones, pan rate/depth, pan a ecos o todo) · SALIDA (mix, width, fase R, salida). Freeze. Mostrar tiempo en ms y en notas. Mantener plugin code Eco1 para continuidad.

### Martillo 0.1.0 — compresor óptico clásico (inspirado en LA-2A, sin usar el nombre)
Gain, Peak Reduction, Compress/Limit, Énfasis HF en la detección (en el original es el trim trasero; acá al frente), Válvula (calidez), Mezcla. Celda óptica: ataque ~10 ms, release en dos etapas (rápida y luego muy lenta) con memoria (más compresión sostenida → suelta más lento). VU de reducción/salida.

### Yunque 0.1.0 — saturación / forja
Calor (drive), Tipo (Válvula, Cinta, Transformador, Fragua=wavefolder metálico), Forma (pre/de-énfasis tilt: dónde satura), Asimetría (armónicos pares), Grave, Brillo, Mezcla, Salida, auto-gain. Oversampling.

## Oído 0.1.0 (nombre de trabajo; a Carlos no lo convence — proponer alternativas, ej. "Tímpano", "Umbral", "Membrana")
Puntos de escucha: el sonido como se oye desde algún lugar/aparato/cuerpo. Capas: filtro de transmisión + resonancia del objeto + espacio del otro lado + defecto (distorsión/compresión/ruido).
Escenas: A través de algo (puerta, pared, ventana, auto desde afuera, otra habitación, bajo el agua) · Por un aparato (parlante de celular, llamada telefónica, radio AM, walkie-talkie, megáfono, TV de al lado, auriculares ajenos) · En el cuerpo (aturdido, oído tapado, acúfeno, pulso propio, adentro de la cabeza).
Controles comunes: Escena, Distancia, Material/Calidad, Espacio, Defecto, TRANSICIÓN (fader de normal → punto de escucha, para automatizar en montaje), Acúfeno, Pulso, Salida. Uso cotidiano incluido.

## Después
- Cóclea: estirador tipo Paulstretch "captura y estira" (algoritmo propio desde la descripción pública).
- Caracol: resonadores, beat repeat, granular.
- Líneas de identidad: "Lugares" (reverbs con IRs grabadas por Carlos: curtiembre de Pompeya, Masliánico…), "Efemérides" (modulación por ciclos planetarios).
- Diseño visual de las interfaces (Carlos quiere trabajarlo más adelante).
- Freesound MCP 0.1.0 escrito en tools/freesound-mcp, sin probar contra la API real (falta que Carlos lo instale con sus claves).
- Plugin "bandeja" de Freesound: descartado por ahora (alcanza con la carpeta en el Media Browser).

## Notas técnicas
- Audition: un track mono pasa 1 canal al plugin; Eco/Estribo acepta mono→estéreo, pero si Audition no lo da, usar bus estéreo o convertir el clip a estéreo.
- GitHub Actions: runner `macos-latest` (macos-14 ya no existe); a veces hay cola por falta de máquinas Mac, se relanza.
- Desde la sesión cloud no se pueden crear repos ni bajar artifacts de Actions; sí pushear y leer check-runs.
