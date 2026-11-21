import { decodePcmWave } from "./wav_decoder.js";

const EFFECT_NAMES = [
  "Input trim",
  "Volume pedal",
  "High-pass",
  "Low-pass",
  "Noise gate",
  "Pedal compressor",
  "Studio compressor",
  "Wah",
  "Envelope wah",
  "Clean boost",
  "Transparent overdrive",
  "Mid-focused overdrive",
  "High-gain distortion",
  "Round fuzz",
  "Soft drive",
  "Tremolo",
  "Chorus",
  "Flanger",
  "Phaser",
  "Vibrato",
  "Three-band parametric EQ",
  "Stereo delay",
  "Polyphonic pitch shifter",
  "Output level",
  "Peak limiter",
];
const DEFAULT_PARAMETERS = {
  inputGainDb: 0,
  outputGainDb: 0,
  volumePedal: [1, -96, 0, 1],
  noiseGate: [-60, -72, 3, 2, 20, 80],
  cleanBoostDb: 0,
  overdrive: [0.15, -3, 0.35],
  midOverdrive: [0.18, 6, -4, 0.4],
  highGainDistortion: [0.75, 100, 9000, 5200, -9, 1],
  roundFuzz: [0.55, -6, 0.85],
  limiter: [-1, 80],
  wah: [0.5, 350, 2200, 4, 0],
  envelopeWah: [0.5, 350, 2200, 4, 0, 5, 80, 5, 0],
  tremolo: [5, 0.5, 0, 0, 120, 1],
  chorus: [0.8, 15, 7, 0.3],
  flanger: [0.25, 2, 1, 0.35, 0, 0.35],
  phaser: [0.5, 250, 1600, 0.2, 4, 0.5],
  vibrato: [5, 0.35, 1],
  compressor: [-18, 3, 6, 8, 100, 0],
  drive: 3,
  eq: [120, 0, 0.707, 800, 0, 0.707, 5000, 0, 0.707],
  delay: [350, 0.25, 0.25],
  delayTempo: [0, 120, 4],
  delayPingPong: false,
  delayDualEnabled: false,
  delayDual: [350, 0.25, 0.25, 525, 0.25, 0.25],
  delayMultiTapEnabled: false,
  delayMultiTap: [250, 0.65, -0.55, 500, 0.45, 0.55,
    750, 0.30, -0.25, 1000, 0.20, 0.25],
  pitchSemitones: 0,
  filters: [80, 12000],
  pedalCompressor: [0.45, 0.45, 0.25, 0],
};
const STATS_HEADER_WORDS = 2;
const STATS_RING_BLOCKS = 512;
const STATS_EFFECT_SLOTS = EFFECT_NAMES.length + 16;
const METER_INPUT_PEAK = EFFECT_NAMES.length + 2;
const METER_INPUT_RMS = EFFECT_NAMES.length + 3;
const METER_OUTPUT_PEAK = EFFECT_NAMES.length + 4;
const METER_OUTPUT_RMS = EFFECT_NAMES.length + 5;
const METER_FLAGS = EFFECT_NAMES.length + 6;
const METER_INPUT_INVALID = EFFECT_NAMES.length + 7;
const METER_OUTPUT_INVALID = EFFECT_NAMES.length + 8;
const TUNER_GENERATION = EFFECT_NAMES.length + 9;
const TUNER_NOTE = EFFECT_NAMES.length + 10;
const TUNER_VALID = EFFECT_NAMES.length + 11;
const TUNER_FREQUENCY = EFFECT_NAMES.length + 12;
const TUNER_CENTS = EFFECT_NAMES.length + 13;
const TUNER_CONFIDENCE = EFFECT_NAMES.length + 14;
const TUNER_TIME = EFFECT_NAMES.length + 15;

