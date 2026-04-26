import asyncio
import json
import math
import os
import statistics
import threading
import time
from collections import deque
from typing import Any

import serial
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel


class SerialConfig(BaseModel):
  port: str = os.getenv("SERIAL_PORT", "COM3")
  baud: int = int(os.getenv("SERIAL_BAUD", "115200"))


SIMULATOR_ENABLED = os.getenv("CPR_SIMULATOR", "0").lower() in {"1", "true", "yes", "on"}
SIMULATOR_BASELINE_FORCE = 520.0
SIMULATOR_SAMPLE_INTERVAL_MS = 20
SIMULATOR_SCRIPT = [
  {"label": "Push harder", "duration_s": 9.0, "cpm": 110.0, "amplitude": 150.0, "release_offset": 0.0},
  {"label": "Too slow", "duration_s": 11.0, "cpm": 82.0, "amplitude": 760.0, "release_offset": 0.0},
  {"label": "Good rate", "duration_s": 13.0, "cpm": 110.0, "amplitude": 760.0, "release_offset": 0.0},
  {"label": "Too hard", "duration_s": 9.0, "cpm": 110.0, "amplitude": 1120.0, "release_offset": 0.0},
  {"label": "Too fast", "duration_s": 11.0, "cpm": 136.0, "amplitude": 760.0, "release_offset": 0.0},
  {"label": "Release fully", "duration_s": 12.0, "cpm": 110.0, "amplitude": 760.0, "release_offset": 38.0},
]


class CalibrationState(BaseModel):
  running: bool = False
  started_at_ms: int = 0
  duration_ms: int = 3000


class CPRState:
  def __init__(self) -> None:
    self.reset_session()
    self.calibration = CalibrationState()

  def reset_session(self) -> None:
    self.baseline_force = 0.0
    self.max_force = 2500.0
    self.dynamic_threshold = 250.0
    self.last_force = 0.0
    self.in_compression = False
    self.last_compression_ms = -10_000
    self.debounce_ms = 250
    self.compression_count = 0
    self.releases_ok = 0
    self.intervals_ms: deque[float] = deque(maxlen=20)
    self.compression_times_ms: deque[float] = deque(maxlen=30)
    self.recent_force_scores: deque[float] = deque(maxlen=5)
    self.force_history: deque[dict[str, float]] = deque(maxlen=300)
    self.current_peak_force_above_baseline = 0.0
    self.last_feedback = "Push harder"
    self.last_metrics: dict[str, Any] = {}

  def start_calibration(self, duration_ms: int = 3000) -> None:
    self.calibration = CalibrationState(
      running=True,
      started_at_ms=int(time.time() * 1000),
      duration_ms=duration_ms,
    )
    self._cal_samples: list[float] = []

  def process_sample(self, sample: dict[str, Any]) -> dict[str, Any]:
    now_ms = float(sample.get("t", time.time() * 1000))
    force = float(sample.get("force", 0.0))

    self.force_history.append({"t": now_ms, "force": force})

    if self.calibration.running:
      self._cal_samples.append(force)
      elapsed = int(time.time() * 1000) - self.calibration.started_at_ms
      if elapsed >= self.calibration.duration_ms:
        self.calibration.running = False
        if self._cal_samples:
          self.baseline_force = float(statistics.fmean(self._cal_samples))
          p95 = sorted(self._cal_samples)[int(len(self._cal_samples) * 0.95)]
          self.max_force = max(self.baseline_force + 800.0, float(p95))
          self.dynamic_threshold = max(70.0, (self.max_force - self.baseline_force) * 0.12)

    force_above_baseline = max(0.0, force - self.baseline_force)
    threshold = self.baseline_force + self.dynamic_threshold

    # Compression detector:
    # 1) Enter compression when force rises above threshold.
    # 2) Count only once force falls back near baseline, with debounce guard.
    if not self.in_compression and force > threshold:
      self.in_compression = True

    release_threshold = self.baseline_force + max(40.0, self.dynamic_threshold * 0.4)
    if self.in_compression:
      self.current_peak_force_above_baseline = max(self.current_peak_force_above_baseline, force_above_baseline)

    if self.in_compression and force <= release_threshold:
      if now_ms - self.last_compression_ms >= self.debounce_ms:
        self.compression_count += 1
        if self.last_compression_ms > 0:
          self.intervals_ms.append(now_ms - self.last_compression_ms)
        self.compression_times_ms.append(now_ms)
        self.last_compression_ms = now_ms
        self.recent_force_scores.append(self._compute_force_score(self.current_peak_force_above_baseline))

        if force <= self.baseline_force + 30.0:
          self.releases_ok += 1
      self.in_compression = False
      self.current_peak_force_above_baseline = 0.0

    recent_cpm = self._compute_rate()
    rhythm_score = self._compute_rhythm_score()
    force_score = statistics.fmean(self.recent_force_scores) if self.recent_force_scores else self._compute_force_score(force_above_baseline)
    release_ratio = (self.releases_ok / self.compression_count) if self.compression_count > 0 else 1.0

    self.last_feedback = self._feedback_text(
      recent_cpm=recent_cpm,
      force_score=force_score,
      release_ratio=release_ratio,
    )

    metrics = {
      "compressionCount": self.compression_count,
      "compressionRate": recent_cpm,
      "forceLevel": force,
      "forceScore": force_score,
      "rhythmConsistency": rhythm_score,
      "releaseQuality": release_ratio,
      "baselineForce": self.baseline_force,
      "threshold": threshold,
      "feedback": self.last_feedback,
      "calibrating": self.calibration.running,
    }
    self.last_metrics = metrics
    return metrics

  def _compute_rate(self) -> float:
    if len(self.compression_times_ms) < 2:
      return 0.0
    elapsed_ms = self.compression_times_ms[-1] - self.compression_times_ms[0]
    if elapsed_ms <= 0:
      return 0.0
    return (len(self.compression_times_ms) - 1) * 60_000.0 / elapsed_ms

  def _compute_rhythm_score(self) -> float:
    if len(self.intervals_ms) < 3:
      return 1.0
    mean_interval = statistics.fmean(self.intervals_ms)
    if mean_interval <= 0:
      return 0.0
    std_dev = statistics.pstdev(self.intervals_ms)
    cv = std_dev / mean_interval
    return max(0.0, min(1.0, 1.0 - cv))

  def _compute_force_score(self, force_above_baseline: float) -> float:
    span = max(200.0, self.max_force - self.baseline_force)
    return max(0.0, min(1.0, force_above_baseline / span))

  def _feedback_text(self, recent_cpm: float, force_score: float, release_ratio: float) -> str:
    if release_ratio < 0.8:
      return "Release fully"
    if force_score < 0.25:
      return "Push harder"
    if recent_cpm < 100 and recent_cpm > 0:
      return "Too slow"
    if recent_cpm > 120:
      return "Too fast"
    if 100 <= recent_cpm <= 120:
      return "Good rate"
    return "Push harder"


