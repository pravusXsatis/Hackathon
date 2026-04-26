# ESP32 Firmware

Board: ESP32 Dev Module  
Upload baud: 115200 or 921600  
Serial Monitor baud: 115200

## Wiring

- FSR voltage divider midpoint -> GPIO33
- ADXL335 XOUT -> GPIO34
- ADXL335 YOUT -> GPIO35
- ADXL335 ZOUT -> GPIO32
- ADXL335 VCC -> 3.3V
- ADXL335 GND -> GND
- FSR divider: 3.3V -> FSR -> GPIO33 -> 10k ohm -> GND

## Standalone Demo

The ESP32 creates an open Wi-Fi access point:

```text
CPR_Trainer
```

Connect a phone or laptop to that network. The captive portal should open automatically. If it does not, visit:

```text
http://192.168.4.1
```

## Endpoints

- `GET /`
- `GET /data`
- `GET /calibrate/rest`
- `GET /calibrate/target`

The firmware also handles common captive portal probe routes and unknown routes by serving the dashboard.
