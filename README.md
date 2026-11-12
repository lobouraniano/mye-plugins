# M.Y.E. plugins

Open-source audio effects for sound design, by Carlos Ángel Luppi (M.Y.E. — martillo · yunque · estribo).
Built with JUCE (VST3 / AU / Standalone).

## Eco
Stereo tape-flavoured delay: free time or tempo sync, right-channel offset, ping-pong,
saturated feedback that can self-oscillate (feedback > 1.0), tone/low-cut inside the loop,
wow, flutter and random drift, freeze, and an equal-power auto-panner (echoes only or everything).

## Build
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

## Tools
- `tools/freesound-mcp`: MCP server to search, audition and download Freesound sounds from Claude (see its README).
