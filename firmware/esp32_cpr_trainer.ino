#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

static const char* AP_SSID = "CPR_Trainer";
static const char* AP_PASSWORD = "cprtrainer2026";
static const int AP_CHANNEL = 1;
static const bool AP_HIDDEN = false;
static const int AP_MAX_CONNECTIONS = 2;
static const byte DNS_PORT = 53;
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

// Sensors
static const int FORCE_PIN = 36;  // FSR voltage divider midpoint
static const int ACCEL_X_PIN = 34; // ADXL335 XOUT
static const int ACCEL_Y_PIN = 32; // ADXL335 YOUT
static const int ACCEL_Z_PIN = 33; // ADXL335 ZOUT
static const int LED_PIN = 18; // Optional CPR metronome guide LED (GPIO18 -> resistor -> LED -> GND)

// Tunable CPR demo thresholds. These are relative-force thresholds, not depth.
static const int DEFAULT_FORCE_TARGET = 800;
static const int DEFAULT_START_THRESHOLD = 300;
static const int MIN_FORCE_TARGET = 100;
static const int RECOIL_THRESHOLD = 30;
static const unsigned long COMPRESSION_DEBOUNCE_MS = 250;
static const unsigned long LED_METRONOME_INTERVAL_MS = 545; // ~110 BPM pacing guide
static const unsigned long LED_METRONOME_ON_MS = 110;
static const bool ENABLE_SERIAL_MONITOR_TELEMETRY = true;
static const unsigned long SERIAL_TELEMETRY_INTERVAL_MS = 250;

DNSServer dnsServer;
WebServer server(80);

int forceRestBaseline = 0;
int forceTarget = DEFAULT_FORCE_TARGET;
int accelXBaseline = 0;
int accelYBaseline = 0;
int accelZBaseline = 0;

bool restCalibrated = false;
bool targetCalibrated = false;

bool inCompression = false;
int activeCompressionPeak = 0;
int lastCompressionPeak = 0;
int compressionCount = 0;
unsigned long lastCompressionTimeMs = 0;
unsigned long lastCompletedCompressionMs = 0;
float compressionRate = 0.0;

enum LedMode { LED_MODE_METRONOME_110 };

