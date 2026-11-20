import { decodePcmWave } from "./wav_decoder.js";

const EFFECT_NAMES = [
  "Input trim",
  "Volume pedal",
  "High-pass",
  "Low-pass",
  "Noise gate",
  "Pedal compressor",
  "Studio compressor",
  "Soft drive",
  "Three-band parametric EQ",
  "Stereo delay",
  "Output level",
  "Peak limiter",
];
const DEFAULT_PARAMETERS = {
  inputGainDb: 0,
  outputGainDb: 0,
  volumePedal: [1, -96, 0, 1],
  limiter: [-1, 80],
  compressor: [-18, 3, 6, 8, 100, 0],
  drive: 3,
  eq: [120, 0, 0.707, 800, 0, 0.707, 5000, 0, 0.707],
  delay: [350, 0.25, 0.25],
  filters: [80, 12000],
  pedalCompressor: [0.45, 0.45, 0.25, 0],
};
const STATS_HEADER_WORDS = 2;
const STATS_RING_BLOCKS = 512;
const STATS_EFFECT_SLOTS = EFFECT_NAMES.length + 2;

const fileInput = document.querySelector("#audio-file");
const playButton = document.querySelector("#play-button");
const stopButton = document.querySelector("#stop-button");
const rewindButton = document.querySelector("#rewind-button");
const audioStatus = document.querySelector("#audio-status");
const fileName = document.querySelector("#file-name");
const fileDetails = document.querySelector("#file-details");
const wetMix = document.querySelector("#wet-mix");
const wetMixValue = document.querySelector("#wet-mix-value");
const inputTrim = document.querySelector("#input-trim-db");
const inputTrimValue = document.querySelector("#input-trim-value");
const outputGain = document.querySelector("#output-gain-db");
const outputGainValue = document.querySelector("#output-gain-value");
const volumePosition = document.querySelector("#volume-position");
const volumePositionValue = document.querySelector("#volume-position-value");
const volumeMinimum = document.querySelector("#volume-min-db");
const volumeMinimumValue = document.querySelector("#volume-min-value");
const volumeMaximum = document.querySelector("#volume-max-db");
const volumeMaximumValue = document.querySelector("#volume-max-value");
const volumeTaper = document.querySelector("#volume-taper");
const limiterCeiling = document.querySelector("#limiter-ceiling-db");
const limiterCeilingValue = document.querySelector("#limiter-ceiling-value");
const limiterRelease = document.querySelector("#limiter-release-ms");
const limiterReleaseValue = document.querySelector("#limiter-release-value");
const loadRows = document.querySelector("#load-rows");
const browserVersion = document.querySelector("#browser-version");
const isolationNote = document.querySelector("#isolation-note");
const timerResolutionNote = document.querySelector("#timer-resolution-note");
const progressFill = document.querySelector("#progress-fill");
const testResult = document.querySelector("#browser-test-result");

let audioContext = null;
let decodedAudio = null;
let workletAdded = false;
let workletNode = null;
let processorReady = Promise.resolve();
let processorSetupTask = null;
let statsBuffer = null;
let stats = null;
let effectMemory = [];
let chainMemoryBytes = 0;
let wasmMemoryBytes = 0;
let timerResolution = "unknown";
let activeSource = null;
let playbackOffset = 0;
let playbackStartedAt = 0;
let progressTimer = 0;
let loadTask = Promise.resolve();
let playTask = Promise.resolve();
let preparedChannels = 0;
let pendingProcessorOptions = null;

function setAudioStatus(text, state = "") {
  audioStatus.textContent = text;
  audioStatus.dataset.state = state;
}

function bytesLabel(bytes) {
  if (!Number.isFinite(bytes) || bytes <= 0) return "—";
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  return `${(bytes / (1024 * 1024)).toFixed(2)} MiB`;
}

function enabledEffects() {
  return [...document.querySelectorAll("[data-effect]")].map((box) => box.checked);
}

function buildStatsBuffer() {
  if (!window.crossOriginIsolated || typeof SharedArrayBuffer === "undefined") {
    isolationNote.hidden = false;
    return null;
  }
  isolationNote.hidden = true;
  return new SharedArrayBuffer(
    Int32Array.BYTES_PER_ELEMENT * (STATS_HEADER_WORDS + STATS_RING_BLOCKS * STATS_EFFECT_SLOTS),
  );
}

