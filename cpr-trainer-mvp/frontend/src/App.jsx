import { useEffect, useMemo, useRef, useState } from "react";

const isLocalDevHost = ["localhost", "127.0.0.1"].includes(window.location.hostname);
const configuredDeviceBase = import.meta.env.VITE_DEVICE_BASE;
const DEVICE_BASE = (configuredDeviceBase ?? (isLocalDevHost ? "http://192.168.4.1" : "")).replace(/\/$/, "");
const POLL_INTERVAL_MS = 50;
const MAX_FORCE_POINTS = 100;
const VOICE_COOLDOWN_MS = 2800;
const TARGET_RATE_MIN = 100;
const TARGET_RATE_MAX = 120;
const GAUGE_RATE_MAX = 150;

// Tunable thresholds after on-device testing.
const RELEASE_THRESHOLD = 30;
const COMPRESSION_START_THRESHOLD = 45;
const PUSH_HARDER_RATIO = 0.6;
const TOO_HARD_RATIO = 1.3;

const EMPTY_DATA = {
  forceRaw: 0,
  forceCorrected: 0,
  forceTarget: 0,
  baselineForce: 0,
  rawTargetForce: 0,
  restCalibrated: false,
  targetCalibrated: false,
  motionMagnitude: 0,
  accelXCorrected: 0,
  accelYCorrected: 0,
  accelZCorrected: 0,
  compressionCount: 0,
  compressionRate: 0,
  feedback: "Start compressions",
  forceFeedback: "Start compressions",
};

const FEEDBACK_META = {
  "Good compression": { tone: "good", cue: "Relative force is in target range." },
  "Good rate": { tone: "good", cue: "Keep compressions steady." },
  "Push harder": { tone: "warn", cue: "Peak force is below 60% of your calibrated target." },
  "Too slow": { tone: "warn", cue: "Speed up toward 100 to 120 compressions per minute." },
  "Too fast": { tone: "warn", cue: "Slow down toward 100 to 120 compressions per minute." },
  "Too hard": { tone: "alert", cue: "Peak force is above 130% of your calibrated target." },
  "Release fully": { tone: "alert", cue: "Do not lean between compressions." },
  "Start compressions": { tone: "idle", cue: "Begin when ready." },
};

function sleep(ms) {
  return new Promise((resolve) => {
    window.setTimeout(resolve, ms);
  });
}

function endpoint(path) {
  return `${DEVICE_BASE}${path}`;
}

function MetricCard({ title, value, unit = "", status = "neutral" }) {
  return (
    <article className={`metric-card ${status}`}>
      <p className="metric-title">{title}</p>
      <p className="metric-value">
        {value}
        {unit}
      </p>
    </article>
  );
}

function RateGauge({ rate }) {
  const safeRate = Number.isFinite(rate) ? rate : 0;
  const percent = Math.max(0, Math.min(100, (safeRate / GAUGE_RATE_MAX) * 100));
  const targetLeft = (TARGET_RATE_MIN / GAUGE_RATE_MAX) * 100;
  const targetWidth = ((TARGET_RATE_MAX - TARGET_RATE_MIN) / GAUGE_RATE_MAX) * 100;
  const isTarget = safeRate >= TARGET_RATE_MIN && safeRate <= TARGET_RATE_MAX;
  const ticks = [0, TARGET_RATE_MIN, TARGET_RATE_MAX, GAUGE_RATE_MAX];

  return (
    <section className="rate-panel" aria-label="Compression rate">
      <div className="rate-head">
        <p>Compression Rate</p>
        <strong className={isTarget ? "in-target" : ""}>{safeRate.toFixed(1)} CPM</strong>
      </div>
      <div className="rate-track">
        <span className="target-zone" style={{ left: `${targetLeft}%`, width: `${targetWidth}%` }} />
        <span className="rate-needle" style={{ left: `${percent}%` }} />
      </div>
      <div className="rate-scale" aria-hidden="true">
        {ticks.map((tick) => (
          <span key={tick} style={{ left: `${(tick / GAUGE_RATE_MAX) * 100}%` }}>
            {tick}
          </span>
        ))}
      </div>
    </section>
  );
}