class ConnectionManager:
  def __init__(self) -> None:
    self.clients: set[WebSocket] = set()

  async def connect(self, websocket: WebSocket) -> None:
    await websocket.accept()
    self.clients.add(websocket)

  def disconnect(self, websocket: WebSocket) -> None:
    self.clients.discard(websocket)

  async def broadcast(self, payload: dict[str, Any]) -> None:
    dead: list[WebSocket] = []
    for client in self.clients:
      try:
        await client.send_json(payload)
      except Exception:
        dead.append(client)
    for ws in dead:
      self.disconnect(ws)


app = FastAPI(title="CPR Trainer Backend")
app.add_middleware(
  CORSMiddleware,
  allow_origins=["*"],
  allow_credentials=True,
  allow_methods=["*"],
  allow_headers=["*"],
)

serial_cfg = SerialConfig()
state = CPRState()
manager = ConnectionManager()
loop: asyncio.AbstractEventLoop | None = None
simulator_started_at_ms = int(time.time() * 1000)


def build_payload(sample: dict[str, Any], metrics: dict[str, Any]) -> dict[str, Any]:
  return {
    "sample": {
      "t": sample.get("t"),
      "force": sample.get("force"),
      "ax": sample.get("ax"),
      "ay": sample.get("ay"),
      "az": sample.get("az"),
    },
    "metrics": metrics,
  }


def dashboard_data() -> dict[str, Any]:
  metrics = state.last_metrics
  raw_force = float(metrics.get("forceLevel", state.force_history[-1]["force"] if state.force_history else 0.0))
  baseline = float(metrics.get("baselineForce", state.baseline_force))
  force_corrected = max(0.0, raw_force - baseline)
  force_target = max(0.0, state.max_force - baseline)
  return {
    "forceRaw": raw_force,
    "forceCorrected": force_corrected,
    "forceTarget": force_target,
    "baselineForce": baseline,
    "rawTargetForce": baseline + force_target,
    "accelXCorrected": 0.0,
    "accelYCorrected": 0.0,
    "accelZCorrected": 0.0,
    "compressionCount": int(metrics.get("compressionCount", state.compression_count)),
    "compressionRate": float(metrics.get("compressionRate", 0.0)),
    "feedback": str(metrics.get("feedback", state.last_feedback)),
    "simulator": SIMULATOR_ENABLED,
  }


def broadcast_from_thread(payload: dict[str, Any]) -> None:
  if loop and not loop.is_closed():
    asyncio.run_coroutine_threadsafe(manager.broadcast(payload), loop)


def configure_simulator_baseline() -> None:
  state.baseline_force = SIMULATOR_BASELINE_FORCE
  state.max_force = SIMULATOR_BASELINE_FORCE + 800.0
  state.dynamic_threshold = 96.0


def current_simulator_phase(elapsed_s: float) -> dict[str, float | str]:
  total_duration = sum(float(phase["duration_s"]) for phase in SIMULATOR_SCRIPT)
  cursor = elapsed_s % total_duration
  for phase in SIMULATOR_SCRIPT:
    cursor -= float(phase["duration_s"])
    if cursor <= 0:
      return phase
  return SIMULATOR_SCRIPT[-1]


