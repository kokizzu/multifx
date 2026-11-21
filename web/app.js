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
  "Optical-style compressor",
  "FET-style compressor",
  "Guitar graphic EQ",
  "Bass graphic EQ",
  "Shelf / tilt EQ",
  "Room reverb",
  "Hall reverb",
  "Plate reverb",
  "Spring reverb",
  "Auto Swell",
  "Rotary Speaker",
  "Ring Modulator",
  "Rhythmic Slicer",
  "Step Filter",
  "Notch Filter",
  "Reverse Delay",
  "Ducking Delay",
  "Modulated Delay",
  "Filtered Delay",
  "Sweep Delay",
  "Formant Filter",
  "Harmonic Resonator",
  "Three-band compressor",
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
  opticalCompressor: [-24, 3, 6, 10, 80, 600, 10, 0],
  fetCompressor: [-24, 8, 2, 0.1, 50, 0],
  guitarGraphicEQ: [1, 0, 0, 0, 0, 0, 0, 0],
  bassGraphicEQ: [1, 0, 0, 0, 0, 0, 0, 0],
  shelfTiltEQ: [0, 1000, 0],
  roomReverb: [0, 0.8, 10, 6000, 0.2, 0],
  hallReverb: [3.2, 32, 9000, 100, 0.25, 0],
  plateReverb: [1.8, 0, 7500, 0.82, 0.5, 0.2, 0],
  springReverb: [2.4, 0, 8000, 0.35, 0.2, 0],
  autoSwell: [-36, 250, 250, 6, 6, 1],
  rotarySpeaker: [6, 0, 120, 1, 0, 0.12, 0, 0.75, 1, 0.55, 0.5, 0.72,
    800, 450, 0, 0, 0.25, 1, 1, 1, 0, 0],
  ringModulator: [440, 1, 20000, 1],
  rhythmicSlicer: [1, 120, 8, 8, 1, 0.15, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0],
  stepFilter: [1, 120, 8, 8, 0, 120, 6000, 0.707, 1, 0.15, 4, 1, 0,
    0, 0.72, 0.18, 0.88, 0.32, 0.62, 0.08, 0.96,
    0.24, 0.78, 0.12, 0.9, 0.38, 0.68, 0.04, 0.84],
  notchFilter: [1000, 4, 1],
  formantFilter: [0, 4, 1, 0],
  harmonicResonator: [110, 1.2, 0.55, 0.5],
  multibandCompressor: [250, 2500, 1,
    -24, 2, 6, 10, 100, 0, 1, 0,
    -24, 2, 6, 10, 100, 0, 1, 0,
    -24, 2, 6, 10, 100, 0, 1, 0],
  reverseDelay: [350, 0.25, 0.35, 10, 0, 4, 120, 0, 0, 0, 0],
  reverseDelayRun: true,
  modulatedDelay: [350, 0.25, 0.25, 0.8, 2, 0, 180, -24],
  filteredDelay: [350, 0.25, 0.25, 120, 6000],
  sweepDelay: [350, 0.25, 0.25, 250, 6000, 1.2, 0.5, 0, 90],
  drive: 3,
  eq: [120, 0, 0.707, 800, 0, 0.707, 5000, 0, 0.707],
  delay: [350, 0.25, 0.25],
  delayBbd: false,
  delayTape: [0, 0.45, 0.35, 0.3, 0.08, 6],
  delayTempo: [0, 120, 4],
  delayPingPong: false,
  delayDualEnabled: false,
  delayDual: [350, 0.25, 0.25, 525, 0.25, 0.25],
  delayMultiTapEnabled: false,
  delayMultiTap: [250, 0.65, -0.55, 500, 0.45, 0.55,
    750, 0.30, -0.25, 1000, 0.20, 0.25],
  delayTapeHeadsEnabled: false,
  delayTapeHeads: [110, 15],
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
const effectCategoryFilter = document.querySelector("#effect-category-filter");
const effectSearch = document.querySelector("#effect-search");
const effectsList = document.querySelector("#effects-list");
const effectCount = document.querySelector("#effect-count");
const effectsEmpty = document.querySelector("#effects-empty");
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
const delayCharacter = document.querySelector("#delay-character");
const delayTapeControls = document.querySelector("#delay-tape-controls");
const delayTapeInputs = [
  { input: document.querySelector("#delay-tape-saturation"), output: document.querySelector("#delay-tape-saturation-value") },
  { input: document.querySelector("#delay-tape-wow-depth"), output: document.querySelector("#delay-tape-wow-depth-value") },
  { input: document.querySelector("#delay-tape-wow-rate"), output: document.querySelector("#delay-tape-wow-rate-value") },
  { input: document.querySelector("#delay-tape-flutter-depth"), output: document.querySelector("#delay-tape-flutter-depth-value") },
  { input: document.querySelector("#delay-tape-flutter-rate"), output: document.querySelector("#delay-tape-flutter-rate-value") },
];
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
const delayTapeHeadControls = document.querySelector("#delay-tape-head-controls");
const delayTapeHeadSpacing = document.querySelector("#delay-tape-head-spacing");
const delayTapeHeadSpacingValue = document.querySelector("#delay-tape-head-spacing-value");
const delayTapeHeadInputs = Array.from({ length: 4 }, (_, index) =>
  document.querySelector(`#delay-tape-head-${index + 1}`));
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
const opticalCompressorControls = [
  "optical-threshold-db", "optical-ratio", "optical-knee-db", "optical-attack-ms",
  "optical-release-ms", "optical-program-release-ms", "optical-rms-window-ms",
  "optical-makeup-db",
].map((id) => ({ input: document.querySelector(`#${id}`), output: document.querySelector(`#${id}-value`) }));
const fetCompressorControls = [
  "fet-threshold-db", "fet-ratio", "fet-knee-db", "fet-attack-ms",
  "fet-release-ms", "fet-makeup-db",
].map((id) => ({ input: document.querySelector(`#${id}`), output: document.querySelector(`#${id}-value`) }));
const guitarGraphicLayout = document.querySelector("#guitar-eq-layout");
const guitarGraphicBands = Array.from({ length: 7 }, (_, index) => {
  const id = `guitar-eq-band-${index + 1}`;
  return { input: document.querySelector(`#${id}`), output: document.querySelector(`#${id}-value`) };
});
const bassGraphicLayout = document.querySelector("#bass-eq-layout");
const bassGraphicBands = Array.from({ length: 7 }, (_, index) => {
  const id = `bass-eq-band-${index + 1}`;
  return { input: document.querySelector(`#${id}`), output: document.querySelector(`#${id}-value`) };
});
const shelfTiltMode = document.querySelector("#shelf-tilt-mode");
const shelfTiltFrequency = document.querySelector("#shelf-tilt-frequency-hz");
const shelfTiltFrequencyValue = document.querySelector("#shelf-tilt-frequency-value");
const shelfTiltGain = document.querySelector("#shelf-tilt-gain-db");
const shelfTiltGainValue = document.querySelector("#shelf-tilt-gain-value");
const roomReverbSize = document.querySelector("#room-reverb-size");
const roomReverbDecay = document.querySelector("#room-reverb-decay-seconds");
const roomReverbDecayValue = document.querySelector("#room-reverb-decay-value");
const roomReverbPreDelay = document.querySelector("#room-reverb-pre-delay-ms");
const roomReverbPreDelayValue = document.querySelector("#room-reverb-pre-delay-value");
const roomReverbDamping = document.querySelector("#room-reverb-damping-hz");
const roomReverbDampingValue = document.querySelector("#room-reverb-damping-value");
const roomReverbMix = document.querySelector("#room-reverb-mix");
const roomReverbMixValue = document.querySelector("#room-reverb-mix-value");
const roomReverbLevel = document.querySelector("#room-reverb-level-db");
const roomReverbLevelValue = document.querySelector("#room-reverb-level-value");
const hallReverbDecay = document.querySelector("#hall-reverb-decay-seconds");
const hallReverbDecayValue = document.querySelector("#hall-reverb-decay-value");
const hallReverbPreDelay = document.querySelector("#hall-reverb-pre-delay-ms");
const hallReverbPreDelayValue = document.querySelector("#hall-reverb-pre-delay-value");
const hallReverbDamping = document.querySelector("#hall-reverb-damping-hz");
const hallReverbDampingValue = document.querySelector("#hall-reverb-damping-value");
const hallReverbLowCut = document.querySelector("#hall-reverb-low-cut-hz");
const hallReverbLowCutValue = document.querySelector("#hall-reverb-low-cut-value");
const hallReverbMix = document.querySelector("#hall-reverb-mix");
const hallReverbMixValue = document.querySelector("#hall-reverb-mix-value");
const hallReverbLevel = document.querySelector("#hall-reverb-level-db");
const hallReverbLevelValue = document.querySelector("#hall-reverb-level-value");
const plateReverbDecay = document.querySelector("#plate-reverb-decay-seconds");
const plateReverbDecayValue = document.querySelector("#plate-reverb-decay-value");
const plateReverbPreDelay = document.querySelector("#plate-reverb-pre-delay-ms");
const plateReverbPreDelayValue = document.querySelector("#plate-reverb-pre-delay-value");
const plateReverbDamping = document.querySelector("#plate-reverb-damping-hz");
const plateReverbDampingValue = document.querySelector("#plate-reverb-damping-value");
const plateReverbDiffusion = document.querySelector("#plate-reverb-diffusion");
const plateReverbDiffusionValue = document.querySelector("#plate-reverb-diffusion-value");
const plateReverbBalance = document.querySelector("#plate-reverb-balance");
const plateReverbBalanceValue = document.querySelector("#plate-reverb-balance-value");
const plateReverbMix = document.querySelector("#plate-reverb-mix");
const plateReverbMixValue = document.querySelector("#plate-reverb-mix-value");
const plateReverbLevel = document.querySelector("#plate-reverb-level-db");
const plateReverbLevelValue = document.querySelector("#plate-reverb-level-value");
const springReverbDecay = document.querySelector("#spring-reverb-decay-seconds");
const springReverbDecayValue = document.querySelector("#spring-reverb-decay-value");
const springReverbPreDelay = document.querySelector("#spring-reverb-pre-delay-ms");
const springReverbPreDelayValue = document.querySelector("#spring-reverb-pre-delay-value");
const springReverbDamping = document.querySelector("#spring-reverb-damping-hz");
const springReverbDampingValue = document.querySelector("#spring-reverb-damping-value");
const springReverbDwell = document.querySelector("#spring-reverb-dwell");
const springReverbDwellValue = document.querySelector("#spring-reverb-dwell-value");
const springReverbMix = document.querySelector("#spring-reverb-mix");
const springReverbMixValue = document.querySelector("#spring-reverb-mix-value");
const springReverbLevel = document.querySelector("#spring-reverb-level-db");
const springReverbLevelValue = document.querySelector("#spring-reverb-level-value");
const autoSwellThreshold = document.querySelector("#auto-swell-threshold-db");
const autoSwellThresholdValue = document.querySelector("#auto-swell-threshold-db-value");
const autoSwellAttack = document.querySelector("#auto-swell-attack-ms");
const autoSwellAttackValue = document.querySelector("#auto-swell-attack-ms-value");
const autoSwellRelease = document.querySelector("#auto-swell-release-ms");
const autoSwellReleaseValue = document.querySelector("#auto-swell-release-ms-value");
const autoSwellHysteresis = document.querySelector("#auto-swell-hysteresis-db");
const autoSwellHysteresisValue = document.querySelector("#auto-swell-hysteresis-db-value");
const autoSwellTaper = document.querySelector("#auto-swell-taper");
const autoSwellVolume = document.querySelector("#auto-swell-volume");
const autoSwellVolumeValue = document.querySelector("#auto-swell-volume-value");
const rotaryRate = document.querySelector("#rotary-rate-hz");
const rotaryRateValue = document.querySelector("#rotary-rate-hz-value");
const rotaryTempoSync = document.querySelector("#rotary-tempo-sync");
const rotaryTempoBpm = document.querySelector("#rotary-tempo-bpm");
const rotaryTempoBpmValue = document.querySelector("#rotary-tempo-bpm-value");
const rotaryBeatsPerRotation = document.querySelector("#rotary-beats-per-rotation");
const rotaryBeatsPerRotationValue = document.querySelector("#rotary-beats-per-rotation-value");
const rotarySpeed = document.querySelector("#rotary-speed");
const rotarySlowSpeedMultiplier = document.querySelector("#rotary-slow-speed-multiplier");
const rotarySlowSpeedMultiplierValue = document.querySelector("#rotary-slow-speed-multiplier-value");
const rotaryDrive = document.querySelector("#rotary-drive");
const rotaryDriveValue = document.querySelector("#rotary-drive-value");
const rotaryHighDepth = document.querySelector("#rotary-high-depth");
const rotaryHighDepthValue = document.querySelector("#rotary-high-depth-value");
const rotaryHighLevel = document.querySelector("#rotary-high-level");
const rotaryHighLevelValue = document.querySelector("#rotary-high-level-value");
const rotaryLowDepth = document.querySelector("#rotary-low-depth");
const rotaryLowDepthValue = document.querySelector("#rotary-low-depth-value");
const rotaryRotorLength = document.querySelector("#rotary-rotor-length");
const rotaryRotorLengthValue = document.querySelector("#rotary-rotor-length-value");
const rotaryLowRateMultiplier = document.querySelector("#rotary-low-rate-multiplier");
const rotaryLowRateMultiplierValue = document.querySelector("#rotary-low-rate-multiplier-value");
const rotaryLowTimeConstant = document.querySelector("#rotary-low-time-constant-ms");
const rotaryLowTimeConstantValue = document.querySelector("#rotary-low-time-constant-ms-value");
const rotaryHighTimeConstant = document.querySelector("#rotary-high-time-constant-ms");
const rotaryHighTimeConstantValue = document.querySelector("#rotary-high-time-constant-ms-value");
const rotaryLowMicSpacing = document.querySelector("#rotary-low-mic-spacing");
const rotaryLowMicSpacingValue = document.querySelector("#rotary-low-mic-spacing-value");
const rotaryHighMicSpacing = document.querySelector("#rotary-high-mic-spacing");
const rotaryHighMicSpacingValue = document.querySelector("#rotary-high-mic-spacing-value");
const rotaryMicDistance = document.querySelector("#rotary-mic-distance");
const rotaryMicDistanceValue = document.querySelector("#rotary-mic-distance-value");
const rotaryStereoSpread = document.querySelector("#rotary-stereo-spread");
const rotaryStereoSpreadValue = document.querySelector("#rotary-stereo-spread-value");
const rotaryMix = document.querySelector("#rotary-mix");
const rotaryMixValue = document.querySelector("#rotary-mix-value");
const rotaryLevel = document.querySelector("#rotary-level");
const rotaryLevelValue = document.querySelector("#rotary-level-value");
const rotaryBalance = document.querySelector("#rotary-balance");
const rotaryBalanceValue = document.querySelector("#rotary-balance-value");
const rotaryInputSelect = document.querySelector("#rotary-input-select");
const ringModFrequency = document.querySelector("#ring-mod-frequency-hz");
const ringModFrequencyValue = document.querySelector("#ring-mod-frequency-hz-value");
const ringModMultiplier = document.querySelector("#ring-mod-multiplier");
const ringModMultiplierValue = document.querySelector("#ring-mod-multiplier-value");
const ringModHighCut = document.querySelector("#ring-mod-high-cut-hz");
const ringModHighCutValue = document.querySelector("#ring-mod-high-cut-hz-value");
const ringModMix = document.querySelector("#ring-mod-mix");
const ringModMixValue = document.querySelector("#ring-mod-mix-value");
const rhythmicSlicerTempoSync = document.querySelector("#rhythmic-slicer-tempo-sync");
const rhythmicSlicerTempoBpm = document.querySelector("#rhythmic-slicer-tempo-bpm");
const rhythmicSlicerTempoBpmValue = document.querySelector("#rhythmic-slicer-tempo-bpm-value");
const rhythmicSlicerDivision = document.querySelector("#rhythmic-slicer-division");
const rhythmicSlicerRate = document.querySelector("#rhythmic-slicer-rate-hz");
const rhythmicSlicerRateValue = document.querySelector("#rhythmic-slicer-rate-value");
const rhythmicSlicerDepth = document.querySelector("#rhythmic-slicer-depth");
const rhythmicSlicerDepthValue = document.querySelector("#rhythmic-slicer-depth-value");
const rhythmicSlicerShape = document.querySelector("#rhythmic-slicer-shape");
const rhythmicSlicerShapeValue = document.querySelector("#rhythmic-slicer-shape-value");
const rhythmicSlicerAttack = document.querySelector("#rhythmic-slicer-attack-ms");
const rhythmicSlicerAttackValue = document.querySelector("#rhythmic-slicer-attack-value");
const rhythmicSlicerRelease = document.querySelector("#rhythmic-slicer-release-ms");
const rhythmicSlicerReleaseValue = document.querySelector("#rhythmic-slicer-release-value");
const rhythmicSlicerPhase = document.querySelector("#rhythmic-slicer-phase");
const rhythmicSlicerPhaseValue = document.querySelector("#rhythmic-slicer-phase-value");
const rhythmicSlicerStepsRoot = document.querySelector("#rhythmic-slicer-steps");
const rhythmicSlicerSteps = Array.from({ length: 16 }, (_, index) => {
  const label = document.createElement("label");
  label.className = "slicer-step";
  const name = document.createElement("span");
  name.textContent = String(index + 1).padStart(2, "0");
  const output = document.createElement("output");
  output.textContent = index % 2 === 0 ? "100%" : "0%";
  const input = document.createElement("input");
  input.type = "range";
  input.min = "0";
  input.max = "1";
  input.step = "0.01";
  input.value = index % 2 === 0 ? "1" : "0";
  input.setAttribute("aria-label", `Rhythmic Slicer step ${index + 1} level`);
  label.append(name, output, input);
  rhythmicSlicerStepsRoot.append(label);
  return { input, output };
});
const stepFilterTempoSync = document.querySelector("#step-filter-tempo-sync");
const stepFilterTempoBpm = document.querySelector("#step-filter-tempo-bpm");
const stepFilterTempoBpmValue = document.querySelector("#step-filter-tempo-bpm-value");
const stepFilterDivision = document.querySelector("#step-filter-division");
const stepFilterRate = document.querySelector("#step-filter-rate-hz");
const stepFilterRateValue = document.querySelector("#step-filter-rate-hz-value");
const stepFilterMode = document.querySelector("#step-filter-mode");
const stepFilterMinimumCutoff = document.querySelector("#step-filter-min-cutoff-hz");
const stepFilterMinimumCutoffValue = document.querySelector("#step-filter-min-cutoff-hz-value");
const stepFilterMaximumCutoff = document.querySelector("#step-filter-max-cutoff-hz");
const stepFilterMaximumCutoffValue = document.querySelector("#step-filter-max-cutoff-hz-value");
const stepFilterResonance = document.querySelector("#step-filter-resonance-q");
const stepFilterResonanceValue = document.querySelector("#step-filter-resonance-q-value");
const stepFilterDepth = document.querySelector("#step-filter-depth");
const stepFilterDepthValue = document.querySelector("#step-filter-depth-value");
const stepFilterShape = document.querySelector("#step-filter-shape");
const stepFilterShapeValue = document.querySelector("#step-filter-shape-value");
const stepFilterSmoothing = document.querySelector("#step-filter-smoothing-ms");
const stepFilterSmoothingValue = document.querySelector("#step-filter-smoothing-ms-value");
const stepFilterMix = document.querySelector("#step-filter-mix");
const stepFilterMixValue = document.querySelector("#step-filter-mix-value");
const stepFilterPhase = document.querySelector("#step-filter-phase");
const stepFilterPhaseValue = document.querySelector("#step-filter-phase-value");
const stepFilterStepsRoot = document.querySelector("#step-filter-steps");
const stepFilterSteps = Array.from({ length: 16 }, (_, index) => {
  const label = document.createElement("label");
  label.className = "slicer-step";
  const name = document.createElement("span");
  name.textContent = String(index + 1).padStart(2, "0");
  const output = document.createElement("output");
  const input = document.createElement("input");
  input.type = "range";
  input.min = "0";
  input.max = "1";
  input.step = "0.01";
  input.value = String(DEFAULT_PARAMETERS.stepFilter[index + 13]);
  output.textContent = `${Math.round(Number(input.value) * 100)}%`;
  input.setAttribute("aria-label", `Step Filter step ${index + 1} cutoff position`);
  label.append(name, output, input);
  stepFilterStepsRoot.append(label);
  return { input, output };
});
const notchFilterFrequency = document.querySelector("#notch-filter-frequency-hz");
const notchFilterFrequencyValue = document.querySelector("#notch-filter-frequency-hz-value");
const notchFilterQ = document.querySelector("#notch-filter-q");
const notchFilterQValue = document.querySelector("#notch-filter-q-value");
const notchFilterMix = document.querySelector("#notch-filter-mix");
const notchFilterMixValue = document.querySelector("#notch-filter-mix-value");
const formantFilterVowel = document.querySelector("#formant-filter-vowel-position");
const formantFilterVowelValue = document.querySelector("#formant-filter-vowel-position-value");
const formantFilterQ = document.querySelector("#formant-filter-q");
const formantFilterQValue = document.querySelector("#formant-filter-q-value");
const formantFilterMix = document.querySelector("#formant-filter-mix");
const formantFilterMixValue = document.querySelector("#formant-filter-mix-value");
const formantFilterLevel = document.querySelector("#formant-filter-level-db");
const formantFilterLevelValue = document.querySelector("#formant-filter-level-db-value");
const harmonicResonatorRoot = document.querySelector("#harmonic-resonator-root-frequency-hz");
const harmonicResonatorRootValue = document.querySelector("#harmonic-resonator-root-frequency-hz-value");
const harmonicResonatorDecay = document.querySelector("#harmonic-resonator-decay-seconds");
const harmonicResonatorDecayValue = document.querySelector("#harmonic-resonator-decay-seconds-value");
const harmonicResonatorBrightness = document.querySelector("#harmonic-resonator-brightness");
const harmonicResonatorBrightnessValue = document.querySelector("#harmonic-resonator-brightness-value");
const harmonicResonatorMix = document.querySelector("#harmonic-resonator-mix");
const harmonicResonatorMixValue = document.querySelector("#harmonic-resonator-mix-value");
const multibandLowMid = document.querySelector("#multiband-low-mid-hz");
const multibandLowMidValue = document.querySelector("#multiband-low-mid-hz-value");
const multibandMidHigh = document.querySelector("#multiband-mid-high-hz");
const multibandMidHighValue = document.querySelector("#multiband-mid-high-hz-value");
const multibandSlope = document.querySelector("#multiband-crossover-slope");
const multibandBandControls = ["low", "mid", "high"].map((band) => ({
  threshold: document.querySelector(`#multiband-${band}-threshold`),
  thresholdValue: document.querySelector(`#multiband-${band}-threshold-value`),
  ratio: document.querySelector(`#multiband-${band}-ratio`),
  ratioValue: document.querySelector(`#multiband-${band}-ratio-value`),
  knee: document.querySelector(`#multiband-${band}-knee`),
  kneeValue: document.querySelector(`#multiband-${band}-knee-value`),
  attack: document.querySelector(`#multiband-${band}-attack`),
  attackValue: document.querySelector(`#multiband-${band}-attack-value`),
  release: document.querySelector(`#multiband-${band}-release`),
  releaseValue: document.querySelector(`#multiband-${band}-release-value`),
  level: document.querySelector(`#multiband-${band}-level`),
  levelValue: document.querySelector(`#multiband-${band}-level-value`),
  detector: document.querySelector(`#multiband-${band}-detector`),
  mute: document.querySelector(`#multiband-${band}-mute`),
}));
const reverseDelayTime = document.querySelector("#reverse-delay-time-ms");
const reverseDelayTimeValue = document.querySelector("#reverse-delay-time-ms-value");
const reverseDelayTimeMode = document.querySelector("#reverse-delay-time-mode");
const reverseDelayTempo = document.querySelector("#reverse-delay-tempo-bpm");
const reverseDelayTempoValue = document.querySelector("#reverse-delay-tempo-bpm-value");
const reverseDelayDivision = document.querySelector("#reverse-delay-division");
const reverseDelayFeedback = document.querySelector("#reverse-delay-feedback");
const reverseDelayFeedbackValue = document.querySelector("#reverse-delay-feedback-value");
const reverseDelayMix = document.querySelector("#reverse-delay-mix");
const reverseDelayMixValue = document.querySelector("#reverse-delay-mix-value");
const reverseDelayCrossfade = document.querySelector("#reverse-delay-crossfade-ms");
const reverseDelayCrossfadeValue = document.querySelector("#reverse-delay-crossfade-ms-value");
const reverseDelayRun = document.querySelector("#reverse-delay-run");
const reverseDelayLowCut = document.querySelector("#reverse-delay-low-cut-hz");
const reverseDelayLowCutValue = document.querySelector("#reverse-delay-low-cut-hz-value");
const reverseDelayHighCut = document.querySelector("#reverse-delay-high-cut-hz");
const reverseDelayHighCutValue = document.querySelector("#reverse-delay-high-cut-hz-value");
const reverseDelayForwardLevel = document.querySelector("#reverse-delay-forward-level");
const reverseDelayForwardLevelValue = document.querySelector("#reverse-delay-forward-level-value");
const reverseDelaySpillover = document.querySelector("#reverse-delay-spillover");
const reverseDelayRestart = document.querySelector("#reverse-delay-restart");
const duckingDelayTime = document.querySelector("#ducking-delay-time-ms");
const duckingDelayTimeValue = document.querySelector("#ducking-delay-time-ms-value");
const duckingDelayFeedback = document.querySelector("#ducking-delay-feedback");
const duckingDelayFeedbackValue = document.querySelector("#ducking-delay-feedback-value");
const duckingDelayMix = document.querySelector("#ducking-delay-mix");
const duckingDelayMixValue = document.querySelector("#ducking-delay-mix-value");
const duckingDelayThreshold = document.querySelector("#ducking-delay-threshold-db");
const duckingDelayThresholdValue = document.querySelector("#ducking-delay-threshold-db-value");
const duckingDelayAttenuation = document.querySelector("#ducking-delay-attenuation-db");
const duckingDelayAttenuationValue = document.querySelector("#ducking-delay-attenuation-db-value");
const duckingDelayRelease = document.querySelector("#ducking-delay-release-ms");
const duckingDelayReleaseValue = document.querySelector("#ducking-delay-release-ms-value");
const modulatedDelayTime = document.querySelector("#modulated-delay-time-ms");
const modulatedDelayTimeValue = document.querySelector("#modulated-delay-time-ms-value");
const modulatedDelayFeedback = document.querySelector("#modulated-delay-feedback");
const modulatedDelayFeedbackValue = document.querySelector("#modulated-delay-feedback-value");
const modulatedDelayMix = document.querySelector("#modulated-delay-mix");
const modulatedDelayMixValue = document.querySelector("#modulated-delay-mix-value");
const modulatedDelayRate = document.querySelector("#modulated-delay-rate-hz");
const modulatedDelayRateValue = document.querySelector("#modulated-delay-rate-hz-value");
const modulatedDelayDepth = document.querySelector("#modulated-delay-depth-ms");
const modulatedDelayDepthValue = document.querySelector("#modulated-delay-depth-ms-value");
const modulatedDelayWaveform = document.querySelector("#modulated-delay-waveform");
const modulatedDelaySpread = document.querySelector("#modulated-delay-spread-degrees");
const modulatedDelaySpreadValue = document.querySelector("#modulated-delay-spread-degrees-value");
const modulatedDelayTriggerThreshold = document.querySelector("#modulated-delay-trigger-threshold-db");
const modulatedDelayTriggerThresholdValue = document.querySelector("#modulated-delay-trigger-threshold-db-value");
const filteredDelayTime = document.querySelector("#filtered-delay-time-ms");
const filteredDelayTimeValue = document.querySelector("#filtered-delay-time-ms-value");
const filteredDelayFeedback = document.querySelector("#filtered-delay-feedback");
const filteredDelayFeedbackValue = document.querySelector("#filtered-delay-feedback-value");
const filteredDelayMix = document.querySelector("#filtered-delay-mix");
const filteredDelayMixValue = document.querySelector("#filtered-delay-mix-value");
const filteredDelayLowCut = document.querySelector("#filtered-delay-low-cut-hz");
const filteredDelayLowCutValue = document.querySelector("#filtered-delay-low-cut-hz-value");
const filteredDelayHighCut = document.querySelector("#filtered-delay-high-cut-hz");
const filteredDelayHighCutValue = document.querySelector("#filtered-delay-high-cut-hz-value");
const sweepDelayTime = document.querySelector("#sweep-delay-time-ms");
const sweepDelayTimeValue = document.querySelector("#sweep-delay-time-ms-value");
const sweepDelayFeedback = document.querySelector("#sweep-delay-feedback");
const sweepDelayFeedbackValue = document.querySelector("#sweep-delay-feedback-value");
const sweepDelayMix = document.querySelector("#sweep-delay-mix");
const sweepDelayMixValue = document.querySelector("#sweep-delay-mix-value");
const sweepDelayStart = document.querySelector("#sweep-delay-start-hz");
const sweepDelayStartValue = document.querySelector("#sweep-delay-start-hz-value");
const sweepDelayStop = document.querySelector("#sweep-delay-stop-hz");
const sweepDelayStopValue = document.querySelector("#sweep-delay-stop-hz-value");
const sweepDelayResonance = document.querySelector("#sweep-delay-resonance-q");
const sweepDelayResonanceValue = document.querySelector("#sweep-delay-resonance-q-value");
const sweepDelayRate = document.querySelector("#sweep-delay-rate-hz");
const sweepDelayRateValue = document.querySelector("#sweep-delay-rate-hz-value");
const sweepDelayWaveform = document.querySelector("#sweep-delay-waveform");
const sweepDelayPhase = document.querySelector("#sweep-delay-phase-degrees");
const sweepDelayPhaseValue = document.querySelector("#sweep-delay-phase-degrees-value");
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