async function getAudioContext() {
  if (audioContext) return audioContext;
  try {
    audioContext = new AudioContext({ sampleRate: 48000, latencyHint: "interactive" });
  } catch {
    audioContext = new AudioContext({ latencyHint: "interactive" });
  }
  audioContext.onstatechange = () => {
    if (audioContext?.state === "closed") setAudioStatus("Audio route closed", "error");
  };
  document.querySelector("#sample-rate").textContent = `${audioContext.sampleRate} Hz`;
  document.querySelector("#browser-version").textContent = navigator.userAgent;
  return audioContext;
}

function stopSource(updateOffset) {
  if (activeSource && audioContext) {
    if (updateOffset) {
      playbackOffset = Math.max(0, Math.min(
        decodedAudio?.duration || 0,
        playbackOffset + audioContext.currentTime - playbackStartedAt,
      ));
    }
    const source = activeSource;
    activeSource = null;
    source.onended = null;
    try { source.stop(); } catch { /* Already stopped. */ }
    try { source.disconnect(); } catch { /* Already disconnected. */ }
  }
  if (workletNode) workletNode.port.postMessage({ type: "active", value: false });
  stopButton.disabled = true;
  if (progressTimer) {
    cancelAnimationFrame(progressTimer);
    progressTimer = 0;
  }
}

function startSilentClock(context) {
  const source = context.createOscillator();
  const silence = context.createGain();
  silence.gain.value = 0;
  source.connect(silence);
  silence.connect(context.destination);
  source.start();
  return () => {
    try { source.stop(); } catch { /* Already stopped. */ }
    source.disconnect();
    silence.disconnect();
  };
}

async function prepareProcessor(testOptions = null) {
  if (workletNode && preparedChannels === decodedAudio.numberOfChannels) return processorReady;
  if (processorSetupTask) return processorSetupTask;
  processorSetupTask = setupProcessor(testOptions);
  try {
    return await processorSetupTask;
  } finally {
    processorSetupTask = null;
  }
}

async function setupProcessor(testOptions = null) {
  const context = await getAudioContext();
  const channels = decodedAudio.numberOfChannels;
  stopSource(false);
  if (workletNode) {
    try { workletNode.disconnect(); } catch { /* Already disconnected. */ }
    workletNode.port.close();
    workletNode = null;
  }
  if (!workletAdded) {
    if (!testResult.hidden) testResult.textContent = "Loading the browser audio worklet…";
    await context.audioWorklet.addModule("/web/audio-worklet.js");
    workletAdded = true;
  }
  if (!testResult.hidden) testResult.textContent = "Initializing the shared C++ WASM processor…";

  statsBuffer = buildStatsBuffer();
  stats = statsBuffer ? new Int32Array(statsBuffer) : null;
  const processorOptions = {
    channels,
    enabled: testOptions?.enabled || enabledEffects(),
    mix: Number(wetMix.value) / 100,
    parameters: testOptions?.parameters || DEFAULT_PARAMETERS,
  };
  if (statsBuffer) processorOptions.statsBuffer = statsBuffer;
  if (testOptions?.captureBuffer) processorOptions.captureBuffer = testOptions.captureBuffer;
  if (testOptions?.captureControl) processorOptions.captureControl = testOptions.captureControl;

  let resolveReady;
  let rejectReady;
  processorReady = new Promise((resolve, reject) => {
    resolveReady = resolve;
    rejectReady = reject;
  });
  workletNode = new AudioWorkletNode(context, "tabletfx-chain", {
    numberOfInputs: 1,
    numberOfOutputs: 1,
    outputChannelCount: [channels],
    channelCount: channels,
    channelCountMode: "explicit",
    processorOptions,
  });
  preparedChannels = channels;
  workletNode.port.onmessage = (event) => {
    const message = event.data;
    if (message?.type === "ready") {
      chainMemoryBytes = message.memoryBytes;
      wasmMemoryBytes = message.wasmMemoryBytes;
      effectMemory = message.effectMemoryBytes;
      timerResolution = message.timerResolution || "unknown";
      timerResolutionNote.hidden = timerResolution === "high-resolution";
      document.querySelector("#sample-rate").textContent = `${message.sampleRate} Hz`;
      document.querySelector("#block-size").textContent = `up to ${message.maxBlockFrames}`;
      document.querySelector("#wasm-memory-use").textContent = bytesLabel(wasmMemoryBytes);
      document.querySelector("#memory-use").textContent = bytesLabel(chainMemoryBytes);
      setAudioStatus("Chain ready", "ready");
      renderLoadRows(null);
      resolveReady(message);
    } else if (message?.type === "error") {
      setAudioStatus(message.message || "Audio processor failed", "error");
      rejectReady(new Error(message.message || "Audio processor failed"));
    }
  };
  workletNode.addEventListener("processorerror", () => {
    const error = new Error("The browser audio worklet processor failed during initialization.");
    setAudioStatus(error.message, "error");
    rejectReady(error);
  });
  workletNode.connect(context.destination);
  return processorReady;
}