LedMode ledMode = LED_MODE_METRONOME_110;
bool ledOutputHigh = false;
unsigned long lastSerialTelemetryMs = 0;

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>CPR Coach</title>
  <style>
    :root {
      font-family: Inter, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      color: #eef6f8;
      background: #101417;
    }

    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      background: linear-gradient(180deg, rgba(23,34,38,.94), rgba(12,15,17,.98));
    }
    button, input { font: inherit; }
    .app {
      width: min(1120px, 100%);
      margin: 0 auto;
      padding: 16px;
      display: grid;
      gap: 14px;
    }
    .topbar, .control-bar, .rate-head, .graph-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
    }
    .topbar { min-height: 64px; }
    .eyebrow {
      margin: 0 0 3px;
      color: #8fd0c1;
      font-size: .78rem;
      font-weight: 800;
      text-transform: uppercase;
    }
    h1, h2, p { margin-top: 0; }
    h1 {
      margin-bottom: 0;
      font-size: clamp(1.8rem, 5vw, 3.4rem);
      line-height: .95;
    }
    .status {
      border-radius: 999px;
      padding: 8px 12px;
      background: #0f4a32;
      color: #bcffdc;
      font-size: .85rem;
      font-weight: 800;
      white-space: nowrap;
    }
    .control-bar {
      flex-wrap: wrap;
      justify-content: flex-start;
      padding: 10px;
      border: 1px solid rgba(185,203,204,.22);
      border-radius: 8px;
      background: rgba(18,24,27,.86);
    }
    button {
      min-height: 42px;
      border: 0;
      border-radius: 8px;
      padding: 10px 14px;
      background: #0f9f79;
      color: white;
      font-weight: 800;
      cursor: pointer;
    }
    button:disabled { opacity: .6; cursor: default; }
    .feedback-card, .rate-panel, .metric-card, .graph-card, .motion-card {
      border: 1px solid rgba(185,203,204,.22);
      border-radius: 8px;
      background: rgba(18,24,27,.9);
      box-shadow: 0 16px 40px rgba(0,0,0,.22);
    }
    .feedback-card {
      min-height: 160px;
      display: grid;
      align-content: center;
      padding: clamp(18px, 4vw, 34px);
      border-left-width: 8px;
    }
    .feedback-card.good { border-left-color: #12d18e; }
    .feedback-card.warn { border-left-color: #ffc857; }
    .feedback-card.alert { border-left-color: #ff5b57; }
    .feedback-card.idle { border-left-color: #70a7ff; }
    .label, .metric-title {
      margin-bottom: 6px;
      color: #abc0c2;
      font-size: .84rem;
      font-weight: 800;
      text-transform: uppercase;
    }
    .feedback-message {
      margin-bottom: 8px;
      font-size: clamp(2.4rem, 8vw, 5.8rem);
      line-height: .95;
      font-weight: 900;
    }
    .feedback-cue {
      margin-bottom: 0;
      color: #d9e5e7;
      font-weight: 650;
    }
    .trainer-grid {
      display: grid;
      grid-template-columns: 1fr;
      gap: 12px;
    }
    .rate-panel, .motion-card, .graph-card, .metric-card { padding: 14px; }
    .rate-head p, .graph-head h2, .graph-head span { margin-bottom: 0; }
    .rate-head p {
      color: #abc0c2;
      font-size: .84rem;
      font-weight: 800;
      text-transform: uppercase;
    }
    .rate-head strong { font-size: clamp(1.5rem, 4vw, 2.5rem); }
    .rate-track {
      position: relative;
      height: 44px;
      margin-top: 18px;
      border-radius: 8px;
      background: linear-gradient(90deg, #5e3030 0%, #7a5f20 58%, #126846 67%, #126846 80%, #7a5f20 90%, #5e3030 100%);
      overflow: hidden;
    }
    .target-zone {
      position: absolute;
      inset: 0 20% 0 66.666%;
      border-inline: 2px solid rgba(255,255,255,.74);
    }
    .rate-needle {
      position: absolute;
      top: 4px;
      bottom: 4px;
      width: 4px;
      transform: translateX(-2px);
      border-radius: 4px;
      background: #fff;
      box-shadow: 0 0 0 3px rgba(0,0,0,.2);
    }
    .rate-scale {
      position: relative;
      height: 22px;
      margin-top: 8px;
      color: #abc0c2;
      font-size: .78rem;
      font-weight: 750;
    }
    .rate-scale span { position: absolute; top: 0; transform: translateX(-50%); }
    .rate-scale span:first-child { transform: none; }
    .rate-scale span:last-child { transform: translateX(-100%); }
    .metrics-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
    }
    .metric-card { min-width: 0; overflow: hidden; }
    .metric-value {
      margin-bottom: 0;
      font-size: clamp(1.55rem, 4vw, 2.3rem);
      line-height: 1.05;
      font-weight: 900;
      overflow-wrap: anywhere;
    }
    .score-note {
      margin: 8px 0 0;
      color: #9fb4b7;
      font-size: .88rem;
      line-height: 1.35;
    }
    .graph-head span { color: #9fb4b7; font-size: .86rem; font-weight: 700; }
    .graph-wrap {
      margin-top: 10px;
      height: 260px;
      border-radius: 8px;
      border: 1px solid rgba(185,203,204,.2);
      background: #0b1012;
      overflow: hidden;
    }
    canvas { width: 100%; height: 100%; display: block; }
    .modal {
      position: fixed;
      inset: 0;
      display: none;
      place-items: center;
      padding: 16px;
      background: rgba(3,7,9,.82);
      z-index: 10;
    }
    .modal.open { display: grid; }
    .modal-card {
      width: min(680px, 100%);
      border-radius: 12px;
      border: 1px solid rgba(185,203,204,.28);
      background: rgba(18,24,27,.97);
      padding: clamp(18px, 4vw, 34px);
    }
    .modal-card h2 {
      margin: 8px 0 0;
      font-size: clamp(1.35rem, 4vw, 2.15rem);
      line-height: 1.2;
    }
    .phase {
      margin: 12px 0 0;
      color: #9ddfcb;
      font-weight: 800;
    }
    .countdown {
      margin: 14px 0 0;
      color: #8df6d0;
      font-size: clamp(2.4rem, 11vw, 5rem);
      font-weight: 900;
      line-height: 1;
    }
    @media (min-width: 760px) {
      .app { padding: 24px; gap: 16px; }
      .trainer-grid { grid-template-columns: minmax(0, 1.2fr) minmax(320px, .8fr); }
      .metrics-grid { grid-template-columns: repeat(4, minmax(0, 1fr)); }
    }
    @media (max-width: 560px) {
      .topbar { align-items: flex-start; flex-direction: column; }
      button { flex: 1 1 150px; }
      .metrics-grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <main class="app">
    <header class="topbar">
      <div>
        <p class="eyebrow">Portable Trainer</p>
        <h1>CPR Coach</h1>
      </div>
      <span id="status" class="status">ESP32 Wi-Fi Live</span>
    </header>

    <section class="control-bar">
      <button id="calibrateBtn" type="button">Start Guided Calibration</button>
    </section>

    <section id="feedbackCard" class="feedback-card idle">
      <p class="label">Compression Quality</p>
      <p id="qualityFeedback" class="feedback-message">Start compressions</p>
      <p id="feedbackCue" class="feedback-cue">Begin when ready.</p>
    </section>

    <section class="trainer-grid">
      <section class="rate-panel">
        <div class="rate-head">
          <p>Compression Rate</p>
          <strong id="rateValue">0.0 CPM</strong>
        </div>
        <div class="rate-track">
          <span class="target-zone"></span>
          <span id="rateNeedle" class="rate-needle" style="left:0%"></span>
        </div>
        <div class="rate-scale">
          <span style="left:0%">0</span>
          <span style="left:66.666%">100</span>
          <span style="left:80%">120</span>
          <span style="left:100%">150</span>
        </div>
      </section>

      <article class="motion-card">
        <p class="label">Motion Quality</p>
        <p id="motionMagnitude" class="metric-value">0</p>
        <p class="score-note">Relative movement from ADXL335 X/Y/Z channels.</p>
      </article>
    </section>

    <section class="metrics-grid">
      <article class="metric-card"><p class="metric-title">Compression Count</p><p id="compressionCount" class="metric-value">0</p></article>
      <article class="metric-card"><p class="metric-title">Raw Force</p><p id="forceRaw" class="metric-value">0</p></article>
      <article class="metric-card"><p class="metric-title">Rest Baseline</p><p id="forceRestBaseline" class="metric-value">0</p></article>
      <article class="metric-card"><p class="metric-title">Relative Force</p><p id="forceCorrected" class="metric-value">0</p></article>
      <article class="metric-card"><p class="metric-title">Target Effort</p><p id="forceTarget" class="metric-value">800</p></article>
      <article class="metric-card"><p class="metric-title">Force Voltage</p><p id="forceVoltage" class="metric-value">0.00</p></article>
      <article class="metric-card"><p class="metric-title">Rate Feedback</p><p id="rateFeedback" class="metric-value">-</p></article>
      <article class="metric-card"><p class="metric-title">Force Feedback</p><p id="forceFeedback" class="metric-value">-</p></article>
    </section>

    <section class="graph-card">
      <div class="graph-head">
        <h2>Relative Force Trend</h2>
        <span>raw force minus baseline</span>
      </div>
      <div class="graph-wrap">
        <canvas id="forceCanvas"></canvas>
      </div>
    </section>
  </main>

  <section id="modal" class="modal" role="dialog" aria-modal="true">
    <div class="modal-card">
      <p id="modalStep" class="label">Step</p>
      <h2 id="modalDetail"></h2>
      <p id="modalPhase" class="phase"></p>
      <p id="modalCountdown" class="countdown"></p>
    </div>
  </section>

  <script>
    const MAX_POINTS = 100;
    const RELEASE_THRESHOLD = 30;
    const RELEASE_CLOSE_TO_ZERO_THRESHOLD = 12;
    const COMPRESSION_START_THRESHOLD = 45;
    const PUSH_HARDER_RATIO = 0.6;
    const TOO_HARD_RATIO = 1.3;
    const state = {
      points: [],
      latest: null,
      compressionActive: false,
      peak: 0,
      lastCompletedAt: 0,
      qualityFeedback: "Start compressions"
    };

    const meta = {
      "Good compression": ["good", "Relative force is in target range."],
      "Good rate": ["good", "Keep compressions steady."],
      "Push harder": ["warn", "Peak force is below 60% of your calibrated target."],
      "Too slow": ["warn", "Speed up toward 100 to 120 compressions per minute."],
      "Too fast": ["warn", "Slow down toward 100 to 120 compressions per minute."],
      "Too hard": ["alert", "Peak force is above 130% of your calibrated target."],
      "Release fully": ["alert", "Do not lean between compressions."],
      "Calibrate target": ["warn", "Run guided calibration before training."],
      "Start compressions": ["idle", "Begin when ready."]
    };

    function $(id) { return document.getElementById(id); }
    function sleep(ms) { return new Promise(resolve => setTimeout(resolve, ms)); }
    function round(value) { return Math.round(Number(value) || 0); }

    async function fetchJson(path) {
      const response = await fetch(path, { cache: "no-store" });
      if (!response.ok) throw new Error(path + " failed");
      return response.json();
    }

    async function fetchJsonWithRetry(path, attempts = 3) {
      let lastError;
      for (let attempt = 1; attempt <= attempts; attempt++) {
        try {
          return await fetchJson(path);
        } catch (error) {
          lastError = error;
          if (attempt < attempts) {
            await sleep(180);
          }
        }
      }
      throw lastError || new Error(path + " failed");
    }

    function updateQuality(data) {
      const force = round(data.forceCorrected);
      const target = Math.max(0, round(data.forceTarget));
      if (force >= COMPRESSION_START_THRESHOLD && !state.compressionActive) {
        state.compressionActive = true;
        state.peak = force;
      }
      if (state.compressionActive) {
        state.peak = Math.max(state.peak, force);
        if (force <= RELEASE_THRESHOLD) {
          state.compressionActive = false;
          state.lastCompletedAt = Date.now();
          if (target > 0) {
            const ratio = state.peak / target;
            if (ratio < PUSH_HARDER_RATIO) state.qualityFeedback = "Push harder";
            else if (ratio > TOO_HARD_RATIO) state.qualityFeedback = "Too hard";
            else state.qualityFeedback = "Good compression";
          } else {
            state.qualityFeedback = "Calibrate target";
          }
          state.peak = 0;
        }
      }
      if (!state.compressionActive && force > RELEASE_THRESHOLD && Date.now() - state.lastCompletedAt < 1500) {
        state.qualityFeedback = "Release fully";
      }
    }

    function setFeedback(text) {
      const info = meta[text] || meta["Start compressions"];
      $("feedbackCard").className = "feedback-card " + info[0];
      $("qualityFeedback").textContent = text;
      $("feedbackCue").textContent = info[1];
    }

    function drawGraph(target) {
      const canvas = $("forceCanvas");
      const rect = canvas.getBoundingClientRect();
      const scale = window.devicePixelRatio || 1;
      canvas.width = Math.max(1, Math.floor(rect.width * scale));
      canvas.height = Math.max(1, Math.floor(rect.height * scale));
      const ctx = canvas.getContext("2d");
      ctx.setTransform(scale, 0, 0, scale, 0, 0);
      ctx.clearRect(0, 0, rect.width, rect.height);

      const values = state.points;
      const maxObserved = Math.max(target, 100, ...values);
      const graphMax = Math.max(100, Math.ceil(maxObserved / 100) * 100);

      ctx.strokeStyle = "rgba(255,255,255,.08)";
      ctx.lineWidth = 1;
      for (let i = 1; i < 4; i++) {
        const y = rect.height * i / 4;
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(rect.width, y);
        ctx.stroke();
      }

      const targetY = rect.height - (Math.max(0, target) / graphMax) * rect.height;
      if (target > 0) {
        ctx.setLineDash([6, 6]);
        ctx.strokeStyle = "#ffc857";
        ctx.beginPath();
        ctx.moveTo(0, targetY);
        ctx.lineTo(rect.width, targetY);
        ctx.stroke();
        ctx.setLineDash([]);
        ctx.fillStyle = "#ffd36c";
        ctx.font = "800 13px system-ui";
        ctx.fillText("target " + target, 12, Math.max(16, targetY - 6));
      }

      ctx.strokeStyle = "#75e6ff";
      ctx.lineWidth = 3;
      ctx.beginPath();
      values.forEach((value, index) => {
        const x = values.length <= 1 ? 0 : index * rect.width / (values.length - 1);
        const y = rect.height - (Math.max(0, value) / graphMax) * rect.height;
        if (index === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.stroke();

      ctx.fillStyle = "#abc0c2";
      ctx.font = "800 13px system-ui";
      ctx.textAlign = "right";
      ctx.fillText(String(graphMax), rect.width - 10, 18);
      ctx.fillText("0", rect.width - 10, rect.height - 10);
    }

    function render(data) {
      state.latest = data;
      updateQuality(data);
      const relativeForce = round(data.forceCorrected);
      const target = round(data.forceTarget);
      state.points.push(relativeForce);
      state.points = state.points.slice(-MAX_POINTS);

      $("forceRaw").textContent = round(data.forceRaw);
      $("forceCorrected").textContent = relativeForce;
      $("forceRestBaseline").textContent = round(data.forceRestBaseline);
      $("forceTarget").textContent = target;
      $("forceVoltage").textContent = Number(data.forceVoltage || 0).toFixed(2);
      $("compressionCount").textContent = round(data.compressionCount);
      $("rateValue").textContent = Number(data.compressionRate || 0).toFixed(1) + " CPM";
      $("motionMagnitude").textContent = round(data.motionMagnitude);
      $("rateFeedback").textContent = data.rateFeedback || "-";
      $("forceFeedback").textContent = data.forceFeedback || "-";

      const ratePercent = Math.max(0, Math.min(100, (Number(data.compressionRate || 0) / 150) * 100));
      $("rateNeedle").style.left = ratePercent + "%";

      setFeedback(state.qualityFeedback);
      drawGraph(target);
      $("status").textContent = "ESP32 Wi-Fi Live";
    }

    async function pollData() {
      try {
        const data = await fetchJson("/data");
        render(data);
      } catch (error) {
        $("status").textContent = "Waiting for ESP32";
      }
    }

    function setModal(open, step, detail, phase, countdown) {
      $("modal").className = open ? "modal open" : "modal";
      $("modalStep").textContent = step || "";
      $("modalDetail").textContent = detail || "";
      $("modalPhase").textContent = phase || "";
      $("modalCountdown").textContent = countdown == null ? "" : String(countdown);
    }

    async function countdown(step, detail, capturePath) {
      setModal(true, step, detail, "Read the instruction", null);
      await sleep(1500);
      for (const cue of ["Ready", "Set", "Go"]) {
        setModal(true, step, detail, cue, null);
        await sleep(700);
      }
      let captured = false;
      for (let value = 5; value > 0; value--) {
        setModal(true, step, detail, "Hold steady", value);
        if (capturePath && value === 1 && !captured) {
          // Keep "1" visible briefly before and during capture request.
          await sleep(250);
          setModal(true, step, detail, "Hold steady", 1);
          await fetchJsonWithRetry(capturePath, 3);
          captured = true;
          await sleep(250);
        } else {
          await sleep(1000);
        }
      }
    }

    async function startCalibration() {
      const button = $("calibrateBtn");
      button.disabled = true;
      try {
        await countdown(
          "Step 1",
          "Place the CPR trainer on your practice surface and keep your hands off the pad.",
          "/calibrate/rest"
        );
        await sleep(600);
        await countdown(
          "Step 2",
          "Place your hands where they will be during CPR, then press down with what feels like a good CPR compression and hold it until the countdown reaches 1.",
          "/calibrate/target"
        );
        await sleep(600);
        setModal(true, "Step 3", "Now take your hands off the pad completely.", "Waiting for recoil", null);

        const startedAt = Date.now();
        let released = false;
        while (Date.now() - startedAt < 10000) {
          const data = await fetchJson("/data");
          render(data);
          if (round(data.forceCorrected) <= RELEASE_CLOSE_TO_ZERO_THRESHOLD) {
            released = true;
            break;
          }
          await sleep(150);
        }

        if (!released) {
          setModal(true, "Step 3", "Release fully — avoid leaning on the pad.", "Still detecting pressure", null);
          await sleep(1800);
          return;
        }

        setModal(true, "Calibration complete", "Recoil baseline confirmed.", "", null);
        await sleep(1200);
      } catch (error) {
        setModal(true, "Calibration failed", "Check the trainer connection and try again.", "", null);
        await sleep(1800);
      } finally {
        setModal(false);
        button.disabled = false;
      }
    }

    $("calibrateBtn").addEventListener("click", startCalibration);
    window.addEventListener("resize", () => {
      if (state.latest) drawGraph(round(state.latest.forceTarget));
    });
    setInterval(pollData, 100);
    pollData();
  </script>
</body>
</html>
)rawliteral";

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Cache-Control", "no-store");
}

float analogToVoltage(int value) {
  return value * (3.3 / 4095.0);
}

int averageAnalogRead(int pin, int samples = 180, int delayMs = 4) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(delayMs);
  }
  return (int)(sum / samples);
}

void resetCompressionStats() {
  inCompression = false;
  activeCompressionPeak = 0;
  lastCompressionPeak = 0;
  compressionCount = 0;
  lastCompressionTimeMs = 0;
  lastCompletedCompressionMs = 0;
  compressionRate = 0.0;
}

LedMode computeLedMode() {
  return LED_MODE_METRONOME_110;
}

const char* ledModeText(LedMode mode) {
  switch (mode) {
    case LED_MODE_METRONOME_110:
      return "metronome_110";
  }
  return "metronome_110";
}

bool ledBaseStateForMode(LedMode mode, unsigned long nowMs) {
  switch (mode) {
    case LED_MODE_METRONOME_110:
      return (nowMs % LED_METRONOME_INTERVAL_MS) < LED_METRONOME_ON_MS;
  }
  return false;
}

void updateStatusLed() {
  const unsigned long nowMs = millis();
  ledMode = computeLedMode();
  bool ledOn = ledBaseStateForMode(ledMode, nowMs);

  if (ledOn != ledOutputHigh) {
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    ledOutputHigh = ledOn;
  }
}

int forceStartThreshold() {
  if (targetCalibrated) {
    return max(DEFAULT_START_THRESHOLD, (int)(forceTarget * 0.45));
  }
  return DEFAULT_START_THRESHOLD;
}

int forceReleaseThreshold() {
  if (targetCalibrated) {
    return max(RECOIL_THRESHOLD, (int)(forceTarget * 0.15));
  }
  return RECOIL_THRESHOLD;
}

void updateCompressionStats(int forceCorrected) {
  const unsigned long now = millis();
  const int startThreshold = forceStartThreshold();
  const int releaseThreshold = forceReleaseThreshold();

  if (!inCompression && forceCorrected >= startThreshold && now - lastCompletedCompressionMs >= COMPRESSION_DEBOUNCE_MS) {
    inCompression = true;
    activeCompressionPeak = forceCorrected;
  }

  if (inCompression) {
    activeCompressionPeak = max(activeCompressionPeak, forceCorrected);
    if (forceCorrected <= releaseThreshold) {
      inCompression = false;
      lastCompressionPeak = activeCompressionPeak;
      activeCompressionPeak = 0;

      if (now - lastCompletedCompressionMs >= COMPRESSION_DEBOUNCE_MS) {
        compressionCount++;
        if (lastCompressionTimeMs > 0) {
          const unsigned long interval = now - lastCompressionTimeMs;
          if (interval > 0) {
            compressionRate = 60000.0 / interval;
          }
        }
        lastCompressionTimeMs = now;
        lastCompletedCompressionMs = now;
      }
    }
  }
}

String rateFeedbackText() {
  if (compressionRate <= 0) {
    return "Start compressions";
  }
  if (compressionRate < 100) {
    return "Too slow";
  }
  if (compressionRate <= 120) {
    return "Good rate";
  }
  return "Too fast";
}

String forceFeedbackText(int forceCorrected) {
  if (!targetCalibrated) {
    return "Calibrate target";
  }

  const int referenceForce = inCompression ? max(activeCompressionPeak, forceCorrected) : max(lastCompressionPeak, forceCorrected);
  const float ratio = (float)referenceForce / (float)max(forceTarget, MIN_FORCE_TARGET);

  if (!inCompression && forceCorrected > RECOIL_THRESHOLD && millis() - lastCompletedCompressionMs < 1500) {
    return "Release fully";
  }
  if (ratio < 0.60) {
    return "Push harder";
  }
  if (ratio <= 1.30) {
    return "Good compression";
  }
  return "Too hard";
}

void serveDashboard() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

void handleData() {
  const int forceRaw = analogRead(FORCE_PIN);
  const int accelXRaw = analogRead(ACCEL_X_PIN);
  const int accelYRaw = analogRead(ACCEL_Y_PIN);
  const int accelZRaw = analogRead(ACCEL_Z_PIN);

  const int forceCorrected = max(0, forceRaw - forceRestBaseline);
  const int accelXCorrected = accelXRaw - accelXBaseline;
  const int accelYCorrected = accelYRaw - accelYBaseline;
  const int accelZCorrected = accelZRaw - accelZBaseline;
  const int motionMagnitude = abs(accelXCorrected) + abs(accelYCorrected) + abs(accelZCorrected);

  updateCompressionStats(forceCorrected);
  updateStatusLed();

  if (ENABLE_SERIAL_MONITOR_TELEMETRY) {
    const unsigned long nowMs = millis();
    if (nowMs - lastSerialTelemetryMs >= SERIAL_TELEMETRY_INTERVAL_MS) {
      Serial.print("forceRaw=");
      Serial.print(forceRaw);
      Serial.print(" forceCorrected=");
      Serial.print(forceCorrected);
      Serial.print(" baseline=");
      Serial.print(forceRestBaseline);
      Serial.print(" target=");
      Serial.print(forceTarget);
      Serial.print(" accelRaw=(");
      Serial.print(accelXRaw);
      Serial.print(",");
      Serial.print(accelYRaw);
      Serial.print(",");
      Serial.print(accelZRaw);
      Serial.print(")");
      Serial.print(" accelCorr=(");
      Serial.print(accelXCorrected);
      Serial.print(",");
      Serial.print(accelYCorrected);
      Serial.print(",");
      Serial.print(accelZCorrected);
      Serial.print(")");
      Serial.print(" motion=");
      Serial.print(motionMagnitude);
      Serial.print(" rate=");
      Serial.print(compressionRate, 1);
      Serial.print(" ledMode=");
      Serial.print(ledModeText(ledMode));
      Serial.print(" restCal=");
      Serial.print(restCalibrated ? "1" : "0");
      Serial.print(" targetCal=");
      Serial.println(targetCalibrated ? "1" : "0");
      lastSerialTelemetryMs = nowMs;
    }
  }

  String json = "{";
  json += "\"time\":" + String(millis()) + ",";
  json += "\"forceRaw\":" + String(forceRaw) + ",";
  json += "\"forceCorrected\":" + String(forceCorrected) + ",";
  json += "\"forceVoltage\":" + String(analogToVoltage(forceRaw), 3) + ",";
  json += "\"forceRestBaseline\":" + String(forceRestBaseline) + ",";
  json += "\"forceTarget\":" + String(forceTarget) + ",";
  json += "\"accelXRaw\":" + String(accelXRaw) + ",";
  json += "\"accelYRaw\":" + String(accelYRaw) + ",";
  json += "\"accelZRaw\":" + String(accelZRaw) + ",";
  json += "\"accelXCorrected\":" + String(accelXCorrected) + ",";
  json += "\"accelYCorrected\":" + String(accelYCorrected) + ",";
  json += "\"accelZCorrected\":" + String(accelZCorrected) + ",";
  json += "\"accelXVoltage\":" + String(analogToVoltage(accelXRaw), 3) + ",";
  json += "\"accelYVoltage\":" + String(analogToVoltage(accelYRaw), 3) + ",";
  json += "\"accelZVoltage\":" + String(analogToVoltage(accelZRaw), 3) + ",";
  json += "\"motionMagnitude\":" + String(motionMagnitude) + ",";
  json += "\"compressionCount\":" + String(compressionCount) + ",";
  json += "\"compressionRate\":" + String(compressionRate, 1) + ",";
  json += "\"rateFeedback\":\"" + rateFeedbackText() + "\",";
  json += "\"forceFeedback\":\"" + forceFeedbackText(forceCorrected) + "\",";
  json += "\"ledMode\":\"" + String(ledModeText(ledMode)) + "\",";
  json += "\"restCalibrated\":" + String(restCalibrated ? "true" : "false") + ",";
  json += "\"targetCalibrated\":" + String(targetCalibrated ? "true" : "false");
  json += "}";

  addCorsHeaders();
  server.send(200, "application/json", json);
}

void handleCalibrateRest() {
  forceRestBaseline = averageAnalogRead(FORCE_PIN);
  accelXBaseline = averageAnalogRead(ACCEL_X_PIN);
  accelYBaseline = averageAnalogRead(ACCEL_Y_PIN);
  accelZBaseline = averageAnalogRead(ACCEL_Z_PIN);
  restCalibrated = true;
  targetCalibrated = false;
  forceTarget = DEFAULT_FORCE_TARGET;
  resetCompressionStats();

  String json = "{";
  json += "\"status\":\"rest_calibrated\",";
  json += "\"forceRestBaseline\":" + String(forceRestBaseline) + ",";
  json += "\"accelXBaseline\":" + String(accelXBaseline) + ",";
  json += "\"accelYBaseline\":" + String(accelYBaseline) + ",";
  json += "\"accelZBaseline\":" + String(accelZBaseline);
  json += "}";

  addCorsHeaders();
  server.send(200, "application/json", json);
}

void handleCalibrateTarget() {
  const int targetReading = averageAnalogRead(FORCE_PIN);
  forceTarget = max(MIN_FORCE_TARGET, targetReading - forceRestBaseline);
  targetCalibrated = true;
  resetCompressionStats();

  String json = "{";
  json += "\"status\":\"target_calibrated\",";
  json += "\"forceTarget\":" + String(forceTarget) + ",";
  json += "\"targetRaw\":" + String(forceRestBaseline + forceTarget);
  json += "}";

  addCorsHeaders();
  server.send(200, "application/json", json);
}

void handleOptions() {
  addCorsHeaders();
  server.send(204);
}

void handleCaptivePortalRoute() {
  serveDashboard();
}

void handleNotFound() {
  if (server.method() == HTTP_OPTIONS) {
    handleOptions();
    return;
  }
  serveDashboard();
}

void registerRoutes() {
  server.on("/", HTTP_GET, serveDashboard);

  server.on("/data", HTTP_GET, handleData);
  server.on("/data", HTTP_OPTIONS, handleOptions);

  server.on("/calibrate/rest", HTTP_GET, handleCalibrateRest);
  server.on("/calibrate/rest", HTTP_OPTIONS, handleOptions);

  server.on("/calibrate/target", HTTP_GET, handleCalibrateTarget);
  server.on("/calibrate/target", HTTP_OPTIONS, handleOptions);

  // Legacy alias for quick manual testing.
  server.on("/calibrate", HTTP_GET, handleCalibrateRest);
  server.on("/calibrate", HTTP_OPTIONS, handleOptions);

  // Common captive portal probes.
  server.on("/generate_204", HTTP_GET, handleCaptivePortalRoute);
  server.on("/gen_204", HTTP_GET, handleCaptivePortalRoute);
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptivePortalRoute);
  server.on("/library/test/success.html", HTTP_GET, handleCaptivePortalRoute);
  server.on("/ncsi.txt", HTTP_GET, handleCaptivePortalRoute);
  server.on("/connecttest.txt", HTTP_GET, handleCaptivePortalRoute);
  server.on("/fwlink", HTTP_GET, handleCaptivePortalRoute);
  server.on("/redirect", HTTP_GET, handleCaptivePortalRoute);

  server.onNotFound(handleNotFound);
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  ledOutputHigh = false;

  Serial.print("FORCE_PIN=");
  Serial.println(FORCE_PIN);
  Serial.print("ACCEL_X_PIN=");
  Serial.println(ACCEL_X_PIN);
  Serial.print("ACCEL_Y_PIN=");
  Serial.println(ACCEL_Y_PIN);
  Serial.print("ACCEL_Z_PIN=");
  Serial.println(ACCEL_Z_PIN);
  if (FORCE_PIN == ACCEL_X_PIN || FORCE_PIN == ACCEL_Y_PIN || FORCE_PIN == ACCEL_Z_PIN) {
    Serial.println("WARNING: FORCE_PIN conflicts with an accelerometer pin.");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, AP_HIDDEN, AP_MAX_CONNECTIONS);
  delay(150);

  dnsServer.start(DNS_PORT, "*", AP_IP);
  registerRoutes();
  server.begin();

  Serial.println();
  Serial.println("CPR Trainer standalone portal started.");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Password: ");
  Serial.println(AP_PASSWORD);
  Serial.print("Open: http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  updateStatusLed();
}
