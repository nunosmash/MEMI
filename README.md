[README.md](https://github.com/user-attachments/files/26696720/README.md)
# MEMI

**MIDI-to-CV interface** for DAW-centered live rigs — tight sync between Ableton Live (or similar) and external hardware sequencers, with **clock** and **run / start–stop** split onto separate CV/Gate outputs.

Part of the **Ash Sound Works** custom tools for live and studio — by sound artist and producer **Ash (Ahn Sunghoon)** · [GitHub](https://github.com/nunosmash)

---

## Why MEMI?

When you run a DAW as the hub but still want a synth or drum machine’s **onboard sequencer**, MIDI Clock alone often couples Start/Stop with the clock stream, which makes it hard to **only** run the sequencer in the bars or sections you want.

MEMI separates **sync (clock)** from **when the sequencer runs (gate)**. You drive both from MIDI notes on a DAW track: the sequencer can be wired so it **only runs while the gate is high**.

---

## Features

- **Selective run control** — not limited to MIDI Clock Start/Stop; define run windows with MIDI note gates  
- **1/16-note** clock resolution  
- **USB MIDI ↔ DIN MIDI** pass-through (hardware MIDI bridge)  
- **Compact** — fits pedalboards and live racks  
- **MCU:** Arduino Pro Micro (5 V, ATmega32U4) — digital logic for CV-level signals  
- **I/O:** USB bus power, 3.5 mm TRS MIDI **Type A** in/out, two CV/Gate outputs  

---

## Technical specs

| Item | Details |
|------|---------|
| CV/Gate control | **MIDI channel 16** only |
| Output level | 0–5 V (digital logic) |
| Clock resolution | 1/16 note |
| Gate pulse | ~10 ms fixed trigger |
| Power | USB 5 V |
| MIDI I/O | TRS Type A (channels 1–15 on the DIN path) |

---

## MIDI note → CV mapping (channel 16)

| MIDI note | Output | Function |
|-----------|--------|----------|
| C1 | Clock 1 | External sequencer step clock (1/16) |
| D1 | Gate 1 | Sequencer run / Start–Stop control |
| C2 | Clock 2 | Second device — independent clock |
| D2 | Gate 2 | Second device — independent gate |

**TRS MIDI Type A:** Tip = signal, Ring = +5 V, Sleeve = GND. Type B gear uses opposite polarity — verify before wiring.

---

## Ableton Live (quick setup)

1. Connect MEMI over USB.  
2. Create a MIDI track → set output to MEMI → choose **channel 16**.  
3. Enter clock notes on a **1/16 grid**.  
4. Use gate notes (e.g. D1) and **note length** to define when the sequencer runs.  

Align with audio using MIDI **Track Delay** (and similar). Many setups need fine tuning around **−18 ms** depending on interface and buffer size.

---

## Repository contents

Depending on branch or release, this repo may include:

- Firmware source (Arduino / PlatformIO, etc.)  
- Schematics, PCB, enclosure files  
- Build and flash instructions  

For full wiring examples, per-device notes, and troubleshooting, see the bundled **MEMI User Manual** (HTML) in the Ash Sound Works site package — add your public URL here once deployed.

---

## Documentation

- Full manual: `memi-manual.html` (ships with the Ash Sound Works website bundle)  
- Other projects: [github.com/nunosmash](https://github.com/nunosmash)

---

## Disclaimer

- Always check external gear **sync/trigger input specs** (voltage, pulse width, Type A vs B). MEMI assumes **5 V TTL**–style signaling.  
- For DIY builds, follow safe practice for power, grounding, and cabling.

---

## License

See the `LICENSE` file in the repository root.