async function loadFile(file, testOptions = null) {
  if (!file) return;
  stopSource(false);
  decodedAudio = null;
  playButton.disabled = true;
  stopButton.disabled = true;
  rewindButton.disabled = true;
  setAudioStatus("Decoding locally…");
  fileName.textContent = file.name;
  fileDetails.textContent = bytesLabel(file.size);
  try {
    if (!testResult.hidden) testResult.textContent = "Creating browser audio context…";
    const context = await getAudioContext();
    if (!testResult.hidden) testResult.textContent = "Reading the local audio file…";
    const bytes = await file.arrayBuffer();
    if (!testResult.hidden) testResult.textContent = "Decoding the local guitar audio…";
    const audio = decodePcmWave(context, bytes) || await context.decodeAudioData(bytes);
    if (audio.numberOfChannels < 1 || audio.numberOfChannels > 2) {
      throw new Error(`This test build supports mono or stereo input; this file has ${audio.numberOfChannels} channels.`);
    }
    decodedAudio = audio;
    playbackOffset = 0;
    fileDetails.textContent = `${audio.numberOfChannels} ch · ${audio.sampleRate} Hz · ${audio.duration.toFixed(2)} s`;
    pendingProcessorOptions = testOptions;
    playButton.disabled = false;
    rewindButton.disabled = false;
    setAudioStatus("Audio ready; chain initializes before playback", "ready");
  } catch (error) {
    decodedAudio = null;
    setAudioStatus(error instanceof Error ? error.message : String(error), "error");
    throw error;
  }
}

function updateProgress() {
  if (!audioContext || !decodedAudio || !activeSource) return;
  const seconds = playbackOffset + audioContext.currentTime - playbackStartedAt;
  const ratio = decodedAudio.duration > 0 ? Math.max(0, Math.min(1, seconds / decodedAudio.duration)) : 0;
  progressFill.style.width = `${ratio * 100}%`;
  progressTimer = requestAnimationFrame(updateProgress);
}

async function startPlayback() {
  if (!decodedAudio) return;
  if (!testResult.hidden) testResult.textContent = "Resuming the browser audio context…";
  const context = await getAudioContext();
  await context.resume();
  const stopClock = startSilentClock(context);
  try {
    await prepareProcessor(pendingProcessorOptions);
  } finally {
    stopClock();
  }
  pendingProcessorOptions = null;
  await processorReady;
  stopSource(false);
  if (playbackOffset >= decodedAudio.duration) playbackOffset = 0;
  workletNode.port.postMessage({ type: "reset" });
  workletNode.port.postMessage({ type: "active", value: true });
  const source = context.createBufferSource();
  source.buffer = decodedAudio;
  source.connect(workletNode);
  activeSource = source;
  playbackStartedAt = context.currentTime;
  source.onended = () => {
    if (activeSource !== source) return;
    activeSource = null;
    playbackOffset = 0;
    stopButton.disabled = true;
    progressFill.style.width = "100%";
    setAudioStatus("Playback finished", "ready");
  };
  source.start(0, playbackOffset);
  if (!testResult.hidden) testResult.textContent = "Audio source is playing through the processed chain…";
  stopButton.disabled = false;
  setAudioStatus("Playing processed chain", "ready");
  if (progressTimer) cancelAnimationFrame(progressTimer);
  progressTimer = requestAnimationFrame(updateProgress);
}