function updateEffectBrowser() {
  const category = effectCategoryFilter.value;
  const query = effectSearch.value.trim().toLocaleLowerCase();
  const rows = [...effectsList.querySelectorAll(".effect-row")];
  let visibleCount = 0;

  for (const row of rows) {
    const matchesCategory = category === "all" || row.dataset.category === category;
    const matchesQuery = !query || row.textContent.toLocaleLowerCase().includes(query);
    row.hidden = !(matchesCategory && matchesQuery);
    if (!row.hidden) visibleCount += 1;
  }

  effectCount.value = `${visibleCount} of ${rows.length}`;
  effectsEmpty.hidden = visibleCount !== 0;
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

effectCategoryFilter.addEventListener("change", updateEffectBrowser);
effectSearch.addEventListener("input", updateEffectBrowser);
updateEffectBrowser();

for (const control of [
  multibandLowMid,
  multibandMidHigh,
  multibandSlope,
  ...multibandBandControls.flatMap((band) => [
    band.threshold, band.ratio, band.knee, band.attack, band.release,
    band.level, band.detector, band.mute,
  ]),
]) {
  control.addEventListener(control.type === "range" ? "input" : "change",
    updateMultibandCompressor);
}

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
  } else if (delayPreset.value === "tape") {
    delayMode.value = "free";
    delayMs.value = "300";
    delayBpm.value = "120";
    delayDivision.value = "4";
    delayFeedback.value = "0.35";
    delayMix.value = "0.25";
    delayCharacter.value = "tape";
  } else if (delayPreset.value === "tape-heads") {
    delayRouting.value = "tape-heads";
    delayMode.value = "free";
    delayMs.value = "110";
    delayBpm.value = "120";
    delayDivision.value = "4";
    delayFeedback.value = "0.28";
    delayMix.value = "0.25";
    delayTapeHeadSpacing.value = "110";
    for (const input of delayTapeHeadInputs) input.checked = true;
    delayCharacter.value = "tape";
    updateDelayRouting();
  }
  updateDelayCharacter();
  updateDelay(true);
}

