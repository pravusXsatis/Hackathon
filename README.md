# CPR Trainer — IDEA Hacks 2026

> **Theme: A Brighter Tomorrow** | **Subcategory: Educate the Future**
>
> Affordable, hands-on CPR practice: anywhere, anytime, no mannequin required.
>
> ⚠️ Training only — not a certified medical device.

---

## Inspiration

Cardiac arrest strikes more than 350,000 people outside of hospitals in the United States every year. Immediate bystander CPR can double or triple a victim's chance of survival, yet the vast majority of people have never practiced it. The barrier isn't awareness; it's access. Certified CPR training requires scheduling a class, traveling to a facility, and working with expensive mannequins that cost hundreds to thousands of dollars per unit.

We built the CPR Trainer to remove that barrier entirely. It is a wrist-worn device that turns any springy surface, like a couch cushion, a foam mat, a folded blanket, into a CPR practice station. A user connects their phone to the device's own Wi-Fi network, opens the dashboard in a browser, and receives real-time, per-compression coaching with zero setup friction and zero ongoing cost. This project gives hands-on CPR practice on a low cost portable setup.

---

## Why This Is an "Educate the Future" Project

- **Enhances how people learn CPR.** Rather than watching a video or attending a one-time class, the user gets immediate, quantified feedback on every compression, rate, force, and full release, forming correct muscle memory through repetition.
- **Integrates into daily life.** The device requires no mannequin, no facility, and no internet connection. Practice happens wherever someone already is, with the phone already in their pocket.
- **Unique educational approach.** A calibration step personalizes the feedback thresholds to the individual user and surface. This makes the coaching adaptive, not one-size-fits-all.
- **Intuitive and engaging.** The dashboard opens instantly in any browser. The guided calibration walks users through setup in under 30 seconds with a countdown UI. Feedback is immediate, plain-language, and per-compression.

---

## What It Does

The CPR Trainer wristband reads an FSR (Force-Sensitive Resistor) and ADXL335 accelerometer on an ESP32. The ESP32 broadcasts its own Wi-Fi access point, named `CPR_Trainer` (password: `cpr2026!`); the user connects their phone or laptop to that network and opens the on-device dashboard at `http://192.168.4.1` — no app install, no internet. The dashboard reads live data and shows rate, relative force, and motion quality.

### Live feedback on every compression

| Label | What it means |
|---|---|
| **Good compression** | Peak force is within the user's calibrated target range |
| **Push harder** | Peak force is below 60 % of the calibrated target |
| **Too hard** | Peak force exceeds 130 % of the calibrated target |
| **Release fully** | Force did not drop back to baseline — leaning detected |
| **Start compressions** | Idle state, ready to begin |

### Dashboard at a glance

- **Compression rate gauge** — live CPM needle against the AHA target band of 100–120 CPM
- **Relative-force trend graph** — scrolling plot of the last 100 force readings with a target-force reference line
- **Metric cards** — compression count, rate, raw force, relative force, target force, and recoil threshold
- **Motion quality score** — sum of corrected accelerometer magnitudes (|X| + |Y| + |Z|) as a wrist-form indicator
- **Guided calibration modal** — 3-step flow with countdown UI, "Ready / Set / Go" cues, and recoil confirmation

---

## How We Built It

### Hardware

| Component | Detail |
|---|---|
| ESP32 | Microcontroller, Wi-Fi access point, web server |
| Adafruit Square FSR (via voltage divider) | Compression force sensing on GPIO 36 |
| ADXL335 Accelerometer | Motion quality on GPIO 34, 32, 33 |
| Battery Pack | Increases project portability |

### Software Stack

| Layer | Technology | Source |
|---|---|---|
| Active runtime | Arduino C++ + ESP32 WebServer + DNSServer | `firmware/esp32_cpr_trainer.ino` |
| Legacy backend (reference) | Python / FastAPI + pyserial | `legacy/backend/main.py` |
| Legacy frontend (reference) | React + Vite | `legacy/frontend/src/App.jsx` |