function readLoadWindow() {
  if (!stats) return null;
  const end = Atomics.load(stats, 0);
  if (end <= 0) return null;
  const start = Math.max(0, end - STATS_RING_BLOCKS);
  const count = end - start;
  const sums = new Float64Array(STATS_EFFECT_SLOTS - 1);
  const peaks = new Float64Array(STATS_EFFECT_SLOTS - 1);
  let sampleRate = audioContext?.sampleRate || 48000;
  let blockFrames = 128;
  for (let sequence = start; sequence < end; sequence += 1) {
    const slot = STATS_HEADER_WORDS + ((sequence % STATS_RING_BLOCKS) * STATS_EFFECT_SLOTS);
    const frames = Math.max(1, stats[slot + EFFECT_NAMES.length + 1]);
    blockFrames = frames;
    const deadlineMs = frames * 1000 / sampleRate;
    for (let effect = 0; effect < sums.length; effect += 1) {
      const percent = (stats[slot + effect] / 1000000) / deadlineMs * 100;
      sums[effect] += percent;
      peaks[effect] = Math.max(peaks[effect], percent);
    }
  }
  return { count, sums, peaks, sampleRate, blockFrames };
}

function renderLoadRows(load) {
  loadRows.replaceChildren();
  const rows = [];
  for (let i = 0; i < EFFECT_NAMES.length; i += 1) {
    const average = load ? `${(load.sums[i] / load.count).toFixed(3)}%` : "—";
    const peak = load ? `${load.peaks[i].toFixed(3)}%` : "—";
    rows.push([EFFECT_NAMES[i], average, peak, bytesLabel(effectMemory[i])]);
  }
  if (load) {
    const total = EFFECT_NAMES.length;
    rows.push([
      "Whole callback",
      `${(load.sums[total] / load.count).toFixed(3)}%`,
      `${load.peaks[total].toFixed(3)}%`,
      bytesLabel(chainMemoryBytes),
    ]);
    document.querySelector("#block-size").textContent = `${load.blockFrames} frames`;
    document.querySelector("#deadline").textContent = `${(load.blockFrames * 1000 / load.sampleRate).toFixed(3)} ms`;
  }
  for (const values of rows) {
    const row = document.createElement("tr");
    for (const value of values) {
      const cell = document.createElement("td");
      cell.textContent = value;
      row.append(cell);
    }
    loadRows.append(row);
  }
}

fileInput.addEventListener("change", () => {
  loadTask = loadFile(fileInput.files?.[0] || null);
  loadTask.catch(() => {});
});

playButton.addEventListener("click", () => {
  playTask = startPlayback();
  playTask.catch((error) => setAudioStatus(error.message || String(error), "error"));
});

stopButton.addEventListener("click", () => {
  stopSource(true);
  setAudioStatus("Stopped", "ready");
});

rewindButton.addEventListener("click", () => {
  stopSource(false);
  playbackOffset = 0;
  progressFill.style.width = "0%";
  if (decodedAudio) setAudioStatus("Rewound", "ready");
});

for (const checkbox of document.querySelectorAll("[data-effect]")) {
  checkbox.addEventListener("change", () => {
    if (workletNode) {
      workletNode.port.postMessage({
        type: "enable",
        index: Number(checkbox.dataset.effect),
        value: checkbox.checked,
      });
    }
  });
}

wetMix.addEventListener("input", () => {
  wetMixValue.value = `${wetMix.value}%`;
  if (workletNode) workletNode.port.postMessage({ type: "mix", value: Number(wetMix.value) / 100 });
});

inputTrim.addEventListener("input", () => {
  const value = Number(inputTrim.value);
  DEFAULT_PARAMETERS.inputGainDb = value;
  inputTrimValue.value = `${value.toFixed(1)} dB`;
  if (workletNode) workletNode.port.postMessage({ type: "input-gain", value });
});

outputGain.addEventListener("input", () => {
  const value = Number(outputGain.value);
  DEFAULT_PARAMETERS.outputGainDb = value;
  outputGainValue.value = `${value.toFixed(1)} dB`;
  if (workletNode) workletNode.port.postMessage({ type: "output-gain", value });
});

function updateVolumePedal() {
  const values = [
    Number(volumePosition.value) / 100,
    Number(volumeMinimum.value),
    Number(volumeMaximum.value),
    volumeTaper.value === "linear" ? 0 : 1,
  ];
  DEFAULT_PARAMETERS.volumePedal = values;
  volumePositionValue.value = `${volumePosition.value}%`;
  volumeMinimumValue.value = `${volumeMinimum.value} dB`;
  volumeMaximumValue.value = `${volumeMaximum.value} dB`;
  if (workletNode) workletNode.port.postMessage({ type: "volume-pedal", value: values });
}