delayPreset.addEventListener("change", applyDelayPreset);
function updateDelayCharacter() {
  const bbd = delayCharacter.value === "bbd";
  const tapeEnabled = delayCharacter.value === "tape";
  const tape = [
    tapeEnabled ? 1 : 0,
    ...delayTapeInputs.map(({ input }) => Number(input.value)),
  ];
  DEFAULT_PARAMETERS.delayBbd = bbd;
  DEFAULT_PARAMETERS.delayTape = tape;
  delayTapeControls.hidden = !tapeEnabled;
  const [enabled, saturation, wowDepth, wowRate, flutterDepth, flutterRate] = tape;
  delayTapeInputs[0].output.value = `${Math.round(saturation * 100)}%`;
  delayTapeInputs[1].output.value = `${wowDepth.toFixed(2)} ms`;
  delayTapeInputs[2].output.value = `${wowRate.toFixed(2)} Hz`;
  delayTapeInputs[3].output.value = `${flutterDepth.toFixed(2)} ms`;
  delayTapeInputs[4].output.value = `${flutterRate.toFixed(1)} Hz`;
  if (workletNode) {
    workletNode.port.postMessage({
      type: "delay-character",
      value: { bbd, tape: [enabled, saturation, wowDepth, wowRate, flutterDepth, flutterRate] },
    });
  }
}
delayCharacter.addEventListener("change", updateDelayCharacter);
for (const { input } of delayTapeInputs) {
  input.addEventListener("input", updateDelayCharacter);
  input.addEventListener("change", updateDelayCharacter);
}
function updateDelayRouting() {
  const mode = delayRouting.value;
  const pingPong = mode === "ping-pong";
  const dual = mode === "dual";
  const multiTap = mode === "multi-tap";
  const tapeHeads = mode === "tape-heads";
  DEFAULT_PARAMETERS.delayPingPong = pingPong;
  DEFAULT_PARAMETERS.delayDualEnabled = dual;
  DEFAULT_PARAMETERS.delayMultiTapEnabled = multiTap;
  DEFAULT_PARAMETERS.delayTapeHeadsEnabled = tapeHeads;
  delayDualControls.hidden = !dual;
  delayMultiTapControls.hidden = !multiTap;
  delayTapeHeadControls.hidden = !tapeHeads;
  for (const control of [delayPreset, delayMode, delayMs, delayBpm, delayDivision]) {
    control.disabled = dual || multiTap || tapeHeads;
  }
  delayFeedback.disabled = dual;
  delayMix.disabled = dual;
  if (!tapeHeads) updateDelayTapeHeads();
  if (dual) {
    updateDelayDual();
  } else if (multiTap) {
    updateDelayMultiTap();
  } else if (tapeHeads) {
    updateDelayTapeHeads();
  } else if (workletNode) {
    workletNode.port.postMessage({ type: "delay-routing", value: pingPong });
  }
}

