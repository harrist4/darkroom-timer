
# Darkroom Dual Timer

A USB-powered stopwatch + countdown timer designed for darkroom and general timing tasks.  
Features a clear LED display, tactile button interface, audible feedback, and ultra-low sleep mode.

![Assembled Darkroom Timer](images/darkroom_timers.jpg)

---

## Features

- Stopwatch and countdown timer modes
- Long-press and chord-based controls
- Sleep mode with wake-on-button
- Audible ticks and alarm tones
- USB-C PD power (via external module)
- Clean UV-resin bezel, red optical gel

---

## Modes
### Global Controls
- **Green + Black** — Enable ticking
- **Red + Black** — Disable ticking
- **Green + Red** — Toggle between Stopwatch and Countdown (Alarm) modes
- **Red (D4, long press)** — Power off

### Stopwatch Mode
- **Green (D2)** — Start
- **Red (D4)** — Stop
- **Black (D3)** — Reset

### Countdown Timer Mode
- **Black (D3)** — Add 1 minute (tap), or rapid 5min increments (hold)
- **Black (D3, long press)** — Reset to 00:00
- **Green (D2)** — Start countdown
- **Red (D4)** — Stop countdown

---

## Wiring Summary

| Arduino Pin | Function     | Notes                                              |
|-------------|--------------|-----------------------------------------------------|
| D2 *        | Green Button | Start / wake / tick toggle (INT0: wake from sleep) |
| D3 *        | Black Button | Add time / reset timer (INT1: wake from sleep)     |
| D4          | Red Button   | Stop / sleep / mode toggle                         |
| D5          | Buzzer       | Passive buzzer tone output                         |
| D6          | TM1637 CLK   | 4-digit display clock                              |
| D7          | TM1637 DIO   | 4-digit display data                               |

- Buttons are connected to GND and use `INPUT_PULLUP`
- Buzzer connects to D5 and GND
- Display uses TM1637 2-wire serial

* Pins D2 and D3 are used for sleep wake interrupts INT0 and INT1

---

## Power

- Powered via 5V USB-C trigger module
- USB-C powers Nano and display via 5V/GND pins
- Nano’s onboard USB port is unused
- Ideal for portable, battery-backed use

---

## 3D Printed Case

- STL files in `/3D/` folder
  - Top: `darkroom-timer-Case Top.stl`
  - Bottom: `darkroom-timer-Case Bottom.stl`
  - Bezel: `darkroom-timer-Bezel.stl` (step-backed for UV resin)

### Assembly Notes
- Internal parts secured with hot glue (release with IPA)
- Bezel glued with Bondic UV resin into side groove
- Red lighting gel sits under bezel over LED display

---

## Files

- `Arduino/DarkroomDualTimer/DarkroomDualTimer.ino`
- `3D/*.stl` and FreeCAD source
- `parts_list.md` with all required materials

---

For parts, assembly tips, and adhesive guidance, see [`parts_list.md`](parts_list.md).