The active demo is fully standalone on the ESP32: it hosts the dashboard at `/`, serves live metrics from `/data`, and handles calibration routes directly on-device (`/calibrate/rest`, `/calibrate/target`). The Python/React host-computer pipeline remains in `legacy/` for comparison and rollback experiments.

### 3-Step Guided Calibration

The calibration modal walks the user through setup before each session:

1. **Rest baseline** — captures resting baselines for force and all accelerometer channels.
2. **Target setup** — applies the current fixed force coaching target used by the firmware.
3. **Recoil confirmation** — checks that force returns close to baseline so release can be detected reliably.

### Compression Detection (inside `firmware/esp32_cpr_trainer.ino`)

- **Enter** compression when corrected force rises above a start threshold (`max(300, forceTarget * 0.45)` after target calibration).
- **Count** a compression when corrected force drops below the release threshold (`max(30, forceTarget * 0.15)`), with a 250 ms debounce guard.
- **Rate** is computed from compression events in a 5-second window.
- **Force feedback** uses fixed raw-force guidance bands for "Push harder" / "Good compression" / "Too hard".
- **Rate feedback** uses AHA guidance bands: `<100` too slow, `100-120` good rate, `>120` too fast.

> ⚠️ Training feedback only — not a certified medical device.

### Compression Quality Logic
 
| Condition | Feedback |
|---|---|
| Compression peak below firmware force window | Push harder |
| Compression peak within firmware force window | Good compression |
| Compression peak above firmware force window | Too hard |
| Force remains above recoil threshold right after release | Release fully |
| Rate below 100 CPM | Too slow |
| Rate from 100 through 120 CPM | Good rate |
| Rate above 120 CPM | Too fast |

---

## Accomplishments We're Proud Of

- **Adaptive calibration** — thresholds are personalized per user and per surface, so feedback is meaningful whether someone is pressing on a firm foam mat or a soft cushion.
- A software architecture designed from the start to run standalone on the ESP32 — no PC, no router, no cloud — so the finished device will be truly self-contained.

---

## Vibe Coding — AI-Assisted Development

The active firmware plus earlier host-computer prototypes (`legacy/backend` and `legacy/frontend`) were generated with heavy AI assistance using multiple LLM tools throughout the hackathon. This section documents both the benefits and the concrete obstacles we ran into, per the DigiKey challenge documentation requirement.

### What worked well

Using LLMs to scaffold both the standalone firmware flow and earlier host-computer prototypes in a single hackathon session was only feasible because of AI assistance. Initial working versions of the Arduino sampling and calibration flow, dashboard rendering logic, and legacy FastAPI/React pipeline were all generated from natural-language prompts, giving the team a running start rather than a blank file.

### Obstacles with AI-generated code

**1. Graph scaling math**
The AI-generated force graph component produced visually incorrect output. The coordinate mapping did not correctly normalize force values to the graph's dimensions, causing the y-axis scaling to be inverted in edge cases. The AI consistently regenerated versions with the same underlying error when prompted generically. Fixing it required us to manually work through the coordinate math and explicitly specify the correct formula in the prompt.
 
**2. Calibration state flow**
AI-generated calibration logic did not correctly sequence the rest, target, and recoil confirmation states. State transitions fired prematurely or skipped steps under certain timing conditions. The team had to manually trace the state machine and rewrite the transition guards to make the flow reliable.
 
**3. Timeout control**
AI-generated timeout handling in the calibration and data polling loops did not account for edge cases where the ESP32 response was slow. The team manually adjusted timeout durations and added fallback handling to keep the dashboard responsive under real hardware conditions.

---

## Challenges We Ran Into

We spent time deciding between three architectures: USB serial with Python backend, Bluetooth, and standalone ESP32 Wi-Fi. USB serial gave strong debug visibility, but added laptop setup steps for users. Bluetooth reduced wires, but browser support and pairing flow added friction for a fast demo. We moved back to standalone ESP32 Wi-Fi with captive portal because setup stayed simple and repeatable.
 
