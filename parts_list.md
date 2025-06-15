# Parts List for Darkroom Timer Project

This project uses commonly available components to build a USB-powered stopwatch-style darkroom timer with a 4-digit LED display, audible tick, and a 3-button interface.

## Core Components

- **Arduino Nano V3.0** — ATmega328P with CH340 USB interface
- **Nano I/O Shield** — Terminal adapter expansion board for clean wiring
- **TM1637 4-Digit Display Module** — 0.56" red LED with built-in serial driver
- **Passive Buzzer** — 5V electromagnetic beeper (e.g., CYT1008)
- **Momentary Push Buttons (x3)** — 19mm metal, SPST, momentary action, screw terminal (Red, Green, Black)
- **USB-C Power Trigger Module** — Adjustable 5V output from USB-C PD supply (e.g., AITRIP 5A PD trigger)

## Mounting and Wiring

- **M3 Screws (x4)** — Used to assemble the case
- **M3 Threaded Inserts (x4)** — Heat-set into plastic for secure case fastening
- **Adhesive Cork Sheet** — 1mm thick, applied to base for grip and surface protection
- **Wiring** — Flexible stranded hookup wire; connectors are up to builder preference

## Optical Component

- **Red Lighting Gel Filter** — 8.5" x 11" transparent red correction gel for safelight compatibility
  - Example: *Transparent Color Red Correction Lighting Gel Filter 8.5 x 11-Inches Red Gel Light Filter Plastic Film Sheets*

## Notes

- Battery operation is not supported or included
- STL files and FreeCAD source are available in the `3D/` directory
- Arduino sketch is located in `Arduino/darkroom_timer/darkroom_timer.ino`

For assembly, wiring diagrams, and case design references, see the `README.md`.