def simulator_sample(now_ms: float) -> dict[str, Any]:
  if state.calibration.running:
    force = SIMULATOR_BASELINE_FORCE + math.sin(now_ms / 95.0) * 4.0
  else:
    elapsed_s = max(0.0, (now_ms - simulator_started_at_ms) / 1000.0)
    phase = current_simulator_phase(elapsed_s)
    period_ms = 60_000.0 / float(phase["cpm"])
    active_ratio = 0.48
    cycle = (now_ms % period_ms) / period_ms
    pulse = 0.0
    if cycle < active_ratio:
      pulse = math.sin(math.pi * (cycle / active_ratio)) * float(phase["amplitude"])
    tremor = math.sin(now_ms / 41.0) * 3.0
    force = SIMULATOR_BASELINE_FORCE + float(phase["release_offset"]) + pulse + tremor

  return {
    "t": now_ms,
    "force": max(0.0, force),
    "ax": math.sin(now_ms / 180.0) * 0.03,
    "ay": math.cos(now_ms / 210.0) * 0.03,
    "az": 1.0,
  }


def simulator_thread() -> None:
  configure_simulator_baseline()
  print("CPR simulator enabled. Streaming fake force samples at 50 Hz.")
  while True:
    now_ms = time.time() * 1000.0
    sample = simulator_sample(now_ms)
    metrics = state.process_sample(sample)
    broadcast_from_thread(build_payload(sample, metrics))
    time.sleep(SIMULATOR_SAMPLE_INTERVAL_MS / 1000.0)


def serial_reader_thread() -> None:
  global loop
  while True:
    try:
      with serial.Serial(serial_cfg.port, serial_cfg.baud, timeout=1) as ser:
        print(f"Connected to {serial_cfg.port} @ {serial_cfg.baud}")
        while True:
          line = ser.readline().decode("utf-8", errors="ignore").strip()
          if not line:
            continue
          try:
            sample = json.loads(line)
            if not isinstance(sample, dict):
              continue
          except json.JSONDecodeError:
            continue

          if "force" not in sample:
            continue

          metrics = state.process_sample(sample)
          broadcast_from_thread(build_payload(sample, metrics))
    except serial.SerialException as exc:
      print(f"Serial error: {exc}. Retrying in 2s.")
      time.sleep(2)
    except Exception as exc:
      print(f"Unexpected serial thread error: {exc}. Retrying in 2s.")
      time.sleep(2)


@app.on_event("startup")
async def startup() -> None:
  global loop
  loop = asyncio.get_running_loop()
  target = simulator_thread if SIMULATOR_ENABLED else serial_reader_thread
  threading.Thread(target=target, daemon=True).start()


@app.get("/health")
async def health() -> dict[str, Any]:
  return {"ok": True, "serialPort": serial_cfg.port, "simulator": SIMULATOR_ENABLED}


@app.get("/simulator")
async def simulator_status() -> dict[str, Any]:
  return {"enabled": SIMULATOR_ENABLED, "script": SIMULATOR_SCRIPT}


@app.get("/data")
async def data() -> dict[str, Any]:
  return dashboard_data()


@app.post("/calibrate")
async def calibrate() -> dict[str, Any]:
  state.start_calibration()
  return {"ok": True, "calibrating": True, "durationMs": state.calibration.duration_ms}


@app.get("/calibrate/rest")
async def calibrate_rest() -> dict[str, Any]:
  if SIMULATOR_ENABLED:
    configure_simulator_baseline()
    return {"ok": True, "baselineForce": state.baseline_force}
  state.start_calibration()
  return {"ok": True, "calibrating": True, "durationMs": state.calibration.duration_ms}


@app.get("/calibrate/target")
async def calibrate_target() -> dict[str, Any]:
  if SIMULATOR_ENABLED:
    state.max_force = state.baseline_force + 800.0
    state.dynamic_threshold = max(70.0, (state.max_force - state.baseline_force) * 0.12)
    return {"ok": True, "targetForce": state.max_force - state.baseline_force}
  return {"ok": True, "targetForce": state.max_force - state.baseline_force}


@app.post("/session/reset")
async def reset_session() -> dict[str, Any]:
  global simulator_started_at_ms
  baseline = state.baseline_force
  state.reset_session()
  state.baseline_force = baseline
  if SIMULATOR_ENABLED:
    configure_simulator_baseline()
    simulator_started_at_ms = int(time.time() * 1000)
  return {"ok": True}


@app.post("/session/start")
async def start_session() -> dict[str, Any]:
  return await reset_session()


@app.websocket("/ws")
async def ws_stream(websocket: WebSocket) -> None:
  await manager.connect(websocket)
  try:
    while True:
      # Keep the socket alive; frontend can send pings if needed.
      _ = await websocket.receive_text()
  except WebSocketDisconnect:
    manager.disconnect(websocket)
  except Exception:
    manager.disconnect(websocket)
