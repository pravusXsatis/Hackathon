# ESP32 Firmware

Primary firmware for the current standalone CPR Trainer demo.

Board: ESP32 Dev Module  
Upload baud: 115200 or 921600  
Serial Monitor baud: 115200

## Wiring

- FSR signal -> GPIO36
- ADXL335 XOUT -> GPIO34
- ADXL335 YOUT -> GPIO32
- ADXL335 ZOUT -> GPIO33
- ADXL335 VCC -> 3.3V
- ADXL335 GND -> GND
- FSR divider: 3.3V -> FSR -> GPIO36 -> 10k ohm -> GND
- LED metronome guide: GPIO18 -> 220 ohm resistor -> LED anode, LED cathode -> GND

## Power Switch and LED Assumptions

- The physical switch is intended to be a hardware power ON/OFF switch.
- The switch is not used as a calibration button in firmware.
- The GPIO18 LED is a software-controlled metronome pacing guide (about 110 BPM), not a true "correct BPM" indicator from measured user rate.
- If you want a true always-on power LED, wire it directly from 3.3V through a resistor to GND (not through a GPIO pin).

## Standalone Demo

The ESP32 creates a Wi-Fi access point:

```text
CPR_Trainer
```

Password:

```text
cpr2026!
```

Connect a phone or laptop to that network. The captive portal should open automatically. If it does not, visit:

```text
http://192.168.4.1
```

This is the current recommended architecture. The older host-computer flow is preserved in `legacy/`.

## CPR Learning Link

- Watch CPR basics: [https://youtu.be/VZqG-tcZvfE?si=8la3IrQzfen--zav&t=35](https://youtu.be/VZqG-tcZvfE?si=8la3IrQzfen--zav&t=35)
- For demos, print a QR code for that link as a separate handout card.

## Endpoints

- `GET /`
- `GET /data`
- `GET /calibrate/rest`
- `GET /calibrate/target`
- `GET /calibrate` (legacy alias of `/calibrate/rest`)

The firmware also handles common captive portal probe routes and unknown routes by serving the dashboard.

`GET /data` includes `ledMode` as `metronome_110` for the pacing guide LED mode.

## Legacy Note

If you need the previous laptop-hosted pipeline (USB serial -> FastAPI -> React), see the `legacy/` folder in the repository root. Use this firmware README for active hardware demos and current development.
