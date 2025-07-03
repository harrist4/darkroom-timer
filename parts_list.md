
# Parts List for Darkroom Timer Project

This project uses commonly available components to build a USB-powered stopwatch-style darkroom timer with a 4-digit LED display, audible tick, and a 3-button interface.

---

## Core Components

- **Arduino Nano V3.0** — ATmega328P with CH340 USB interface
- **Nano I/O Shield** — Terminal adapter expansion board for clean wiring
- **TM1637 4-Digit Display Module** — 0.56" red LED with built-in serial driver
- **Passive Buzzer** — 5V electromagnetic beeper (e.g., CYT1008)
- **Momentary Push Buttons (x3)** — 19mm metal, SPST, momentary action, screw terminal (Red, Green, Black)
- **USB-C Power Trigger Module** — Converts power from a USB-C charger into a steady 5V output for the timer (e.g., AITRIP 5A USB-C PD trigger board)

---

## Mounting and Wiring

- **M3 Screws (x4)** — Used to assemble the case
- **M3 Threaded Inserts (x4)** — Heat-set into plastic for secure case fastening
- **Adhesive Cork Sheet** — 1mm thick, applied to base for grip and surface protection
- **Wiring** — Flexible stranded hookup wire; connectors are up to builder preference

---

## Optical Component

- **Red Lighting Gel Filter** — 8.5" x 11" transparent red correction gel for safelight compatibility  
  - Example: *Transparent Color Red Correction Lighting Gel Filter 8.5 x 11-Inches Red Gel Light Filter Plastic Film Sheets*

---

## Adhesives and Assembly Aids

- **Hot Glue** — Used to secure internal components (Nano, display, USB-C module) to the case interior  
  - Easily released with a spray of isopropyl alcohol (IPA)
- **Bondic UV Resin Kit** — Used to affix the bezel cleanly without fogging or warping the red gel  
  - UV-accessible groove in the bezel makes this straightforward

---

## STL and Source Files

- FreeCAD source and STL files are located in the `3D/` directory  
  - Bezel: `darkroom-timer-Bezel.stl` (stepped design for UV resin)
  - Top/Bottom Case: `darkroom-timer-Case Top.stl`, `darkroom-timer-Case Bottom.stl`
- Arduino sketches are located in the `Arduino/` directory  
  - Main project: `Arduino/DarkroomDualTimer/DarkroomDualTimer.ino`
  - Demo/test sketch: `Arduino/VIsForVictory/VIsForVictory.ino`

---

For assembly instructions, wiring diagrams, and additional images, see the `README.md`.