function ForceGraph({ points, targetLine }) {
  if (points.length < 2) {
    return <p className="graph-empty">Waiting for compression effort data...</p>;
  }

  const maxObserved = Math.max(...points, targetLine, 1);
  const graphMax = Math.max(100, Math.ceil(maxObserved / 100) * 100);
  const targetY = 100 - (Math.max(0, targetLine) / graphMax) * 100;

  const polyline = points
    .map((value, index) => {
      const x = (index / (points.length - 1)) * 100;
      const y = 100 - (Math.max(0, value) / graphMax) * 100;
      return `${x},${y}`;
    })
    .join(" ");

  return (
    <div className="force-plot">
      <span className="graph-axis-label graph-axis-top">{graphMax}</span>
      <span className="graph-axis-label graph-axis-bottom">0</span>
      {targetLine > 0 && (
        <span className="threshold-tag" style={{ top: `${targetY}%` }}>
          target {Math.round(targetLine)}
        </span>
      )}
      <svg className="force-graph" viewBox="0 0 100 100" preserveAspectRatio="none" role="img" aria-label="Compression effort graph">
        {targetLine > 0 && <line className="threshold-line" x1="0" x2="100" y1={targetY} y2={targetY} />}
        <polyline points={polyline} fill="none" stroke="currentColor" strokeWidth="2" vectorEffect="non-scaling-stroke" />
      </svg>
    </div>
  );
}

