## Inspiration
Traditional CPR dummies are expensive mannequins, and their equipment is largely inaccessible to the general public due to high costs. We wanted to bridge this gap by creating an affordable, accessible alternative that allows anyone to educate themselves on life-saving techniques without needing to visit a dedicated CPR training facility.

## What it does
The CPR Trainer is a wearable wristband that helps users practice their chest compressions on any surface that has a spring-like "recoil". Using an onboard accelerometer and a force sensor, it tracks both the frequency and the pressure of the user's compressions.
It provides real-time feedback through a User Interface and features a visual LED metronome to keep the user on pace. Users can switch between two modes: a "Training" mode with the audio on to practice timing, and a "Test" mode with the audio off to check their synchronization within an acceptable range.

## How we built it
We designed the system to run entirely off an ESP32 microcontroller, eliminating the need for a connected PC. We integrated an Adafruit Square FSR (Force Sensitive Resistor) via a voltage divider and wired up an accelerometer.
Instead of relying on an external server, the ESP32 acts as its own access point, hosting a website locally. The user simply connects to the ESP32's Wi-Fi network to access the UI. To keep the wristband module lightweight, the system is wired to an external power source.

## Challenges we ran into
Integrating the hardware safely and reliably presented several hurdles. \<insert challenges here>

## Accomplishments that we're proud of
We are proud of achieving a fully standalone system where the ESP32 handles real-time sensor data acquisition while simultaneously hosting the web UI. Mapping the hardware constraints to build a functional prototype was a major win.

## What we learned
\<insert what we learned>

## What's next for CPR Trainer
Our immediate next steps involve ... (insert next steps)