delayRouting.addEventListener("change", updateDelayRouting);
function updateDelayTapeHeads() {
  const spacing = Number(delayTapeHeadSpacing.value);
  const selectedHeads = delayTapeHeadInputs.reduce(
    (mask, input, index) => mask | (input.checked ? (1 << index) : 0), 0,
  );
  const enabled = delayRouting.value === "tape-heads";
  DEFAULT_PARAMETERS.delayTapeHeadsEnabled = enabled;
  DEFAULT_PARAMETERS.delayTapeHeads = [spacing, selectedHeads];
  delayTapeHeadSpacingValue.value = `${spacing.toFixed(0)} ms`;
  if (delayPreset.value === "tape-heads" &&
      (spacing !== 110 || selectedHeads !== 15)) delayPreset.value = "custom";
  if (workletNode) {
    workletNode.port.postMessage({
      type: "delay-tape-heads",
      value: { enabled, spacingMs: spacing, selectedHeads },
    });
  }
}
delayTapeHeadSpacing.addEventListener("input", updateDelayTapeHeads);
delayTapeHeadSpacing.addEventListener("change", updateDelayTapeHeads);
for (const input of delayTapeHeadInputs) {
  input.addEventListener("input", updateDelayTapeHeads);
  input.addEventListener("change", updateDelayTapeHeads);
}

function updateCompressorControls(controls, parameterKey, messageType, formats) {
  const values = controls.map(({ input }) => Number(input.value));
  controls.forEach(({ output }, index) => {
    const value = values[index];
    output.value = formats[index] === "ratio"
      ? `${value.toFixed(1)}:1`
      : `${value.toFixed(1)}${formats[index]}`;
  });
  DEFAULT_PARAMETERS[parameterKey] = values;
  if (workletNode) workletNode.port.postMessage({ type: messageType, value: values });
}

function updateOpticalCompressor() {
  updateCompressorControls(
    opticalCompressorControls, "opticalCompressor", "optical-compressor",
    [" dB", "ratio", " dB", " ms", " ms", " ms", " ms", " dB"],
  );
}

function updateFetCompressor() {
  updateCompressorControls(
    fetCompressorControls, "fetCompressor", "fet-compressor",
    [" dB", "ratio", " dB", " ms", " ms", " dB"],
  );
}

const GUITAR_EQ_FIVE_HZ = [100, 250, 630, 1600, 4000];
const GUITAR_EQ_SEVEN_HZ = [100, 200, 400, 800, 1600, 3200, 6400];
const BASS_EQ_FOUR_HZ = [50, 250, 1000, 4000];
const BASS_EQ_SEVEN_HZ = [40, 80, 160, 320, 640, 1600, 5000];

function updateGraphicEQ(layoutInput, bands, parameterKey, messageType, kind) {
  const layout = layoutInput.value;
  const bandCenters = kind === "guitar"
    ? (layout === "five" ? GUITAR_EQ_FIVE_HZ : GUITAR_EQ_SEVEN_HZ)
    : (layout === "four" ? BASS_EQ_FOUR_HZ : BASS_EQ_SEVEN_HZ);
  const gains = bands.map(({ input }, index) => {
    input.disabled = index >= bandCenters.length;
    return Number(input.value);
  });
  bands.forEach(({ output }, index) => {
    output.value = index < bandCenters.length
      ? `${bandCenters[index]} Hz · ${gains[index].toFixed(1)} dB`
      : "—";
  });
  const layoutCode = kind === "guitar" ? (layout === "five" ? 0 : 1)
    : (layout === "four" ? 0 : 1);
  const values = [layoutCode, ...gains];
  DEFAULT_PARAMETERS[parameterKey] = values;
  if (workletNode) workletNode.port.postMessage({ type: messageType, value: values });
}

function updateGuitarGraphicEQ() {
  updateGraphicEQ(guitarGraphicLayout, guitarGraphicBands,
    "guitarGraphicEQ", "guitar-graphic-eq", "guitar");
}

function updateBassGraphicEQ() {
  updateGraphicEQ(bassGraphicLayout, bassGraphicBands,
    "bassGraphicEQ", "bass-graphic-eq", "bass");
}

function updateShelfTiltEQ() {
  const frequency = Number(shelfTiltFrequency.value);
  const gain = Number(shelfTiltGain.value);
  const values = [Number(shelfTiltMode.value), frequency, gain];
  shelfTiltFrequencyValue.value = `${frequency.toFixed(0)} Hz`;
  shelfTiltGainValue.value = `${gain.toFixed(1)} dB`;
  DEFAULT_PARAMETERS.shelfTiltEQ = values;
  if (workletNode) workletNode.port.postMessage({ type: "shelf-tilt-eq", value: values });
}

function updateRoomReverb() {
  const values = [
    Number(roomReverbSize.value),
    Number(roomReverbDecay.value),
    Number(roomReverbPreDelay.value),
    Number(roomReverbDamping.value),
    Number(roomReverbMix.value) / 100,
    Number(roomReverbLevel.value),
  ];
  roomReverbDecayValue.value = `${values[1].toFixed(2)} s`;
  roomReverbPreDelayValue.value = `${values[2].toFixed(1)} ms`;
  roomReverbDampingValue.value = `${values[3].toFixed(0)} Hz`;
  roomReverbMixValue.value = `${Math.round(values[4] * 100)}%`;
  roomReverbLevelValue.value = `${values[5].toFixed(1)} dB`;
  DEFAULT_PARAMETERS.roomReverb = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "room-reverb", value: values });
  }
}

function updateHallReverb() {
  const values = [
    Number(hallReverbDecay.value),
    Number(hallReverbPreDelay.value),
    Number(hallReverbDamping.value),
    Number(hallReverbLowCut.value),
    Number(hallReverbMix.value) / 100,
    Number(hallReverbLevel.value),
  ];
  hallReverbDecayValue.value = `${values[0].toFixed(2)} s`;
  hallReverbPreDelayValue.value = `${values[1].toFixed(1)} ms`;
  hallReverbDampingValue.value = `${values[2].toFixed(0)} Hz`;
  hallReverbLowCutValue.value = `${values[3].toFixed(0)} Hz`;
  hallReverbMixValue.value = `${Math.round(values[4] * 100)}%`;
  hallReverbLevelValue.value = `${values[5].toFixed(1)} dB`;
  DEFAULT_PARAMETERS.hallReverb = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "hall-reverb", value: values });
  }
}

function updatePlateReverb() {
  const values = [
    Number(plateReverbDecay.value),
    Number(plateReverbPreDelay.value),
    Number(plateReverbDamping.value),
    Number(plateReverbDiffusion.value) / 100,
    Number(plateReverbBalance.value) / 100,
    Number(plateReverbMix.value) / 100,
    Number(plateReverbLevel.value),
  ];
  plateReverbDecayValue.value = `${values[0].toFixed(2)} s`;
  plateReverbPreDelayValue.value = `${values[1].toFixed(1)} ms`;
  plateReverbDampingValue.value = `${values[2].toFixed(0)} Hz`;
  plateReverbDiffusionValue.value = `${Math.round(values[3] * 100)}%`;
  plateReverbBalanceValue.value = `${Math.round(values[4] * 100)}% late`;
  plateReverbMixValue.value = `${Math.round(values[5] * 100)}%`;
  plateReverbLevelValue.value = `${values[6].toFixed(1)} dB`;
  DEFAULT_PARAMETERS.plateReverb = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "plate-reverb", value: values });
  }
}

function updateSpringReverb() {
  const values = [
    Number(springReverbDecay.value),
    Number(springReverbPreDelay.value),
    Number(springReverbDamping.value),
    Number(springReverbDwell.value) / 100,
    Number(springReverbMix.value) / 100,
    Number(springReverbLevel.value),
  ];
  springReverbDecayValue.value = `${values[0].toFixed(2)} s`;
  springReverbPreDelayValue.value = `${values[1].toFixed(1)} ms`;
  springReverbDampingValue.value = `${values[2].toFixed(0)} Hz`;
  springReverbDwellValue.value = `${Math.round(values[3] * 100)}%`;
  springReverbMixValue.value = `${Math.round(values[4] * 100)}%`;
  springReverbLevelValue.value = `${values[5].toFixed(1)} dB`;
  DEFAULT_PARAMETERS.springReverb = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "spring-reverb", value: values });
  }
}

function updateAutoSwell() {
  const values = [
    Number(autoSwellThreshold.value),
    Number(autoSwellAttack.value),
    Number(autoSwellRelease.value),
    Number(autoSwellHysteresis.value),
    Number(autoSwellTaper.value),
    Number(autoSwellVolume.value) / 100,
  ];
  autoSwellThresholdValue.value = `${values[0].toFixed(1)} dB`;
  autoSwellAttackValue.value = `${values[1].toFixed(0)} ms`;
  autoSwellReleaseValue.value = `${values[2].toFixed(0)} ms`;
  autoSwellHysteresisValue.value = `${values[3].toFixed(1)} dB`;
  autoSwellVolumeValue.value = `${Math.round(values[5] * 100)}%`;
  DEFAULT_PARAMETERS.autoSwell = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "auto-swell", value: values });
  }
}

