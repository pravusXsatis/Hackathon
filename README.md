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

The CPR Trainer wristband samples compression force at **50 Hz** using an FSR (Force-Sensitive Resistor). The ESP32 microcontroller broadcasts its own Wi-Fi access point, named `CPR_Trainer`; the user connects their phone or laptop to that network and opens the React dashboard at `http://192.168.4.1` — no app install, no internet. The dashboard reads live data and shows rate, relative force, and motion quality.

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
| Adafruit Square FSR (via voltage divider) | Compression force sensing on GPIO 39 |
| ADXL335 Accelerometer | Motion quality on GPIO 34, 32, 33 |
| Battery Pack | Increases project portability |

### Software Stack

| Layer | Technology | Source |
|---|---|---|
| Firmware | Arduino C++ | `firmware/esp32_cpr_trainer.ino` |
| Backend | Python / FastAPI + pyserial | `backend/main.py` |
| Frontend | React + Vite | `frontend/src/App.jsx` |

The FastAPI backend reads the serial JSON stream, runs the CPR metric engine, and pushes live data to every connected browser over a **WebSocket** (`/ws`). The frontend polls at 50 ms and renders all metrics without page reload.

### 3-Step Guided Calibration

The calibration modal walks the user through setup before each session:

1. **Rest baseline** (3 s sample) — establishes the zero-compression ADC floor for this surface and sensor placement.
2. **Target compression** — user presses with their intended CPR force; this fixes the "good" force reference and the dynamic detection threshold (`max(70, (max_force − baseline) × 0.12)`).
3. **Recoil confirmation** — system waits up to 6 s for force to return within 30 ADC units of baseline, confirming the surface has enough spring-back to detect full release.

### Compression Detection (inside `backend/main.py`)

- **Enter** compression when force rises above `baseline + dynamic_threshold`.
- **Count** (250 ms debounce) when force falls back to `baseline + max(40, dynamic_threshold × 0.4)`.
- **Force score** = `peak_force_above_baseline / calibrated_span`, clamped 0 → 1.
- **Rhythm consistency** = `1 − CV` of the last 20 inter-compression intervals (coefficient of variation).
- **Release quality** = fraction of compressions where force returned within 30 ADC units of true baseline.

Feedback priority order: release quality → force score → rate.

> ⚠️ Training feedback only — not a certified medical device.

### Compression Quality Logic
 
| Condition | Feedback |
|---|---|
| Peak below 60% of `forceTarget` | Push harder |
| Peak from 60% through 130% of `forceTarget` | Good compression |
| Peak above 130% of `forceTarget` | Too hard |
| Force above recoil threshold after release | Release fully |
| Rate below 100 CPM | Too slow |
| Rate from 100 through 120 CPM | Good rate |
| Rate above 120 CPM | Too fast |

---

## Accomplishments We're Proud Of

- **Adaptive calibration** — thresholds are personalized per user and per surface, so feedback is meaningful whether someone is pressing on a firm foam mat or a soft cushion.
- A software architecture designed from the start to run standalone on the ESP32 — no PC, no router, no cloud — so the finished device will be truly self-contained.

---

## Vibe Coding — AI-Assisted Development

All three layers of the codebase — firmware (Arduino C++), backend (Python / FastAPI), and frontend (React) — were generated with heavy AI assistance using multiple LLM tools throughout the hackathon. This section documents both the benefits and the concrete obstacles we ran into, per the DigiKey challenge documentation requirement.

### What worked well

Using LLMs to scaffold the entire stack from scratch in a single hackathon session was only feasible because of AI assistance. The initial working versions of the FastAPI serial reader, the WebSocket broadcast loop, the React component structure, and the Arduino FSR sampling loop were all generated in full from natural-language prompts, giving the team a running start rather than a blank file.

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
- That a clear project split between firmware and frontend folders helps development move faster and keeps responsibilities clear.

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

---

## Repository Structure

```
cpr-trainer-mvp/
  firmware/
    esp32_cpr_trainer.ino   # FSR + MPU6050 sampling, 50 Hz serial JSON output
  backend/
    main.py                 # FastAPI — serial reader, CPR metric engine, WebSocket broadcast
    requirements.txt        # fastapi, uvicorn[standard], pyserial
  frontend/
    src/
      App.jsx               # React dashboard — rate gauge, force graph, calibration modal
      styles.css
    index.html
    package.json
    vite.config.js
```

---

## Quick Start

### 1 — Flash the ESP32

1. Open `cpr-trainer-mvp/firmware/esp32_cpr_trainer.ino` in Arduino IDE.
2. Install the **ESP32** board package and these libraries:
   - `Adafruit MPU6050`
   - `Adafruit Unified Sensor`
3. Wire FSR analog output → **GPIO 34**. Optionally wire MPU6050: SDA → GPIO 21, SCL → GPIO 22.
4. Flash. Confirm serial output at **115200 baud**:
   ```json
   {"t":12345,"force":1842,"ax":0.02,"ay":0.11,"az":1.21}
   ```

### 2 — Run the Backend

```bash
cd cpr-trainer-mvp/backend
python -m venv .venv

# Activate (Windows PowerShell):
.venv\Scripts\Activate.ps1
# Activate (macOS / Linux):
source .venv/bin/activate

pip install -r requirements.txt

# Set serial port if needed (default: COM3):
$env:SERIAL_PORT="COM5"   # PowerShell
# export SERIAL_PORT=/dev/ttyUSB0   # bash

uvicorn main:app --reload --port 8000
```

**No hardware?** Run in simulator mode instead:
```bash
# PowerShell:
$env:CPR_SIMULATOR="1"; uvicorn main:app --reload --port 8000
# bash:
CPR_SIMULATOR=1 uvicorn main:app --reload --port 8000
```

### 3 — Run the Frontend

```bash
cd cpr-trainer-mvp/frontend
npm install
npm run dev
```

Open [http://localhost:5173](http://localhost:5173) in a browser.

**On real hardware:** connect your phone or laptop to the **CPR_Trainer** Wi-Fi network broadcast by the ESP32, then open `http://192.168.4.1` in a browser. The dashboard loads directly from the device — no internet needed.
