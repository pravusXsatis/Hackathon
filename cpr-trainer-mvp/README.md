# Portable CPR Trainer

Standalone ESP32 CPR trainer demo. The ESP32 creates its own Wi-Fi network, opens a captive portal, serves the dashboard, and provides live sensor data directly from the board.

This project uses relative training feedback only: force, rate, recoil, and motion quality. It does not claim exact compression depth in centimeters.

## Main Demo Architecture

```text
ESP32 SoftAP "CPR_Trainer"
        |
        v
Phone/laptop joins ESP32 Wi-Fi
        |
        v
Captive portal opens dashboard from http://192.168.4.1
        |
        v
Dashboard polls ESP32 endpoints directly
```

The main demo does not depend on USB serial data streaming, Python backend services, localhost, or `127.0.0.1`.

## Repo Structure

```text
cpr-trainer-mvp/
  firmware/
    esp32_cpr_trainer.ino
  frontend/
    index.html
    package.json
    vite.config.js
    src/
      App.jsx
      main.jsx
      styles.css
  backend/
    main.py
    requirements.txt
```

## Hardware Wiring

- FSR voltage divider midpoint -> GPIO33
- ADXL335 XOUT -> GPIO34
- ADXL335 YOUT -> GPIO35
- ADXL335 ZOUT -> GPIO32
- ADXL335 VCC -> 3.3V
- ADXL335 GND -> GND
- FSR divider example: `3.3V -> FSR -> GPIO33 -> 10k ohm -> GND`

## Upload Firmware

1. Open `firmware/esp32_cpr_trainer.ino` in Arduino IDE.
2. Select an ESP32 board, such as **ESP32 Dev Module**.
3. Upload the sketch.
4. Open Serial Monitor at `115200` if you want to confirm startup logs.

Required Arduino libraries are included with the ESP32 Arduino core:

- `WiFi.h`
- `WebServer.h`
- `DNSServer.h`

## Run The Demo

1. Power the ESP32.
2. Connect your phone or laptop to Wi-Fi network `CPR_Trainer` with password `cprtrainer2026` (WPA2).
3. The captive portal should open automatically.
4. If it does not open, manually visit:

```text
http://192.168.4.1
```

The ESP32 serves the dashboard from `GET /`.
The SoftAP is limited to **2 simultaneous client connections**, which helps keep `/data` polling responsive during the live demo.

## Captive Portal

The firmware starts a DNS server and redirects unknown DNS/browser requests to the ESP32 SoftAP IP, normally `192.168.4.1`.

Captive portal routes include:

- `GET /generate_204`
- `GET /hotspot-detect.html`
- `GET /ncsi.txt`
- `GET /connecttest.txt`

Unknown routes serve the dashboard instead of returning `404`.

## ESP32 Endpoints

The dashboard reads directly from the ESP32:

- `GET /`
- `GET /data`
- `GET /calibrate/rest`
- `GET /calibrate/target`

JSON endpoints include CORS headers.

`GET /data` returns:

- `time`
- `forceRaw`
- `forceCorrected`
- `forceVoltage`
- `forceRestBaseline`
- `forceTarget`
- `accelXRaw`
- `accelYRaw`
- `accelZRaw`
- `accelXCorrected`
- `accelYCorrected`
- `accelZCorrected`
- `accelXVoltage`
- `accelYVoltage`
- `accelZVoltage`
- `motionMagnitude`
- `compressionCount`
- `compressionRate`
- `rateFeedback`
- `forceFeedback`
- `restCalibrated`
- `targetCalibrated`

## Guided Calibration

The dashboard guides the user through:

1. **Rest baseline**: "Place the trainer flat and do not press down."
   After a 3-second countdown, the dashboard calls `GET /calibrate/rest`.
2. **Target effort**: "Press down with what feels like a good CPR compression and hold."
   After a 3-second countdown, the dashboard calls `GET /calibrate/target`.
3. **Recoil check**: "Release fully."
   The dashboard polls `GET /data` until `forceCorrected` is close to `0`, then shows calibration complete.

## Frontend Development

The ESP32 firmware already serves the main demo dashboard. The React/Vite frontend is for local UI development.

When the React app is served by the ESP32, it uses relative URLs:

- `/data`
- `/calibrate/rest`
- `/calibrate/target`

When running the React app locally with Vite, it falls back to `http://192.168.4.1` unless `VITE_DEVICE_BASE` is set.

From `frontend/`:

```powershell
npm install
npm.cmd run dev -- --host 127.0.0.1
```

Then open:

```text
http://127.0.0.1:5173
```

Make sure your computer is connected to `CPR_Trainer` Wi-Fi so the local dev app can reach `http://192.168.4.1`.

Optional override:

```powershell
$env:VITE_DEVICE_BASE="http://192.168.4.1"
npm.cmd run dev -- --host 127.0.0.1
```

## Optional Legacy/Dev Backend

The Python backend is not part of the main submission/demo path. It is only an optional development fallback for simulation or older USB-serial workflows.

Use it only when the ESP32 hardware is unavailable or when you deliberately want a simulator:

```powershell
cd backend
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
$env:CPR_SIMULATOR="1"
.\.venv\Scripts\python.exe -m uvicorn main:app --port 8000
```

If using the optional backend with the React dev app:

```powershell
$env:VITE_DEVICE_BASE="http://127.0.0.1:8000"
npm.cmd run dev -- --host 127.0.0.1
```

Again: this backend mode is optional legacy/dev mode, not the standalone ESP32 demo path.

## Notes

- This is for training/demo use only and is not a medical device.
- Feedback is relative to the user's calibration and sensor readings.
- The FSR force signal is a proxy for compression effort, not exact chest depth.
