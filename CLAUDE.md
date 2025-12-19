# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

tspec is a terminal-based audio spectrum analyzer written in C23. It captures audio via JACK/PipeWire, performs FFT analysis using FFTW3, and renders a real-time spectrum display using ncurses with UTF-8 block characters.

## Build Commands

```bash
# Configure
cmake -B build

# Build
cmake --build build

# Clean
rm -rf build
```

## Dependencies

- JACK audio connection kit (or PipeWire with JACK support)
- FFTW3 for FFT processing
- ncursesw for terminal UI

## Architecture

```
src/
├── main.c      # Application entry, main loop, signal handling
├── audio.c     # JACK client, audio capture ring buffer
├── spectrum.c  # FFTW3 FFT processing, Hann window, dB scaling
└── display.c   # ncurses rendering with UTF-8 block chars (▁▂▃▄▅▆▇█)

include/
├── audio.h     # Audio context, buffer constants
├── spectrum.h  # FFT context, bin configuration
└── display.h   # Display context, color pairs
```

## Code Conventions

- C23 standard with `constexpr` for compile-time constants
- Module pattern: each subsystem has init/shutdown functions and an opaque context struct
- Ring buffer for lock-free audio capture from JACK callback
- Logarithmic frequency mapping for perceptually meaningful display
- Exponential smoothing for visual stability (adjustable at runtime)

## Running

Requires JACK or PipeWire-JACK to be running. Connect tspec's input port to an audio source using a patchbay (qjackctl, Carla, or `pw-jack`).

Controls: +/- adjust smoothing, q to quit.