for (const control of [volumePosition, volumeMinimum, volumeMaximum, volumeTaper]) {
  control.addEventListener("input", updateVolumePedal);
  control.addEventListener("change", updateVolumePedal);
}

function updateLimiter() {
  const values = [Number(limiterCeiling.value), Number(limiterRelease.value)];
  DEFAULT_PARAMETERS.limiter = values;
  limiterCeilingValue.value = `${limiterCeiling.value} dBFS`;
  limiterReleaseValue.value = `${limiterRelease.value} ms`;
  if (workletNode) workletNode.port.postMessage({ type: "limiter", value: values });
}

for (const control of [limiterCeiling, limiterRelease]) {
  control.addEventListener("input", updateLimiter);
  control.addEventListener("change", updateLimiter);
}

setInterval(() => {
  if (stats) {
    const load = readLoadWindow();
    if (load) renderLoadRows(load);
    const error = Atomics.load(stats, 1);
    if (error) setAudioStatus("Audio block exceeded the prepared processor capacity", "error");
  }
}, 500);

function readFloatWave(bytes) {
  const view = new DataView(bytes);
  if (view.getUint32(0, false) !== 0x52494646 || view.getUint32(8, false) !== 0x57415645) {
    throw new Error("Native reference is not a WAV file.");
  }
  let offset = 12;
  let format = 0;
  let channels = 0;
  let sampleRate = 0;
  let bits = 0;
  let sampleOffset = -1;
  let sampleBytes = 0;
  while (offset + 8 <= view.byteLength) {
    const id = view.getUint32(offset, false);
    const size = view.getUint32(offset + 4, true);
    const body = offset + 8;
    if (id === 0x666d7420) {
      format = view.getUint16(body, true);
      channels = view.getUint16(body + 2, true);
      sampleRate = view.getUint32(body + 4, true);
      bits = view.getUint16(body + 14, true);
    } else if (id === 0x64617461) {
      sampleOffset = body;
      sampleBytes = size;
      break;
    }
    offset = body + size + (size & 1);
  }
  if (format !== 3 || bits !== 32 || sampleOffset < 0 || sampleBytes % 4 !== 0) {
    throw new Error("Native reference must contain float32 WAV samples.");
  }
  const samples = new Float32Array(sampleBytes / 4);
  for (let i = 0; i < samples.length; i += 1) {
    samples[i] = view.getFloat32(sampleOffset + i * 4, true);
  }
  return { samples, channels, sampleRate, frames: samples.length / channels };
}

async function waitForCapture(control, expectedSamples) {
  const started = performance.now();
  while (Atomics.load(control, 1) === 0) {
    if (performance.now() - started > 8000) {
      throw new Error(`Timed out waiting for processed audio capture (${Atomics.load(control, 0)} of ${expectedSamples} samples).`);
    }
    await new Promise((resolve) => setTimeout(resolve, 20));
  }
}