function updateRotarySpeaker() {
  const values = [
    Number(rotaryRate.value),
    rotaryTempoSync.value === "on" ? 1 : 0,
    Number(rotaryTempoBpm.value),
    Number(rotaryBeatsPerRotation.value),
    rotarySpeed.value === "fast" ? 1 : 0,
    Number(rotarySlowSpeedMultiplier.value),
    Number(rotaryDrive.value),
    Number(rotaryHighDepth.value),
    Number(rotaryHighLevel.value),
    Number(rotaryLowDepth.value),
    Number(rotaryRotorLength.value),
    Number(rotaryLowRateMultiplier.value),
    Number(rotaryLowTimeConstant.value),
    Number(rotaryHighTimeConstant.value),
    Number(rotaryLowMicSpacing.value),
    Number(rotaryHighMicSpacing.value),
    Number(rotaryMicDistance.value),
    Number(rotaryStereoSpread.value) / 100,
    Number(rotaryMix.value) / 100,
    Number(rotaryLevel.value) / 100,
    Number(rotaryBalance.value) / 100,
    Number(rotaryInputSelect.value),
  ];
  rotaryRateValue.value = `${values[0].toFixed(2)} Hz`;
  rotaryTempoBpmValue.value = `${values[2].toFixed(0)} BPM`;
  rotaryBeatsPerRotationValue.value = `${values[3].toFixed(3)} beats`;
  rotarySlowSpeedMultiplierValue.value = `${values[5].toFixed(3)}×`;
  rotaryDriveValue.value = `${Math.round(values[6] * 100)}%`;
  rotaryHighDepthValue.value = `${Math.round(values[7] * 100)}%`;
  rotaryHighLevelValue.value = `${Math.round(values[8] * 100)}%`;
  rotaryLowDepthValue.value = `${Math.round(values[9] * 100)}%`;
  rotaryRotorLengthValue.value = `${Math.round(values[10] * 100)}%`;
  rotaryLowRateMultiplierValue.value = `${values[11].toFixed(2)}×`;
  rotaryLowTimeConstantValue.value = `${values[12].toFixed(0)} ms`;
  rotaryHighTimeConstantValue.value = `${values[13].toFixed(0)} ms`;
  rotaryLowMicSpacingValue.value = `${Math.round(values[14] * 100)}%`;
  rotaryHighMicSpacingValue.value = `${Math.round(values[15] * 100)}%`;
  rotaryMicDistanceValue.value = `${Math.round(values[16] * 100)}%`;
  rotaryStereoSpreadValue.value = `${values[17].toFixed(2)}×`;
  rotaryMixValue.value = `${Math.round(values[18] * 100)}%`;
  rotaryLevelValue.value = `${Math.round(values[19] * 100)}%`;
  rotaryBalanceValue.value = `${Math.round(values[20] * 100)}%`;
  DEFAULT_PARAMETERS.rotarySpeaker = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "rotary-speaker", value: values });
  }
}

function updateRingModulator() {
  const values = [
    Number(ringModFrequency.value),
    Number(ringModMultiplier.value),
    Number(ringModHighCut.value),
    Number(ringModMix.value) / 100,
  ];
  ringModFrequencyValue.value = `${values[0].toFixed(0)} Hz`;
  ringModMultiplierValue.value = `${values[1].toFixed(2)}×`;
  ringModHighCutValue.value = `${values[2].toFixed(0)} Hz`;
  ringModMixValue.value = `${Math.round(values[3] * 100)}%`;
  DEFAULT_PARAMETERS.ringModulator = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "ring-modulator", value: values });
  }
}

function updateRhythmicSlicer() {
  const stepValues = rhythmicSlicerSteps.map(({ input }) => Number(input.value));
  const values = [
    rhythmicSlicerTempoSync.checked ? 1 : 0,
    Number(rhythmicSlicerTempoBpm.value),
    Number(rhythmicSlicerDivision.value),
    Number(rhythmicSlicerRate.value),
    Number(rhythmicSlicerDepth.value) / 100,
    Number(rhythmicSlicerShape.value) / 100,
    Number(rhythmicSlicerAttack.value),
    Number(rhythmicSlicerRelease.value),
    Number(rhythmicSlicerPhase.value) / 100,
    ...stepValues,
  ];
  rhythmicSlicerTempoBpmValue.value = `${values[1].toFixed(0)} BPM`;
  rhythmicSlicerRateValue.value = `${values[3].toFixed(2)} Hz`;
  rhythmicSlicerDepthValue.value = `${Math.round(values[4] * 100)}%`;
  rhythmicSlicerShapeValue.value = `${Math.round(values[5] * 100)}%`;
  rhythmicSlicerAttackValue.value = `${values[6].toFixed(1)} ms`;
  rhythmicSlicerReleaseValue.value = `${values[7].toFixed(1)} ms`;
  rhythmicSlicerPhaseValue.value = `${Math.round(values[8] * 100)}%`;
  rhythmicSlicerSteps.forEach(({ output }, index) => {
    output.value = `${Math.round(stepValues[index] * 100)}%`;
  });
  DEFAULT_PARAMETERS.rhythmicSlicer = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "rhythmic-slicer", value: values });
  }
}

function updateStepFilter() {
  const stepValues = stepFilterSteps.map(({ input }) => Number(input.value));
  const values = [
    stepFilterTempoSync.checked ? 1 : 0,
    Number(stepFilterTempoBpm.value),
    Number(stepFilterDivision.value),
    Number(stepFilterRate.value),
    Number(stepFilterMode.value),
    Number(stepFilterMinimumCutoff.value),
    Number(stepFilterMaximumCutoff.value),
    Number(stepFilterResonance.value),
    Number(stepFilterDepth.value) / 100,
    Number(stepFilterShape.value) / 100,
    Number(stepFilterSmoothing.value),
    Number(stepFilterMix.value) / 100,
    Number(stepFilterPhase.value) / 100,
    ...stepValues,
  ];
  stepFilterTempoBpmValue.value = `${values[1].toFixed(0)} BPM`;
  stepFilterRateValue.value = `${values[3].toFixed(2)} Hz`;
  stepFilterMinimumCutoffValue.value = `${values[5].toFixed(0)} Hz`;
  stepFilterMaximumCutoffValue.value = `${values[6].toFixed(0)} Hz`;
  stepFilterResonanceValue.value = values[7].toFixed(2);
  stepFilterDepthValue.value = `${Math.round(values[8] * 100)}%`;
  stepFilterShapeValue.value = `${Math.round(values[9] * 100)}%`;
  stepFilterSmoothingValue.value = `${values[10].toFixed(1)} ms`;
  stepFilterMixValue.value = `${Math.round(values[11] * 100)}%`;
  stepFilterPhaseValue.value = `${Math.round(values[12] * 100)}%`;
  stepFilterSteps.forEach(({ output }, index) => {
    output.value = `${Math.round(stepValues[index] * 100)}%`;
  });
  DEFAULT_PARAMETERS.stepFilter = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "step-filter", value: values });
  }
}

function updateNotchFilter() {
  const values = [
    Number(notchFilterFrequency.value),
    Number(notchFilterQ.value),
    Number(notchFilterMix.value) / 100,
  ];
  notchFilterFrequencyValue.value = `${values[0].toFixed(0)} Hz`;
  notchFilterQValue.value = values[1].toFixed(2);
  notchFilterMixValue.value = `${Math.round(values[2] * 100)}%`;
  DEFAULT_PARAMETERS.notchFilter = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "notch-filter", value: values });
  }
}

function updateFormantFilter() {
  const values = [
    Number(formantFilterVowel.value),
    Number(formantFilterQ.value),
    Number(formantFilterMix.value) / 100,
    Number(formantFilterLevel.value),
  ];
  const vowels = ["A", "E", "I", "O", "U"];
  const lower = Math.min(Math.floor(values[0]), vowels.length - 1);
  const upper = Math.min(lower + 1, vowels.length - 1);
  const fraction = values[0] - lower;
  formantFilterVowelValue.value = lower === upper || fraction < 0.005
    ? vowels[lower]
    : `${vowels[lower]} → ${vowels[upper]} ${Math.round(fraction * 100)}%`;
  formantFilterQValue.value = values[1].toFixed(2);
  formantFilterMixValue.value = `${Math.round(values[2] * 100)}%`;
  formantFilterLevelValue.value = `${values[3].toFixed(1)} dB`;
  DEFAULT_PARAMETERS.formantFilter = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "formant-filter", value: values });
  }
}

function updateHarmonicResonator() {
  const values = [
    Number(harmonicResonatorRoot.value),
    Number(harmonicResonatorDecay.value),
    Number(harmonicResonatorBrightness.value) / 100,
    Number(harmonicResonatorMix.value) / 100,
  ];
  harmonicResonatorRootValue.value = `${Math.round(values[0])} Hz`;
  harmonicResonatorDecayValue.value = `${values[1].toFixed(2)} s`;
  harmonicResonatorBrightnessValue.value = `${Math.round(values[2] * 100)}%`;
  harmonicResonatorMixValue.value = `${Math.round(values[3] * 100)}%`;
  DEFAULT_PARAMETERS.harmonicResonator = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "harmonic-resonator", value: values });
  }
}

function updateMultibandCompressor() {
  const values = [
    Number(multibandLowMid.value),
    Number(multibandMidHigh.value),
    Number(multibandSlope.value),
  ];
  multibandLowMidValue.value = `${Math.round(values[0])} Hz`;
  multibandMidHighValue.value = `${Math.round(values[1])} Hz`;
  for (const band of multibandBandControls) {
    const threshold = Number(band.threshold.value);
    const ratio = Number(band.ratio.value);
    const knee = Number(band.knee.value);
    const attack = Number(band.attack.value);
    const release = Number(band.release.value);
    const level = Number(band.level.value);
    band.thresholdValue.value = `${threshold.toFixed(1)} dB`;
    band.ratioValue.value = `${ratio.toFixed(1)}:1`;
    band.kneeValue.value = `${knee.toFixed(1)} dB`;
    band.attackValue.value = `${attack.toFixed(1)} ms`;
    band.releaseValue.value = `${Math.round(release)} ms`;
    band.levelValue.value = `${level.toFixed(1)} dB`;
    values.push(threshold, ratio, knee, attack, release, level,
      Number(band.detector.value), Number(band.mute.checked));
  }
  DEFAULT_PARAMETERS.multibandCompressor = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "multiband-compressor", value: values });
  }
}

function updateReverseDelay() {
  const timeMs = Number(reverseDelayTime.value);
  const timeMode = Number(reverseDelayTimeMode.value);
  const division = Number(reverseDelayDivision.value);
  const tempoBpm = Number(reverseDelayTempo.value);
  const beatsPerDivision = [4, 3, 2, 1.5, 1, 2 / 3, 0.75, 0.5,
    1 / 3, 0.375, 0.25, 1 / 6];
  const effectiveTimeMs = timeMode === 1
    ? (60000 * beatsPerDivision[division]) / tempoBpm
    : timeMs;
  const maximumCrossfade = Math.min(50, effectiveTimeMs / 2);
  reverseDelayCrossfade.max = String(maximumCrossfade);
  if (Number(reverseDelayCrossfade.value) > maximumCrossfade) {
    reverseDelayCrossfade.value = String(maximumCrossfade);
  }
  const values = [
    timeMs,
    Number(reverseDelayFeedback.value) / 100,
    Number(reverseDelayMix.value) / 100,
    Number(reverseDelayCrossfade.value),
    timeMode,
    division,
    tempoBpm,
    Number(reverseDelayLowCut.value),
    Number(reverseDelayHighCut.value),
    Number(reverseDelayForwardLevel.value) / 100,
    reverseDelaySpillover.checked ? 1 : 0,
  ];
  reverseDelayTimeValue.value = `${effectiveTimeMs.toFixed(0)} ms${timeMode === 1 ? " (sync)" : ""}`;
  reverseDelayTempoValue.value = `${tempoBpm.toFixed(0)} BPM`;
  reverseDelayFeedbackValue.value = `${Math.round(values[1] * 100)}%`;
  reverseDelayMixValue.value = `${Math.round(values[2] * 100)}%`;
  reverseDelayCrossfadeValue.value = `${values[3].toFixed(1)} ms`;
  reverseDelayLowCutValue.value = values[7] <= 0 ? "Off" : `${values[7].toFixed(0)} Hz`;
  reverseDelayHighCutValue.value = values[8] <= 0 ? "Off" : `${values[8].toFixed(0)} Hz`;
  reverseDelayForwardLevelValue.value = `${Math.round(values[9] * 100)}%`;
  DEFAULT_PARAMETERS.reverseDelay = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "reverse-delay", value: values });
  }
}