We also worked through reliability issues while serving both dashboard and live `/data` responses from one ESP32. Calibration flow started as technical, then we rewrote it into a guided rest and target sequence using `/calibrate/rest` and `/calibrate/target`. FSR threshold tuning required repeated tests because sensor values are relative and shift with strap tension, hand position, and surface type. We avoided claiming exact compression depth in centimeters because accelerometer integration drift can accumulate error over time.

---

## What We Learned

- How to configure ESP32 as a Wi-Fi access point named `CPR_Trainer` and route captive portal traffic with `DNSServer`.
- How a browser dashboard can talk to an embedded device through simple HTTP endpoints like `/data` and calibration routes.
- How to combine FSR force data and ADXL335 motion data for relative compression quality, rate, recoil, and motion quality feedback.
- That calibration and threshold tuning matter as much as UI polish for trustworthy coaching behavior.
- That keeping active code and experimental legacy code separated helps development move faster and keeps responsibilities clear.

---

## What's Next for CPR Trainer

- Improve calibration flow with more guided testing prompts and clearer completion checks.
- Tune force thresholds across more users and more surfaces to improve consistency.
- Add stronger recoil detection logic for edge cases where users keep slight pressure after release.
- Evaluate adding a true distance sensor if future versions require exact compression depth measurement.
- Improve enclosure and wristband comfort for longer practice sessions.
- Add data logging and session summaries so users can review progress over time.
- Improve mobile dashboard layout for smaller screens and faster one-hand use.
- Utilize more various sensors to standardize sufficient force for chest compressions.
- Provide alternative languages in the UI for expanded accesibility.

---

## Repository Structure

```
CPR-Trainer/
  firmware/
    esp32_cpr_trainer.ino   # ESP32 standalone AP + ADXL335/FSR coaching dashboard
  hardware/
    board/                  # KiCad board/schematic source files
    README.md               # KiCad schematic notes and image
  legacy/
    backend/main.py         # Earlier USB-serial FastAPI prototype
    frontend/src/App.jsx    # Earlier React dashboard prototype
```

### Legacy Folder

The `legacy/` folder keeps the earlier laptop-hosted architecture used before the fully standalone ESP32 captive-portal flow. It includes an older FastAPI backend and React frontend that read CPR data over USB serial from a separate host machine. Keep it for reference, comparisons, and rollback experiments, but use `firmware/esp32_cpr_trainer.ino` as the primary demo path for current development.

---

## Quick Start (Standalone Firmware)

### 1 — Flash the ESP32

1. Open `firmware/esp32_cpr_trainer.ino` in Arduino IDE.
2. Install the **ESP32** board package.
3. Wire sensors:
   - FSR analog output -> **GPIO 36**
   - ADXL335 XOUT -> **GPIO 34**
   - ADXL335 YOUT -> **GPIO 32**
   - ADXL335 ZOUT -> **GPIO 33**
   - Optional metronome LED -> **GPIO 18** (through resistor to GND)
4. Flash. Confirm serial output at **115200 baud**:
   ```json
   {"t":12345,"force":1842,"ax":0.02,"ay":0.11,"az":1.21}
   ```

### 2 — Connect and Run

```bash
Connect your phone or laptop to Wi-Fi SSID "CPR_Trainer" (password: cpr2026!)
Open http://192.168.4.1 in a browser
```

The dashboard and coaching logic are served directly by the ESP32. No laptop backend is required for the active demo.

### Legacy Host-Computer Flow (Optional)

```bash
# Backend
cd legacy/backend
python -m venv .venv
.venv\Scripts\Activate.ps1   # PowerShell
pip install -r requirements.txt
uvicorn main:app --reload --port 8000

# Frontend (second terminal)
cd legacy/frontend
npm install
npm run dev
```

Then open [http://localhost:5173](http://localhost:5173) in a browser.

For first-time learners, use this CPR basics video: [https://youtu.be/VZqG-tcZvfE?si=8la3IrQzfen--zav&t=35](https://youtu.be/VZqG-tcZvfE?si=8la3IrQzfen--zav&t=35). For in-person demos, print a QR code for the same link as a separate card/poster.
