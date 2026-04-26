# Portable CPR Trainer MVP

Hackathon-ready software stack for a portable CPR trainer with ESP32 + local dashboard.

## What this MVP includes

- **ESP32 firmware** that samples force from GPIO34 at ~50 Hz and emits JSON lines over serial.
- **FastAPI backend** that reads serial JSON, auto-reconnects after serial errors, computes CPR metrics, and broadcasts live data via WebSocket.
- **React dashboard** with live compression rate, count, force, feedback, and a scrolling force graph.

## File structure

```text
cpr-trainer-mvp/
  firmware/
    esp32_cpr_trainer.ino
  backend/
    main.py
    requirements.txt
  frontend/
    index.html
    package.json
    vite.config.js
    src/
      App.jsx
      main.jsx
      styles.css
```

## 1) ESP32 firmware setup

1. Open `firmware/esp32_cpr_trainer.ino` in Arduino IDE.
2. Install board package for **ESP32**.
3. Install libraries:
   - `Adafruit MPU6050`
   - `Adafruit Unified Sensor`
4. Wire:
   - Force sensor analog output -> `GPIO34`
   - MPU6050 optional (I2C): SDA -> `GPIO21`, SCL -> `GPIO22` (default ESP32)
5. Flash and open serial monitor at **115200** baud.

Example output:

```json
{"t":12345,"force":1842,"ax":0.02,"ay":0.11,"az":1.21}
```

If MPU6050 is missing, JSON still streams with `t` and `force`.

## 2) Backend setup (FastAPI)

From `backend/`:

```bash
python -m venv .venv
# Windows PowerShell:
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

Set serial port env var if needed:

```bash
# PowerShell example:
$env:SERIAL_PORT="COM5"
$env:SERIAL_BAUD="115200"
```

Run backend:

```bash
uvicorn main:app --reload --port 8000
```

API endpoints:

- `GET /health`
- `POST /calibrate` (captures baseline force for 3s)
- `POST /session/start`
- `POST /session/reset`
- `GET /simulator` (shows the no-hardware practice script)
- `WS /ws`

### Practice without hardware

If the ESP32 hardware is not ready yet, run the backend in simulator mode:

```bash
# Windows PowerShell:
$env:CPR_SIMULATOR="1"
uvicorn main:app --reload --port 8000
```

Then run the frontend as usual and open [http://localhost:5173](http://localhost:5173).

The simulator streams fake 50 Hz force data and cycles through:

- Push harder
- Too slow
- Good rate
- Too fast
- Release fully

Use **Reset Session** to restart the scripted practice sequence. Turn on **Voice prompts** in the dashboard to rehearse the audio coaching.

## 3) Frontend setup (React + Vite)

From `frontend/`:

```bash
npm install
npm run dev
```

Open [http://localhost:5173](http://localhost:5173)

## CPR logic notes

- Target cadence: **100-120 CPM**
- Compression detection:
  - enters compression when force rises above `baseline + dynamic_threshold`
  - counts compression when force falls near baseline
  - applies **250 ms debounce** to prevent double count
- Force score:
  - normalizes current force above baseline against calibrated force span
- Release detection:
  - checks if force returns close to baseline after each compression
- Rhythm consistency:
  - derived from variation (coefficient of variation) in recent inter-compression intervals

This is training feedback logic for demos only, not a medical device algorithm.

## Demo flow

1. Connect ESP32 and start backend.
2. Open dashboard.
3. Press **Calibrate Baseline** without compressing.
4. Press **Start Session**.
5. Practice compressions and watch real-time feedback:
   - Good rate
   - Too slow
   - Too fast
   - Push harder
   - Release fully