export default function App() {
  const [isConnected, setIsConnected] = useState(false);
  const [pollError, setPollError] = useState(false);
  const [voiceEnabled, setVoiceEnabled] = useState(false);
  const [sensorData, setSensorData] = useState(EMPTY_DATA);
  const [forcePoints, setForcePoints] = useState([]);
  const [qualityFeedback, setQualityFeedback] = useState("Start compressions");
  const [calibrationState, setCalibrationState] = useState({
    open: false,
    running: false,
    step: 0,
    title: "",
    detail: "",
    phase: "",
    countdown: null,
    error: "",
  });
  const latestForceRef = useRef(0);
  const lastSpokenRef = useRef({ text: "", at: 0 });
  const compressionRef = useRef({
    active: false,
    peak: 0,
    lastCompletedAt: 0,
    lastResult: "Start compressions",
  });

  useEffect(() => {
    let isMounted = true;
    async function pollData() {
      try {
        const response = await fetch(endpoint("/data"), { cache: "no-store" });
        if (!response.ok) {
          throw new Error(`Failed (${response.status})`);
        }
        const data = await response.json();
        if (!isMounted) {
          return;
        }

        const forceCorrected = Math.round(Number(data.forceCorrected ?? 0));
        const forceTarget = Math.round(Number(data.forceTarget ?? 0));
        const baselineForce = Math.round(Number(data.forceRestBaseline ?? data.baselineForce ?? 0));
        const rawTargetForce = Math.round(Number(data.rawTargetForce ?? baselineForce + forceTarget));
        const next = {
          forceRaw: Number(data.forceRaw ?? 0),
          forceCorrected,
          forceTarget,
          baselineForce,
          rawTargetForce,
          restCalibrated: Boolean(data.restCalibrated),
          targetCalibrated: Boolean(data.targetCalibrated),
          motionMagnitude: Number(data.motionMagnitude ?? 0),
          accelXCorrected: Number(data.accelXCorrected ?? 0),
          accelYCorrected: Number(data.accelYCorrected ?? 0),
          accelZCorrected: Number(data.accelZCorrected ?? 0),
          compressionCount: Number(data.compressionCount ?? 0),
          compressionRate: Number(data.compressionRate ?? 0),
          feedback: String(data.rateFeedback ?? data.feedback ?? "Start compressions"),
          forceFeedback: String(data.forceFeedback ?? "Start compressions"),
        };

        latestForceRef.current = forceCorrected;
        setSensorData(next);
        setForcePoints((prev) => [...prev, forceCorrected].slice(-MAX_FORCE_POINTS));
        setIsConnected(true);
        setPollError(false);
      } catch (error) {
        if (isMounted) {
          setIsConnected(false);
          setPollError(true);
        }
      }
    }

    pollData();
    const pollId = window.setInterval(pollData, POLL_INTERVAL_MS);

    return () => {
      isMounted = false;
      window.clearInterval(pollId);
    };
  }, []);

  useEffect(() => {
    const force = sensorData.forceCorrected;
    const target = sensorData.forceTarget;
    const detector = compressionRef.current;
    if (force >= COMPRESSION_START_THRESHOLD && !detector.active) {
      detector.active = true;
      detector.peak = force;
    }
    if (detector.active) {
      detector.peak = Math.max(detector.peak, force);
      if (force <= RELEASE_THRESHOLD) {
        detector.active = false;
        detector.lastCompletedAt = Date.now();

        if (target > 0) {
          const ratio = detector.peak / target;
          if (ratio < PUSH_HARDER_RATIO) {
            detector.lastResult = "Push harder";
          } else if (ratio > TOO_HARD_RATIO) {
            detector.lastResult = "Too hard";
          } else {
            detector.lastResult = "Good compression";
          }
        } else {
          detector.lastResult = "Good compression";
        }
        detector.peak = 0;
      }
    }

    if (!detector.active && force > RELEASE_THRESHOLD && Date.now() - detector.lastCompletedAt < 1500) {
      setQualityFeedback("Release fully");
      return;
    }
    setQualityFeedback(detector.lastResult);
  }, [sensorData.forceCorrected, sensorData.forceTarget]);

  const feedback = useMemo(() => FEEDBACK_META[qualityFeedback] ?? FEEDBACK_META["Start compressions"], [qualityFeedback]);
  const voicePrompt = useMemo(() => {
    return FEEDBACK_META[sensorData.feedback] ? sensorData.feedback : qualityFeedback;
  }, [qualityFeedback, sensorData.feedback]);

  useEffect(() => {
    if (!voiceEnabled || !("speechSynthesis" in window)) {
      return;
    }

    function speakIfReady() {
      if (!voicePrompt || voicePrompt === "Start compressions") {
        return;
      }

      const now = Date.now();
      if (voicePrompt === lastSpokenRef.current.text && now - lastSpokenRef.current.at < VOICE_COOLDOWN_MS) {
        return;
      }

      window.speechSynthesis.cancel();
      const utterance = new SpeechSynthesisUtterance(voicePrompt);
      utterance.rate = 1.05;
      utterance.pitch = 1;
      utterance.volume = 0.9;
      window.speechSynthesis.speak(utterance);
      lastSpokenRef.current = { text: voicePrompt, at: now };
    }

    speakIfReady();
    const repeatId = window.setInterval(speakIfReady, 500);

    return () => {
      window.clearInterval(repeatId);
    };
  }, [voiceEnabled, voicePrompt]);

  const motionMagnitude = useMemo(() => {
    if (sensorData.motionMagnitude > 0) {
      return sensorData.motionMagnitude;
    }
    return Math.abs(sensorData.accelXCorrected) + Math.abs(sensorData.accelYCorrected) + Math.abs(sensorData.accelZCorrected);
  }, [sensorData.accelXCorrected, sensorData.accelYCorrected, sensorData.accelZCorrected, sensorData.motionMagnitude]);

  async function runCountdown(step, title, detail) {
    setCalibrationState({
      open: true,
      running: true,
      step,
      title,
      detail,
      phase: "Read the instruction",
      countdown: null,
      error: "",
    });
    await sleep(1600);

    for (const cue of ["Ready", "Set", "Go"]) {
      setCalibrationState((prev) => ({
        ...prev,
        phase: cue,
        countdown: null,
      }));
      await sleep(700);
    }

    for (let value = 3; value > 0; value -= 1) {
      setCalibrationState((prev) => ({
        ...prev,
        phase: "Hold steady",
        countdown: value,
        error: "",
      }));
      await sleep(1000);
    }
    setCalibrationState((prev) => ({ ...prev, countdown: null }));
  }

  async function callCalibrationEndpoint(path) {
    const controller = new AbortController();
    const timeoutId = window.setTimeout(() => controller.abort(), 4000);
    let response;
    try {
      response = await fetch(endpoint(path), {
        method: "GET",
        cache: "no-store",
        signal: controller.signal,
      });
    } finally {
      window.clearTimeout(timeoutId);
    }
    if (!response.ok) {
      throw new Error(`Calibration call failed (${response.status})`);
    }
  }

  async function waitForRelease(maxWaitMs) {
    const startedAt = Date.now();
    while (Date.now() - startedAt < maxWaitMs) {
      if (latestForceRef.current <= RELEASE_THRESHOLD) {
        return true;
      }
      await sleep(120);
    }
    return false;
  }

  function closeCalibrationModal() {
    setCalibrationState({
      open: false,
      running: false,
      step: 0,
      title: "",
      detail: "",
      phase: "",
      countdown: null,
      error: "",
    });
  }

  async function startGuidedCalibration() {
    try {
      await runCountdown(1, "Step 1", "Place the trainer flat and do not press down.");
      await callCalibrationEndpoint("/calibrate/rest");
      setCalibrationState((prev) => ({
        ...prev,
        title: "Step 1 complete",
        detail: "Rest baseline saved.",
        phase: "",
      }));
      await sleep(800);

      await runCountdown(2, "Step 2", "Press down with what feels like a good CPR compression and hold.");
      await callCalibrationEndpoint("/calibrate/target");
      setCalibrationState((prev) => ({
        ...prev,
        title: "Step 2 complete",
        detail: "Target compression saved.",
        phase: "",
      }));
      await sleep(800);

      setCalibrationState({
        open: true,
        running: true,
        step: 3,
        title: "Step 3",
        detail: "Release fully.",
        phase: "Waiting for recoil",
        countdown: null,
        error: "",
      });

      // Tunable recoil confirmation threshold.
      const releasedInTime = await waitForRelease(6000);
      if (!releasedInTime) {
        throw new Error("Release confirmation timed out");
      }

      setCalibrationState((prev) => ({
        ...prev,
        title: "Calibration complete",
        detail: "Recoil baseline confirmed.",
        phase: "",
      }));
      await sleep(1100);
      closeCalibrationModal();
    } catch (error) {
      setCalibrationState((prev) => ({
        ...prev,
        running: false,
        phase: "",
        error: "Calibration failed or timed out. Check connection/recoil and try again.",
      }));
    } finally {
      // no-op
    }
  }

  return (
    <main className="app">
      <header className="topbar">
        <div>
          <p className="eyebrow">Portable Trainer</p>
          <h1>CPR Coach</h1>
        </div>
        <span className={isConnected ? "status connected" : "status disconnected"}>
          {isConnected ? "ESP32 Live" : "Waiting for Signal"}
        </span>
      </header>

      <section className="control-bar" aria-label="Trainer controls">
        <button type="button" onClick={startGuidedCalibration} disabled={calibrationState.running}>
          {calibrationState.running ? "Calibrating..." : "Start Guided Calibration"}
        </button>
        <label className="voice-toggle">
          <input type="checkbox" checked={voiceEnabled} onChange={(event) => setVoiceEnabled(event.target.checked)} />
          <span>Voice prompts</span>
        </label>
      </section>

      {pollError && <p className="error-banner">Connect to CPR_Trainer Wi-Fi. If you are using Vite locally, make sure the ESP32 is reachable at http://192.168.4.1.</p>}

      <section className={`feedback-card ${feedback.tone}`}>
        <p className="feedback-label">Compression Quality</p>
        <p className="feedback-message">{qualityFeedback}</p>
        <p className="feedback-cue">{feedback.cue}</p>
      </section>

      <section className="trainer-grid">
        <RateGauge rate={sensorData.compressionRate} />
        <article className="scores-panel">
          <p className="feedback-label">Motion Quality</p>
          <p className="metric-value">{motionMagnitude.toFixed(1)}</p>
          <p className="score-note">Relative movement from corrected accelerometer channels.</p>
        </article>
      </section>

      <section className="metrics-grid">
        <MetricCard title="Compression Count" value={sensorData.compressionCount} />
        <MetricCard title="Compression Rate" value={sensorData.compressionRate.toFixed(1)} unit=" CPM" />
        <MetricCard title="Raw Force" value={Math.round(sensorData.forceRaw)} />
        <MetricCard title="Rest Baseline" value={sensorData.baselineForce} />
        <MetricCard title="Relative Force" value={sensorData.forceCorrected} />
        <MetricCard title="Target Effort" value={Math.round(sensorData.forceTarget || 0)} />
        <MetricCard title="Raw Target" value={sensorData.rawTargetForce} />
        <MetricCard title="Recoil Threshold" value={RELEASE_THRESHOLD} />
        <MetricCard title="Rate Feedback" value={sensorData.feedback} />
        <MetricCard title="Force Feedback" value={sensorData.forceFeedback} />
      </section>

      <section className="graph-card">
        <div className="graph-head">
          <h2>Relative Force Trend</h2>
          <span>raw force minus baseline - last {MAX_FORCE_POINTS} readings</span>
        </div>
        <div className="graph-wrap">
          <ForceGraph points={forcePoints} targetLine={sensorData.forceTarget} />
        </div>
      </section>

      {calibrationState.open && (
        <section className="calibration-overlay" role="dialog" aria-modal="true" aria-label="Guided calibration">
          <div className="calibration-card">
            <p className="feedback-label">{calibrationState.title}</p>
            <h2>{calibrationState.detail}</h2>
            {calibrationState.phase && <p className="calibration-phase">{calibrationState.phase}</p>}
            {calibrationState.countdown !== null && <p className="calibration-countdown">{calibrationState.countdown}</p>}
            {calibrationState.error && <p className="error-banner">{calibrationState.error}</p>}
            {!calibrationState.running && (
              <button type="button" className="secondary-button" onClick={closeCalibrationModal}>
                Close
              </button>
            )}
          </div>
        </section>
      )}
    </main>
  );
}
