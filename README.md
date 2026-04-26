CPR Trainer, IDEA Hacks 2026

Theme: A Brighter Tomorrow
Subcategory: Educate the Future

Inspiration
Cardiac arrest hits more than 350000 people outside hospitals in the United States each year.
Fast bystander CPR raises survival odds.
Many people miss practice due to class cost, class travel, and mannequin access limits.
This project gives hands-on CPR practice on a low cost portable setup.

Why This Is an Educate the Future Project
You learn by action.
You get feedback after each compression.
You run practice at home, school, or club meetings.
You finish setup in under one minute.

What It Does
ESP32 starts Wi-Fi access point CPR_Trainer.
You join with phone or laptop.
Captive portal opens dashboard at http://192.168.4.1.
Dashboard reads live data every 50 ms.
Dashboard shows rate, relative force, recoil, and motion quality.

Live feedback labels
Good compression
Push harder
Too hard
Release fully
Too slow
Good rate
Too fast
Start compressions

How We Built It
Hardware
ESP32 Dev Module
FSR with voltage divider on GPIO33
ADXL335 on GPIO34, GPIO35, GPIO32
Power bank for portable use

Software
Firmware path: cpr-trainer-mvp/firmware/esp32_cpr_trainer.ino
Frontend path: cpr-trainer-mvp/frontend/src/App.jsx
Backend path: cpr-trainer-mvp/backend/main.py
Main demo path runs from ESP32 only.
Backend stays for simulator and legacy development.

Guided calibration flow
Step 1
Prompt asks for flat placement with no pressure.
UI shows Read prompt, Ready, Set, Go, then 3 second hold.
Dashboard calls GET /calibrate/rest.

Step 2
Prompt asks for good compression hold.
UI shows Ready, Set, Go, then 3 second hold.
Dashboard calls GET /calibrate/target.

Step 3
Prompt asks for full release.
Dashboard checks forceCorrected from GET /data.
Pass state appears after force drops near baseline.

Compression quality logic
Peak below 60 percent of forceTarget gives Push harder.
Peak from 60 percent through 130 percent gives Good compression.
Peak above 130 percent gives Too hard.
Force above recoil threshold after release gives Release fully.
Rate below 100 CPM gives Too slow.
Rate from 100 through 120 CPM gives Good rate.
Rate above 120 CPM gives Too fast.

Current Status
Firmware, frontend, and full standalone demo path are working.
Hardware enclosure polish is in progress.
Optional backend simulator path is working.

Accomplishments We Are Proud Of
Standalone offline demo runs from ESP32 with no cloud service.
Adaptive calibration gives user specific and surface specific feedback.
Captive portal opens fast on phone and laptop.
SoftAP security and client cap keep demo stable.

Vibe Coding, AI Assisted Development
Team used AI support across firmware, frontend, and backend.
Team reviewed output and applied manual fixes.
Main fixes covered graph scaling math, calibration state flow, and timeout control.

Challenges We Ran Into
To be filled by team.

What We Learned
To be filled by team.

What Is Next for CPR Trainer
To be filled by team.

Repository Structure
README.md
cpr-trainer-mvp/firmware/esp32_cpr_trainer.ino
cpr-trainer-mvp/firmware/README.md
cpr-trainer-mvp/frontend/src/App.jsx
cpr-trainer-mvp/frontend/src/styles.css
cpr-trainer-mvp/backend/main.py
cpr-trainer-mvp/hardware/board/

Quick Start
1. Flash cpr-trainer-mvp/firmware/esp32_cpr_trainer.ino to ESP32.
2. Power board.
3. Join Wi-Fi CPR_Trainer with password cprtrainer2026.
4. Open http://192.168.4.1 if portal page stays hidden.
5. Run guided calibration.
6. Start compression practice.

Demo security settings
SSID: CPR_Trainer
Password: cprtrainer2026
Security: WPA2
SoftAP client cap: 2 devices
Client cap keeps polling responsive during live judging.

Optional frontend development
1. Open terminal in cpr-trainer-mvp/frontend.
2. Run npm install.
3. Run npm run dev.
4. Open local URL from Vite output.
5. Keep computer on CPR_Trainer Wi-Fi for direct polling.

Optional backend simulator path
1. Open terminal in cpr-trainer-mvp/backend.
2. Create virtual environment.
3. Install requirements.txt.
4. Set CPR_SIMULATOR=1.
5. Run uvicorn main:app --reload --port 8000.
6. Set frontend base URL to http://127.0.0.1:8000.

Medical disclaimer
Training demo only.
Device output does not provide clinical diagnosis.