const fileInput = document.querySelector("#audio-file");
const playButton = document.querySelector("#play-button");
const stopButton = document.querySelector("#stop-button");
const rewindButton = document.querySelector("#rewind-button");
const audioStatus = document.querySelector("#audio-status");
const inputPeakBar = document.querySelector("#input-peak-bar");
const inputPeakValue = document.querySelector("#input-peak-value");
const inputRmsValue = document.querySelector("#input-rms-value");
const inputMeterStatus = document.querySelector("#input-meter-status");
const inputMeterDiagnostics = document.querySelector("#input-meter-diagnostics");
const outputPeakBar = document.querySelector("#output-peak-bar");
const outputPeakValue = document.querySelector("#output-peak-value");
const outputRmsValue = document.querySelector("#output-rms-value");
const outputMeterStatus = document.querySelector("#output-meter-status");
const outputMeterDiagnostics = document.querySelector("#output-meter-diagnostics");
const clearMeterClips = document.querySelector("#clear-meter-clips");
const fileName = document.querySelector("#file-name");
const fileDetails = document.querySelector("#file-details");
const wetMix = document.querySelector("#wet-mix");
const wetMixValue = document.querySelector("#wet-mix-value");
const inputMute = document.querySelector("#input-mute");
const globalBypass = document.querySelector("#global-bypass");
const panicMute = document.querySelector("#panic-mute");
const inputTrim = document.querySelector("#input-trim-db");
const inputTrimValue = document.querySelector("#input-trim-value");
const outputGain = document.querySelector("#output-gain-db");
const outputGainValue = document.querySelector("#output-gain-value");
const pitchSemitones = document.querySelector("#pitch-semitones");
const pitchSemitonesValue = document.querySelector("#pitch-semitones-value");
const volumePosition = document.querySelector("#volume-position");
const volumePositionValue = document.querySelector("#volume-position-value");
const volumeMinimum = document.querySelector("#volume-min-db");
const volumeMinimumValue = document.querySelector("#volume-min-value");
const volumeMaximum = document.querySelector("#volume-max-db");
const volumeMaximumValue = document.querySelector("#volume-max-value");
const volumeTaper = document.querySelector("#volume-taper");
const noiseGateThreshold = document.querySelector("#noise-gate-threshold-db");
const noiseGateThresholdValue = document.querySelector("#noise-gate-threshold-value");
const noiseGateAttenuation = document.querySelector("#noise-gate-attenuation-db");
const noiseGateAttenuationValue = document.querySelector("#noise-gate-attenuation-value");
const noiseGateHysteresis = document.querySelector("#noise-gate-hysteresis-db");
const noiseGateHysteresisValue = document.querySelector("#noise-gate-hysteresis-value");
const noiseGateAttack = document.querySelector("#noise-gate-attack-ms");
const noiseGateAttackValue = document.querySelector("#noise-gate-attack-value");
const noiseGateHold = document.querySelector("#noise-gate-hold-ms");
const noiseGateHoldValue = document.querySelector("#noise-gate-hold-value");
const noiseGateRelease = document.querySelector("#noise-gate-release-ms");
const noiseGateReleaseValue = document.querySelector("#noise-gate-release-value");
const limiterCeiling = document.querySelector("#limiter-ceiling-db");
const limiterCeilingValue = document.querySelector("#limiter-ceiling-value");
const limiterRelease = document.querySelector("#limiter-release-ms");
const limiterReleaseValue = document.querySelector("#limiter-release-value");
const wahPosition = document.querySelector("#wah-position");
const wahPositionValue = document.querySelector("#wah-position-value");
const wahMinimum = document.querySelector("#wah-min-hz");
const wahMinimumValue = document.querySelector("#wah-min-value");
const wahMaximum = document.querySelector("#wah-max-hz");
const wahMaximumValue = document.querySelector("#wah-max-value");
const wahResonance = document.querySelector("#wah-q");
const wahResonanceValue = document.querySelector("#wah-q-value");
const wahLevel = document.querySelector("#wah-level-db");
const wahLevelValue = document.querySelector("#wah-level-value");
const touchWahSensitivity = document.querySelector("#touch-wah-sensitivity");
const touchWahSensitivityValue = document.querySelector("#touch-wah-sensitivity-value");
const touchWahDirection = document.querySelector("#touch-wah-direction");
const touchWahMinimum = document.querySelector("#touch-wah-min-hz");
const touchWahMinimumValue = document.querySelector("#touch-wah-min-value");
const touchWahMaximum = document.querySelector("#touch-wah-max-hz");
const touchWahMaximumValue = document.querySelector("#touch-wah-max-value");
const touchWahResonance = document.querySelector("#touch-wah-q");
const touchWahResonanceValue = document.querySelector("#touch-wah-q-value");
const touchWahLevel = document.querySelector("#touch-wah-level-db");
const touchWahLevelValue = document.querySelector("#touch-wah-level-value");
const touchWahAttack = document.querySelector("#touch-wah-attack-ms");
const touchWahAttackValue = document.querySelector("#touch-wah-attack-value");
const touchWahRelease = document.querySelector("#touch-wah-release-ms");
const touchWahReleaseValue = document.querySelector("#touch-wah-release-value");
const touchWahSmoothing = document.querySelector("#touch-wah-smoothing-ms");
const touchWahSmoothingValue = document.querySelector("#touch-wah-smoothing-value");
const cleanBoost = document.querySelector("#clean-boost-db");
const cleanBoostValue = document.querySelector("#clean-boost-value");
const overdriveDrive = document.querySelector("#overdrive-drive");
const overdriveDriveValue = document.querySelector("#overdrive-drive-value");
const overdriveLevel = document.querySelector("#overdrive-level-db");
const overdriveLevelValue = document.querySelector("#overdrive-level-value");
const overdriveMix = document.querySelector("#overdrive-mix");
const overdriveMixValue = document.querySelector("#overdrive-mix-value");
const midOverdriveDrive = document.querySelector("#mid-overdrive-drive");
const midOverdriveDriveValue = document.querySelector("#mid-overdrive-drive-value");
const midOverdriveBoost = document.querySelector("#mid-overdrive-boost-db");
const midOverdriveBoostValue = document.querySelector("#mid-overdrive-boost-value");
const midOverdriveLevel = document.querySelector("#mid-overdrive-level-db");
const midOverdriveLevelValue = document.querySelector("#mid-overdrive-level-value");
const midOverdriveMix = document.querySelector("#mid-overdrive-mix");
const midOverdriveMixValue = document.querySelector("#mid-overdrive-mix-value");
const highGainDrive = document.querySelector("#high-gain-drive");
const highGainDriveValue = document.querySelector("#high-gain-drive-value");
const highGainPreLowCut = document.querySelector("#high-gain-pre-low-cut-hz");
const highGainPreLowCutValue = document.querySelector("#high-gain-pre-low-cut-value");
const highGainPreHighCut = document.querySelector("#high-gain-pre-high-cut-hz");
const highGainPreHighCutValue = document.querySelector("#high-gain-pre-high-cut-value");
const highGainPostTone = document.querySelector("#high-gain-post-tone-hz");
const highGainPostToneValue = document.querySelector("#high-gain-post-tone-value");
const highGainLevel = document.querySelector("#high-gain-level-db");
const highGainLevelValue = document.querySelector("#high-gain-level-value");
const highGainMix = document.querySelector("#high-gain-mix");
const highGainMixValue = document.querySelector("#high-gain-mix-value");
const roundFuzzAmount = document.querySelector("#round-fuzz-amount");
const roundFuzzAmountValue = document.querySelector("#round-fuzz-amount-value");
const roundFuzzLevel = document.querySelector("#round-fuzz-level-db");
const roundFuzzLevelValue = document.querySelector("#round-fuzz-level-value");
const roundFuzzMix = document.querySelector("#round-fuzz-mix");
const roundFuzzMixValue = document.querySelector("#round-fuzz-mix-value");
const tunerReference = document.querySelector("#tuner-reference-hz");
const tunerReferenceValue = document.querySelector("#tuner-reference-value");
const tunerMuteOutput = document.querySelector("#tuner-mute-output");
const tunerReadingOutput = document.querySelector("#tuner-reading");
const tremoloRate = document.querySelector("#tremolo-rate-hz");
const tremoloRateValue = document.querySelector("#tremolo-rate-value");
const tremoloDepth = document.querySelector("#tremolo-depth");
const tremoloDepthValue = document.querySelector("#tremolo-depth-value");
const tremoloWaveform = document.querySelector("#tremolo-waveform");
const tremoloSync = document.querySelector("#tremolo-sync");
const tremoloBpm = document.querySelector("#tremolo-tempo-bpm");
const tremoloBpmValue = document.querySelector("#tremolo-tempo-value");
const tremoloDivision = document.querySelector("#tremolo-beats-per-cycle");
const chorusRate = document.querySelector("#chorus-rate-hz");
const chorusRateValue = document.querySelector("#chorus-rate-value");
const chorusDelay = document.querySelector("#chorus-delay-ms");
const chorusDelayValue = document.querySelector("#chorus-delay-value");
const chorusDepth = document.querySelector("#chorus-depth-ms");
const chorusDepthValue = document.querySelector("#chorus-depth-value");
const chorusMix = document.querySelector("#chorus-mix");
const chorusMixValue = document.querySelector("#chorus-mix-value");
const flangerRate = document.querySelector("#flanger-rate-hz");
const flangerRateValue = document.querySelector("#flanger-rate-value");
const flangerDelay = document.querySelector("#flanger-delay-ms");
const flangerDelayValue = document.querySelector("#flanger-delay-value");
const flangerDepth = document.querySelector("#flanger-depth-ms");
const flangerDepthValue = document.querySelector("#flanger-depth-value");
const flangerFeedback = document.querySelector("#flanger-feedback");
const flangerFeedbackValue = document.querySelector("#flanger-feedback-value");
const flangerPolarity = document.querySelector("#flanger-feedback-polarity");
const flangerMix = document.querySelector("#flanger-mix");
const flangerMixValue = document.querySelector("#flanger-mix-value");
const phaserRate = document.querySelector("#phaser-rate-hz");
const phaserRateValue = document.querySelector("#phaser-rate-value");
const phaserMinimum = document.querySelector("#phaser-min-hz");
const phaserMinimumValue = document.querySelector("#phaser-min-value");
const phaserMaximum = document.querySelector("#phaser-max-hz");
const phaserMaximumValue = document.querySelector("#phaser-max-value");
const phaserFeedback = document.querySelector("#phaser-feedback");
const phaserFeedbackValue = document.querySelector("#phaser-feedback-value");
const phaserStages = document.querySelector("#phaser-stages");
const phaserMix = document.querySelector("#phaser-mix");
const phaserMixValue = document.querySelector("#phaser-mix-value");
const vibratoRate = document.querySelector("#vibrato-rate-hz");
const vibratoRateValue = document.querySelector("#vibrato-rate-value");
const vibratoDepth = document.querySelector("#vibrato-depth-ms");
const vibratoDepthValue = document.querySelector("#vibrato-depth-value");
const vibratoMix = document.querySelector("#vibrato-mix");
const vibratoMixValue = document.querySelector("#vibrato-mix-value");
const delayPreset = document.querySelector("#delay-preset");
const delayRouting = document.querySelector("#delay-routing");
const delayDualControls = document.querySelector("#delay-dual-controls");
const delayDualAms = document.querySelector("#delay-dual-a-ms");
const delayDualAmsValue = document.querySelector("#delay-dual-a-ms-value");
const delayDualAFeedback = document.querySelector("#delay-dual-a-feedback");
const delayDualAFeedbackValue = document.querySelector("#delay-dual-a-feedback-value");
const delayDualAMix = document.querySelector("#delay-dual-a-mix");
const delayDualAMixValue = document.querySelector("#delay-dual-a-mix-value");
const delayDualBms = document.querySelector("#delay-dual-b-ms");
const delayDualBmsValue = document.querySelector("#delay-dual-b-ms-value");
const delayDualBFeedback = document.querySelector("#delay-dual-b-feedback");
const delayDualBFeedbackValue = document.querySelector("#delay-dual-b-feedback-value");
const delayDualBMix = document.querySelector("#delay-dual-b-mix");
const delayDualBMixValue = document.querySelector("#delay-dual-b-mix-value");
const delayMultiTapControls = document.querySelector("#delay-multitap-controls");
const delayMultiTapInputs = Array.from({ length: 4 }, (_, index) => {
  const tap = index + 1;
  return {
    time: document.querySelector(`#delay-tap-${tap}-ms`),
    timeValue: document.querySelector(`#delay-tap-${tap}-ms-value`),
    level: document.querySelector(`#delay-tap-${tap}-level`),
    levelValue: document.querySelector(`#delay-tap-${tap}-level-value`),
    pan: document.querySelector(`#delay-tap-${tap}-pan`),
    panValue: document.querySelector(`#delay-tap-${tap}-pan-value`),
  };
});
const delayMode = document.querySelector("#delay-time-mode");
const delayMs = document.querySelector("#delay-time-ms");
const delayMsValue = document.querySelector("#delay-time-ms-value");
const delayBpm = document.querySelector("#delay-tempo-bpm");
const delayBpmValue = document.querySelector("#delay-tempo-bpm-value");
const delayDivision = document.querySelector("#delay-division");
const delayFeedback = document.querySelector("#delay-feedback");
const delayFeedbackValue = document.querySelector("#delay-feedback-value");
const delayMix = document.querySelector("#delay-mix");
const delayMixValue = document.querySelector("#delay-mix-value");
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
let statsFloats = null;
let effectMemory = [];
let tunerMemoryBytes = 0;
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
let lastTunerGeneration = 0;

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
  statsFloats = statsBuffer ? new Float32Array(statsBuffer) : null;
  const processorOptions = {
    channels,
    enabled: testOptions?.enabled || enabledEffects(),
    mix: Number(wetMix.value) / 100,
    inputMuted: testOptions?.inputMuted ?? inputMute.checked,
    globalBypass: testOptions?.globalBypass ?? globalBypass.checked,
    panicMuted: testOptions?.panicMuted ?? panicMute.getAttribute("aria-pressed") === "true",
    tunerReferenceHz: testOptions?.tunerReferenceHz ?? Number(tunerReference.value),
    tunerMuted: testOptions?.tunerMuted ?? tunerMuteOutput.checked,
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
      tunerMemoryBytes = message.tunerMemoryBytes || 0;
      timerResolution = message.timerResolution || "unknown";
      timerResolutionNote.hidden = timerResolution === "high-resolution";
      document.querySelector("#sample-rate").textContent = `${message.sampleRate} Hz`;
      document.querySelector("#block-size").textContent = `up to ${message.maxBlockFrames}`;
      document.querySelector("#wasm-memory-use").textContent = bytesLabel(wasmMemoryBytes);
      document.querySelector("#memory-use").textContent = bytesLabel(chainMemoryBytes);
      setAudioStatus("Chain ready", "ready");
      lastTunerGeneration = 0;
      renderLoadRows(null);
      resolveReady(message);
    } else if (message?.type === "tuner-reading") {
      renderTunerReading(message);
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
  const sums = new Float64Array(EFFECT_NAMES.length + 2);
  const peaks = new Float64Array(EFFECT_NAMES.length + 2);
  let sampleRate = audioContext?.sampleRate || 48000;
  let blockFrames = 128;
  for (let sequence = start; sequence < end; sequence += 1) {
    const slot = STATS_HEADER_WORDS + ((sequence % STATS_RING_BLOCKS) * STATS_EFFECT_SLOTS);
    const frames = Math.max(1, stats[slot + EFFECT_NAMES.length + 1]);
    blockFrames = frames;
    const deadlineMs = frames * 1000 / sampleRate;
    for (let effect = 0; effect < EFFECT_NAMES.length; effect += 1) {
      const percent = (stats[slot + effect] / 1000000) / deadlineMs * 100;
      sums[effect] += percent;
      peaks[effect] = Math.max(peaks[effect], percent);
    }
    const callbackPercent = (stats[slot + EFFECT_NAMES.length] / 1000000) / deadlineMs * 100;
    const tunerPercent = (stats[slot + TUNER_TIME] / 1000000) / deadlineMs * 100;
    sums[EFFECT_NAMES.length] += tunerPercent;
    peaks[EFFECT_NAMES.length] = Math.max(peaks[EFFECT_NAMES.length], tunerPercent);
    sums[EFFECT_NAMES.length + 1] += callbackPercent;
    peaks[EFFECT_NAMES.length + 1] = Math.max(peaks[EFFECT_NAMES.length + 1], callbackPercent);
  }
  return { count, sums, peaks, sampleRate, blockFrames };
}