function updateReverseDelayRun() {
  DEFAULT_PARAMETERS.reverseDelayRun = reverseDelayRun.checked;
  if (workletNode) {
    workletNode.port.postMessage({ type: "reverse-delay-run", value: reverseDelayRun.checked });
  }
}

function restartReverseDelay() {
  if (workletNode) workletNode.port.postMessage({ type: "reverse-delay-restart" });
}

function updateDuckingDelay() {
  const values = [
    Number(duckingDelayTime.value),
    Number(duckingDelayFeedback.value) / 100,
    Number(duckingDelayMix.value) / 100,
    Number(duckingDelayThreshold.value),
    Number(duckingDelayAttenuation.value),
    Number(duckingDelayRelease.value),
  ];
  duckingDelayTimeValue.value = `${values[0].toFixed(0)} ms`;
  duckingDelayFeedbackValue.value = `${Math.round(values[1] * 100)}%`;
  duckingDelayMixValue.value = `${Math.round(values[2] * 100)}%`;
  duckingDelayThresholdValue.value = `${values[3].toFixed(0)} dB`;
  duckingDelayAttenuationValue.value = `${values[4].toFixed(0)} dB`;
  duckingDelayReleaseValue.value = `${values[5].toFixed(0)} ms`;
  DEFAULT_PARAMETERS.duckingDelay = values;
  if (workletNode) workletNode.port.postMessage({ type: "ducking-delay", value: values });
}

function updateModulatedDelay() {
  const values = [
    Number(modulatedDelayTime.value),
    Number(modulatedDelayFeedback.value) / 100,
    Number(modulatedDelayMix.value) / 100,
    Number(modulatedDelayRate.value),
    Number(modulatedDelayDepth.value),
    Number(modulatedDelayWaveform.value),
    Number(modulatedDelaySpread.value),
    Number(modulatedDelayTriggerThreshold.value),
  ];
  modulatedDelayTimeValue.value = `${values[0].toFixed(0)} ms`;
  modulatedDelayFeedbackValue.value = `${Math.round(values[1] * 100)}%`;
  modulatedDelayMixValue.value = `${Math.round(values[2] * 100)}%`;
  modulatedDelayRateValue.value = `${values[3].toFixed(2)} Hz`;
  modulatedDelayDepthValue.value = `${values[4].toFixed(1)} ms`;
  modulatedDelaySpreadValue.value = `${values[6].toFixed(0)}°`;
  modulatedDelayTriggerThresholdValue.value = `${values[7].toFixed(0)} dB`;
  DEFAULT_PARAMETERS.modulatedDelay = values;
  if (workletNode) workletNode.port.postMessage({ type: "modulated-delay", value: values });
}

function updateFilteredDelay() {
  const values = [
    Number(filteredDelayTime.value),
    Number(filteredDelayFeedback.value) / 100,
    Number(filteredDelayMix.value) / 100,
    Number(filteredDelayLowCut.value),
    Number(filteredDelayHighCut.value),
  ];
  filteredDelayTimeValue.value = `${values[0].toFixed(0)} ms`;
  filteredDelayFeedbackValue.value = `${Math.round(values[1] * 100)}%`;
  filteredDelayMixValue.value = `${Math.round(values[2] * 100)}%`;
  filteredDelayLowCutValue.value = `${values[3].toFixed(0)} Hz`;
  filteredDelayHighCutValue.value = `${values[4].toFixed(0)} Hz`;
  DEFAULT_PARAMETERS.filteredDelay = values;
  if (workletNode) workletNode.port.postMessage({ type: "filtered-delay", value: values });
}

function updateSweepDelay() {
  const values = [
    Number(sweepDelayTime.value),
    Number(sweepDelayFeedback.value) / 100,
    Number(sweepDelayMix.value) / 100,
    Number(sweepDelayStart.value),
    Number(sweepDelayStop.value),
    Number(sweepDelayResonance.value),
    Number(sweepDelayRate.value),
    Number(sweepDelayWaveform.value),
    Number(sweepDelayPhase.value),
  ];
  sweepDelayTimeValue.value = `${values[0].toFixed(0)} ms`;
  sweepDelayFeedbackValue.value = `${Math.round(values[1] * 100)}%`;
  sweepDelayMixValue.value = `${Math.round(values[2] * 100)}%`;
  sweepDelayStartValue.value = `${values[3].toFixed(0)} Hz`;
  sweepDelayStopValue.value = `${values[4].toFixed(0)} Hz`;
  sweepDelayResonanceValue.value = values[5].toFixed(1);
  sweepDelayRateValue.value = `${values[6].toFixed(2)} Hz`;
  sweepDelayPhaseValue.value = `${values[8].toFixed(0)}°`;
  DEFAULT_PARAMETERS.sweepDelay = values;
  if (workletNode) workletNode.port.postMessage({ type: "sweep-delay", value: values });
}