async function runBrowserSelfTest() {
  testResult.hidden = false;
  testResult.dataset.status = "running";
  testResult.textContent = "Running browser audio test…";
  try {
    if (!window.crossOriginIsolated || typeof SharedArrayBuffer === "undefined") {
      throw new Error("The test server must enable cross-origin isolation for audio capture.");
    }
    const context = await getAudioContext();
    await context.resume();
    const stopClock = startSilentClock(context);
    testResult.textContent = "Checking browser AudioWorklet module loading…";
    try {
      await Promise.race([
        context.audioWorklet.addModule("/web/worklet-smoke.js"),
        new Promise((_, reject) => setTimeout(
          () => reject(new Error("Browser did not finish loading a basic AudioWorklet module.")),
          10000,
        )),
      ]);
    } finally {
      stopClock();
    }
    const [inputResponse, referenceResponse] = await Promise.all([
      fetch("/__test_input.wav"),
      fetch("/__test_expected.wav"),
    ]);
    testResult.textContent = "Reading local guitar fixture and native reference…";
    if (!inputResponse.ok || !referenceResponse.ok) throw new Error("Browser test fixtures were not served.");
    const [inputBytes, referenceBytes] = await Promise.all([
      inputResponse.arrayBuffer(),
      referenceResponse.arrayBuffer(),
    ]);
    const reference = readFloatWave(referenceBytes);
    const captureBuffer = new SharedArrayBuffer(reference.samples.length * Float32Array.BYTES_PER_ELEMENT);
    const captureControl = new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 2);
    const captureView = new Int32Array(captureControl);
    const testOptions = {
      enabled: [true, true, true, true, true, true, true, true, true, true, true, true],
      parameters: {
        inputGainDb: 2,
        outputGainDb: -1,
        volumePedal: [0.92, -96, 0, 1],
        limiter: [-1, 80],
        compressor: [-18, 3, 6, 8, 100, 0],
        drive: 3,
        eq: [120, 2, 0.707, 800, -1.5, 0.707, 5000, 1, 0.707],
        delay: [180, 0.25, 0.2],
        filters: [80, 12000],
        pedalCompressor: [0.45, 0.45, 0.25, 0],
      },
      captureBuffer,
      captureControl,
    };
    volumePosition.value = "92";
    volumeMinimum.value = "-96";
    volumeMaximum.value = "0";
    volumeTaper.value = "audio";
    limiterCeiling.value = "-1";
    limiterRelease.value = "80";
    updateVolumePedal();
    updateLimiter();
    for (const checkbox of document.querySelectorAll("[data-effect]")) {
      checkbox.checked = testOptions.enabled[Number(checkbox.dataset.effect)];
    }
    const file = new File([inputBytes], "browser-guitar-fixture.wav", { type: "audio/wav" });
    const transfer = new DataTransfer();
    transfer.items.add(file);
    fileInput.files = transfer.files;
    testResult.textContent = "Starting the local audio-file flow…";
    loadTask = loadFile(file, testOptions);
    await Promise.race([
      loadTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error(`Timed out loading browser audio file at: ${testResult.textContent}`)),
        5000,
      )),
    ]);
    testResult.textContent = "Playing the processed chain and capturing its output…";
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error(`Timed out starting browser playback at: ${testResult.textContent}`)),
        5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);

    const actual = new Float32Array(captureBuffer);
    if (actual.length !== reference.samples.length) throw new Error("Captured browser output has the wrong sample count.");
    let maximumError = 0;
    let errorSquares = 0;
    let nonZeroSamples = 0;
    for (let i = 0; i < actual.length; i += 1) {
      const error = Math.abs(actual[i] - reference.samples[i]);
      maximumError = Math.max(maximumError, error);
      errorSquares += error * error;
      if (Math.abs(actual[i]) > 1e-6) nonZeroSamples += 1;
    }
    const rmsError = Math.sqrt(errorSquares / Math.max(1, actual.length));
    const maxAbsTolerance = 0.0002;
    const rmsTolerance = 0.00002;
    if (maximumError > maxAbsTolerance || rmsError > rmsTolerance || nonZeroSamples === 0) {
      throw new Error(`Browser/native audio mismatch: max error ${maximumError} (limit ${maxAbsTolerance}), RMS ${rmsError} (limit ${rmsTolerance}).`);
    }
    if (audioContext.state !== "running") throw new Error("Play did not start the browser audio context.");
    const load = readLoadWindow();
    const report = {
      result: "passed",
      browser: navigator.userAgent,
      sampleRate: audioContext.sampleRate,
      channels: reference.channels,
      frames: reference.frames,
      maximumAbsoluteError: maximumError,
      rmsError,
      maxAbsoluteTolerance: maxAbsTolerance,
      rmsTolerance,
      processorMemoryBytes: chainMemoryBytes,
      wasmLinearMemoryBytes: wasmMemoryBytes,
      timingResolution: timerResolution,
      callbackLoad: load ? {
        blockFrames: load.blockFrames,
        averagePercentByProcessor: [...load.sums].map((value) => value / load.count),
        peakPercentByProcessor: [...load.peaks],
      } : null,
      localFileDecode: fileName.textContent === file.name,
      playOutputCaptured: nonZeroSamples > 0,
    };
    testResult.dataset.status = "passed";
    testResult.textContent = JSON.stringify(report);
    document.title = "PASS · TabletFX browser audio test";
  } catch (error) {
    testResult.dataset.status = "failed";
    testResult.textContent = error instanceof Error ? error.message : String(error);
    document.title = "FAIL · TabletFX browser audio test";
  }
}

if (new URLSearchParams(location.search).has("selftest")) {
  void runBrowserSelfTest();
}