function readLatestMeters() {
  if (!stats || !statsFloats) return null;
  const end = Atomics.load(stats, 0);
  if (end <= 0) return null;
  return readMetersAtSequence(end - 1);
}

function readLatestTuner() {
  if (!stats || !statsFloats) return null;
  const end = Atomics.load(stats, 0);
  if (end <= 0) return null;
  const slot = STATS_HEADER_WORDS + (((end - 1) % STATS_RING_BLOCKS) * STATS_EFFECT_SLOTS);
  return {
    generation: Math.max(0, stats[slot + TUNER_GENERATION]),
    midiNote: stats[slot + TUNER_NOTE],
    valid: stats[slot + TUNER_VALID] !== 0,
    frequencyHz: statsFloats[slot + TUNER_FREQUENCY],
    cents: statsFloats[slot + TUNER_CENTS],
    confidence: statsFloats[slot + TUNER_CONFIDENCE],
  };
}

function renderTunerReading(reading) {
  if (!reading || reading.generation === 0) {
    tunerReadingOutput.textContent = "Play guitar audio to detect a note.";
    return;
  }
  if (!reading.valid) {
    tunerReadingOutput.textContent = "No stable note detected.";
    return;
  }
  const names = ["C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"];
  const noteNumber = reading.midiNote;
  const note = `${names[((noteNumber % 12) + 12) % 12]}${Math.floor(noteNumber / 12) - 1}`;
  const cents = Number(reading.cents);
  const direction = Math.abs(cents) < 5 ? "in tune" : cents > 0 ? "sharp" : "flat";
  const signedCents = `${cents > 0 ? "+" : ""}${cents.toFixed(1)} cents`;
  tunerReadingOutput.textContent = `${note} · ${signedCents} (${direction}) · ${Number(reading.frequencyHz).toFixed(1)} Hz`;
}

function readMetersAtSequence(sequence) {
  if (!stats || !statsFloats) return null;
  const slot = STATS_HEADER_WORDS + ((sequence % STATS_RING_BLOCKS) * STATS_EFFECT_SLOTS);
  return {
    sequence,
    frames: Math.max(0, stats[slot + EFFECT_NAMES.length + 1]),
    input: {
      peak: statsFloats[slot + METER_INPUT_PEAK],
      rms: statsFloats[slot + METER_INPUT_RMS],
      clipped: (stats[slot + METER_FLAGS] & 0x3) !== 0,
      invalidSamples: Math.max(0, stats[slot + METER_INPUT_INVALID]),
    },
    output: {
      peak: statsFloats[slot + METER_OUTPUT_PEAK],
      rms: statsFloats[slot + METER_OUTPUT_RMS],
      clipped: ((stats[slot + METER_FLAGS] >>> 2) & 0x3) !== 0,
      invalidSamples: Math.max(0, stats[slot + METER_OUTPUT_INVALID]),
    },
  };
}