for (const controls of [opticalCompressorControls, fetCompressorControls]) {
  for (const { input } of controls) {
    input.addEventListener("input", controls === opticalCompressorControls
      ? updateOpticalCompressor : updateFetCompressor);
    input.addEventListener("change", controls === opticalCompressorControls
      ? updateOpticalCompressor : updateFetCompressor);
  }
}
guitarGraphicLayout.addEventListener("change", updateGuitarGraphicEQ);
bassGraphicLayout.addEventListener("change", updateBassGraphicEQ);
for (const { input } of [...guitarGraphicBands, ...bassGraphicBands]) {
  input.addEventListener("input", input.id.startsWith("guitar-")
    ? updateGuitarGraphicEQ : updateBassGraphicEQ);
  input.addEventListener("change", input.id.startsWith("guitar-")
    ? updateGuitarGraphicEQ : updateBassGraphicEQ);
}
for (const input of [shelfTiltMode, shelfTiltFrequency, shelfTiltGain]) {
  input.addEventListener("input", updateShelfTiltEQ);
  input.addEventListener("change", updateShelfTiltEQ);
}
for (const input of [roomReverbSize, roomReverbDecay, roomReverbPreDelay,
  roomReverbDamping, roomReverbMix, roomReverbLevel]) {
  input.addEventListener("input", updateRoomReverb);
  input.addEventListener("change", updateRoomReverb);
}
for (const input of [hallReverbDecay, hallReverbPreDelay, hallReverbDamping,
  hallReverbLowCut, hallReverbMix, hallReverbLevel]) {
  input.addEventListener("input", updateHallReverb);
  input.addEventListener("change", updateHallReverb);
}
for (const input of [plateReverbDecay, plateReverbPreDelay, plateReverbDamping,
  plateReverbDiffusion, plateReverbBalance, plateReverbMix, plateReverbLevel]) {
  input.addEventListener("input", updatePlateReverb);
  input.addEventListener("change", updatePlateReverb);
}
for (const input of [springReverbDecay, springReverbPreDelay, springReverbDamping,
  springReverbDwell, springReverbMix, springReverbLevel]) {
  input.addEventListener("input", updateSpringReverb);
  input.addEventListener("change", updateSpringReverb);
}
for (const input of [autoSwellThreshold, autoSwellAttack, autoSwellRelease,
  autoSwellHysteresis, autoSwellTaper, autoSwellVolume]) {
  input.addEventListener("input", updateAutoSwell);
  input.addEventListener("change", updateAutoSwell);
}
for (const input of [rotaryRate, rotaryTempoBpm, rotaryBeatsPerRotation,
  rotarySlowSpeedMultiplier, rotaryDrive, rotaryHighDepth, rotaryHighLevel,
  rotaryLowDepth, rotaryRotorLength, rotaryLowRateMultiplier,
  rotaryLowTimeConstant, rotaryHighTimeConstant, rotaryLowMicSpacing,
  rotaryHighMicSpacing, rotaryMicDistance,
  rotaryStereoSpread, rotaryMix, rotaryLevel, rotaryBalance]) {
  input.addEventListener("input", updateRotarySpeaker);
  input.addEventListener("change", updateRotarySpeaker);
}
for (const input of [rotaryTempoSync, rotarySpeed, rotaryInputSelect]) {
  input.addEventListener("change", updateRotarySpeaker);
}
for (const input of [ringModFrequency, ringModMultiplier, ringModHighCut, ringModMix]) {
  input.addEventListener("input", updateRingModulator);
  input.addEventListener("change", updateRingModulator);
}
for (const input of [rhythmicSlicerTempoBpm, rhythmicSlicerRate,
  rhythmicSlicerDepth, rhythmicSlicerShape, rhythmicSlicerAttack,
  rhythmicSlicerRelease, rhythmicSlicerPhase, ...rhythmicSlicerSteps.map(({ input: step }) => step)]) {
  input.addEventListener("input", updateRhythmicSlicer);
  input.addEventListener("change", updateRhythmicSlicer);
}
for (const input of [rhythmicSlicerTempoSync, rhythmicSlicerDivision]) {
  input.addEventListener("change", updateRhythmicSlicer);
}
for (const input of [stepFilterTempoBpm, stepFilterRate, stepFilterMinimumCutoff,
  stepFilterMaximumCutoff, stepFilterResonance, stepFilterDepth, stepFilterShape,
  stepFilterSmoothing, stepFilterMix, stepFilterPhase,
  ...stepFilterSteps.map(({ input: step }) => step)]) {
  input.addEventListener("input", updateStepFilter);
  input.addEventListener("change", updateStepFilter);
}
for (const input of [stepFilterTempoSync, stepFilterDivision, stepFilterMode]) {
  input.addEventListener("change", updateStepFilter);
}
for (const input of [notchFilterFrequency, notchFilterQ, notchFilterMix]) {
  input.addEventListener("input", updateNotchFilter);
  input.addEventListener("change", updateNotchFilter);
}
for (const input of [formantFilterVowel, formantFilterQ, formantFilterMix,
  formantFilterLevel]) {
  input.addEventListener("input", updateFormantFilter);
  input.addEventListener("change", updateFormantFilter);
}
for (const input of [harmonicResonatorRoot, harmonicResonatorDecay,
  harmonicResonatorBrightness, harmonicResonatorMix]) {
  input.addEventListener("input", updateHarmonicResonator);
  input.addEventListener("change", updateHarmonicResonator);
}
for (const input of [reverseDelayTime, reverseDelayTempo,
  reverseDelayFeedback, reverseDelayMix, reverseDelayCrossfade,
  reverseDelayLowCut, reverseDelayHighCut, reverseDelayForwardLevel]) {
  input.addEventListener("input", updateReverseDelay);
  input.addEventListener("change", updateReverseDelay);
}
for (const input of [reverseDelayTimeMode, reverseDelayDivision,
  reverseDelaySpillover]) {
  input.addEventListener("change", updateReverseDelay);
}
for (const input of [duckingDelayTime, duckingDelayFeedback, duckingDelayMix,
  duckingDelayThreshold, duckingDelayAttenuation, duckingDelayRelease]) {
  input.addEventListener("input", updateDuckingDelay);
  input.addEventListener("change", updateDuckingDelay);
}
for (const input of [modulatedDelayTime, modulatedDelayFeedback, modulatedDelayMix,
  modulatedDelayRate, modulatedDelayDepth, modulatedDelaySpread,
  modulatedDelayTriggerThreshold]) {
  input.addEventListener("input", updateModulatedDelay);
  input.addEventListener("change", updateModulatedDelay);
}
modulatedDelayWaveform.addEventListener("change", updateModulatedDelay);
for (const input of [filteredDelayTime, filteredDelayFeedback, filteredDelayMix,
  filteredDelayLowCut, filteredDelayHighCut]) {
  input.addEventListener("input", updateFilteredDelay);
  input.addEventListener("change", updateFilteredDelay);
}
sweepDelayWaveform.addEventListener("change", updateSweepDelay);
for (const input of [sweepDelayTime, sweepDelayFeedback, sweepDelayMix,
  sweepDelayStart, sweepDelayStop, sweepDelayResonance, sweepDelayRate,
  sweepDelayPhase]) {
  input.addEventListener("input", updateSweepDelay);
  input.addEventListener("change", updateSweepDelay);
}
reverseDelayRun.addEventListener("change", updateReverseDelayRun);
reverseDelayRestart.addEventListener("click", restartReverseDelay);
updateOpticalCompressor();
updateFetCompressor();
updateGuitarGraphicEQ();
updateBassGraphicEQ();
updateShelfTiltEQ();
updateRoomReverb();
updateHallReverb();
updatePlateReverb();
updateSpringReverb();
updateAutoSwell();
updateRotarySpeaker();
updateRingModulator();
updateRhythmicSlicer();
updateStepFilter();
updateNotchFilter();
updateFormantFilter();
updateHarmonicResonator();
updateReverseDelay();
updateReverseDelayRun();
updateDuckingDelay();
updateModulatedDelay();
updateFilteredDelay();
updateSweepDelay();

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
    const effectRows = [...effectsList.querySelectorAll(".effect-row")];
    const initialEffectState = enabledEffects();
    const effectIds = effectRows.map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
    const knownCategories = new Set([...effectCategoryFilter.options].map((option) => option.value));
    const expectedEffectIds = Array.from({ length: 48 }, (_, index) => index);
    if (EFFECT_NAMES.length !== 48 || effectRows.length !== 48 ||
        effectIds.join(",") !== expectedEffectIds.join(",") || initialEffectState[28] ||
        initialEffectState[29] || initialEffectState[30] || initialEffectState[31] ||
        initialEffectState[32] || initialEffectState[33] || initialEffectState[34] ||
        initialEffectState[35] || initialEffectState[36] || initialEffectState[37] ||
        initialEffectState[38] || initialEffectState[39] || initialEffectState[40] ||
        initialEffectState[41] || initialEffectState[42] || initialEffectState[43] ||
        initialEffectState[44] || initialEffectState[45] ||
        effectRows.some((row) => !knownCategories.has(row.dataset.category) ||
          !row.querySelector(".effect-category-tag"))) {
      throw new Error("The effect picker must list 48 ordered, categorized effects (IDs 0–47), with all optional reverbs, modulation, filter, delay, resonator, and multiband stages disabled by default.");
    }
    const defaultDuckThreshold = duckingDelayThreshold.value;
    duckingDelayThreshold.value = "-24";
    duckingDelayThreshold.dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.duckingDelay[3] !== -24 ||
        duckingDelayThresholdValue.value !== "-24 dB") {
      throw new Error("Ducking Delay controls must update the processor parameters and displayed value.");
    }
    duckingDelayThreshold.value = defaultDuckThreshold;
    duckingDelayThreshold.dispatchEvent(new Event("input", { bubbles: true }));
    const defaultModulatedRate = modulatedDelayRate.value;
    modulatedDelayRate.value = "1.25";
    modulatedDelayRate.dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.modulatedDelay[3] !== 1.25 ||
        modulatedDelayRateValue.value !== "1.25 Hz") {
      throw new Error("Modulated Delay controls must update processor parameters and display values.");
    }
    modulatedDelayRate.value = defaultModulatedRate;
    modulatedDelayRate.dispatchEvent(new Event("input", { bubbles: true }));
    const defaultFilteredHighCut = filteredDelayHighCut.value;
    filteredDelayHighCut.value = "8000";
    filteredDelayHighCut.dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.filteredDelay[4] !== 8000 ||
        filteredDelayHighCutValue.value !== "8000 Hz") {
      throw new Error("Filtered Delay controls must update processor parameters and display values.");
    }
    filteredDelayHighCut.value = defaultFilteredHighCut;
    filteredDelayHighCut.dispatchEvent(new Event("input", { bubbles: true }));
    const defaultSweepRate = sweepDelayRate.value;
    sweepDelayRate.value = "1.25";
    sweepDelayRate.dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.sweepDelay[6] !== 1.25 ||
        sweepDelayRateValue.value !== "1.25 Hz") {
      throw new Error("Sweep Delay controls must update processor parameters and display values.");
    }
    sweepDelayRate.value = defaultSweepRate;
    sweepDelayRate.dispatchEvent(new Event("input", { bubbles: true }));
    effectCategoryFilter.value = "modulation";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const modulationIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
    if (modulationIds.join(",") !== "15,16,17,18,19,33,34,35" ||
        effectCount.value !== "8 of 48" ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("Filtering by effect category changed chain state or returned the wrong effects.");
    }
    effectCategoryFilter.value = "dynamics";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const dynamicsIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
    if (dynamicsIds.join(",") !== "4,5,6,23,24,32,45,47" ||
        effectCount.value !== "8 of 48" ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Dynamics filter must include Auto Swell and preserve the current chain state.");
    }
    effectCategoryFilter.value = "filter";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const filterIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
    if (filterIds.join(",") !== "2,3,7,8,36,37,43,44" ||
        effectCount.value !== "8 of 48" ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Filters category must include Step Filter, Notch Filter, Formant Filter, and Harmonic Resonator and preserve the current chain state.");
    }
    effectCategoryFilter.value = "delay";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const delayIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
    if (delayIds.join(",") !== "21,38,39,40,41,42" || effectCount.value !== "6 of 48" ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Delay category must include Digital, Reverse, Ducking, Modulated, Filtered, and Sweep Delay and preserve the current chain state.");
    }
    effectCategoryFilter.value = "reverb";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const reverbIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
    if (reverbIds.join(",") !== "28,29,30,31" || effectCount.value !== "4 of 48" ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Reverb filter must return Room, Hall, Plate, and Spring Reverb without changing enabled state.");
    }
    effectCategoryFilter.value = "all";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    effectSearch.value = "guitar graphic";
    effectSearch.dispatchEvent(new Event("input", { bubbles: true }));
    const searchIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
    if (searchIds.join(",") !== "25" || effectCount.value !== "1 of 48") {
      throw new Error("Searching the effect browser returned the wrong processor.");
    }
    effectSearch.value = "";
    effectSearch.dispatchEvent(new Event("input", { bubbles: true }));
    if (effectCount.value !== "48 of 48" || effectRows.some((row) => row.hidden)) {
      throw new Error("Clearing effect filters did not restore the complete chain.");
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
      enabled: Array.from({ length: EFFECT_NAMES.length }, () => true),
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
        opticalCompressor: [-24, 3, 6, 10, 80, 600, 10, 0],
        fetCompressor: [-30, 5, 2, 0.1, 50, 0],
        guitarGraphicEQ: [1, 1.5, -1, 0, 1, 0.5, -0.5, 1],
        bassGraphicEQ: [1, -1, 0.5, 2, -2, 0.5, 1, -0.5],
        shelfTiltEQ: [2, 1200, 2],
        roomReverb: [0, 0.8, 10, 6000, 0.2, 0],
        hallReverb: [3.2, 32, 9000, 100, 0.25, 0],
        plateReverb: [1.8, 0, 7500, 0.82, 0.5, 0.2, 0],
        springReverb: [2.4, 0, 8000, 0.35, 0.2, 0],
        autoSwell: [-90, 100, 150, 6, 0, 0.9],
         rotarySpeaker: [6, 0, 120, 1, 0, 0.12, 0.15, 0.75, 1, 0.55, 0.5,
           0.72, 800, 450, 0.4, 0.75, 0.25, 1, 1, 1, 0, 0],
        ringModulator: [713, 1.75, 9000, 0.83],
        rhythmicSlicer: [1, 120, 8, 8, 0.4, 0.15, 0.5, 1, 0,
          1, 0.25, 1, 0.25, 1, 0.25, 1, 0.25,
          1, 0.25, 1, 0.25, 1, 0.25, 1, 0.25],
         stepFilter: [1, 120, 8, 8, 0, 120, 6000, 0.707, 0.65, 0.4, 3, 0.7, 0,
           0, 0.75, 0.2, 0.9, 0.35, 0.6, 0.1, 1,
           0.25, 0.8, 0.15, 0.95, 0.4, 0.7, 0.05, 0.85],
        notchFilter: [713, 6, 0.72],
        formantFilter: [1.5, 6, 0.72, -2],
        harmonicResonator: [220, 0.8, 0.7, 0.65],
        multibandCompressor: [500, 4000, 0,
          -30, 4, 0, 1, 30, 6, 0, 0,
          -24, 2, 6, 10, 100, 0, 1, 0,
          -18, 3, 3, 5, 250, -2, 1, 0],
          reverseDelay: [350, 0.25, 0.35, 10, 0, 4, 120, 0, 0, 0, 0],
         reverseDelayRun: true,
         duckingDelay: [350, 0.25, 0.25, -30, 12, 250],
        sweepDelay: [350, 0.25, 0.25, 250, 6000, 1.2, 0.5, 0, 90],
        drive: 3,
        eq: [120, 2, 0.707, 800, -1.5, 0.707, 5000, 1, 0.707],
        delay: [180, 0.25, 0.2],
        delayBbd: false,
        delayTape: [1, 0.45, 0.35, 0.3, 0.08, 6],
        delayTempo: [0, 120, 7],
        delayPingPong: false,
        delayDualEnabled: false,
        delayDual: [180, 0.25, 0.35, 240, 0.15, 0.25],
        delayMultiTapEnabled: false,
        delayMultiTap: [180, 0.65, -1, 240, 0.45, 1,
          360, 0.3, -0.25, 480, 0.2, 0.25],
        delayTapeHeadsEnabled: true,
        delayTapeHeads: [180, 15],
        pitchSemitones: 5,
        filters: [80, 12000],
        pedalCompressor: [0.45, 0.45, 0.25, 0],
      },
      captureBuffer,
      captureControl,
    };
    const slicerParameters = testOptions.parameters.rhythmicSlicer;
    rhythmicSlicerTempoSync.checked = slicerParameters[0] !== 0;
    rhythmicSlicerTempoBpm.value = String(slicerParameters[1]);
    rhythmicSlicerDivision.value = String(slicerParameters[2]);
    rhythmicSlicerRate.value = String(slicerParameters[3]);
    rhythmicSlicerDepth.value = String(slicerParameters[4] * 100);
    rhythmicSlicerShape.value = String(slicerParameters[5] * 100);
    rhythmicSlicerAttack.value = String(slicerParameters[6]);
    rhythmicSlicerRelease.value = String(slicerParameters[7]);
    rhythmicSlicerPhase.value = String(slicerParameters[8] * 100);
    rhythmicSlicerSteps.forEach(({ input }, index) => {
      input.value = String(slicerParameters[index + 9]);
    });
    updateRhythmicSlicer();
    const stepFilterParameters = testOptions.parameters.stepFilter;
    stepFilterTempoSync.checked = stepFilterParameters[0] !== 0;
    stepFilterTempoBpm.value = String(stepFilterParameters[1]);
    stepFilterDivision.value = String(stepFilterParameters[2]);
    stepFilterRate.value = String(stepFilterParameters[3]);
    stepFilterMode.value = String(stepFilterParameters[4]);
    stepFilterMinimumCutoff.value = String(stepFilterParameters[5]);
    stepFilterMaximumCutoff.value = String(stepFilterParameters[6]);
    stepFilterResonance.value = String(stepFilterParameters[7]);
    stepFilterDepth.value = String(stepFilterParameters[8] * 100);
    stepFilterShape.value = String(stepFilterParameters[9] * 100);
    stepFilterSmoothing.value = String(stepFilterParameters[10]);
    stepFilterMix.value = String(stepFilterParameters[11] * 100);
    stepFilterPhase.value = String(stepFilterParameters[12] * 100);
    stepFilterSteps.forEach(({ input }, index) => {
      input.value = String(stepFilterParameters[index + 13]);
    });
    updateStepFilter();
    const notchFilterParameters = testOptions.parameters.notchFilter;
    notchFilterFrequency.value = String(notchFilterParameters[0]);
    notchFilterQ.value = String(notchFilterParameters[1]);
    notchFilterMix.value = String(notchFilterParameters[2] * 100);
    updateNotchFilter();
    const formantFilterParameters = testOptions.parameters.formantFilter;
    formantFilterVowel.value = String(formantFilterParameters[0]);
    formantFilterQ.value = String(formantFilterParameters[1]);
    formantFilterMix.value = String(formantFilterParameters[2] * 100);
    formantFilterLevel.value = String(formantFilterParameters[3]);
    updateFormantFilter();
    if (DEFAULT_PARAMETERS.formantFilter[0] !== 1.5 ||
        formantFilterVowelValue.value !== "E → I 50%" ||
        DEFAULT_PARAMETERS.formantFilter[1] !== 6 ||
        DEFAULT_PARAMETERS.formantFilter[2] !== 0.72 ||
        DEFAULT_PARAMETERS.formantFilter[3] !== -2) {
      throw new Error("Formant Filter controls must update vowel position, Q, mix, and output level.");
    }
    const harmonicParameters = testOptions.parameters.harmonicResonator;
    harmonicResonatorRoot.value = String(harmonicParameters[0]);
    harmonicResonatorDecay.value = String(harmonicParameters[1]);
    harmonicResonatorBrightness.value = String(harmonicParameters[2] * 100);
    harmonicResonatorMix.value = String(harmonicParameters[3] * 100);
    updateHarmonicResonator();
    if (DEFAULT_PARAMETERS.harmonicResonator[0] !== 220 ||
        DEFAULT_PARAMETERS.harmonicResonator[1] !== 0.8 ||
        DEFAULT_PARAMETERS.harmonicResonator[2] !== 0.7 ||
        DEFAULT_PARAMETERS.harmonicResonator[3] !== 0.65) {
      throw new Error("Harmonic Resonator controls must update root, decay, brightness, and mix.");
    }
    const multibandParameters = testOptions.parameters.multibandCompressor;
    multibandLowMid.value = String(multibandParameters[0]);
    multibandMidHigh.value = String(multibandParameters[1]);
    multibandSlope.value = String(multibandParameters[2]);
    for (let bandIndex = 0; bandIndex < 3; bandIndex += 1) {
      const offset = 3 + bandIndex * 8;
      const band = multibandBandControls[bandIndex];
      band.threshold.value = String(multibandParameters[offset]);
      band.ratio.value = String(multibandParameters[offset + 1]);
      band.knee.value = String(multibandParameters[offset + 2]);
      band.attack.value = String(multibandParameters[offset + 3]);
      band.release.value = String(multibandParameters[offset + 4]);
      band.level.value = String(multibandParameters[offset + 5]);
      band.detector.value = String(multibandParameters[offset + 6]);
      band.mute.checked = Boolean(multibandParameters[offset + 7]);
    }
    updateMultibandCompressor();
    const configuredCrossover = multibandLowMid.value;
    multibandLowMid.value = "350";
    multibandLowMid.dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.multibandCompressor[0] !== 350 ||
        multibandLowMidValue.value !== "350 Hz" ||
        DEFAULT_PARAMETERS.multibandCompressor[2] !== 0 ||
        DEFAULT_PARAMETERS.multibandCompressor[9] !== 0) {
      throw new Error("Three-band compressor controls did not update crossover, slope, and band settings.");
    }
    multibandLowMid.value = configuredCrossover;
    multibandLowMid.dispatchEvent(new Event("input", { bubbles: true }));
    const reverseDelayParameters = testOptions.parameters.reverseDelay;
    reverseDelayTime.value = String(reverseDelayParameters[0]);
    reverseDelayFeedback.value = String(reverseDelayParameters[1] * 100);
    reverseDelayMix.value = String(reverseDelayParameters[2] * 100);
    reverseDelayCrossfade.value = String(reverseDelayParameters[3]);
    reverseDelayTimeMode.value = String(reverseDelayParameters[4]);
    reverseDelayDivision.value = String(reverseDelayParameters[5]);
    reverseDelayTempo.value = String(reverseDelayParameters[6]);
    reverseDelayLowCut.value = String(reverseDelayParameters[7]);
    reverseDelayHighCut.value = String(reverseDelayParameters[8]);
    reverseDelayForwardLevel.value = String(reverseDelayParameters[9] * 100);
    reverseDelaySpillover.checked = Boolean(reverseDelayParameters[10]);
    reverseDelayRun.checked = testOptions.parameters.reverseDelayRun;
    updateReverseDelay();
    updateReverseDelayRun();
    const duckingDelayParameters = testOptions.parameters.duckingDelay;
    duckingDelayTime.value = String(duckingDelayParameters[0]);
    duckingDelayFeedback.value = String(duckingDelayParameters[1] * 100);
    duckingDelayMix.value = String(duckingDelayParameters[2] * 100);
    duckingDelayThreshold.value = String(duckingDelayParameters[3]);
    duckingDelayAttenuation.value = String(duckingDelayParameters[4]);
    duckingDelayRelease.value = String(duckingDelayParameters[5]);
    updateDuckingDelay();
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
    opticalCompressorControls.forEach(({ input }, index) => {
      input.value = String(testOptions.parameters.opticalCompressor[index]);
    });
    updateOpticalCompressor();
    fetCompressorControls.forEach(({ input }, index) => {
      input.value = String(testOptions.parameters.fetCompressor[index]);
    });
    updateFetCompressor();
    guitarGraphicLayout.value = "seven";
    guitarGraphicBands.forEach(({ input }, index) => {
      input.value = String(testOptions.parameters.guitarGraphicEQ[index + 1]);
    });
    updateGuitarGraphicEQ();
    bassGraphicLayout.value = "seven";
    bassGraphicBands.forEach(({ input }, index) => {
      input.value = String(testOptions.parameters.bassGraphicEQ[index + 1]);
    });
    updateBassGraphicEQ();
    shelfTiltMode.value = String(testOptions.parameters.shelfTiltEQ[0]);
    shelfTiltFrequency.value = String(testOptions.parameters.shelfTiltEQ[1]);
    shelfTiltGain.value = String(testOptions.parameters.shelfTiltEQ[2]);
    updateShelfTiltEQ();
    [roomReverbSize.value, roomReverbDecay.value, roomReverbPreDelay.value,
      roomReverbDamping.value, roomReverbMix.value, roomReverbLevel.value] =
      ["0", "0.8", "10", "6000", "20", "0"];
    updateRoomReverb();
    [springReverbDecay.value, springReverbPreDelay.value, springReverbDamping.value,
      springReverbDwell.value, springReverbMix.value, springReverbLevel.value] =
      ["2.4", "0", "8000", "35", "20", "0"];
    updateSpringReverb();
    delayPreset.value = "slapback";
    applyDelayPreset();
    if (DEFAULT_PARAMETERS.delay[0] !== 110 || DEFAULT_PARAMETERS.delay[1] !== 0.12 ||
        DEFAULT_PARAMETERS.delay[2] !== 0.25 || delayMode.value !== "free") {
      throw new Error("The slapback preset did not set its advertised delay controls.");
    }
    delayPreset.value = "tape";
    applyDelayPreset();
    if (DEFAULT_PARAMETERS.delay[0] !== 300 || DEFAULT_PARAMETERS.delay[1] !== 0.35 ||
        delayCharacter.value !== "tape" || DEFAULT_PARAMETERS.delayTape[0] !== 1 ||
        delayTapeControls.hidden) {
      throw new Error("The tape preset did not enable its controls and advertised delay settings.");
    }
    delayTapeInputs[1].input.value = "0.4";
    delayTapeInputs[1].input.dispatchEvent(new Event("input", { bubbles: true }));
    if (Math.abs(DEFAULT_PARAMETERS.delayTape[2] - 0.4) > 1e-6) {
      throw new Error("Changing the tape wow control did not update Worklet state.");
    }
    delayCharacter.value = "bbd";
    updateDelayCharacter();
    if (!DEFAULT_PARAMETERS.delayBbd || DEFAULT_PARAMETERS.delayTape[0] !== 0 ||
        !delayTapeControls.hidden) {
      throw new Error("The BBD-style delay character did not update Worklet state.");
    }
    delayCharacter.value = "digital";
    updateDelayCharacter();
    if (DEFAULT_PARAMETERS.delayBbd || DEFAULT_PARAMETERS.delayTape[0] !== 0) {
      throw new Error("Digital delay did not disable BBD and tape coloration.");
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
    delayCharacter.value = "tape";
    updateDelayCharacter();
    delayMode.value = "free";
    delayRouting.value = "tape-heads";
    delayTapeHeadSpacing.value = "180";
    for (const input of delayTapeHeadInputs) input.checked = true;
    updateDelayRouting();
    if (delayTapeHeadControls.hidden || DEFAULT_PARAMETERS.delayTapeHeads[0] !== 180 ||
        DEFAULT_PARAMETERS.delayTapeHeads[1] !== 15) {
      throw new Error("The multi-head delay controls did not select all four heads at 180 ms spacing.");
    }
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
    reverseDelayRestart.click();
    reverseDelayRun.checked = false;
    reverseDelayRun.dispatchEvent(new Event("change", { bubbles: true }));
    reverseDelayRun.checked = true;
    reverseDelayRun.dispatchEvent(new Event("change", { bubbles: true }));

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