function findLastNonSilentInputMeter() {
  if (!stats || !statsFloats) return null;
  const end = Atomics.load(stats, 0);
  const start = Math.max(0, end - STATS_RING_BLOCKS);
  for (let sequence = end - 1; sequence >= start; sequence -= 1) {
    const meter = readMetersAtSequence(sequence);
    if (meter?.input.peak > 0) return meter;
  }
  return null;
}

function dbfs(value) {
  if (!Number.isFinite(value) || value <= 0) return "−∞ dBFS";
  const level = 20 * Math.log10(value);
  return `${level > 0 ? "+" : ""}${level.toFixed(1)} dBFS`;
}

function renderOneMeter(reading, frames, peakBar, peakText, rmsText, status, diagnostics) {
  if (!reading) {
    peakBar.value = 0;
    peakText.textContent = "Peak —";
    rmsText.textContent = "RMS —";
    status.textContent = "No blocks yet";
    status.dataset.clipped = "false";
    diagnostics.textContent = "Waiting for audio blocks.";
    return;
  }
  peakBar.value = Math.min(1, Math.max(0, reading.peak));
  peakText.textContent = `Peak ${dbfs(reading.peak)}`;
  rmsText.textContent = `RMS ${dbfs(reading.rms)}`;
  status.textContent = reading.clipped ? "CLIP latched" : "No clip";
  status.dataset.clipped = String(reading.clipped);
  const invalid = reading.invalidSamples;
  diagnostics.textContent = `${frames} frames · ${invalid} invalid sample${invalid === 1 ? "" : "s"} in latest block`;
}

function renderLevelMeters(meters) {
  renderOneMeter(meters?.input, meters?.frames || 0,
    inputPeakBar, inputPeakValue, inputRmsValue, inputMeterStatus, inputMeterDiagnostics);
  renderOneMeter(meters?.output, meters?.frames || 0,
    outputPeakBar, outputPeakValue, outputRmsValue, outputMeterStatus, outputMeterDiagnostics);
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
      "Chromatic tuner analysis",
      `${(load.sums[total] / load.count).toFixed(3)}%`,
      `${load.peaks[total].toFixed(3)}%`,
      bytesLabel(tunerMemoryBytes),
    ]);
    rows.push([
      "Whole callback",
      `${(load.sums[total + 1] / load.count).toFixed(3)}%`,
      `${load.peaks[total + 1].toFixed(3)}%`,
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

inputMute.addEventListener("change", () => {
  if (workletNode) workletNode.port.postMessage({ type: "input-mute", value: inputMute.checked });
});

globalBypass.addEventListener("change", () => {
  if (workletNode) workletNode.port.postMessage({ type: "global-bypass", value: globalBypass.checked });
});

panicMute.addEventListener("click", () => {
  const muted = panicMute.getAttribute("aria-pressed") !== "true";
  panicMute.setAttribute("aria-pressed", String(muted));
  panicMute.textContent = muted ? "Release panic mute" : "Panic mute";
  if (workletNode) workletNode.port.postMessage({ type: "panic-mute", value: muted });
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

pitchSemitones.addEventListener("input", () => {
  const value = Number(pitchSemitones.value);
  DEFAULT_PARAMETERS.pitchSemitones = value;
  pitchSemitonesValue.value = `${value > 0 ? "+" : ""}${value.toFixed(1)} semitones`;
  if (workletNode) workletNode.port.postMessage({ type: "pitch-semitones", value });
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

function updateNoiseGate() {
  const values = [
    Number(noiseGateThreshold.value),
    Number(noiseGateAttenuation.value),
    Number(noiseGateHysteresis.value),
    Number(noiseGateAttack.value),
    Number(noiseGateHold.value),
    Number(noiseGateRelease.value),
  ];
  DEFAULT_PARAMETERS.noiseGate = values;
  noiseGateThresholdValue.value = `${noiseGateThreshold.value} dB`;
  noiseGateAttenuationValue.value = `${noiseGateAttenuation.value} dB`;
  noiseGateHysteresisValue.value = `${noiseGateHysteresis.value} dB`;
  noiseGateAttackValue.value = `${noiseGateAttack.value} ms`;
  noiseGateHoldValue.value = `${noiseGateHold.value} ms`;
  noiseGateReleaseValue.value = `${noiseGateRelease.value} ms`;
  if (workletNode) workletNode.port.postMessage({ type: "noise-gate", value: values });
}

for (const control of [noiseGateThreshold, noiseGateAttenuation, noiseGateHysteresis,
  noiseGateAttack, noiseGateHold, noiseGateRelease]) {
  control.addEventListener("input", updateNoiseGate);
  control.addEventListener("change", updateNoiseGate);
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

function updateWah() {
  const values = [
    Number(wahPosition.value) / 100,
    Number(wahMinimum.value),
    Number(wahMaximum.value),
    Number(wahResonance.value),
    Number(wahLevel.value),
  ];
  DEFAULT_PARAMETERS.wah = values;
  wahPositionValue.value = `${wahPosition.value}%`;
  wahMinimumValue.value = `${wahMinimum.value} Hz`;
  wahMaximumValue.value = `${wahMaximum.value} Hz`;
  wahResonanceValue.value = wahResonance.value;
  wahLevelValue.value = `${wahLevel.value} dB`;
  if (workletNode) workletNode.port.postMessage({ type: "wah", value: values });
}

for (const control of [wahPosition, wahMinimum, wahMaximum, wahResonance, wahLevel]) {
  control.addEventListener("input", updateWah);
  control.addEventListener("change", updateWah);
}

function updateEnvelopeWah() {
  const values = [
    Number(touchWahSensitivity.value),
    Number(touchWahMinimum.value),
    Number(touchWahMaximum.value),
    Number(touchWahResonance.value),
    Number(touchWahLevel.value),
    Number(touchWahAttack.value),
    Number(touchWahRelease.value),
    Number(touchWahSmoothing.value),
    touchWahDirection.value === "down" ? 1 : 0,
  ];
  DEFAULT_PARAMETERS.envelopeWah = values;
  touchWahSensitivityValue.value = Number(touchWahSensitivity.value).toFixed(2);
  touchWahMinimumValue.value = `${touchWahMinimum.value} Hz`;
  touchWahMaximumValue.value = `${touchWahMaximum.value} Hz`;
  touchWahResonanceValue.value = touchWahResonance.value;
  touchWahLevelValue.value = `${touchWahLevel.value} dB`;
  touchWahAttackValue.value = `${touchWahAttack.value} ms`;
  touchWahReleaseValue.value = `${touchWahRelease.value} ms`;
  touchWahSmoothingValue.value = `${touchWahSmoothing.value} ms`;
  if (workletNode) workletNode.port.postMessage({ type: "envelope-wah", value: values });
}

for (const control of [touchWahSensitivity, touchWahDirection, touchWahMinimum,
  touchWahMaximum, touchWahResonance, touchWahLevel, touchWahAttack,
  touchWahRelease, touchWahSmoothing]) {
  control.addEventListener("input", updateEnvelopeWah);
  control.addEventListener("change", updateEnvelopeWah);
}

cleanBoost.addEventListener("input", () => {
  const value = Number(cleanBoost.value);
  DEFAULT_PARAMETERS.cleanBoostDb = value;
  cleanBoostValue.value = `${value.toFixed(1)} dB`;
  if (workletNode) workletNode.port.postMessage({ type: "clean-boost", value });
});

function updateOverdrive() {
  const values = [Number(overdriveDrive.value), Number(overdriveLevel.value),
    Number(overdriveMix.value)];
  DEFAULT_PARAMETERS.overdrive = values;
  overdriveDriveValue.value = Number(overdriveDrive.value).toFixed(2);
  overdriveLevelValue.value = `${Number(overdriveLevel.value).toFixed(1)} dB`;
  overdriveMixValue.value = `${Math.round(Number(overdriveMix.value) * 100)}%`;
  if (workletNode) workletNode.port.postMessage({ type: "overdrive", value: values });
}

for (const control of [overdriveDrive, overdriveLevel, overdriveMix]) {
  control.addEventListener("input", updateOverdrive);
  control.addEventListener("change", updateOverdrive);
}

function updateMidOverdrive() {
  const values = [Number(midOverdriveDrive.value), Number(midOverdriveBoost.value),
    Number(midOverdriveLevel.value), Number(midOverdriveMix.value)];
  DEFAULT_PARAMETERS.midOverdrive = values;
  midOverdriveDriveValue.value = Number(midOverdriveDrive.value).toFixed(2);
  midOverdriveBoostValue.value = `${Number(midOverdriveBoost.value).toFixed(1)} dB`;
  midOverdriveLevelValue.value = `${Number(midOverdriveLevel.value).toFixed(1)} dB`;
  midOverdriveMixValue.value = `${Math.round(Number(midOverdriveMix.value) * 100)}%`;
  if (workletNode) workletNode.port.postMessage({ type: "mid-overdrive", value: values });
}

for (const control of [midOverdriveDrive, midOverdriveBoost,
  midOverdriveLevel, midOverdriveMix]) {
  control.addEventListener("input", updateMidOverdrive);
  control.addEventListener("change", updateMidOverdrive);
}

function updateHighGainDistortion() {
  const values = [Number(highGainDrive.value), Number(highGainPreLowCut.value),
    Number(highGainPreHighCut.value), Number(highGainPostTone.value),
    Number(highGainLevel.value), Number(highGainMix.value)];
  DEFAULT_PARAMETERS.highGainDistortion = values;
  highGainDriveValue.value = Number(highGainDrive.value).toFixed(2);
  highGainPreLowCutValue.value = `${Math.round(Number(highGainPreLowCut.value))} Hz`;
  highGainPreHighCutValue.value = `${Math.round(Number(highGainPreHighCut.value))} Hz`;
  highGainPostToneValue.value = `${Math.round(Number(highGainPostTone.value))} Hz`;
  highGainLevelValue.value = `${Number(highGainLevel.value).toFixed(1)} dB`;
  highGainMixValue.value = `${Math.round(Number(highGainMix.value) * 100)}%`;
  if (workletNode) workletNode.port.postMessage({ type: "high-gain-distortion", value: values });
}

for (const control of [highGainDrive, highGainPreLowCut, highGainPreHighCut,
  highGainPostTone, highGainLevel, highGainMix]) {
  control.addEventListener("input", updateHighGainDistortion);
  control.addEventListener("change", updateHighGainDistortion);
}

function updateRoundFuzz() {
  const values = [Number(roundFuzzAmount.value), Number(roundFuzzLevel.value),
    Number(roundFuzzMix.value)];
  DEFAULT_PARAMETERS.roundFuzz = values;
  roundFuzzAmountValue.value = Number(roundFuzzAmount.value).toFixed(2);
  roundFuzzLevelValue.value = `${Number(roundFuzzLevel.value).toFixed(1)} dB`;
  roundFuzzMixValue.value = `${Math.round(Number(roundFuzzMix.value) * 100)}%`;
  if (workletNode) workletNode.port.postMessage({ type: "round-fuzz", value: values });
}

for (const control of [roundFuzzAmount, roundFuzzLevel, roundFuzzMix]) {
  control.addEventListener("input", updateRoundFuzz);
  control.addEventListener("change", updateRoundFuzz);
}

function updateTremolo() {
  const waveform = { sine: 0, triangle: 1, square: 2 }[tremoloWaveform.value] ?? 0;
  const values = [
    Number(tremoloRate.value),
    Number(tremoloDepth.value),
    waveform,
    tremoloSync.value === "on" ? 1 : 0,
    Number(tremoloBpm.value),
    Number(tremoloDivision.value),
  ];
  DEFAULT_PARAMETERS.tremolo = values;
  tremoloRateValue.value = `${Number(tremoloRate.value).toFixed(2)} Hz`;
  tremoloDepthValue.value = `${Math.round(Number(tremoloDepth.value) * 100)}%`;
  tremoloBpmValue.value = `${tremoloBpm.value} BPM`;
  if (workletNode) workletNode.port.postMessage({ type: "tremolo", value: values });
}

for (const control of [tremoloRate, tremoloDepth, tremoloWaveform,
  tremoloSync, tremoloBpm, tremoloDivision]) {
  control.addEventListener("input", updateTremolo);
  control.addEventListener("change", updateTremolo);
}

function updateChorus() {
  const delayMs = Number(chorusDelay.value);
  const maximumDepthMs = Math.max(0, Math.min(14, delayMs - 1, 30 - delayMs));
  chorusDepth.max = String(maximumDepthMs);
  chorusDepth.value = String(Math.min(Number(chorusDepth.value), maximumDepthMs));
  const values = [
    Number(chorusRate.value),
    delayMs,
    Number(chorusDepth.value),
    Number(chorusMix.value),
  ];
  DEFAULT_PARAMETERS.chorus = values;
  chorusRateValue.value = `${Number(chorusRate.value).toFixed(2)} Hz`;
  chorusDelayValue.value = `${Number(chorusDelay.value).toFixed(1)} ms`;
  chorusDepthValue.value = `${Number(chorusDepth.value).toFixed(1)} ms`;
  chorusMixValue.value = `${Math.round(Number(chorusMix.value) * 100)}%`;
  if (workletNode) workletNode.port.postMessage({ type: "chorus", value: values });
}

for (const control of [chorusRate, chorusDelay, chorusDepth, chorusMix]) {
  control.addEventListener("input", updateChorus);
  control.addEventListener("change", updateChorus);
}

function updateFlanger() {
  const delayMs = Number(flangerDelay.value);
  const maximumDepthMs = Math.max(0, Math.min(4.5, delayMs - 0.25, 12 - delayMs));
  flangerDepth.max = String(maximumDepthMs);
  flangerDepth.value = String(Math.min(Number(flangerDepth.value), maximumDepthMs));
  const polarity = flangerPolarity.value === "negative" ? 1 : 0;
  const values = [
    Number(flangerRate.value), delayMs, Number(flangerDepth.value),
    Number(flangerFeedback.value), polarity, Number(flangerMix.value),
  ];
  DEFAULT_PARAMETERS.flanger = values;
  flangerRateValue.value = `${Number(flangerRate.value).toFixed(2)} Hz`;
  flangerDelayValue.value = `${delayMs.toFixed(2)} ms`;
  flangerDepthValue.value = `${Number(flangerDepth.value).toFixed(2)} ms`;
  flangerFeedbackValue.value = `${Math.round(Number(flangerFeedback.value) * 100)}%`;
  flangerMixValue.value = `${Math.round(Number(flangerMix.value) * 100)}%`;
  if (workletNode) workletNode.port.postMessage({ type: "flanger", value: values });
}

for (const control of [flangerRate, flangerDelay, flangerDepth, flangerFeedback,
  flangerPolarity, flangerMix]) {
  control.addEventListener("input", updateFlanger);
  control.addEventListener("change", updateFlanger);
}

function updatePhaser() {
  let minimumHz = Number(phaserMinimum.value);
  let maximumHz = Number(phaserMaximum.value);
  if (minimumHz > maximumHz) [minimumHz, maximumHz] = [maximumHz, minimumHz];
  phaserMinimum.value = String(minimumHz);
  phaserMaximum.value = String(maximumHz);
  const values = [
    Number(phaserRate.value), minimumHz, maximumHz,
    Number(phaserFeedback.value), Number(phaserStages.value), Number(phaserMix.value),
  ];
  DEFAULT_PARAMETERS.phaser = values;
  phaserRateValue.value = `${Number(phaserRate.value).toFixed(2)} Hz`;
  phaserMinimumValue.value = `${minimumHz.toFixed(0)} Hz`;
  phaserMaximumValue.value = `${maximumHz.toFixed(0)} Hz`;
  phaserFeedbackValue.value = `${Number(phaserFeedback.value).toFixed(2)}`;
  phaserMixValue.value = `${Math.round(Number(phaserMix.value) * 100)}%`;
  if (workletNode) workletNode.port.postMessage({ type: "phaser", value: values });
}

for (const control of [phaserRate, phaserMinimum, phaserMaximum,
  phaserFeedback, phaserStages, phaserMix]) {
  control.addEventListener("input", updatePhaser);
  control.addEventListener("change", updatePhaser);
}

function updateVibrato() {
  const values = [Number(vibratoRate.value), Number(vibratoDepth.value),
    Number(vibratoMix.value)];
  DEFAULT_PARAMETERS.vibrato = values;
  vibratoRateValue.value = `${Number(vibratoRate.value).toFixed(1)} Hz`;
  vibratoDepthValue.value = `${Number(vibratoDepth.value).toFixed(2)} ms`;
  vibratoMixValue.value = `${Math.round(Number(vibratoMix.value) * 100)}%`;
  if (workletNode) workletNode.port.postMessage({ type: "vibrato", value: values });
}

for (const control of [vibratoRate, vibratoDepth, vibratoMix]) {
  control.addEventListener("input", updateVibrato);
  control.addEventListener("change", updateVibrato);
}

function updateDelay(keepPreset = false) {
  if (!keepPreset) delayPreset.value = "custom";
  const tempoSync = delayMode.value === "tempo";
  delayMs.disabled = tempoSync;
  delayBpm.disabled = !tempoSync;
  delayDivision.disabled = !tempoSync;
  const delayMsNumber = Number(delayMs.value);
  const bpm = Number(delayBpm.value);
  const division = Number(delayDivision.value);
  const feedback = Number(delayFeedback.value);
  const mix = Number(delayMix.value);
  DEFAULT_PARAMETERS.delay = [delayMsNumber, feedback, mix];
  DEFAULT_PARAMETERS.delayTempo = [tempoSync ? 1 : 0, bpm, division];
  delayMsValue.value = `${delayMsNumber.toFixed(0)} ms`;
  delayBpmValue.value = `${bpm.toFixed(0)} BPM`;
  delayFeedbackValue.value = `${Math.round(feedback * 100)}%`;
  delayMixValue.value = `${Math.round(mix * 100)}%`;
  if (workletNode) {
    workletNode.port.postMessage({
      type: "delay",
      value: { tempoSync, delayMs: delayMsNumber, bpm, division, feedback, mix },
    });
  }
}

function applyDelayPreset() {
  if (delayPreset.value === "slapback") {
    delayMode.value = "free";
    delayMs.value = "110";
    delayBpm.value = "120";
    delayDivision.value = "4";
    delayFeedback.value = "0.12";
    delayMix.value = "0.25";
  }
  updateDelay(true);
}

delayPreset.addEventListener("change", applyDelayPreset);
function updateDelayRouting() {
  const mode = delayRouting.value;
  const pingPong = mode === "ping-pong";
  const dual = mode === "dual";
  const multiTap = mode === "multi-tap";
  DEFAULT_PARAMETERS.delayPingPong = pingPong;
  DEFAULT_PARAMETERS.delayDualEnabled = dual;
  DEFAULT_PARAMETERS.delayMultiTapEnabled = multiTap;
  delayDualControls.hidden = !dual;
  delayMultiTapControls.hidden = !multiTap;
  for (const control of [delayPreset, delayMode, delayMs, delayBpm, delayDivision]) {
    control.disabled = dual || multiTap;
  }
  delayFeedback.disabled = dual;
  delayMix.disabled = dual;
  if (dual) {
    updateDelayDual();
  } else if (multiTap) {
    updateDelayMultiTap();
  } else if (workletNode) {
    workletNode.port.postMessage({ type: "delay-routing", value: pingPong });
  }
}

delayRouting.addEventListener("change", updateDelayRouting);
function updateDelayDual() {
  const values = [
    Number(delayDualAms.value), Number(delayDualAFeedback.value), Number(delayDualAMix.value),
    Number(delayDualBms.value), Number(delayDualBFeedback.value), Number(delayDualBMix.value),
  ];
  DEFAULT_PARAMETERS.delayDual = values;
  delayDualAmsValue.value = `${values[0].toFixed(0)} ms`;
  delayDualAFeedbackValue.value = `${Math.round(values[1] * 100)}%`;
  delayDualAMixValue.value = `${Math.round(values[2] * 100)}%`;
  delayDualBmsValue.value = `${values[3].toFixed(0)} ms`;
  delayDualBFeedbackValue.value = `${Math.round(values[4] * 100)}%`;
  delayDualBMixValue.value = `${Math.round(values[5] * 100)}%`;
  if (workletNode && DEFAULT_PARAMETERS.delayDualEnabled) {
    workletNode.port.postMessage({ type: "delay-dual", value: values });
  }
}

for (const control of [delayDualAms, delayDualAFeedback, delayDualAMix,
  delayDualBms, delayDualBFeedback, delayDualBMix]) {
  control.addEventListener("input", updateDelayDual);
  control.addEventListener("change", updateDelayDual);
}
function updateDelayMultiTap() {
  const taps = delayMultiTapInputs.flatMap((controls) => [
    Number(controls.time.value), Number(controls.level.value), Number(controls.pan.value),
  ]);
  DEFAULT_PARAMETERS.delayMultiTap = taps;
  delayMultiTapInputs.forEach((controls, index) => {
    const offset = index * 3;
    const pan = taps[offset + 2];
    controls.timeValue.value = `${taps[offset].toFixed(0)} ms`;
    controls.levelValue.value = `${Math.round(taps[offset + 1] * 100)}%`;
    controls.panValue.value = Math.abs(pan) < 0.005
      ? "Center"
      : `${pan < 0 ? "L" : "R"} ${Math.round(Math.abs(pan) * 100)}%`;
  });
  if (workletNode && DEFAULT_PARAMETERS.delayMultiTapEnabled) {
    workletNode.port.postMessage({
      type: "delay-multitap",
      value: { taps, feedback: Number(delayFeedback.value), mix: Number(delayMix.value) },
    });
  }
}
for (const controls of delayMultiTapInputs) {
  for (const control of [controls.time, controls.level, controls.pan]) {
    control.addEventListener("input", updateDelayMultiTap);
    control.addEventListener("change", updateDelayMultiTap);
  }
}
for (const control of [delayMode, delayMs, delayBpm, delayDivision, delayFeedback, delayMix]) {
  control.addEventListener("input", () => updateDelay());
  control.addEventListener("change", () => updateDelay());
}

setInterval(() => {
  if (stats) {
    const load = readLoadWindow();
    if (load) renderLoadRows(load);
    renderLevelMeters(readLatestMeters());
    const tuner = readLatestTuner();
    if (tuner && tuner.generation !== lastTunerGeneration) {
      lastTunerGeneration = tuner.generation;
      renderTunerReading(tuner);
    }
    const error = Atomics.load(stats, 1);
    if (error) setAudioStatus("Audio block exceeded the prepared processor capacity", "error");
  }
}, 500);

function updateTunerSettings() {
  const referenceHz = Number(tunerReference.value);
  tunerReferenceValue.value = `${referenceHz.toFixed(1)} Hz`;
  if (workletNode) {
    workletNode.port.postMessage({
      type: "tuner-settings",
      referenceHz,
      muted: tunerMuteOutput.checked,
    });
  }
}

tunerReference.addEventListener("input", updateTunerSettings);
tunerReference.addEventListener("change", updateTunerSettings);
tunerMuteOutput.addEventListener("change", updateTunerSettings);

clearMeterClips.addEventListener("click", () => {
  if (workletNode) workletNode.port.postMessage({ type: "clear-meter-clips" });
});

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
    const [inputBytes, referenceBytes, tunerBytes] = await Promise.all([
      inputResponse.arrayBuffer(),
      referenceResponse.arrayBuffer(),
      fetch("/__test_tuner.wav").then((response) => {
        if (!response.ok) throw new Error("Browser tuner fixture was not served.");
        return response.arrayBuffer();
      }),
    ]);
    const reference = readFloatWave(referenceBytes);
    const captureBuffer = new SharedArrayBuffer(reference.samples.length * Float32Array.BYTES_PER_ELEMENT);
    const captureControl = new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 2);
    const captureView = new Int32Array(captureControl);
    const testOptions = {
        enabled: Array.from({ length: 25 }, () => true),
      inputMuted: false,
      globalBypass: false,
      panicMuted: false,
      parameters: {
        inputGainDb: 2,
        outputGainDb: -1,
        volumePedal: [0.92, -96, 0, 1],
        noiseGate: [-60, -72, 3, 2, 20, 80],
        cleanBoostDb: 3,
        overdrive: [0.15, -3, 0.35],
        midOverdrive: [0.18, 6, -4, 0.4],
        highGainDistortion: [0.75, 100, 9000, 5200, -9, 1],
        roundFuzz: [0.55, -6, 0.85],
        limiter: [-1, 80],
        wah: [0.62, 350, 2200, 4, 0],
        envelopeWah: [0.8, 350, 2200, 4, 0, 5, 80, 5, 0],
        tremolo: [4, 0.2, 0, 0, 120, 1],
        chorus: [0.8, 15, 7, 0.2],
        flanger: [0.25, 2, 1, 0.35, 1, 0.2],
        phaser: [0.5, 250, 1600, 0.15, 6, 0.2],
        vibrato: [5, 0.35, 0.35],
        compressor: [-18, 3, 6, 8, 100, 0],
        drive: 3,
        eq: [120, 2, 0.707, 800, -1.5, 0.707, 5000, 1, 0.707],
        delay: [180, 0.25, 0.2],
        delayTempo: [0, 120, 7],
        delayPingPong: false,
        delayDualEnabled: false,
        delayDual: [180, 0.25, 0.35, 240, 0.15, 0.25],
        delayMultiTapEnabled: true,
        delayMultiTap: [180, 0.65, -1, 240, 0.45, 1,
          360, 0.3, -0.25, 480, 0.2, 0.25],
        pitchSemitones: 5,
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
    noiseGateThreshold.value = "-60";
    noiseGateAttenuation.value = "-72";
    noiseGateHysteresis.value = "3";
    noiseGateAttack.value = "2";
    noiseGateHold.value = "20";
    noiseGateRelease.value = "80";
    limiterCeiling.value = "-1";
    limiterRelease.value = "80";
    wahPosition.value = "62";
    wahMinimum.value = "350";
    wahMaximum.value = "2200";
    wahResonance.value = "4";
    wahLevel.value = "0";
    touchWahSensitivity.value = "0.8";
    touchWahDirection.value = "up";
    touchWahMinimum.value = "350";
    touchWahMaximum.value = "2200";
    touchWahResonance.value = "4";
    touchWahLevel.value = "0";
    touchWahAttack.value = "5";
    touchWahRelease.value = "80";
    touchWahSmoothing.value = "5";
    cleanBoost.value = "3";
    cleanBoostValue.value = "3.0 dB";
    overdriveDrive.value = "0.15";
    overdriveLevel.value = "-3";
    overdriveMix.value = "0.35";
    midOverdriveDrive.value = "0.18";
    midOverdriveBoost.value = "6";
    midOverdriveLevel.value = "-4";
    midOverdriveMix.value = "0.4";
    tremoloRate.value = "4";
    tremoloDepth.value = "0.2";
    tremoloWaveform.value = "sine";
    tremoloSync.value = "off";
    pitchSemitones.value = "5";
    pitchSemitonesValue.value = "+5.0 semitones";
    tremoloBpm.value = "120";
    tremoloDivision.value = "1";
    chorusRate.value = "0.8";
    chorusDelay.value = "15";
    chorusDepth.value = "7";
    chorusMix.value = "0.2";
    flangerRate.value = "0.25";
    flangerDelay.value = "2";
    flangerDepth.value = "1";
    flangerFeedback.value = "0.35";
    flangerPolarity.value = "negative";
    flangerMix.value = "0.2";
    phaserRate.value = "0.5";
    phaserMinimum.value = "250";
    phaserMaximum.value = "1600";
    phaserFeedback.value = "0.15";
    phaserStages.value = "6";
    phaserMix.value = "0.2";
    vibratoRate.value = "5";
    vibratoDepth.value = "0.35";
    vibratoMix.value = "0.35";
    delayPreset.value = "slapback";
    applyDelayPreset();
    if (DEFAULT_PARAMETERS.delay[0] !== 110 || DEFAULT_PARAMETERS.delay[1] !== 0.12 ||
        DEFAULT_PARAMETERS.delay[2] !== 0.25 || delayMode.value !== "free") {
      throw new Error("The slapback preset did not set its advertised delay controls.");
    }
    delayMs.value = "120";
    delayMs.dispatchEvent(new Event("input", { bubbles: true }));
    if (delayPreset.value !== "custom" || DEFAULT_PARAMETERS.delay[0] !== 120) {
      throw new Error("Editing a Slapback control did not switch it to a custom delay.");
    }
    delayPreset.value = "custom";
    delayMode.value = "tempo";
    delayRouting.value = "multi-tap";
    updateDelayRouting();
    const testTaps = [180, 0.65, -1, 240, 0.45, 1,
      360, 0.3, -0.25, 480, 0.2, 0.25];
    delayMultiTapInputs.forEach((controls, index) => {
      const offset = index * 3;
      controls.time.value = String(testTaps[offset]);
      controls.level.value = String(testTaps[offset + 1]);
      controls.pan.value = String(testTaps[offset + 2]);
    });
    updateDelayMultiTap();
    delayMs.value = "180";
    delayBpm.value = "120";
    delayDivision.value = "7";
    delayFeedback.value = "0.25";
    delayMix.value = "0.2";
    updateVolumePedal();
    updateNoiseGate();
    updateLimiter();
    updateWah();
    updateEnvelopeWah();
    updateOverdrive();
    updateMidOverdrive();
    updateTremolo();
    updateChorus();
    updateFlanger();
    updatePhaser();
    updateVibrato();
    updateDelay();
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
    let maximumErrorIndex = 0;
    let errorSquares = 0;
    let referenceSquares = 0;
    let referencePeak = 0;
    let nonZeroSamples = 0;
    for (let i = 0; i < actual.length; i += 1) {
      const expected = reference.samples[i];
      const error = Math.abs(actual[i] - expected);
      if (error > maximumError) {
        maximumError = error;
        maximumErrorIndex = i;
      }
      errorSquares += error * error;
      referenceSquares += expected * expected;
      referencePeak = Math.max(referencePeak, Math.abs(expected));
      if (Math.abs(actual[i]) > 1e-6) nonZeroSamples += 1;
    }
    const rmsError = Math.sqrt(errorSquares / Math.max(1, actual.length));
    const referenceRms = Math.sqrt(referenceSquares / Math.max(1, actual.length));
    const maxAbsTolerance = Math.max(0.0002, referencePeak * 0.06);
    const rmsTolerance = Math.max(0.00002, referenceRms * 0.075);
    if (maximumError > maxAbsTolerance || rmsError > rmsTolerance || nonZeroSamples === 0) {
      const firstSamples = Array.from({ length: 8 }, (_, index) =>
        `${index}:${actual[index].toFixed(5)}/${reference.samples[index].toFixed(5)}`).join(", ");
      let actualSquares = 0;
      let expectedSquares = 0;
      for (let i = 0; i < actual.length; i += 1) {
        actualSquares += actual[i] * actual[i];
        expectedSquares += reference.samples[i] * reference.samples[i];
      }
      let bestLag = 0;
      let bestLagError = Number.POSITIVE_INFINITY;
      for (let lag = -512; lag <= 512; lag += 16) {
        const start = Math.max(0, -lag);
        const end = Math.min(actual.length, reference.samples.length - lag);
        let squares = 0;
        for (let i = start; i < end; i += 1) {
          const error = actual[i] - reference.samples[i + lag];
          squares += error * error;
        }
        const meanSquare = squares / Math.max(1, end - start);
        if (meanSquare < bestLagError) {
          bestLagError = meanSquare;
          bestLag = lag;
        }
      }
      throw new Error(`Browser/native audio mismatch: max error ${maximumError} at sample ${maximumErrorIndex} (${actual[maximumErrorIndex].toFixed(5)}/${reference.samples[maximumErrorIndex].toFixed(5)}), RMS error ${rmsError}, signal RMS browser/native ${Math.sqrt(actualSquares / actual.length)}/${Math.sqrt(expectedSquares / actual.length)}, best offset ${bestLag} samples (RMS ${Math.sqrt(bestLagError)}); first samples browser/native ${firstSamples}; decoded/context rates ${decodedAudio.sampleRate}/${audioContext.sampleRate} Hz; user agent ${navigator.userAgent}.`);
    }
    if (audioContext.state !== "running") throw new Error("Play did not start the browser audio context.");
    const load = readLoadWindow();
    const meters = findLastNonSilentInputMeter();
    if (!meters || meters.frames < 1 || meters.input.peak <= 0 || meters.output.peak <= 0) {
      throw new Error(`Input/output level meters did not report a non-silent processed block: ${JSON.stringify(meters)}`);
    }
    if (meters.input.invalidSamples !== 0 || meters.output.invalidSamples !== 0) {
      throw new Error("Level meters reported invalid samples for the finite guitar fixture.");
    }
    const lastBlockSamples = meters.frames * reference.channels;
    const inputChannels = Array.from({ length: decodedAudio.numberOfChannels }, (_, channel) =>
      decodedAudio.getChannelData(channel));
    let inputStartFrame = -1;
    for (let start = 0; start + meters.frames <= inputChannels[0].length; start += 1) {
      let candidatePeak = 0;
      let candidateSquares = 0;
      let candidateSamples = 0;
      for (let frame = start; frame < start + meters.frames; frame += 1) {
        for (const channel of inputChannels) {
          const sample = channel[frame];
          candidatePeak = Math.max(candidatePeak, Math.abs(sample));
          candidateSquares += sample * sample;
          candidateSamples += 1;
        }
      }
      const candidateRms = Math.sqrt(candidateSquares / Math.max(1, candidateSamples));
      if (Math.abs(meters.input.peak - candidatePeak) <= 1e-6 &&
          Math.abs(meters.input.rms - candidateRms) <= 1e-6) {
        inputStartFrame = start;
        break;
      }
    }
    if (inputStartFrame < 0) {
      throw new Error("Input meter did not match any block in the decoded local audio file.");
    }
    const outputStart = inputStartFrame * reference.channels;
    if (outputStart + lastBlockSamples > actual.length) {
      throw new Error("Metered output block was outside the captured audio range.");
    }
    let measuredOutputPeak = 0;
    let measuredOutputSquares = 0;
    for (let i = outputStart; i < actual.length; i += 1) {
      const magnitude = Math.abs(actual[i]);
      measuredOutputPeak = Math.max(measuredOutputPeak, magnitude);
      measuredOutputSquares += actual[i] * actual[i];
    }
    const measuredOutputRms = Math.sqrt(measuredOutputSquares / Math.max(1, actual.length - outputStart));
    if (Math.abs(meters.output.peak - measuredOutputPeak) > 1e-6 ||
        Math.abs(meters.output.rms - measuredOutputRms) > 1e-6) {
      throw new Error("Output meter does not match the final processed audio block.");
    }
    const localFileDecoded = fileName.textContent === file.name;
    const tunerFile = new File([tunerBytes], "browser-tuner-a4.wav", { type: "audio/wav" });
    const tunerTransfer = new DataTransfer();
    tunerTransfer.items.add(tunerFile);
    fileInput.files = tunerTransfer.files;
    testResult.textContent = "Checking WASM tuner analysis and A4 reference control…";
    await loadFile(tunerFile);
    tunerReference.value = "432";
    updateTunerSettings();
    await new Promise((resolve) => setTimeout(resolve, 25));
    const tunerStartSequence = Atomics.load(stats, 0);
    const tunerDetectionStartedMs = performance.now();
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting the browser tuner fixture.")), 5000,
      )),
    ]);
    let tunerReading = null;
    const tunerDeadline = performance.now() + 1500;
    while (performance.now() < tunerDeadline) {
      tunerReading = readLatestTuner();
      if (Atomics.load(stats, 0) > tunerStartSequence &&
          tunerReading?.generation > 0 && tunerReading.valid) break;
      await new Promise((resolve) => setTimeout(resolve, 20));
    }
    const tunerDetectionMs = performance.now() - tunerDetectionStartedMs;
    if (!tunerReading?.valid || tunerReading.midiNote !== 69 ||
        Math.abs(tunerReading.frequencyHz - 440) > 2 ||
        Math.abs(tunerReading.cents - 31.77) > 2 || tunerDetectionMs > 300) {
      throw new Error(`WASM tuner did not report A4 against A=432 Hz: ${JSON.stringify(tunerReading)}`);
    }
    const tunerUiDeadline = performance.now() + 1000;
    while (performance.now() < tunerUiDeadline && !tunerReadingOutput.textContent.startsWith("A4")) {
      await new Promise((resolve) => setTimeout(resolve, 20));
    }
    if (!tunerReadingOutput.textContent.startsWith("A4")) {
      throw new Error(`Browser tuner UI did not display A4: ${tunerReadingOutput.textContent}`);
    }
    stopSource(false);
    tunerReference.value = "440";
    updateTunerSettings();
    const report = {
      result: "passed",
      browser: navigator.userAgent,
      sampleRate: audioContext.sampleRate,
      channels: reference.channels,
      frames: reference.frames,
      maximumAbsoluteError: maximumError,
      rmsError,
      relativeRmsError: rmsError / Math.max(referenceRms, 1e-12),
      maxAbsoluteTolerance: maxAbsTolerance,
      rmsTolerance,
      processorMemoryBytes: chainMemoryBytes,
      tunerMemoryBytes,
      wasmLinearMemoryBytes: wasmMemoryBytes,
      timingResolution: timerResolution,
      callbackLoad: load ? {
        blockFrames: load.blockFrames,
        averagePercentByProcessor: [...load.sums].map((value) => value / load.count),
        peakPercentByProcessor: [...load.peaks],
      } : null,
      levelMeters: { capturedBlock: meters, latestBlock: readLatestMeters() },
      localFileDecode: localFileDecoded,
      playOutputCaptured: nonZeroSamples > 0,
      tuner: tunerReading,
      tunerDetectionMs,
      tunerUi: tunerReadingOutput.textContent,
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
