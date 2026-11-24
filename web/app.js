import { decodePcmWave, decodeCabinetIrWave } from "./wav_decoder.js";

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
  "Bit Crusher",
  "Dimension Chorus",
  "Triple-Double Chorus",
  "Four-band resonator",
  "Diatonic Harmonizer",
  "Pitch Arpeggiator · project-authored mono tracker",
  "Mono Guitar Synth · project-authored single-note tracker",
  "Note Generator · project-authored free-running oscillators",
  "Grain Delay · project-authored",
  "Pitch Feedback Delay · project-authored",
  "Feedback Generator · project-authored mono tracker",
  "Shimmer Reverb · project-authored pitch-fed reverb",
  "Convolution Reverb · project-authored user IR",
  "Five-band parametric EQ · project-authored",
  "Phrase Looper · project-authored one-layer utility",
  "Stereo Enhancer · project-authored",
  "Resonant Delay · project-authored",
  "Harmonic Tremolo · project-authored",
  "Auto-Swell Delay · project-authored",
  "Shuffling Looper · project-authored",
  "Ten-Tap Delay · project-authored",
  "Auto-Pan · project-authored",
  "Megatap Delay · project-authored",
  "De-Esser · project-authored",
  "Dual-Engine Distortion · project-authored",
  "Vibe · project-authored",
  "Multi-Delay · project-authored",
  "Porta Preamp · project-authored",
  "Pitch Sequence · project-authored poly transposition",
  "Rhythm Tap Delay · project-authored learned rhythm",
  "Plex Matrix Delay · project-authored feedback matrix",
  "Celestial Reverb · project-authored dual-pitch feedback",
  "Three-Voice Tracked Synth · project-authored",
  "Cloud Reverb · project-authored modulated reverb",
  "Three-Stage Distortion · project-authored Mercator-inspired controls",
  "Manual Through-Zero Flanger · project-authored Fractal-inspired controls",
  "Octobot · project-authored Fender control roles",
  "AM Ring Mod · project-authored Line 6-inspired category coverage",
  "Tape Echo · project-authored Fender control roles",
  "Octave Fuzz · project-authored Fender control roles",
  "Stereo Crossover · project-authored Fractal XVR-inspired controls",
  "Asheville Pattrn · project-authored filter bank",
  "Rock Box Dist · project-authored",
  "Rock Box Boost · project-authored",
  "Stereo Panner · project-authored Fractal FM9-family controls",
  "Poly Sustain · project-authored Helix Stadium-inspired",
  "Step Filter Delay · project-authored controls",
  "Volume/Pan · project-authored Fractal FM9-family controls",
  "Output level",
  "Peak limiter",
  "Stereo Filter · project-authored controls",
  "Step Tremolo · project-authored controls",
  "Deluxe Looper · project-authored",
  "Chromatic Pitch Delay · project-authored Fender-inspired control roles",
  "Cabinet IR · project-authored mono impulse response",
  "Octave Divider · project-authored Fractal-inspired controls",
  "Double Take · project-authored dynamic double",
  "EQ5 Graphic · project-authored five-band EQ",
  "Optical Tremolo · project-authored Fractal FM9-family controls",
  "Bias Trem · project-authored Fractal Blocks Guide-inspired controls",
  "Vibratone · project-authored rotating-baffle effect",
  "Uni-Verse Vibe · project-authored",
  "4 OSC Generator · project-authored free-running source",
  "Poly Detune · project-authored modulated micro-delay",
];
const DEFAULT_PARAMETERS = {
  volumePan: [1, -96, 0, 0, 0, 0, -100, 100, 0, 0, 0],
  stereoFilter: [1, 1000, 2, 0.707, 0, 0, 0, -100, 100, 0, 5, 0.5, 0.5, 0, 0, 0, 0, 0.5, 50, 4000, 0, 0, 1, 0, 120, 1, -36, 5, 80, 0, 0.25, 0, 0, 4],
  stepTremolo: [4, 0.75, 8, 1, 0.75, 0.5, 0.75, 1, 0.75, 0.5, 0.75, 1, 0.75, 0.5, 0.75, 1, 0.75, 0.5, 0.75, 4],
  octaveDivider: [1, 0.45, -0.25, 0.2, 0.25, 0.65, 1],
  doubleTake: [2, 0.25, 0.55, 0.85, 0.15, 0],
  eq5Graphic: [0, 0, 0, 0, 0, 0],
  opticalTremolo: [0, 4.5, 0.5, 0, 0, 120, 1, 50, 50, 0, 0, 0, -36, 0, 1, 0, 0, 0],
  fractalBiasTrem: [4.5, 0.35, 0, 0, 120, 1, 0, -36, 0, 0.18, 1, 0, 0, 0],
  vibratone: [0, 0.8, 0, 6.5, 0.85, 0, 650, 0.5, 0],
  octaveFuzz: [0.55, 0.75, 1],
  rockBoxDistortion: [0.5, 0.5, 0, 10],
  rockBoxBoost: [0, 0, 10],
  stereoCrossover: {
    frequencyHz: 500,
    frequencyMultiplier: "x1",
    lowLevelLeftDb: 0,
    lowLevelRightDb: 0,
    highLevelLeftDb: 0,
    highLevelRightDb: 0,
    lowPanLeft: -1,
    highPanLeft: -1,
    lowPanRight: 1,
    highPanRight: 1,
    levelDb: 0,
    balance: 0,
  },
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
  harmonicTremolo: [3, 0.65, 700, 0, 0, 120, 1, 1, 10],
  chorus: [0.8, 15, 7, 0.3],
  dimensionChorus: [0, 0, 0.5, 0],
  tripleDoubleChorus: [0, 0, 0.8, 7, 5, 8, 0.1, 0.2, 1],
  fourBandResonator: [0, 0, 1, 110, 164.81, 220, 329.63, 110,
    0.75, 0.75, 0.75, 0.75, 2, 2, 2, 2, 0.5, 0.5, 0.5, 0.5,
    -0.75, -0.25, 0.25, 0.75, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0.5],
  diatonicHarmonizer: [0.35, 2, 0, 0, 8000, 0, 0.2, 0, 120],
  diatonicHarmonizerCustomScale: [7, 0, 2, 4, 5, 7, 9, 11, 0],
  pitchArpeggiator: [1, 1, 1, 0, 0, 4, 0, 7, 120, 250, 0, 0, 0, 0,
    0, 2, 4, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -90],
  monoGuitarSynth: [2, 0.75, 0.6, -54, 8, 180, 8],
  noteGenerator: [3, 0, 40, 0, 0.45, 1, 47, 1, 0.3, 2, 52, 0, 0.25, 0.6],
  fourOscGenerator: [0, 48, 0, 0.15, 0, 55, 0, 0.15,
    0, 60, 0, 0.15, 0, 64, 0, 0.15, 0.35],
  polyDetune: [2, 0.25, 0.75, 0.75, 0.25, 0],
  fractalThreeVoiceSynth: [
    3, 2, 440, 0, 0, 0.5, 0, 12000, 0.707, 5, 0.25,
    3, 2, 440, 12, 0, 0.5, -0.35, 12000, 0.707, 10, 0.2,
    3, 2, 440, -12, 0, 0.5, 0.35, 12000, 0.707, 20, 0.2,
    0,
  ],
  enhancer: [0, 1, 0.25, 120, 12000, 7, 0, 0, 1, 0, 1],
  resonantDelay: [350, 0.35, 800, 2, 0.25],
  autoSwellDelay: [350, 0.25, 0.25, 0, 4, 120, 300, 0.15, 0.7, -36, 1, 0, 0],
  shufflingLooper: [8, 8, 0.5, 0, 0, 0, 0, 0, 0.7, 0, 0, -12, 12],
  tenTapDelay: [4, 220, 120, 0, 30, 0, 50, 100, 1, 100, 0, 20000, 0.25, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
  autoPan: [0.5, 0.75, 1, 0, 120, 5],
  stereoPanner: [0.5, 100, 0, 0, 120, 5, 50, 50, 180, 0, 0, 0, 0, 10],
  megatapDelay: [1000, 16, 1, 0, 2, 1, 0, 1, 100, 0, 0, 0, 16, 20, 20000,
    0, 10, 0.5, 0, -70, 400, 15, 0, 0.25, 0, 0, 0],
  deEsser: [6500, 1.5, -30, 12, 2, 80],
  dualEngineDistortion: [0.25, 0.7, 0.6, 0.5, 0.5, 0, 0, 0],
  vibe: [1.5, 0.7, 0.2, 2, 0.5, 1, 0, 4, 120],
  uniVerseVibe: [1.5, 0.65, 1, 0, 4, 120, 0, 0],
  multiDelay: {
    topology: 0,
    lines: [
      { timeMs: 250, timeMode: 0, division: 4, feedback: 0, crossFeedback: 0, level: 1, pan: 0.1 },
      { timeMs: 375, timeMode: 0, division: 3, feedback: 0, crossFeedback: 0, level: 1, pan: 0.35 },
      { timeMs: 500, timeMode: 0, division: 2, feedback: 0, crossFeedback: 0, level: 1, pan: 0.65 },
      { timeMs: 750, timeMode: 0, division: 0, feedback: 0, crossFeedback: 0, level: 1, pan: 0.9 },
    ],
    tempoBpm: 120, lowCutHz: 20, highCutHz: 20000,
    inputGainDb: 0, mix: 0.25, levelDb: 0,
    seriesFeedback: 0, seriesFeedbackSend: 3, seriesFeedbackReturn: 0,
    masterTime: 1, masterFeedback: 0.5, motorSpeed: 1,
    timeLfo: [
      { waveform: 0, phaseDegrees: 90, rateHz: 0.5, tempoSync: 0, division: 4, depthMs: 0 },
      { waveform: 0, phaseDegrees: 90, rateHz: 0.5, tempoSync: 0, division: 4, depthMs: 0 },
    ],
  },
  portaPreamp: [6, 3, 0, 0, 0],
  pitchSequence: [
    0, 7, 12, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    4, 0, 0, 4, 250, 120, 15, 7, 40, 80, 1, 1,
  ],
  polySustain: [0, 222, 2000, 1, 0, 0],
  stepFilterDelay: [350, 0, 4, 1, 120, 10, 8, 0.35, 0.25, 0, 120, 6000, 0.707, 1, 0.15, 4, 0,
    0, 0.72, 0.18, 0.88, 0.32, 0.62, 0.08, 0.96,
    0.24, 0.78, 0.12, 0.90, 0.38, 0.68, 0.04, 0.84],
  rhythmTapDelay: [
    4, 120, 0, 25, 0, 50, 1, 100, 0, 20000, 0.25, 0, 0,
    220, 220, 220, 220, 220, 220, 220, 220, 220, 220,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  ],
  plexDelay: {
    activeLines: 8, inputSelect: 0, tempoBpm: 120, masterTime: 1, masterLevel: 1,
    masterPan: 1, decaySeconds: 3, diffusion: 0.5,
    inputDiffusionMix: 0, inputDiffusionTimeMs: 10, lowCutHz: 20,
    highCutHz: 20000, preDelayMs: 0, stackMode: 0,
    inputGainDb: 0, mix: 0.25, killDry: false, levelDb: 0, balance: 0,
    wetFilterType: 0, wetFilterFrequencyHz: 2000, wetFilterQ: 0.707,
    wetFilterGainDb: 0, filterLfoEnabled: false, filterLfoType: 0,
    filterLfoRateHz: 0.5, filterModFrequencyHz: 2000,
    envelopeEnabled: false, envelopeThresholdDb: -70,
    envelopeAttackMs: 400, envelopeReleaseMs: 15,
    duckerAttenuationDb: 0, duckerThresholdDb: -30,
    duckerReleaseMs: 200, stereoSpread: 1,
    lines: [
      { timeMs: 45, timeMode: 0, division: 9, level: 1, pan: -0.85 },
      { timeMs: 70, timeMode: 0, division: 8, level: 1, pan: -0.6 },
      { timeMs: 110, timeMode: 0, division: 7, level: 1, pan: -0.35 },
      { timeMs: 150, timeMode: 0, division: 6, level: 1, pan: -0.1 },
      { timeMs: 220, timeMode: 0, division: 4, level: 1, pan: 0.1 },
      { timeMs: 300, timeMode: 0, division: 3, level: 1, pan: 0.35 },
      { timeMs: 450, timeMode: 0, division: 2, level: 1, pan: 0.6 },
      { timeMs: 600, timeMode: 0, division: 0, level: 1, pan: 0.85 },
    ],
  },
  grainDelay: [350, 0.25, 0.25, 80, 2, 0, 0],
  pitchFeedbackDelay: [350, 0.35, 0.25, 7],
  feedbackGenerator: [1, 0.35, 15, 600, 0, 1, -48],
  shimmerReverb: [3, 6500, 0.85, 0.25, 80, 12, 1, 0.25, 0],
  celestialReverb: [0.25, 3, 12, 7, 2, 9000, 0.5, 0],
  cloudReverb: [0.25, 0.5, 7, 0.8, 9000, 80, 0.3, 0.15, 0],
  mercatorDistortion: [0.45, 0.45, 0, 0, 0, 0, 0, 0],
  manualThruZeroFlanger: [0.34, 8, 5, 0.5, 0, 3, 0.5, 10],
  octobot: [0.65, 0.2, 0.25],
  amRingMod: [440, 0.5, 1, 0],
  tapeEcho: [0, 350, 120, 4, 0.35, 0.25, 0.25, 0.35, 0, 0.15, 0,
    0, 350, 350, 4, 4],
  convolutionReverb: [0.25, 0],
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
  ringModulator: [0, 440, 1, 20000, 1, 0, 0],
  rhythmicSlicer: [1, 120, 8, 8, 1, 0.15, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0],
  stepFilter: [1, 120, 8, 8, 0, 120, 6000, 0.707, 1, 0.15, 4, 1, 0,
    0, 0.72, 0.18, 0.88, 0.32, 0.62, 0.08, 0.96,
    0.24, 0.78, 0.12, 0.9, 0.38, 0.68, 0.04, 0.84],
  ashevillePattrn: [0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0.5, 0.75, 3, 0.5, 10],
  notchFilter: [1000, 4, 1],
  formantFilter: [0, 4, 1, 0, 0, 0, 1, 4, 0, 0],
  harmonicResonator: [110, 1.2, 0.55, 0.5],
  bitCrusher: [16, 1, 1],
  multibandCompressor: [250, 2500, 1,
    -24, 2, 6, 10, 100, 0, 1, 0,
    -24, 2, 6, 10, 100, 0, 1, 0,
    -24, 2, 6, 10, 100, 0, 1, 0],
  reverseDelay: [350, 0.25, 0.35, 10, 0, 4, 120, 0, 0, 0, 0],
  reverseDelayRun: true,
  modulatedDelay: [350, 0.25, 0.25, 0.8, 2, 0, 180, -24],
  filteredDelay: [350, 0.25, 0.25, 120, 6000, 0, 120, 4],
  sweepDelay: [350, 0.25, 0.25, 250, 6000, 1.2, 0.5, 0, 90, 1, 0, 120, 4],
  drive: 3,
  eq: [120, 0, 0.707, 800, 0, 0.707, 5000, 0, 0.707],
  fiveBandEq: [100, 0, 0.707, 0, 12, 350, 0, 0.707, 0, 12,
    1000, 0, 0.707, 0, 12, 3500, 0, 0.707, 0, 12,
    8000, 0, 0.707, 0, 12, 0],
  delay: [350, 0.25, 0.25],
  delayBbd: false,
  delayBbdMode: false,
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
  pitchExpression: { enabled: false, targetSemitones: 12, position: 0 },
  chromaticPitchDelay: [0, 350, 0.25, 0.65, 0.25, 0, 120, 4, 0],
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
const MAX_CONVOLUTION_IR_FRAMES = 480000;

const fileInput = document.querySelector("#audio-file");
const convolutionIrFileInput = document.querySelector("#convolution-ir-file");
const convolutionIrStatus = document.querySelector("#convolution-ir-status");
const cabinetIrFileInput = document.querySelector("#cabinet-ir-file");
const cabinetIrStatus = document.querySelector("#cabinet-ir-status");
const effectCategoryFilter = document.querySelector("#effect-category-filter");
const effectSearch = document.querySelector("#effect-search");
const effectsList = document.querySelector("#effects-list");
const effectCount = document.querySelector("#effect-count");
const effectsEmpty = document.querySelector("#effects-empty");
const looperStatus = document.querySelector("#looper-status");
const deluxeLooperStatus = document.querySelector("#deluxe-looper-status");
const shufflingLooperStatus = document.querySelector("#shuffling-looper-status");
const looperReverse = document.querySelector("#looper-reverse");
const looperHalfSpeed = document.querySelector("#looper-half-speed");
const looperOverdubLevel = document.querySelector("#looper-overdub-level");
const looperOverdubLevelValue = document.querySelector("#looper-overdub-level-value");
const looperAutoRecord = document.querySelector("#looper-auto-record");
const looperAutoRecordThreshold = document.querySelector(
  "#looper-auto-record-threshold-db",
);
const deluxeLooperReverse = document.querySelector("#deluxe-looper-reverse");
const deluxeLooperHalfSpeed = document.querySelector("#deluxe-looper-half-speed");
const deluxeLooperPlayback = document.querySelector("#deluxe-looper-playback");
const deluxeLooperOverdubDecay = document.querySelector("#deluxe-looper-overdub-decay");
const deluxeLooperLowCut = document.querySelector("#deluxe-looper-low-cut-hz");
const deluxeLooperHighCut = document.querySelector("#deluxe-looper-high-cut-hz");
const fourBandResonatorControls = document.querySelector("#four-band-resonator-controls");
const fractalThreeVoiceSynthControlsContainer = document.querySelector(
  "#fractal-three-voice-synth-controls",
);
const diatonicHarmonizerControls = [
  document.querySelector("#diatonic-harmonizer-mix"),
  document.querySelector("#diatonic-harmonizer-interval"),
  document.querySelector("#diatonic-harmonizer-key"),
  document.querySelector("#diatonic-harmonizer-scale"),
  document.querySelector("#diatonic-harmonizer-tone"),
  document.querySelector("#diatonic-harmonizer-delay"),
  document.querySelector("#diatonic-harmonizer-feedback"),
  document.querySelector("#diatonic-harmonizer-division"),
  document.querySelector("#diatonic-harmonizer-tempo"),
];
const diatonicCustomScaleCount = document.querySelector(
  "#diatonic-harmonizer-custom-scale-count",
);
const diatonicCustomScaleNotes = [
  ...document.querySelectorAll("[data-diatonic-custom-scale-note]"),
];
const diatonicCustomScaleGroup = document.querySelector(
  "#diatonic-harmonizer-custom-scale-controls",
);
const pitchArpeggiatorControls = [
  document.querySelector("#pitch-arpeggiator-run"),
  document.querySelector("#pitch-arpeggiator-mix"),
  document.querySelector("#pitch-arpeggiator-level"),
  document.querySelector("#pitch-arpeggiator-key"),
  document.querySelector("#pitch-arpeggiator-scale"),
  document.querySelector("#pitch-arpeggiator-steps"),
  document.querySelector("#pitch-arpeggiator-repeats"),
  document.querySelector("#pitch-arpeggiator-division"),
  document.querySelector("#pitch-arpeggiator-tempo"),
  document.querySelector("#pitch-arpeggiator-step-ms"),
  document.querySelector("#pitch-arpeggiator-amplitude-shape"),
  document.querySelector("#pitch-arpeggiator-amplitude-alpha"),
  document.querySelector("#pitch-arpeggiator-pan-shape"),
  document.querySelector("#pitch-arpeggiator-pan-alpha"),
  ...Array.from({ length: 16 }, (_, index) =>
    document.querySelector(`#pitch-arpeggiator-step-${index + 1}`)),
  document.querySelector("#pitch-arpeggiator-trigger-threshold"),
];
const monoGuitarSynthControls = [
  document.querySelector("#mono-guitar-synth-waveform"),
  document.querySelector("#mono-guitar-synth-mix"),
  document.querySelector("#mono-guitar-synth-level"),
  document.querySelector("#mono-guitar-synth-gate-threshold-db"),
  document.querySelector("#mono-guitar-synth-attack-ms"),
  document.querySelector("#mono-guitar-synth-release-ms"),
  document.querySelector("#mono-guitar-synth-portamento-ms"),
];
const noteGeneratorControls = [
  document.querySelector("#note-generator-voice-count"),
  ...Array.from({ length: 3 }, (_, index) => {
    const voice = index + 1;
    return [
      document.querySelector(`#note-generator-voice-${voice}-waveform`),
      document.querySelector(`#note-generator-voice-${voice}-note`),
      document.querySelector(`#note-generator-voice-${voice}-octave`),
      document.querySelector(`#note-generator-voice-${voice}-level`),
    ];
  }).flat(),
  document.querySelector("#note-generator-output-level"),
];
const fourOscGeneratorControls = [];
const polyDetuneControls = [
  document.querySelector("#poly-detune-voices"),
  document.querySelector("#poly-detune-rate"),
  document.querySelector("#poly-detune-depth"),
  document.querySelector("#poly-detune-width"),
  document.querySelector("#poly-detune-mix"),
  document.querySelector("#poly-detune-level-db"),
];
const fractalThreeVoiceSynthControls = [];
const enhancerControls = [
  document.querySelector("#enhancer-mode"),
  document.querySelector("#enhancer-width"),
  document.querySelector("#enhancer-depth"),
  document.querySelector("#enhancer-low-cut-hz"),
  document.querySelector("#enhancer-high-cut-hz"),
  document.querySelector("#enhancer-classic-delay-ms"),
  document.querySelector("#enhancer-phase-invert"),
  document.querySelector("#enhancer-pan-left"),
  document.querySelector("#enhancer-pan-right"),
  document.querySelector("#enhancer-balance"),
  document.querySelector("#enhancer-mix"),
];
const resonantDelayControls = [
  document.querySelector("#resonant-delay-time-ms"),
  document.querySelector("#resonant-delay-feedback"),
  document.querySelector("#resonant-delay-center-hz"),
  document.querySelector("#resonant-delay-q"),
  document.querySelector("#resonant-delay-mix"),
];
const grainDelayControls = [
  document.querySelector("#grain-delay-time"),
  document.querySelector("#grain-delay-feedback"),
  document.querySelector("#grain-delay-mix"),
  document.querySelector("#grain-delay-size"),
  document.querySelector("#grain-delay-overlap"),
  document.querySelector("#grain-delay-pitch"),
  document.querySelector("#grain-delay-spray"),
];
const pitchFeedbackDelayControls = [
  document.querySelector("#pitch-feedback-delay-time"),
  document.querySelector("#pitch-feedback-delay-feedback"),
  document.querySelector("#pitch-feedback-delay-mix"),
  document.querySelector("#pitch-feedback-delay-pitch"),
];
const chromaticPitchDelayControls = [
  document.querySelector("#chromatic-pitch-delay-pitch"),
  document.querySelector("#chromatic-pitch-delay-time-ms"),
  document.querySelector("#chromatic-pitch-delay-feedback"),
  document.querySelector("#chromatic-pitch-delay-tone"),
  document.querySelector("#chromatic-pitch-delay-mix"),
  document.querySelector("#chromatic-pitch-delay-time-mode"),
  document.querySelector("#chromatic-pitch-delay-tempo-bpm"),
  document.querySelector("#chromatic-pitch-delay-tap-division"),
  document.querySelector("#chromatic-pitch-delay-spillover"),
];
const feedbackGeneratorControls = [
  document.querySelector("#feedback-generator-type"),
  document.querySelector("#feedback-generator-gain"),
  document.querySelector("#feedback-generator-attack-ms"),
  document.querySelector("#feedback-generator-release-ms"),
  document.querySelector("#feedback-generator-dry-kill"),
  document.querySelector("#feedback-generator-dry-level"),
  document.querySelector("#feedback-generator-silence-threshold-db"),
];
const shimmerReverbControls = [
  document.querySelector("#shimmer-reverb-decay"),
  document.querySelector("#shimmer-reverb-damping"),
  document.querySelector("#shimmer-reverb-diffusion"),
  document.querySelector("#shimmer-reverb-feedback"),
  document.querySelector("#shimmer-reverb-high-pass"),
  document.querySelector("#shimmer-reverb-pitch"),
  document.querySelector("#shimmer-reverb-intensity"),
  document.querySelector("#shimmer-reverb-mix"),
  document.querySelector("#shimmer-reverb-level"),
];
const celestialReverbControls = [
  document.querySelector("#celestial-reverb-mix"),
  document.querySelector("#celestial-reverb-decay"),
  document.querySelector("#celestial-reverb-pitch-1"),
  document.querySelector("#celestial-reverb-pitch-2"),
  document.querySelector("#celestial-reverb-pitch-decay"),
  document.querySelector("#celestial-reverb-high-cut"),
  document.querySelector("#celestial-reverb-blend"),
  document.querySelector("#celestial-reverb-spillover"),
];
const cloudReverbControls = [
  document.querySelector("#cloud-reverb-mix"),
  document.querySelector("#cloud-reverb-tone"),
  document.querySelector("#cloud-reverb-decay"),
  document.querySelector("#cloud-reverb-diffusion"),
  document.querySelector("#cloud-reverb-high-damp"),
  document.querySelector("#cloud-reverb-low-damp"),
  document.querySelector("#cloud-reverb-modulation-rate"),
  document.querySelector("#cloud-reverb-modulation-depth"),
  document.querySelector("#cloud-reverb-spillover"),
];
const mercatorDistortionControls = [
  document.querySelector("#mercator-drive"),
  document.querySelector("#mercator-drive-2"),
  document.querySelector("#mercator-bass-db"),
  document.querySelector("#mercator-mid-db"),
  document.querySelector("#mercator-treble-db"),
  document.querySelector("#mercator-bright-1"),
  document.querySelector("#mercator-bright-2"),
  document.querySelector("#mercator-level-db"),
];
const octobotControls = [
  document.querySelector("#octobot-direct"),
  document.querySelector("#octobot-down"),
  document.querySelector("#octobot-sizzle"),
];
const amRingModControls = [
  document.querySelector("#am-ring-mod-rate"),
  document.querySelector("#am-ring-mod-depth"),
  document.querySelector("#am-ring-mod-mix"),
  document.querySelector("#am-ring-mod-level-db"),
];
const tapeEchoControls = [
  document.querySelector("#tape-echo-time-mode"),
  document.querySelector("#tape-echo-time-ms"),
  document.querySelector("#tape-echo-tempo-bpm"),
  document.querySelector("#tape-echo-tap-division"),
  document.querySelector("#tape-echo-feedback"),
  document.querySelector("#tape-echo-mix"),
  document.querySelector("#tape-echo-record-level"),
  document.querySelector("#tape-echo-bias"),
  document.querySelector("#tape-echo-noise"),
  document.querySelector("#tape-echo-wow-flutter"),
  document.querySelector("#tape-echo-spillover"),
  document.querySelector("#tape-echo-independent-times"),
  document.querySelector("#tape-echo-time-left-ms"),
  document.querySelector("#tape-echo-time-right-ms"),
  document.querySelector("#tape-echo-tap-division-left"),
  document.querySelector("#tape-echo-tap-division-right"),
];
const octaveFuzzControls = [
  document.querySelector("#octave-fuzz-boost"),
  document.querySelector("#octave-fuzz-volume"),
  document.querySelector("#octave-fuzz-octave-switch"),
];
const rockBoxDistControls = [
  document.querySelector("#rock-box-dist-drive"),
  document.querySelector("#rock-box-dist-tone"),
  document.querySelector("#rock-box-dist-level-db"),
  document.querySelector("#rock-box-dist-smoothing-ms"),
];
const rockBoxBoostControls = [
  document.querySelector("#rock-box-boost-boost-db"),
  document.querySelector("#rock-box-boost-level-db"),
  document.querySelector("#rock-box-boost-smoothing-ms"),
];
const stereoCrossoverControls = {
  frequencyHz: document.querySelector("#stereo-crossover-frequency-hz"),
  frequencyMultiplier: document.querySelector("#stereo-crossover-frequency-multiplier"),
  lowLevelLeftDb: document.querySelector("#stereo-crossover-low-level-left-db"),
  lowLevelRightDb: document.querySelector("#stereo-crossover-low-level-right-db"),
  highLevelLeftDb: document.querySelector("#stereo-crossover-high-level-left-db"),
  highLevelRightDb: document.querySelector("#stereo-crossover-high-level-right-db"),
  lowPanLeft: document.querySelector("#stereo-crossover-low-pan-left"),
  highPanLeft: document.querySelector("#stereo-crossover-high-pan-left"),
  lowPanRight: document.querySelector("#stereo-crossover-low-pan-right"),
  highPanRight: document.querySelector("#stereo-crossover-high-pan-right"),
  levelDb: document.querySelector("#stereo-crossover-level-db"),
  balance: document.querySelector("#stereo-crossover-balance"),
};
const manualThruZeroFlangerControls = [
  document.querySelector("#manual-tzf-minimum-time"),
  document.querySelector("#manual-tzf-maximum-time"),
  document.querySelector("#manual-tzf-manual"),
  document.querySelector("#manual-tzf-dry-delay-position"),
  document.querySelector("#manual-tzf-feedback"),
  document.querySelector("#manual-tzf-phase-reverse"),
  document.querySelector("#manual-tzf-mix"),
  document.querySelector("#manual-tzf-smoothing"),
];
const convolutionReverbControls = [
  document.querySelector("#convolution-reverb-mix"),
  document.querySelector("#convolution-reverb-level"),
  document.querySelector("#convolution-reverb-high-pass"),
  document.querySelector("#convolution-reverb-low-pass"),
];
const convolutionReverbTails = document.querySelector("#convolution-reverb-tails");

function appendResonatorControl(parent, labelText, index, options) {
  const label = document.createElement("label");
  label.className = "mix-control";
  const id = `four-band-resonator-${options.id}`;
  label.htmlFor = id;
  const title = document.createElement("span");
  title.textContent = labelText;
  const output = document.createElement("output");
  output.htmlFor = id;
  const control = options.choices ? document.createElement("select") : document.createElement("input");
  control.id = id;
  control.dataset.resonatorIndex = String(index);
  control.dataset.displayFormat = options.format;
  if (options.choices) {
    for (const [value, text] of options.choices) {
      const option = document.createElement("option");
      option.value = String(value);
      option.textContent = text;
      control.append(option);
    }
  } else {
    control.type = "range";
    control.min = String(options.min);
    control.max = String(options.max);
    control.step = String(options.step);
  }
  control.value = String(DEFAULT_PARAMETERS.fourBandResonator[index]);
  label.append(title, output, control);
  parent.append(label);
  return control;
}

function buildFourBandResonatorControls() {
  const modeGroup = document.createElement("div");
  modeGroup.className = "gain-controls";
  appendResonatorControl(modeGroup, "Mode", 0, {
    id: "mode", format: "choice", choices: [[0, "Manual"], [1, "Chord"]],
  });
  appendResonatorControl(modeGroup, "Chord", 1, {
    id: "chord", format: "choice",
    choices: [[0, "Major"], [1, "Major 6"], [2, "Dominant 7"], [3, "Major 7"],
      [4, "Minor"], [5, "Minor 6"], [6, "Minor 7"], [7, "Diminished"], [8, "Augmented"]],
  });
  appendResonatorControl(modeGroup, "Master Freq · project-authored range", 2, {
    id: "master-frequency", format: "factor", min: 0.1, max: 10, step: 0.01,
  });
  appendResonatorControl(modeGroup, "Chord-mode Frequency · project-authored range", 7, {
    id: "frequency-hz", format: "hz", min: 50, max: 5000, step: 0.1,
  });
  appendResonatorControl(modeGroup, "Input routing", 28, {
    id: "input-mode", format: "choice", choices: [[0, "Mono sum to all bands"], [1, "Stereo pairs"]],
  });
  appendResonatorControl(modeGroup, "Master Level · project-authored range", 29, {
    id: "master-level", format: "factor", min: 0, max: 2, step: 0.01,
  });
  appendResonatorControl(modeGroup, "Master Pan · project-authored range", 30, {
    id: "master-pan", format: "pan", min: -1, max: 1, step: 0.01,
  });
  appendResonatorControl(modeGroup, "Master Feedback · project-authored range", 31, {
    id: "master-feedback", format: "ratio", min: 0, max: 1, step: 0.01,
  });
  appendResonatorControl(modeGroup, "Master Q · project-authored range", 32, {
    id: "master-q", format: "ratio", min: 0.1, max: 4, step: 0.01,
  });
  appendResonatorControl(modeGroup, "Mix · project-authored range", 33, {
    id: "mix", format: "percent", min: 0, max: 1, step: 0.01,
  });
  fourBandResonatorControls.append(modeGroup);

  for (let band = 0; band < 4; band += 1) {
    const details = document.createElement("details");
    details.className = "resonator-band-controls";
    const summary = document.createElement("summary");
    summary.textContent = `Band ${band + 1}`;
    details.append(summary);
    const group = document.createElement("div");
    group.className = "gain-controls";
    appendResonatorControl(group, `Frequency · project-authored range`, 3 + band, {
      id: `band-${band + 1}-frequency-hz`, format: "hz", min: 100, max: 10000, step: 0.01,
    });
    appendResonatorControl(group, "Feedback · project-authored range", 8 + band, {
      id: `band-${band + 1}-feedback`, format: "ratio", min: 0, max: 0.98, step: 0.01,
    });
    appendResonatorControl(group, "Filter Q · project-authored range", 12 + band, {
      id: `band-${band + 1}-q`, format: "ratio", min: 0.5, max: 12, step: 0.01,
    });
    appendResonatorControl(group, "Level · project-authored range", 16 + band, {
      id: `band-${band + 1}-level`, format: "factor", min: 0, max: 2, step: 0.01,
    });
    appendResonatorControl(group, "Pan · project-authored range", 20 + band, {
      id: `band-${band + 1}-pan`, format: "pan", min: -1, max: 1, step: 0.01,
    });
    appendResonatorControl(group, "Filter Location", 24 + band, {
      id: `band-${band + 1}-filter-location`, format: "choice",
      choices: [[0, "Pre resonator"], [1, "Post resonator"]],
    });
    details.append(group);
    fourBandResonatorControls.append(details);
  }
}

buildFourBandResonatorControls();

function buildFractalThreeVoiceSynthControls() {
  const trackingLabel = document.createElement("label");
  trackingLabel.className = "mix-control";
  trackingLabel.htmlFor = "fractal-three-voice-synth-input-tracking";
  const trackingTitle = document.createElement("span");
  trackingTitle.textContent = "Input tracking range · all voices";
  const trackingOutput = document.createElement("output");
  trackingOutput.id = "fractal-three-voice-synth-input-tracking-value";
  trackingOutput.htmlFor = "fractal-three-voice-synth-input-tracking";
  const trackingSelect = document.createElement("select");
  trackingSelect.id = "fractal-three-voice-synth-input-tracking";
  trackingSelect.setAttribute("aria-label", "Synth input tracking range for all voices");
  for (const [value, text] of [[0, "Guitar"], [1, "Bass"]]) {
    const option = document.createElement("option");
    option.value = String(value);
    option.textContent = text;
    trackingSelect.append(option);
  }
  trackingSelect.value = String(DEFAULT_PARAMETERS.fractalThreeVoiceSynth[33]);
  trackingLabel.append(trackingTitle, trackingOutput, trackingSelect);
  fractalThreeVoiceSynthControlsContainer.append(trackingLabel);

  const waveforms = [
    [0, "Sine"], [1, "Triangle"], [2, "Square"], [3, "Sawtooth"],
    [4, "Random sample/hold"], [5, "White noise"], [6, "Pink noise"], [7, "Off"],
  ];
  const trackingModes = [
    [0, "Off"], [1, "Envelope only"], [2, "Pitch + envelope"], [3, "Quantize"],
  ];
  const voiceControls = [
    { label: "Waveform", choices: waveforms, format: "choice" },
    { label: "Tracking mode", choices: trackingModes, format: "choice" },
    { label: "Frequency · project range", min: 20, max: 2000, step: 1, format: "hz" },
    { label: "Shift · project range", min: -24, max: 24, step: 1, format: "semitones" },
    { label: "Tune · guide documents ±50 cents", min: -50, max: 50, step: 1, format: "cents" },
    { label: "Duty cycle · project range", min: 0.05, max: 0.95, step: 0.01, format: "percent" },
    { label: "Pan · project range", min: -1, max: 1, step: 0.01, format: "pan" },
    { label: "Filter frequency · project range", min: 20, max: 20000, step: 1, format: "hz" },
    { label: "Filter Q · project range", min: 0.25, max: 12, step: 0.01, format: "q" },
    { label: "Attack · project range", min: 0, max: 2000, step: 1, format: "ms" },
    { label: "Level · project range", min: 0, max: 1, step: 0.01, format: "percent" },
  ];
  for (let voice = 0; voice < 3; voice += 1) {
    const details = document.createElement("details");
    details.className = "resonator-band-controls";
    const summary = document.createElement("summary");
    summary.textContent = `Voice ${voice + 1}`;
    details.append(summary);
    const group = document.createElement("div");
    group.className = "gain-controls";
    for (let parameter = 0; parameter < voiceControls.length; parameter += 1) {
      const specification = voiceControls[parameter];
      const index = voice * voiceControls.length + parameter;
      const id = `fractal-three-voice-synth-voice-${voice + 1}-${[
        "waveform", "tracking", "frequency-hz", "shift-semitones", "tune-cents",
        "duty-cycle", "pan", "filter-frequency-hz", "filter-q", "attack-ms", "level",
      ][parameter]}`;
      const label = document.createElement("label");
      label.className = "mix-control";
      label.htmlFor = id;
      const title = document.createElement("span");
      title.textContent = specification.label;
      const output = document.createElement("output");
      output.id = `${id}-value`;
      output.htmlFor = id;
      const control = specification.choices
        ? document.createElement("select")
        : document.createElement("input");
      control.id = id;
      control.dataset.fractalSynthIndex = String(index);
      if (specification.choices) {
        for (const [value, text] of specification.choices) {
          const option = document.createElement("option");
          option.value = String(value);
          option.textContent = text;
          control.append(option);
        }
      } else {
        control.type = "range";
        control.min = String(specification.min);
        control.max = String(specification.max);
        control.step = String(specification.step);
      }
      control.value = String(DEFAULT_PARAMETERS.fractalThreeVoiceSynth[index]);
      label.append(title, output, control);
      group.append(label);
      fractalThreeVoiceSynthControls.push(control);
    }
    details.append(group);
    fractalThreeVoiceSynthControlsContainer.append(details);
  }
  fractalThreeVoiceSynthControls.push(trackingSelect);
}

buildFractalThreeVoiceSynthControls();

function buildFourOscGeneratorControls() {
  const container = document.querySelector("#four-osc-generator-control-list");
  if (!container) return;
  const waveformChoices = [
    [0, "Sine"], [1, "Triangle"], [2, "Sawtooth"], [3, "Square"],
  ];
  const addControl = (parent, id, labelText, valueIndex, specification) => {
    const label = document.createElement("label");
    label.className = "mix-control";
    label.htmlFor = id;
    const title = document.createElement("span");
    title.textContent = labelText;
    const output = document.createElement("output");
    output.id = `${id}-value`;
    output.htmlFor = id;
    const control = specification.choices
      ? document.createElement("select")
      : document.createElement("input");
    control.id = id;
    control.dataset.fourOscIndex = String(valueIndex);
    if (specification.choices) {
      for (const [value, text] of specification.choices) {
        const option = document.createElement("option");
        option.value = String(value);
        option.textContent = text;
        control.append(option);
      }
    } else {
      control.type = "range";
      control.min = String(specification.min);
      control.max = String(specification.max);
      control.step = String(specification.step);
    }
    control.value = String(DEFAULT_PARAMETERS.fourOscGenerator[valueIndex]);
    label.append(title, output, control);
    parent.append(label);
    fourOscGeneratorControls.push(control);
  };

  for (let voice = 0; voice < 4; voice += 1) {
    const details = document.createElement("details");
    details.className = "resonator-band-controls";
    const summary = document.createElement("summary");
    summary.textContent = `Oscillator ${voice + 1}`;
    details.append(summary);
    const group = document.createElement("div");
    group.className = "gain-controls";
    const offset = voice * 4;
    addControl(group, `four-osc-voice-${voice + 1}-waveform`, "Waveform",
      offset, { choices: waveformChoices });
    addControl(group, `four-osc-voice-${voice + 1}-note`, "Static MIDI note · 0–127",
      offset + 1, { min: 0, max: 127, step: 1 });
    addControl(group, `four-osc-voice-${voice + 1}-octave`, "Octave shift · −2…+2",
      offset + 2, { min: -2, max: 2, step: 1 });
    addControl(group, `four-osc-voice-${voice + 1}-level`, "Voice level · 0–100%",
      offset + 3, { min: 0, max: 1, step: 0.01 });
    details.append(group);
    container.append(details);
  }

  addControl(container, "four-osc-output-level", "Output level · 0–100%",
    16, { min: 0, max: 1, step: 0.01 });
}

buildFourOscGeneratorControls();

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
const pitchMode = document.querySelector("#pitch-mode");
const pitchSemitones = document.querySelector("#pitch-semitones");
const pitchSemitonesValue = document.querySelector("#pitch-semitones-value");
const pitchExpressionTarget = document.querySelector("#pitch-expression-target");
const pitchExpressionTargetValue = document.querySelector("#pitch-expression-target-value");
const pitchExpressionPosition = document.querySelector("#pitch-expression-position");
const pitchExpressionPositionValue = document.querySelector("#pitch-expression-position-value");
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
const harmonicTremoloRate = document.querySelector("#harmonic-tremolo-rate-hz");
const harmonicTremoloDepth = document.querySelector("#harmonic-tremolo-depth");
const harmonicTremoloCrossover = document.querySelector("#harmonic-tremolo-crossover-hz");
const harmonicTremoloWaveform = document.querySelector("#harmonic-tremolo-waveform");
const harmonicTremoloSync = document.querySelector("#harmonic-tremolo-sync");
const harmonicTremoloBpm = document.querySelector("#harmonic-tremolo-tempo-bpm");
const harmonicTremoloDivision = document.querySelector("#harmonic-tremolo-beats-per-cycle");
const harmonicTremoloStereo = document.querySelector("#harmonic-tremolo-stereo-alternation");
const harmonicTremoloSmoothing = document.querySelector("#harmonic-tremolo-smoothing-ms");
const autoSwellDelayTime = document.querySelector("#auto-swell-delay-time-ms");
const autoSwellDelayFeedback = document.querySelector("#auto-swell-delay-feedback");
const autoSwellDelayMix = document.querySelector("#auto-swell-delay-mix");
const autoSwellDelayTimeMode = document.querySelector("#auto-swell-delay-time-mode");
const autoSwellDelayTapDivision = document.querySelector("#auto-swell-delay-tap-division");
const autoSwellDelayTempo = document.querySelector("#auto-swell-delay-tempo-bpm");
const autoSwellDelaySwell = document.querySelector("#auto-swell-delay-swell-ms");
const autoSwellDelayMod = document.querySelector("#auto-swell-delay-mod");
const autoSwellDelayTone = document.querySelector("#auto-swell-delay-tone");
const autoSwellDelaySense = document.querySelector("#auto-swell-delay-sense-db");
const autoSwellDelayTaper = document.querySelector("#auto-swell-delay-taper");
const autoSwellDelayDry = document.querySelector("#auto-swell-delay-dry");
const autoSwellDelaySpillover = document.querySelector("#auto-swell-delay-spillover");
const shufflingLooperSlices = document.querySelector("#shuffling-looper-slices");
const shufflingLooperSequenceLength = document.querySelector("#shuffling-looper-sequence-length");
const shufflingLooperShuffle = document.querySelector("#shuffling-looper-shuffle");
const shufflingLooperPitchProbability = document.querySelector("#shuffling-looper-pitch-probability");
const shufflingLooperReverseProbability = document.querySelector("#shuffling-looper-reverse-probability");
const shufflingLooperRepeatProbability = document.querySelector("#shuffling-looper-repeat-probability");
const shufflingLooperSmoothing = document.querySelector("#shuffling-looper-smoothing");
const shufflingLooperSequenceDrift = document.querySelector("#shuffling-looper-sequence-drift");
const shufflingLooperPlayback = document.querySelector("#shuffling-looper-playback");
const shufflingLooperLowCut = document.querySelector("#shuffling-looper-low-cut-hz");
const shufflingLooperHighCut = document.querySelector("#shuffling-looper-high-cut-hz");
const shufflingLooperInterval1 = document.querySelector("#shuffling-looper-interval-1");
const shufflingLooperInterval2 = document.querySelector("#shuffling-looper-interval-2");
const shufflingLooperControls = [
  shufflingLooperSlices, shufflingLooperSequenceLength, shufflingLooperShuffle,
  shufflingLooperPitchProbability, shufflingLooperReverseProbability,
  shufflingLooperRepeatProbability, shufflingLooperSmoothing,
  shufflingLooperSequenceDrift, shufflingLooperPlayback, shufflingLooperLowCut,
  shufflingLooperHighCut, shufflingLooperInterval1, shufflingLooperInterval2,
];
const tenTapDelayTapCount = document.querySelector("#ten-tap-delay-tap-count");
const tenTapDelayTime = document.querySelector("#ten-tap-delay-time-ms");
const tenTapDelayTempo = document.querySelector("#ten-tap-delay-tempo-bpm");
const tenTapDelayDivision = document.querySelector("#ten-tap-delay-division");
const tenTapDelayDecay = document.querySelector("#ten-tap-delay-decay");
const tenTapDelayShuffle = document.querySelector("#ten-tap-delay-shuffle");
const tenTapDelaySpread = document.querySelector("#ten-tap-delay-spread");
const tenTapDelayRatio = document.querySelector("#ten-tap-delay-ratio");
const tenTapDelayPanShape = document.querySelector("#ten-tap-delay-pan-shape");
const tenTapDelayPanAlpha = document.querySelector("#ten-tap-delay-pan-alpha");
const tenTapDelayLowCut = document.querySelector("#ten-tap-delay-low-cut-hz");
const tenTapDelayHighCut = document.querySelector("#ten-tap-delay-high-cut-hz");
const tenTapDelayMix = document.querySelector("#ten-tap-delay-mix");
const tenTapDelayLevel = document.querySelector("#ten-tap-delay-level-db");
const tenTapDelayMono = document.querySelector("#ten-tap-delay-mono");
const tenTapDelayTapLevels = Array.from({ length: 10 }, (_, index) =>
  document.querySelector(`#ten-tap-delay-tap-${index + 1}-level-db`));
const tenTapDelayControls = [
  tenTapDelayTapCount, tenTapDelayTime, tenTapDelayTempo, tenTapDelayDivision,
  tenTapDelayDecay, tenTapDelayShuffle, tenTapDelaySpread, tenTapDelayRatio,
  tenTapDelayPanShape, tenTapDelayPanAlpha, tenTapDelayLowCut,
  tenTapDelayHighCut, tenTapDelayMix, tenTapDelayLevel, tenTapDelayMono,
  ...tenTapDelayTapLevels,
];
const rhythmTapRepeatCount = document.querySelector("#rhythm-tap-repeat-count");
const rhythmTapTempo = document.querySelector("#rhythm-tap-tempo-bpm");
const rhythmTapQuantize = document.querySelector("#rhythm-tap-quantize");
const rhythmTapFeedback = document.querySelector("#rhythm-tap-feedback");
const rhythmTapDecay = document.querySelector("#rhythm-tap-decay");
const rhythmTapSpread = document.querySelector("#rhythm-tap-spread");
const rhythmTapPanShape = document.querySelector("#rhythm-tap-pan-shape");
const rhythmTapPanAlpha = document.querySelector("#rhythm-tap-pan-alpha");
const rhythmTapLowCut = document.querySelector("#rhythm-tap-low-cut-hz");
const rhythmTapHighCut = document.querySelector("#rhythm-tap-high-cut-hz");
const rhythmTapMix = document.querySelector("#rhythm-tap-mix");
const rhythmTapLevel = document.querySelector("#rhythm-tap-level-db");
const rhythmTapMono = document.querySelector("#rhythm-tap-mono");
const rhythmTapTimes = Array.from({ length: 10 }, (_, index) =>
  document.querySelector(`#rhythm-tap-time-${index + 1}-value`));
const rhythmTapLevels = Array.from({ length: 10 }, (_, index) =>
  document.querySelector(`#rhythm-tap-level-${index + 1}-db`));
const rhythmTapControls = [
  rhythmTapRepeatCount, rhythmTapTempo, rhythmTapQuantize, rhythmTapFeedback,
  rhythmTapDecay, rhythmTapSpread, rhythmTapPanShape, rhythmTapPanAlpha,
  rhythmTapLowCut, rhythmTapHighCut, rhythmTapMix, rhythmTapLevel,
  rhythmTapMono, ...rhythmTapTimes, ...rhythmTapLevels,
];
const rhythmTapLearnStart = document.querySelector("#rhythm-tap-learn-start");
const rhythmTapLearnTap = document.querySelector("#rhythm-tap-learn-tap");
const rhythmTapLearnDone = document.querySelector("#rhythm-tap-learn-done");
const rhythmTapLearnStatus = document.querySelector("#rhythm-tap-learn-status");
let rhythmTapLearning = false;
let rhythmTapLastTapAt = null;
let rhythmTapCapturedIntervals = 0;
const plexDelayLineContainer = document.querySelector("#plex-delay-line-controls");
const plexDelayLineTemplate = document.querySelector("#plex-delay-line-template");
const plexDelayLines = DEFAULT_PARAMETERS.plexDelay.lines.map((parameters, index) => {
  const line = plexDelayLineTemplate.content.firstElementChild.cloneNode(true);
  line.dataset.plexLineIndex = String(index);
  line.querySelector("[data-plex-line-title]").textContent = `Delay line ${index + 1}`;
  for (const control of line.querySelectorAll("[data-plex-line-param]")) {
    const name = control.dataset.plexLineParam;
    const value = name === "level"
      ? parameters.level * 100
      : name === "pan" ? parameters.pan * 100 : parameters[name];
    control.value = String(value);
  }
  plexDelayLineContainer.append(line);
  return line;
});
const plexDelayGlobalControls = [
  ...document.querySelectorAll("[data-plex-global]"),
];
const plexDelayLineControls = plexDelayLines.flatMap((line) =>
  [...line.querySelectorAll("[data-plex-line-param]")]);
const autoPanRate = document.querySelector("#auto-pan-rate-hz");
const autoPanDepth = document.querySelector("#auto-pan-depth");
const autoPanWaveform = document.querySelector("#auto-pan-waveform");
const autoPanTempoSync = document.querySelector("#auto-pan-tempo-sync");
const autoPanTempo = document.querySelector("#auto-pan-tempo-bpm");
const autoPanDivision = document.querySelector("#auto-pan-division");
const autoPanControls = [
  autoPanRate, autoPanDepth, autoPanWaveform, autoPanTempoSync,
  autoPanTempo, autoPanDivision,
];
const stereoPannerControls = [
  document.querySelector("#stereo-panner-rate-hz"),
  document.querySelector("#stereo-panner-width-percent"),
  document.querySelector("#stereo-panner-waveform"),
  document.querySelector("#stereo-panner-tempo-sync"),
  document.querySelector("#stereo-panner-tempo-bpm"),
  document.querySelector("#stereo-panner-division"),
  document.querySelector("#stereo-panner-duty-cycle"),
  document.querySelector("#stereo-panner-shape"),
  document.querySelector("#stereo-panner-phase-degrees"),
  document.querySelector("#stereo-panner-pan-center-percent"),
  document.querySelector("#stereo-panner-ducking-percent"),
  document.querySelector("#stereo-panner-level-db"),
  document.querySelector("#stereo-panner-balance-percent"),
  document.querySelector("#stereo-panner-smoothing-ms"),
];
const megatapDelayTime = document.querySelector("#megatap-delay-time-ms");
const megatapDelayTapCount = document.querySelector("#megatap-delay-tap-count");
const megatapDelayTimeAlpha = document.querySelector("#megatap-delay-time-alpha");
const megatapDelayTimeRandomize = document.querySelector("#megatap-delay-time-randomize");
const megatapDelayAmplitudeShape = document.querySelector("#megatap-delay-amplitude-shape");
const megatapDelayAmplitudeAlpha = document.querySelector("#megatap-delay-amplitude-alpha");
const megatapDelayAmplitudeRandomize = document.querySelector("#megatap-delay-amplitude-randomize");
const megatapDelayPanShape = document.querySelector("#megatap-delay-pan-shape");
const megatapDelayPanAlpha = document.querySelector("#megatap-delay-pan-alpha");
const megatapDelayStereoSpread = document.querySelector("#megatap-delay-stereo-spread");
const megatapDelayPredelay = document.querySelector("#megatap-delay-predelay-ms");
const megatapDelayFeedback = document.querySelector("#megatap-delay-feedback");
const megatapDelayFeedbackTap = document.querySelector("#megatap-delay-feedback-tap");
const megatapDelayLowCut = document.querySelector("#megatap-delay-low-cut-hz");
const megatapDelayHighCut = document.querySelector("#megatap-delay-high-cut-hz");
const megatapDelayDiffusionMix = document.querySelector("#megatap-delay-diffusion-mix");
const megatapDelayDiffusionTime = document.querySelector("#megatap-delay-diffusion-time-ms");
const megatapDelayDiffusionRate = document.querySelector("#megatap-delay-diffusion-rate-hz");
const megatapDelayDiffusionDepth = document.querySelector("#megatap-delay-diffusion-depth-ms");
const megatapDelayEnvelopeEnabled = document.querySelector("#megatap-delay-envelope-enabled");
const megatapDelayEnvelopeThreshold = document.querySelector("#megatap-delay-envelope-threshold-db");
const megatapDelayEnvelopeAttack = document.querySelector("#megatap-delay-envelope-attack-ms");
const megatapDelayEnvelopeRelease = document.querySelector("#megatap-delay-envelope-release-ms");
const megatapDelayInputGain = document.querySelector("#megatap-delay-input-gain-db");
const megatapDelayMix = document.querySelector("#megatap-delay-mix");
const megatapDelayLevel = document.querySelector("#megatap-delay-level-db");
const megatapDelayInputSelect = document.querySelector("#megatap-delay-input-select");
const megatapDelayControls = [
  megatapDelayTime, megatapDelayTapCount, megatapDelayTimeAlpha,
  megatapDelayTimeRandomize, megatapDelayAmplitudeShape,
  megatapDelayAmplitudeAlpha, megatapDelayAmplitudeRandomize,
  megatapDelayPanShape, megatapDelayPanAlpha, megatapDelayStereoSpread,
  megatapDelayPredelay, megatapDelayFeedback, megatapDelayFeedbackTap,
  megatapDelayLowCut, megatapDelayHighCut, megatapDelayDiffusionMix,
  megatapDelayDiffusionTime, megatapDelayDiffusionRate,
  megatapDelayDiffusionDepth, megatapDelayEnvelopeEnabled,
  megatapDelayEnvelopeThreshold, megatapDelayEnvelopeAttack,
  megatapDelayEnvelopeRelease, megatapDelayInputGain, megatapDelayMix,
  megatapDelayLevel, megatapDelayInputSelect,
];
const deEsserFrequency = document.querySelector("#de-esser-frequency-hz");
const deEsserQ = document.querySelector("#de-esser-q");
const deEsserThreshold = document.querySelector("#de-esser-threshold-dbfs");
const deEsserReduction = document.querySelector("#de-esser-max-reduction-db");
const deEsserAttack = document.querySelector("#de-esser-attack-ms");
const deEsserRelease = document.querySelector("#de-esser-release-ms");
const deEsserControls = [
  deEsserFrequency, deEsserQ, deEsserThreshold,
  deEsserReduction, deEsserAttack, deEsserRelease,
];
const dualDistortionGainA = document.querySelector("#dual-distortion-gain-a");
const dualDistortionToneA = document.querySelector("#dual-distortion-tone-a");
const dualDistortionGainB = document.querySelector("#dual-distortion-gain-b");
const dualDistortionToneB = document.querySelector("#dual-distortion-tone-b");
const dualDistortionBlend = document.querySelector("#dual-distortion-blend");
const dualDistortionMode = document.querySelector("#dual-distortion-mode");
const dualDistortionBassBoost = document.querySelector("#dual-distortion-bass-boost");
const dualDistortionLevel = document.querySelector("#dual-distortion-level-db");
const dualDistortionControls = [
  dualDistortionGainA, dualDistortionToneA, dualDistortionGainB,
  dualDistortionToneB, dualDistortionBlend, dualDistortionMode,
  dualDistortionBassBoost, dualDistortionLevel,
];
const vibeRate = document.querySelector("#vibe-rate-hz");
const vibeDepth = document.querySelector("#vibe-depth");
const vibeTrem = document.querySelector("#vibe-trem");
const vibeStages = document.querySelector("#vibe-stages");
const vibeSpread = document.querySelector("#vibe-spread");
const vibeLevel = document.querySelector("#vibe-level");
const vibeRateMode = document.querySelector("#vibe-rate-mode");
const vibeRateDivision = document.querySelector("#vibe-rate-division");
const vibeTempoBpm = document.querySelector("#vibe-tempo-bpm");
const vibeControls = [
  vibeRate, vibeDepth, vibeTrem, vibeStages, vibeSpread, vibeLevel,
  vibeRateMode, vibeRateDivision, vibeTempoBpm,
];
const uniVerseVibeSpeed = document.querySelector("#uni-verse-vibe-speed-hz");
const uniVerseVibeIntensity = document.querySelector("#uni-verse-vibe-intensity");
const uniVerseVibeVolume = document.querySelector("#uni-verse-vibe-volume");
const uniVerseVibeRateMode = document.querySelector("#uni-verse-vibe-rate-mode");
const uniVerseVibeRateDivision = document.querySelector("#uni-verse-vibe-rate-division");
const uniVerseVibeTempoBpm = document.querySelector("#uni-verse-vibe-tempo-bpm");
const uniVerseVibeToneMode = document.querySelector("#uni-verse-vibe-tone-mode");
const uniVerseVibeModulationMode = document.querySelector("#uni-verse-vibe-modulation-mode");
const uniVerseVibeControls = [
  uniVerseVibeSpeed, uniVerseVibeIntensity, uniVerseVibeVolume,
  uniVerseVibeRateMode, uniVerseVibeRateDivision, uniVerseVibeTempoBpm,
  uniVerseVibeToneMode, uniVerseVibeModulationMode,
];
const portaPreampGain1 = document.querySelector("#porta-preamp-gain1-db");
const portaPreampGain2 = document.querySelector("#porta-preamp-gain2-db");
const portaPreampBass = document.querySelector("#porta-preamp-bass-db");
const portaPreampTreble = document.querySelector("#porta-preamp-treble-db");
const portaPreampLevel = document.querySelector("#porta-preamp-level-db");
const portaPreampControls = [portaPreampGain1, portaPreampGain2,
  portaPreampBass, portaPreampTreble, portaPreampLevel];
const pitchSequenceSteps = Array.from({ length: 16 }, (_, index) => ({
  semitones: document.querySelector(`#pitch-sequence-step-${index + 1}-semitones`),
  mode: document.querySelector(`#pitch-sequence-step-${index + 1}-mode`),
}));
const pitchSequenceStepsCount = document.querySelector("#pitch-sequence-steps");
const pitchSequenceRun = document.querySelector("#pitch-sequence-run");
const pitchSequenceTimeMode = document.querySelector("#pitch-sequence-time-mode");
const pitchSequenceDivision = document.querySelector("#pitch-sequence-division");
const pitchSequenceFreeMs = document.querySelector("#pitch-sequence-free-ms");
const pitchSequenceTempo = document.querySelector("#pitch-sequence-tempo");
const pitchSequenceGlide = document.querySelector("#pitch-sequence-glide-ms");
const pitchSequenceGlitchDepth = document.querySelector("#pitch-sequence-glitch-depth");
const pitchSequenceGlitchInterval = document.querySelector("#pitch-sequence-glitch-interval-ms");
const pitchSequenceFreezeWindow = document.querySelector("#pitch-sequence-freeze-window-ms");
const pitchSequenceMix = document.querySelector("#pitch-sequence-mix");
const pitchSequenceLevel = document.querySelector("#pitch-sequence-level");
const pitchSequenceControls = [
  ...pitchSequenceSteps.flatMap((step) => [step.semitones, step.mode]),
  pitchSequenceStepsCount, pitchSequenceRun, pitchSequenceTimeMode,
  pitchSequenceDivision, pitchSequenceFreeMs, pitchSequenceTempo,
  pitchSequenceGlide, pitchSequenceGlitchDepth, pitchSequenceGlitchInterval,
  pitchSequenceFreezeWindow, pitchSequenceMix, pitchSequenceLevel,
];
const polySustainInterval = document.querySelector("#poly-sustain-interval");
const polySustainAttack = document.querySelector("#poly-sustain-attack-ms");
const polySustainDecay = document.querySelector("#poly-sustain-decay-ms");
const polySustainFxLevel = document.querySelector("#poly-sustain-fx-level");
const polySustainLevel = document.querySelector("#poly-sustain-level-db");
const polySustainOperation = document.querySelector("#poly-sustain-operation");
const polySustainControls = [
  polySustainInterval, polySustainAttack, polySustainDecay,
  polySustainFxLevel, polySustainLevel, polySustainOperation,
];
const stepFilterDelayTimeMode = document.querySelector("#step-filter-delay-time-mode");
const stepFilterDelayTimeMs = document.querySelector("#step-filter-delay-time-ms");
const stepFilterDelayDivision = document.querySelector("#step-filter-delay-division");
const stepFilterDelayStepMode = document.querySelector("#step-filter-delay-step-mode");
const stepFilterDelayStepDivision = document.querySelector("#step-filter-delay-step-division");
const stepFilterDelayTempo = document.querySelector("#step-filter-delay-tempo-bpm");
const stepFilterDelayStepRate = document.querySelector("#step-filter-delay-step-rate-hz");
const stepFilterDelayFeedback = document.querySelector("#step-filter-delay-feedback");
const stepFilterDelayMix = document.querySelector("#step-filter-delay-mix");
const stepFilterDelayFilterMode = document.querySelector("#step-filter-delay-filter-mode");
const stepFilterDelayMinimumCutoff = document.querySelector("#step-filter-delay-minimum-cutoff");
const stepFilterDelayMaximumCutoff = document.querySelector("#step-filter-delay-maximum-cutoff");
const stepFilterDelayResonanceQ = document.querySelector("#step-filter-delay-resonance-q");
const stepFilterDelayDepth = document.querySelector("#step-filter-delay-depth");
const stepFilterDelayShape = document.querySelector("#step-filter-delay-shape");
const stepFilterDelaySmoothing = document.querySelector("#step-filter-delay-smoothing");
const stepFilterDelayPhase = document.querySelector("#step-filter-delay-phase");
const stepFilterDelaySteps = [...document.querySelectorAll("[data-step-filter-delay-step]")];
const stepFilterDelayControls = [
  stepFilterDelayTimeMode, stepFilterDelayTimeMs, stepFilterDelayDivision,
  stepFilterDelayStepMode, stepFilterDelayStepDivision, stepFilterDelayTempo,
  stepFilterDelayStepRate, stepFilterDelayFeedback, stepFilterDelayMix,
  stepFilterDelayFilterMode, stepFilterDelayMinimumCutoff,
  stepFilterDelayMaximumCutoff, stepFilterDelayResonanceQ, stepFilterDelayDepth,
  stepFilterDelayShape, stepFilterDelaySmoothing, stepFilterDelayPhase,
  ...stepFilterDelaySteps,
];
const volumePanPosition = document.querySelector("#volume-pan-position");
const volumePanMinimumDb = document.querySelector("#volume-pan-minimum-db");
const volumePanMaximumDb = document.querySelector("#volume-pan-maximum-db");
const volumePanTaper = document.querySelector("#volume-pan-taper");
const volumePanInputSelect = document.querySelector("#volume-pan-input-select");
const volumePanLeft = document.querySelector("#volume-pan-left");
const volumePanRight = document.querySelector("#volume-pan-right");
const volumePanBalance = document.querySelector("#volume-pan-balance");
const volumePanOutputLevel = document.querySelector("#volume-pan-output-level");
const volumePanBypassed = document.querySelector("#volume-pan-bypassed");
const volumePanBypassMode = document.querySelector("#volume-pan-bypass-mode");
const volumePanControls = [
  volumePanPosition, volumePanMinimumDb, volumePanMaximumDb, volumePanTaper,
  volumePanInputSelect, volumePanLeft, volumePanRight, volumePanBalance,
  volumePanOutputLevel, volumePanBypassed, volumePanBypassMode,
];
const stepTremoloStepCount = document.querySelector("#step-tremolo-step-count");
const stepTremoloRateHz = document.querySelector("#step-tremolo-rate-hz");
const stepTremoloDepth = document.querySelector("#step-tremolo-depth");
const stepTremoloTransitionMs = document.querySelector("#step-tremolo-transition-ms");
const stepTremoloLevels = [...document.querySelectorAll("[data-step-tremolo-level]")];
const stepTremoloControls = [
  stepTremoloStepCount, stepTremoloRateHz, stepTremoloDepth,
  stepTremoloTransitionMs, ...stepTremoloLevels,
];
const octaveDividerControls = [
  document.querySelector("#octave-divider-input-gain"),
  document.querySelector("#octave-divider-voice1-level"),
  document.querySelector("#octave-divider-voice1-pan"),
  document.querySelector("#octave-divider-voice2-level"),
  document.querySelector("#octave-divider-voice2-pan"),
  document.querySelector("#octave-divider-mix"),
  document.querySelector("#octave-divider-output-level"),
];
const doubleTakeControls = [
  document.querySelector("#double-take-doubles"),
  document.querySelector("#double-take-slop"),
  document.querySelector("#double-take-sensitivity"),
  document.querySelector("#double-take-dry-level"),
  document.querySelector("#double-take-wet-level"),
  document.querySelector("#double-take-source"),
];
const eq5GraphicControls = [
  document.querySelector("#eq5-band-1-db"),
  document.querySelector("#eq5-band-2-db"),
  document.querySelector("#eq5-band-3-db"),
  document.querySelector("#eq5-band-4-db"),
  document.querySelector("#eq5-band-5-db"),
  document.querySelector("#eq5-overall-gain-db"),
];
const opticalTremoloControls = [
  document.querySelector("#optical-tremolo-variant"),
  document.querySelector("#optical-tremolo-rate-hz"),
  document.querySelector("#optical-tremolo-depth"),
  document.querySelector("#optical-tremolo-lfo"),
  document.querySelector("#optical-tremolo-tempo-sync"),
  document.querySelector("#optical-tremolo-tempo-bpm"),
  document.querySelector("#optical-tremolo-beats-per-cycle"),
  document.querySelector("#optical-tremolo-duty-percent"),
  document.querySelector("#optical-tremolo-shape-percent"),
  document.querySelector("#optical-tremolo-phase-degrees"),
  document.querySelector("#optical-tremolo-start-phase-degrees"),
  document.querySelector("#optical-tremolo-trigger-enabled"),
  document.querySelector("#optical-tremolo-trigger-threshold-dbfs"),
  document.querySelector("#optical-tremolo-ducking-percent"),
  document.querySelector("#optical-tremolo-mix"),
  document.querySelector("#optical-tremolo-level-db"),
  document.querySelector("#optical-tremolo-balance-percent"),
  document.querySelector("#optical-tremolo-bypass-mode"),
];
const fractalBiasTremControls = [
  document.querySelector("#fractal-bias-trem-rate-hz"),
  document.querySelector("#fractal-bias-trem-depth"),
  document.querySelector("#fractal-bias-trem-waveform"),
  document.querySelector("#fractal-bias-trem-tempo-sync"),
  document.querySelector("#fractal-bias-trem-tempo-bpm"),
  document.querySelector("#fractal-bias-trem-beats-per-cycle"),
  document.querySelector("#fractal-bias-trem-trigger-sync"),
  document.querySelector("#fractal-bias-trem-trigger-threshold-db"),
  document.querySelector("#fractal-bias-trem-ducking"),
  document.querySelector("#fractal-bias-trem-drive"),
  document.querySelector("#fractal-bias-trem-mix"),
  document.querySelector("#fractal-bias-trem-level-db"),
  document.querySelector("#fractal-bias-trem-balance"),
  document.querySelector("#fractal-bias-trem-bypass"),
];
const vibratoneControls = [
  document.querySelector("#vibratone-drive"),
  document.querySelector("#vibratone-tone"),
  document.querySelector("#vibratone-level-db"),
  document.querySelector("#vibratone-fast-speed-hz"),
  document.querySelector("#vibratone-slow-speed-hz"),
  document.querySelector("#vibratone-speed-switch"),
  document.querySelector("#vibratone-ramp-ms"),
  document.querySelector("#vibratone-sensitivity"),
  document.querySelector("#vibratone-dynamics-switch"),
];
const stereoFilterControls = [
  document.querySelector("#stereo-filter-type"),
  document.querySelector("#stereo-filter-frequency-hz"),
  document.querySelector("#stereo-filter-order"),
  document.querySelector("#stereo-filter-q"),
  document.querySelector("#stereo-filter-gain-db"),
  document.querySelector("#stereo-filter-low-cut-hz"),
  document.querySelector("#stereo-filter-high-cut-hz"),
  document.querySelector("#stereo-filter-pan-left"),
  document.querySelector("#stereo-filter-pan-right"),
  document.querySelector("#stereo-filter-phase"),
  document.querySelector("#stereo-filter-delay-time-ms"),
  document.querySelector("#stereo-filter-depth"),
  document.querySelector("#stereo-filter-feedback"),
  document.querySelector("#stereo-filter-level-db"),
  document.querySelector("#stereo-filter-balance"),
  document.querySelector("#stereo-filter-lfo-enabled"),
  document.querySelector("#stereo-filter-lfo-shape"),
  document.querySelector("#stereo-filter-lfo-rate-hz"),
  document.querySelector("#stereo-filter-lfo-duty-percent"),
  document.querySelector("#stereo-filter-mod-frequency-hz"),
  document.querySelector("#stereo-filter-lfo-quantize-steps"),
  document.querySelector("#stereo-filter-mode"),
  document.querySelector("#stereo-filter-sweep-shape"),
  document.querySelector("#stereo-filter-tempo-sync"),
  document.querySelector("#stereo-filter-tempo-bpm"),
  document.querySelector("#stereo-filter-tempo-division"),
  document.querySelector("#stereo-filter-sensitivity-db"),
  document.querySelector("#stereo-filter-attack-ms"),
  document.querySelector("#stereo-filter-release-ms"),
  document.querySelector("#stereo-filter-emphasis"),
  document.querySelector("#stereo-filter-ladder-resonance"),
  document.querySelector("#stereo-filter-ladder-drive"),
  document.querySelector("#stereo-filter-ladder-bass-comp"),
  document.querySelector("#stereo-filter-ladder-order"),
];
const multiDelayLineControls = [...document.querySelectorAll("[data-multi-delay-line]")]
  .map((row) => Object.fromEntries(
    [...row.querySelectorAll("[data-control]")]
      .map((control) => [control.dataset.control, control]),
  ));
const multiDelayLfoControls = [...document.querySelectorAll("[data-multi-delay-lfo]")]
  .map((row) => Object.fromEntries(
    [...row.querySelectorAll("[data-control]")]
      .map((control) => [control.dataset.control, control]),
  ));
const multiDelayTempo = document.querySelector("#multi-delay-tempo");
const multiDelayTopology = document.querySelector("#multi-delay-topology");
const multiDelayLowCut = document.querySelector("#multi-delay-low-cut");
const multiDelayHighCut = document.querySelector("#multi-delay-high-cut");
const multiDelayInputGain = document.querySelector("#multi-delay-input-gain");
const multiDelayMix = document.querySelector("#multi-delay-mix");
const multiDelayLevel = document.querySelector("#multi-delay-level");
const multiDelaySeriesPanel = document.querySelector("#multi-delay-series-controls");
const multiDelaySeriesFeedback = document.querySelector("#multi-delay-series-feedback");
const multiDelaySeriesSend = document.querySelector("#multi-delay-series-send");
const multiDelaySeriesReturn = document.querySelector("#multi-delay-series-return");
const multiDelayDiffusorPanel = document.querySelector("#multi-delay-diffusor-controls");
const multiDelayDiffusorMasterTime = document.querySelector("#multi-delay-diffusor-master-time");
const multiDelayDiffusorMasterFeedback = document.querySelector("#multi-delay-diffusor-master-feedback");
const multiDelayMotorPanel = document.querySelector("#multi-delay-motor-controls");
const multiDelayMotorSpeed = document.querySelector("#multi-delay-motor-speed");
const multiDelayTopologyMasterHint = document.querySelector("#multi-delay-topology-master-hint");
const multiDelayTimeLimit = document.querySelector("#multi-delay-time-limit");
const multiDelayControls = [
  ...multiDelayLineControls.flatMap((line) => Object.values(line)),
  ...multiDelayLfoControls.flatMap((lfo) => Object.values(lfo)),
  multiDelayTopology, multiDelayTempo, multiDelayLowCut, multiDelayHighCut,
  multiDelayInputGain, multiDelayMix, multiDelayLevel,
  multiDelaySeriesFeedback, multiDelaySeriesSend, multiDelaySeriesReturn,
  multiDelayDiffusorMasterTime, multiDelayDiffusorMasterFeedback,
  multiDelayMotorSpeed,
];
const chorusRate = document.querySelector("#chorus-rate-hz");
const chorusRateValue = document.querySelector("#chorus-rate-value");
const chorusDelay = document.querySelector("#chorus-delay-ms");
const chorusDelayValue = document.querySelector("#chorus-delay-value");
const chorusDepth = document.querySelector("#chorus-depth-ms");
const chorusDepthValue = document.querySelector("#chorus-depth-value");
const chorusMix = document.querySelector("#chorus-mix");
const chorusMixValue = document.querySelector("#chorus-mix-value");
const dimensionChorusVersion = document.querySelector("#dimension-chorus-version");
const dimensionChorusMode = document.querySelector("#dimension-chorus-mode");
const dimensionChorusMix = document.querySelector("#dimension-chorus-mix");
const dimensionChorusMixValue = document.querySelector("#dimension-chorus-mix-value");
const dimensionChorusLevel = document.querySelector("#dimension-chorus-level-db");
const dimensionChorusLevelValue = document.querySelector("#dimension-chorus-level-value");
const tripleDoubleChorusPresetSwitch = document.querySelector("#triple-double-chorus-preset-switch");
const tripleDoubleChorusManualSwitch = document.querySelector("#triple-double-chorus-manual-switch");
const tripleDoubleChorusRate = document.querySelector("#triple-double-chorus-rate-hz");
const tripleDoubleChorusRateValue = document.querySelector("#triple-double-chorus-rate-hz-value");
const tripleDoubleChorusLeftDepth = document.querySelector("#triple-double-chorus-left-depth-ms");
const tripleDoubleChorusLeftDepthValue = document.querySelector("#triple-double-chorus-left-depth-ms-value");
const tripleDoubleChorusCenterDepth = document.querySelector("#triple-double-chorus-center-depth-ms");
const tripleDoubleChorusCenterDepthValue = document.querySelector("#triple-double-chorus-center-depth-ms-value");
const tripleDoubleChorusRightDepth = document.querySelector("#triple-double-chorus-right-depth-ms");
const tripleDoubleChorusRightDepthValue = document.querySelector("#triple-double-chorus-right-depth-ms-value");
const tripleDoubleChorusFeedback = document.querySelector("#triple-double-chorus-feedback");
const tripleDoubleChorusFeedbackValue = document.querySelector("#triple-double-chorus-feedback-value");
const tripleDoubleChorusSparkle = document.querySelector("#triple-double-chorus-sparkle");
const tripleDoubleChorusSparkleValue = document.querySelector("#triple-double-chorus-sparkle-value");
const tripleDoubleChorusStereoSwitch = document.querySelector("#triple-double-chorus-stereo-switch");
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
const fiveBandEqControls = Array.from({ length: 5 }, (_, index) => {
  const id = `five-band-eq-band-${index + 1}`;
  return {
    frequency: document.querySelector(`#${id}-frequency`),
    frequencyValue: document.querySelector(`#${id}-frequency-value`),
    gain: document.querySelector(`#${id}-gain`),
    gainValue: document.querySelector(`#${id}-gain-value`),
    q: document.querySelector(`#${id}-q`),
    qValue: document.querySelector(`#${id}-q-value`),
    shape: document.querySelector(`#${id}-shape`),
    slope: document.querySelector(`#${id}-slope`),
    slopeValue: document.querySelector(`#${id}-slope-value`),
  };
});
const fiveBandEqSolo = document.querySelector("#five-band-eq-solo-band");
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
const ringModType = document.querySelector("#ring-mod-type");
const ringModFrequency = document.querySelector("#ring-mod-frequency-hz");
const ringModFrequencyValue = document.querySelector("#ring-mod-frequency-hz-value");
const ringModMultiplier = document.querySelector("#ring-mod-multiplier");
const ringModMultiplierValue = document.querySelector("#ring-mod-multiplier-value");
const ringModHighCut = document.querySelector("#ring-mod-high-cut-hz");
const ringModHighCutValue = document.querySelector("#ring-mod-high-cut-hz-value");
const ringModMix = document.querySelector("#ring-mod-mix");
const ringModMixValue = document.querySelector("#ring-mod-mix-value");
const ringModPitchTracking = document.querySelector("#ring-mod-pitch-tracking");
const ringModTrackingMode = document.querySelector("#ring-mod-tracking-mode");
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
const ashevillePattrnBank = document.querySelector("#asheville-pattrn-bank");
const ashevillePattrnPattern = document.querySelector("#asheville-pattrn-pattern");
const ashevillePattrnDrive = document.querySelector("#asheville-pattrn-drive-db");
const ashevillePattrnDriveValue = document.querySelector("#asheville-pattrn-drive-db-value");
const ashevillePattrnOutput = document.querySelector("#asheville-pattrn-output-db");
const ashevillePattrnOutputValue = document.querySelector("#asheville-pattrn-output-db-value");
const ashevillePattrnMix = document.querySelector("#asheville-pattrn-mix");
const ashevillePattrnMixValue = document.querySelector("#asheville-pattrn-mix-value");
const ashevillePattrnRate = document.querySelector("#asheville-pattrn-rate-hz");
const ashevillePattrnRateValue = document.querySelector("#asheville-pattrn-rate-hz-value");
const ashevillePattrnQ = document.querySelector("#asheville-pattrn-resonance-q");
const ashevillePattrnQValue = document.querySelector("#asheville-pattrn-resonance-q-value");
const ashevillePattrnEnvelope = document.querySelector("#asheville-pattrn-envelope-shape");
const ashevillePattrnEnvelopeValue = document.querySelector("#asheville-pattrn-envelope-shape-value");
const ashevillePattrnSmoothing = document.querySelector("#asheville-pattrn-smoothing-ms");
const ashevillePattrnSmoothingValue = document.querySelector("#asheville-pattrn-smoothing-ms-value");
const ashevillePattrnBandLevelsRoot = document.querySelector("#asheville-pattrn-band-levels");
const ashevillePattrnBandLevels = Array.from({ length: 8 }, (_, index) => {
  const label = document.createElement("label");
  label.className = "slicer-step";
  const name = document.createElement("span");
  name.textContent = `Band ${index + 1}`;
  const output = document.createElement("output");
  const input = document.createElement("input");
  input.type = "range";
  input.min = "0";
  input.max = "1";
  input.step = "0.01";
  input.value = String(DEFAULT_PARAMETERS.ashevillePattrn[index + 2]);
  output.textContent = `${Math.round(Number(input.value) * 100)}%`;
  input.setAttribute("aria-label", `Asheville Pattrn band ${index + 1} level`);
  label.append(name, output, input);
  ashevillePattrnBandLevelsRoot.append(label);
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
const formantFilterMode = document.querySelector("#formant-filter-mode");
const formantFilterMorphControls = document.querySelector("#formant-filter-morph-controls");
const formantFilterStartVowel = document.querySelector("#formant-filter-start-vowel");
const formantFilterMiddleVowel = document.querySelector("#formant-filter-middle-vowel");
const formantFilterEndVowel = document.querySelector("#formant-filter-end-vowel");
const formantFilterControl = document.querySelector("#formant-filter-control-percent");
const formantFilterControlValue = document.querySelector("#formant-filter-control-percent-value");
const formantFilterQ = document.querySelector("#formant-filter-q");
const formantFilterQValue = document.querySelector("#formant-filter-q-value");
const formantFilterMix = document.querySelector("#formant-filter-mix");
const formantFilterMixValue = document.querySelector("#formant-filter-mix-value");
const formantFilterLevel = document.querySelector("#formant-filter-level-db");
const formantFilterLevelValue = document.querySelector("#formant-filter-level-db-value");
const formantFilterBalance = document.querySelector("#formant-filter-balance-percent");
const formantFilterBalanceValue = document.querySelector("#formant-filter-balance-percent-value");
const harmonicResonatorRoot = document.querySelector("#harmonic-resonator-root-frequency-hz");
const harmonicResonatorRootValue = document.querySelector("#harmonic-resonator-root-frequency-hz-value");
const harmonicResonatorDecay = document.querySelector("#harmonic-resonator-decay-seconds");
const harmonicResonatorDecayValue = document.querySelector("#harmonic-resonator-decay-seconds-value");
const harmonicResonatorBrightness = document.querySelector("#harmonic-resonator-brightness");
const harmonicResonatorBrightnessValue = document.querySelector("#harmonic-resonator-brightness-value");
const harmonicResonatorMix = document.querySelector("#harmonic-resonator-mix");
const harmonicResonatorMixValue = document.querySelector("#harmonic-resonator-mix-value");
const bitCrusherBitDepth = document.querySelector("#bit-crusher-bit-depth");
const bitCrusherBitDepthValue = document.querySelector("#bit-crusher-bit-depth-value");
const bitCrusherRateDivisor = document.querySelector("#bit-crusher-rate-divisor");
const bitCrusherRateDivisorValue = document.querySelector("#bit-crusher-rate-divisor-value");
const bitCrusherMix = document.querySelector("#bit-crusher-mix");
const bitCrusherMixValue = document.querySelector("#bit-crusher-mix-value");
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
const duckingDelayTimeMode = document.querySelector("#ducking-delay-time-mode");
const duckingDelayTempo = document.querySelector("#ducking-delay-tempo-bpm");
const duckingDelayTempoValue = document.querySelector("#ducking-delay-tempo-bpm-value");
const duckingDelayTapDivision = document.querySelector("#ducking-delay-tap-division");
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
const duckingDelaySpillover = document.querySelector("#ducking-delay-spillover");
const duckingDelayLowCut = document.querySelector("#ducking-delay-low-cut-hz");
const duckingDelayLowCutValue = document.querySelector("#ducking-delay-low-cut-hz-value");
const duckingDelayHighCut = document.querySelector("#ducking-delay-high-cut-hz");
const duckingDelayHighCutValue = document.querySelector("#ducking-delay-high-cut-hz-value");
const duckingDelayModRate = document.querySelector("#ducking-delay-mod-rate-hz");
const duckingDelayModRateValue = document.querySelector("#ducking-delay-mod-rate-hz-value");
const duckingDelayModDepth = document.querySelector("#ducking-delay-mod-depth-ms");
const duckingDelayModDepthValue = document.querySelector("#ducking-delay-mod-depth-ms-value");
const duckingDelayModMode = document.querySelector("#ducking-delay-mod-mode");
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
const filteredDelayTimeMode = document.querySelector("#filtered-delay-time-mode");
const filteredDelayTempoBpm = document.querySelector("#filtered-delay-tempo-bpm");
const filteredDelayTempoBpmValue = document.querySelector("#filtered-delay-tempo-bpm-value");
const filteredDelayDivision = document.querySelector("#filtered-delay-division");
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
const sweepDelayFilterType = document.querySelector("#sweep-delay-filter-type");
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
const sweepDelayRateMode = document.querySelector("#sweep-delay-rate-mode");
const sweepDelayTempoBpm = document.querySelector("#sweep-delay-tempo-bpm");
const sweepDelayTempoBpmValue = document.querySelector("#sweep-delay-tempo-bpm-value");
const sweepDelayDivision = document.querySelector("#sweep-delay-division");
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
let decodedConvolutionIR = null;
let convolutionIRVersion = 0;
let convolutionIRUploadedVersion = -1;
let convolutionIRRequestId = 0;
let decodedCabinetIR = null;
let cabinetIRVersion = 0;
let cabinetIRUploadedVersion = -1;
let cabinetIRRequestId = 0;
let cabinetIRLatencyFrames = 0;
let shufflingLooperStatusTimeout = 0;
const pendingConvolutionIRLoads = new Map();
const pendingCabinetIRLoads = new Map();

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

function processorChannelCount(inputChannels, enabled, parameters = DEFAULT_PARAMETERS) {
  const enhancerMode = Number(parameters?.enhancer?.[0] ?? 0);
  const modernEnhancer = inputChannels === 1 && enabled?.[61] && enhancerMode === 0;
  return modernEnhancer ? 2 : inputChannels;
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

function updateLooperOptions() {
  looperOverdubLevelValue.value = `${looperOverdubLevel.value}%`;
  looperAutoRecordThreshold.parentElement.querySelector("output").value =
    `${Number(looperAutoRecordThreshold.value)} dBFS`;
  if (workletNode) {
    workletNode.port.postMessage({
      type: "looper-options",
      reverse: looperReverse.checked,
      halfSpeed: looperHalfSpeed.checked,
      overdubLevel: Number(looperOverdubLevel.value) / 100,
      autoRecord: looperAutoRecord.checked,
      autoRecordThresholdDb: Number(looperAutoRecordThreshold.value),
    });
  }
}

function sendLooperCommand(command) {
  if (!workletNode) {
    looperStatus.value = "Load audio and start processed playback first";
    return;
  }
  workletNode.port.postMessage({ type: "looper-command", command });
}

function updateDeluxeLooperOptions() {
  const cutFrequency = (control) => {
    let hz = Number(control.value);
    if (hz > 0 && hz < 20) hz = 20;
    control.value = String(hz);
    control.parentElement.querySelector("output").value = hz === 0 ? "Off" : `${hz} Hz`;
    return hz;
  };
  deluxeLooperPlayback.parentElement.querySelector("output").value =
    `${Number(deluxeLooperPlayback.value)}%`;
  const decay = Number(deluxeLooperOverdubDecay.value);
  deluxeLooperOverdubDecay.parentElement.querySelector("output").value =
    `${decay} dB/pass`;
  const values = {
    reverse: deluxeLooperReverse.checked,
    halfSpeed: deluxeLooperHalfSpeed.checked,
    playbackLevel: Number(deluxeLooperPlayback.value) / 100,
    overdubDecayDb: decay,
    lowCutHz: cutFrequency(deluxeLooperLowCut),
    highCutHz: cutFrequency(deluxeLooperHighCut),
  };
  if (workletNode) {
    workletNode.port.postMessage({ type: "deluxe-looper-options", ...values });
  }
}

function sendDeluxeLooperCommand(command) {
  if (!workletNode) {
    deluxeLooperStatus.value = "Load audio and start processed playback first";
    return;
  }
  workletNode.port.postMessage({ type: "deluxe-looper-command", command });
}

let deluxeLooperStatusTimeout = 0;
function scheduleDeluxeLooperStatusPoll(state) {
  window.clearTimeout(deluxeLooperStatusTimeout);
  deluxeLooperStatusTimeout = 0;
  if (state === 0 || !workletNode) return;
  deluxeLooperStatusTimeout = window.setTimeout(() => {
    workletNode?.port.postMessage({ type: "deluxe-looper-status-request" });
  }, 250);
}

function renderDeluxeLooperStatus(message) {
  const labels = ["Stopped", "Recording", "Playing", "Overdubbing"];
  const state = Number(message.state) || 0;
  const frames = Number(message.frames) || 0;
  const sampleRate = Number(message.sampleRate) || 48000;
  const length = frames > 0 ? `${(frames / sampleRate).toFixed(2)} s` : "no loop";
  deluxeLooperStatus.value = `${labels[state] || "Unknown"} · ${length}${message.accepted === false ? " · command unavailable" : ""}`;
  scheduleDeluxeLooperStatusPoll(state);
}

function updateShufflingLooperOptions() {
  const cutFrequency = (control) => {
    let hz = Number(control.value);
    if (hz > 0 && hz < 20) hz = 20;
    control.value = String(hz);
    return hz;
  };
  const values = [
    Number(shufflingLooperSlices.value),
    Number(shufflingLooperSequenceLength.value),
    Number(shufflingLooperShuffle.value) / 100,
    Number(shufflingLooperPitchProbability.value) / 100,
    Number(shufflingLooperReverseProbability.value) / 100,
    Number(shufflingLooperRepeatProbability.value) / 100,
    Number(shufflingLooperSmoothing.value) / 100,
    Number(shufflingLooperSequenceDrift.value) / 100,
    Number(shufflingLooperPlayback.value) / 100,
    cutFrequency(shufflingLooperLowCut),
    cutFrequency(shufflingLooperHighCut),
    Number(shufflingLooperInterval1.value),
    Number(shufflingLooperInterval2.value),
  ];
  DEFAULT_PARAMETERS.shufflingLooper = values;
  shufflingLooperSlices.parentElement.querySelector("output").value =
    `${values[0]} slices`;
  shufflingLooperSequenceLength.parentElement.querySelector("output").value =
    `${values[1]} steps`;
  for (const control of [shufflingLooperShuffle, shufflingLooperPitchProbability,
    shufflingLooperReverseProbability, shufflingLooperRepeatProbability,
    shufflingLooperSmoothing, shufflingLooperSequenceDrift,
    shufflingLooperPlayback]) {
    control.parentElement.querySelector("output").value = `${control.value}%`;
  }
  shufflingLooperLowCut.parentElement.querySelector("output").value =
    values[9] === 0 ? "Off" : `${values[9]} Hz`;
  shufflingLooperHighCut.parentElement.querySelector("output").value =
    values[10] === 0 ? "Off" : `${values[10]} Hz`;
  shufflingLooperInterval1.parentElement.querySelector("output").value =
    `${values[11] > 0 ? "+" : ""}${values[11]} semitones`;
  shufflingLooperInterval2.parentElement.querySelector("output").value =
    `${values[12] > 0 ? "+" : ""}${values[12]} semitones`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "shuffling-looper-options", value: values });
  }
}

function updateTenTapDelayOptions() {
  const values = [
    Number(tenTapDelayTapCount.value),
    Number(tenTapDelayTime.value),
    Number(tenTapDelayTempo.value),
    Number(tenTapDelayDivision.value),
    Number(tenTapDelayDecay.value),
    Number(tenTapDelayShuffle.value),
    Number(tenTapDelaySpread.value),
    Number(tenTapDelayRatio.value),
    Number(tenTapDelayPanShape.value),
    Number(tenTapDelayPanAlpha.value),
    Number(tenTapDelayLowCut.value),
    Number(tenTapDelayHighCut.value),
    Number(tenTapDelayMix.value) / 100,
    Number(tenTapDelayLevel.value),
    tenTapDelayMono.checked ? 1 : 0,
    ...tenTapDelayTapLevels.map((control) => Number(control.value)),
  ];
  DEFAULT_PARAMETERS.tenTapDelay = values;
  const output = (control, value) => {
    control.parentElement.querySelector("output").value = value;
  };
  output(tenTapDelayTapCount, `${values[0]} taps`);
  output(tenTapDelayTime, `${values[1]} ms`);
  output(tenTapDelayTempo, `${values[2]} BPM`);
  output(tenTapDelayDecay, `${values[4]}%`);
  output(tenTapDelayShuffle, `${values[5]}%`);
  output(tenTapDelaySpread, `${values[6]}%`);
  output(tenTapDelayRatio, `${values[7]}% left/right`);
  output(tenTapDelayPanAlpha, `${values[9]}%`);
  output(tenTapDelayLowCut, values[10] === 0 ? "Off" : `${values[10]} Hz`);
  output(tenTapDelayHighCut, `${values[11]} Hz`);
  output(tenTapDelayMix, `${Math.round(values[12] * 100)}%`);
  output(tenTapDelayLevel, `${values[13] > 0 ? "+" : ""}${values[13]} dB`);
  tenTapDelayTapLevels.forEach((control, index) => {
    output(control, `${values[index + 15] > 0 ? "+" : ""}${values[index + 15]} dB`);
  });
  if (workletNode) {
    workletNode.port.postMessage({ type: "ten-tap-delay-options", value: values });
  }
}

function updateRhythmTapDelayOptions() {
  const quantize = Number(rhythmTapQuantize.value);
  const maximumTapValue = quantize === 0 ? 2000 : 16;
  for (const control of rhythmTapTimes) {
    control.max = String(maximumTapValue);
    if (Number(control.value) > maximumTapValue) {
      control.value = String(maximumTapValue);
    }
  }
  const intervalValues = rhythmTapTimes.map((control) => Number(control.value));
  const tapLevels = rhythmTapLevels.map((control) => Number(control.value));
  const values = [
    Number(rhythmTapRepeatCount.value), Number(rhythmTapTempo.value), quantize,
    Number(rhythmTapFeedback.value), Number(rhythmTapDecay.value),
    Number(rhythmTapSpread.value), Number(rhythmTapPanShape.value),
    Number(rhythmTapPanAlpha.value), Number(rhythmTapLowCut.value),
    Number(rhythmTapHighCut.value), Number(rhythmTapMix.value) / 100,
    Number(rhythmTapLevel.value), rhythmTapMono.checked ? 1 : 0,
    ...intervalValues, ...tapLevels,
  ];
  DEFAULT_PARAMETERS.rhythmTapDelay = values;
  const show = (control, text) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = text;
  };
  show(rhythmTapRepeatCount, `${values[0]} repeats`);
  show(rhythmTapTempo, `${values[1]} BPM`);
  show(rhythmTapQuantize,
    rhythmTapQuantize.selectedOptions[0]?.textContent || "None");
  show(rhythmTapFeedback, `${values[3]}%`);
  show(rhythmTapDecay, `${values[4]}%`);
  show(rhythmTapSpread, `${values[5]}%`);
  show(rhythmTapPanAlpha, `${values[7]}%`);
  show(rhythmTapLowCut, values[8] === 0 ? "Off" : `${values[8]} Hz`);
  show(rhythmTapHighCut, `${values[9]} Hz`);
  show(rhythmTapMix, `${Math.round(values[10] * 100)}%`);
  show(rhythmTapLevel,
    `${values[11] > 0 ? "+" : ""}${values[11]} dB`);
  rhythmTapTempo.disabled = quantize === 0;
  for (const [index, control] of rhythmTapTimes.entries()) {
    show(control, quantize === 0
      ? `${intervalValues[index]} ms`
      : `${intervalValues[index]} div`);
  }
  for (const control of rhythmTapLevels) {
    const value = Number(control.value);
    show(control, `${value > 0 ? "+" : ""}${value} dB`);
  }
  if (workletNode) {
    workletNode.port.postMessage({ type: "rhythm-tap-delay", value: values });
  }
}

function updatePlexDelayOptions() {
  const control = (name) => plexDelayGlobalControls.find(
    (item) => item.dataset.plexGlobal === name);
  const value = (name) => {
    const item = control(name);
    return item.type === "checkbox" ? item.checked : Number(item.value);
  };
  const parameters = {
    activeLines: value("activeLines"),
    inputSelect: value("inputSelect"),
    tempoBpm: value("tempoBpm"),
    masterTime: value("masterTime"),
    masterLevel: value("masterLevel") / 100,
    masterPan: value("masterPan") / 100,
    decaySeconds: value("decaySeconds"),
    diffusion: value("diffusion") / 100,
    inputDiffusionMix: value("inputDiffusionMix") / 100,
    inputDiffusionTimeMs: value("inputDiffusionTimeMs"),
    lowCutHz: value("lowCutHz"),
    highCutHz: value("highCutHz"),
    preDelayMs: value("preDelayMs"),
    stackMode: value("stackMode"),
    inputGainDb: value("inputGainDb"),
    mix: value("mix") / 100,
    killDry: value("killDry"),
    levelDb: value("levelDb"),
    balance: value("balance") / 100,
    wetFilterType: value("wetFilterType"),
    wetFilterFrequencyHz: value("wetFilterFrequencyHz"),
    wetFilterQ: value("wetFilterQ"),
    wetFilterGainDb: value("wetFilterGainDb"),
    filterLfoEnabled: value("filterLfoEnabled"),
    filterLfoType: value("filterLfoType"),
    filterLfoRateHz: value("filterLfoRateHz"),
    filterModFrequencyHz: value("filterModFrequencyHz"),
    envelopeEnabled: value("envelopeEnabled"),
    envelopeThresholdDb: value("envelopeThresholdDb"),
    envelopeAttackMs: value("envelopeAttackMs"),
    envelopeReleaseMs: value("envelopeReleaseMs"),
    duckerAttenuationDb: value("duckerAttenuationDb"),
    duckerThresholdDb: value("duckerThresholdDb"),
    duckerReleaseMs: value("duckerReleaseMs"),
    stereoSpread: value("stereoSpread") / 100,
    lines: plexDelayLines.map((line) => {
      const read = (name) => Number(line.querySelector(
        `[data-plex-line-param="${name}"]`).value);
      const timeMode = read("timeMode");
      const timeControl = line.querySelector('[data-plex-line-param="timeMs"]');
      const divisionControl = line.querySelector('[data-plex-line-param="division"]');
      timeControl.disabled = timeMode === 1;
      divisionControl.disabled = timeMode !== 1;
      const timeMs = Number(timeControl.value);
      const level = read("level") / 100;
      const pan = read("pan") / 100;
      line.querySelector('[data-plex-line-output="timeMs"]').value = `${timeMs} ms`;
      line.querySelector('[data-plex-line-output="level"]').value = `${Math.round(level * 100)}%`;
      line.querySelector('[data-plex-line-output="pan"]').value =
        pan === 0 ? "Center" : `${pan < 0 ? "−" : "+"}${Math.round(Math.abs(pan) * 100)}%`;
      return {
        timeMs,
        timeMode,
        division: read("division"),
        level,
        pan,
      };
    }),
  };
  const labels = {
    activeLines: `${parameters.activeLines} lines`,
    inputSelect: ["L+R (default)", "Left", "Right"][parameters.inputSelect] ??
      "L+R (default)",
    tempoBpm: `${parameters.tempoBpm} BPM`,
    masterTime: `${parameters.masterTime.toFixed(2)}×`,
    masterLevel: `${Math.round(parameters.masterLevel * 100)}%`,
    masterPan: `${Math.round(parameters.masterPan * 100)}%`,
    decaySeconds: `${parameters.decaySeconds.toFixed(1)} s`,
    diffusion: `${Math.round(parameters.diffusion * 100)}%`,
    inputDiffusionMix: `${Math.round(parameters.inputDiffusionMix * 100)}%`,
    inputDiffusionTimeMs: `${parameters.inputDiffusionTimeMs.toFixed(1)} ms`,
    lowCutHz: parameters.lowCutHz === 0 ? "Off" : `${parameters.lowCutHz} Hz`,
    highCutHz: `${(parameters.highCutHz / 1000).toFixed(parameters.highCutHz % 1000 === 0 ? 0 : 1)} kHz`,
    preDelayMs: `${parameters.preDelayMs} ms`,
    inputGainDb: `${parameters.inputGainDb.toFixed(1)} dB`,
    mix: `${Math.round(parameters.mix * 100)}%`,
    killDry: parameters.killDry ? "On" : "Off",
    levelDb: `${parameters.levelDb.toFixed(1)} dB`,
    balance: parameters.balance === 0 ? "Center" :
      `${parameters.balance < 0 ? "Left" : "Right"} ${Math.round(Math.abs(parameters.balance) * 100)}%`,
    wetFilterFrequencyHz: `${parameters.wetFilterFrequencyHz} Hz`,
    wetFilterQ: parameters.wetFilterQ.toFixed(2),
    wetFilterGainDb: `${parameters.wetFilterGainDb.toFixed(1)} dB`,
    filterLfoRateHz: `${parameters.filterLfoRateHz.toFixed(2)} Hz`,
    filterModFrequencyHz: `${parameters.filterModFrequencyHz} Hz`,
    envelopeThresholdDb: `${parameters.envelopeThresholdDb} dB`,
    envelopeAttackMs: `${parameters.envelopeAttackMs} ms`,
    envelopeReleaseMs: `${parameters.envelopeReleaseMs} ms`,
    duckerAttenuationDb: parameters.duckerAttenuationDb === 0
      ? "0 dB (off)" : `${parameters.duckerAttenuationDb} dB`,
    duckerThresholdDb: `${parameters.duckerThresholdDb} dB`,
    duckerReleaseMs: `${parameters.duckerReleaseMs} ms`,
    stereoSpread: `${Math.round(parameters.stereoSpread * 100)}%`,
  };
  for (const [name, label] of Object.entries(labels)) {
    const output = document.querySelector(`[data-plex-output="${name}"]`);
    if (output) output.value = label;
  }
  for (const name of ["filterLfoEnabled", "envelopeEnabled", "killDry"]) {
    const item = control(name);
    const output = item.parentElement.querySelector("output");
    if (output) output.value = item.checked ? "On" : "Off";
  }
  for (const name of ["filterLfoType", "filterLfoRateHz", "filterModFrequencyHz"]) {
    control(name).disabled = !parameters.filterLfoEnabled;
  }
  for (const name of ["envelopeThresholdDb", "envelopeAttackMs", "envelopeReleaseMs"]) {
    control(name).disabled = !parameters.envelopeEnabled;
  }
  DEFAULT_PARAMETERS.plexDelay = parameters;
  if (workletNode) {
    workletNode.port.postMessage({ type: "plex-delay", value: parameters });
  }
}

function rhythmTapDivisionLengthMs() {
  const beats = [0, 4, 2, 3, 4 / 3, 1, 1.5, 2 / 3,
    0.5, 0.75, 1 / 3, 0.25, 0.375, 1 / 6, 0.125, 1 / 12];
  const division = Number(rhythmTapQuantize.value);
  if (division === 0) return 1;
  return (60000 / Number(rhythmTapTempo.value)) * beats[division];
}

function beginRhythmTapLearning() {
  rhythmTapLearning = true;
  rhythmTapLastTapAt = null;
  rhythmTapCapturedIntervals = 0;
  rhythmTapLearnStart.disabled = true;
  rhythmTapLearnTap.disabled = false;
  rhythmTapLearnDone.disabled = false;
  rhythmTapLearnStatus.value =
    "Tap the first beat (dry signal), then each repeat. Up to 10 intervals.";
}

function captureRhythmTapLearningBeat() {
  if (!rhythmTapLearning) return;
  const now = performance.now();
  if (rhythmTapLastTapAt === null) {
    rhythmTapLastTapAt = now;
    rhythmTapLearnStatus.value = "Dry beat recorded · tap the first repeat";
    return;
  }
  if (rhythmTapCapturedIntervals >= rhythmTapTimes.length) {
    rhythmTapLearnStatus.value = "Maximum of 10 repeat intervals reached";
    return;
  }
  const quantize = Number(rhythmTapQuantize.value);
  const elapsedMs = Math.max(1, now - rhythmTapLastTapAt);
  const interval = quantize === 0
    ? Math.round(elapsedMs)
    : Math.max(1, Math.round(elapsedMs / rhythmTapDivisionLengthMs()));
  rhythmTapTimes[rhythmTapCapturedIntervals].value = String(interval);
  rhythmTapCapturedIntervals += 1;
  rhythmTapLastTapAt = now;
  rhythmTapRepeatCount.value = String(rhythmTapCapturedIntervals);
  updateRhythmTapDelayOptions();
  rhythmTapLearnStatus.value =
    `Captured ${rhythmTapCapturedIntervals} interval${rhythmTapCapturedIntervals === 1 ? "" : "s"}`;
}

function finishRhythmTapLearning() {
  rhythmTapLearning = false;
  rhythmTapLearnStart.disabled = false;
  rhythmTapLearnTap.disabled = true;
  rhythmTapLearnDone.disabled = true;
  if (rhythmTapCapturedIntervals > 0) {
    rhythmTapRepeatCount.value = String(rhythmTapCapturedIntervals);
    updateRhythmTapDelayOptions();
    rhythmTapLearnStatus.value =
      `Learned ${rhythmTapCapturedIntervals} repeat interval${rhythmTapCapturedIntervals === 1 ? "" : "s"}`;
  } else {
    rhythmTapLearnStatus.value = "No repeat intervals captured";
  }
}

function updateAutoPanOptions() {
  const values = [
    Number(autoPanRate.value),
    Number(autoPanDepth.value) / 100,
    Number(autoPanWaveform.value),
    autoPanTempoSync.checked ? 1 : 0,
    Number(autoPanTempo.value),
    Number(autoPanDivision.value),
  ];
  DEFAULT_PARAMETERS.autoPan = values;
  autoPanRate.parentElement.querySelector("output").value = `${values[0].toFixed(2)} Hz`;
  autoPanDepth.parentElement.querySelector("output").value = `${Math.round(values[1] * 100)}%`;
  autoPanTempo.parentElement.querySelector("output").value = `${values[4]} BPM`;
  autoPanTempo.disabled = values[3] === 0;
  autoPanDivision.disabled = values[3] === 0;
  if (workletNode) {
    workletNode.port.postMessage({ type: "auto-pan", value: values });
  }
}

function updateStereoPannerOptions() {
  const values = [
    Number(stereoPannerControls[0].value),
    Number(stereoPannerControls[1].value),
    Number(stereoPannerControls[2].value),
    stereoPannerControls[3].checked ? 1 : 0,
    Number(stereoPannerControls[4].value),
    Number(stereoPannerControls[5].value),
    Number(stereoPannerControls[6].value),
    Number(stereoPannerControls[7].value),
    Number(stereoPannerControls[8].value),
    Number(stereoPannerControls[9].value),
    Number(stereoPannerControls[10].value),
    Number(stereoPannerControls[11].value),
    Number(stereoPannerControls[12].value),
    Number(stereoPannerControls[13].value),
  ];
  DEFAULT_PARAMETERS.stereoPanner = values;
  stereoPannerControls[0].parentElement.querySelector("output").value =
    `${values[0].toFixed(2)} Hz`;
  for (const index of [1, 6, 7, 9, 10, 12]) {
    stereoPannerControls[index].parentElement.querySelector("output").value =
      `${Math.round(values[index])}%`;
  }
  stereoPannerControls[4].parentElement.querySelector("output").value =
    `${values[4]} BPM`;
  stereoPannerControls[8].parentElement.querySelector("output").value =
    `${Math.round(values[8])}°`;
  stereoPannerControls[11].parentElement.querySelector("output").value =
    `${values[11].toFixed(1)} dB`;
  stereoPannerControls[13].parentElement.querySelector("output").value =
    `${Math.round(values[13])} ms`;
  stereoPannerControls[4].disabled = values[3] === 0;
  stereoPannerControls[5].disabled = values[3] === 0;
  if (workletNode) {
    workletNode.port.postMessage({ type: "stereo-panner", value: values });
  }
}

function updateMegatapDelayOptions() {
  const tapCount = Number(megatapDelayTapCount.value);
  megatapDelayFeedbackTap.max = String(tapCount);
  if (Number(megatapDelayFeedbackTap.value) > tapCount) {
    megatapDelayFeedbackTap.value = String(tapCount);
  }
  const values = [
    Number(megatapDelayTime.value), tapCount,
    Number(megatapDelayTimeAlpha.value),
    Number(megatapDelayTimeRandomize.value),
    Number(megatapDelayAmplitudeShape.value),
    Number(megatapDelayAmplitudeAlpha.value),
    Number(megatapDelayAmplitudeRandomize.value),
    Number(megatapDelayPanShape.value),
    Number(megatapDelayPanAlpha.value),
    Number(megatapDelayStereoSpread.value),
    Number(megatapDelayPredelay.value),
    Number(megatapDelayFeedback.value) / 100,
    Number(megatapDelayFeedbackTap.value),
    Number(megatapDelayLowCut.value), Number(megatapDelayHighCut.value),
    Number(megatapDelayDiffusionMix.value) / 100,
    Number(megatapDelayDiffusionTime.value),
    Number(megatapDelayDiffusionRate.value),
    Number(megatapDelayDiffusionDepth.value),
    Number(megatapDelayEnvelopeThreshold.value),
    Number(megatapDelayEnvelopeAttack.value),
    Number(megatapDelayEnvelopeRelease.value),
    Number(megatapDelayInputGain.value), Number(megatapDelayMix.value) / 100,
    Number(megatapDelayLevel.value), Number(megatapDelayInputSelect.value),
    megatapDelayEnvelopeEnabled.checked ? 1 : 0,
  ];
  DEFAULT_PARAMETERS.megatapDelay = values;
  const show = (control, text) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = text;
  };
  show(megatapDelayTime, `${values[0]} ms`);
  show(megatapDelayTapCount, `${values[1]} taps`);
  show(megatapDelayTimeAlpha, values[2].toFixed(2));
  show(megatapDelayTimeRandomize, `${values[3]}%`);
  show(megatapDelayAmplitudeShape,
    megatapDelayAmplitudeShape.selectedOptions[0].textContent);
  show(megatapDelayAmplitudeAlpha, values[5].toFixed(2));
  show(megatapDelayAmplitudeRandomize, `${values[6]}%`);
  show(megatapDelayPanShape, megatapDelayPanShape.selectedOptions[0].textContent);
  show(megatapDelayPanAlpha, `${values[8]}%`);
  show(megatapDelayStereoSpread, `${values[9]}%`);
  show(megatapDelayPredelay, `${values[10]} ms`);
  show(megatapDelayFeedback, `${Math.round(values[11] * 100)}%`);
  show(megatapDelayFeedbackTap, `Tap ${values[12]}`);
  show(megatapDelayLowCut, values[13] === 0 ? "Off" : `${values[13]} Hz`);
  show(megatapDelayHighCut, `${values[14]} Hz`);
  show(megatapDelayDiffusionMix, `${Math.round(values[15] * 100)}%`);
  show(megatapDelayDiffusionTime, `${values[16]} ms`);
  show(megatapDelayDiffusionRate, `${values[17].toFixed(2)} Hz`);
  show(megatapDelayDiffusionDepth, `${values[18].toFixed(2)} ms`);
  show(megatapDelayEnvelopeThreshold, `${values[19]} dB`);
  show(megatapDelayEnvelopeAttack, `${values[20]} ms`);
  show(megatapDelayEnvelopeRelease, `${values[21]} ms`);
  show(megatapDelayInputGain, `${values[22].toFixed(1)} dB`);
  show(megatapDelayMix, `${Math.round(values[23] * 100)}%`);
  show(megatapDelayLevel, `${values[24].toFixed(1)} dB`);
  show(megatapDelayInputSelect,
    megatapDelayInputSelect.selectedOptions[0].textContent);
  if (workletNode) {
    workletNode.port.postMessage({ type: "megatap-delay", value: values });
  }
}

function updateDeEsserOptions() {
  const values = [
    Number(deEsserFrequency.value), Number(deEsserQ.value),
    Number(deEsserThreshold.value), Number(deEsserReduction.value),
    Number(deEsserAttack.value), Number(deEsserRelease.value),
  ];
  DEFAULT_PARAMETERS.deEsser = values;
  const show = (control, text) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = text;
  };
  show(deEsserFrequency, `${values[0]} Hz`);
  show(deEsserQ, values[1].toFixed(2));
  show(deEsserThreshold, `${values[2]} dBFS`);
  show(deEsserReduction, `${values[3]} dB`);
  show(deEsserAttack, `${values[4]} ms`);
  show(deEsserRelease, `${values[5]} ms`);
  if (workletNode) {
    workletNode.port.postMessage({ type: "de-esser", value: values });
  }
}

function updateDualEngineDistortionOptions() {
  const values = [
    Number(dualDistortionGainA.value), Number(dualDistortionToneA.value),
    Number(dualDistortionGainB.value), Number(dualDistortionToneB.value),
    Number(dualDistortionBlend.value), Number(dualDistortionMode.value),
    dualDistortionBassBoost.checked ? 1 : 0,
    Number(dualDistortionLevel.value),
  ];
  DEFAULT_PARAMETERS.dualEngineDistortion = values;
  const show = (control, text) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = text;
  };
  show(dualDistortionGainA, values[0].toFixed(2));
  show(dualDistortionToneA, values[1].toFixed(2));
  show(dualDistortionGainB, values[2].toFixed(2));
  show(dualDistortionToneB, values[3].toFixed(2));
  show(dualDistortionBlend, `${Math.round(values[4] * 100)}% B`);
  show(dualDistortionMode, values[5] === 1 ? "Series" : "Blend");
  show(dualDistortionBassBoost, dualDistortionBassBoost.checked ? "On" : "Off");
  show(dualDistortionLevel, `${values[7]} dB`);
  if (workletNode) {
    workletNode.port.postMessage({ type: "dual-engine-distortion", value: values });
  }
}

function updateVibeOptions() {
  const rateMode = Number(vibeRateMode.value);
  const noteDivision = Number(vibeRateDivision.value);
  const tempoBpm = Number(vibeTempoBpm.value);
  const cycleBeats = [4, 3, 2, 1.5, 1, 2 / 3, 0.75, 0.5, 1 / 3,
    0.375, 0.25, 1 / 6][noteDivision] ?? 1;
  const effectiveRateHz = rateMode === 1
    ? Math.min(30, Math.max(0.05, tempoBpm / (60 * cycleBeats)))
    : Number(vibeRate.value);
  const values = [
    Number(vibeRate.value), Number(vibeDepth.value) / 100,
    Number(vibeTrem.value) / 100, Number(vibeStages.value),
    Number(vibeSpread.value) / 100, Number(vibeLevel.value),
    rateMode, noteDivision, tempoBpm,
  ];
  DEFAULT_PARAMETERS.vibe = values;
  const show = (control, text) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = text;
  };
  show(vibeRate, rateMode === 1
    ? `${vibeRateDivision.selectedOptions[0].text} · ${tempoBpm} BPM · ${effectiveRateHz.toFixed(2)} Hz`
    : `${values[0].toFixed(1)} Hz`);
  show(vibeDepth, `${Math.round(values[1] * 100)}%`);
  show(vibeTrem, `${Math.round(values[2] * 100)}%`);
  show(vibeStages, String(values[3]));
  show(vibeSpread, `${Math.round(values[4] * 100)}%`);
  show(vibeLevel, values[5].toFixed(2));
  show(vibeRateMode, rateMode === 1 ? "Tempo note" : "Free Hz");
  show(vibeRateDivision, vibeRateDivision.selectedOptions[0].text);
  show(vibeTempoBpm, `${tempoBpm} BPM`);
  vibeRate.disabled = rateMode === 1;
  vibeRateDivision.disabled = rateMode !== 1;
  vibeTempoBpm.disabled = rateMode !== 1;
  if (workletNode) workletNode.port.postMessage({ type: "vibe", value: values });
}

function updateUniVerseVibeOptions() {
  const rateMode = Number(uniVerseVibeRateMode.value);
  const noteDivision = Number(uniVerseVibeRateDivision.value);
  const tempoBpm = Number(uniVerseVibeTempoBpm.value);
  const cycleBeats = [4, 3, 2, 1.5, 1, 2 / 3, 0.75, 0.5, 1 / 3,
    0.375, 0.25, 1 / 6][noteDivision] ?? 1;
  const effectiveRateHz = rateMode === 1
    ? Math.min(30, Math.max(0.05, tempoBpm / (60 * cycleBeats)))
    : Number(uniVerseVibeSpeed.value);
  const values = [
    Number(uniVerseVibeSpeed.value),
    Number(uniVerseVibeIntensity.value) / 100,
    Number(uniVerseVibeVolume.value),
    rateMode, noteDivision, tempoBpm,
    Number(uniVerseVibeToneMode.value),
    Number(uniVerseVibeModulationMode.value),
  ];
  DEFAULT_PARAMETERS.uniVerseVibe = values;
  const show = (control, text) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = text;
  };
  show(uniVerseVibeSpeed, rateMode === 1
    ? `${uniVerseVibeRateDivision.selectedOptions[0].text} · ${tempoBpm} BPM · ${effectiveRateHz.toFixed(2)} Hz`
    : `${values[0].toFixed(1)} Hz`);
  show(uniVerseVibeIntensity, `${Math.round(values[1] * 100)}%`);
  show(uniVerseVibeVolume, values[2].toFixed(2));
  show(uniVerseVibeRateMode, rateMode === 1 ? "Tempo note" : "Free Hz");
  show(uniVerseVibeRateDivision, uniVerseVibeRateDivision.selectedOptions[0].text);
  show(uniVerseVibeTempoBpm, `${tempoBpm} BPM`);
  show(uniVerseVibeToneMode, uniVerseVibeToneMode.selectedOptions[0].text);
  show(uniVerseVibeModulationMode,
    uniVerseVibeModulationMode.selectedOptions[0].text);
  uniVerseVibeSpeed.disabled = rateMode === 1;
  uniVerseVibeRateDivision.disabled = rateMode !== 1;
  uniVerseVibeTempoBpm.disabled = rateMode !== 1;
  if (workletNode) {
    workletNode.port.postMessage({ type: "uni-verse-vibe", value: values });
  }
}

function updatePortaPreampOptions() {
  const values = portaPreampControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.portaPreamp = values;
  for (let index = 0; index < portaPreampControls.length; index += 1) {
    const output = portaPreampControls[index].parentElement.querySelector("output");
    if (output) output.value = `${values[index].toFixed(1)} dB`;
  }
  if (workletNode) {
    workletNode.port.postMessage({ type: "porta-preamp", value: values });
  }
}

function updatePitchSequenceOptions() {
  const values = [
    ...pitchSequenceSteps.map((step) => Number(step.semitones.value)),
    ...pitchSequenceSteps.map((step) => Number(step.mode.value)),
    Number(pitchSequenceStepsCount.value), pitchSequenceRun.checked ? 1 : 0,
    Number(pitchSequenceTimeMode.value), Number(pitchSequenceDivision.value),
    Number(pitchSequenceFreeMs.value), Number(pitchSequenceTempo.value),
    Number(pitchSequenceGlide.value), Number(pitchSequenceGlitchDepth.value),
    Number(pitchSequenceGlitchInterval.value),
    Number(pitchSequenceFreezeWindow.value), Number(pitchSequenceMix.value) / 100,
    Number(pitchSequenceLevel.value) / 100,
  ];
  DEFAULT_PARAMETERS.pitchSequence = values;
  for (const [index, step] of pitchSequenceSteps.entries()) {
    const semitones = values[index];
    const semitonesOutput = step.semitones.parentElement.querySelector("output");
    if (semitonesOutput) {
      semitonesOutput.value = `${semitones > 0 ? "+" : ""}${semitones} semitones`;
    }
    const modeOutput = step.mode.parentElement.querySelector("output");
    if (modeOutput) {
      modeOutput.value = ["Pitch", "Glitch", "Freeze"][values[16 + index]] || "Pitch";
    }
  }
  const show = (control, value) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = value;
  };
  show(pitchSequenceStepsCount, String(values[32]));
  show(pitchSequenceRun, pitchSequenceRun.checked ? "On" : "Off");
  show(pitchSequenceTimeMode, values[34] === 1 ? "Tempo" : "Free");
  show(pitchSequenceDivision,
    ["Whole", "Dotted half", "Half", "Dotted quarter", "Quarter",
      "Quarter triplet", "Dotted eighth", "Eighth", "Eighth triplet",
      "Dotted sixteenth", "Sixteenth", "Sixteenth triplet"][values[35]] || "Quarter");
  show(pitchSequenceFreeMs, `${values[36]} ms`);
  show(pitchSequenceTempo, `${values[37]} BPM`);
  show(pitchSequenceGlide, `${values[38]} ms`);
  show(pitchSequenceGlitchDepth, `±${values[39]} semitones`);
  show(pitchSequenceGlitchInterval, `${values[40]} ms`);
  show(pitchSequenceFreezeWindow, `${values[41]} ms`);
  show(pitchSequenceMix, `${Math.round(values[42] * 100)}%`);
  show(pitchSequenceLevel, `${Math.round(values[43] * 100)}%`);
  if (workletNode) {
    workletNode.port.postMessage({ type: "pitch-sequence", value: values });
  }
}

function updatePolySustainOptions() {
  const values = [
    Number(polySustainInterval.value),
    Number(polySustainAttack.value),
    Number(polySustainDecay.value),
    Number(polySustainFxLevel.value) / 100,
    Number(polySustainLevel.value),
    Number(polySustainOperation.value),
  ];
  DEFAULT_PARAMETERS.polySustain = values;
  const show = (control, value) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = value;
  };
  const interval = values[0];
  show(polySustainInterval, `${interval > 0 ? "+" : ""}${interval} semitones`);
  show(polySustainAttack, `${values[1]} ms`);
  show(polySustainDecay, `${values[2]} ms`);
  show(polySustainFxLevel, `${Math.round(values[3] * 100)}%`);
  show(polySustainLevel, `${values[4].toFixed(1)} dB`);
  show(polySustainOperation, ["Normal", "Dry Kill", "Mute All"][values[5]] || "Normal");
  if (workletNode) {
    workletNode.port.postMessage({ type: "poly-sustain", value: values });
  }
}

function updateStepFilterDelayOptions() {
  const values = [
    Number(stepFilterDelayTimeMs.value),
    Number(stepFilterDelayTimeMode.value),
    Number(stepFilterDelayDivision.value),
    Number(stepFilterDelayStepMode.value),
    Number(stepFilterDelayTempo.value),
    Number(stepFilterDelayStepDivision.value),
    Number(stepFilterDelayStepRate.value),
    Number(stepFilterDelayFeedback.value) / 100,
    Number(stepFilterDelayMix.value) / 100,
    Number(stepFilterDelayFilterMode.value),
    Number(stepFilterDelayMinimumCutoff.value),
    Number(stepFilterDelayMaximumCutoff.value),
    Number(stepFilterDelayResonanceQ.value),
    Number(stepFilterDelayDepth.value) / 100,
    Number(stepFilterDelayShape.value) / 100,
    Number(stepFilterDelaySmoothing.value),
    Number(stepFilterDelayPhase.value) / 100,
    ...stepFilterDelaySteps.map((control) => Number(control.value) / 100),
  ];
  DEFAULT_PARAMETERS.stepFilterDelay = values;
  const show = (control, value) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = value;
  };
  const divisions = [
    "Whole", "Dotted half", "Half", "Dotted quarter", "Quarter",
    "Quarter triplet", "Dotted eighth", "Eighth", "Eighth triplet",
    "Dotted sixteenth", "Sixteenth", "Sixteenth triplet", "Thirty-second",
    "Sixty-fourth",
  ];
  const modes = ["Low-pass", "Band-pass", "High-pass", "Notch"];
  show(stepFilterDelayTimeMode, values[1] ? "Tempo sync" : "Free");
  show(stepFilterDelayTimeMs, `${values[0]} ms`);
  show(stepFilterDelayDivision, divisions[values[2]] || "Quarter");
  show(stepFilterDelayStepMode, values[3] ? "Tempo sync" : "Free rate");
  show(stepFilterDelayStepDivision, divisions[values[5]] || "Sixteenth");
  show(stepFilterDelayTempo, `${values[4]} BPM`);
  show(stepFilterDelayStepRate, `${values[6].toFixed(2)} Hz`);
  show(stepFilterDelayFeedback, `${Math.round(values[7] * 100)}%`);
  show(stepFilterDelayMix, `${Math.round(values[8] * 100)}%`);
  show(stepFilterDelayFilterMode, modes[values[9]] || "Low-pass");
  show(stepFilterDelayMinimumCutoff, `${values[10].toLocaleString("en-US")} Hz`);
  show(stepFilterDelayMaximumCutoff, `${values[11].toLocaleString("en-US")} Hz`);
  show(stepFilterDelayResonanceQ, values[12].toFixed(3));
  show(stepFilterDelayDepth, `${Math.round(values[13] * 100)}%`);
  show(stepFilterDelayShape, `${Math.round(values[14] * 100)}%`);
  show(stepFilterDelaySmoothing, `${values[15]} ms`);
  show(stepFilterDelayPhase, `${Math.round(values[16] * 100)}%`);
  stepFilterDelaySteps.forEach((control, index) => {
    show(control, `${Math.round(values[17 + index] * 100)}%`);
  });
  if (workletNode) {
    workletNode.port.postMessage({ type: "step-filter-delay", value: values });
  }
}

function updateStereoFilterOptions() {
  const isAutoWah = Number(stereoFilterControls[0].value) === 15;
  const isEnvelopeFilter = Number(stereoFilterControls[0].value) === 16;
  const isLadderFilter = Number(stereoFilterControls[0].value) === 17;
  const isTouchLadderFilter = Number(stereoFilterControls[0].value) === 18;
  const isTouchWahFilter = Number(stereoFilterControls[0].value) === 19;
  const isLadderFamily = isLadderFilter || isTouchLadderFilter;
  const hasEnvelopeSweep = isAutoWah || isEnvelopeFilter ||
    isTouchLadderFilter || isTouchWahFilter;
  const frequencyControl = stereoFilterControls[1];
  const modFrequencyControl = stereoFilterControls[19];
  const frequencySpan = frequencyControl.parentElement.querySelector("span");
  const modFrequencySpan = modFrequencyControl.parentElement.querySelector("span");
  stereoFilterControls[2].parentElement.hidden = hasEnvelopeSweep || isLadderFamily;
  stereoFilterControls[3].parentElement.hidden = isLadderFamily;
  stereoFilterControls[4].parentElement.hidden = hasEnvelopeSweep || isLadderFamily;
  stereoFilterControls[10].parentElement.hidden = hasEnvelopeSweep || isLadderFamily;
  stereoFilterControls[11].parentElement.hidden = hasEnvelopeSweep || isLadderFamily;
  stereoFilterControls[12].parentElement.hidden = hasEnvelopeSweep || isLadderFamily;
  stereoFilterControls[7].parentElement.hidden = hasEnvelopeSweep;
  stereoFilterControls[8].parentElement.hidden = hasEnvelopeSweep;
  stereoFilterControls[9].parentElement.hidden = hasEnvelopeSweep;
  frequencyControl.min = hasEnvelopeSweep ? "100" : "20";
  frequencyControl.max = hasEnvelopeSweep ? "10000" : "20000";
  modFrequencyControl.min = hasEnvelopeSweep ? "100" : "20";
  modFrequencyControl.max = hasEnvelopeSweep ? "10000" : "20000";
  frequencyControl.value = String(Math.max(Number(frequencyControl.min),
    Math.min(Number(frequencyControl.max), Number(frequencyControl.value))));
  modFrequencyControl.value = String(Math.max(Number(modFrequencyControl.min),
    Math.min(Number(modFrequencyControl.max), Number(modFrequencyControl.value))));
  frequencySpan.textContent = hasEnvelopeSweep ? "Start frequency" : "Frequency";
  modFrequencySpan.textContent = hasEnvelopeSweep ? "Stop frequency" : "Modulation frequency";
  stereoFilterControls[3].parentElement.querySelector("span").textContent =
    hasEnvelopeSweep ? "Resonance" : "Q";
  stereoFilterControls[15].parentElement.hidden = hasEnvelopeSweep;
  stereoFilterControls[16].parentElement.hidden =
    isEnvelopeFilter || isTouchLadderFilter || isTouchWahFilter;
  stereoFilterControls[17].parentElement.hidden =
    isEnvelopeFilter || isTouchLadderFilter || isTouchWahFilter;
  stereoFilterControls[18].parentElement.hidden =
    isEnvelopeFilter || isTouchLadderFilter || isTouchWahFilter;
  stereoFilterControls[19].parentElement.hidden =
    isEnvelopeFilter || isTouchLadderFilter || isTouchWahFilter;
  stereoFilterControls[20].parentElement.hidden =
    isEnvelopeFilter || isTouchLadderFilter || isTouchWahFilter;
  stereoFilterControls[21].parentElement.hidden = !hasEnvelopeSweep && !isLadderFamily;
  stereoFilterControls[22].parentElement.hidden = !hasEnvelopeSweep;
  stereoFilterControls[23].parentElement.hidden = !isAutoWah && !isLadderFilter;
  stereoFilterControls[24].parentElement.hidden = !isAutoWah && !isLadderFilter;
  stereoFilterControls[25].parentElement.hidden = !isAutoWah && !isLadderFilter;
  stereoFilterControls[26].parentElement.hidden =
    !isEnvelopeFilter && !isTouchLadderFilter && !isTouchWahFilter;
  stereoFilterControls[27].parentElement.hidden =
    !isEnvelopeFilter && !isTouchLadderFilter && !isTouchWahFilter;
  stereoFilterControls[28].parentElement.hidden =
    !isEnvelopeFilter && !isTouchLadderFilter && !isTouchWahFilter;
  stereoFilterControls[29].parentElement.hidden =
    !isEnvelopeFilter && !isTouchLadderFilter && !isTouchWahFilter;
  for (let index = 30; index < stereoFilterControls.length; index += 1) {
    stereoFilterControls[index].parentElement.hidden = !isLadderFamily;
  }
  const ladderHighpassOption = stereoFilterControls[21].querySelector('option[value="2"]');
  ladderHighpassOption.disabled = isLadderFamily;
  if (isLadderFamily && Number(stereoFilterControls[21].value) === 2) {
    stereoFilterControls[21].value = "0";
  }
  stereoFilterControls[32].disabled = isLadderFamily &&
    Number(stereoFilterControls[21].value) === 1;
  stereoFilterControls[15].disabled = hasEnvelopeSweep;
  const values = [
    Number(stereoFilterControls[0].value),
    Number(stereoFilterControls[1].value),
    Number(stereoFilterControls[2].value),
    Number(stereoFilterControls[3].value),
    Number(stereoFilterControls[4].value),
    Number(stereoFilterControls[5].value),
    Number(stereoFilterControls[6].value),
    Number(stereoFilterControls[7].value),
    Number(stereoFilterControls[8].value),
    Number(stereoFilterControls[9].value),
    Number(stereoFilterControls[10].value),
    Number(stereoFilterControls[11].value),
    Number(stereoFilterControls[12].value),
    Number(stereoFilterControls[13].value),
    Number(stereoFilterControls[14].value),
    Number(isAutoWah || (!hasEnvelopeSweep && stereoFilterControls[15].checked)),
    Number(stereoFilterControls[16].value),
    Number(stereoFilterControls[17].value),
    Number(stereoFilterControls[18].value),
    Number(stereoFilterControls[19].value),
    Number(stereoFilterControls[20].value),
    Number(stereoFilterControls[21].value),
    Number(stereoFilterControls[22].value),
    Number(stereoFilterControls[23].checked),
    Number(stereoFilterControls[24].value),
    Number(stereoFilterControls[25].value),
    Number(stereoFilterControls[26].value),
    Number(stereoFilterControls[27].value),
    Number(stereoFilterControls[28].value),
    Number(stereoFilterControls[29].value) / 100,
    Number(stereoFilterControls[30].value),
    Number(stereoFilterControls[31].value),
    Number(stereoFilterControls[32].value),
    Number(stereoFilterControls[33].value),
  ];
  DEFAULT_PARAMETERS.stereoFilter = values;
  const displays = [
    stereoFilterControls[0].selectedOptions[0]?.textContent ?? values[0],
    `${values[1]} Hz`,
    `${values[2]}-pole`,
    values[3].toFixed(3),
    `${values[4]} dB`,
    values[5] === 0 ? "Off" : `${values[5]} Hz`,
    values[6] === 0 ? "Off" : `${values[6]} Hz`,
    `${values[7]}%`,
    `${values[8]}%`,
    stereoFilterControls[9].selectedOptions[0]?.textContent ?? values[9],
    `${values[10]} ms`,
    values[11].toFixed(2),
    values[12].toFixed(2),
    `${values[13]} dB`,
    `${values[14]}%`,
    isAutoWah ? "Automatic" :
      isEnvelopeFilter ? "Attack envelope" :
        isTouchLadderFilter ? "Touch envelope" :
          isTouchWahFilter ? "Touch volume" : values[15] ? "On" : "Off",
    stereoFilterControls[16].selectedOptions[0]?.textContent ?? values[16],
    `${values[17]} Hz`,
    `${values[18]}%`,
    `${values[19]} Hz`,
    values[20] === 0 ? "Off" : String(values[20]),
    stereoFilterControls[21].selectedOptions[0]?.textContent ?? values[21],
    values[22].toFixed(2),
    values[23] ? "Tempo sync" : "Hz rate",
    `${values[24]} BPM`,
    stereoFilterControls[25].selectedOptions[0]?.textContent ?? values[25],
    `${values[26]} dBFS`,
    `${values[27]} ms`,
    `${values[28]} ms`,
    `${Math.round(values[29] * 100)}%`,
    `${Math.round(values[30] * 100)}%`,
    `${Math.round(values[31] * 100)}%`,
    `${Math.round(values[32] * 100)}%`,
    `${values[33]}-pole`,
  ];
  stereoFilterControls.forEach((control, index) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = displays[index];
  });
  if (workletNode) {
    workletNode.port.postMessage({ type: "stereo-filter", value: values });
  }
}

function updateStepTremoloOptions() {
  const values = [
    Number(stepTremoloRateHz.value),
    Number(stepTremoloDepth.value) / 100,
    Number(stepTremoloStepCount.value),
    ...stepTremoloLevels.map((control) => Number(control.value) / 100),
    Number(stepTremoloTransitionMs.value),
  ];
  DEFAULT_PARAMETERS.stepTremolo = values;
  const rateLabel = (value) => Number(value).toFixed(2).replace(/\.?0+$/, "");
  stepTremoloStepCount.parentElement.querySelector("output").value =
    `${stepTremoloStepCount.value} steps`;
  stepTremoloRateHz.parentElement.querySelector("output").value =
    `${rateLabel(stepTremoloRateHz.value)} Hz`;
  stepTremoloDepth.parentElement.querySelector("output").value =
    `${stepTremoloDepth.value}%`;
  stepTremoloTransitionMs.parentElement.querySelector("output").value =
    `${stepTremoloTransitionMs.value} ms`;
  for (const control of stepTremoloLevels) {
    control.parentElement.querySelector("output").value = `${control.value}%`;
  }
  if (workletNode) {
    workletNode.port.postMessage({ type: "step-tremolo", value: values });
  }
}

function updateOctaveDividerOptions() {
  const values = octaveDividerControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.octaveDivider = values;
  octaveDividerControls.forEach((control, index) => {
    const output = control.parentElement.querySelector("output");
    if (!output) return;
    if (index === 0) output.value = `${values[index].toFixed(2)}×`;
    else if (index === 1 || index === 3 || index === 5) {
      output.value = `${Math.round(values[index] * 100)}%`;
    } else if (index === 2 || index === 4) {
      const percent = Math.round(values[index] * 100);
      output.value = `${percent > 0 ? "+" : ""}${percent}%`;
    } else output.value = `${Math.round(values[index] * 100)}%`;
  });
  if (workletNode) {
    workletNode.port.postMessage({ type: "octave-divider", value: values });
  }
}

function updateDoubleTakeOptions() {
  const values = doubleTakeControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.doubleTake = values;
  const output = (control, value) => {
    const node = control.parentElement.querySelector("output");
    if (node) node.value = value;
  };
  output(doubleTakeControls[0], `${values[0]} voices`);
  output(doubleTakeControls[1], `${Math.round(values[1] * 100)}%`);
  output(doubleTakeControls[2], `${Math.round(values[2] * 100)}%`);
  output(doubleTakeControls[3], `${Math.round(values[3] * 100)}%`);
  output(doubleTakeControls[4], `${Math.round(values[4] * 100)}%`);
  output(doubleTakeControls[5], ["Mono", "True Stereo", "Left Only", "Right Only"][values[5]] || "Mono");
  if (workletNode) {
    workletNode.port.postMessage({ type: "double-take", value: values });
  }
}

function updateEQ5GraphicOptions() {
  const values = eq5GraphicControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.eq5Graphic = values;
  eq5GraphicControls.forEach((control, index) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = `${values[index].toFixed(1)} dB`;
  });
  if (workletNode) {
    workletNode.port.postMessage({ type: "eq5-graphic", value: values });
  }
}

function updateOpticalTremoloOptions() {
  const variant = Number(opticalTremoloControls[0].value);
  const values = [
    variant,
    Number(opticalTremoloControls[1].value),
    Number(opticalTremoloControls[2].value) / 100,
    Number(opticalTremoloControls[3].value),
    Number(opticalTremoloControls[4].checked),
    Number(opticalTremoloControls[5].value),
    Number(opticalTremoloControls[6].value),
    Number(opticalTremoloControls[7].value),
    Number(opticalTremoloControls[8].value),
    Number(opticalTremoloControls[9].value),
    Number(opticalTremoloControls[10].value),
    Number(opticalTremoloControls[11].checked),
    Number(opticalTremoloControls[12].value),
    Number(opticalTremoloControls[13].value),
    Number(opticalTremoloControls[14].value) / 100,
    Number(opticalTremoloControls[15].value),
    Number(opticalTremoloControls[16].value),
    Number(opticalTremoloControls[17].value),
  ];
  DEFAULT_PARAMETERS.opticalTremolo = values;
  const lfoNames = [
    "Sine", "Triangle", "Square", "Saw Up", "Saw Down", "Random",
    "Logarithmic", "Exponential", "Trapezoid", "Astable",
  ];
  const labels = [
    ["Optical 1", "Optical 2", "Neon Trem"][variant] || "Optical 1",
    `${values[1].toFixed(2)} Hz`, `${Math.round(values[2] * 100)}%`,
    lfoNames[values[3]] || "Sine", values[4] ? "On" : "Off",
    `${Math.round(values[5])} BPM`, `${values[6]} beats`,
    `${Math.round(values[7])}%`, `${Math.round(values[8])}%`,
    `${Math.round(values[9])}°`, `${Math.round(values[10])}°`,
    values[11] ? "On" : "Off", `${Math.round(values[12])} dBFS`,
    `${Math.round(values[13])}%`, `${Math.round(values[14] * 100)}%`,
    `${values[15].toFixed(1)} dB`, `${Math.round(values[16])}%`,
    values[17] === 0 ? "Pass" : "Mute",
  ];
  opticalTremoloControls.forEach((control, index) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = labels[index];
  });
  const neon = variant === 2;
  opticalTremoloControls[3].disabled = neon;
  opticalTremoloControls[7].disabled = neon;
  opticalTremoloControls[8].disabled = neon;
  if (workletNode) {
    workletNode.port.postMessage({ type: "optical-tremolo", value: values });
  }
}

function updateFractalBiasTremOptions() {
  const values = [
    Number(fractalBiasTremControls[0].value),
    Number(fractalBiasTremControls[1].value) / 100,
    Number(fractalBiasTremControls[2].value),
    Number(fractalBiasTremControls[3].checked),
    Number(fractalBiasTremControls[4].value),
    Number(fractalBiasTremControls[5].value),
    Number(fractalBiasTremControls[6].checked),
    Number(fractalBiasTremControls[7].value),
    Number(fractalBiasTremControls[8].value) / 100,
    Number(fractalBiasTremControls[9].value) / 100,
    Number(fractalBiasTremControls[10].value) / 100,
    Number(fractalBiasTremControls[11].value),
    Number(fractalBiasTremControls[12].value) / 100,
    Number(fractalBiasTremControls[13].checked),
  ];
  DEFAULT_PARAMETERS.fractalBiasTrem = values;
  const waveforms = ["Sine", "Triangle", "Square", "Saw Up", "Saw Down", "Sample & Hold"];
  const labels = [
    `${values[0].toFixed(2)} Hz`, `${Math.round(values[1] * 100)}%`,
    waveforms[values[2]] || "Sine", values[3] ? "On" : "Off",
    `${Math.round(values[4])} BPM`, `${values[5]} beats`,
    values[6] ? "On" : "Off", `${values[7].toFixed(1)} dB`,
    `${Math.round(values[8] * 100)}%`, `${Math.round(values[9] * 100)}%`,
    `${Math.round(values[10] * 100)}%`, `${values[11].toFixed(1)} dB`,
    `${Math.round(values[12] * 100)}%`, values[13] ? "On" : "Off",
  ];
  fractalBiasTremControls.forEach((control, index) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = labels[index];
  });
  if (workletNode) {
    workletNode.port.postMessage({ type: "fractal-bias-trem", value: values });
  }
}

function updateVibratoneOptions() {
  const values = [
    Number(vibratoneControls[0].value) / 100,
    Number(vibratoneControls[1].value) / 100,
    Number(vibratoneControls[2].value),
    Number(vibratoneControls[3].value),
    Number(vibratoneControls[4].value),
    Number(vibratoneControls[5].value),
    Number(vibratoneControls[6].value),
    Number(vibratoneControls[7].value) / 100,
    Number(vibratoneControls[8].checked),
  ];
  DEFAULT_PARAMETERS.vibratone = values;
  const labels = [
    `${Math.round(values[0] * 100)}%`, `${Math.round(values[1] * 100)}%`,
    `${values[2].toFixed(1)} dB`, `${values[3].toFixed(2)} Hz`,
    `${values[4].toFixed(2)} Hz`, values[5] ? "Fast" : "Slow",
    `${Math.round(values[6])} ms`, `${Math.round(values[7] * 100)}%`,
    values[8] ? "On" : "Off",
  ];
  vibratoneControls.forEach((control, index) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = labels[index];
  });
  vibratoneControls[5].disabled = Boolean(values[8]);
  if (workletNode) {
    workletNode.port.postMessage({ type: "vibratone", value: values });
  }
}

function updateVolumePanOptions() {
  const values = [
    Number(volumePanPosition.value) / 100,
    Number(volumePanMinimumDb.value),
    Number(volumePanMaximumDb.value),
    Number(volumePanTaper.value),
    Number(volumePanBalance.value),
    Number(volumePanInputSelect.value),
    Number(volumePanLeft.value),
    Number(volumePanRight.value),
    Number(volumePanOutputLevel.value),
    Number(volumePanBypassed.checked),
    Number(volumePanBypassMode.value),
  ];
  DEFAULT_PARAMETERS.volumePan = values;
  const show = (control, value) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = value;
  };
  const tapers = ["Linear", "Log 30A", "Log 20A", "Log 15A", "Log 10A", "Log 5A", "S-Taper"];
  const inputs = ["Stereo", "Left Only", "Right Only"];
  show(volumePanPosition, `${Math.round(values[0] * 100)}%`);
  show(volumePanMinimumDb, `${values[1].toFixed(1)} dB`);
  show(volumePanMaximumDb, `${values[2].toFixed(1)} dB`);
  show(volumePanTaper, tapers[values[3]] || "Linear");
  show(volumePanInputSelect, inputs[values[5]] || "Stereo");
  show(volumePanLeft, `${Math.round(values[6])}%`);
  show(volumePanRight, `${Math.round(values[7])}%`);
  show(volumePanBalance, `${Math.round(values[4])}%`);
  show(volumePanOutputLevel, `${values[8].toFixed(1)} dB`);
  show(volumePanBypassed, values[9] ? "On" : "Off");
  show(volumePanBypassMode, values[10] ? "Mute input" : "Pass input");
  if (workletNode) {
    workletNode.port.postMessage({ type: "volume-pan", value: values });
  }
}

function updateMultiDelayOptions() {
  const sampleRate = audioContext?.sampleRate || 48000;
  const topology = Number(multiDelayTopology.value);
  const maximumMasterFeedbackPercent = topology >= 3 ? 200 : 90;
  multiDelayDiffusorMasterFeedback.max = String(maximumMasterFeedbackPercent);
  if (Number(multiDelayDiffusorMasterFeedback.value) > maximumMasterFeedbackPercent) {
    multiDelayDiffusorMasterFeedback.value = String(maximumMasterFeedbackPercent);
  }
  const maximumFrames = Math.floor((512 * 1024) /
    (Float32Array.BYTES_PER_ELEMENT * 4));
  const maximumDelayMs = Math.max(1, Math.min(2000,
    Math.floor((maximumFrames - 1) * 1000 / sampleRate)));
  for (const line of multiDelayLineControls) {
    line.timeMs.max = String(maximumDelayMs);
    if (Number(line.timeMs.value) > maximumDelayMs) {
      line.timeMs.value = String(maximumDelayMs);
    }
  }
  const lines = multiDelayLineControls.map((line) => ({
    timeMs: Number(line.timeMs.value),
    timeMode: Number(line.timeMode.value),
    division: Number(line.division.value),
    feedback: Number(line.feedback.value) / 100,
    crossFeedback: Number(line.crossFeedback.value) / 100,
    level: Number(line.level.value) / 100,
    pan: Number(line.pan.value) / 100,
  }));
  const timeLfo = multiDelayLfoControls.map((lfo) => ({
    waveform: Number(lfo.waveform.value),
    phaseDegrees: Number(lfo.phaseDegrees.value),
    rateHz: Number(lfo.rateHz.value),
    tempoSync: Number(lfo.rateMode.value),
    division: Number(lfo.division.value),
    depthMs: Number(lfo.depthMs.value),
  }));
  const value = {
    lines,
    timeLfo,
    topology,
    tempoBpm: Number(multiDelayTempo.value),
    lowCutHz: Number(multiDelayLowCut.value),
    highCutHz: Number(multiDelayHighCut.value),
    inputGainDb: Number(multiDelayInputGain.value),
    mix: Number(multiDelayMix.value) / 100,
    levelDb: Number(multiDelayLevel.value),
    seriesFeedback: Number(multiDelaySeriesFeedback.value) / 100,
    seriesFeedbackSend: Number(multiDelaySeriesSend.value),
    seriesFeedbackReturn: Number(multiDelaySeriesReturn.value),
    masterTime: Number(multiDelayDiffusorMasterTime.value) / 100,
    masterFeedback: Number(multiDelayDiffusorMasterFeedback.value) / 100,
    motorSpeed: Number(multiDelayMotorSpeed.value),
  };
  DEFAULT_PARAMETERS.multiDelay = value;
  const divisions = ["Whole", "Dotted half", "Half", "Dotted quarter", "Quarter",
    "Quarter triplet", "Dotted eighth", "Eighth", "Eighth triplet",
    "Dotted sixteenth", "Sixteenth", "Sixteenth triplet", "Thirty-second",
    "Sixty-fourth"];
  const show = (control, text) => {
    const output = control.parentElement.querySelector("output");
    if (output) output.value = text;
  };
  const diffusor = value.topology === 2;
  const quadTap = value.topology === 3;
  const quadTapBand = value.topology === 4;
  const quadTape = value.topology === 5;
  multiDelayMotorPanel.classList.toggle("is-hidden", !quadTape);
  multiDelayMotorSpeed.disabled = !quadTape;
  multiDelayLineControls.forEach((line, index) => {
    const settings = lines[index];
    const parallel = value.topology === 0;
    const series = value.topology === 1;
    line.timeMs.disabled = settings.timeMode === 1;
    line.feedback.disabled = series || diffusor;
    line.crossFeedback.disabled = !parallel;
    line.level.disabled = diffusor;
    line.pan.disabled = diffusor;
    line.feedback.parentElement.classList.toggle("is-hidden", diffusor);
    line.crossFeedback.parentElement.classList.toggle("is-hidden", !parallel);
    line.level.parentElement.classList.toggle("is-hidden", diffusor);
    line.pan.parentElement.classList.toggle("is-hidden", diffusor);
    line.feedback.min = "-45";
    line.feedback.max = "45";
    show(line.timeMs, `${settings.timeMs} ms`);
    show(line.timeMode, settings.timeMode === 1 ? "Tempo" : "Milliseconds");
    show(line.division, divisions[settings.division] || "Quarter");
    show(line.feedback, `${Math.round(settings.feedback * 100)}%`);
    show(line.crossFeedback, `${Math.round(settings.crossFeedback * 100)}%`);
    show(line.level, `${Math.round(settings.level * 100)}%`);
    show(line.pan, `${Math.round(settings.pan * 100)}%`);
  });
  multiDelaySeriesPanel.classList.toggle("is-hidden", value.topology !== 1);
  multiDelayDiffusorPanel.classList.toggle("is-hidden", !diffusor && !quadTap && !quadTapBand && !quadTape);
  multiDelayDiffusorMasterFeedback.min = "0";
  multiDelayTopologyMasterHint.textContent = diffusor
    ? "One project Master Feedback value controls feedback across all four all-pass stages; each stage has its own Time/Tempo. Per-stage Feedback, tap Level/Pan, and cross-feedback are hidden. Documented diffuser stereo spread, filters, LFO, and input envelope follower remain unimplemented. Project bounds are 10–200% Time and 0–90% Feedback."
    : quadTapBand
      ? "Quad Tap Band-like mode filters the audible tap output after its feedback loop. Master Time scales all four delay times and Master Feedback scales each tap’s feedback setting. Per-tap Time/Tempo, Feedback, Level, and Pan remain available; cross-feedback is disabled. Project bounds are 10–200% Time and 0–200% Feedback Scale."
      : quadTape
        ? "Quad Tape-like mode uses four independent delay heads. The guide documents Master Time and Motor Speed: 1.000 is normal speed, and while it moves the delay times and echo pitch change. The project range is 0.8–1.2× because no Quad Tape numeric range is published; read-head slew and interpolation are project-authored. Per-tap tape coloration, drive, diffusion, and other advanced pages remain unimplemented."
        : "For Quad-Tap, Master Time scales all four delay times and Master Feedback scales each tap’s feedback setting. Each tap retains its own Time/Tempo, Feedback, Level, and Pan; cross-feedback is disabled. The project uses 10–200% Time and 0–200% Feedback Scale. These are project bounds, not Fractal numeric settings.";
  const topologyName = value.topology === 1
    ? "Quad Series"
    : value.topology === 2 ? "Quad Diffusor"
      : value.topology === 3 ? "Quad-Tap"
      : value.topology === 4 ? "Quad Tap Band"
        : value.topology === 5 ? "Quad Tape" : "Quad Parallel";
  show(multiDelayTopology, topologyName);
  show(multiDelaySeriesFeedback,
    `${Math.round(value.seriesFeedback * 100)}%`);
  show(multiDelaySeriesSend,
    `Line ${value.seriesFeedbackSend + 1}`);
  show(multiDelaySeriesReturn,
    `Line ${value.seriesFeedbackReturn + 1}`);
  show(multiDelayDiffusorMasterTime,
    `${Math.round(value.masterTime * 100)}%`);
  show(multiDelayDiffusorMasterFeedback,
    `${Math.round(value.masterFeedback * 100)}%`);
  show(multiDelayMotorSpeed, `${value.motorSpeed.toFixed(3)}×`);
  show(multiDelayTempo, `${value.tempoBpm} BPM`);
  show(multiDelayLowCut, value.lowCutHz === 0 ? "Off" : `${value.lowCutHz} Hz`);
  show(multiDelayHighCut, `${value.highCutHz} Hz`);
  show(multiDelayInputGain, `${value.inputGainDb} dB`);
  show(multiDelayMix, `${Math.round(value.mix * 100)}%`);
  show(multiDelayLevel, `${value.levelDb} dB`);
  const waveformLabels = ["Sine", "Triangle", "Square", "Saw Up", "Saw Down",
    "Random", "Log", "Exponential", "Trapezoid", "Astable"];
  multiDelayLfoControls.forEach((lfo, index) => {
    const settings = timeLfo[index];
    const tempoSync = settings.tempoSync === 1;
    lfo.rateHz.disabled = tempoSync;
    lfo.division.disabled = !tempoSync;
    show(lfo.waveform, waveformLabels[settings.waveform] || "Sine");
    show(lfo.phaseDegrees, `${settings.phaseDegrees}°`);
    show(lfo.rateMode, tempoSync ? "Tempo sync" : "Free rate");
    show(lfo.rateHz, `${settings.rateHz.toFixed(2)} Hz`);
    show(lfo.division, divisions[settings.division] || "Quarter");
    show(lfo.depthMs, `${settings.depthMs.toFixed(1)} ms`);
  });
  multiDelayTimeLimit.textContent =
    `Effective time ceiling at ${sampleRate} Hz: ${maximumDelayMs} ms (512 KiB fixed ring storage).`;
  if (workletNode) workletNode.port.postMessage({ type: "multi-delay", value });
}

function sendShufflingLooperCommand(command) {
  if (!workletNode) {
    shufflingLooperStatus.value = "Start processed playback before using the looper";
    return;
  }
  workletNode.port.postMessage({ type: "shuffling-looper-command", command });
}

function scheduleShufflingLooperStatusPoll(state) {
  window.clearTimeout(shufflingLooperStatusTimeout);
  shufflingLooperStatusTimeout = 0;
  if (state !== 1) return;
  shufflingLooperStatusTimeout = window.setTimeout(() => {
    if (workletNode) {
      workletNode.port.postMessage({ type: "shuffling-looper-status-request" });
    }
  }, 250);
}

function renderShufflingLooperStatus(message) {
  const labels = ["Stopped", "Recording", "Playing"];
  const state = labels[message.state] || "Unknown";
  const frames = Number(message.frames) || 0;
  const sampleRate = Number(message.sampleRate) || 48000;
  const steps = Number(message.steps) || 0;
  const length = frames > 0 ? `${(frames / sampleRate).toFixed(2)} s` : "no loop";
  const accepted = message.accepted === false ? " · command unavailable" : "";
  shufflingLooperStatus.value = `${state} · ${length} · ${steps} steps${accepted}`;
  scheduleShufflingLooperStatusPoll(message.state);
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
  updateMultiDelayOptions();
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
  const channels = processorChannelCount(
    decodedAudio.numberOfChannels,
    testOptions?.enabled || enabledEffects(),
    testOptions?.parameters || DEFAULT_PARAMETERS,
  );
  if (workletNode && preparedChannels === channels) return processorReady;
  if (processorSetupTask) return processorSetupTask;
  processorSetupTask = setupProcessor(testOptions);
  try {
    return await processorSetupTask;
  } finally {
    processorSetupTask = null;
  }
}

function settleConvolutionIRLoad(message) {
  const request = pendingConvolutionIRLoads.get(Number(message.requestId));
  if (!request) return;
  pendingConvolutionIRLoads.delete(Number(message.requestId));
  clearTimeout(request.timeout);
  if (message.type === "convolution-ir-loaded") {
    if (request.version === convolutionIRVersion) {
      convolutionIRUploadedVersion = request.version;
    }
    request.resolve(message);
  } else {
    request.reject(new Error(message.message || "The browser processor rejected the convolution IR."));
  }
}

async function ensureConvolutionIRLoaded() {
  if (!decodedConvolutionIR || !workletNode) return;
  if (convolutionIRUploadedVersion === convolutionIRVersion) return;

  const version = convolutionIRVersion;
  const audio = decodedConvolutionIR.audio;
  const channels = audio.numberOfChannels;
  const left = new Float32Array(audio.getChannelData(0));
  const right = channels === 2 ? new Float32Array(audio.getChannelData(1)) : null;
  const requestId = ++convolutionIRRequestId;
  const transfer = right ? [left.buffer, right.buffer] : [left.buffer];
  const result = new Promise((resolve, reject) => {
    const timeout = setTimeout(() => {
      pendingConvolutionIRLoads.delete(requestId);
      reject(new Error("Timed out loading the local convolution IR into the browser processor."));
    }, 10000);
    pendingConvolutionIRLoads.set(requestId, { resolve, reject, timeout, version });
  });
  workletNode.port.postMessage({
    type: "convolution-ir",
    requestId,
    version,
    frames: audio.length,
    channels,
    sampleRate: audio.sampleRate,
    left: left.buffer,
    right: right?.buffer,
  }, transfer);
  await result;
}

function settleCabinetIRLoad(message) {
  const requestId = Number(message.requestId);
  const request = pendingCabinetIRLoads.get(requestId);
  if (!request) return;
  pendingCabinetIRLoads.delete(requestId);
  clearTimeout(request.timeout);
  if (message.type === "cabinet-ir-loaded") {
    if (request.version === cabinetIRVersion) {
      cabinetIRUploadedVersion = request.version;
      cabinetIRLatencyFrames = Number(message.latencyFrames) || 0;
      chainMemoryBytes = Number(message.memoryBytes) || chainMemoryBytes;
      wasmMemoryBytes = Number(message.wasmMemoryBytes) || wasmMemoryBytes;
      if (Array.isArray(effectMemory) && effectMemory.length > 100) {
        effectMemory[100] = Number(message.effectMemoryBytes) || 0;
      }
      document.querySelector("#wasm-memory-use").textContent = bytesLabel(wasmMemoryBytes);
      document.querySelector("#memory-use").textContent = bytesLabel(chainMemoryBytes);
      renderLoadRows(null);
      cabinetIrStatus.textContent =
        `${decodedCabinetIR.fileName} · mono PCM24 · 48 kHz · 2,048 samples · ${cabinetIRLatencyFrames}-frame project latency · local only`;
    }
    request.resolve(message);
  } else {
    request.reject(new Error(message.message || "The browser processor rejected the Cabinet IR."));
  }
}

async function ensureCabinetIRLoaded() {
  if (!decodedCabinetIR || !workletNode) return;
  if (cabinetIRUploadedVersion === cabinetIRVersion) return;

  const version = cabinetIRVersion;
  const samples = new Float32Array(decodedCabinetIR.samples);
  const requestId = ++cabinetIRRequestId;
  const result = new Promise((resolve, reject) => {
    const timeout = setTimeout(() => {
      pendingCabinetIRLoads.delete(requestId);
      reject(new Error("Timed out loading the local Cabinet IR into the browser processor."));
    }, 10000);
    pendingCabinetIRLoads.set(requestId, { resolve, reject, timeout, version });
  });
  workletNode.port.postMessage({
    type: "cabinet-ir",
    requestId,
    version,
    frames: decodedCabinetIR.frames,
    channels: decodedCabinetIR.channels,
    sampleRate: decodedCabinetIR.sampleRate,
    bitsPerSample: decodedCabinetIR.bitsPerSample,
    samples: samples.buffer,
  }, [samples.buffer]);
  await result;
}

async function disposeProcessorNode(node) {
  if (!node) return;
  await new Promise((resolve) => {
    let settled = false;
    const finish = () => {
      if (settled) return;
      settled = true;
      clearTimeout(timeout);
      node.port.removeEventListener("message", onMessage);
      resolve();
    };
    const onMessage = (event) => {
      if (event.data?.type === "disposed") finish();
    };
    const timeout = setTimeout(finish, 1000);
    node.port.addEventListener("message", onMessage);
    node.port.postMessage({ type: "dispose" });
  });
  try { node.disconnect(); } catch { /* Already disconnected. */ }
  try { node.port.close(); } catch { /* Already closed. */ }
}

async function setupProcessor(testOptions = null) {
  const context = await getAudioContext();
  const channels = processorChannelCount(
    decodedAudio.numberOfChannels,
    testOptions?.enabled || enabledEffects(),
    testOptions?.parameters || DEFAULT_PARAMETERS,
  );
  stopSource(false);
  if (workletNode) {
    for (const request of pendingConvolutionIRLoads.values()) {
      clearTimeout(request.timeout);
      request.reject(new Error("The audio processor was rebuilt while loading the convolution IR."));
    }
    pendingConvolutionIRLoads.clear();
    convolutionIRUploadedVersion = -1;
    for (const request of pendingCabinetIRLoads.values()) {
      clearTimeout(request.timeout);
      request.reject(new Error("The audio processor was rebuilt while loading the Cabinet IR."));
    }
    pendingCabinetIRLoads.clear();
    cabinetIRUploadedVersion = -1;
    await disposeProcessorNode(workletNode);
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
      updateLooperOptions();
       updateShufflingLooperOptions();
       workletNode.port.postMessage({ type: "shuffling-looper-status-request" });
       updateDeluxeLooperOptions();
       workletNode.port.postMessage({ type: "deluxe-looper-status-request" });
      resolveReady(message);
    } else if (message?.type === "tuner-reading") {
      renderTunerReading(message);
    } else if (message?.type === "looper-status") {
      const labels = ["Stopped", "Recording", "Playing", "Overdubbing"];
      const state = labels[message.state] || "Unknown";
      const frames = Number(message.frames) || 0;
      const sampleRate = Number(message.sampleRate) || 48000;
      const length = frames > 0 ? `${(frames / sampleRate).toFixed(2)} s` : "no loop";
      looperStatus.value = `${state} · ${length}${message.accepted ? "" : " · command unavailable"}`;
     } else if (message?.type === "deluxe-looper-status") {
       renderDeluxeLooperStatus(message);
     } else if (message?.type === "shuffling-looper-status") {
       renderShufflingLooperStatus(message);
    } else if (message?.type === "convolution-ir-loaded" ||
               message?.type === "convolution-ir-error") {
      settleConvolutionIRLoad(message);
     } else if (message?.type === "cabinet-ir-loaded" ||
                message?.type === "cabinet-ir-error") {
       settleCabinetIRLoad(message);
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

async function rebuildProcessorForChannelLayout() {
  const resumePlayback = Boolean(activeSource);
  if (resumePlayback) stopSource(true);
  try {
    await prepareProcessor();
    if (resumePlayback) await startPlayback();
  } catch (error) {
    setAudioStatus(error instanceof Error ? error.message : String(error), "error");
  }
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

async function loadConvolutionIRFile(file) {
  if (!file) return;
  stopSource(true);
  try {
    const context = await getAudioContext();
    const bytes = await file.arrayBuffer();
    const audio = decodePcmWave(context, bytes) || await context.decodeAudioData(bytes);
    if (audio.numberOfChannels < 1 || audio.numberOfChannels > 2) {
      throw new Error(`Convolution IR must be mono or stereo; this file has ${audio.numberOfChannels} channels.`);
    }
    if (audio.length > MAX_CONVOLUTION_IR_FRAMES) {
      throw new Error(`Convolution IR is ${audio.length} frames; this project accepts at most ${MAX_CONVOLUTION_IR_FRAMES} frames.`);
    }
    decodedConvolutionIR = { audio, fileName: file.name };
    convolutionIRVersion += 1;
    convolutionIRUploadedVersion = -1;
    const duration = audio.length / audio.sampleRate;
    convolutionIrStatus.textContent =
      `${file.name} · ${audio.numberOfChannels} ch · ${audio.sampleRate} Hz · ${duration.toFixed(3)} s · local only`;
    if (workletNode) {
      await processorReady;
      await ensureConvolutionIRLoaded();
      setAudioStatus("Convolution IR loaded locally", "ready");
    } else {
      setAudioStatus("Convolution IR ready; it will load before playback", "ready");
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    convolutionIrStatus.textContent = message;
    setAudioStatus(message, "error");
    throw error;
  }
}

async function loadCabinetIRFile(file) {
  if (!file) return;
  stopSource(true);
  try {
    const bytes = await file.arrayBuffer();
    const ir = decodeCabinetIrWave(bytes);
    decodedCabinetIR = { ...ir, fileName: file.name };
    cabinetIRVersion += 1;
    cabinetIRUploadedVersion = -1;
    cabinetIrStatus.textContent =
      `${file.name} · mono PCM24 · 48 kHz · 2,048 samples · 64-frame project latency · local only`;
    if (workletNode) {
      await processorReady;
      await ensureCabinetIRLoaded();
      setAudioStatus("Cabinet IR loaded locally", "ready");
    } else {
      setAudioStatus("Cabinet IR ready; it will load before playback", "ready");
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    cabinetIrStatus.textContent = message;
    setAudioStatus(message, "error");
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
  await ensureConvolutionIRLoaded();
  await ensureCabinetIRLoaded();
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

convolutionIrFileInput.addEventListener("change", () => {
  const file = convolutionIrFileInput.files?.[0] || null;
  const task = loadConvolutionIRFile(file);
  task.catch(() => {});
});

cabinetIrFileInput.addEventListener("change", () => {
  const file = cabinetIrFileInput.files?.[0] || null;
  const task = loadCabinetIRFile(file);
  task.catch(() => {});
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

for (const button of document.querySelectorAll("[data-looper-command]")) {
  button.addEventListener("click", () => {
    sendLooperCommand(Number(button.dataset.looperCommand));
  });
}

for (const button of document.querySelectorAll("[data-deluxe-looper-command]")) {
  button.addEventListener("click", () => {
    sendDeluxeLooperCommand(Number(button.dataset.deluxeLooperCommand));
  });
}

for (const button of document.querySelectorAll("[data-shuffling-looper-command]")) {
  button.addEventListener("click", () => {
    sendShufflingLooperCommand(Number(button.dataset.shufflingLooperCommand));
  });
}

looperReverse.addEventListener("change", updateLooperOptions);
looperHalfSpeed.addEventListener("change", updateLooperOptions);
looperOverdubLevel.addEventListener("input", updateLooperOptions);
looperAutoRecord.addEventListener("change", updateLooperOptions);
looperAutoRecordThreshold.addEventListener("input", updateLooperOptions);
for (const control of [
  deluxeLooperReverse, deluxeLooperHalfSpeed, deluxeLooperPlayback,
  deluxeLooperOverdubDecay, deluxeLooperLowCut, deluxeLooperHighCut,
]) {
  control.addEventListener("input", updateDeluxeLooperOptions);
  control.addEventListener("change", updateDeluxeLooperOptions);
}
for (const control of shufflingLooperControls) {
  control.addEventListener("input", updateShufflingLooperOptions);
  control.addEventListener("change", updateShufflingLooperOptions);
}
for (const control of tenTapDelayControls) {
  control.addEventListener("input", updateTenTapDelayOptions);
  control.addEventListener("change", updateTenTapDelayOptions);
}
for (const control of rhythmTapControls) {
  control.addEventListener("input", updateRhythmTapDelayOptions);
  control.addEventListener("change", updateRhythmTapDelayOptions);
}
rhythmTapLearnStart.addEventListener("click", beginRhythmTapLearning);
rhythmTapLearnTap.addEventListener("click", captureRhythmTapLearningBeat);
rhythmTapLearnDone.addEventListener("click", finishRhythmTapLearning);
updateRhythmTapDelayOptions();
for (const control of [...plexDelayGlobalControls, ...plexDelayLineControls]) {
  control.addEventListener(control.tagName === "SELECT" ? "change" : "input",
    updatePlexDelayOptions);
  if (control.tagName !== "SELECT") {
    control.addEventListener("change", updatePlexDelayOptions);
  }
}
updatePlexDelayOptions();
for (const control of autoPanControls) {
  control.addEventListener("input", updateAutoPanOptions);
  control.addEventListener("change", updateAutoPanOptions);
}
for (const control of stereoPannerControls) {
  control.addEventListener("input", updateStereoPannerOptions);
  control.addEventListener("change", updateStereoPannerOptions);
}
updateStereoPannerOptions();
for (const control of megatapDelayControls) {
  control.addEventListener("input", updateMegatapDelayOptions);
  control.addEventListener("change", updateMegatapDelayOptions);
}
for (const control of deEsserControls) {
  control.addEventListener("input", updateDeEsserOptions);
  control.addEventListener("change", updateDeEsserOptions);
}
updateDeEsserOptions();
for (const control of dualDistortionControls) {
  control.addEventListener(control.type === "checkbox" || control.tagName === "SELECT"
    ? "change" : "input", updateDualEngineDistortionOptions);
  if (control.type === "range") {
    control.addEventListener("change", updateDualEngineDistortionOptions);
  }
}
updateDualEngineDistortionOptions();
for (const control of vibeControls) {
  control.addEventListener(control.type === "range" ? "input" : "change",
    updateVibeOptions);
  if (control.type === "range") {
    control.addEventListener("change", updateVibeOptions);
  }
}
updateVibeOptions();
for (const control of uniVerseVibeControls) {
  control.addEventListener(control.type === "range" ? "input" : "change",
    updateUniVerseVibeOptions);
  if (control.type === "range") {
    control.addEventListener("change", updateUniVerseVibeOptions);
  }
}
updateUniVerseVibeOptions();
for (const control of portaPreampControls) {
  control.addEventListener("input", updatePortaPreampOptions);
  control.addEventListener("change", updatePortaPreampOptions);
}
updatePortaPreampOptions();
for (const control of pitchSequenceControls) {
  control.addEventListener(control.type === "range" ? "input" : "change",
    updatePitchSequenceOptions);
  if (control.type === "range") {
    control.addEventListener("change", updatePitchSequenceOptions);
  }
}
updatePitchSequenceOptions();
for (const control of polySustainControls) {
  control.addEventListener(control.type === "range" ? "input" : "change",
    updatePolySustainOptions);
  if (control.type === "range") {
    control.addEventListener("change", updatePolySustainOptions);
  }
}
updatePolySustainOptions();
for (const control of stepFilterDelayControls) {
  control.addEventListener(control.type === "range" ? "input" : "change",
    updateStepFilterDelayOptions);
  if (control.type === "range") {
    control.addEventListener("change", updateStepFilterDelayOptions);
  }
}
updateStepFilterDelayOptions();
for (const control of volumePanControls) {
  control.addEventListener(control.type === "range" ? "input" : "change",
    updateVolumePanOptions);
  if (control.type === "range") {
    control.addEventListener("change", updateVolumePanOptions);
  }
}
updateVolumePanOptions();
for (const control of stereoFilterControls) {
  control.addEventListener(control.type === "range" ? "input" : "change",
    updateStereoFilterOptions);
  if (control.type === "range") {
    control.addEventListener("change", updateStereoFilterOptions);
  }
}
updateStereoFilterOptions();
for (const control of stepTremoloControls) {
  control.addEventListener(control.type === "range" ? "input" : "change",
    updateStepTremoloOptions);
  if (control.type === "range") {
    control.addEventListener("change", updateStepTremoloOptions);
  }
}
updateStepTremoloOptions();
for (const control of octaveDividerControls) {
  control.addEventListener("input", updateOctaveDividerOptions);
  control.addEventListener("change", updateOctaveDividerOptions);
}
updateOctaveDividerOptions();

for (const control of doubleTakeControls) {
  control.addEventListener("input", updateDoubleTakeOptions);
  control.addEventListener("change", updateDoubleTakeOptions);
}
updateDoubleTakeOptions();
for (const control of eq5GraphicControls) {
  control.addEventListener("input", updateEQ5GraphicOptions);
  control.addEventListener("change", updateEQ5GraphicOptions);
}
updateEQ5GraphicOptions();
for (const control of opticalTremoloControls) {
  control.addEventListener("input", updateOpticalTremoloOptions);
  control.addEventListener("change", updateOpticalTremoloOptions);
}
updateOpticalTremoloOptions();
for (const control of fractalBiasTremControls) {
  control.addEventListener("input", updateFractalBiasTremOptions);
  control.addEventListener("change", updateFractalBiasTremOptions);
}
updateFractalBiasTremOptions();
for (const control of vibratoneControls) {
  control.addEventListener("input", updateVibratoneOptions);
  control.addEventListener("change", updateVibratoneOptions);
}
updateVibratoneOptions();
for (const control of multiDelayControls) {
  control.addEventListener(control.type === "range" ? "input" : "change",
    updateMultiDelayOptions);
  if (control.type === "range") control.addEventListener("change", updateMultiDelayOptions);
}
updateMultiDelayOptions();

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
      const index = Number(checkbox.dataset.effect);
      const targetChannels = processorChannelCount(
        decodedAudio?.numberOfChannels || preparedChannels,
        enabledEffects(),
      );
      if (targetChannels !== preparedChannels) {
        void rebuildProcessorForChannelLayout();
      } else {
        workletNode.port.postMessage({
          type: "enable",
          index,
          value: checkbox.checked,
        });
      }
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

function updatePitchExpressionPosition() {
  const enabled = pitchMode.value === "pedal-shifter";
  const targetSemitones = Number(pitchExpressionTarget.value);
  const position = Number(pitchExpressionPosition.value) / 100;
  pitchSemitones.disabled = enabled;
  pitchExpressionTarget.disabled = !enabled;
  pitchExpressionPosition.disabled = !enabled;
  pitchExpressionTargetValue.value = `${targetSemitones > 0 ? "+" : ""}${targetSemitones.toFixed(1)} semitones`;
  pitchExpressionPositionValue.value = `${Math.round(position * 100)}%`;
  DEFAULT_PARAMETERS.pitchExpression = { enabled, targetSemitones, position };
  if (!workletNode) return;
  if (enabled) {
    workletNode.port.postMessage({
      type: "pitch-expression-position", targetSemitones, position,
    });
  } else {
    workletNode.port.postMessage({
      type: "pitch-semitones", value: Number(pitchSemitones.value),
    });
  }
}

pitchMode.addEventListener("change", updatePitchExpressionPosition);
for (const control of [pitchExpressionTarget, pitchExpressionPosition]) {
  control.addEventListener("input", updatePitchExpressionPosition);
  control.addEventListener("change", updatePitchExpressionPosition);
}
updatePitchExpressionPosition();

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

function updateMercatorDistortion() {
  const values = [
    Number(mercatorDistortionControls[0].value),
    Number(mercatorDistortionControls[1].value),
    Number(mercatorDistortionControls[2].value),
    Number(mercatorDistortionControls[3].value),
    Number(mercatorDistortionControls[4].value),
    Number(mercatorDistortionControls[5].checked),
    Number(mercatorDistortionControls[6].checked),
    Number(mercatorDistortionControls[7].value),
  ];
  DEFAULT_PARAMETERS.mercatorDistortion = values;
  const formats = [
    (value) => value.toFixed(2),
    (value) => value.toFixed(2),
    (value) => `${value.toFixed(1)} dB`,
    (value) => `${value.toFixed(1)} dB`,
    (value) => `${value.toFixed(1)} dB`,
    (value) => value === 1 ? "On" : "Off",
    (value) => value === 1 ? "On" : "Off",
    (value) => `${value.toFixed(1)} dB`,
  ];
  mercatorDistortionControls.forEach((control, index) => {
    control.parentElement.querySelector("output").value = formats[index](values[index]);
  });
  if (workletNode) {
    workletNode.port.postMessage({ type: "mercator-distortion", value: values });
  }
}

for (const control of mercatorDistortionControls) {
  control.addEventListener("input", updateMercatorDistortion);
  control.addEventListener("change", updateMercatorDistortion);
}
updateMercatorDistortion();

function updateOctobot() {
  const values = octobotControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.octobot = values;
  octobotControls.forEach((control, index) => {
    control.parentElement.querySelector("output").value = `${Math.round(values[index] * 100)}%`;
  });
  if (workletNode) {
    workletNode.port.postMessage({ type: "octobot", value: values });
  }
}

for (const control of octobotControls) {
  control.addEventListener("input", updateOctobot);
  control.addEventListener("change", updateOctobot);
}
updateOctobot();

function updateAmRingMod() {
  const values = amRingModControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.amRingMod = values;
  amRingModControls.forEach((control, index) => {
    const output = control.parentElement.querySelector("output");
    output.value = index === 0 ? `${values[index].toFixed(1)} Hz`
      : index === 3 ? `${values[index].toFixed(1)} dB`
      : `${Math.round(values[index] * 100)}%`;
  });
  if (workletNode) {
    workletNode.port.postMessage({ type: "am-ring-mod", value: values });
  }
}

for (const control of amRingModControls) {
  control.addEventListener("input", updateAmRingMod);
  control.addEventListener("change", updateAmRingMod);
}
updateAmRingMod();

function updateTapeEcho() {
  const values = [
    Number(tapeEchoControls[0].value),
    Number(tapeEchoControls[1].value),
    Number(tapeEchoControls[2].value),
    Number(tapeEchoControls[3].value),
    Number(tapeEchoControls[4].value),
    Number(tapeEchoControls[5].value),
    Number(tapeEchoControls[6].value),
    Number(tapeEchoControls[7].value),
    Number(tapeEchoControls[8].value),
    Number(tapeEchoControls[9].value),
    tapeEchoControls[10].checked ? 1 : 0,
    tapeEchoControls[11].checked ? 1 : 0,
    Number(tapeEchoControls[12].value),
    Number(tapeEchoControls[13].value),
    Number(tapeEchoControls[14].value),
    Number(tapeEchoControls[15].value),
  ];
  DEFAULT_PARAMETERS.tapeEcho = values;
  const independent = values[11] === 1;
  document.querySelector("#tape-echo-shared-time-controls").hidden = independent;
  document.querySelector("#tape-echo-independent-time-controls").hidden = !independent;
  tapeEchoControls[1].parentElement.querySelector("output").value =
    `${Math.round(values[1])} ms`;
  tapeEchoControls[2].parentElement.querySelector("output").value =
    `${Math.round(values[2])} BPM`;
  tapeEchoControls[12].parentElement.querySelector("output").value =
    `${Math.round(values[12])} ms`;
  tapeEchoControls[13].parentElement.querySelector("output").value =
    `${Math.round(values[13])} ms`;
  for (let index = 4; index <= 9; index += 1) {
    tapeEchoControls[index].parentElement.querySelector("output").value =
      `${Math.round(values[index] * 100)}%`;
  }
  if (workletNode) {
    workletNode.port.postMessage({ type: "tape-echo", value: values });
  }
}

for (const control of tapeEchoControls) {
  control.addEventListener("input", updateTapeEcho);
  control.addEventListener("change", updateTapeEcho);
}
updateTapeEcho();

function updateOctaveFuzz() {
  const values = [
    Number(octaveFuzzControls[0].value),
    Number(octaveFuzzControls[1].value),
    octaveFuzzControls[2].checked ? 1 : 0,
  ];
  DEFAULT_PARAMETERS.octaveFuzz = values;
  octaveFuzzControls[0].parentElement.querySelector("output").value =
    `${Math.round(values[0] * 100)}%`;
  octaveFuzzControls[1].parentElement.querySelector("output").value =
    `${Math.round(values[1] * 100)}%`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "octave-fuzz", value: values });
  }
}

for (const control of octaveFuzzControls) {
  control.addEventListener("input", updateOctaveFuzz);
  control.addEventListener("change", updateOctaveFuzz);
}
updateOctaveFuzz();

function updateRockBoxDist() {
  const values = rockBoxDistControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.rockBoxDistortion = values;
  rockBoxDistControls[0].parentElement.querySelector("output").value =
    `${Math.round(values[0] * 100)}%`;
  rockBoxDistControls[1].parentElement.querySelector("output").value =
    `${Math.round(values[1] * 100)}%`;
  rockBoxDistControls[2].parentElement.querySelector("output").value =
    `${values[2].toFixed(1)} dB`;
  rockBoxDistControls[3].parentElement.querySelector("output").value =
    `${values[3].toFixed(0)} ms`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "rock-box-dist", value: values });
  }
}

for (const control of rockBoxDistControls) {
  control.addEventListener("input", updateRockBoxDist);
  control.addEventListener("change", updateRockBoxDist);
}
updateRockBoxDist();

function updateRockBoxBoost() {
  const values = rockBoxBoostControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.rockBoxBoost = values;
  rockBoxBoostControls[0].parentElement.querySelector("output").value =
    `${values[0].toFixed(1)} dB`;
  rockBoxBoostControls[1].parentElement.querySelector("output").value =
    `${values[1].toFixed(1)} dB`;
  rockBoxBoostControls[2].parentElement.querySelector("output").value =
    `${values[2].toFixed(0)} ms`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "rock-box-boost", value: values });
  }
}

for (const control of rockBoxBoostControls) {
  control.addEventListener("input", updateRockBoxBoost);
  control.addEventListener("change", updateRockBoxBoost);
}
updateRockBoxBoost();

function updateStereoCrossover() {
  const values = {
    frequencyHz: Number(stereoCrossoverControls.frequencyHz.value),
    frequencyMultiplier: stereoCrossoverControls.frequencyMultiplier.value,
    lowLevelLeftDb: Number(stereoCrossoverControls.lowLevelLeftDb.value),
    lowLevelRightDb: Number(stereoCrossoverControls.lowLevelRightDb.value),
    highLevelLeftDb: Number(stereoCrossoverControls.highLevelLeftDb.value),
    highLevelRightDb: Number(stereoCrossoverControls.highLevelRightDb.value),
    lowPanLeft: Number(stereoCrossoverControls.lowPanLeft.value),
    highPanLeft: Number(stereoCrossoverControls.highPanLeft.value),
    lowPanRight: Number(stereoCrossoverControls.lowPanRight.value),
    highPanRight: Number(stereoCrossoverControls.highPanRight.value),
    levelDb: Number(stereoCrossoverControls.levelDb.value),
    balance: Number(stereoCrossoverControls.balance.value),
  };
  DEFAULT_PARAMETERS.stereoCrossover = values;
  stereoCrossoverControls.frequencyHz.parentElement.querySelector("output").value =
    `${Math.round(values.frequencyHz)} Hz`;
  for (const key of [
    "lowLevelLeftDb", "lowLevelRightDb", "highLevelLeftDb", "highLevelRightDb", "levelDb",
  ]) {
    stereoCrossoverControls[key].parentElement.querySelector("output").value =
      `${values[key].toFixed(1)} dB`;
  }
  for (const key of ["lowPanLeft", "highPanLeft", "lowPanRight", "highPanRight", "balance"]) {
    stereoCrossoverControls[key].parentElement.querySelector("output").value =
      `${Math.round(values[key] * 100)}%`;
  }
  if (workletNode) {
    workletNode.port.postMessage({ type: "stereo-crossover", value: values });
  }
}

for (const control of Object.values(stereoCrossoverControls)) {
  control.addEventListener("input", updateStereoCrossover);
  control.addEventListener("change", updateStereoCrossover);
}
updateStereoCrossover();

function updateManualThruZeroFlanger(event) {
  const minimumControl = manualThruZeroFlangerControls[0];
  const maximumControl = manualThruZeroFlangerControls[1];
  let minimumTime = Number(minimumControl.value);
  let maximumTime = Number(maximumControl.value);
  if (event?.currentTarget === minimumControl && maximumTime < minimumTime + 0.01) {
    maximumTime = Math.min(20, minimumTime + 0.01);
    maximumControl.value = maximumTime.toFixed(2);
  } else if (event?.currentTarget === maximumControl &&
             minimumTime > maximumTime - 0.01) {
    minimumTime = Math.max(0.34, maximumTime - 0.01);
    minimumControl.value = minimumTime.toFixed(2);
  }
  minimumControl.max = Math.min(19.99, maximumTime - 0.01).toFixed(2);
  maximumControl.min = Math.max(0.35, minimumTime + 0.01).toFixed(2);
  const values = [
    minimumTime,
    maximumTime,
    Number(manualThruZeroFlangerControls[2].value),
    Number(manualThruZeroFlangerControls[3].value),
    Number(manualThruZeroFlangerControls[4].value),
    Number(manualThruZeroFlangerControls[5].value),
    Number(manualThruZeroFlangerControls[6].value),
    Number(manualThruZeroFlangerControls[7].value),
  ];
  DEFAULT_PARAMETERS.manualThruZeroFlanger = values;
  const formats = [
    (value) => `${value.toFixed(2)} ms`,
    (value) => `${value.toFixed(2)} ms`,
    (value) => `${value.toFixed(2)} ms`,
    (value) => `${Math.round(value * 100)}%`,
    (value) => `${Math.round(value)}%`,
    (value) => ["None", "Left", "Right", "Both"][value] || "Both",
    (value) => `${Math.round(value * 100)}%`,
    (value) => `${Math.round(value)} ms`,
  ];
  manualThruZeroFlangerControls.forEach((control, index) => {
    control.parentElement.querySelector("output").value = formats[index](values[index]);
  });
  if (workletNode) {
    workletNode.port.postMessage({ type: "manual-thru-zero-flanger", value: values });
  }
}

for (const control of manualThruZeroFlangerControls) {
  control.addEventListener("input", updateManualThruZeroFlanger);
  control.addEventListener("change", updateManualThruZeroFlanger);
}
updateManualThruZeroFlanger();

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

function updateHarmonicTremolo() {
  const waveform = { sine: 0, triangle: 1, square: 2 }[harmonicTremoloWaveform.value] ?? 0;
  const values = [
    Number(harmonicTremoloRate.value),
    Number(harmonicTremoloDepth.value),
    Number(harmonicTremoloCrossover.value),
    waveform,
    harmonicTremoloSync.value === "on" ? 1 : 0,
    Number(harmonicTremoloBpm.value),
    Number(harmonicTremoloDivision.value),
    harmonicTremoloStereo.value === "on" ? 1 : 0,
    Number(harmonicTremoloSmoothing.value),
  ];
  DEFAULT_PARAMETERS.harmonicTremolo = values;
  harmonicTremoloRate.parentElement.querySelector("output").value =
    `${Number(harmonicTremoloRate.value).toFixed(2)} Hz`;
  harmonicTremoloDepth.parentElement.querySelector("output").value =
    `${Math.round(Number(harmonicTremoloDepth.value) * 100)}%`;
  harmonicTremoloCrossover.parentElement.querySelector("output").value =
    `${Math.round(Number(harmonicTremoloCrossover.value))} Hz`;
  harmonicTremoloBpm.parentElement.querySelector("output").value =
    `${harmonicTremoloBpm.value} BPM`;
  harmonicTremoloSmoothing.parentElement.querySelector("output").value =
    `${Number(harmonicTremoloSmoothing.value).toFixed(1)} ms`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "harmonic-tremolo", value: values });
  }
}

for (const control of [harmonicTremoloRate, harmonicTremoloDepth,
  harmonicTremoloCrossover, harmonicTremoloWaveform, harmonicTremoloSync,
  harmonicTremoloBpm, harmonicTremoloDivision, harmonicTremoloStereo,
  harmonicTremoloSmoothing]) {
  control.addEventListener("input", updateHarmonicTremolo);
  control.addEventListener("change", updateHarmonicTremolo);
}

function updateAutoSwellDelay() {
  const feedback = Number(autoSwellDelayFeedback.value) / 100;
  const mix = Number(autoSwellDelayMix.value) / 100;
  const mod = Number(autoSwellDelayMod.value) / 100;
  const tone = Number(autoSwellDelayTone.value) / 100;
  const sense = Number(autoSwellDelaySense.value);
  const values = [
    Number(autoSwellDelayTime.value),
    feedback,
    mix,
    Number(autoSwellDelayTimeMode.value),
    Number(autoSwellDelayTapDivision.value),
    Number(autoSwellDelayTempo.value),
    Number(autoSwellDelaySwell.value),
    mod,
    tone,
    sense,
    Number(autoSwellDelayTaper.value),
    autoSwellDelayDry.checked ? 1 : 0,
    autoSwellDelaySpillover.checked ? 1 : 0,
  ];
  DEFAULT_PARAMETERS.autoSwellDelay = values;
  autoSwellDelayTime.parentElement.querySelector("output").value =
    `${autoSwellDelayTime.value} ms`;
  autoSwellDelayFeedback.parentElement.querySelector("output").value =
    `${autoSwellDelayFeedback.value}%`;
  autoSwellDelayMix.parentElement.querySelector("output").value =
    `${autoSwellDelayMix.value}%`;
  autoSwellDelayTempo.parentElement.querySelector("output").value =
    `${autoSwellDelayTempo.value} BPM`;
  autoSwellDelaySwell.parentElement.querySelector("output").value =
    `${autoSwellDelaySwell.value} ms`;
  autoSwellDelayMod.parentElement.querySelector("output").value =
    `${autoSwellDelayMod.value}%`;
  autoSwellDelayTone.parentElement.querySelector("output").value =
    `${autoSwellDelayTone.value}%`;
  autoSwellDelaySense.parentElement.querySelector("output").value =
    `${sense < 0 ? "−" : ""}${Math.abs(sense)} dB`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "auto-swell-delay", value: values });
  }
}

for (const control of [autoSwellDelayTime, autoSwellDelayFeedback,
  autoSwellDelayMix, autoSwellDelayTimeMode, autoSwellDelayTapDivision,
  autoSwellDelayTempo, autoSwellDelaySwell, autoSwellDelayMod,
  autoSwellDelayTone, autoSwellDelaySense, autoSwellDelayTaper,
  autoSwellDelayDry, autoSwellDelaySpillover]) {
  control.addEventListener("input", updateAutoSwellDelay);
  control.addEventListener("change", updateAutoSwellDelay);
}
updateAutoSwellDelay();

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

function updateDimensionChorus() {
  const values = [
    Number(dimensionChorusVersion.value),
    Number(dimensionChorusMode.value),
    Number(dimensionChorusMix.value) / 100,
    Number(dimensionChorusLevel.value),
  ];
  DEFAULT_PARAMETERS.dimensionChorus = values;
  dimensionChorusMixValue.value = `${Math.round(Number(dimensionChorusMix.value))}%`;
  dimensionChorusLevelValue.value = `${Number(dimensionChorusLevel.value).toFixed(1)} dB`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "dimension-chorus", value: values });
  }
}

for (const control of [dimensionChorusVersion, dimensionChorusMode,
  dimensionChorusMix, dimensionChorusLevel]) {
  control.addEventListener("input", updateDimensionChorus);
  control.addEventListener("change", updateDimensionChorus);
}

function updateTripleDoubleChorus() {
  const values = [
    Number(tripleDoubleChorusPresetSwitch.value),
    Number(tripleDoubleChorusManualSwitch.value),
    Number(tripleDoubleChorusRate.value),
    Number(tripleDoubleChorusLeftDepth.value),
    Number(tripleDoubleChorusCenterDepth.value),
    Number(tripleDoubleChorusRightDepth.value),
    Number(tripleDoubleChorusFeedback.value),
    Number(tripleDoubleChorusSparkle.value),
    Number(tripleDoubleChorusStereoSwitch.value),
  ];
  DEFAULT_PARAMETERS.tripleDoubleChorus = values;
  tripleDoubleChorusRateValue.value = `${Number(tripleDoubleChorusRate.value).toFixed(2)} Hz`;
  tripleDoubleChorusLeftDepthValue.value = `${Number(tripleDoubleChorusLeftDepth.value).toFixed(1)} ms`;
  tripleDoubleChorusCenterDepthValue.value = `${Number(tripleDoubleChorusCenterDepth.value).toFixed(1)} ms`;
  tripleDoubleChorusRightDepthValue.value = `${Number(tripleDoubleChorusRightDepth.value).toFixed(1)} ms`;
  tripleDoubleChorusFeedbackValue.value = Number(tripleDoubleChorusFeedback.value).toFixed(2);
  tripleDoubleChorusSparkleValue.value = `${Math.round(Number(tripleDoubleChorusSparkle.value) * 100)}%`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "triple-double-chorus", value: values });
  }
}

for (const control of [tripleDoubleChorusPresetSwitch,
  tripleDoubleChorusManualSwitch, tripleDoubleChorusRate,
  tripleDoubleChorusLeftDepth, tripleDoubleChorusCenterDepth,
  tripleDoubleChorusRightDepth, tripleDoubleChorusFeedback,
  tripleDoubleChorusSparkle, tripleDoubleChorusStereoSwitch]) {
  control.addEventListener("input", updateTripleDoubleChorus);
  control.addEventListener("change", updateTripleDoubleChorus);
}

function updateFourBandResonator() {
  const controls = [...fourBandResonatorControls.querySelectorAll("[data-resonator-index]")];
  const values = Array(34).fill(0);
  for (const control of controls) {
    const index = Number(control.dataset.resonatorIndex);
    const value = Number(control.value);
    values[index] = value;
    const output = control.parentElement.querySelector("output");
    switch (control.dataset.displayFormat) {
      case "hz": output.value = `${value.toFixed(1)} Hz`; break;
      case "factor": output.value = `${value.toFixed(2)}×`; break;
      case "ratio": output.value = value.toFixed(2); break;
      case "percent": output.value = `${Math.round(value * 100)}%`; break;
      case "pan": output.value = value.toFixed(2); break;
      default: output.value = control.selectedOptions[0].textContent;
    }
  }
  DEFAULT_PARAMETERS.fourBandResonator = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "four-band-resonator", value: values });
  }
}

for (const control of fourBandResonatorControls.querySelectorAll("[data-resonator-index]")) {
  control.addEventListener("input", updateFourBandResonator);
  control.addEventListener("change", updateFourBandResonator);
}
updateFourBandResonator();

function updateDiatonicHarmonizer() {
  const values = diatonicHarmonizerControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.diatonicHarmonizer = values;
  const [mix, interval, , , tone, delay, feedback, , tempo] = values;
  diatonicHarmonizerControls[0].parentElement.querySelector("output").value =
    `${Math.round(mix * 100)}%`;
  diatonicHarmonizerControls[1].parentElement.querySelector("output").value =
    `${interval > 0 ? "+" : ""}${interval} degrees`;
  diatonicHarmonizerControls[4].parentElement.querySelector("output").value =
    `${Math.round(tone)} Hz`;
  diatonicHarmonizerControls[5].parentElement.querySelector("output").value =
    `${Math.round(delay)} ms`;
  diatonicHarmonizerControls[6].parentElement.querySelector("output").value =
    `${Math.round(feedback * 100)}%`;
  diatonicHarmonizerControls[8].parentElement.querySelector("output").value =
    `${Math.round(tempo)} BPM`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "diatonic-harmonizer", value: values });
  }
  updateDiatonicCustomScale();
}

function updateDiatonicCustomScale() {
  const noteCount = Number(diatonicCustomScaleCount.value);
  const values = [noteCount];
  let previous = -1;
  for (let index = 0; index < diatonicCustomScaleNotes.length; index += 1) {
    const control = diatonicCustomScaleNotes[index];
    const active = index < noteCount;
    control.closest("label").hidden = !active;
    if (!active) {
      values.push(0);
      continue;
    }
    const minimum = previous + 1;
    const maximum = 11 - (noteCount - 1 - index);
    const requested = Number(control.value);
    const sanitized = Math.max(minimum, Math.min(maximum,
      Number.isFinite(requested) ? Math.round(requested) : minimum));
    control.value = String(sanitized);
    values.push(sanitized);
    previous = sanitized;
  }
  DEFAULT_PARAMETERS.diatonicHarmonizerCustomScale = values;
  diatonicCustomScaleGroup.hidden = diatonicHarmonizerControls[3].value !== "13";
  if (workletNode) {
    workletNode.port.postMessage({ type: "diatonic-custom-scale", value: values });
  }
}

for (const control of diatonicHarmonizerControls) {
  control.addEventListener("input", updateDiatonicHarmonizer);
  control.addEventListener("change", updateDiatonicHarmonizer);
}
diatonicCustomScaleCount.addEventListener("input", updateDiatonicCustomScale);
diatonicCustomScaleCount.addEventListener("change", updateDiatonicCustomScale);
for (const control of diatonicCustomScaleNotes) {
  control.addEventListener("input", updateDiatonicCustomScale);
  control.addEventListener("change", updateDiatonicCustomScale);
}
updateDiatonicHarmonizer();

function updatePitchArpeggiator() {
  const values = pitchArpeggiatorControls.map((control, index) =>
    index === 0 ? Number(control.checked) : Number(control.value));
  DEFAULT_PARAMETERS.pitchArpeggiator = values;
  const [run, mix, level, , , steps, repeats, division, tempo, stepMs,
    amplitudeShape, amplitudeAlpha, panShape, panAlpha] = values;
  const triggerThreshold = values[30];
  pitchArpeggiatorControls[0].disabled = triggerThreshold > -90;
  pitchArpeggiatorControls[0].parentElement.querySelector("output").value =
    run ? "On" : "Off";
  pitchArpeggiatorControls[1].parentElement.querySelector("output").value =
    `${Math.round(mix * 100)}%`;
  pitchArpeggiatorControls[2].parentElement.querySelector("output").value =
    `${Math.round(level * 100)}%`;
  pitchArpeggiatorControls[5].parentElement.querySelector("output").value =
    String(Math.round(steps));
  pitchArpeggiatorControls[6].parentElement.querySelector("output").value =
    repeats === 0 ? "Infinite" : String(Math.round(repeats));
  pitchArpeggiatorControls[7].parentElement.querySelector("output").value =
    pitchArpeggiatorControls[7].selectedOptions[0].textContent;
  pitchArpeggiatorControls[8].parentElement.querySelector("output").value =
    `${Math.round(tempo)} BPM`;
  pitchArpeggiatorControls[9].parentElement.querySelector("output").value =
    `${Math.round(stepMs)} ms`;
  pitchArpeggiatorControls[10].parentElement.querySelector("output").value =
    pitchArpeggiatorControls[10].selectedOptions[0].textContent;
  pitchArpeggiatorControls[11].parentElement.querySelector("output").value =
    `${Math.round(amplitudeAlpha * 100)}%`;
  pitchArpeggiatorControls[12].parentElement.querySelector("output").value =
    pitchArpeggiatorControls[12].selectedOptions[0].textContent;
  pitchArpeggiatorControls[13].parentElement.querySelector("output").value =
    `${Math.round(panAlpha * 100)}%`;
  for (let step = 0; step < 16; step += 1) {
    const shift = values[14 + step];
    pitchArpeggiatorControls[14 + step].parentElement.querySelector("output").value =
      `${shift > 0 ? "+" : ""}${Math.round(shift)} degrees`;
  }
  pitchArpeggiatorControls[30].parentElement.querySelector("output").value =
    triggerThreshold <= -90 ? "Manual Run" : `${Math.round(triggerThreshold)} dBFS`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "pitch-arpeggiator", value: values });
  }
}

for (const control of pitchArpeggiatorControls) {
  control.addEventListener("input", updatePitchArpeggiator);
  control.addEventListener("change", updatePitchArpeggiator);
}
updatePitchArpeggiator();

function updateMonoGuitarSynth() {
  const values = monoGuitarSynthControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.monoGuitarSynth = values;
  const waveformNames = ["Sine", "Triangle", "Saw", "Square"];
  monoGuitarSynthControls[0].parentElement.querySelector("output").value =
    waveformNames[values[0]] || "Saw";
  monoGuitarSynthControls[1].parentElement.querySelector("output").value =
    `${Math.round(values[1] * 100)}%`;
  monoGuitarSynthControls[2].parentElement.querySelector("output").value =
    `${Math.round(values[2] * 100)}%`;
  monoGuitarSynthControls[3].parentElement.querySelector("output").value =
    `${Math.round(values[3])} dBFS`;
  monoGuitarSynthControls[4].parentElement.querySelector("output").value =
    `${Math.round(values[4])} ms`;
  monoGuitarSynthControls[5].parentElement.querySelector("output").value =
    `${Math.round(values[5])} ms`;
  monoGuitarSynthControls[6].parentElement.querySelector("output").value =
    `${Math.round(values[6])} ms`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "mono-guitar-synth", value: values });
  }
}

for (const control of monoGuitarSynthControls) {
  control.addEventListener("input", updateMonoGuitarSynth);
  control.addEventListener("change", updateMonoGuitarSynth);
}
updateMonoGuitarSynth();

function updateNoteGenerator() {
  const values = noteGeneratorControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.noteGenerator = values;
  const waveformNames = ["Sine", "Triangle", "Sawtooth", "Square"];
  noteGeneratorControls[0].parentElement.querySelector("output").value =
    `${values[0]} oscillator${values[0] === 1 ? "" : "s"}`;
  for (let voice = 0; voice < 3; voice += 1) {
    const offset = 1 + voice * 4;
    const waveform = noteGeneratorControls[offset];
    const note = noteGeneratorControls[offset + 1];
    const octave = noteGeneratorControls[offset + 2];
    const level = noteGeneratorControls[offset + 3];
    waveform.parentElement.querySelector("output").value =
      waveformNames[values[offset]] || "Sine";
    note.parentElement.querySelector("output").value = `MIDI ${values[offset + 1]}`;
    const octaveValue = values[offset + 2];
    octave.parentElement.querySelector("output").value = octaveValue === 0
      ? "0 octaves"
      : `${octaveValue > 0 ? "+" : "−"}${Math.abs(octaveValue)} octave${Math.abs(octaveValue) === 1 ? "" : "s"}`;
    level.parentElement.querySelector("output").value =
      `${Math.round(values[offset + 3] * 100)}%`;
  }
  const outputLevel = noteGeneratorControls[13];
  outputLevel.parentElement.querySelector("output").value =
    `${Math.round(values[13] * 100)}%`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "note-generator", value: values });
  }
}

for (const control of noteGeneratorControls) {
  control.addEventListener("input", updateNoteGenerator);
  control.addEventListener("change", updateNoteGenerator);
}
updateNoteGenerator();

function updateFourOscGenerator() {
  const values = fourOscGeneratorControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.fourOscGenerator = values;
  const waveformNames = ["Sine", "Triangle", "Sawtooth", "Square"];
  for (let voice = 0; voice < 4; voice += 1) {
    const offset = voice * 4;
    const controls = fourOscGeneratorControls.slice(offset, offset + 4);
    controls[0].parentElement.querySelector("output").value =
      waveformNames[values[offset]] || "Sine";
    controls[1].parentElement.querySelector("output").value =
      `MIDI ${values[offset + 1]}`;
    const octave = values[offset + 2];
    controls[2].parentElement.querySelector("output").value = octave === 0
      ? "0 octaves"
      : `${octave > 0 ? "+" : "−"}${Math.abs(octave)} octave${Math.abs(octave) === 1 ? "" : "s"}`;
    controls[3].parentElement.querySelector("output").value =
      `${Math.round(values[offset + 3] * 100)}%`;
  }
  fourOscGeneratorControls[16].parentElement.querySelector("output").value =
    `${Math.round(values[16] * 100)}%`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "four-osc-generator", value: values });
  }
}

for (const control of fourOscGeneratorControls) {
  control.addEventListener("input", updateFourOscGenerator);
  control.addEventListener("change", updateFourOscGenerator);
}
updateFourOscGenerator();

function updatePolyDetune() {
  const values = polyDetuneControls.map((control) => Number(control.value));
  values[0] = values[0] >= 1.5 ? 2 : 1;
  DEFAULT_PARAMETERS.polyDetune = values;
  const formatters = [
    (value) => `${value} tap${value === 1 ? "" : "s"}`,
    (value) => `${value.toFixed(2)} Hz`,
    (value) => `${value.toFixed(2)} ms`,
    (value) => `${Math.round(value * 100)}%`,
    (value) => `${Math.round(value * 100)}%`,
    (value) => `${value.toFixed(1)} dB`,
  ];
  polyDetuneControls.forEach((control, index) => {
    control.parentElement.querySelector("output").value = formatters[index](values[index]);
  });
  if (workletNode) {
    workletNode.port.postMessage({ type: "poly-detune", value: values });
  }
}

for (const control of polyDetuneControls) {
  control.addEventListener("input", updatePolyDetune);
  control.addEventListener("change", updatePolyDetune);
}
updatePolyDetune();

function updateFractalThreeVoiceSynth() {
  const values = fractalThreeVoiceSynthControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.fractalThreeVoiceSynth = values;
  const formatters = [
    (value, control) => control.selectedOptions[0]?.textContent || "Sine",
    (value, control) => control.selectedOptions[0]?.textContent || "Off",
    (value) => `${Math.round(value)} Hz`,
    (value) => `${value > 0 ? "+" : ""}${value} semitones`,
    (value) => `${value > 0 ? "+" : ""}${value} cents`,
    (value) => `${Math.round(value * 100)}%`,
    (value) => value === 0 ? "Center" : `${Math.round(Math.abs(value) * 100)}% ${value < 0 ? "left" : "right"}`,
    (value) => `${Math.round(value)} Hz`,
    (value) => value.toFixed(2),
    (value) => `${Math.round(value)} ms`,
    (value) => `${Math.round(value * 100)}%`,
  ];
  for (let voice = 0; voice < 3; voice += 1) {
    for (let parameter = 0; parameter < 11; parameter += 1) {
      const index = voice * 11 + parameter;
      const control = fractalThreeVoiceSynthControls[index];
      const output = control.parentElement.querySelector("output");
      if (output) output.value = formatters[parameter](values[index], control);
    }
  }
  const trackingModeControl = fractalThreeVoiceSynthControls[33];
  if (trackingModeControl) {
    const output = trackingModeControl.parentElement.querySelector("output");
    if (output) output.value = trackingModeControl.selectedOptions[0]?.textContent || "Guitar";
  }
  if (workletNode) {
    workletNode.port.postMessage({
      type: "fractal-three-voice-synth",
      value: values,
    });
  }
}

for (const control of fractalThreeVoiceSynthControls) {
  control.addEventListener("input", updateFractalThreeVoiceSynth);
  control.addEventListener("change", updateFractalThreeVoiceSynth);
}
updateFractalThreeVoiceSynth();

function updateEnhancer() {
  const values = enhancerControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.enhancer = values;
  const modeNames = ["Modern", "Classic", "Stereoizer"];
  const phaseNames = ["None", "Left", "Right", "Both"];
  for (const [index, names] of [[0, modeNames], [6, phaseNames]]) {
    enhancerControls[index].parentElement.querySelector("output").value =
      names[values[index]] || names[0];
  }
  const labels = [
    "", `${values[1].toFixed(2)}× width`, `${Math.round(values[2] * 100)}% depth`,
    `${Math.round(values[3])} Hz`, `${Math.round(values[4])} Hz`,
    `${values[5].toFixed(1)} ms`, "", `${Math.round(values[7] * 100)}%`,
    `${Math.round(values[8] * 100)}%`, `${values[9].toFixed(2)}`,
    `${Math.round(values[10] * 100)}%`,
  ];
  for (let control = 1; control < enhancerControls.length; control += 1) {
    if (!labels[control]) continue;
    enhancerControls[control].parentElement.querySelector("output").value =
      labels[control];
  }
  if (workletNode) {
    const targetChannels = processorChannelCount(
      decodedAudio?.numberOfChannels || preparedChannels,
      enabledEffects(),
    );
    if (targetChannels !== preparedChannels) {
      void rebuildProcessorForChannelLayout();
    } else {
      workletNode.port.postMessage({ type: "enhancer", value: values });
    }
  }
}

for (const control of enhancerControls) {
  control.addEventListener("input", updateEnhancer);
  control.addEventListener("change", updateEnhancer);
}
updateEnhancer();

function updateResonantDelay() {
  const values = resonantDelayControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.resonantDelay = values;
  resonantDelayControls[0].parentElement.querySelector("output").value = Math.round(values[0]) + " ms";
  resonantDelayControls[1].parentElement.querySelector("output").value = Math.round(values[1] * 100) + "%";
  resonantDelayControls[2].parentElement.querySelector("output").value = Math.round(values[2]) + " Hz";
  resonantDelayControls[3].parentElement.querySelector("output").value = values[3].toFixed(1) + " Q";
  resonantDelayControls[4].parentElement.querySelector("output").value = Math.round(values[4] * 100) + "%";
  if (workletNode) {
    workletNode.port.postMessage({ type: "resonant-delay", value: values });
  }
}
for (const control of resonantDelayControls) {
  control.addEventListener("input", updateResonantDelay);
  control.addEventListener("change", updateResonantDelay);
}
updateResonantDelay();

function updateGrainDelay() {
  const values = grainDelayControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.grainDelay = values;
  grainDelayControls[0].parentElement.querySelector("output").value = `${Math.round(values[0])} ms`;
  grainDelayControls[1].parentElement.querySelector("output").value = `${Math.round(values[1] * 100)}%`;
  grainDelayControls[2].parentElement.querySelector("output").value = `${Math.round(values[2] * 100)}%`;
  grainDelayControls[3].parentElement.querySelector("output").value = `${Math.round(values[3])} ms`;
  grainDelayControls[4].parentElement.querySelector("output").value = `${values[4]} grains`;
  grainDelayControls[5].parentElement.querySelector("output").value =
    `${values[5] > 0 ? "+" : ""}${values[5]} semitones`;
  grainDelayControls[6].parentElement.querySelector("output").value = `${Math.round(values[6])} ms`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "grain-delay", value: values });
  }
}

for (const control of grainDelayControls) {
  control.addEventListener("input", updateGrainDelay);
  control.addEventListener("change", updateGrainDelay);
}
updateGrainDelay();

function updatePitchFeedbackDelay() {
  const values = pitchFeedbackDelayControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.pitchFeedbackDelay = values;
  pitchFeedbackDelayControls[0].parentElement.querySelector("output").value =
    `${Math.round(values[0])} ms`;
  pitchFeedbackDelayControls[1].parentElement.querySelector("output").value =
    `${Math.round(values[1] * 100)}%`;
  pitchFeedbackDelayControls[2].parentElement.querySelector("output").value =
    `${Math.round(values[2] * 100)}%`;
  const pitchText = Number.isInteger(values[3]) ? String(values[3]) : values[3].toFixed(1);
  pitchFeedbackDelayControls[3].parentElement.querySelector("output").value =
    `${values[3] > 0 ? "+" : ""}${pitchText} semitones`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "pitch-feedback-delay", value: values });
  }
}

for (const control of pitchFeedbackDelayControls) {
  control.addEventListener("input", updatePitchFeedbackDelay);
  control.addEventListener("change", updatePitchFeedbackDelay);
}
updatePitchFeedbackDelay();

function updateChromaticPitchDelay() {
  const [pitch, time, feedback, tone, mix, timeMode, tempo, division, spillover] =
    chromaticPitchDelayControls.map((control, index) =>
      index === 8 ? Number(control.checked) : Number(control.value));
  const values = [pitch, time, feedback / 100, tone / 100, mix / 100,
    timeMode, tempo, division, spillover];
  chromaticPitchDelayControls[1].disabled = timeMode === 1;
  chromaticPitchDelayControls[0].parentElement.querySelector("output").value =
    `${pitch > 0 ? "+" : ""}${pitch.toFixed(0)} semitones`;
  chromaticPitchDelayControls[1].parentElement.querySelector("output").value =
    `${time.toFixed(0)} ms${timeMode === 1 ? " · saved" : ""}`;
  chromaticPitchDelayControls[2].parentElement.querySelector("output").value =
    `${Math.round(feedback)}%`;
  chromaticPitchDelayControls[3].parentElement.querySelector("output").value =
    `${Math.round(tone)}%`;
  chromaticPitchDelayControls[4].parentElement.querySelector("output").value =
    `${Math.round(mix)}%`;
  chromaticPitchDelayControls[6].parentElement.querySelector("output").value =
    `${tempo.toFixed(0)} BPM`;
  chromaticPitchDelayControls[7].parentElement.querySelector("output").value =
    chromaticPitchDelayControls[7].selectedOptions[0].textContent;
  DEFAULT_PARAMETERS.chromaticPitchDelay = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "chromatic-pitch-delay", value: values });
  }
}

for (const control of chromaticPitchDelayControls) {
  control.addEventListener("input", updateChromaticPitchDelay);
  control.addEventListener("change", updateChromaticPitchDelay);
}
updateChromaticPitchDelay();

function updateFeedbackGenerator() {
  const values = feedbackGeneratorControls.map((control) => Number(control.value));
  DEFAULT_PARAMETERS.feedbackGenerator = values;
  const typeNames = ["Octave down", "Unison", "Octave up"];
  feedbackGeneratorControls[0].parentElement.querySelector("output").value =
    typeNames[values[0]] || "Unison";
  feedbackGeneratorControls[1].parentElement.querySelector("output").value =
    `${Math.round(values[1] * 100)}%`;
  feedbackGeneratorControls[2].parentElement.querySelector("output").value =
    `${Math.round(values[2])} ms`;
  feedbackGeneratorControls[3].parentElement.querySelector("output").value =
    `${Math.round(values[3])} ms`;
  feedbackGeneratorControls[4].parentElement.querySelector("output").value =
    values[4] === 1 ? "On" : "Off";
  feedbackGeneratorControls[5].parentElement.querySelector("output").value =
    `${Math.round(values[5] * 100)}%`;
  feedbackGeneratorControls[6].parentElement.querySelector("output").value =
    `${Math.round(values[6])} dBFS`;
  if (workletNode) {
    workletNode.port.postMessage({ type: "feedback-generator", value: values });
  }
}

for (const control of feedbackGeneratorControls) {
  control.addEventListener("input", updateFeedbackGenerator);
  control.addEventListener("change", updateFeedbackGenerator);
}
updateFeedbackGenerator();

function updateShimmerReverb() {
  const values = [
    Number(shimmerReverbControls[0].value),
    Number(shimmerReverbControls[1].value),
    Number(shimmerReverbControls[2].value) / 100,
    Number(shimmerReverbControls[3].value) / 100,
    Number(shimmerReverbControls[4].value),
    Number(shimmerReverbControls[5].value),
    Number(shimmerReverbControls[6].value) / 100,
    Number(shimmerReverbControls[7].value) / 100,
    Number(shimmerReverbControls[8].value),
  ];
  DEFAULT_PARAMETERS.shimmerReverb = values;
  const formats = [
    (value) => `${value.toFixed(2)} s`,
    (value) => `${Math.round(value)} Hz`,
    (value) => `${Math.round(value * 100)}%`,
    (value) => `${Math.round(value * 100)}%`,
    (value) => `${Math.round(value)} Hz`,
    (value) => `${value > 0 ? "+" : ""}${Number.isInteger(value) ? value : value.toFixed(1)} semitones`,
    (value) => `${Math.round(value * 100)}%`,
    (value) => `${Math.round(value * 100)}%`,
    (value) => `${value.toFixed(1)} dB`,
  ];
  shimmerReverbControls.forEach((control, index) => {
    control.parentElement.querySelector("output").value = formats[index](values[index]);
  });
  if (workletNode) {
    workletNode.port.postMessage({ type: "shimmer-reverb", value: values });
  }
}

for (const control of shimmerReverbControls) {
  control.addEventListener("input", updateShimmerReverb);
  control.addEventListener("change", updateShimmerReverb);
}
updateShimmerReverb();

function updateCelestialReverb() {
  const values = [
    Number(celestialReverbControls[0].value) / 100,
    Number(celestialReverbControls[1].value),
    Number(celestialReverbControls[2].value),
    Number(celestialReverbControls[3].value),
    Number(celestialReverbControls[4].value),
    Number(celestialReverbControls[5].value),
    Number(celestialReverbControls[6].value) / 100,
    Number(celestialReverbControls[7].checked),
  ];
  DEFAULT_PARAMETERS.celestialReverb = values;
  const formats = [
    (value) => `${Math.round(value * 100)}%`,
    (value) => `${value.toFixed(2)} s`,
    (value) => `${value > 0 ? "+" : ""}${Number.isInteger(value) ? value : value.toFixed(1)} semitones`,
    (value) => `${value > 0 ? "+" : ""}${Number.isInteger(value) ? value : value.toFixed(1)} semitones`,
    (value) => `${value.toFixed(2)} s`,
    (value) => `${Math.round(value)} Hz`,
    (value) => `${Math.round(value * 100)}%`,
    (value) => value === 1 ? "On" : "Off",
  ];
  celestialReverbControls.forEach((control, index) => {
    control.parentElement.querySelector("output").value = formats[index](values[index]);
  });
  if (workletNode) {
    workletNode.port.postMessage({ type: "celestial-reverb", value: values });
  }
}

for (const control of celestialReverbControls) {
  control.addEventListener("input", updateCelestialReverb);
  control.addEventListener("change", updateCelestialReverb);
}
updateCelestialReverb();

function updateCloudReverb() {
  const values = [
    Number(cloudReverbControls[0].value) / 100,
    Number(cloudReverbControls[1].value) / 100,
    Number(cloudReverbControls[2].value),
    Number(cloudReverbControls[3].value) / 100,
    Number(cloudReverbControls[4].value),
    Number(cloudReverbControls[5].value),
    Number(cloudReverbControls[6].value),
    Number(cloudReverbControls[7].value) / 100,
    Number(cloudReverbControls[8].checked),
  ];
  DEFAULT_PARAMETERS.cloudReverb = values;
  const formats = [
    (value) => `${Math.round(value * 100)}%`,
    (value) => `${Math.round(value * 100)}%`,
    (value) => `${value.toFixed(2)} s`,
    (value) => `${Math.round(value * 100)}%`,
    (value) => `${Math.round(value)} Hz`,
    (value) => `${Math.round(value)} Hz`,
    (value) => `${value.toFixed(2)} Hz`,
    (value) => `${Math.round(value * 100)}%`,
    (value) => value === 1 ? "On" : "Off",
  ];
  cloudReverbControls.forEach((control, index) => {
    control.parentElement.querySelector("output").value = formats[index](values[index]);
  });
  if (workletNode) {
    workletNode.port.postMessage({ type: "cloud-reverb", value: values });
  }
}

for (const control of cloudReverbControls) {
  control.addEventListener("input", updateCloudReverb);
  control.addEventListener("change", updateCloudReverb);
}
updateCloudReverb();

function updateConvolutionReverb() {
  const values = [
    Number(convolutionReverbControls[0].value) / 100,
    Number(convolutionReverbControls[1].value),
    Number(convolutionReverbControls[2].value),
    Number(convolutionReverbControls[3].value),
    Number(convolutionReverbTails.checked),
  ];
  DEFAULT_PARAMETERS.convolutionReverb = values;
  convolutionReverbControls[0].parentElement.querySelector("output").value =
    `${Math.round(values[0] * 100)}%`;
  convolutionReverbControls[1].parentElement.querySelector("output").value =
    `${values[1].toFixed(1)} dB`;
  convolutionReverbControls[2].parentElement.querySelector("output").value =
    values[2] === 0 ? "Off" : `${Math.round(values[2])} Hz`;
  convolutionReverbControls[3].parentElement.querySelector("output").value =
    values[3] === 0 ? "Off" : `${Math.round(values[3])} Hz`;
  convolutionReverbTails.parentElement.querySelector("output").value =
    convolutionReverbTails.checked ? "On" : "Off";
  if (workletNode) {
    workletNode.port.postMessage({ type: "convolution-reverb", value: values });
  }
}

for (const control of convolutionReverbControls) {
  control.addEventListener("input", updateConvolutionReverb);
  control.addEventListener("change", updateConvolutionReverb);
}
convolutionReverbTails.addEventListener("input", updateConvolutionReverb);
convolutionReverbTails.addEventListener("change", updateConvolutionReverb);
updateConvolutionReverb();

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
  const bbd = delayCharacter.value === "bbd-classic" ||
    delayCharacter.value === "bbd-mod";
  const bbdMode = delayCharacter.value === "bbd-mod";
  const tapeEnabled = delayCharacter.value === "tape";
  const tape = [
    tapeEnabled ? 1 : 0,
    ...delayTapeInputs.map(({ input }) => Number(input.value)),
  ];
  DEFAULT_PARAMETERS.delayBbd = bbd;
  DEFAULT_PARAMETERS.delayBbdMode = bbdMode;
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
      value: { bbd, bbdMode,
        tape: [enabled, saturation, wowDepth, wowRate, flutterDepth, flutterRate] },
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

function updateFiveBandEQ() {
  const values = [];
  for (const band of fiveBandEqControls) {
    const frequency = Number(band.frequency.value);
    const gain = Number(band.gain.value);
    const q = Number(band.q.value);
    const shape = Number(band.shape.value);
    const slope = band.slope ? Number(band.slope.value) : 12;
    const isBlocking = shape === 3 || shape === 4;
    const usesQ = [0, 5, 6, 7].includes(shape);
    band.frequencyValue.value = `${frequency.toFixed(0)} Hz`;
    band.gainValue.value = `${gain.toFixed(1)} dB`;
    band.qValue.value = q.toFixed(2);
    band.gain.disabled = isBlocking;
    band.q.disabled = !usesQ;
    if (band.slope) band.slope.disabled = !isBlocking;
    if (band.slopeValue) band.slopeValue.value = `${slope} dB/oct`;
    values.push(frequency, gain, q, shape, slope);
  }
  values.push(Number(fiveBandEqSolo.value));
  DEFAULT_PARAMETERS.fiveBandEq = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "five-band-eq", value: values });
  }
}

for (const band of fiveBandEqControls) {
  for (const control of [band.frequency, band.gain, band.q, band.shape, band.slope]
    .filter(Boolean)) {
    control.addEventListener("input", updateFiveBandEQ);
    control.addEventListener("change", updateFiveBandEQ);
  }
}
fiveBandEqSolo.addEventListener("change", updateFiveBandEQ);
updateFiveBandEQ();

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
    Number(ringModType.value),
    Number(ringModFrequency.value),
    Number(ringModMultiplier.value),
    Number(ringModHighCut.value),
    Number(ringModMix.value) / 100,
    ringModPitchTracking.checked ? 1 : 0,
    Number(ringModTrackingMode.value),
  ];
  ringModFrequency.disabled = ringModPitchTracking.checked;
  ringModFrequencyValue.value = `${values[1].toFixed(0)} Hz`;
  ringModMultiplierValue.value = `${values[2].toFixed(2)}×`;
  ringModHighCutValue.value = `${values[3].toFixed(0)} Hz`;
  ringModMixValue.value = `${Math.round(values[4] * 100)}%`;
  ringModPitchTracking.parentElement.querySelector("output").value =
    ringModPitchTracking.checked ? "On" : "Off";
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

function updateAshevillePattrn() {
  const bandLevels = ashevillePattrnBandLevels.map(({ input }) =>
    Number(input.value));
  const values = [
    Number(ashevillePattrnBank.value),
    Number(ashevillePattrnPattern.value),
    ...bandLevels,
    Number(ashevillePattrnDrive.value),
    Number(ashevillePattrnOutput.value),
    Number(ashevillePattrnMix.value) / 100,
    Number(ashevillePattrnRate.value),
    Number(ashevillePattrnQ.value),
    Number(ashevillePattrnEnvelope.value),
    Number(ashevillePattrnSmoothing.value),
  ];
  ashevillePattrnDriveValue.value = `${values[10].toFixed(1)} dB`;
  ashevillePattrnOutputValue.value = `${values[11].toFixed(1)} dB`;
  ashevillePattrnMixValue.value = `${Math.round(values[12] * 100)}%`;
  ashevillePattrnRateValue.value = `${values[13].toFixed(2)} Hz`;
  ashevillePattrnQValue.value = values[14].toFixed(2);
  ashevillePattrnEnvelopeValue.value = `${Math.round(values[15] * 100)}%`;
  ashevillePattrnSmoothingValue.value = `${values[16].toFixed(0)} ms`;
  ashevillePattrnBandLevels.forEach(({ output }, index) => {
    output.value = `${Math.round(bandLevels[index] * 100)}%`;
  });
  DEFAULT_PARAMETERS.ashevillePattrn = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "asheville-pattrn", value: values });
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
  const morphMode = Number(formantFilterMode.value);
  const values = [
    Number(formantFilterVowel.value),
    Number(formantFilterQ.value),
    Number(formantFilterMix.value) / 100,
    Number(formantFilterLevel.value),
    morphMode,
    Number(formantFilterStartVowel.value),
    Number(formantFilterMiddleVowel.value),
    Number(formantFilterEndVowel.value),
    Number(formantFilterControl.value),
    Number(formantFilterBalance.value),
  ];
  const vowels = ["A", "E", "I", "O", "U"];
  const morphVowels = ["AAA", "EEE", "III", "OHH", "OOO", "EHH", "AHH", "AWW", "UHH", "ERR"];
  const lower = Math.min(Math.floor(values[0]), vowels.length - 1);
  const upper = Math.min(lower + 1, vowels.length - 1);
  const fraction = values[0] - lower;
  const start = morphVowels[Math.round(values[5])];
  const middle = morphVowels[Math.round(values[6])];
  const end = morphVowels[Math.round(values[7])];
  formantFilterVowel.closest("label").hidden = morphMode >= 0.5;
  formantFilterMorphControls.hidden = morphMode < 0.5;
  formantFilterVowelValue.value = morphMode >= 0.5
    ? `${start} → ${middle} → ${end}`
    : lower === upper || fraction < 0.005
      ? vowels[lower]
      : `${vowels[lower]} → ${vowels[upper]} ${Math.round(fraction * 100)}%`;
  formantFilterControlValue.value = `${Math.round(values[8])}%`;
  formantFilterQValue.value = values[1].toFixed(2);
  formantFilterMixValue.value = `${Math.round(values[2] * 100)}%`;
  formantFilterLevelValue.value = `${values[3].toFixed(1)} dB`;
  formantFilterBalanceValue.value = `${Math.round(values[9])}%`;
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

function updateBitCrusher() {
  const values = [
    Number(bitCrusherBitDepth.value),
    Number(bitCrusherRateDivisor.value),
    Number(bitCrusherMix.value) / 100,
  ];
  bitCrusherBitDepthValue.value = `${values[0]} bits`;
  bitCrusherRateDivisorValue.value = `÷${values[1]}`;
  bitCrusherMixValue.value = `${Math.round(values[2] * 100)}%`;
  DEFAULT_PARAMETERS.bitCrusher = values;
  if (workletNode) {
    workletNode.port.postMessage({ type: "bit-crusher", value: values });
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
  const timeMode = Number(duckingDelayTimeMode.value);
  const tempoBpm = Number(duckingDelayTempo.value);
  const tapDivision = Number(duckingDelayTapDivision.value);
  const lowCutHz = Number(duckingDelayLowCut.value);
  const highCutHz = Number(duckingDelayHighCut.value);
  const modRateHz = Number(duckingDelayModRate.value);
  const modDepthMs = Number(duckingDelayModDepth.value);
  const modulationMode = Number(duckingDelayModMode.value);
  const values = [
    Number(duckingDelayTime.value),
    Number(duckingDelayFeedback.value) / 100,
    Number(duckingDelayMix.value) / 100,
    Number(duckingDelayThreshold.value),
    Number(duckingDelayAttenuation.value),
    Number(duckingDelayRelease.value),
    duckingDelaySpillover.checked ? 1 : 0,
    timeMode,
    tempoBpm,
    tapDivision,
    lowCutHz,
    highCutHz,
    modRateHz,
    modDepthMs,
    modulationMode,
  ];
  duckingDelayTime.disabled = timeMode === 1;
  duckingDelayTimeValue.value = `${values[0].toFixed(0)} ms${timeMode === 1 ? " · saved" : ""}`;
  duckingDelayTempoValue.value = `${tempoBpm.toFixed(0)} BPM`;
  duckingDelayFeedbackValue.value = `${Math.round(values[1] * 100)}%`;
  duckingDelayMixValue.value = `${Math.round(values[2] * 100)}%`;
  duckingDelayThresholdValue.value = `${values[3].toFixed(0)} dB`;
  duckingDelayAttenuationValue.value = `${values[4].toFixed(0)} dB`;
  duckingDelayReleaseValue.value = `${values[5].toFixed(0)} ms`;
  duckingDelayLowCutValue.value = lowCutHz === 0 ? "Off" : `${lowCutHz.toFixed(0)} Hz`;
  duckingDelayHighCutValue.value = highCutHz === 0 ? "Off" : `${highCutHz.toFixed(0)} Hz`;
  duckingDelayModRateValue.value = `${modRateHz.toFixed(2)} Hz`;
  duckingDelayModDepthValue.value = `${modDepthMs.toFixed(1)} ms`;
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
  const timeMode = Number(filteredDelayTimeMode.value);
  const tempoBpm = Number(filteredDelayTempoBpm.value);
  const division = Number(filteredDelayDivision.value);
  const values = [
    Number(filteredDelayTime.value),
    Number(filteredDelayFeedback.value) / 100,
    Number(filteredDelayMix.value) / 100,
    Number(filteredDelayLowCut.value),
    Number(filteredDelayHighCut.value),
    timeMode,
    tempoBpm,
    division,
  ];
  filteredDelayTime.disabled = timeMode === 1;
  filteredDelayTempoBpm.disabled = timeMode !== 1;
  filteredDelayDivision.disabled = timeMode !== 1;
  filteredDelayTimeValue.value = timeMode === 1
    ? `${filteredDelayDivision.selectedOptions[0].text} · ${tempoBpm.toFixed(0)} BPM`
    : `${values[0].toFixed(0)} ms`;
  filteredDelayTempoBpmValue.value = `${tempoBpm.toFixed(0)} BPM`;
  filteredDelayFeedbackValue.value = `${Math.round(values[1] * 100)}%`;
  filteredDelayMixValue.value = `${Math.round(values[2] * 100)}%`;
  filteredDelayLowCutValue.value = `${values[3].toFixed(0)} Hz`;
  filteredDelayHighCutValue.value = `${values[4].toFixed(0)} Hz`;
  DEFAULT_PARAMETERS.filteredDelay = values;
  if (workletNode) workletNode.port.postMessage({ type: "filtered-delay", value: values });
}

function updateSweepDelay() {
  const rateMode = Number(sweepDelayRateMode.value);
  const tempoBpm = Number(sweepDelayTempoBpm.value);
  const division = Number(sweepDelayDivision.value);
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
    Number(sweepDelayFilterType.value),
    rateMode,
    tempoBpm,
    division,
  ];
  sweepDelayRate.disabled = rateMode === 1;
  sweepDelayTempoBpm.disabled = rateMode !== 1;
  sweepDelayDivision.disabled = rateMode !== 1;
  sweepDelayTimeValue.value = `${values[0].toFixed(0)} ms`;
  sweepDelayFeedbackValue.value = `${Math.round(values[1] * 100)}%`;
  sweepDelayMixValue.value = `${Math.round(values[2] * 100)}%`;
  sweepDelayStartValue.value = `${values[3].toFixed(0)} Hz`;
  sweepDelayStopValue.value = `${values[4].toFixed(0)} Hz`;
  sweepDelayResonanceValue.value = values[5].toFixed(1);
  sweepDelayRateValue.value = rateMode === 1
    ? `${sweepDelayDivision.selectedOptions[0].text} · ${tempoBpm.toFixed(0)} BPM`
    : `${values[6].toFixed(2)} Hz`;
  sweepDelayTempoBpmValue.value = `${tempoBpm.toFixed(0)} BPM`;
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
ringModType.addEventListener("change", updateRingModulator);
ringModPitchTracking.addEventListener("change", updateRingModulator);
ringModTrackingMode.addEventListener("change", updateRingModulator);
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
for (const input of [ashevillePattrnDrive, ashevillePattrnOutput,
  ashevillePattrnMix, ashevillePattrnRate, ashevillePattrnQ,
  ashevillePattrnEnvelope, ashevillePattrnSmoothing,
  ...ashevillePattrnBandLevels.map(({ input: level }) => level)]) {
  input.addEventListener("input", updateAshevillePattrn);
  input.addEventListener("change", updateAshevillePattrn);
}
for (const input of [ashevillePattrnBank, ashevillePattrnPattern]) {
  input.addEventListener("change", updateAshevillePattrn);
}
for (const input of [notchFilterFrequency, notchFilterQ, notchFilterMix]) {
  input.addEventListener("input", updateNotchFilter);
  input.addEventListener("change", updateNotchFilter);
}
for (const input of [formantFilterVowel, formantFilterMode,
  formantFilterStartVowel, formantFilterMiddleVowel, formantFilterEndVowel,
  formantFilterControl, formantFilterQ, formantFilterMix, formantFilterLevel,
  formantFilterBalance]) {
  input.addEventListener("input", updateFormantFilter);
  input.addEventListener("change", updateFormantFilter);
}
for (const input of [harmonicResonatorRoot, harmonicResonatorDecay,
  harmonicResonatorBrightness, harmonicResonatorMix]) {
  input.addEventListener("input", updateHarmonicResonator);
  input.addEventListener("change", updateHarmonicResonator);
}
for (const input of [bitCrusherBitDepth, bitCrusherRateDivisor, bitCrusherMix]) {
  input.addEventListener("input", updateBitCrusher);
  input.addEventListener("change", updateBitCrusher);
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
  duckingDelayThreshold, duckingDelayAttenuation, duckingDelayRelease,
  duckingDelayTempo, duckingDelayLowCut, duckingDelayHighCut,
  duckingDelayModRate, duckingDelayModDepth]) {
  input.addEventListener("input", updateDuckingDelay);
  input.addEventListener("change", updateDuckingDelay);
}
for (const input of [duckingDelayTimeMode, duckingDelayTapDivision,
  duckingDelaySpillover, duckingDelayModMode]) {
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
for (const input of [filteredDelayTimeMode, filteredDelayTempoBpm,
  filteredDelayDivision]) {
  input.addEventListener("input", updateFilteredDelay);
  input.addEventListener("change", updateFilteredDelay);
}
sweepDelayWaveform.addEventListener("change", updateSweepDelay);
sweepDelayFilterType.addEventListener("change", updateSweepDelay);
for (const input of [sweepDelayRateMode, sweepDelayDivision]) {
  input.addEventListener("change", updateSweepDelay);
}
for (const input of [sweepDelayTempoBpm]) {
  input.addEventListener("input", updateSweepDelay);
  input.addEventListener("change", updateSweepDelay);
}
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
updateBitCrusher();
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

async function waitForLooperStatus(prefix) {
  const started = performance.now();
  while (!looperStatus.value.startsWith(prefix)) {
    if (performance.now() - started > 3000) {
      throw new Error(`Timed out waiting for Phrase Looper status: ${looperStatus.value}`);
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
}

async function runPhraseLooperAutoRecordBrowserSmokeTest() {
  const previousEffectState = enabledEffects();
  const previousOptions = {
    reverse: looperReverse.checked,
    halfSpeed: looperHalfSpeed.checked,
    overdubLevel: looperOverdubLevel.value,
    autoRecord: looperAutoRecord.checked,
    thresholdDb: looperAutoRecordThreshold.value,
  };
  const testEnabled = previousEffectState.map((_, index) =>
    index === 0 || index === 60);
  let loopCleared = false;

  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Phrase Looper Auto Record smoke test requires the browser audio chain and guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);

    looperReverse.checked = false;
    looperHalfSpeed.checked = false;
    looperOverdubLevel.value = "100";
    looperAutoRecordThreshold.value = "-90";
    looperAutoRecord.checked = true;
    updateLooperOptions();

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({
        type: "enable",
        index,
        value: testEnabled[index],
      });
    });

    const samples = decodedAudio.getChannelData(0);
    const firstActiveFrame = samples.findIndex((sample) => Math.abs(sample) > 1e-4);
    if (firstActiveFrame < 0) {
      throw new Error("Phrase Looper Auto Record guitar fixture contains no threshold-crossing sample.");
    }

    const source = audioContext.createBufferSource();
    source.buffer = decodedAudio;
    source.loop = true;
    source.connect(workletNode);
    activeSource = source;
    playbackOffset = 0;
    playbackStartedAt = audioContext.currentTime;
    workletNode.port.postMessage({ type: "active", value: true });
    source.start(0, firstActiveFrame / decodedAudio.sampleRate);
    await new Promise((resolve) => setTimeout(resolve, 150));
    stopSource(false);

    looperStatus.value = "Waiting for threshold-triggered recording";
    sendLooperCommand(3);
    await waitForLooperStatus("Stopped · ");
    if (looperStatus.value.includes("no loop") ||
        looperStatus.value.includes("command unavailable")) {
      throw new Error(`Phrase Looper did not auto-record the local guitar signal: ${looperStatus.value}`);
    }
    const recordedStatus = looperStatus.value;

    sendLooperCommand(6);
    await waitForLooperStatus("Stopped · no loop");
    loopCleared = true;
    return { recordedStatus, clearedStatus: looperStatus.value, thresholdDb: -90 };
  } finally {
    stopSource(false);
    if (workletNode && !loopCleared) sendLooperCommand(6);
    looperReverse.checked = previousOptions.reverse;
    looperHalfSpeed.checked = previousOptions.halfSpeed;
    looperOverdubLevel.value = previousOptions.overdubLevel;
    looperAutoRecord.checked = previousOptions.autoRecord;
    looperAutoRecordThreshold.value = previousOptions.thresholdDb;
    updateLooperOptions();
    if (workletNode) {
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable",
          index,
          value: previousEffectState[index],
        });
      });
    }
  }
}

async function waitForDeluxeLooperStatus(predicate, description, timeoutMs = 4000) {
  const started = performance.now();
  while (!predicate(deluxeLooperStatus.value)) {
    if (performance.now() - started > timeoutMs) {
      throw new Error(`Timed out waiting for Deluxe Looper ${description}: ${deluxeLooperStatus.value}`);
    }
    if (workletNode) {
      workletNode.port.postMessage({ type: "deluxe-looper-status-request" });
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  return deluxeLooperStatus.value;
}

async function runDeluxeLooperBrowserSmokeTest() {
  const previousEffectState = enabledEffects();
  const previousControls = {
    reverse: deluxeLooperReverse.checked,
    halfSpeed: deluxeLooperHalfSpeed.checked,
    playback: deluxeLooperPlayback.value,
    decay: deluxeLooperOverdubDecay.value,
    lowCut: deluxeLooperLowCut.value,
    highCut: deluxeLooperHighCut.value,
  };
  const testEnabled = previousEffectState.map((_, index) => index === 0 || index === 98);
  let loopCleared = false;

  const command = async (id, prefix, description) => {
    const button = document.querySelector(`[data-deluxe-looper-command="${id}"]`);
    if (!button) throw new Error(`Deluxe Looper ${description} button is missing.`);
    deluxeLooperStatus.value = `Waiting for ${description}`;
    button.click();
    return waitForDeluxeLooperStatus((value) => value.startsWith(prefix), description);
  };

  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Deluxe Looper smoke test requires the ready browser chain and local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);

    deluxeLooperReverse.checked = false;
    deluxeLooperHalfSpeed.checked = false;
    deluxeLooperPlayback.value = "100";
    deluxeLooperOverdubDecay.value = "0";
    deluxeLooperLowCut.value = "0";
    deluxeLooperHighCut.value = "0";
    updateDeluxeLooperOptions();

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });

    const samples = decodedAudio.getChannelData(0);
    const firstActiveFrame = samples.findIndex((sample) => Math.abs(sample) > 1e-4);
    if (firstActiveFrame < 0) throw new Error("Deluxe Looper guitar fixture contains no active sample.");
    const source = audioContext.createBufferSource();
    source.buffer = decodedAudio;
    source.loop = true;
    source.connect(workletNode);
    activeSource = source;
    playbackOffset = 0;
    playbackStartedAt = audioContext.currentTime;
    workletNode.port.postMessage({ type: "active", value: true });
    source.start(0, firstActiveFrame / decodedAudio.sampleRate);

    await command(0, "Recording", "recording");
    await new Promise((resolve) => setTimeout(resolve, 150));
    const recordedStatus = await command(3, "Playing ·", "finish recording into playback");
    if (recordedStatus.includes("no loop") || recordedStatus.includes("command unavailable")) {
      throw new Error(`Deluxe Looper did not retain the local guitar signal: ${recordedStatus}`);
    }
    await command(3, "Stopped ·", "stop recorded loop playback");
    const playingStatus = await command(1, "Playing", "playback");
    await command(3, "Stopped ·", "stop after playback");
    await command(6, "Stopped · no loop", "clear loop");
    loopCleared = true;
    return { recordedStatus, playingStatus, clearedStatus: deluxeLooperStatus.value };
  } finally {
    stopSource(false);
    if (workletNode && !loopCleared) {
      const clearButton = document.querySelector('[data-deluxe-looper-command="6"]');
      clearButton?.click();
    }
    deluxeLooperReverse.checked = previousControls.reverse;
    deluxeLooperHalfSpeed.checked = previousControls.halfSpeed;
    deluxeLooperPlayback.value = previousControls.playback;
    deluxeLooperOverdubDecay.value = previousControls.decay;
    deluxeLooperLowCut.value = previousControls.lowCut;
    deluxeLooperHighCut.value = previousControls.highCut;
    updateDeluxeLooperOptions();
    if (workletNode) {
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable",
          index,
          value: previousEffectState[index],
        });
      });
    }
  }
}

async function waitForShufflingLooperStatus(predicate, description, timeoutMs = 4000) {
  const started = performance.now();
  while (!predicate(shufflingLooperStatus.value)) {
    if (performance.now() - started > timeoutMs) {
      throw new Error(`Timed out waiting for Shuffling Looper ${description}: ${shufflingLooperStatus.value}`);
    }
    if (workletNode) {
      workletNode.port.postMessage({ type: "shuffling-looper-status-request" });
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  return shufflingLooperStatus.value;
}

async function runShufflingLooperBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = shufflingLooperControls.map((control) => control.value);
  const testEnabled = previousEffectState.map((_, index) => index === 0 || index === 65);
  const parseStatus = (value) => {
    const match = /^(Stopped|Recording|Playing) · (no loop|([\d.]+) s) · (\d+) steps/.exec(value);
    if (!match) return null;
    return {
      state: match[1],
      hasLoop: match[2] !== "no loop",
      seconds: Number(match[3] || 0),
      steps: Number(match[4]),
    };
  };
  const waitForCommand = async (command, predicate, description) => {
    shufflingLooperStatus.value = `Waiting for ${description}`;
    const button = document.querySelector(
      `[data-shuffling-looper-command="${command}"]`,
    );
    if (!button) throw new Error(`Shuffling Looper ${description} button is missing.`);
    button.click();
    return waitForShufflingLooperStatus((value) => {
      const status = parseStatus(value);
      return status !== null && !value.includes("command unavailable") && predicate(status);
    }, description);
  };
  let loopCleared = false;

  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Shuffling Looper smoke test requires the ready browser audio chain and guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);

    [shufflingLooperSlices.value, shufflingLooperSequenceLength.value,
      shufflingLooperShuffle.value, shufflingLooperPitchProbability.value,
      shufflingLooperReverseProbability.value, shufflingLooperRepeatProbability.value,
      shufflingLooperSmoothing.value, shufflingLooperSequenceDrift.value,
      shufflingLooperPlayback.value, shufflingLooperLowCut.value,
      shufflingLooperHighCut.value, shufflingLooperInterval1.value,
      shufflingLooperInterval2.value] = [
      "4", "4", "0", "0", "0", "0", "0", "0", "100", "0", "0", "-12", "12",
    ];
    updateShufflingLooperOptions();

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({
        type: "enable",
        index,
        value: testEnabled[index],
      });
    });

    const recordingStatus = await waitForCommand(
      0,
      (status) => status.state === "Recording",
      "recording state",
    );
    const firstActiveFrame = decodedAudio.getChannelData(0)
      .findIndex((sample) => Math.abs(sample) > 1e-4);
    const recordOffset = firstActiveFrame >= 0
      ? firstActiveFrame / decodedAudio.sampleRate : 0;
    const recordSource = audioContext.createBufferSource();
    recordSource.buffer = decodedAudio;
    recordSource.loop = true;
    recordSource.connect(workletNode);
    activeSource = recordSource;
    playbackOffset = 0;
    playbackStartedAt = audioContext.currentTime;
    workletNode.port.postMessage({ type: "active", value: true });
    recordSource.start(0, recordOffset);

    const capturedStatus = await waitForShufflingLooperStatus((value) => {
      const status = parseStatus(value);
      return status?.state === "Recording" && status.seconds >= 0.08;
    }, "recorded audio frames");
    const recorded = parseStatus(capturedStatus);
    if (!recorded || recorded.seconds < 0.08) {
      throw new Error(`Shuffling Looper did not record enough audio: ${capturedStatus}`);
    }
    stopSource(false);

    const playbackStatus = await waitForCommand(
      1,
      (status) => status.state === "Playing" && status.hasLoop && status.steps > 0,
      "stop-recording and playback",
    );
    const stoppedStatus = await waitForCommand(
      3,
      (status) => status.state === "Stopped" && status.hasLoop && status.steps > 0,
      "stop with loop retained",
    );
    const resumedStatus = await waitForCommand(
      2,
      (status) => status.state === "Playing" && status.hasLoop && status.steps > 0,
      "playback restart",
    );
    const randomizedStatus = await waitForCommand(
      4,
      (status) => status.state === "Playing" && status.hasLoop && status.steps > 0,
      "sequence randomization",
    );

    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    workletNode.port.postMessage({ type: "capture-reset" });
    const silenceBuffer = audioContext.createBuffer(
      decodedAudio.numberOfChannels,
      decodedAudio.length,
      audioContext.sampleRate,
    );
    const silenceSource = audioContext.createBufferSource();
    silenceSource.buffer = silenceBuffer;
    silenceSource.loop = true;
    silenceSource.connect(workletNode);
    activeSource = silenceSource;
    playbackOffset = 0;
    playbackStartedAt = audioContext.currentTime;
    workletNode.port.postMessage({ type: "active", value: true });
    silenceSource.start();
    await waitForCapture(captureView, reference.samples.length);

    const loopOutput = new Float32Array(captureBuffer);
    let outputPeak = 0;
    let activeOutputSamples = 0;
    for (const sample of loopOutput) {
      const magnitude = Math.abs(sample);
      outputPeak = Math.max(outputPeak, magnitude);
      if (magnitude > 1e-6) activeOutputSamples += 1;
    }
    if (outputPeak < 1e-5 || activeOutputSamples < 32) {
      throw new Error(`Shuffling Looper produced no measurable output from recorded audio: peak ${outputPeak}, active samples ${activeOutputSamples}.`);
    }
    stopSource(false);

    const stoppedAfterPlayback = await waitForCommand(
      3,
      (status) => status.state === "Stopped" && status.hasLoop && status.steps > 0,
      "stop after playback capture",
    );
    const clearedStatus = await waitForCommand(
      5,
      (status) => status.state === "Stopped" && !status.hasLoop && status.steps === 0,
      "clear",
    );
    loopCleared = true;

    return {
      recording: recordingStatus,
      recorded: capturedStatus,
      stopRecordingAndPlay: playbackStatus,
      stopped: stoppedStatus,
      resumed: resumedStatus,
      randomized: randomizedStatus,
      stoppedAfterCapture: stoppedAfterPlayback,
      cleared: clearedStatus,
      silentInputPeak: 0,
      loopOutputPeak: outputPeak,
      activeOutputSamples,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      if (!loopCleared) {
        workletNode.port.postMessage({ type: "shuffling-looper-command", command: 5 });
      }
      shufflingLooperControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateShufflingLooperOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable",
          index,
          value: previousEffectState[index],
        });
      });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runMegatapDelayBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = megatapDelayControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 68 || index === 75 || index === 77);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Megatap Delay smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [megatapDelayTime.value, megatapDelayTapCount.value,
      megatapDelayTimeAlpha.value, megatapDelayTimeRandomize.value,
      megatapDelayAmplitudeShape.value, megatapDelayAmplitudeAlpha.value,
      megatapDelayAmplitudeRandomize.value, megatapDelayPanShape.value,
      megatapDelayPanAlpha.value, megatapDelayStereoSpread.value,
      megatapDelayPredelay.value, megatapDelayFeedback.value,
      megatapDelayFeedbackTap.value, megatapDelayLowCut.value,
      megatapDelayHighCut.value, megatapDelayDiffusionMix.value,
      megatapDelayDiffusionTime.value, megatapDelayDiffusionRate.value,
      megatapDelayDiffusionDepth.value, megatapDelayEnvelopeThreshold.value,
      megatapDelayEnvelopeAttack.value, megatapDelayEnvelopeRelease.value,
      megatapDelayEnvelopeEnabled.checked, megatapDelayInputGain.value,
      megatapDelayMix.value, megatapDelayLevel.value,
      megatapDelayInputSelect.value] = [
      "50", "4", "1", "0", "0", "1", "0", "0", "50", "0", "0", "0",
      "4", "0", "20000", "0", "10", "0.5", "0", "-70", "400", "15",
      false, "0", "100", "0", "0",
    ];
    updateMegatapDelayOptions();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Megatap Delay playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let nonzeroSamples = 0;
    for (const sample of actual) {
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) nonzeroSamples += 1;
    }
    if (peak < 1e-5 || nonzeroSamples < 32) {
      throw new Error(`Megatap Delay produced no measurable processed output from local guitar audio (peak ${peak}, active samples ${nonzeroSamples}).`);
    }
    return { peak, nonzeroSamples, enabledEffectIndex: 68, delayTimeMs: 50, tapCount: 4 };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      megatapDelayControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = previousControls[index];
        else control.value = previousControls[index];
      });
      updateMegatapDelayOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runDeEsserBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = deEsserControls.map((control) => control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 69 || index === 75 || index === 77);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("De-Esser smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [deEsserFrequency.value, deEsserQ.value, deEsserThreshold.value,
      deEsserReduction.value, deEsserAttack.value, deEsserRelease.value] =
      ["6500", "1.5", "-60", "18", "2", "80"];
    updateDeEsserOptions();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting De-Esser playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    for (const sample of actual) {
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
    }
    if (peak < 1e-5 || activeSamples < 32) {
      throw new Error(`De-Esser produced no measurable processed output from local guitar audio (peak ${peak}, active samples ${activeSamples}).`);
    }
    return { peak, activeSamples, enabledEffectIndex: 69, detectorFrequencyHz: 6500 };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      deEsserControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateDeEsserOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runDualEngineDistortionBrowserSmokeTest(
  reference, captureView, captureBuffer,
) {
  const previousEffectState = enabledEffects();
  const previousControls = dualDistortionControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 70 || index === 75 || index === 77);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Dual-Engine Distortion smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [dualDistortionGainA.value, dualDistortionToneA.value,
      dualDistortionGainB.value, dualDistortionToneB.value,
      dualDistortionBlend.value, dualDistortionMode.value,
      dualDistortionLevel.value] =
      ["0.65", "0.8", "0.75", "0.55", "0.35", "0", "0"];
    dualDistortionBassBoost.checked = false;
    updateDualEngineDistortionOptions();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Dual-Engine Distortion playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    for (const sample of actual) {
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
    }
    if (peak < 1e-5 || activeSamples < 32) {
      throw new Error(`Dual-Engine Distortion produced no measurable output from local guitar audio (peak ${peak}, active samples ${activeSamples}).`);
    }
    return { peak, activeSamples, enabledEffectIndex: 70, mode: "blend" };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      dualDistortionControls.forEach((control, index) => {
        if (control.type === "checkbox") {
          control.checked = previousControls[index];
        } else {
          control.value = previousControls[index];
        }
      });
      updateDualEngineDistortionOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runVibeBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = vibeControls.map((control) => control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 71 || index === 75 || index === 77);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Vibe smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [vibeRate.value, vibeDepth.value, vibeTrem.value,
      vibeStages.value, vibeSpread.value, vibeLevel.value,
      vibeRateMode.value, vibeRateDivision.value, vibeTempoBpm.value] =
      ["2.3", "82", "35", "1", "75", "0.9", "1", "7", "90"];
    updateVibeOptions();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Vibe playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    for (const sample of actual) {
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
    }
    if (peak < 1e-5 || activeSamples < 32) {
      throw new Error(`Vibe produced no measurable output from local guitar audio (peak ${peak}, active samples ${activeSamples}).`);
    }
    return {
      peak, activeSamples, enabledEffectIndex: 71, stages: 1,
      rateMode: "tempo-note", noteDivision: "eighth", tempoBpm: 90,
      effectiveRateHz: 3,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      vibeControls.forEach((control, index) => { control.value = previousControls[index]; });
      updateVibeOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runMultiDelayBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = multiDelayControls.map((control) => control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 72 || index === 75 || index === 77);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Multi-Delay smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    const testLines = [
      { timeMs: "55", timeMode: "0", division: "4", feedback: "15", crossFeedback: "12", level: "100", pan: "10" },
      { timeMs: "80", timeMode: "0", division: "3", feedback: "-10", crossFeedback: "8", level: "85", pan: "35" },
      { timeMs: "110", timeMode: "0", division: "2", feedback: "8", crossFeedback: "-7", level: "75", pan: "65" },
      { timeMs: "145", timeMode: "0", division: "0", feedback: "-6", crossFeedback: "10", level: "65", pan: "90" },
    ];
    multiDelayLineControls.forEach((line, index) => {
      for (const [name, value] of Object.entries(testLines[index])) {
        line[name].value = value;
      }
    });
    multiDelayTempo.value = "96";
    multiDelayLowCut.value = "100";
    multiDelayHighCut.value = "8000";
    multiDelayInputGain.value = "2";
    multiDelayMix.value = "35";
    multiDelayLevel.value = "-2";
    multiDelaySeriesFeedback.value = "35";
    multiDelaySeriesSend.value = "3";
    multiDelaySeriesReturn.value = "0";
    multiDelayLfoControls[0].waveform.value = "3";
    multiDelayLfoControls[0].phaseDegrees.value = "90";
    multiDelayLfoControls[0].rateMode.value = "0";
    multiDelayLfoControls[0].rateHz.value = "0.5";
    multiDelayLfoControls[0].division.value = "4";
    multiDelayLfoControls[0].depthMs.value = "5";
    multiDelayLfoControls[1].waveform.value = "1";
    multiDelayLfoControls[1].phaseDegrees.value = "90";
    multiDelayLfoControls[1].rateMode.value = "1";
    multiDelayLfoControls[1].rateHz.value = "0.5";
    multiDelayLfoControls[1].division.value = "12";
    multiDelayLfoControls[1].depthMs.value = "2.5";
    updateMultiDelayOptions();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    const captures = [];
    for (const [topology, name] of [
      [1, "quad-series"], [2, "quad-diffusor"], [3, "quad-tap"],
      [4, "quad-tap-band"], [5, "quad-tape"],
    ]) {
      multiDelayTopology.value = String(topology);
      multiDelayDiffusorMasterTime.value = topology === 2 ? "130" : "100";
      multiDelayDiffusorMasterFeedback.value = topology === 2 ? "60" : "50";
      multiDelayMotorSpeed.value = topology === 5 ? "0.8" : "1";
      updateMultiDelayOptions();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
      Atomics.store(captureView, 0, 0);
      Atomics.store(captureView, 1, 0);
      playbackOffset = 0;
      playButton.click();
      await Promise.race([
        playTask,
        new Promise((_, reject) => setTimeout(
          () => reject(new Error(`Timed out starting ${name} Multi-Delay playback.`)), 5000,
        )),
      ]);
      await waitForCapture(captureView, reference.samples.length);
      stopSource(false);
      const actual = new Float32Array(captureBuffer);
      let peak = 0;
      let activeSamples = 0;
      for (const sample of actual) {
        const magnitude = Math.abs(sample);
        peak = Math.max(peak, magnitude);
        if (magnitude > 1e-6) activeSamples += 1;
      }
      if (peak < 1e-5 || activeSamples < 32) {
        throw new Error(`${name} Multi-Delay produced no measurable output from local guitar audio (peak ${peak}, active samples ${activeSamples}).`);
      }
      captures.push({ peak, activeSamples, topology: name });
    }
    return {
      ...captures[0], enabledEffectIndex: 72, lines: 4, topologies: captures,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      multiDelayControls.forEach((control, index) => { control.value = previousControls[index]; });
      updateMultiDelayOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runPortaPreampBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = portaPreampControls.map((control) => control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 73 || index === 75 || index === 77);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Porta Preamp smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [portaPreampGain1.value, portaPreampGain2.value, portaPreampBass.value,
      portaPreampTreble.value, portaPreampLevel.value] = ["9", "4", "2", "-1", "0"];
    updatePortaPreampOptions();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Porta Preamp playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    for (const sample of actual) {
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
    }
    if (peak < 1e-5 || activeSamples < 32) {
      throw new Error(`Porta Preamp produced no measurable output from local guitar audio (peak ${peak}, active samples ${activeSamples}).`);
    }
    return { peak, activeSamples, enabledEffectIndex: 73, controls: [9, 4, 2, -1, 0] };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      portaPreampControls.forEach((control, index) => { control.value = previousControls[index]; });
      updatePortaPreampOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runPitchSequenceBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = pitchSequenceControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 74 || index === 77 || index === 78);
  try {
    await processorReady;
  if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Pitch Sequence smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    pitchSequenceStepsCount.value = "3";
    pitchSequenceRun.checked = true;
    pitchSequenceTimeMode.value = "0";
    pitchSequenceFreeMs.value = "180";
    pitchSequenceGlide.value = "8";
    pitchSequenceGlitchDepth.value = "5";
    pitchSequenceGlitchInterval.value = "30";
    pitchSequenceFreezeWindow.value = "70";
    pitchSequenceMix.value = "100";
    pitchSequenceLevel.value = "100";
    pitchSequenceSteps[0].semitones.value = "0";
    pitchSequenceSteps[0].mode.value = "0";
    pitchSequenceSteps[1].semitones.value = "5";
    pitchSequenceSteps[1].mode.value = "1";
    pitchSequenceSteps[2].semitones.value = "7";
    pitchSequenceSteps[2].mode.value = "2";
    updatePitchSequenceOptions();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Pitch Sequence playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    for (const sample of actual) {
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
    }
    if (peak < 1e-5 || activeSamples < 32) {
      throw new Error(`Pitch Sequence produced no measurable output from local guitar audio (peak ${peak}, active samples ${activeSamples}).`);
    }
    return { peak, activeSamples, enabledEffectIndex: 74, stepCount: 3,
      modes: ["pitch", "glitch", "freeze"] };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      pitchSequenceControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = previousControls[index];
        else control.value = previousControls[index];
      });
      updatePitchSequenceOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runRhythmTapBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = rhythmTapControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 75 || index === 77 || index === 78);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Rhythm Tap smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    rhythmTapQuantize.value = "0";
    updateRhythmTapDelayOptions();
    rhythmTapLearnStart.click();
    rhythmTapLearnTap.click();
    await new Promise((resolve) => setTimeout(resolve, 20));
    rhythmTapLearnTap.click();
    rhythmTapLearnDone.click();
    const learnedIntervalMs = Number(rhythmTapTimes[0].value);
    if (rhythmTapRepeatCount.value !== "1" ||
        rhythmTapLearnStatus.value !== "Learned 1 repeat interval" ||
        !Number.isFinite(learnedIntervalMs) || learnedIntervalMs < 1 ||
        learnedIntervalMs > 2000) {
      throw new Error("Rhythm Tap Learn must record the dry beat and save a bounded repeat interval.");
    }
    rhythmTapRepeatCount.value = "4";
    rhythmTapTempo.value = "120";
    rhythmTapQuantize.value = "0";
    rhythmTapFeedback.value = "20";
    rhythmTapDecay.value = "0";
    rhythmTapSpread.value = "70";
    rhythmTapPanShape.value = "1";
    rhythmTapPanAlpha.value = "100";
    rhythmTapLowCut.value = "0";
    rhythmTapHighCut.value = "20000";
    rhythmTapMix.value = "100";
    rhythmTapLevel.value = "0";
    rhythmTapMono.checked = false;
    [40, 70, 100, 140, 220, 220, 220, 220, 220, 220]
      .forEach((value, index) => { rhythmTapTimes[index].value = String(value); });
    rhythmTapLevels.forEach((control) => { control.value = "0"; });
    updateRhythmTapDelayOptions();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Rhythm Tap playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    for (const sample of actual) {
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
    }
    if (peak < 1e-5 || activeSamples < 32) {
      throw new Error(`Rhythm Tap produced no measurable output from local guitar audio (peak ${peak}, active samples ${activeSamples}).`);
    }
    return { peak, activeSamples, enabledEffectIndex: 75,
      intervalMs: [40, 70, 100, 140], feedbackPercent: 20, learnedIntervalMs };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      rhythmTapControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = previousControls[index];
        else control.value = previousControls[index];
      });
      updateRhythmTapDelayOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runPlexDelayBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const plexControls = [...plexDelayGlobalControls, ...plexDelayLineControls];
  const previousControls = plexControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 76 || index === 77 || index === 78);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Plex Delay smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    const setGlobal = (name, value) => {
      const control = plexDelayGlobalControls.find((item) =>
        item.dataset.plexGlobal === name);
      if (control.type === "checkbox") control.checked = Boolean(value);
      else control.value = String(value);
    };
    for (const [name, value] of Object.entries({
      activeLines: 2, inputSelect: 1, tempoBpm: 120, masterTime: 1, masterLevel: 100,
      masterPan: 100, decaySeconds: 3, diffusion: 55,
      inputDiffusionMix: 60, inputDiffusionTimeMs: 20, lowCutHz: 20,
      highCutHz: 20000, preDelayMs: 0, stackMode: 0, inputGainDb: 0,
      mix: 65, killDry: true, levelDb: 0, balance: 0, stereoSpread: 120,
      wetFilterType: 6, wetFilterFrequencyHz: 1400, wetFilterQ: 2,
      wetFilterGainDb: 3, filterLfoEnabled: true, filterLfoType: 1,
      filterLfoRateHz: 1.5, filterModFrequencyHz: 3500,
      envelopeEnabled: true, envelopeThresholdDb: -60,
      envelopeAttackMs: 25, envelopeReleaseMs: 80,
      duckerAttenuationDb: 3, duckerThresholdDb: -24,
      duckerReleaseMs: 150,
    })) setGlobal(name, value);
    plexDelayLines.forEach((line, index) => {
      line.querySelector('[data-plex-line-param="timeMode"]').value = "0";
      line.querySelector('[data-plex-line-param="timeMs"]').value = String(40 + index * 30);
      line.querySelector('[data-plex-line-param="division"]').value = "4";
      line.querySelector('[data-plex-line-param="level"]').value = "100";
      line.querySelector('[data-plex-line-param="pan"]').value = String(index < 4 ? -70 : 70);
    });
    updatePlexDelayOptions();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Plex Delay playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    for (const sample of actual) {
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
    }
    if (peak < 1e-5 || activeSamples < 32) {
      throw new Error(`Plex Delay produced no measurable output from local guitar audio (peak ${peak}, active samples ${activeSamples}).`);
    }
    return { peak, activeSamples, enabledEffectIndex: 76, activeLines: 2,
      inputSelect: "left", delayMs: [40, 70], diffusionPercent: 55,
      inputDiffusionMixPercent: 60, inputDiffusionTimeMs: 20,
      killDry: true,
      advancedControls: true };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      plexControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = previousControls[index];
        else control.value = previousControls[index];
      });
      updatePlexDelayOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runCelestialReverbBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = celestialReverbControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 77 || index === 82 || index === 83);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Celestial Reverb smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [85, 4.5, 12, 7, 2.5, 9500, 75].forEach((value, index) => {
      celestialReverbControls[index].value = String(value);
    });
    celestialReverbControls[7].checked = true;
    updateCelestialReverb();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Celestial Reverb playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    for (const sample of actual) {
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
    }
    if (peak < 1e-5 || activeSamples < 32) {
      throw new Error(`Celestial Reverb produced no measurable output from local guitar audio (peak ${peak}, active samples ${activeSamples}).`);
    }
    return {
      peak,
      activeSamples,
      enabledEffectIndex: 77,
      mixPercent: 85,
      decaySeconds: 4.5,
      pitchSemitones: [12, 7],
      pitchDecaySeconds: 2.5,
      highCutHz: 9500,
      blendPercent: 75,
      spillover: true,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      celestialReverbControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = previousControls[index];
        else control.value = previousControls[index];
      });
      updateCelestialReverb();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runCloudReverbBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = cloudReverbControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 79 || index === 82 || index === 83);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Cloud Reverb smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [85, 65, 7, 80, 9000, 100, 0.4, 30].forEach((value, index) => {
      cloudReverbControls[index].value = String(value);
    });
    cloudReverbControls[8].checked = true;
    updateCloudReverb();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Cloud Reverb playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    for (const sample of actual) {
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
    }
    if (peak < 1e-5 || activeSamples < 32) {
      throw new Error(`Cloud Reverb produced no measurable output from local guitar audio (peak ${peak}, active samples ${activeSamples}).`);
    }
    return {
      peak,
      activeSamples,
      enabledEffectIndex: 79,
      mixPercent: 85,
      tonePercent: 65,
      decaySeconds: 7,
      diffusionPercent: 80,
      highDampHz: 9000,
      lowDampHz: 100,
      modulationRateHz: 0.4,
      modulationDepthPercent: 30,
      spillover: true,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      cloudReverbControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = previousControls[index];
        else control.value = previousControls[index];
      });
      updateCloudReverb();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runMercatorDistortionBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = mercatorDistortionControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 80 || index === 83 || index === 85);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Three-Stage Distortion smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [0.65, 0.72, 2, -1.5, 1, 1, 1, -3].forEach((value, index) => {
      const control = mercatorDistortionControls[index];
      if (control.type === "checkbox") control.checked = Boolean(value);
      else control.value = String(value);
    });
    updateMercatorDistortion();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Three-Stage Distortion playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    for (const sample of actual) {
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
    }
    if (peak < 1e-5 || activeSamples < 32) {
      throw new Error(`Three-Stage Distortion produced no measurable output from local guitar audio (peak ${peak}, active samples ${activeSamples}).`);
    }
    return {
      peak,
      activeSamples,
      enabledEffectIndex: 80,
      drive: 0.65,
      drive2: 0.72,
      bassDb: 2,
      midDb: -1.5,
      trebleDb: 1,
      bright1: true,
      bright2: true,
      levelDb: -3,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      mercatorDistortionControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = previousControls[index];
        else control.value = previousControls[index];
      });
      updateMercatorDistortion();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runOctobotBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = octobotControls.map((control) => control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 82 || index === 85 || index === 86);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Octobot smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [0.7, 0.3, 0.55].forEach((value, index) => {
      octobotControls[index].value = String(value);
    });
    updateOctobot();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Octobot playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    for (let index = 0; index < actual.length; index += 1) {
      const sample = actual[index];
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = Math.sqrt(differenceSquares / Math.max(1, actual.length));
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-5) {
      throw new Error(`Octobot produced no measurable processed guitar signal (peak ${peak}, changed samples ${activeSamples}, difference RMS ${differenceRms}).`);
    }
    return {
      peak,
      activeSamples,
      differenceRms,
      enabledEffectIndex: 82,
      direct: 0.7,
      down: 0.3,
      sizzle: 0.55,
      latencySamples: 256,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      octobotControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateOctobot();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runAmRingModBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = amRingModControls.map((control) => control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 83 || index === 85 || index === 86);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("AM Ring Mod smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [713, 0.8, 0.83, 0].forEach((value, index) => {
      amRingModControls[index].value = String(value);
    });
    updateAmRingMod();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting AM Ring Mod playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    for (let index = 0; index < actual.length; index += 1) {
      const sample = actual[index];
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = Math.sqrt(differenceSquares / Math.max(1, actual.length));
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-5) {
      throw new Error(`AM Ring Mod produced no measurable guitar output (peak ${peak}, active samples ${activeSamples}, difference RMS ${differenceRms}).`);
    }
    return {
      peak,
      activeSamples,
      differenceRms,
      enabledEffectIndex: 83,
      rateHz: 713,
      depth: 0.8,
      mix: 0.83,
      levelDb: 0,
      latencySamples: 0,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      amRingModControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateAmRingMod();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runTapeEchoBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = tapeEchoControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 84 || index === 86 || index === 87);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Tape Echo smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    const values = [0, 20, 120, 4, 0.55, 0.75, 0.45, 0.55, 0.1, 0.6, 0];
    tapeEchoControls.forEach((control, index) => {
      if (control.type === "checkbox") control.checked = Boolean(values[index]);
      else control.value = String(values[index]);
    });
    updateTapeEcho();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Tape Echo playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    for (let index = 0; index < actual.length; index += 1) {
      const sample = actual[index];
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = Math.sqrt(differenceSquares / Math.max(1, actual.length));
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-5) {
      throw new Error(`Tape Echo produced no measurable guitar output (peak ${peak}, active samples ${activeSamples}, difference RMS ${differenceRms}).`);
    }
    return {
      peak,
      activeSamples,
      differenceRms,
      enabledEffectIndex: 84,
      timeMs: 20,
      feedback: 0.55,
      mix: 0.75,
      spillover: false,
      latencySamples: 0,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      tapeEchoControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = Boolean(previousControls[index]);
        else control.value = previousControls[index];
      });
      updateTapeEcho();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runOctaveFuzzBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = octaveFuzzControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 85 || index === 86 || index === 87);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Octave Fuzz smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [octaveFuzzControls[0].value, octaveFuzzControls[1].value] = ["0.7", "0.8"];
    octaveFuzzControls[2].checked = true;
    updateOctaveFuzz();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Octave Fuzz playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    for (let index = 0; index < actual.length; index += 1) {
      const sample = actual[index];
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = Math.sqrt(differenceSquares / Math.max(1, actual.length));
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-5) {
      throw new Error(`Octave Fuzz produced no measurable guitar output (peak ${peak}, active samples ${activeSamples}, difference RMS ${differenceRms}).`);
    }
    return { peak, activeSamples, differenceRms, enabledEffectIndex: 85,
      boost: 0.7, volume: 0.8, octaveSwitch: true, latencySamples: 256 };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      octaveFuzzControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = Boolean(previousControls[index]);
        else control.value = previousControls[index];
      });
      updateOctaveFuzz();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runManualThruZeroFlangerBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = manualThruZeroFlangerControls.map((control) => control.value);
  const testEnabled = previousEffectState.map((_, index) =>
    index === 81 || index === 86 || index === 87);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Manual Through-Zero Flanger smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [0.34, 8, 5.25, 0.46, 30, 1, 0.55, 10].forEach((value, index) => {
      manualThruZeroFlangerControls[index].value = String(value);
    });
    updateManualThruZeroFlanger();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Manual Through-Zero Flanger playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    for (let index = 0; index < actual.length; index += 1) {
      const sample = actual[index];
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = Math.sqrt(differenceSquares / Math.max(1, actual.length));
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-5) {
      throw new Error(`Manual Through-Zero Flanger produced no measurable processed guitar signal (peak ${peak}, changed samples ${activeSamples}, difference RMS ${differenceRms}).`);
    }
    return {
      peak,
      activeSamples,
      differenceRms,
      enabledEffectIndex: 81,
      minimumTimeMs: 0.34,
      maximumTimeMs: 8,
      manualMs: 5.25,
      dryDelayPosition: 0.46,
      feedbackPercent: 30,
      phaseReverse: "Left",
      mix: 0.55,
      smoothingMs: 10,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      manualThruZeroFlangerControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateManualThruZeroFlanger();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runRingModulatorBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = [
    ringModType.value,
    ringModFrequency.value,
    ringModMultiplier.value,
    ringModHighCut.value,
    ringModMix.value,
    ringModPitchTracking.checked,
    ringModTrackingMode.value,
  ];
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Ring Modulator smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    ringModFrequency.value = "713";
    ringModMultiplier.value = "1.75";
    ringModHighCut.value = "9000";
    ringModMix.value = "100";
    ringModPitchTracking.checked = false;
    ringModTrackingMode.value = "0";
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    const capture = async (type, ringEnabled = true,
      pitchTracking = false, trackingMode = 0) => {
      checkboxes.forEach((checkbox, index) => {
    const enabled = (ringEnabled && index === 34) || index === 82 || index === 83;
        checkbox.checked = enabled;
        workletNode.port.postMessage({ type: "enable", index, value: enabled });
      });
      ringModType.value = String(type);
      ringModType.dispatchEvent(new Event("change", { bubbles: true }));
      ringModPitchTracking.checked = pitchTracking;
      ringModTrackingMode.value = String(trackingMode);
      ringModPitchTracking.dispatchEvent(new Event("change", { bubbles: true }));
      ringModTrackingMode.dispatchEvent(new Event("change", { bubbles: true }));
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
      Atomics.store(captureView, 0, 0);
      Atomics.store(captureView, 1, 0);
      playbackOffset = 0;
      playButton.click();
      await Promise.race([
        playTask,
        new Promise((_, reject) => setTimeout(
          () => reject(new Error("Timed out starting Ring Modulator playback.")), 5000,
        )),
      ]);
      await waitForCapture(captureView, reference.samples.length);
      stopSource(false);
      return Float32Array.from(new Float32Array(captureBuffer));
    };
    const dry = await capture(0, false);
    const outputs = [
      await capture(0),
      await capture(1),
      await capture(2),
    ];
    const summaries = outputs.map((samples, index) => {
      let peak = 0;
      let squares = 0;
      let activeSamples = 0;
      for (const sample of samples) {
        const magnitude = Math.abs(sample);
        peak = Math.max(peak, magnitude);
        squares += sample * sample;
        if (magnitude > 1e-6) activeSamples += 1;
      }
      const rms = Math.sqrt(squares / Math.max(1, samples.length));
      if (peak < 1e-5 || activeSamples < 32) {
        throw new Error(
          `Ring Modulator type ${index} produced no measurable output from local guitar audio: ${JSON.stringify({ peak, rms, activeSamples })}.`);
      }
      return { type: index, peak, rms, activeSamples };
    });
    let classicMaxDifference = 0;
    let classicDifferenceSquares = 0;
    let classicChangedSamples = 0;
    for (let index = 0; index < outputs[0].length; index += 1) {
      const difference = Math.abs(outputs[0][index] - dry[index]);
      classicMaxDifference = Math.max(classicMaxDifference, difference);
      classicDifferenceSquares += difference * difference;
      if (difference > 1e-6) classicChangedSamples += 1;
    }
    const classicRmsDifference = Math.sqrt(
      classicDifferenceSquares / Math.max(1, outputs[0].length),
    );
    if (classicMaxDifference < 1e-4 || classicRmsDifference < 1e-5 ||
        classicChangedSamples < 32) {
      throw new Error(
        `Ring Modulator Classic did not alter local guitar output: ${JSON.stringify({ classicMaxDifference, classicRmsDifference, classicChangedSamples })}`);
    }
    let ssbMaxDifference = 0;
    let ssbDifferenceSquares = 0;
    let ssbChangedSamples = 0;
    for (let index = 0; index < outputs[1].length; index += 1) {
      const difference = Math.abs(outputs[1][index] - outputs[2][index]);
      ssbMaxDifference = Math.max(ssbMaxDifference, difference);
      ssbDifferenceSquares += difference * difference;
      if (difference > 1e-6) ssbChangedSamples += 1;
    }
    const ssbRmsDifference = Math.sqrt(
      ssbDifferenceSquares / Math.max(1, outputs[1].length),
    );
    if (ssbMaxDifference < 1e-4 || ssbRmsDifference < 1e-5 || ssbChangedSamples < 32) {
      throw new Error(
        `Ring Modulator SSB Up and SSB Down did not produce distinct output: ${JSON.stringify({ ssbMaxDifference, ssbRmsDifference, ssbChangedSamples })}`);
    }
    const tracked = await capture(0, true, true, 0);
    let trackingDifferenceSquares = 0;
    let trackingChangedSamples = 0;
    for (let index = 0; index < outputs[0].length; index += 1) {
      const difference = tracked[index] - outputs[0][index];
      trackingDifferenceSquares += difference * difference;
      if (Math.abs(difference) > 1e-6) trackingChangedSamples += 1;
    }
    const trackingRmsDifference = Math.sqrt(
      trackingDifferenceSquares / Math.max(1, outputs[0].length),
    );
    if (trackingRmsDifference < 1e-5 || trackingChangedSamples < 32) {
      throw new Error(
        `Ring Modulator pitch tracking did not change local guitar output: ${JSON.stringify({ trackingRmsDifference, trackingChangedSamples })}`);
    }
    return {
      enabledEffectIndex: 34,
      types: ["Classic", "SSB Up", "SSB Down"],
      summaries,
      classicDryRmsDifference: classicRmsDifference,
      ssbUpDownRmsDifference: ssbRmsDifference,
      ssbUpDownMaxDifference: ssbMaxDifference,
      ssbUpDownChangedSamples: ssbChangedSamples,
      pitchTrackingRmsDifference: trackingRmsDifference,
      pitchTrackingChangedSamples: trackingChangedSamples,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      [ringModType, ringModFrequency, ringModMultiplier, ringModHighCut, ringModMix]
        .forEach((control, index) => { control.value = previousControls[index]; });
      ringModPitchTracking.checked = previousControls[5];
      ringModPitchTracking.dispatchEvent(new Event("change", { bubbles: true }));
      ringModTrackingMode.value = previousControls[6];
      ringModTrackingMode.dispatchEvent(new Event("change", { bubbles: true }));
      ringModType.dispatchEvent(new Event("change", { bubbles: true }));
      [...document.querySelectorAll("[data-effect]")].forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runRockBoxDistBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = rockBoxDistControls.map((control) => control.value);
  const testEnabled = previousEffectState.map((_, index) => index === 88);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Rock Box Dist smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [rockBoxDistControls[0].value, rockBoxDistControls[1].value,
      rockBoxDistControls[2].value, rockBoxDistControls[3].value] =
      ["0.72", "0.32", "-3", "0"];
    updateRockBoxDist();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Rock Box Dist playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    for (let index = 0; index < actual.length; index += 1) {
      const sample = actual[index];
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = Math.sqrt(differenceSquares / Math.max(1, actual.length));
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-5) {
      throw new Error(`Rock Box Dist produced no measurable guitar output (peak ${peak}, active samples ${activeSamples}, difference RMS ${differenceRms}).`);
    }
    return { peak, activeSamples, differenceRms, enabledEffectIndex: 88,
      drive: 0.72, tone: 0.32, levelDb: -3 };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      rockBoxDistControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateRockBoxDist();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runRockBoxBoostBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = rockBoxBoostControls.map((control) => control.value);
  const testEnabled = previousEffectState.map((_, index) => index === 89);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Rock Box Boost smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [rockBoxBoostControls[0].value, rockBoxBoostControls[1].value,
      rockBoxBoostControls[2].value] = ["6", "-3", "0"];
    updateRockBoxBoost();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Rock Box Boost playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    for (let index = 0; index < actual.length; index += 1) {
      const sample = actual[index];
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = Math.sqrt(differenceSquares / Math.max(1, actual.length));
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-5) {
      throw new Error(`Rock Box Boost produced no measurable guitar output (peak ${peak}, active samples ${activeSamples}, difference RMS ${differenceRms}).`);
    }
    return { peak, activeSamples, differenceRms, enabledEffectIndex: 89,
      boostDb: 6, levelDb: -3 };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      rockBoxBoostControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateRockBoxBoost();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runStereoPannerBrowserSmokeTest(reference, captureView, captureBuffer) {
  // Run the adjacent project-authored sustain processor against the same
  // prerecorded guitar fixture as part of the browser playback smoke suite.
  await runPolySustainBrowserSmokeTest(reference, captureView, captureBuffer);
  const previousEffectState = enabledEffects();
  const previousControls = stereoPannerControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const testEnabled = previousEffectState.map((_, index) => index === 90);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== 2 || reference.channels !== 2) {
      throw new Error("Stereo Panner browser smoke requires the local stereo guitar DI fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    [stereoPannerControls[0].value, stereoPannerControls[1].value,
      stereoPannerControls[2].value, stereoPannerControls[3].checked,
      stereoPannerControls[4].value, stereoPannerControls[5].value,
      stereoPannerControls[6].value, stereoPannerControls[7].value,
      stereoPannerControls[8].value, stereoPannerControls[9].value,
      stereoPannerControls[10].value, stereoPannerControls[11].value,
      stereoPannerControls[12].value, stereoPannerControls[13].value] =
      ["2.5", "100", "0", false, "120", "5", "50", "50", "180", "0",
        "0", "0", "0", "0"];
    updateStereoPannerOptions();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Stereo Panner playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    const sourceLeft = decodedAudio.getChannelData(0);
    const sourceRight = decodedAudio.getChannelData(1);
    let peak = 0;
    let activeSamples = 0;
    let sourceDifferenceSquares = 0;
    let stereoDifferenceSquares = 0;
    const frames = Math.min(decodedAudio.length, Math.floor(actual.length / 2));
    for (let frame = 0; frame < frames; frame += 1) {
      const left = actual[frame * 2];
      const right = actual[frame * 2 + 1];
      peak = Math.max(peak, Math.abs(left), Math.abs(right));
      if (Math.abs(left) > 1e-6) activeSamples += 1;
      if (Math.abs(right) > 1e-6) activeSamples += 1;
      const leftDifference = left - sourceLeft[frame];
      const rightDifference = right - sourceRight[frame];
      sourceDifferenceSquares += leftDifference * leftDifference +
        rightDifference * rightDifference;
      const stereoDifference = left - right;
      stereoDifferenceSquares += stereoDifference * stereoDifference;
    }
    const differenceRms = Math.sqrt(sourceDifferenceSquares / Math.max(1, frames * 2));
    const stereoDifferenceRms = Math.sqrt(stereoDifferenceSquares / Math.max(1, frames));
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-5 ||
        stereoDifferenceRms < 1e-5) {
      throw new Error(`Stereo Panner did not produce measurable stereo guitar output (peak ${peak}, source difference RMS ${differenceRms}, L/R difference RMS ${stereoDifferenceRms}).`);
    }
    return { peak, activeSamples, differenceRms, stereoDifferenceRms,
      enabledEffectIndex: 90, rateHz: 2.5, widthPercent: 100, phaseDegrees: 180 };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      stereoPannerControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = previousControls[index];
        else control.value = previousControls[index];
      });
      updateStereoPannerOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runPolySustainBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = polySustainControls.map((control) =>
    control.type === "range" ? control.value : control.value);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Poly Sustain browser smoke requires the local prerecorded guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    [polySustainInterval.value, polySustainAttack.value,
      polySustainDecay.value, polySustainFxLevel.value,
      polySustainLevel.value, polySustainOperation.value] =
      [7, 0, 2000, 100, 0, 1].map(String);
    updatePolySustainOptions();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = false;
      workletNode.port.postMessage({ type: "enable", index, value: false });
    });
    checkboxes[91].click();
    if (!checkboxes[91].checked) {
      throw new Error("Poly Sustain picker row did not enable from its click handler.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);

    const actual = new Float32Array(captureBuffer);
    const source = decodedAudio.getChannelData(0);
    const frames = Math.min(decodedAudio.length, Math.floor(actual.length / reference.channels));
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    for (let frame = 0; frame < frames; frame += 1) {
      for (let channel = 0; channel < reference.channels; channel += 1) {
        const value = actual[frame * reference.channels + channel];
        peak = Math.max(peak, Math.abs(value));
        if (Math.abs(value) > 1e-6) activeSamples += 1;
        const sourceValue = decodedAudio.getChannelData(
          Math.min(channel, decodedAudio.numberOfChannels - 1),
        )[frame];
        const difference = value - sourceValue;
        differenceSquares += difference * difference;
      }
    }
    const differenceRms = Math.sqrt(
      differenceSquares / Math.max(1, frames * reference.channels),
    );
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-5) {
      throw new Error(`Poly Sustain produced no measurable held guitar output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return { enabledEffectIndex: 91, pitchIntervalSemitones: 7,
      operation: "Dry Kill", peak, activeSamples, differenceRms };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      polySustainControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updatePolySustainOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runStepFilterDelayBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = stepFilterDelayControls.map((control) => control.value);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Step Filter Delay browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    stepFilterDelayTimeMode.value = "0";
    stepFilterDelayTimeMs.value = "20";
    stepFilterDelayDivision.value = "4";
    stepFilterDelayStepMode.value = "0";
    stepFilterDelayStepDivision.value = "10";
    stepFilterDelayTempo.value = "120";
    stepFilterDelayStepRate.value = "8";
    stepFilterDelayFeedback.value = "0";
    stepFilterDelayMix.value = "100";
    stepFilterDelayFilterMode.value = "0";
    stepFilterDelayMinimumCutoff.value = "120";
    stepFilterDelayMaximumCutoff.value = "6000";
    stepFilterDelayResonanceQ.value = "0.707";
    stepFilterDelayDepth.value = "100";
    stepFilterDelayShape.value = "15";
    stepFilterDelaySmoothing.value = "0";
    stepFilterDelayPhase.value = "0";
    stepFilterDelaySteps.forEach((control, index) => {
      control.value = String([0, 72, 18, 88, 32, 62, 8, 96,
        24, 78, 12, 90, 38, 68, 4, 84][index]);
    });
    updateStepFilterDelayOptions();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      checkbox.checked = false;
      workletNode.port.postMessage({ type: "enable", index, value: false });
    });
    checkboxes[92].click();
    if (!checkboxes[92].checked) {
      throw new Error("Step Filter Delay picker row did not enable from its click handler.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);

    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    const frames = Math.min(decodedAudio.length,
      Math.floor(actual.length / reference.channels));
    for (let frame = 0; frame < frames; frame += 1) {
      for (let channel = 0; channel < reference.channels; channel += 1) {
        const value = actual[frame * reference.channels + channel];
        peak = Math.max(peak, Math.abs(value));
        if (Math.abs(value) > 1e-6) activeSamples += 1;
        const source = decodedAudio.getChannelData(
          Math.min(channel, decodedAudio.numberOfChannels - 1),
        )[frame];
        const difference = value - source;
        differenceSquares += difference * difference;
      }
    }
    const differenceRms = Math.sqrt(
      differenceSquares / Math.max(1, frames * reference.channels),
    );
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-5) {
      throw new Error(`Step Filter Delay produced no measurable guitar output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return { enabledEffectIndex: 92, delayMs: 20, steps: stepFilterDelaySteps.length,
      peak, activeSamples, differenceRms };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      stepFilterDelayControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateStepFilterDelayOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runVolumePanBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = volumePanControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const previousCategory = effectCategoryFilter.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Volume/Pan browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    volumePanPosition.value = "55";
    volumePanMinimumDb.value = "-18";
    volumePanMaximumDb.value = "0";
    volumePanTaper.value = "4";
    volumePanInputSelect.value = "0";
    volumePanLeft.value = "-75";
    volumePanRight.value = "70";
    volumePanBalance.value = "10";
    volumePanOutputLevel.value = "2";
    volumePanBypassed.checked = false;
    volumePanBypassMode.value = "0";
    updateVolumePanOptions();
    effectCategoryFilter.value = "utility";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="93"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    if (row?.dataset.category !== "utility" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Utility" ||
        !checkbox) {
      throw new Error("Volume/Pan is missing from the Utility picker category.");
    }
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      box.checked = false;
      workletNode.port.postMessage({ type: "enable", index, value: false });
    });
    checkbox.click();
    if (!checkbox.checked) {
      throw new Error("Volume/Pan picker row did not enable from its click handler.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);

    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    const frames = Math.min(decodedAudio.length,
      Math.floor(actual.length / reference.channels));
    for (let frame = 0; frame < frames; frame += 1) {
      for (let channel = 0; channel < reference.channels; channel += 1) {
        const value = actual[frame * reference.channels + channel];
        peak = Math.max(peak, Math.abs(value));
        if (Math.abs(value) > 1e-6) activeSamples += 1;
        const source = decodedAudio.getChannelData(
          Math.min(channel, decodedAudio.numberOfChannels - 1),
        )[frame];
        const difference = value - source;
        differenceSquares += difference * difference;
      }
    }
    const differenceRms = Math.sqrt(
      differenceSquares / Math.max(1, frames * reference.channels),
    );
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-4) {
      throw new Error(`Volume/Pan produced no measurable guitar output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return { category: "Utility", enabledEffectIndex: 93, playOutputCaptured: true,
      peak, activeSamples, differenceRms };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      volumePanControls.forEach((control, index) => {
        if (control.type === "checkbox") {
          control.checked = Boolean(previousControls[index]);
        } else {
          control.value = previousControls[index];
        }
      });
      updateVolumePanOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runDuckingDelayBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const controls = [duckingDelayTime, duckingDelayTimeMode, duckingDelayTempo,
    duckingDelayTapDivision, duckingDelayFeedback, duckingDelayMix,
    duckingDelayThreshold, duckingDelayAttenuation, duckingDelayRelease,
    duckingDelaySpillover, duckingDelayLowCut, duckingDelayHighCut,
    duckingDelayModRate, duckingDelayModDepth, duckingDelayModMode];
  const previousControls = controls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const previousCategory = effectCategoryFilter.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Ducking Delay browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    duckingDelayTime.value = "350";
    duckingDelayTimeMode.value = "1";
    duckingDelayTempo.value = "120";
    duckingDelayTapDivision.value = "7";
    duckingDelayFeedback.value = "35";
    duckingDelayMix.value = "100";
    duckingDelayThreshold.value = "-24";
    duckingDelayAttenuation.value = "12";
    duckingDelayRelease.value = "250";
    duckingDelaySpillover.checked = false;
    duckingDelayLowCut.value = "150";
    duckingDelayHighCut.value = "7000";
    duckingDelayModRate.value = "1.25";
    duckingDelayModDepth.value = "6.5";
    duckingDelayModMode.value = "0";
    updateDuckingDelay();
    effectCategoryFilter.value = "delay";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="39"]')?.closest(".effect-row");
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    const checkbox = checkboxes[39];
    if (row?.dataset.category !== "delay" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Delay" ||
        !checkbox || !duckingDelayTime.disabled ||
        DEFAULT_PARAMETERS.duckingDelay[7] !== 1 ||
        DEFAULT_PARAMETERS.duckingDelay[8] !== 120 ||
        DEFAULT_PARAMETERS.duckingDelay[9] !== 7 ||
        DEFAULT_PARAMETERS.duckingDelay[10] !== 150 ||
        DEFAULT_PARAMETERS.duckingDelay[11] !== 7000 ||
        DEFAULT_PARAMETERS.duckingDelay[12] !== 1.25 ||
        DEFAULT_PARAMETERS.duckingDelay[13] !== 6.5 ||
        DEFAULT_PARAMETERS.duckingDelay[14] !== 0) {
      throw new Error("Ducking Delay tempo, repeat-cut, modulation controls, or Delay picker category did not reach the browser chain.");
    }
    checkboxes.forEach((box, index) => {
      const enabled = index === 0 || index === 39;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    if (!checkbox.checked) checkbox.click();
    if (!checkbox.checked) {
      throw new Error("Ducking Delay picker row did not enable from its click handler.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Ducking Delay playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);

    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    const sampleCount = Math.min(actual.length, reference.samples.length);
    for (let index = 0; index < sampleCount; index += 1) {
      const sample = actual[index];
      peak = Math.max(peak, Math.abs(sample));
      if (Math.abs(sample) > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = sampleCount > 0
      ? Math.sqrt(differenceSquares / sampleCount) : 0;
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-4) {
      throw new Error(`Ducking Delay produced no measurable guitar output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return {
      category: "Delay", enabledEffectIndex: 39, timeMode: "tempo-sync",
      tempoBpm: 120, tapDivision: "eighth", repeatLowCutHz: 150,
      repeatHighCutHz: 7000, modRateHz: 1.25, modDepthMs: 6.5,
      modMode: "sine", playOutputCaptured: true,
      peak, activeSamples, differenceRms,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      controls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = previousControls[index];
        else control.value = previousControls[index];
      });
      updateDuckingDelay();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runChromaticPitchDelayBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const controls = chromaticPitchDelayControls;
  const previousControls = controls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const previousCategory = effectCategoryFilter.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Chromatic Pitch Delay browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    controls[0].value = "12";
    controls[1].value = "350";
    controls[2].value = "20";
    controls[3].value = "70";
    controls[4].value = "100";
    controls[5].value = "1";
    controls[6].value = "120";
    controls[7].value = "7";
    controls[8].checked = false;
    updateChromaticPitchDelay();
    effectCategoryFilter.value = "pitch";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="99"]')?.closest(".effect-row");
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    const checkbox = checkboxes[99];
    const sampleCount = Math.min(reference.samples.length,
      audioContext.sampleRate * reference.channels);
    if (row?.dataset.category !== "pitch" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Pitch" ||
        !checkbox || !controls[1].disabled ||
        DEFAULT_PARAMETERS.chromaticPitchDelay[0] !== 12 ||
        DEFAULT_PARAMETERS.chromaticPitchDelay[5] !== 1 ||
        DEFAULT_PARAMETERS.chromaticPitchDelay[6] !== 120 ||
        DEFAULT_PARAMETERS.chromaticPitchDelay[7] !== 7 ||
        sampleCount < 256) {
      throw new Error("Chromatic Pitch Delay controls, tempo sync, or Pitch category did not reach the browser chain.");
    }
    checkboxes.forEach((box, index) => {
      const enabled = index === 0 || index === 99;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    if (!checkbox.checked) checkbox.click();
    if (!checkbox.checked) {
      throw new Error("Chromatic Pitch Delay picker row did not enable from its click handler.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Chromatic Pitch Delay playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, sampleCount);
    stopSource(false);

    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    for (let index = 0; index < sampleCount; index += 1) {
      const sample = actual[index];
      peak = Math.max(peak, Math.abs(sample));
      if (Math.abs(sample) > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = Math.sqrt(differenceSquares / sampleCount);
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-4) {
      throw new Error(`Chromatic Pitch Delay produced no measurable guitar output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return {
      category: "Pitch", enabledEffectIndex: 99, pitchSemitones: 12,
      timeMode: "tempo-sync", tempoBpm: 120, tapDivision: "eighth",
      feedbackPercent: 20, mixPercent: 100, playOutputCaptured: true,
      sampleCount, peak, activeSamples, differenceRms,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      controls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = previousControls[index];
        else control.value = previousControls[index];
      });
      updateChromaticPitchDelay();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runCabinetIrBrowserSmokeTest(
  reference, captureView, captureBuffer, cabinetIRBytes,
) {
  const previousEffectState = enabledEffects();
  const previousCategory = effectCategoryFilter.value;
  const makeCopy = () => cabinetIRBytes.slice(0);
  const expectFormatError = (name, bytes, messagePart) => {
    try {
      decodeCabinetIrWave(bytes);
    } catch (error) {
      if (error instanceof Error && error.message.includes(messagePart)) return error.message;
      throw new Error(`${name} failed with an unexpected format message: ${String(error)}`);
    }
    throw new Error(`${name} Cabinet IR format was incorrectly accepted.`);
  };
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Cabinet IR smoke requires the local prerecorded guitar fixture.");
    }
    await audioContext.resume();
    stopSource(true);

    const decodedIR = decodeCabinetIrWave(cabinetIRBytes);
    if (decodedIR.frames !== 2048 || decodedIR.channels !== 1 ||
        decodedIR.sampleRate !== 48000 || decodedIR.bitsPerSample !== 24 ||
        Math.abs(decodedIR.samples[0] - 0.5) > 1e-6 ||
        Math.abs(decodedIR.samples[1] + 0.125) > 1e-6) {
      throw new Error("Synthetic Cabinet IR did not decode as signed mono PCM24 samples.");
    }

    const malformed = makeCopy();
    new DataView(malformed).setUint32(0, 0x42414421, false);
    const wrongEncoding = makeCopy();
    new DataView(wrongEncoding).setUint16(20, 3, true);
    const wrongChannels = makeCopy();
    new DataView(wrongChannels).setUint16(22, 2, true);
    const wrongRate = makeCopy();
    new DataView(wrongRate).setUint32(24, 44100, true);
    const wrongDepth = makeCopy();
    new DataView(wrongDepth).setUint16(34, 16, true);
    // RIFF chunks with an odd payload size include one zero padding byte. Keep
    // that byte in the 2,047-frame case so the test reaches frame validation.
    const cabinetIRView = new Uint8Array(cabinetIRBytes);
    const short = new ArrayBuffer(cabinetIRView.byteLength - 2);
    new Uint8Array(short).set(
      cabinetIRView.subarray(0, cabinetIRView.byteLength - 3),
    );
    const shortView = new DataView(short);
    shortView.setUint32(4, short.byteLength - 8, true);
    shortView.setUint32(40, 2047 * 3, true);
    const rejectedFormats = [
      expectFormatError("bad RIFF signature", malformed, "RIFF/WAVE"),
      expectFormatError("non-PCM encoding", wrongEncoding, "integer PCM"),
      expectFormatError("stereo input", wrongChannels, "mono"),
      expectFormatError("44.1 kHz input", wrongRate, "48 kHz"),
      expectFormatError("16-bit input", wrongDepth, "24-bit PCM"),
      expectFormatError("2,047-frame input", short, "exactly 2,048 samples"),
    ];

    const previousChainMemory = chainMemoryBytes;
    const invalidFile = new File([wrongDepth], "invalid-cabinet-ir.wav", {
      type: "audio/wav",
    });
    try {
      await loadCabinetIRFile(invalidFile);
      throw new Error("Cabinet IR file picker accepted a 16-bit WAV.");
    } catch (error) {
      if (!(error instanceof Error) || !error.message.includes("24-bit PCM")) throw error;
    }
    if (!cabinetIrStatus.textContent.includes("24-bit PCM")) {
      throw new Error("Cabinet IR UI did not report the rejected PCM bit depth.");
    }

    const file = new File([cabinetIRBytes], "browser-cabinet-ir-pcm24.wav", {
      type: "audio/wav",
    });
    await loadCabinetIRFile(file);
    if (cabinetIRUploadedVersion !== cabinetIRVersion ||
        effectMemory.length <= 100 || effectMemory[100] <= 0 ||
        chainMemoryBytes <= previousChainMemory || cabinetIRLatencyFrames !== 64) {
      throw new Error("Cabinet IR load did not update processor memory totals or report the fixed 64-frame latency.");
    }

    effectCategoryFilter.value = "ir-loader";
    updateEffectBrowser();
    const row = document.querySelector('[data-effect="100"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    if (row?.dataset.category !== "ir-loader" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "IR Loader" ||
        row.hidden || !checkbox) {
      throw new Error("Cabinet IR is missing from the IR Loader picker category.");
    }

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      const enabled = index === 0 || index === 100;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    if (!checkbox.checked) checkbox.click();
    if (!checkbox.checked) throw new Error("Cabinet IR picker row did not enable its processor.");
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Cabinet IR guitar playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);

    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let differenceSquares = 0;
    let changedSamples = 0;
    for (let frame = 0; frame < decodedAudio.length; frame += 1) {
      for (let channel = 0; channel < reference.channels; channel += 1) {
        const sampleIndex = frame * reference.channels + channel;
        const value = actual[sampleIndex];
        const dry = decodedAudio.getChannelData(
          Math.min(channel, decodedAudio.numberOfChannels - 1),
        )[frame];
        if (!Number.isFinite(value)) {
          throw new Error("Cabinet IR produced a non-finite sample from prerecorded guitar audio.");
        }
        peak = Math.max(peak, Math.abs(value));
        const difference = value - dry;
        differenceSquares += difference * difference;
        if (Math.abs(difference) > 1e-6) changedSamples += 1;
      }
    }
    const differenceRms = Math.sqrt(
      differenceSquares / Math.max(1, decodedAudio.length * reference.channels),
    );
    if (peak < 1e-5 || differenceRms < 1e-5 || changedSamples < 32) {
      throw new Error("Cabinet IR did not produce changed local guitar playback: " +
        JSON.stringify({ peak, differenceRms, changedSamples }));
    }
    return {
      category: "IR Loader",
      enabledEffectIndex: 100,
      format: "mono PCM24 · 48 kHz · 2,048 samples",
      latencyFrames: cabinetIRLatencyFrames,
      latencyMsAt48000Hz: cabinetIRLatencyFrames * 1000 / 48000,
      rejectedFormats,
      playOutputCaptured: true,
      peak,
      differenceRms,
      changedSamples,
      effectMemoryBytes: effectMemory[100],
      processorMemoryBytes: chainMemoryBytes,
      wasmLinearMemoryBytes: wasmMemoryBytes,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runStepTremoloBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = stepTremoloControls.map((control) => control.value);
  const previousCategory = effectCategoryFilter.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Step Tremolo browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    stepTremoloStepCount.value = "4";
    stepTremoloRateHz.value = "4";
    stepTremoloDepth.value = "100";
    stepTremoloTransitionMs.value = "4";
    const testLevels = [100, 0, 75, 25, 100, 50, 0, 75,
      100, 0, 75, 25, 100, 50, 0, 75];
    stepTremoloLevels.forEach((control, index) => {
      control.value = String(testLevels[index]);
    });
    updateStepTremoloOptions();
    effectCategoryFilter.value = "modulation";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="97"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    const slicerRow = document.querySelector('[data-effect="35"]')?.closest(".effect-row");
    if (row?.dataset.category !== "modulation" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Modulation" ||
        row.querySelector(".effect-name")?.textContent.includes("Rhythmic Slicer") ||
        slicerRow?.querySelector(".effect-name")?.textContent.trim() !== "Rhythmic Slicer" ||
        !checkbox || stepTremoloLevels.length !== 16) {
      throw new Error("Step Tremolo is missing from its distinct Modulation picker entry or controls.");
    }

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      const enabled = index === 0 || index === 97;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    if (!checkbox.checked) {
      checkbox.click();
    }
    if (!checkbox.checked) {
      throw new Error("Step Tremolo picker row did not enable from its click handler.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Step Tremolo playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    const sampleCount = Math.min(actual.length, reference.samples.length);
    for (let index = 0; index < sampleCount; index += 1) {
      const sample = actual[index];
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = sampleCount > 0 ? Math.sqrt(differenceSquares / sampleCount) : 0;
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-4) {
      throw new Error(`Step Tremolo produced no measurable guitar output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return {
      category: "Modulation", enabledEffectIndex: 97, stepCount: 4,
      rateHz: 4, depthPercent: 100, levelsPercent: testLevels,
      transitionMs: 4, playOutputCaptured: true, peak, activeSamples, differenceRms,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      stepTremoloControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateStepTremoloOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runOctaveDividerBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = octaveDividerControls.map((control) => control.value);
  const previousCategory = effectCategoryFilter.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Octave Divider browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    octaveDividerControls[0].value = "1.1";
    octaveDividerControls[1].value = "0.45";
    octaveDividerControls[2].value = "-0.5";
    octaveDividerControls[3].value = "0.25";
    octaveDividerControls[4].value = "0.5";
    octaveDividerControls[5].value = "0.8";
    octaveDividerControls[6].value = "1";
    octaveDividerControls[5].dispatchEvent(new Event("input", { bubbles: true }));
    effectCategoryFilter.value = "pitch";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="101"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    if (row?.dataset.category !== "pitch" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Pitch" ||
        !checkbox || octaveDividerControls.length !== 7 ||
        Math.abs(DEFAULT_PARAMETERS.octaveDivider[5] - 0.8) > 1e-6) {
      throw new Error("Octave Divider is missing its categorized Pitch row or seven parameter controls.");
    }

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      const enabled = index === 0 || index === 101;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    if (!checkbox.checked) checkbox.click();
    if (!checkbox.checked) {
      throw new Error("Octave Divider picker row did not enable from its click handler.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Octave Divider playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    const sampleCount = Math.min(actual.length, reference.samples.length);
    for (let index = 0; index < sampleCount; index += 1) {
      const sample = actual[index];
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = sampleCount > 0
      ? Math.sqrt(differenceSquares / sampleCount)
      : 0;
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-4) {
      throw new Error(`Octave Divider produced no measurable local guitar output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return {
      category: "Pitch", enabledEffectIndex: 101,
      inputGain: 1.1, voice1Level: 0.45, voice1Pan: -0.5,
      voice2Level: 0.25, voice2Pan: 0.5, mix: 0.8, outputLevel: 1,
      playOutputCaptured: true, peak, activeSamples, differenceRms,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      octaveDividerControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateOctaveDividerOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runDoubleTakeBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = doubleTakeControls.map((control) => control.value);
  const previousCategory = effectCategoryFilter.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Double Take browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    ["3", "0.65", "0.55", "0.75", "0.45", "0"].forEach((value, index) => {
      doubleTakeControls[index].value = value;
    });
    doubleTakeControls[1].dispatchEvent(new Event("input", { bubbles: true }));
    effectCategoryFilter.value = "modulation";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="102"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    if (row?.dataset.category !== "modulation" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Modulation" ||
        !checkbox || doubleTakeControls.length !== 6 ||
        effectMemory.length !== 110 || !(effectMemory[102] > 0) ||
        DEFAULT_PARAMETERS.doubleTake.join(",") !== "3,0.65,0.55,0.75,0.45,0") {
      throw new Error("Double Take is missing its categorized Modulation row or six controls.");
    }

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      const enabled = index === 0 || index === 102;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    if (!checkbox.checked) checkbox.click();
    if (!checkbox.checked) {
      throw new Error("Double Take picker row did not enable from its click handler.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Double Take playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    const sampleCount = Math.min(actual.length, reference.samples.length);
    for (let index = 0; index < sampleCount; index += 1) {
      const sample = actual[index];
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = sampleCount > 0
      ? Math.sqrt(differenceSquares / sampleCount)
      : 0;
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-4) {
      throw new Error(`Double Take produced no measurable local guitar output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return {
      category: "Modulation", enabledEffectIndex: 102,
      doubles: 3, slop: 0.65, sensitivity: 0.55,
      dryLevel: 0.75, wetLevel: 0.45, source: "Mono",
      playOutputCaptured: true, peak, activeSamples, differenceRms,
      effectMemoryBytes: effectMemory[102],
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      doubleTakeControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateDoubleTakeOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runEQ5GraphicBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = eq5GraphicControls.map((control) => control.value);
  const previousCategory = effectCategoryFilter.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("EQ5 Graphic browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    ["3", "-3", "6", "-3", "3", "0"].forEach((value, index) => {
      eq5GraphicControls[index].value = value;
    });
    eq5GraphicControls[2].dispatchEvent(new Event("input", { bubbles: true }));
    effectCategoryFilter.value = "eq";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="103"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    if (row?.dataset.category !== "eq" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Equalizer" ||
        !checkbox || eq5GraphicControls.length !== 6 ||
        effectMemory.length !== 110 || !(effectMemory[103] > 0) ||
        DEFAULT_PARAMETERS.eq5Graphic.join(",") !== "3,-3,6,-3,3,0") {
      throw new Error("EQ5 Graphic is missing its categorized Equalizer row or six controls.");
    }

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      const enabled = index === 0 || index === 103;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    if (!checkbox.checked) checkbox.click();
    if (!checkbox.checked) {
      throw new Error("EQ5 Graphic picker row did not enable from its click handler.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting EQ5 Graphic playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    const sampleCount = Math.min(actual.length, reference.samples.length);
    for (let index = 0; index < sampleCount; index += 1) {
      const sample = actual[index];
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = sampleCount > 0
      ? Math.sqrt(differenceSquares / sampleCount)
      : 0;
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-4) {
      throw new Error(`EQ5 Graphic produced no measurable local guitar output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return {
      category: "Equalizer", enabledEffectIndex: 103,
      bandGainsDb: [3, -3, 6, -3, 3], overallGainDb: 0,
      playOutputCaptured: true, peak, activeSamples, differenceRms,
      effectMemoryBytes: effectMemory[103],
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      eq5GraphicControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateEQ5GraphicOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runOpticalTremoloBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = opticalTremoloControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const previousCategory = effectCategoryFilter.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Optical Tremolo browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    const smokeValues = [1, 5.2, 65, 4, 0, 120, 1, 35, 58, 18, 10, 0, -36, 25, 75, -1, 0, 0];
    opticalTremoloControls.forEach((control, index) => {
      if (control.type === "checkbox") {
        control.checked = Boolean(smokeValues[index]);
      } else {
        control.value = String(smokeValues[index]);
      }
    });
    updateOpticalTremoloOptions();
    effectCategoryFilter.value = "modulation";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="104"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    if (row?.dataset.category !== "modulation" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Modulation" ||
        !checkbox || opticalTremoloControls.length !== 18 ||
        effectMemory.length !== 110 || !(effectMemory[104] > 0) ||
        opticalTremoloControls[3].disabled ||
        DEFAULT_PARAMETERS.opticalTremolo.join(",") !==
          "1,5.2,0.65,4,0,120,1,35,58,18,10,0,-36,25,0.75,-1,0,0") {
      throw new Error("Optical Tremolo is missing its Modulation row or documented control roles.");
    }

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      const enabled = index === 0 || index === 104;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    if (!checkbox.checked) checkbox.click();
    if (!checkbox.checked) {
      throw new Error("Optical Tremolo picker row did not enable from its click handler.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Optical Tremolo playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    const sampleCount = Math.min(actual.length, reference.samples.length);
    for (let index = 0; index < sampleCount; index += 1) {
      const sample = actual[index];
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = sampleCount > 0
      ? Math.sqrt(differenceSquares / sampleCount)
      : 0;
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-4) {
      throw new Error(`Optical Tremolo produced no measurable local guitar output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return {
      category: "Modulation", enabledEffectIndex: 104,
      variant: "Optical 2", lfo: "Saw Down", rateHz: 5.2, depth: 0.65,
      playOutputCaptured: true, peak, activeSamples, differenceRms,
      effectMemoryBytes: effectMemory[104],
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      opticalTremoloControls.forEach((control, index) => {
        if (control.type === "checkbox") {
          control.checked = previousControls[index];
        } else {
          control.value = previousControls[index];
        }
      });
      updateOpticalTremoloOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runFractalBiasTremBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = fractalBiasTremControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const previousCategory = effectCategoryFilter.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Bias Trem browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    const smokeValues = [3.7, 75, 1, 0, 120, 1, 0, -36, 0, 70, 100, 0, 0, 0];
    fractalBiasTremControls.forEach((control, index) => {
      if (control.type === "checkbox") {
        control.checked = Boolean(smokeValues[index]);
      } else {
        control.value = String(smokeValues[index]);
      }
    });
    updateFractalBiasTremOptions();
    effectCategoryFilter.value = "modulation";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="105"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    if (row?.dataset.category !== "modulation" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Modulation" ||
        !checkbox || fractalBiasTremControls.length !== 14 ||
        effectMemory.length !== 110 || !(effectMemory[105] > 0) ||
        DEFAULT_PARAMETERS.fractalBiasTrem.join(",") !==
          "3.7,0.75,1,0,120,1,0,-36,0,0.7,1,0,0,0") {
      throw new Error("Bias Trem is missing its Modulation row or project control mapping.");
    }

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      const enabled = index === 0 || index === 105;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    if (!checkbox.checked) checkbox.click();
    if (!checkbox.checked) {
      throw new Error("Bias Trem picker row did not enable from its click handler.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Bias Trem playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    const sampleCount = Math.min(actual.length, reference.samples.length);
    for (let index = 0; index < sampleCount; index += 1) {
      const sample = actual[index];
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = sampleCount > 0
      ? Math.sqrt(differenceSquares / sampleCount)
      : 0;
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-4) {
      throw new Error(`Bias Trem produced no measurable local guitar output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return {
      category: "Modulation", enabledEffectIndex: 105,
      waveform: "Triangle", rateHz: 3.7, depth: 0.75, biasDrive: 0.7,
      playOutputCaptured: true, peak, activeSamples, differenceRms,
      effectMemoryBytes: effectMemory[105],
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      fractalBiasTremControls.forEach((control, index) => {
        if (control.type === "checkbox") {
          control.checked = previousControls[index];
        } else {
          control.value = previousControls[index];
        }
      });
      updateFractalBiasTremOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runVibratoneBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = vibratoneControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const previousCategory = effectCategoryFilter.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Vibratone browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    const smokeValues = [30, 35, 3, 7.4, 1.1, 0, 450, 70, 1];
    vibratoneControls.forEach((control, index) => {
      if (control.type === "checkbox") {
        control.checked = Boolean(smokeValues[index]);
      } else {
        control.value = String(smokeValues[index]);
      }
    });
    updateVibratoneOptions();
    effectCategoryFilter.value = "modulation";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="106"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    if (row?.dataset.category !== "modulation" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Modulation" ||
        !checkbox || vibratoneControls.length !== 9 ||
        effectMemory.length !== 110 || !(effectMemory[106] > 0) ||
        DEFAULT_PARAMETERS.vibratone.join(",") !== "0.3,0.35,3,7.4,1.1,0,450,0.7,1") {
      throw new Error("Vibratone is missing its Modulation row or project control mapping.");
    }

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      const enabled = index === 0 || index === 106;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    if (!checkbox.checked) checkbox.click();
    if (!checkbox.checked) {
      throw new Error("Vibratone picker row did not enable from its click handler.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Vibratone playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    const sampleCount = Math.min(actual.length, reference.samples.length);
    for (let index = 0; index < sampleCount; index += 1) {
      const sample = actual[index];
      const magnitude = Math.abs(sample);
      peak = Math.max(peak, magnitude);
      if (magnitude > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = sampleCount > 0
      ? Math.sqrt(differenceSquares / sampleCount)
      : 0;
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-4) {
      throw new Error(`Vibratone produced no measurable local guitar output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return {
      category: "Modulation", enabledEffectIndex: 106,
      drive: 0.3, tone: 0.35, levelDb: 3, fastSpeedHz: 7.4,
      slowSpeedHz: 1.1, rampMs: 450, sensitivity: 0.7,
      dynamicsSwitch: true, playOutputCaptured: true, peak, activeSamples,
      differenceRms, effectMemoryBytes: effectMemory[106],
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      vibratoneControls.forEach((control, index) => {
        if (control.type === "checkbox") {
          control.checked = previousControls[index];
        } else {
          control.value = previousControls[index];
        }
      });
      updateVibratoneOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runUniVerseVibeBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = uniVerseVibeControls.map((control) => control.value);
  const previousCategory = effectCategoryFilter.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Uni-Verse Vibe browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    const smokeValues = [4.2, 82, 1.1, 0, 4, 120, 1, 1];
    uniVerseVibeControls.forEach((control, index) => {
      control.value = String(smokeValues[index]);
    });
    updateUniVerseVibeOptions();
    effectCategoryFilter.value = "modulation";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="107"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    if (row?.dataset.category !== "modulation" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Modulation" ||
        !row.querySelector(".effect-name")?.textContent.startsWith("Uni-Verse Vibe") ||
        !checkbox || uniVerseVibeControls.length !== 8 ||
        effectMemory.length !== 110 || !(effectMemory[107] > 0) ||
        DEFAULT_PARAMETERS.uniVerseVibe.join(",") !== "4.2,0.82,1.1,0,4,120,1,1") {
      throw new Error("Uni-Verse Vibe is missing its Modulation row or project control mapping.");
    }

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      const enabled = index === 0 || index === 107;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    if (!checkbox.checked) checkbox.click();
    if (!checkbox.checked) {
      throw new Error("Uni-Verse Vibe picker row did not enable from its click handler.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Uni-Verse Vibe playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    const sampleCount = Math.min(actual.length, reference.samples.length);
    for (let index = 0; index < sampleCount; index += 1) {
      const sample = actual[index];
      peak = Math.max(peak, Math.abs(sample));
      if (Math.abs(sample) > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = sampleCount > 0
      ? Math.sqrt(differenceSquares / sampleCount)
      : 0;
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-4) {
      throw new Error(`Uni-Verse Vibe produced no measurable guitar output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return {
      category: "Modulation", enabledEffectIndex: 107,
      speedHz: 4.2, intensity: 0.82, volume: 1.1,
      toneMode: "Vintage", modulationMode: "Vibrato",
      playOutputCaptured: true, peak, activeSamples, differenceRms,
      effectMemoryBytes: effectMemory[107],
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      uniVerseVibeControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateUniVerseVibeOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runFourOscGeneratorBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = fourOscGeneratorControls.map((control) => control.value);
  const previousCategory = effectCategoryFilter.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("4 OSC Generator browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    updateFourOscGenerator();
    effectCategoryFilter.value = "synth";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="108"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    if (row?.dataset.category !== "synth" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Synth" ||
        !row.querySelector(".effect-name")?.textContent.startsWith("4 OSC Generator") ||
        !checkbox || checkbox.checked || fourOscGeneratorControls.length !== 17 ||
        effectMemory.length !== 110 || !(effectMemory[108] > 0) ||
        DEFAULT_PARAMETERS.fourOscGenerator.join(",") !==
          "0,48,0,0.15,0,55,0,0.15,0,60,0,0.15,0,64,0,0.15,0.35") {
      throw new Error("4 OSC Generator is missing its categorized row, safe defaults, or controls.");
    }

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      const enabled = index === 0 || index === 12;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    checkbox.click();
    if (!checkbox.checked) {
      throw new Error("4 OSC Generator picker row did not enable from its click handler.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting 4 OSC Generator playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    const sampleCount = Math.min(actual.length, reference.samples.length);
    for (let index = 0; index < sampleCount; index += 1) {
      const sample = actual[index];
      peak = Math.max(peak, Math.abs(sample));
      if (Math.abs(sample) > 1e-6) activeSamples += 1;
      const difference = sample - reference.samples[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = sampleCount > 0
      ? Math.sqrt(differenceSquares / sampleCount)
      : 0;
    if (peak < 1e-5 || peak > 0.5 || activeSamples < 32 || differenceRms < 1e-4) {
      throw new Error(`4 OSC Generator produced unexpected output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return {
      category: "Synth", enabledEffectIndex: 108,
      sourceReplacesInput: true, downstreamHighGainDistortionEnabled: true,
      voices: 4, voiceNotes: [48, 55, 60, 64], outputLevel: 0.35,
      playOutputCaptured: true, peak, activeSamples, differenceRms,
      effectMemoryBytes: effectMemory[108],
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      fourOscGeneratorControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateFourOscGenerator();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runFractalPolyPitchRangeBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousCategory = effectCategoryFilter.value;
  const previousPitch = pitchSemitones.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Polyphonic pitch range smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    effectCategoryFilter.value = "pitch";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="22"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    if (row?.dataset.category !== "pitch" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Pitch" ||
        !row.querySelector(".effect-name")?.textContent.includes("Polyphonic pitch shifter") ||
        !checkbox || pitchSemitones.min !== "-24" ||
        pitchSemitones.max !== "24" || effectMemory.length !== 110 ||
        !(effectMemory[22] > 0)) {
      throw new Error(`Polyphonic pitch control mismatch: category=${row?.dataset.category}, ` +
        `tag=${row?.querySelector(".effect-category-tag")?.textContent.trim()}, ` +
        `name=${row?.querySelector(".effect-name")?.textContent}, ` +
        `checked=${checkbox?.checked}, min=${pitchSemitones.min}, max=${pitchSemitones.max}, ` +
        `memoryLength=${effectMemory.length}, effectMemory=${effectMemory[22]}.`);
    }

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      const enabled = index === 0 || index === 22;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    const captureAt = async (semitones) => {
      pitchSemitones.value = String(semitones);
      pitchSemitones.dispatchEvent(new Event("input", { bubbles: true }));
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
      Atomics.store(captureView, 0, 0);
      Atomics.store(captureView, 1, 0);
      playbackOffset = 0;
      playButton.click();
      await Promise.race([
        playTask,
        new Promise((_, reject) => setTimeout(
          () => reject(new Error(`Timed out starting ${semitones}-semitone pitch playback.`)),
          5000,
        )),
      ]);
      await waitForCapture(captureView, reference.samples.length);
      stopSource(false);
      return new Float32Array(captureBuffer).slice();
    };
    const at12 = await captureAt(12);
    const at24 = await captureAt(24);
    const atMinus12 = await captureAt(-12);
    const atMinus24 = await captureAt(-24);
    const compare = (first, second, label) => {
      let squares = 0;
      let changedSamples = 0;
      let peak = 0;
      for (let index = 0; index < second.length; index += 1) {
        const sample = second[index];
        if (!Number.isFinite(sample)) {
          throw new Error(`${label} output contains a non-finite sample at ${index}.`);
        }
        peak = Math.max(peak, Math.abs(sample));
        const delta = sample - first[index];
        squares += delta * delta;
        if (Math.abs(delta) > 1e-6) changedSamples += 1;
      }
      const differenceRms = second.length > 0
        ? Math.sqrt(squares / second.length) : 0;
      if (peak < 1e-5 || differenceRms < 1e-5 || changedSamples < 32) {
        throw new Error(`${label} did not produce a distinct shifted output.`);
      }
      return { peak, differenceRms, changedSamples };
    };
    return {
      category: "Pitch",
      enabledEffectIndex: 22,
      documentedRangeSemitones: [-24, 24],
      projectPitchEngine: "polyphonic STFT; no Fractal algorithm parity claimed",
      positiveEndpoint: compare(at12, at24, "+24 vs +12 semitones"),
      negativeEndpoint: compare(atMinus12, atMinus24, "-24 vs -12 semitones"),
      effectMemoryBytes: effectMemory[22],
      playOutputCaptured: true,
    };
  } finally {
    stopSource(false);
    pitchSemitones.value = previousPitch;
    pitchSemitones.dispatchEvent(new Event("input", { bubbles: true }));
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runFenderPedalShifterBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousCategory = effectCategoryFilter.value;
  const previousPitch = pitchSemitones.value;
  const previousMode = pitchMode.value;
  const previousTarget = pitchExpressionTarget.value;
  const previousPosition = pitchExpressionPosition.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Pedal Shifter smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    effectCategoryFilter.value = "pitch";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="22"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    if (row?.dataset.category !== "pitch" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Pitch" ||
        !row.querySelector(".effect-name")?.textContent.includes("Pedal Shifter") ||
        !checkbox || pitchMode.querySelector('option[value="pedal-shifter"]') === null ||
        pitchExpressionTarget.min !== "-24" || pitchExpressionTarget.max !== "24" ||
        pitchExpressionPosition.min !== "0" || pitchExpressionPosition.max !== "100" ||
        effectMemory.length !== 110 || !(effectMemory[22] > 0)) {
      throw new Error("Pedal Shifter controls are missing or incorrectly categorized.");
    }

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      const enabled = index === 0 || index === 22;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });

    let barrierSequence = 0;
    const waitForWorkletBarrier = () => new Promise((resolve, reject) => {
      const port = workletNode.port;
      const token = `pedal-shifter-${++barrierSequence}`;
      const timeout = setTimeout(() => {
        port.removeEventListener("message", onMessage);
        reject(new Error("Timed out waiting for worklet test barrier."));
      }, 5000);
      const onMessage = (event) => {
        if (event.data?.type !== "test-barrier-ack" || event.data.token !== token) return;
        clearTimeout(timeout);
        port.removeEventListener("message", onMessage);
        resolve();
      };
      port.addEventListener("message", onMessage);
      port.postMessage({ type: "test-barrier", token });
    });

    const captureAt = async ({ mode, semitones = 0, target = 12, position = 0 }) => {
      pitchSemitones.value = String(semitones);
      pitchSemitones.dispatchEvent(new Event("input", { bubbles: true }));
      pitchExpressionTarget.value = String(target);
      pitchExpressionTarget.dispatchEvent(new Event("input", { bubbles: true }));
      pitchExpressionPosition.value = String(position);
      pitchExpressionPosition.dispatchEvent(new Event("input", { bubbles: true }));
      pitchMode.value = mode;
      pitchMode.dispatchEvent(new Event("change", { bubbles: true }));
      await waitForWorkletBarrier();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
      await waitForWorkletBarrier();
      Atomics.store(captureView, 0, 0);
      Atomics.store(captureView, 1, 0);
      playbackOffset = 0;
      playButton.click();
      await Promise.race([
        playTask,
        new Promise((_, reject) => setTimeout(
          () => reject(new Error(`Timed out starting ${mode} pitch playback.`)), 5000,
        )),
      ]);
      await waitForCapture(captureView, reference.samples.length);
      stopSource(false);
      return new Float32Array(captureBuffer).slice();
    };

    const pedalHeel = await captureAt({ mode: "pedal-shifter", target: 12, position: 0 });
    const pedalMid = await captureAt({ mode: "pedal-shifter", target: 12, position: 50 });
    const staticMid = await captureAt({ mode: "static", semitones: 6 });
    const pedalToe = await captureAt({ mode: "pedal-shifter", target: 12, position: 100 });
    const staticToe = await captureAt({ mode: "static", semitones: 12 });

    const compareEquivalent = (first, second, label) => {
      let maxError = 0;
      let errorSquares = 0;
      for (let index = 0; index < first.length; index += 1) {
        if (!Number.isFinite(first[index]) || !Number.isFinite(second[index])) {
          throw new Error(`${label} contains non-finite audio.`);
        }
        const error = first[index] - second[index];
        maxError = Math.max(maxError, Math.abs(error));
        errorSquares += error * error;
      }
      const rmsError = Math.sqrt(errorSquares / first.length);
      return { maxError, rmsError };
    };
    const midpointMatch = compareEquivalent(pedalMid, staticMid, "50% expression position");
    const toeMatch = compareEquivalent(pedalToe, staticToe, "100% expression position");
    if (midpointMatch.maxError >= 2e-5 || midpointMatch.rmsError >= 2e-6 ||
        toeMatch.maxError >= 2e-5 || toeMatch.rmsError >= 2e-6) {
      throw new Error("Expression pitch does not match the equivalent static pitch.");
    }
    const compareSweep = (first, second) => {
      let squares = 0;
      let changedSamples = 0;
      for (let index = 0; index < first.length; index += 1) {
        if (!Number.isFinite(first[index]) || !Number.isFinite(second[index])) {
          throw new Error("Pedal Shifter sweep contains non-finite audio.");
        }
        const difference = first[index] - second[index];
        squares += difference * difference;
        if (Math.abs(difference) > 1e-6) changedSamples += 1;
      }
      return { differenceRms: Math.sqrt(squares / first.length), changedSamples };
    };
    const heelToMid = compareSweep(pedalHeel, pedalMid);
    const heelToToe = compareSweep(pedalHeel, pedalToe);
    if (heelToMid.differenceRms < 1e-5 || heelToMid.changedSamples < 32 ||
        heelToToe.differenceRms < 1e-5 || heelToToe.changedSamples < 32) {
      throw new Error("Pedal Shifter POSITION did not produce distinct heel, midpoint, and toe output.");
    }
    let sweepDifference = 0;
    for (let index = 0; index < pedalHeel.length; index += 1) {
      sweepDifference = Math.max(sweepDifference, Math.abs(pedalMid[index] - pedalHeel[index]));
    }
    if (sweepDifference < 1e-5) {
      throw new Error("Pedal Shifter POSITION did not change the captured guitar output.");
    }
    return {
      category: "Pitch", pickerIndex: 22, mode: "Pedal Shifter",
      targetSemitones: 12, positionsPercent: [0, 50, 100],
      midpointStaticWaveformDiagnostic: midpointMatch,
      toeStaticWaveformDiagnostic: toeMatch, heelToMid, heelToToe, sweepDifference,
      expressionControl: "software slider; no physical controller route", playOutputCaptured: true,
      effectMemoryBytes: effectMemory[22],
    };
  } finally {
    stopSource(false);
    pitchMode.value = previousMode;
    pitchExpressionTarget.value = previousTarget;
    pitchExpressionPosition.value = previousPosition;
    pitchSemitones.value = previousPitch;
    pitchMode.dispatchEvent(new Event("change", { bubbles: true }));
    pitchSemitones.dispatchEvent(new Event("input", { bubbles: true }));
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runFractalDiatonicCustomScaleBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousCategory = effectCategoryFilter.value;
  const previousHarmonizer = diatonicHarmonizerControls.map((control) => control.value);
  const previousScaleCount = diatonicCustomScaleCount.value;
  const previousCustomNotes = diatonicCustomScaleNotes.map((control) => control.value);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Diatonic custom-scale smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    effectCategoryFilter.value = "pitch";
    updateEffectBrowser();

    const effectIndex = 50;
    const row = document.querySelector(`[data-effect="${effectIndex}"]`)?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    const customOption = diatonicHarmonizerControls[3].querySelector('option[value="13"]');
    if (row?.dataset.category !== "pitch" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Pitch" ||
        !row.querySelector(".effect-name")?.textContent.includes("Diatonic Harmonizer") ||
        !checkbox || !customOption || effectMemory.length !== 110 ||
        !(effectMemory[effectIndex] > 0) || diatonicCustomScaleNotes.length !== 8) {
      throw new Error("Diatonic custom scale is missing its categorized picker row or controls.");
    }

    document.querySelectorAll("[data-effect]").forEach((box, index) => {
      const enabled = index === 0 || index === effectIndex;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    const setMainControl = (index, value) => {
      diatonicHarmonizerControls[index].value = String(value);
      diatonicHarmonizerControls[index].dispatchEvent(
        new Event("input", { bubbles: true }),
      );
    };
    setMainControl(0, 1);
    setMainControl(1, 2);
    setMainControl(2, 0);
    setMainControl(3, 0);
    setMainControl(4, 8000);
    setMainControl(5, 0);
    setMainControl(6, 0);
    setMainControl(7, 0);
    setMainControl(8, 120);

    const captureAt = async (label) => {
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
      Atomics.store(captureView, 0, 0);
      Atomics.store(captureView, 1, 0);
      playbackOffset = 0;
      playButton.click();
      await Promise.race([
        playTask,
        new Promise((_, reject) => setTimeout(
          () => reject(new Error(`Timed out starting ${label} harmonizer playback.`)),
          5000,
        )),
      ]);
      await waitForCapture(captureView, reference.samples.length);
      stopSource(false);
      return new Float32Array(captureBuffer).slice();
    };

    const majorOutput = await captureAt("major-scale");
    diatonicHarmonizerControls[3].value = "13";
    diatonicCustomScaleCount.value = "4";
    [0, 2, 5, 7, 0, 0, 0, 0].forEach((value, index) => {
      diatonicCustomScaleNotes[index].value = String(value);
    });
    diatonicHarmonizerControls[3].dispatchEvent(new Event("input", { bubbles: true }));
    diatonicCustomScaleCount.dispatchEvent(new Event("input", { bubbles: true }));
    const customOutput = await captureAt("custom-scale");

    let squareSum = 0;
    let changedSamples = 0;
    let peak = 0;
    for (let index = 0; index < customOutput.length; index += 1) {
      const sample = customOutput[index];
      if (!Number.isFinite(sample)) {
        throw new Error(`Custom-scale output contains a non-finite sample at ${index}.`);
      }
      peak = Math.max(peak, Math.abs(sample));
      const delta = sample - majorOutput[index];
      squareSum += delta * delta;
      if (Math.abs(delta) > 1e-6) changedSamples += 1;
    }
    const differenceRms = customOutput.length > 0
      ? Math.sqrt(squareSum / customOutput.length) : 0;
    if (peak < 1e-5 || differenceRms < 1e-5 || changedSamples < 32) {
      throw new Error("Custom scale did not produce an audible browser output change.");
    }
    return {
      category: "Pitch",
      enabledEffectIndex: effectIndex,
      customScaleNoteCount: Number(diatonicCustomScaleCount.value),
      customScaleSemitoneOffsets: DEFAULT_PARAMETERS.diatonicHarmonizerCustomScale.slice(1),
      scaleComparison: { peak, differenceRms, changedSamples },
      effectMemoryBytes: effectMemory[effectIndex],
      playOutputCaptured: true,
    };
  } finally {
    stopSource(false);
    previousHarmonizer.forEach((value, index) => {
      diatonicHarmonizerControls[index].value = value;
    });
    diatonicCustomScaleCount.value = previousScaleCount;
    previousCustomNotes.forEach((value, index) => {
      diatonicCustomScaleNotes[index].value = value;
    });
    updateDiatonicHarmonizer();
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      document.querySelectorAll("[data-effect]").forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runPolyDetuneBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = polyDetuneControls.map((control) => control.value);
  const previousCategory = effectCategoryFilter.value;
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Poly Detune browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    updatePolyDetune();
    effectCategoryFilter.value = "modulation";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="109"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    if (row?.dataset.category !== "modulation" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Modulation" ||
        !row.querySelector(".effect-name")?.textContent.startsWith("Poly Detune") ||
        !checkbox || checkbox.checked || polyDetuneControls.length !== 6 ||
        effectMemory.length !== 110 || !(effectMemory[109] > 0) ||
        DEFAULT_PARAMETERS.polyDetune.join(",") !== "2,0.25,0.75,0.75,0.25,0") {
      throw new Error("Poly Detune is missing its categorized row, safe defaults, or controls.");
    }

    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      const enabled = index === 0;
      box.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    const capturePlay = async (label) => {
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
      Atomics.store(captureView, 0, 0);
      Atomics.store(captureView, 1, 0);
      playbackOffset = 0;
      playButton.click();
      await Promise.race([
        playTask,
        new Promise((_, reject) => setTimeout(
          () => reject(new Error(`Timed out starting Poly Detune ${label} playback.`)), 5000,
        )),
      ]);
      await waitForCapture(captureView, reference.samples.length);
      stopSource(false);
      return new Float32Array(captureBuffer).slice();
    };

    const dry = await capturePlay("dry");
    checkbox.click();
    if (!checkbox.checked) {
      throw new Error("Poly Detune picker row did not enable from its click handler.");
    }
    const wet = await capturePlay("effect");
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    for (let index = 0; index < wet.length; index += 1) {
      const sample = wet[index];
      if (!Number.isFinite(sample)) {
        throw new Error(`Poly Detune output contains a non-finite sample at ${index}.`);
      }
      peak = Math.max(peak, Math.abs(sample));
      if (Math.abs(sample) > 1e-6) activeSamples += 1;
      const difference = sample - dry[index];
      differenceSquares += difference * difference;
    }
    const differenceRms = wet.length > 0
      ? Math.sqrt(differenceSquares / wet.length) : 0;
    if (peak < 1e-5 || peak > 2 || activeSamples < 32 || differenceRms < 1e-5) {
      throw new Error(`Poly Detune produced unexpected output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return {
      category: "Modulation", enabledEffectIndex: 109,
      projectAuthoredModulatedDelay: true, pitchTracking: false,
      parameters: DEFAULT_PARAMETERS.polyDetune,
      playOutputCaptured: true, peak, activeSamples, differenceRms,
      effectMemoryBytes: effectMemory[109],
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      polyDetuneControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updatePolyDetune();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runStereoFilterBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = stereoFilterControls.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const previousCategory = effectCategoryFilter.value;
  const postedStereoFilterMessages = [];
  const portPrototype = MessagePort.prototype;
  const originalPost = portPrototype.postMessage;
  portPrototype.postMessage = function(message, ...transfer) {
    if (message?.type === "stereo-filter" ||
        (message?.type === "enable" && message.index === 96)) {
      postedStereoFilterMessages.push(message);
    }
    return originalPost.call(this, message, ...transfer);
  };
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext ||
        decodedAudio.numberOfChannels !== reference.channels) {
      throw new Error("Stereo Filter browser smoke requires the local guitar fixture.");
    }
    await audioContext.resume();
    stopSource(false);
    stereoFilterControls[0].value = "15";
    stereoFilterControls[1].value = "300";
    stereoFilterControls[2].value = "4";
    stereoFilterControls[3].value = "4";
    stereoFilterControls[4].value = "0";
    stereoFilterControls[5].value = "0";
    stereoFilterControls[6].value = "0";
    stereoFilterControls[7].value = "-100";
    stereoFilterControls[8].value = "100";
    stereoFilterControls[9].value = "0";
    stereoFilterControls[10].value = "5";
    stereoFilterControls[11].value = "0.5";
    stereoFilterControls[12].value = "0.5";
    stereoFilterControls[13].value = "0";
    stereoFilterControls[14].value = "0";
    stereoFilterControls[15].checked = false;
    stereoFilterControls[16].value = "1";
    stereoFilterControls[17].value = "2";
    stereoFilterControls[18].value = "50";
    stereoFilterControls[19].value = "6000";
    stereoFilterControls[20].value = "0";
    stereoFilterControls[21].value = "1";
    stereoFilterControls[22].value = "2";
    stereoFilterControls[23].checked = true;
    stereoFilterControls[24].value = "120";
    stereoFilterControls[25].value = "1";
    stereoFilterControls[26].value = "-36";
    stereoFilterControls[27].value = "5";
    stereoFilterControls[28].value = "80";
    stereoFilterControls[29].value = "0";
    updateStereoFilterOptions();
    if (DEFAULT_PARAMETERS.stereoFilter.length !== 34 ||
        DEFAULT_PARAMETERS.stereoFilter[0] !== 15 ||
        DEFAULT_PARAMETERS.stereoFilter[1] !== 300 ||
        DEFAULT_PARAMETERS.stereoFilter[2] !== 4) {
      throw new Error("Stereo Filter controls did not produce the expected 34-value API vector.");
    }
    stereoFilterControls[0].value = "16";
    updateStereoFilterOptions();
    const postedEnvelopeFilter = postedStereoFilterMessages.find(
      (message) => message.type === "stereo-filter" &&
        message.value?.length === 34 && message.value?.[0] === 16,
    );
    if (!postedEnvelopeFilter || !stereoFilterControls[15].disabled ||
        stereoFilterControls[26].parentElement.hidden ||
        !stereoFilterControls[23].parentElement.hidden ||
        postedEnvelopeFilter.value[21] !== 1 ||
        postedEnvelopeFilter.value[26] !== -36 ||
        postedEnvelopeFilter.value[27] !== 5 ||
        postedEnvelopeFilter.value[28] !== 80 ||
        postedEnvelopeFilter.value[29] !== 0) {
      throw new Error("Envelope Filter controls did not reach the 34-value Worklet API.");
    }
    stereoFilterControls[0].value = "17";
    stereoFilterControls[1].value = "700";
    stereoFilterControls[15].checked = false;
    stereoFilterControls[21].value = "0";
    stereoFilterControls[23].checked = false;
    stereoFilterControls[30].value = "0.65";
    stereoFilterControls[31].value = "0.35";
    stereoFilterControls[32].value = "0.55";
    stereoFilterControls[33].value = "2";
    updateStereoFilterOptions();
    const postedLadderFilter = postedStereoFilterMessages.find(
      (message) => message.type === "stereo-filter" &&
        message.value?.length === 34 && message.value?.[0] === 17,
    );
    if (!postedLadderFilter ||
        stereoFilterControls[2].parentElement.hidden !== true ||
        stereoFilterControls[3].parentElement.hidden !== true ||
        stereoFilterControls[21].parentElement.hidden ||
        stereoFilterControls[30].parentElement.hidden ||
        stereoFilterControls[31].parentElement.hidden ||
        stereoFilterControls[32].parentElement.hidden ||
        stereoFilterControls[33].parentElement.hidden ||
        stereoFilterControls[32].disabled ||
        postedLadderFilter.value[0] !== 17 ||
        postedLadderFilter.value[1] !== 700 ||
        postedLadderFilter.value[21] !== 0 ||
        postedLadderFilter.value[30] !== 0.65 ||
        postedLadderFilter.value[31] !== 0.35 ||
        postedLadderFilter.value[32] !== 0.55 ||
        postedLadderFilter.value[33] !== 2) {
      throw new Error("Ladder Filter mode and project controls did not reach the 34-value Worklet API.");
    }
    stereoFilterControls[0].value = "18";
    stereoFilterControls[1].value = "100";
    stereoFilterControls[15].checked = true;
    stereoFilterControls[19].value = "10000";
    stereoFilterControls[21].value = "1";
    stereoFilterControls[26].value = "-24";
    stereoFilterControls[27].value = "10";
    stereoFilterControls[28].value = "100";
    stereoFilterControls[29].value = "25";
    updateStereoFilterOptions();
    const postedTouchLadder = postedStereoFilterMessages.find(
      (message) => message.type === "stereo-filter" &&
        message.value?.length === 34 && message.value?.[0] === 18,
    );
    if (!postedTouchLadder ||
        !stereoFilterControls[15].disabled ||
        !stereoFilterControls[16].parentElement.hidden ||
        !stereoFilterControls[23].parentElement.hidden ||
        stereoFilterControls[21].parentElement.hidden ||
        stereoFilterControls[22].parentElement.hidden ||
        stereoFilterControls[26].parentElement.hidden ||
        stereoFilterControls[30].parentElement.hidden ||
        !stereoFilterControls[32].disabled ||
        !stereoFilterControls[21].querySelector('option[value="2"]').disabled ||
        postedTouchLadder.value[1] !== 100 ||
        postedTouchLadder.value[15] !== 0 ||
        postedTouchLadder.value[19] !== 10000 ||
        postedTouchLadder.value[21] !== 1 ||
        postedTouchLadder.value[26] !== -24 ||
        postedTouchLadder.value[27] !== 10 ||
        postedTouchLadder.value[28] !== 100 ||
        postedTouchLadder.value[29] !== 0.25) {
      throw new Error("Touch-Ladder envelope and shared Ladder controls did not reach the 34-value Worklet API.");
    }
    stereoFilterControls[0].value = "19";
    stereoFilterControls[1].value = "100";
    stereoFilterControls[15].checked = true;
    stereoFilterControls[19].value = "10000";
    stereoFilterControls[21].value = "2";
    stereoFilterControls[26].value = "-24";
    stereoFilterControls[27].value = "10";
    stereoFilterControls[28].value = "100";
    stereoFilterControls[29].value = "25";
    updateStereoFilterOptions();
    const postedTouchWah = postedStereoFilterMessages.find(
      (message) => message.type === "stereo-filter" &&
        message.value?.length === 34 && message.value?.[0] === 19,
    );
    if (!postedTouchWah ||
        !stereoFilterControls[15].disabled ||
        !stereoFilterControls[16].parentElement.hidden ||
        stereoFilterControls[21].parentElement.hidden ||
        stereoFilterControls[22].parentElement.hidden ||
        stereoFilterControls[26].parentElement.hidden ||
        stereoFilterControls[30].parentElement.hidden === false ||
        stereoFilterControls[21].querySelector('option[value="2"]').disabled ||
        postedTouchWah.value[1] !== 100 ||
        postedTouchWah.value[15] !== 0 ||
        postedTouchWah.value[19] !== 10000 ||
        postedTouchWah.value[21] !== 2 ||
        postedTouchWah.value[26] !== -24 ||
        postedTouchWah.value[27] !== 10 ||
        postedTouchWah.value[28] !== 100 ||
        postedTouchWah.value[29] !== 0.25) {
      throw new Error("Touch-Wah mode and direct-envelope controls did not reach the 34-value Worklet API.");
    }
    effectCategoryFilter.value = "filter";
    updateEffectBrowser();

    const row = document.querySelector('[data-effect="96"]')?.closest(".effect-row");
    const checkbox = row?.querySelector("[data-effect]");
    if (row?.dataset.category !== "filter" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Filters & wah" ||
        !checkbox) {
      throw new Error("Stereo Filter is missing from the Filters & wah picker category.");
    }
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((box, index) => {
      box.checked = false;
      workletNode.port.postMessage({ type: "enable", index, value: false });
    });
    checkbox.click();
    if (!checkbox.checked) {
      throw new Error("Stereo Filter picker row did not enable from its click handler.");
    }
    const postedConfiguration = postedStereoFilterMessages.find(
      (message) => message.type === "stereo-filter" &&
        message.value?.length === 34 && message.value?.[0] === 19,
    );
    const postedEnable = postedStereoFilterMessages.find(
      (message) => message.type === "enable" && message.index === 96 && message.value,
    );
    if (!postedConfiguration || !postedEnable) {
      throw new Error("Stereo Filter settings or ID 96 enable message did not reach the Worklet port.");
    }
    const expectedStereoFilter = [
      19, 100, 4, 4, 0, 0, 0, -100, 100, 0, 5, 0.5, 0.5, 0, 0,
      0, 1, 2, 50, 10000, 0, 2, 2, 0, 120, 1, -24, 10, 100, 0.25,
      0.65, 0.35, 0.55, 2,
    ];
    if (JSON.stringify(postedConfiguration.value) !== JSON.stringify(expectedStereoFilter)) {
      throw new Error("Stereo Filter Worklet message does not match the 34-value Touch-Wah API order.");
    }
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);

    const actual = new Float32Array(captureBuffer);
    let peak = 0;
    let activeSamples = 0;
    let differenceSquares = 0;
    const frames = Math.min(decodedAudio.length,
      Math.floor(actual.length / reference.channels));
    for (let frame = 0; frame < frames; frame += 1) {
      for (let channel = 0; channel < reference.channels; channel += 1) {
        const value = actual[frame * reference.channels + channel];
        peak = Math.max(peak, Math.abs(value));
        if (Math.abs(value) > 1e-6) activeSamples += 1;
        const source = decodedAudio.getChannelData(
          Math.min(channel, decodedAudio.numberOfChannels - 1),
        )[frame];
        const difference = value - source;
        differenceSquares += difference * difference;
      }
    }
    const differenceRms = Math.sqrt(
      differenceSquares / Math.max(1, frames * reference.channels),
    );
    if (peak < 1e-5 || activeSamples < 32 || differenceRms < 1e-4) {
      throw new Error(`Stereo Filter produced no measurable guitar output (peak ${peak}, difference RMS ${differenceRms}).`);
    }
    return { category: "Filters & wah",
      type: stereoFilterControls[0].selectedOptions[0]?.textContent ?? "unknown",
      enabledEffectIndex: 96, playOutputCaptured: true,
      peak, activeSamples, differenceRms };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      stereoFilterControls.forEach((control, index) => {
        if (control.type === "checkbox") {
          control.checked = Boolean(previousControls[index]);
        } else {
          control.value = previousControls[index];
        }
      });
      updateStereoFilterOptions();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((box, index) => {
        box.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      effectCategoryFilter.value = previousCategory;
      updateEffectBrowser();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
    portPrototype.postMessage = originalPost;
  }
}

async function runStereoCrossoverBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = Object.values(stereoCrossoverControls)
    .map((control) => control.value);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error(
        "The Stereo Crossover smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    const nativeResponse = await fetch("/__test_stereo_crossover_expected.wav");
    if (!nativeResponse.ok) {
      throw new Error("The native Stereo Crossover reference was not served.");
    }
    const nativeExpected = readFloatWave(await nativeResponse.arrayBuffer());
    if (nativeExpected.samples.length !== reference.samples.length ||
        nativeExpected.channels !== reference.channels) {
      throw new Error("The native Stereo Crossover reference has an unexpected format.");
    }
    const values = {
      ...DEFAULT_PARAMETERS.stereoCrossover,
      frequencyHz: 500,
      frequencyMultiplier: "x1",
      lowLevelLeftDb: -60,
      lowLevelRightDb: -60,
      highLevelLeftDb: 0,
      highLevelRightDb: 0,
      levelDb: 0,
      balance: 0,
    };
    for (const [key, value] of Object.entries(values)) {
      stereoCrossoverControls[key].value = String(value);
    }
    updateStereoCrossover();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    const capture = async (crossoverEnabled) => {
      checkboxes.forEach((checkbox, index) => {
        const enabled = index === 86 && crossoverEnabled;
        checkbox.checked = enabled;
        workletNode.port.postMessage({ type: "enable", index, value: enabled });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
      Atomics.store(captureView, 0, 0);
      Atomics.store(captureView, 1, 0);
      playbackOffset = 0;
      playButton.click();
      await Promise.race([
        playTask,
        new Promise((_, reject) => setTimeout(
          () => reject(new Error("Timed out starting Stereo Crossover playback.")), 5000,
        )),
      ]);
      await waitForCapture(captureView, reference.samples.length);
      stopSource(false);
      return Float32Array.from(new Float32Array(captureBuffer));
    };
    const dry = await capture(false);
    const processed = await capture(true);
    let maximumDifferenceFromDry = 0;
    let differenceFromDrySquares = 0;
    let changedSamplesFromDry = 0;
    let maximumDifferenceFromNative = 0;
    let differenceFromNativeSquares = 0;
    for (let index = 0; index < processed.length; index += 1) {
      const differenceFromDry = Math.abs(processed[index] - dry[index]);
      const differenceFromNative = Math.abs(processed[index] - nativeExpected.samples[index]);
      maximumDifferenceFromDry = Math.max(maximumDifferenceFromDry, differenceFromDry);
      differenceFromDrySquares += differenceFromDry * differenceFromDry;
      if (differenceFromDry > 1e-6) changedSamplesFromDry += 1;
      maximumDifferenceFromNative = Math.max(maximumDifferenceFromNative, differenceFromNative);
      differenceFromNativeSquares += differenceFromNative * differenceFromNative;
    }
    const rmsDifferenceFromDry = Math.sqrt(
      differenceFromDrySquares / Math.max(1, processed.length),
    );
    const rmsDifferenceFromNative = Math.sqrt(
      differenceFromNativeSquares / Math.max(1, processed.length),
    );
    if (maximumDifferenceFromDry < 1e-4 || rmsDifferenceFromDry < 1e-5 ||
        changedSamplesFromDry < 32) {
      throw new Error(
        `The XVR-inspired stage did not alter local guitar output: ${JSON.stringify({ maximumDifferenceFromDry, rmsDifferenceFromDry, changedSamplesFromDry })}`);
    }
    if (maximumDifferenceFromNative > 1e-4 || rmsDifferenceFromNative > 1e-5) {
      throw new Error(
        `Browser Stereo Crossover did not match the native reference: ${JSON.stringify({ maximumDifferenceFromNative, rmsDifferenceFromNative })}`);
    }
    return {
      enabledEffectIndex: 86,
      topology: "two-way LR4 stereo crossover",
      frequencyHz: values.frequencyHz,
      frequencyMultiplier: values.frequencyMultiplier,
      lowLevelLeftDb: values.lowLevelLeftDb,
      lowLevelRightDb: values.lowLevelRightDb,
      highLevelLeftDb: values.highLevelLeftDb,
      highLevelRightDb: values.highLevelRightDb,
      outputLevelDb: values.levelDb,
      balance: values.balance,
      rmsDifferenceFromDry,
      maximumDifferenceFromDry,
      changedSamplesFromDry,
      rmsDifferenceFromNative,
      maximumDifferenceFromNative,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      Object.values(stereoCrossoverControls).forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateStereoCrossover();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

function interleaveDecodedAudio(audio) {
  const channelCount = audio.numberOfChannels;
  const output = new Float32Array(audio.length * channelCount);
  const channels = Array.from({ length: channelCount }, (_, index) =>
    audio.getChannelData(index));
  for (let frame = 0; frame < audio.length; frame += 1) {
    for (let channel = 0; channel < channelCount; channel += 1) {
      output[frame * channelCount + channel] = channels[channel][frame];
    }
  }
  return output;
}

async function runAshevillePattrnBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const previousControls = [
    ashevillePattrnBank.value, ashevillePattrnPattern.value,
    ashevillePattrnDrive.value, ashevillePattrnOutput.value,
    ashevillePattrnMix.value, ashevillePattrnRate.value,
    ashevillePattrnQ.value, ashevillePattrnEnvelope.value,
    ashevillePattrnSmoothing.value,
    ...ashevillePattrnBandLevels.map(({ input }) => input.value),
  ];
  const previousParameters = [...DEFAULT_PARAMETERS.ashevillePattrn];
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error(
        "Asheville Pattrn smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    ashevillePattrnBank.value = "1";
    ashevillePattrnPattern.value = "5";
    [
      ashevillePattrnDrive.value, ashevillePattrnOutput.value,
      ashevillePattrnMix.value, ashevillePattrnRate.value,
      ashevillePattrnQ.value, ashevillePattrnEnvelope.value,
      ashevillePattrnSmoothing.value,
    ] = ["8", "-3", "85", "1.7", "5.5", "0.7", "10"];
    const bandLevels = [0.2, 0.85, 0.3, 1, 0.35, 0.9, 0.25, 0.7];
    ashevillePattrnBandLevels.forEach(({ input }, index) => {
      input.value = String(bandLevels[index]);
    });
    updateAshevillePattrn();
    if (decodedAudio.numberOfChannels !== reference.channels ||
        decodedAudio.length * reference.channels !== reference.samples.length) {
      throw new Error("Asheville Pattrn smoke requires a matching local guitar fixture.");
    }
    const dry = interleaveDecodedAudio(decodedAudio);
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    const capture = async () => {
      checkboxes.forEach((checkbox, index) => {
        const enabled = index === 87;
        checkbox.checked = enabled;
        workletNode.port.postMessage({ type: "enable", index, value: enabled });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
      Atomics.store(captureView, 0, 0);
      Atomics.store(captureView, 1, 0);
      playbackOffset = 0;
      playButton.click();
      await Promise.race([
        playTask,
        new Promise((_, reject) => setTimeout(
          () => reject(new Error("Timed out starting Asheville Pattrn playback.")), 5000,
        )),
      ]);
      await waitForCapture(captureView, reference.samples.length);
      stopSource(false);
      return Float32Array.from(new Float32Array(captureBuffer));
    };
    const processed = await capture();
    let peak = 0;
    let squares = 0;
    let maximumDifferenceFromDry = 0;
    let differenceFromDrySquares = 0;
    let changedSamplesFromDry = 0;
    for (let index = 0; index < processed.length; index += 1) {
      const sample = processed[index];
      const drySample = dry[index];
      if (!Number.isFinite(sample) || !Number.isFinite(drySample)) {
        throw new Error(
          "Asheville Pattrn produced a non-finite sample at frame " + index + ".");
      }
      peak = Math.max(peak, Math.abs(sample));
      squares += sample * sample;
      const difference = Math.abs(sample - drySample);
      maximumDifferenceFromDry = Math.max(maximumDifferenceFromDry, difference);
      differenceFromDrySquares += difference * difference;
      if (difference > 1e-6) changedSamplesFromDry += 1;
    }
    const rms = Math.sqrt(squares / Math.max(1, processed.length));
    const rmsDifferenceFromDry = Math.sqrt(
      differenceFromDrySquares / Math.max(1, processed.length),
    );
    if (peak < 1e-5 || rms < 1e-6 || maximumDifferenceFromDry < 1e-4 ||
        rmsDifferenceFromDry < 1e-5 || changedSamplesFromDry < 32) {
      throw new Error(
        "Asheville Pattrn did not produce finite, changed local guitar output: " +
        JSON.stringify({ peak, rms, maximumDifferenceFromDry, rmsDifferenceFromDry, changedSamplesFromDry }));
    }
    return {
      enabledEffectIndex: 87,
      bank: "MIDS",
      pattern: 5,
      bandLevels,
      driveDb: 8,
      outputDb: -3,
      mix: 0.85,
      rateHz: 1.7,
      resonanceQ: 5.5,
      envelopeShape: 0.7,
      smoothingMs: 10,
      peak,
      rms,
      maximumDifferenceFromDry,
      rmsDifferenceFromDry,
      changedSamplesFromDry,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      [
        ashevillePattrnBank.value, ashevillePattrnPattern.value,
        ashevillePattrnDrive.value, ashevillePattrnOutput.value,
        ashevillePattrnMix.value, ashevillePattrnRate.value,
        ashevillePattrnQ.value, ashevillePattrnEnvelope.value,
        ashevillePattrnSmoothing.value,
      ] = previousControls.slice(0, 9);
      ashevillePattrnBandLevels.forEach(({ input }, index) => {
        input.value = previousControls[index + 9];
      });
      updateAshevillePattrn();
      DEFAULT_PARAMETERS.ashevillePattrn = previousParameters;
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runSweepDelayBrowserSmokeTest(reference, captureView, captureBuffer) {
  const previousEffectState = enabledEffects();
  const controls = [
    sweepDelayTime, sweepDelayFeedback, sweepDelayMix, sweepDelayStart,
    sweepDelayStop, sweepDelayResonance, sweepDelayRate, sweepDelayWaveform,
    sweepDelayPhase, sweepDelayFilterType, sweepDelayRateMode,
    sweepDelayTempoBpm, sweepDelayDivision,
  ];
  const previousControls = controls.map((control) => control.value);
  const previousParameters = [...DEFAULT_PARAMETERS.sweepDelay];
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error(
        "Sweep Delay smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    [
      sweepDelayTime.value, sweepDelayFeedback.value, sweepDelayMix.value,
      sweepDelayStart.value, sweepDelayStop.value, sweepDelayResonance.value,
      sweepDelayRate.value, sweepDelayWaveform.value, sweepDelayPhase.value,
      sweepDelayFilterType.value, sweepDelayRateMode.value,
      sweepDelayTempoBpm.value, sweepDelayDivision.value,
    ] = ["10", "0", "100", "250", "6000", "1.2", "0.5", "0", "90",
      "0", "1", "120", "4"];
    updateSweepDelay();
    if (decodedAudio.numberOfChannels !== reference.channels ||
        decodedAudio.length * reference.channels !== reference.samples.length) {
      throw new Error("Sweep Delay smoke requires a matching local guitar fixture.");
    }
    const dry = interleaveDecodedAudio(decodedAudio);
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    checkboxes.forEach((checkbox, index) => {
      const enabled = index === 42;
      checkbox.checked = enabled;
      workletNode.port.postMessage({ type: "enable", index, value: enabled });
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting Sweep Delay playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const processed = Float32Array.from(new Float32Array(captureBuffer));
    let peak = 0;
    let squares = 0;
    let differenceSquares = 0;
    let changedSamples = 0;
    for (let index = 0; index < processed.length; index += 1) {
      const sample = processed[index];
      if (!Number.isFinite(sample) || !Number.isFinite(dry[index])) {
        throw new Error("Sweep Delay produced a non-finite local guitar sample.");
      }
      peak = Math.max(peak, Math.abs(sample));
      squares += sample * sample;
      const difference = sample - dry[index];
      differenceSquares += difference * difference;
      if (Math.abs(difference) > 1e-6) changedSamples += 1;
    }
    const rms = Math.sqrt(squares / Math.max(1, processed.length));
    const rmsDifferenceFromDry = Math.sqrt(
      differenceSquares / Math.max(1, processed.length),
    );
    if (peak < 1e-5 || rms < 1e-6 || rmsDifferenceFromDry < 1e-5 ||
        changedSamples < 32) {
      throw new Error("Sweep Delay did not produce changed local guitar playback: " +
        JSON.stringify({ peak, rms, rmsDifferenceFromDry, changedSamples }));
    }
    return {
      enabledEffectIndex: 42,
      filterType: "lowpass",
      rateMode: "tempo-sync",
      tempoBpm: 120,
      division: "quarter",
      peak,
      rms,
      rmsDifferenceFromDry,
      changedSamples,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      controls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateSweepDelay();
      DEFAULT_PARAMETERS.sweepDelay = previousParameters;
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({
          type: "enable", index, value: previousEffectState[index],
        });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runFractalThreeVoiceSynthBrowserSmokeTest(
  reference, captureView, captureBuffer,
) {
  const previousEffectState = enabledEffects();
  const previousControls = fractalThreeVoiceSynthControls.map((control) => control.value);
  try {
    await processorReady;
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error(
        "The tracked synth smoke test requires the ready browser chain and local guitar audio.");
    }
    await audioContext.resume();
    stopSource(false);
    const parameters = [...DEFAULT_PARAMETERS.fractalThreeVoiceSynth];
    parameters[0] = 0;
    parameters[1] = 2;
    parameters[10] = 0.5;
    parameters[11] = 7;
    parameters[22] = 7;
    parameters[33] = 1;
    fractalThreeVoiceSynthControls.forEach((control, index) => {
      control.value = String(parameters[index]);
    });
    updateFractalThreeVoiceSynth();
    const checkboxes = [...document.querySelectorAll("[data-effect]")];
    const capture = async (synthEnabled) => {
      checkboxes.forEach((checkbox, index) => {
        const enabled = index === 83 || index === 85 || (synthEnabled && index === 78);
        checkbox.checked = enabled;
        workletNode.port.postMessage({ type: "enable", index, value: enabled });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
      Atomics.store(captureView, 0, 0);
      Atomics.store(captureView, 1, 0);
      playbackOffset = 0;
      playButton.click();
      await Promise.race([
        playTask,
        new Promise((_, reject) => setTimeout(
          () => reject(new Error("Timed out starting tracked synth playback.")), 5000,
        )),
      ]);
      await waitForCapture(captureView, reference.samples.length);
      stopSource(false);
      // captureBuffer is a SharedArrayBuffer; a normal Float32Array wrapper
      // would alias it and the second capture would overwrite the first.
      return Float32Array.from(new Float32Array(captureBuffer));
    };
    const dry = await capture(false);
    const processed = await capture(true);
    let maxDifference = 0;
    let differenceSquares = 0;
    let changedSamples = 0;
    for (let index = 0; index < processed.length; index += 1) {
      const difference = Math.abs(processed[index] - dry[index]);
      maxDifference = Math.max(maxDifference, difference);
      differenceSquares += difference * difference;
      if (difference > 1e-6) changedSamples += 1;
    }
    const rmsDifference = Math.sqrt(differenceSquares / Math.max(1, processed.length));
    if (maxDifference < 1e-4 || rmsDifference < 1e-5 || changedSamples < 32) {
      throw new Error(
        `The tracked synth did not alter local guitar output: ${JSON.stringify({ maxDifference, rmsDifference, changedSamples })}`);
    }
    return {
      enabledEffectIndex: 78,
      waveform: "Sine",
      tracking: "Pitch + envelope",
      inputTrackingRange: "Bass",
      generatedVoices: 1,
      rmsDifference,
      maxDifference,
      changedSamples,
    };
  } finally {
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      fractalThreeVoiceSynthControls.forEach((control, index) => {
        control.value = previousControls[index];
      });
      updateFractalThreeVoiceSynth();
      const checkboxes = [...document.querySelectorAll("[data-effect]")];
      checkboxes.forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}

async function runMonoModernEnhancerBrowserSmokeTest(sourceAudio, restoreOptions) {
  const context = await getAudioContext();
  const frames = Math.min(Math.floor(context.sampleRate * 0.25), sourceAudio.length);
  const monoAudio = context.createBuffer(1, frames, context.sampleRate);
  const monoSamples = monoAudio.getChannelData(0);
  const left = sourceAudio.getChannelData(0);
  const right = sourceAudio.numberOfChannels > 1
    ? sourceAudio.getChannelData(1) : left;
  for (let frame = 0; frame < frames; frame += 1) {
    monoSamples[frame] = 0.5 * (left[frame] + right[frame]);
  }

  const captureBuffer = new SharedArrayBuffer(
    frames * 2 * Float32Array.BYTES_PER_ELEMENT,
  );
  const captureControl = new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 2);
  const captureView = new Int32Array(captureControl);
  const enabled = Array.from({ length: EFFECT_NAMES.length }, () => false);
  enabled[61] = true;
  const parameters = {
    ...DEFAULT_PARAMETERS,
    enhancer: [0, 1, 0.75, 120, 12000, 7, 0, 0, 1, 0, 1],
  };
  const monoOptions = { enabled, parameters, captureBuffer, captureControl };
  const previousAudio = decodedAudio;
  const previousOffset = playbackOffset;

  try {
    decodedAudio = monoAudio;
    playbackOffset = 0;
    await setupProcessor(monoOptions);
    await processorReady;
    if (preparedChannels !== 2) {
      throw new Error("The Modern Enhancer did not create a stereo browser chain for mono audio.");
    }
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    workletNode.port.postMessage({ type: "capture-reset" });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "active", value: true });
    const source = context.createBufferSource();
    source.buffer = monoAudio;
    source.connect(workletNode);
    activeSource = source;
    playbackStartedAt = context.currentTime;
    source.start(0);
    await waitForCapture(captureView, frames * 2);
    stopSource(false);

    const output = new Float32Array(captureBuffer);
    let stereoDifference = 0;
    let peak = 0;
    for (let frame = 0; frame < frames; frame += 1) {
      const outLeft = output[frame * 2];
      const outRight = output[frame * 2 + 1];
      if (!Number.isFinite(outLeft) || !Number.isFinite(outRight)) {
        throw new Error("Modern Enhancer produced non-finite stereo output from mono guitar audio.");
      }
      stereoDifference += Math.abs(outLeft - outRight);
      peak = Math.max(peak, Math.abs(outLeft), Math.abs(outRight));
    }
    if (peak < 1.0e-5 || stereoDifference / frames < 1.0e-5) {
      throw new Error("Modern Enhancer did not produce audible stereo output from mono guitar audio.");
    }
    return {
      inputChannels: 1,
      outputChannels: 2,
      frames,
      peak,
      meanLeftRightDifference: stereoDifference / frames,
    };
  } finally {
    stopSource(false);
    decodedAudio = previousAudio;
    playbackOffset = previousOffset;
    if (previousAudio) {
      await setupProcessor(restoreOptions);
      await processorReady;
      await ensureConvolutionIRLoaded();
    }
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
    const expectedEffectIds = Array.from({ length: 110 }, (_, index) => index);
    if (EFFECT_NAMES.length !== 110 || effectRows.length !== 110 ||
        effectIds.join(",") !== expectedEffectIds.join(",") || initialEffectState[28] ||
        EFFECT_NAMES[64] !== "Auto-Swell Delay · project-authored" ||
        EFFECT_NAMES[65] !== "Shuffling Looper · project-authored" ||
        EFFECT_NAMES[66] !== "Ten-Tap Delay · project-authored" ||
        EFFECT_NAMES[67] !== "Auto-Pan · project-authored" ||
         EFFECT_NAMES[68] !== "Megatap Delay · project-authored" ||
         EFFECT_NAMES[69] !== "De-Esser · project-authored" ||
         EFFECT_NAMES[70] !== "Dual-Engine Distortion · project-authored" ||
         EFFECT_NAMES[71] !== "Vibe · project-authored" ||
          EFFECT_NAMES[72] !== "Multi-Delay · project-authored" ||
            EFFECT_NAMES[73] !== "Porta Preamp · project-authored" ||
            EFFECT_NAMES[74] !== "Pitch Sequence · project-authored poly transposition" ||
            EFFECT_NAMES[75] !== "Rhythm Tap Delay · project-authored learned rhythm" ||
            EFFECT_NAMES[76] !== "Plex Matrix Delay · project-authored feedback matrix" ||
             EFFECT_NAMES[77] !== "Celestial Reverb · project-authored dual-pitch feedback" ||
              EFFECT_NAMES[78] !== "Three-Voice Tracked Synth · project-authored" ||
              fractalThreeVoiceSynthControls.length !== 34 ||
              fractalThreeVoiceSynthControls[33]?.id !== "fractal-three-voice-synth-input-tracking" ||
              [...(fractalThreeVoiceSynthControls[33]?.options || [])].map((option) => option.textContent).join(",") !== "Guitar,Bass" ||
             !EFFECT_NAMES[79]?.startsWith("Cloud Reverb") ||
          !EFFECT_NAMES[80]?.startsWith("Three-Stage Distortion") ||
          !EFFECT_NAMES[81]?.startsWith("Manual Through-Zero Flanger") ||
            !EFFECT_NAMES[82]?.startsWith("Octobot") ||
            !EFFECT_NAMES[83]?.startsWith("AM Ring Mod") ||
            !EFFECT_NAMES[84]?.startsWith("Tape Echo") ||
            !EFFECT_NAMES[85]?.startsWith("Octave Fuzz") ||
            !EFFECT_NAMES[86]?.startsWith("Stereo Crossover") ||
             !EFFECT_NAMES[87]?.startsWith("Asheville Pattrn") ||
             effectRows[87]?.dataset.category !== "filter" ||
             effectRows[87]?.querySelector(".effect-category-tag")?.textContent !== "Filters & wah" ||
             !EFFECT_NAMES[88]?.startsWith("Rock Box Dist") ||
             effectRows[88]?.dataset.category !== "drive" ||
             effectRows[88]?.querySelector(".effect-category-tag")?.textContent !== "Drive & fuzz" ||
              !EFFECT_NAMES[89]?.startsWith("Rock Box Boost") ||
              effectRows[89]?.dataset.category !== "drive" ||
              effectRows[89]?.querySelector(".effect-category-tag")?.textContent !== "Drive & fuzz" ||
              !EFFECT_NAMES[90]?.startsWith("Stereo Panner") ||
              effectRows[90]?.dataset.category !== "modulation" ||
              effectRows[90]?.querySelector(".effect-category-tag")?.textContent !== "Modulation" ||
              !EFFECT_NAMES[91]?.startsWith("Poly Sustain") ||
              effectRows[91]?.dataset.category !== "delay" ||
              effectRows[91]?.querySelector(".effect-category-tag")?.textContent !== "Delay" ||
              !EFFECT_NAMES[92]?.startsWith("Step Filter Delay") ||
              effectRows[92]?.dataset.category !== "delay" ||
              effectRows[92]?.querySelector(".effect-category-tag")?.textContent !== "Delay" ||
              !EFFECT_NAMES[93]?.startsWith("Volume/Pan") ||
              effectRows[93]?.dataset.category !== "utility" ||
              effectRows[93]?.querySelector(".effect-category-tag")?.textContent !== "Utility" ||
               EFFECT_NAMES[94] !== "Output level" || EFFECT_NAMES[95] !== "Peak limiter" ||
               !EFFECT_NAMES[96]?.startsWith("Stereo Filter") ||
               effectRows[96]?.dataset.category !== "filter" ||
               effectRows[96]?.querySelector(".effect-category-tag")?.textContent !== "Filters & wah" ||
                !EFFECT_NAMES[97]?.startsWith("Step Tremolo") ||
                effectRows[97]?.dataset.category !== "modulation" ||
                effectRows[97]?.querySelector(".effect-category-tag")?.textContent !== "Modulation" ||
                !EFFECT_NAMES[98]?.startsWith("Deluxe Looper") ||
                effectRows[98]?.dataset.category !== "loop" ||
                effectRows[98]?.querySelector(".effect-category-tag")?.textContent !== "Loop & performance" ||
              !EFFECT_NAMES[100]?.startsWith("Cabinet IR") ||
              effectRows[100]?.dataset.category !== "ir-loader" ||
              effectRows[100]?.querySelector(".effect-category-tag")?.textContent !== "IR Loader" ||
              !EFFECT_NAMES[101]?.startsWith("Octave Divider") ||
              effectRows[101]?.dataset.category !== "pitch" ||
              effectRows[101]?.querySelector(".effect-category-tag")?.textContent !== "Pitch" ||
              !EFFECT_NAMES[102]?.startsWith("Double Take") ||
              effectRows[102]?.dataset.category !== "modulation" ||
              effectRows[102]?.querySelector(".effect-category-tag")?.textContent !== "Modulation" ||
              !EFFECT_NAMES[103]?.startsWith("EQ5 Graphic") ||
              effectRows[103]?.dataset.category !== "eq" ||
              effectRows[103]?.querySelector(".effect-category-tag")?.textContent !== "Equalizer" ||
              !EFFECT_NAMES[104]?.startsWith("Optical Tremolo") ||
              effectRows[104]?.dataset.category !== "modulation" ||
              effectRows[104]?.querySelector(".effect-category-tag")?.textContent !== "Modulation" ||
              !EFFECT_NAMES[107]?.startsWith("Uni-Verse Vibe") ||
              effectRows[107]?.dataset.category !== "modulation" ||
              effectRows[107]?.querySelector(".effect-category-tag")?.textContent !== "Modulation" ||
               !EFFECT_NAMES[108]?.startsWith("4 OSC Generator") ||
               effectRows[108]?.dataset.category !== "synth" ||
               effectRows[108]?.querySelector(".effect-category-tag")?.textContent !== "Synth" ||
               !EFFECT_NAMES[109]?.startsWith("Poly Detune") ||
               effectRows[109]?.dataset.category !== "modulation" ||
               effectRows[109]?.querySelector(".effect-category-tag")?.textContent !== "Modulation" ||
        initialEffectState[64] || initialEffectState[65] || initialEffectState[66] ||
         initialEffectState[67] || initialEffectState[68] ||
         initialEffectState[69] || initialEffectState[70] ||
          initialEffectState[71] || initialEffectState[72] ||
            initialEffectState[73] || initialEffectState[74] ||
            initialEffectState[75] || initialEffectState[76] ||
              initialEffectState[77] || initialEffectState[78] ||
               initialEffectState[79] || initialEffectState[80] || initialEffectState[81] ||
                initialEffectState[82] || initialEffectState[83] ||
                 initialEffectState[84] || initialEffectState[85] ||
                  initialEffectState[86] || initialEffectState[87] ||
                   initialEffectState[88] || initialEffectState[89] ||
                   initialEffectState[90] || initialEffectState[91] ||
                    initialEffectState[92] || initialEffectState[93] ||
                      !initialEffectState[94] || !initialEffectState[95] ||
                        initialEffectState[96] || initialEffectState[97] || initialEffectState[98] ||
        initialEffectState[100] || initialEffectState[101] || initialEffectState[102] ||
        initialEffectState[103] || initialEffectState[104] ||
        initialEffectState[105] || initialEffectState[106] || initialEffectState[107] ||
         initialEffectState[108] || initialEffectState[109] ||
        initialEffectState[29] || initialEffectState[30] || initialEffectState[31] ||
        initialEffectState[32] || initialEffectState[33] || initialEffectState[34] ||
        initialEffectState[35] || initialEffectState[36] || initialEffectState[37] ||
        initialEffectState[38] || initialEffectState[39] || initialEffectState[40] ||
        initialEffectState[41] || initialEffectState[42] || initialEffectState[43] ||
        initialEffectState[44] || initialEffectState[45] || initialEffectState[46] ||
        initialEffectState[47] || initialEffectState[48] || initialEffectState[49] ||
         initialEffectState[50] || initialEffectState[51] || initialEffectState[52] ||
         initialEffectState[53] || initialEffectState[54] || initialEffectState[55] ||
           initialEffectState[56] || initialEffectState[57] ||
            initialEffectState[58] || initialEffectState[59] ||
            initialEffectState[60] || initialEffectState[61] ||
              initialEffectState[62] || initialEffectState[63] || initialEffectState[64] ||
              initialEffectState[65] ||
         effectRows.some((row) => !knownCategories.has(row.dataset.category) ||
          !row.querySelector(".effect-category-tag"))) {
      throw new Error("The effect picker must list 109 ordered, categorized effects (IDs 0–108), including Uni-Verse Vibe in Modulation and 4 OSC Generator in Synth; existing effect IDs and optional-disabled defaults must stay stable.");
     }
      const rockBoxDistRanges = [
        [rockBoxDistControls[0], "0", "1"],
        [rockBoxDistControls[1], "0", "1"],
        [rockBoxDistControls[2], "-24", "12"],
        [rockBoxDistControls[3], "0", "1000"],
      ];
      if (DEFAULT_PARAMETERS.rockBoxDistortion.join(",") !== "0.5,0.5,0,10" ||
          rockBoxDistRanges.some(([control, minimum, maximum]) =>
            control.min !== minimum || control.max !== maximum)) {
        throw new Error("Rock Box Dist must expose the project-authored defaults and bounded Drive, Tone, Level, and smoothing controls.");
      }
      const rockBoxBoostRanges = [
        [rockBoxBoostControls[0], "0", "24"],
        [rockBoxBoostControls[1], "-24", "12"],
        [rockBoxBoostControls[2], "0", "1000"],
      ];
      if (DEFAULT_PARAMETERS.rockBoxBoost.join(",") !== "0,0,10" ||
          rockBoxBoostRanges.some(([control, minimum, maximum]) =>
            control.min !== minimum || control.max !== maximum)) {
        throw new Error("Rock Box Boost must expose project-authored defaults and bounded Boost, Level, and smoothing controls.");
      }
      const ashevilleRanges = [
        [ashevillePattrnDrive, "0", "18"],
        [ashevillePattrnOutput, "-24", "12"],
        [ashevillePattrnMix, "0", "100"],
        [ashevillePattrnRate, "0.05", "10"],
        [ashevillePattrnQ, "0.5", "12"],
        [ashevillePattrnEnvelope, "0", "1"],
        [ashevillePattrnSmoothing, "0", "1000"],
        ...ashevillePattrnBandLevels.map(({ input }) => [input, "0", "1"]),
      ];
      if (DEFAULT_PARAMETERS.ashevillePattrn.join(",") !==
            "0,0,1,1,1,1,1,1,1,1,0,0,0.5,0.75,3,0.5,10" ||
          ashevilleRanges.some(([control, minimum, maximum]) =>
            control.min !== minimum || control.max !== maximum) ||
          ashevillePattrnBank.querySelectorAll("option").length !== 2 ||
          ashevillePattrnPattern.querySelectorAll("option").length !== 8 ||
          ashevillePattrnBandLevels.length !== 8) {
        throw new Error("Asheville Pattrn must expose its project-authored defaults, eight band levels, bank/pattern selectors, and bounded controls.");
      }
      const ashevilleControlSnapshot = [
        ashevillePattrnBank.value, ashevillePattrnPattern.value,
        ashevillePattrnDrive.value, ashevillePattrnOutput.value,
        ashevillePattrnMix.value, ashevillePattrnRate.value,
        ashevillePattrnQ.value, ashevillePattrnEnvelope.value,
        ashevillePattrnSmoothing.value,
        ...ashevillePattrnBandLevels.map(({ input }) => input.value),
      ];
      const ashevilleDefaultsSnapshot = [...DEFAULT_PARAMETERS.ashevillePattrn];
      const previousAshevilleWorklet = workletNode;
      const ashevilleMessages = [];
      try {
        workletNode = { port: { postMessage: (message) => ashevilleMessages.push(message) } };
        [ashevillePattrnBank.value, ashevillePattrnPattern.value] = ["1", "2"];
        [ashevillePattrnDrive.value, ashevillePattrnOutput.value,
          ashevillePattrnMix.value, ashevillePattrnRate.value,
          ashevillePattrnQ.value, ashevillePattrnEnvelope.value,
          ashevillePattrnSmoothing.value] =
          ["6", "-3", "65", "2.5", "4", "0.8", "25"];
        ashevillePattrnBandLevels.forEach(({ input }, index) => {
          input.value = index === 0 ? "0.25" : index === 7 ? "0.75" : "1";
        });
        updateAshevillePattrn();
        const message = ashevilleMessages.at(-1);
        if (message?.type !== "asheville-pattrn" ||
            message.value.join(",") !==
              "1,2,0.25,1,1,1,1,1,1,0.75,6,-3,0.65,2.5,4,0.8,25") {
          throw new Error("Asheville Pattrn UI controls must send bank, pattern, bands, and project parameters in WASM API order.");
        }
      } finally {
        workletNode = null;
        [ashevillePattrnBank.value, ashevillePattrnPattern.value,
          ashevillePattrnDrive.value, ashevillePattrnOutput.value,
          ashevillePattrnMix.value, ashevillePattrnRate.value,
          ashevillePattrnQ.value, ashevillePattrnEnvelope.value,
          ashevillePattrnSmoothing.value] = ashevilleControlSnapshot.slice(0, 9);
        ashevillePattrnBandLevels.forEach(({ input }, index) => {
          input.value = ashevilleControlSnapshot[index + 9];
        });
        updateAshevillePattrn();
        DEFAULT_PARAMETERS.ashevillePattrn = ashevilleDefaultsSnapshot;
        workletNode = previousAshevilleWorklet;
      }
     const convolutionDefaults = [...DEFAULT_PARAMETERS.convolutionReverb];
     const convolutionRanges = [
       [convolutionReverbControls[0], "0", "100"],
       [convolutionReverbControls[1], "-24", "12"],
       [convolutionReverbControls[2], "0", "10000"],
       [convolutionReverbControls[3], "0", "20000"],
     ];
     const convolutionHint = document.querySelector(
       '.effect-parameter-group[aria-label="Convolution reverb controls"] .hint',
     ).textContent;
     if (convolutionDefaults.join(",") !== "0.25,0,0,0,0" ||
         convolutionRanges.some(([control, minimum, maximum]) =>
           control.min !== minimum || control.max !== maximum) ||
          !convolutionHint.includes("HeadRush DSP") ||
          !convolutionHint.includes("10 s at 48 kHz") ||
         !convolutionHint.includes("Cross-rig spillover") ||
         convolutionReverbTails.checked) {
       throw new Error("Convolution Reverb must expose its project filter and wet controls, per-block tails toggle, and Prime capability limits.");
     }
     const convolutionControlSnapshot = convolutionReverbControls.map((control) => control.value);
     const convolutionTailsSnapshot = convolutionReverbTails.checked;
     const previousConvolutionWorklet = workletNode;
     const convolutionMessages = [];
     try {
       workletNode = { port: { postMessage: (message) => convolutionMessages.push(message) } };
       [convolutionReverbControls[0].value, convolutionReverbControls[1].value,
         convolutionReverbControls[2].value, convolutionReverbControls[3].value] =
         ["45", "-2", "96", "8650"];
       convolutionReverbTails.checked = true;
       convolutionReverbControls[2].dispatchEvent(new Event("input", { bubbles: true }));
       const convolutionMessage = convolutionMessages.at(-1);
       if (convolutionMessage?.type !== "convolution-reverb" ||
           convolutionMessage.value.join(",") !== "0.45,-2,96,8650,1" ||
           convolutionReverbControls[2].parentElement.querySelector("output").value !== "96 Hz" ||
           convolutionReverbControls[3].parentElement.querySelector("output").value !== "8650 Hz" ||
           convolutionReverbTails.parentElement.querySelector("output").value !== "On") {
         throw new Error("Convolution Reverb controls must send filters, wet gain, mix, and tails in WASM API order.");
       }
     } finally {
       workletNode = previousConvolutionWorklet;
       convolutionReverbControls.forEach((control, index) => {
         control.value = convolutionControlSnapshot[index];
       });
       convolutionReverbTails.checked = convolutionTailsSnapshot;
       updateConvolutionReverb();
     }
     if (DEFAULT_PARAMETERS.convolutionReverb.join(",") !== "0.25,0,0,0,0") {
       throw new Error("Convolution Reverb self-test must restore its project-authored defaults.");
     }
     const defaultFiveBandEq = [...DEFAULT_PARAMETERS.fiveBandEq];
     fiveBandEqControls[2].gain.value = "3.5";
     fiveBandEqControls[2].q.value = "2.25";
     fiveBandEqControls[2].q.dispatchEvent(new Event("input", { bubbles: true }));
     fiveBandEqControls[0].shape.value = "1";
     fiveBandEqControls[0].shape.dispatchEvent(new Event("input", { bubbles: true }));
     fiveBandEqControls[4].shape.value = "4";
     fiveBandEqControls[4].slope.value = "24";
     fiveBandEqControls[4].slope.dispatchEvent(new Event("input", { bubbles: true }));
     if (DEFAULT_PARAMETERS.fiveBandEq[11] !== 3.5 ||
         DEFAULT_PARAMETERS.fiveBandEq[12] !== 2.25 ||
         DEFAULT_PARAMETERS.fiveBandEq[3] !== 1 ||
         DEFAULT_PARAMETERS.fiveBandEq[23] !== 4 ||
         DEFAULT_PARAMETERS.fiveBandEq[24] !== 24 ||
         !fiveBandEqControls[4].gain.disabled ||
         !fiveBandEqControls[4].q.disabled ||
         fiveBandEqControls[4].slope.disabled ||
         fiveBandEqControls[2].gainValue.value !== "3.5 dB" ||
         fiveBandEqControls[2].qValue.value !== "2.25") {
       throw new Error("Five-band EQ shape, slope, and band controls must update the project parameters.");
     }
     for (const [index, band] of fiveBandEqControls.entries()) {
       const values = defaultFiveBandEq.slice(index * 5, index * 5 + 5);
       [band.frequency, band.gain, band.q, band.shape, band.slope].filter(Boolean)
         .forEach((control, controlIndex) => { control.value = String(values[controlIndex]); });
     }
     updateFiveBandEQ();
     const defaultBitCrusher = [...DEFAULT_PARAMETERS.bitCrusher];
    bitCrusherBitDepth.value = "8";
    bitCrusherRateDivisor.value = "4";
    bitCrusherMix.value = "65";
    bitCrusherMix.dispatchEvent(new Event("input", { bubbles: true }));
    const defaultDimensionChorus = [...DEFAULT_PARAMETERS.dimensionChorus];
    dimensionChorusVersion.value = "1";
    dimensionChorusMode.value = "9";
    dimensionChorusMix.value = "65";
    dimensionChorusLevel.value = "-3";
    dimensionChorusMode.dispatchEvent(new Event("input", { bubbles: true }));
    const defaultTripleDoubleChorus = [...DEFAULT_PARAMETERS.tripleDoubleChorus];
    tripleDoubleChorusPresetSwitch.value = "0";
    tripleDoubleChorusManualSwitch.value = "1";
    tripleDoubleChorusRate.value = "1.25";
    tripleDoubleChorusLeftDepth.value = "10";
    tripleDoubleChorusCenterDepth.value = "6";
    tripleDoubleChorusRightDepth.value = "14";
    tripleDoubleChorusFeedback.value = "-0.2";
    tripleDoubleChorusSparkle.value = "0.65";
    tripleDoubleChorusStereoSwitch.value = "0";
    tripleDoubleChorusManualSwitch.dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.tripleDoubleChorus.join(",") !==
          "0,1,1.25,10,6,14,-0.2,0.65,0" ||
        tripleDoubleChorusRateValue.value !== "1.25 Hz" ||
        tripleDoubleChorusSparkleValue.value !== "65%") {
      throw new Error("Triple-Double Chorus controls must update project parameters and displayed values.");
    }
    [tripleDoubleChorusPresetSwitch.value,
      tripleDoubleChorusManualSwitch.value, tripleDoubleChorusRate.value,
      tripleDoubleChorusLeftDepth.value, tripleDoubleChorusCenterDepth.value,
      tripleDoubleChorusRightDepth.value, tripleDoubleChorusFeedback.value,
      tripleDoubleChorusSparkle.value, tripleDoubleChorusStereoSwitch.value] =
      defaultTripleDoubleChorus.map(String);
    tripleDoubleChorusManualSwitch.dispatchEvent(new Event("input", { bubbles: true }));
    const defaultFourBandResonator = [...DEFAULT_PARAMETERS.fourBandResonator];
    const testFourBandResonator = [1, 2, 1.1, 220, 330, 440, 660, 220,
      0.65, 0.55, 0.45, 0.35, 2, 2.5, 3, 3.5, 0.8, 0.7, 0.6, 0.5,
      -0.75, -0.25, 0.25, 0.75, 0, 1, 0, 1, 0, 0.9, -0.25, 0.75, 1.2, 0.65];
    const fourBandControls = [...fourBandResonatorControls.querySelectorAll("[data-resonator-index]")];
    for (const control of fourBandControls) {
      control.value = String(testFourBandResonator[Number(control.dataset.resonatorIndex)]);
    }
    fourBandControls[0].dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.fourBandResonator.join(",") !== testFourBandResonator.join(",") ||
        fourBandResonatorControls.querySelector("#four-band-resonator-band-2-filter-location").selectedOptions[0].textContent !== "Post resonator") {
      throw new Error("Four-band resonator controls must update every parameter and display the selected filter location.");
    }
    for (const control of fourBandControls) {
      control.value = String(defaultFourBandResonator[Number(control.dataset.resonatorIndex)]);
    }
    fourBandControls[0].dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.dimensionChorus.join(",") !== "1,9,0.65,-3" ||
        dimensionChorusMixValue.value !== "65%" ||
        dimensionChorusLevelValue.value !== "-3.0 dB") {
      throw new Error("Dimension Chorus selectors and controls must update the processor settings.");
    }
    [dimensionChorusVersion.value, dimensionChorusMode.value,
      dimensionChorusMix.value, dimensionChorusLevel.value] = [
      String(defaultDimensionChorus[0]), String(defaultDimensionChorus[1]),
      String(Math.round(defaultDimensionChorus[2] * 100)),
      String(defaultDimensionChorus[3]),
    ];
    dimensionChorusMode.dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.bitCrusher.join(",") !== "8,4,0.65" ||
        bitCrusherBitDepthValue.value !== "8 bits" ||
        bitCrusherRateDivisorValue.value !== "÷4" ||
        bitCrusherMixValue.value !== "65%") {
      throw new Error("Bit Crusher controls must update processor parameters and display values.");
    }
    [bitCrusherBitDepth.value, bitCrusherRateDivisor.value, bitCrusherMix.value] =
      [String(defaultBitCrusher[0]), String(defaultBitCrusher[1]),
        String(Math.round(defaultBitCrusher[2] * 100))];
    bitCrusherMix.dispatchEvent(new Event("input", { bubbles: true }));
    const defaultHarmonicTremolo = [...DEFAULT_PARAMETERS.harmonicTremolo];
    harmonicTremoloRate.value = "4.25";
    harmonicTremoloDepth.value = "0.8";
    harmonicTremoloCrossover.value = "950";
    harmonicTremoloWaveform.value = "triangle";
    harmonicTremoloSync.value = "on";
    harmonicTremoloBpm.value = "96";
    harmonicTremoloDivision.value = "0.5";
    harmonicTremoloStereo.value = "off";
    harmonicTremoloSmoothing.value = "25";
    harmonicTremoloCrossover.dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.harmonicTremolo.join(",") !==
          "4.25,0.8,950,1,1,96,0.5,0,25" ||
        harmonicTremoloCrossover.parentElement.querySelector("output").value !== "950 Hz" ||
        harmonicTremoloSmoothing.parentElement.querySelector("output").value !== "25.0 ms") {
      throw new Error("Harmonic Tremolo controls must update the project-owned processor settings and readouts.");
    }
    harmonicTremoloRate.value = String(defaultHarmonicTremolo[0]);
    harmonicTremoloDepth.value = String(defaultHarmonicTremolo[1]);
    harmonicTremoloCrossover.value = String(defaultHarmonicTremolo[2]);
    harmonicTremoloWaveform.value = ["sine", "triangle", "square"][defaultHarmonicTremolo[3]];
    harmonicTremoloSync.value = defaultHarmonicTremolo[4] === 0 ? "off" : "on";
    harmonicTremoloBpm.value = String(defaultHarmonicTremolo[5]);
    harmonicTremoloDivision.value = String(defaultHarmonicTremolo[6]);
    harmonicTremoloStereo.value = defaultHarmonicTremolo[7] === 0 ? "off" : "on";
    harmonicTremoloSmoothing.value = String(defaultHarmonicTremolo[8]);
    harmonicTremoloCrossover.dispatchEvent(new Event("input", { bubbles: true }));
     const autoSwellDelayDefaults = [...DEFAULT_PARAMETERS.autoSwellDelay];
     const autoSwellDelayRanges = [
       [autoSwellDelayTime, "1", "2000"],
       [autoSwellDelayFeedback, "0", "95"],
       [autoSwellDelayMix, "0", "100"],
       [autoSwellDelayTempo, "20", "300"],
       [autoSwellDelaySwell, "1", "5000"],
       [autoSwellDelayMod, "0", "100"],
       [autoSwellDelayTone, "0", "100"],
       [autoSwellDelaySense, "-72", "0"],
     ];
     const tapDivisionValues = [...autoSwellDelayTapDivision.options]
       .map((option) => option.value).join(",");
     const taperValues = [...autoSwellDelayTaper.options]
       .map((option) => option.value).join(",");
     const autoSwellHint = document.querySelector(
       '.effect-parameter-group[aria-label="Auto-Swell Delay controls"] .hint',
     ).textContent;
     if (autoSwellDelayDefaults.join(",") !==
           "350,0.25,0.25,0,4,120,300,0.15,0.7,-36,1,0,0" ||
         autoSwellDelayRanges.some(([control, minimum, maximum]) =>
           control.min !== minimum || control.max !== maximum) ||
         [...autoSwellDelayTimeMode.options].map((option) => option.value).join(",") !== "0,1" ||
         tapDivisionValues !== "0,1,2,3,4,5,6,7,8,9,10,11" ||
         taperValues !== "0,1,2" ||
         !autoSwellHint.includes("Model Guide v1.1.100") ||
         !autoSwellHint.includes("numeric ranges") ||
         !autoSwellHint.includes("parity are not claimed")) {
       throw new Error("Auto-Swell Delay defaults, project control ranges, enum order, and vendor qualification must remain explicit.");
     }
     const autoSwellControls = [autoSwellDelayTime, autoSwellDelayFeedback,
       autoSwellDelayMix, autoSwellDelayTimeMode, autoSwellDelayTapDivision,
       autoSwellDelayTempo, autoSwellDelaySwell, autoSwellDelayMod,
       autoSwellDelayTone, autoSwellDelaySense, autoSwellDelayTaper];
     const autoSwellControlSnapshot = autoSwellControls.map((control) => control.value);
     const autoSwellSwitchSnapshot = [autoSwellDelayDry.checked,
       autoSwellDelaySpillover.checked];
     const previousWorkletNode = workletNode;
     const autoSwellMessages = [];
     try {
       workletNode = { port: { postMessage: (message) => autoSwellMessages.push(message) } };
       autoSwellDelayTime.value = "650";
       autoSwellDelayFeedback.value = "70";
       autoSwellDelayMix.value = "40";
       autoSwellDelayTimeMode.value = "1";
       autoSwellDelayTapDivision.value = "8";
       autoSwellDelayTempo.value = "96";
       autoSwellDelaySwell.value = "900";
       autoSwellDelayMod.value = "20";
       autoSwellDelayTone.value = "35";
       autoSwellDelaySense.value = "-42";
       autoSwellDelayTaper.value = "2";
       autoSwellDelayDry.checked = true;
       autoSwellDelaySpillover.checked = true;
       autoSwellDelayTime.dispatchEvent(new Event("input", { bubbles: true }));
       const autoSwellMessage = autoSwellMessages.at(-1);
       if (autoSwellMessage?.type !== "auto-swell-delay" ||
           autoSwellMessage.value.join(",") !== "650,0.7,0.4,1,8,96,900,0.2,0.35,-42,2,1,1" ||
           autoSwellDelayTime.parentElement.querySelector("output").value !== "650 ms" ||
           autoSwellDelayFeedback.parentElement.querySelector("output").value !== "70%" ||
           autoSwellDelayMix.parentElement.querySelector("output").value !== "40%" ||
           autoSwellDelaySense.parentElement.querySelector("output").value !== "−42 dB") {
         throw new Error("Auto-Swell Delay controls must send the 13-value worklet message in API order and update readouts.");
       }
     } finally {
       workletNode = previousWorkletNode;
       autoSwellControls.forEach((control, index) => {
         control.value = autoSwellControlSnapshot[index];
       });
       autoSwellDelayDry.checked = autoSwellSwitchSnapshot[0];
       autoSwellDelaySpillover.checked = autoSwellSwitchSnapshot[1];
       updateAutoSwellDelay();
     }
     if (DEFAULT_PARAMETERS.autoSwellDelay.join(",") !==
         "350,0.25,0.25,0,4,120,300,0.15,0.7,-36,1,0,0") {
       throw new Error("Auto-Swell Delay self-test must restore its initial project-authored controls.");
     }
      const shufflingLooperDefaults = [...DEFAULT_PARAMETERS.shufflingLooper];
      const shufflingLooperRanges = [
        [shufflingLooperSlices, "2", "16"],
        [shufflingLooperSequenceLength, "1", "32"],
        [shufflingLooperShuffle, "0", "100"],
        [shufflingLooperPitchProbability, "0", "100"],
        [shufflingLooperReverseProbability, "0", "100"],
        [shufflingLooperRepeatProbability, "0", "100"],
        [shufflingLooperSmoothing, "0", "100"],
        [shufflingLooperSequenceDrift, "0", "100"],
        [shufflingLooperPlayback, "0", "100"],
        [shufflingLooperLowCut, "0", "20000"],
        [shufflingLooperHighCut, "0", "20000"],
        [shufflingLooperInterval1, "-12", "12"],
        [shufflingLooperInterval2, "-12", "12"],
      ];
      const shufflingLooperHint = document.querySelector(
        '.effect-parameter-group[aria-label="Shuffling Looper controls"] .hint',
      ).textContent;
      const shufflingLooperButtons = [...document.querySelectorAll(
        "[data-shuffling-looper-command]",
      )];
      if (shufflingLooperDefaults.join(",") !== "8,8,0.5,0,0,0,0,0,0.7,0,0,-12,12" ||
          shufflingLooperRanges.some(([control, minimum, maximum]) =>
            control.min !== minimum || control.max !== maximum) ||
          !shufflingLooperHint.includes("manual selector identifies firmware v1.4.1 / Rev. E") ||
          !shufflingLooperHint.includes("looper page still displays a Rev D header") ||
          !shufflingLooperHint.includes("Numeric bounds/defaults") ||
          !shufflingLooperHint.includes("does not reproduce the manual’s single-switch") ||
          !shufflingLooperHint.includes("no Line 6 code, control, or sound parity") ||
          shufflingLooperButtons.map((button) => button.dataset.shufflingLooperCommand).join(",") !==
            "0,1,2,3,4,5") {
        throw new Error("Shuffling Looper project ranges, documented-source caveat, and transport commands must be explicit.");
      }
      const shufflingLooperControlSnapshot = shufflingLooperControls.map((control) => control.value);
      const previousShufflingWorklet = workletNode;
      const shufflingLooperMessages = [];
      try {
        workletNode = { port: { postMessage: (message) => shufflingLooperMessages.push(message) } };
        [shufflingLooperSlices.value, shufflingLooperSequenceLength.value,
          shufflingLooperShuffle.value, shufflingLooperPitchProbability.value,
          shufflingLooperReverseProbability.value, shufflingLooperRepeatProbability.value,
          shufflingLooperSmoothing.value, shufflingLooperSequenceDrift.value,
          shufflingLooperPlayback.value, shufflingLooperLowCut.value,
          shufflingLooperHighCut.value, shufflingLooperInterval1.value,
          shufflingLooperInterval2.value] = [
          "12", "17", "40", "20", "30", "40", "50", "60", "75", "5", "12000", "-7", "5",
        ];
        shufflingLooperInterval2.dispatchEvent(new Event("input", { bubbles: true }));
        const optionsMessage = shufflingLooperMessages.at(-1);
        if (optionsMessage?.type !== "shuffling-looper-options" ||
            optionsMessage.value.join(",") !== "12,17,0.4,0.2,0.3,0.4,0.5,0.6,0.75,20,12000,-7,5" ||
            shufflingLooperLowCut.value !== "20" ||
            shufflingLooperLowCut.parentElement.querySelector("output").value !== "20 Hz" ||
            shufflingLooperInterval2.parentElement.querySelector("output").value !== "+5 semitones") {
          throw new Error("Shuffling Looper controls must send all 13 bounded options in WASM API order.");
        }
        for (const button of shufflingLooperButtons) button.click();
        const commandIds = shufflingLooperMessages
          .filter((message) => message.type === "shuffling-looper-command")
          .map((message) => message.command).join(",");
        if (commandIds !== "0,1,2,3,4,5") {
          throw new Error("Shuffling Looper transport buttons must send the documented command IDs.");
        }
      } finally {
        workletNode = previousShufflingWorklet;
        shufflingLooperControls.forEach((control, index) => {
          control.value = shufflingLooperControlSnapshot[index];
        });
        updateShufflingLooperOptions();
      }
      if (DEFAULT_PARAMETERS.shufflingLooper.join(",") !==
          "8,8,0.5,0,0,0,0,0,0.7,0,0,-12,12") {
        throw new Error("Shuffling Looper self-test must restore its project-authored defaults.");
      }
    const tenTapDefaults = [...DEFAULT_PARAMETERS.tenTapDelay];
    const tenTapHint = document.querySelector(
      '.effect-parameter-group[aria-label="Ten-Tap Delay controls"] .hint',
    ).textContent;
    const tenTapRanges = [
      [tenTapDelayTapCount, "1", "10"], [tenTapDelayTime, "1", "2000"],
      [tenTapDelayTempo, "20", "300"], [tenTapDelayDecay, "0", "100"],
      [tenTapDelayShuffle, "0", "100"], [tenTapDelaySpread, "0", "100"],
      [tenTapDelayRatio, "50", "200"], [tenTapDelayPanAlpha, "0", "100"],
      [tenTapDelayLowCut, "0", "5000"], [tenTapDelayHighCut, "1000", "20000"],
      [tenTapDelayMix, "0", "100"], [tenTapDelayLevel, "-24", "12"],
      ...tenTapDelayTapLevels.map((control) => [control, "-80", "20"]),
    ];
    if (tenTapDefaults.join(",") !==
          "4,220,120,0,30,0,50,100,1,100,0,20000,0.25,0,0,0,0,0,0,0,0,0,0,0,0" ||
        tenTapRanges.some(([control, minimum, maximum]) =>
          control.min !== minimum || control.max !== maximum) ||
        !tenTapHint.includes("November 2026") ||
        !tenTapHint.includes("Rhythm Tap") ||
        !tenTapHint.includes("not Fractal sound/control parity")) {
      throw new Error("Ten-Tap Delay controls must expose the documented type controls and explicit project limits.");
    }
    const tenTapControlSnapshot = tenTapDelayControls.map((control) =>
      control.type === "checkbox" ? control.checked : control.value);
    const previousTenTapWorklet = workletNode;
    const tenTapMessages = [];
    const tenTapTestLevels = ["1", "-2", "-3", "-4", "-5", "-6", "-7", "-8", "-9", "-10"];
    try {
      workletNode = { port: { postMessage: (message) => tenTapMessages.push(message) } };
      [tenTapDelayTapCount.value, tenTapDelayTime.value, tenTapDelayTempo.value,
        tenTapDelayDivision.value, tenTapDelayDecay.value, tenTapDelayShuffle.value,
        tenTapDelaySpread.value, tenTapDelayRatio.value, tenTapDelayPanShape.value,
        tenTapDelayPanAlpha.value, tenTapDelayLowCut.value, tenTapDelayHighCut.value,
        tenTapDelayMix.value, tenTapDelayLevel.value, tenTapDelayMono.checked] = [
        "7", "250", "100", "11", "40", "30", "80", "120", "2", "75",
        "100", "8000", "40", "-3", true,
      ];
      tenTapDelayTapLevels.forEach((control, index) => {
        control.value = tenTapTestLevels[index];
      });
      tenTapDelayTapLevels[9].dispatchEvent(new Event("input", { bubbles: true }));
      const tenTapMessage = tenTapMessages.at(-1);
      if (tenTapMessage?.type !== "ten-tap-delay-options" ||
          tenTapMessage.value.join(",") !==
            "7,250,100,11,40,30,80,120,2,75,100,8000,0.4,-3,1,1,-2,-3,-4,-5,-6,-7,-8,-9,-10" ||
          tenTapDelayRatio.parentElement.querySelector("output").value !== "120% left/right" ||
          tenTapDelayTapLevels[9].parentElement.querySelector("output").value !== "-10 dB") {
        throw new Error("Ten-Tap Delay controls must send all global and per-tap settings in WASM order.");
      }
    } finally {
      workletNode = previousTenTapWorklet;
      tenTapDelayControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = tenTapControlSnapshot[index];
        else control.value = tenTapControlSnapshot[index];
      });
      updateTenTapDelayOptions();
    }
    if (DEFAULT_PARAMETERS.tenTapDelay.join(",") !== tenTapDefaults.join(",")) {
      throw new Error("Ten-Tap Delay self-test must restore its project-authored defaults.");
    }
    const autoPanDefaults = [...DEFAULT_PARAMETERS.autoPan];
    const autoPanControlSnapshot = autoPanControls.map((control) =>
      control.type === "checkbox" ? control.checked : control.value);
    const previousAutoPanWorklet = workletNode;
    const autoPanMessages = [];
    try {
      workletNode = { port: { postMessage: (message) => autoPanMessages.push(message) } };
      [autoPanRate.value, autoPanDepth.value, autoPanWaveform.value,
        autoPanTempoSync.checked, autoPanTempo.value, autoPanDivision.value] =
        ["2.5", "50", "2", true, "90", "7"];
      autoPanDivision.dispatchEvent(new Event("change", { bubbles: true }));
      const autoPanMessage = autoPanMessages.at(-1);
      if (autoPanMessage?.type !== "auto-pan" ||
          autoPanMessage.value.join(",") !== "2.5,0.5,2,1,90,7" ||
          autoPanRate.parentElement.querySelector("output").value !== "2.50 Hz" ||
          autoPanDepth.parentElement.querySelector("output").value !== "50%" ||
          autoPanTempo.disabled || autoPanDivision.disabled) {
        throw new Error("Auto-Pan controls must send rate, depth, type, sync, tempo, and division in WASM order.");
      }
    } finally {
      workletNode = previousAutoPanWorklet;
      autoPanControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = autoPanControlSnapshot[index];
        else control.value = autoPanControlSnapshot[index];
      });
      updateAutoPanOptions();
    }
    if (DEFAULT_PARAMETERS.autoPan.join(",") !== autoPanDefaults.join(",")) {
      throw new Error("Auto-Pan self-test must restore its project-authored defaults.");
    }
    const stereoPannerDefaults = [...DEFAULT_PARAMETERS.stereoPanner];
    const stereoPannerControlSnapshot = stereoPannerControls.map((control) =>
      control.type === "checkbox" ? control.checked : control.value);
    const previousStereoPannerWorklet = workletNode;
    const stereoPannerMessages = [];
    try {
      workletNode = { port: { postMessage: (message) => stereoPannerMessages.push(message) } };
      [stereoPannerControls[0].value, stereoPannerControls[1].value,
        stereoPannerControls[2].value, stereoPannerControls[3].checked,
        stereoPannerControls[4].value, stereoPannerControls[5].value,
        stereoPannerControls[6].value, stereoPannerControls[7].value,
        stereoPannerControls[8].value, stereoPannerControls[9].value,
        stereoPannerControls[10].value, stereoPannerControls[11].value,
        stereoPannerControls[12].value, stereoPannerControls[13].value] =
        ["2.5", "150", "9", true, "90", "7", "25", "75", "270", "-20",
          "60", "-3", "40", "0"];
      stereoPannerControls[13].dispatchEvent(new Event("change", { bubbles: true }));
      const message = stereoPannerMessages.at(-1);
      if (message?.type !== "stereo-panner" ||
          message.value.join(",") !== "2.5,150,9,1,90,7,25,75,270,-20,60,-3,40,0" ||
          stereoPannerControls[4].disabled || stereoPannerControls[5].disabled ||
          stereoPannerControls[1].parentElement.querySelector("output").value !== "150%" ||
          stereoPannerControls[8].parentElement.querySelector("output").value !== "270°") {
        throw new Error("Stereo Panner controls must serialize all documented controls in WASM order.");
      }
    } finally {
      workletNode = previousStereoPannerWorklet;
      stereoPannerControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = stereoPannerControlSnapshot[index];
        else control.value = stereoPannerControlSnapshot[index];
      });
      updateStereoPannerOptions();
    }
    if (DEFAULT_PARAMETERS.stereoPanner.join(",") !== stereoPannerDefaults.join(",")) {
      throw new Error("Stereo Panner self-test must restore its project-authored defaults.");
    }
    const megatapDelayDefaults = [...DEFAULT_PARAMETERS.megatapDelay];
    const megatapDelayControlSnapshot = megatapDelayControls.map((control) =>
      control.type === "checkbox" ? control.checked : control.value);
    const previousMegatapDelayWorklet = workletNode;
    const megatapDelayMessages = [];
    try {
      workletNode = { port: { postMessage: (message) => megatapDelayMessages.push(message) } };
      [megatapDelayTime.value, megatapDelayTapCount.value,
        megatapDelayTimeAlpha.value, megatapDelayTimeRandomize.value,
        megatapDelayAmplitudeShape.value, megatapDelayAmplitudeAlpha.value,
        megatapDelayAmplitudeRandomize.value, megatapDelayPanShape.value,
        megatapDelayPanAlpha.value, megatapDelayStereoSpread.value,
        megatapDelayPredelay.value, megatapDelayFeedback.value,
        megatapDelayFeedbackTap.value, megatapDelayLowCut.value,
        megatapDelayHighCut.value, megatapDelayDiffusionMix.value,
        megatapDelayDiffusionTime.value, megatapDelayDiffusionRate.value,
        megatapDelayDiffusionDepth.value, megatapDelayEnvelopeThreshold.value,
        megatapDelayEnvelopeAttack.value, megatapDelayEnvelopeRelease.value,
        megatapDelayEnvelopeEnabled.checked, megatapDelayInputGain.value,
        megatapDelayMix.value, megatapDelayLevel.value,
        megatapDelayInputSelect.value] = [
        "750", "12", "0.75", "25", "1", "2", "10", "3", "60", "30",
        "90", "45", "3", "120", "10000", "50", "5", "1.25", "1.5",
        "-40", "250", "100", true, "3", "75", "-2", "2",
      ];
      megatapDelayInputSelect.dispatchEvent(new Event("change", { bubbles: true }));
      const megatapDelayMessage = megatapDelayMessages.at(-1);
      if (megatapDelayMessage?.type !== "megatap-delay" ||
          megatapDelayMessage.value.join(",") !==
            "750,12,0.75,25,1,2,10,3,60,30,90,0.45,3,120,10000,0.5,5,1.25,1.5,-40,250,100,3,0.75,-2,2,1" ||
          megatapDelayTime.parentElement.querySelector("output").value !== "750 ms" ||
          megatapDelayFeedback.parentElement.querySelector("output").value !== "45%" ||
          megatapDelayInputSelect.parentElement.querySelector("output").value !== "Right only") {
        throw new Error(`Megatap Delay controls must send all project settings in WASM parameter order (type ${megatapDelayMessage?.type || "missing"}; values ${megatapDelayMessage?.value?.join(",") || "missing"}; time ${megatapDelayTime.parentElement.querySelector("output").value}; feedback ${megatapDelayFeedback.parentElement.querySelector("output").value}; input ${megatapDelayInputSelect.parentElement.querySelector("output").value}).`);
      }
    } finally {
      workletNode = previousMegatapDelayWorklet;
      megatapDelayTapCount.value = megatapDelayControlSnapshot[1];
      megatapDelayFeedbackTap.max = megatapDelayTapCount.value;
      megatapDelayControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = megatapDelayControlSnapshot[index];
        else control.value = megatapDelayControlSnapshot[index];
      });
      updateMegatapDelayOptions();
    }
    if (DEFAULT_PARAMETERS.megatapDelay.join(",") !== megatapDelayDefaults.join(",")) {
      throw new Error(`Megatap Delay self-test must restore project-authored defaults (expected ${megatapDelayDefaults.join(",")}; got ${DEFAULT_PARAMETERS.megatapDelay.join(",")}).`);
    }
    const deEsserDefaults = [...DEFAULT_PARAMETERS.deEsser];
    const deEsserControlSnapshot = deEsserControls.map((control) => control.value);
    const previousDeEsserWorklet = workletNode;
    const deEsserMessages = [];
    try {
      workletNode = { port: { postMessage: (message) => deEsserMessages.push(message) } };
      [deEsserFrequency.value, deEsserQ.value, deEsserThreshold.value,
        deEsserReduction.value, deEsserAttack.value, deEsserRelease.value] =
        ["7500", "2.5", "-36", "9", "4", "120"];
      deEsserRelease.dispatchEvent(new Event("change", { bubbles: true }));
      const message = deEsserMessages.at(-1);
      if (message?.type !== "de-esser" ||
          message.value.join(",") !== "7500,2.5,-36,9,4,120" ||
          deEsserFrequency.parentElement.querySelector("output").value !== "7500 Hz" ||
          deEsserQ.parentElement.querySelector("output").value !== "2.50" ||
          deEsserThreshold.parentElement.querySelector("output").value !== "-36 dBFS" ||
          deEsserReduction.parentElement.querySelector("output").value !== "9 dB" ||
          deEsserAttack.parentElement.querySelector("output").value !== "4 ms" ||
          deEsserRelease.parentElement.querySelector("output").value !== "120 ms") {
        throw new Error("De-Esser controls must send project settings in WASM parameter order and display their values.");
      }
    } finally {
      workletNode = previousDeEsserWorklet;
      deEsserControls.forEach((control, index) => {
        control.value = deEsserControlSnapshot[index];
      });
      updateDeEsserOptions();
    }
    if (DEFAULT_PARAMETERS.deEsser.join(",") !== deEsserDefaults.join(",")) {
      throw new Error("De-Esser self-test must restore project-authored defaults.");
    }
    const dualDistortionDefaults = [...DEFAULT_PARAMETERS.dualEngineDistortion];
    const dualDistortionControlSnapshot = dualDistortionControls.map((control) =>
      control.type === "checkbox" ? control.checked : control.value);
    const previousDualDistortionWorklet = workletNode;
    const dualDistortionMessages = [];
    try {
      workletNode = { port: { postMessage: (message) => dualDistortionMessages.push(message) } };
      [dualDistortionGainA.value, dualDistortionToneA.value,
        dualDistortionGainB.value, dualDistortionToneB.value,
        dualDistortionBlend.value, dualDistortionMode.value,
        dualDistortionLevel.value] =
        ["0.2", "0.4", "0.8", "0.6", "0.3", "1", "3"];
      dualDistortionBassBoost.checked = true;
      dualDistortionLevel.dispatchEvent(new Event("input", { bubbles: true }));
      const message = dualDistortionMessages.at(-1);
      if (message?.type !== "dual-engine-distortion" ||
          message.value.join(",") !== "0.2,0.4,0.8,0.6,0.3,1,1,3" ||
          dualDistortionBlend.parentElement.querySelector("output").value !== "30% B" ||
          dualDistortionMode.parentElement.querySelector("output").value !== "Series" ||
          dualDistortionBassBoost.parentElement.querySelector("output").value !== "On" ||
          dualDistortionLevel.parentElement.querySelector("output").value !== "3 dB") {
        throw new Error("Dual-Engine Distortion controls must send project settings in WASM parameter order and display their values.");
      }
    } finally {
      workletNode = previousDualDistortionWorklet;
      dualDistortionControls.forEach((control, index) => {
        if (control.type === "checkbox") {
          control.checked = dualDistortionControlSnapshot[index];
        } else {
          control.value = dualDistortionControlSnapshot[index];
        }
      });
      updateDualEngineDistortionOptions();
    }
    if (DEFAULT_PARAMETERS.dualEngineDistortion.join(",") !==
        dualDistortionDefaults.join(",")) {
      throw new Error("Dual-Engine Distortion self-test must restore project-authored defaults.");
    }
    const vibeDefaults = [...DEFAULT_PARAMETERS.vibe];
    const vibeControlSnapshot = vibeControls.map((control) => control.value);
    const previousVibeWorklet = workletNode;
    const vibeMessages = [];
    try {
      workletNode = { port: { postMessage: (message) => vibeMessages.push(message) } };
      [vibeRate.value, vibeDepth.value, vibeTrem.value,
        vibeStages.value, vibeSpread.value, vibeLevel.value,
        vibeRateMode.value, vibeRateDivision.value, vibeTempoBpm.value] =
        ["2.3", "82", "35", "1", "75", "0.9", "1", "7", "90"];
      vibeSpread.dispatchEvent(new Event("input", { bubbles: true }));
      const message = vibeMessages.at(-1);
      if (message?.type !== "vibe" ||
          message.value.join(",") !== "2.3,0.82,0.35,1,0.75,0.9,1,7,90" ||
          vibeRate.parentElement.querySelector("output").value !== "Eighth · 90 BPM · 3.00 Hz" ||
          vibeDepth.parentElement.querySelector("output").value !== "82%" ||
          vibeTrem.parentElement.querySelector("output").value !== "35%" ||
          vibeStages.parentElement.querySelector("output").value !== "1" ||
          vibeSpread.parentElement.querySelector("output").value !== "75%" ||
          vibeLevel.parentElement.querySelector("output").value !== "0.90" ||
          vibeRateMode.parentElement.querySelector("output").value !== "Tempo note" ||
          vibeRateDivision.parentElement.querySelector("output").value !== "Eighth" ||
          vibeTempoBpm.parentElement.querySelector("output").value !== "90 BPM") {
        throw new Error("Vibe controls must send project settings in WASM parameter order and display their values.");
      }
    } finally {
      workletNode = previousVibeWorklet;
      vibeControls.forEach((control, index) => { control.value = vibeControlSnapshot[index]; });
      updateVibeOptions();
    }
    if (DEFAULT_PARAMETERS.vibe.join(",") !== vibeDefaults.join(",")) {
      throw new Error("Vibe self-test must restore project-authored defaults.");
    }
    const uniVerseVibeDefaults = [...DEFAULT_PARAMETERS.uniVerseVibe];
    const uniVerseVibeControlSnapshot = uniVerseVibeControls.map((control) => control.value);
    const previousUniVerseVibeWorklet = workletNode;
    const uniVerseVibeMessages = [];
    try {
      workletNode = { port: { postMessage: (message) => uniVerseVibeMessages.push(message) } };
      [uniVerseVibeSpeed.value, uniVerseVibeIntensity.value,
        uniVerseVibeVolume.value, uniVerseVibeRateMode.value,
        uniVerseVibeRateDivision.value, uniVerseVibeTempoBpm.value,
        uniVerseVibeToneMode.value, uniVerseVibeModulationMode.value] =
        ["2.3", "82", "1.1", "1", "7", "90", "1", "1"];
      uniVerseVibeIntensity.dispatchEvent(new Event("input", { bubbles: true }));
      const message = uniVerseVibeMessages.at(-1);
      if (message?.type !== "uni-verse-vibe" ||
          message.value.join(",") !== "2.3,0.82,1.1,1,7,90,1,1" ||
          uniVerseVibeSpeed.parentElement.querySelector("output").value !== "Eighth · 90 BPM · 3.00 Hz" ||
          uniVerseVibeIntensity.parentElement.querySelector("output").value !== "82%" ||
          uniVerseVibeVolume.parentElement.querySelector("output").value !== "1.10" ||
          uniVerseVibeToneMode.parentElement.querySelector("output").value !== "Vintage" ||
          uniVerseVibeModulationMode.parentElement.querySelector("output").value !== "Vibrato" ||
          !uniVerseVibeSpeed.disabled || uniVerseVibeRateDivision.disabled ||
          uniVerseVibeTempoBpm.disabled) {
        throw new Error("Uni-Verse Vibe controls must send project settings in WASM parameter order and display their values.");
      }
    } finally {
      workletNode = previousUniVerseVibeWorklet;
      uniVerseVibeControls.forEach((control, index) => {
        control.value = uniVerseVibeControlSnapshot[index];
      });
      updateUniVerseVibeOptions();
    }
    if (DEFAULT_PARAMETERS.uniVerseVibe.join(",") !== uniVerseVibeDefaults.join(",")) {
      throw new Error("Uni-Verse Vibe self-test must restore project-authored defaults.");
    }
    const multiDelayDefaults = JSON.stringify(DEFAULT_PARAMETERS.multiDelay);
    const multiDelayControlSnapshot = multiDelayControls.map((control) => control.value);
    const previousMultiDelayWorklet = workletNode;
    const multiDelayMessages = [];
    try {
      workletNode = { port: { postMessage: (message) => multiDelayMessages.push(message) } };
      const firstLine = multiDelayLineControls[0];
      const firstLfo = multiDelayLfoControls[0];
      const secondLfo = multiDelayLfoControls[1];
      const testTimeMs = Math.min(620, Number(firstLine.timeMs.max));
      firstLine.timeMs.value = String(testTimeMs);
      firstLine.timeMode.value = "1";
      firstLine.division.value = "10";
      firstLine.feedback.value = "-20";
      firstLine.crossFeedback.value = "25";
      firstLine.level.value = "80";
      firstLine.pan.value = "25";
      multiDelayTempo.value = "96";
      multiDelayLowCut.value = "100";
      multiDelayHighCut.value = "8000";
      multiDelayInputGain.value = "3";
      multiDelayMix.value = "40";
      multiDelayLevel.value = "-3";
      firstLfo.waveform.value = "8";
      firstLfo.phaseDegrees.value = "135";
      firstLfo.rateMode.value = "1";
      firstLfo.rateHz.value = "3.25";
      firstLfo.division.value = "13";
      firstLfo.depthMs.value = "4.5";
      secondLfo.waveform.value = "5";
      secondLfo.phaseDegrees.value = "45";
      secondLfo.rateMode.value = "0";
      secondLfo.rateHz.value = "0.75";
      secondLfo.division.value = "4";
      secondLfo.depthMs.value = "0";
      multiDelayMix.dispatchEvent(new Event("input", { bubbles: true }));
      const message = multiDelayMessages.at(-1);
      const line = message?.value?.lines?.[0];
      const timeLfo = message?.value?.timeLfo;
      if (message?.type !== "multi-delay" ||
          line?.timeMs !== testTimeMs || line.timeMode !== 1 || line.division !== 10 ||
          line.feedback !== -0.2 || line.crossFeedback !== 0.25 ||
          line.level !== 0.8 || line.pan !== 0.25 ||
          timeLfo?.length !== 2 ||
          timeLfo[0]?.waveform !== 8 || timeLfo[0]?.phaseDegrees !== 135 ||
          timeLfo[0]?.rateHz !== 3.25 || timeLfo[0]?.tempoSync !== 1 ||
          timeLfo[0]?.division !== 13 || timeLfo[0]?.depthMs !== 4.5 ||
          timeLfo[1]?.waveform !== 5 || timeLfo[1]?.phaseDegrees !== 45 ||
          timeLfo[1]?.rateHz !== 0.75 || timeLfo[1]?.tempoSync !== 0 ||
          timeLfo[1]?.division !== 4 || timeLfo[1]?.depthMs !== 0 ||
          !firstLfo.rateHz.disabled || firstLfo.division.disabled ||
          secondLfo.rateHz.disabled || !secondLfo.division.disabled ||
          firstLfo.waveform.parentElement.querySelector("output").value !== "Trapezoid" ||
          firstLfo.phaseDegrees.parentElement.querySelector("output").value !== "135°" ||
          firstLfo.rateMode.parentElement.querySelector("output").value !== "Tempo sync" ||
          firstLfo.rateHz.parentElement.querySelector("output").value !== "3.25 Hz" ||
          firstLfo.division.parentElement.querySelector("output").value !== "Sixty-fourth" ||
          firstLfo.depthMs.parentElement.querySelector("output").value !== "4.5 ms" ||
          secondLfo.waveform.parentElement.querySelector("output").value !== "Random" ||
          secondLfo.phaseDegrees.parentElement.querySelector("output").value !== "45°" ||
          secondLfo.rateMode.parentElement.querySelector("output").value !== "Free rate" ||
          secondLfo.rateHz.parentElement.querySelector("output").value !== "0.75 Hz" ||
          secondLfo.division.parentElement.querySelector("output").value !== "Quarter" ||
          secondLfo.depthMs.parentElement.querySelector("output").value !== "0.0 ms" ||
          message.value.tempoBpm !== 96 || message.value.lowCutHz !== 100 ||
          message.value.highCutHz !== 8000 || message.value.inputGainDb !== 3 ||
          message.value.mix !== 0.4 || message.value.levelDb !== -3 ||
          firstLine.timeMs.parentElement.querySelector("output").value !== `${testTimeMs} ms` ||
          firstLine.timeMode.parentElement.querySelector("output").value !== "Tempo" ||
          firstLine.division.parentElement.querySelector("output").value !== "Sixteenth" ||
          multiDelayMix.parentElement.querySelector("output").value !== "40%" ||
          multiDelayLevel.parentElement.querySelector("output").value !== "-3 dB") {
        throw new Error("Multi-Delay controls must send four-line project settings in WASM parameter order and display their values.");
      }
      if (!firstLine.timeMs.disabled) {
        throw new Error("Tempo-synced Multi-Delay must disable its free-time control.");
      }
      multiDelayTopology.value = "2";
      multiDelayDiffusorMasterTime.value = "150";
      multiDelayDiffusorMasterFeedback.value = "50";
      multiDelayDiffusorMasterFeedback.dispatchEvent(new Event("input", { bubbles: true }));
      const diffusorMessage = multiDelayMessages.at(-1);
      if (diffusorMessage?.value?.topology !== 2 ||
          diffusorMessage.value.masterTime !== 1.5 ||
          diffusorMessage.value.masterFeedback !== 0.5 ||
          !multiDelaySeriesPanel.classList.contains("is-hidden") ||
          multiDelayDiffusorPanel.classList.contains("is-hidden") ||
          !firstLine.feedback.disabled || !firstLine.feedback.parentElement.classList.contains("is-hidden") ||
          !firstLine.level.disabled || !firstLine.level.parentElement.classList.contains("is-hidden") ||
          !firstLine.pan.disabled || !firstLine.pan.parentElement.classList.contains("is-hidden") ||
          !firstLine.crossFeedback.parentElement.classList.contains("is-hidden") ||
          multiDelayTopology.parentElement.querySelector("output").value !== "Quad Diffusor") {
        throw new Error("Quad Diffusor controls must send master values, expose diffuser feedback bounds, and show the active topology.");
      }
      multiDelayTopology.value = "3";
      multiDelayDiffusorMasterTime.value = "120";
      multiDelayDiffusorMasterFeedback.value = "80";
      multiDelayDiffusorMasterFeedback.dispatchEvent(new Event("input", { bubbles: true }));
      const quadTapMessage = multiDelayMessages.at(-1);
      if (quadTapMessage?.value?.topology !== 3 ||
          quadTapMessage.value.masterTime !== 1.2 ||
          quadTapMessage.value.masterFeedback !== 0.8 ||
          multiDelayDiffusorPanel.classList.contains("is-hidden") ||
          firstLine.feedback.disabled || firstLine.feedback.parentElement.classList.contains("is-hidden") ||
          !firstLine.crossFeedback.disabled || !firstLine.crossFeedback.parentElement.classList.contains("is-hidden") ||
          firstLine.level.disabled || firstLine.level.parentElement.classList.contains("is-hidden") ||
          firstLine.pan.disabled || firstLine.pan.parentElement.classList.contains("is-hidden") ||
          multiDelayDiffusorMasterFeedback.max !== "200" ||
          multiDelayTopology.parentElement.querySelector("output").value !== "Quad-Tap") {
        throw new Error("Quad-Tap controls must expose independent tap feedback, level, pan and shared scaling without cross-feedback.");
      }
      multiDelayDiffusorMasterFeedback.value = "160";
      multiDelayDiffusorMasterFeedback.dispatchEvent(new Event("input", { bubbles: true }));
      multiDelayTopology.value = "2";
      updateMultiDelayOptions();
      if (multiDelayDiffusorMasterFeedback.value !== "90" ||
          multiDelayMessages.at(-1)?.value?.masterFeedback !== 0.9) {
        throw new Error("Switching from Quad-Tap to Diffusor must clamp Master Feedback to its topology-specific project range.");
      }
      multiDelayTopology.value = "4";
      updateMultiDelayOptions();
      if (multiDelayMessages.at(-1)?.value?.topology !== 4 ||
          multiDelayDiffusorPanel.classList.contains("is-hidden") ||
          multiDelayDiffusorMasterFeedback.max !== "200" ||
          firstLine.feedback.disabled || firstLine.feedback.parentElement.classList.contains("is-hidden") ||
          !firstLine.crossFeedback.disabled || !firstLine.crossFeedback.parentElement.classList.contains("is-hidden") ||
          firstLine.level.disabled || firstLine.pan.disabled ||
          !multiDelayTopologyMasterHint.textContent.includes("after its feedback loop") ||
          multiDelayTopology.parentElement.querySelector("output").value !== "Quad Tap Band") {
        throw new Error("Quad Tap Band controls must expose independent feedback lines and describe its external filter position.");
      }
      multiDelayTopology.value = "5";
      multiDelayMotorSpeed.value = "0.8";
      updateMultiDelayOptions();
      const quadTapeMessage = multiDelayMessages.at(-1);
      if (quadTapeMessage?.value?.topology !== 5 ||
          quadTapeMessage.value.motorSpeed !== 0.8 ||
          multiDelayMotorPanel.classList.contains("is-hidden") ||
          multiDelayMotorSpeed.disabled ||
          multiDelayMotorSpeed.parentElement.querySelector("output").value !== "0.800×" ||
          multiDelayDiffusorMasterFeedback.max !== "200" ||
          multiDelayTopology.parentElement.querySelector("output").value !== "Quad Tape" ||
          !multiDelayTopologyMasterHint.textContent.includes("while it moves")) {
        throw new Error("Quad Tape controls must send motor speed and identify the project-authored read-head behavior.");
      }
    } finally {
      workletNode = previousMultiDelayWorklet;
      multiDelayControls.forEach((control, index) => { control.value = multiDelayControlSnapshot[index]; });
      updateMultiDelayOptions();
    }
    if (JSON.stringify(DEFAULT_PARAMETERS.multiDelay) !== multiDelayDefaults) {
      throw new Error("Multi-Delay self-test must restore project-authored defaults.");
    }
    const portaPreampDefaults = [...DEFAULT_PARAMETERS.portaPreamp];
    const portaPreampControlSnapshot = portaPreampControls.map((control) => control.value);
    const previousPortaPreampWorklet = workletNode;
    const portaPreampMessages = [];
    try {
      workletNode = { port: { postMessage: (message) => portaPreampMessages.push(message) } };
      [portaPreampGain1.value, portaPreampGain2.value, portaPreampBass.value,
        portaPreampTreble.value, portaPreampLevel.value] = ["9", "4", "2", "-1", "0"];
      portaPreampLevel.dispatchEvent(new Event("input", { bubbles: true }));
      const message = portaPreampMessages.at(-1);
      if (message?.type !== "porta-preamp" || message.value.join(",") !== "9,4,2,-1,0" ||
          portaPreampGain1.parentElement.querySelector("output").value !== "9.0 dB" ||
          portaPreampTreble.parentElement.querySelector("output").value !== "-1.0 dB") {
        throw new Error("Porta Preamp controls must send project settings in WASM parameter order and display their values.");
      }
    } finally {
      workletNode = previousPortaPreampWorklet;
      portaPreampControls.forEach((control, index) => { control.value = portaPreampControlSnapshot[index]; });
      updatePortaPreampOptions();
    }
    if (DEFAULT_PARAMETERS.portaPreamp.join(",") !== portaPreampDefaults.join(",")) {
      throw new Error("Porta Preamp self-test must restore project-authored defaults.");
    }
    const pitchSequenceDefaults = [...DEFAULT_PARAMETERS.pitchSequence];
    const pitchSequenceControlSnapshot = pitchSequenceControls.map((control) =>
      control.type === "checkbox" ? control.checked : control.value);
    const previousPitchSequenceWorklet = workletNode;
    const pitchSequenceMessages = [];
    try {
      workletNode = { port: { postMessage: (message) => pitchSequenceMessages.push(message) } };
      pitchSequenceSteps[0].semitones.value = "-5";
      pitchSequenceSteps[0].mode.value = "1";
      pitchSequenceStepsCount.value = "3";
      pitchSequenceRun.checked = true;
      pitchSequenceTimeMode.value = "1";
      pitchSequenceDivision.value = "7";
      pitchSequenceFreeMs.value = "180";
      pitchSequenceTempo.value = "96";
      pitchSequenceGlide.value = "40";
      pitchSequenceGlitchDepth.value = "4";
      pitchSequenceGlitchInterval.value = "25";
      pitchSequenceFreezeWindow.value = "50";
      pitchSequenceMix.value = "60";
      pitchSequenceLevel.value = "80";
      pitchSequenceLevel.dispatchEvent(new Event("input", { bubbles: true }));
      const message = pitchSequenceMessages.at(-1);
      if (message?.type !== "pitch-sequence" || message.value.length !== 44 ||
          message.value[0] !== -5 || message.value[16] !== 1 ||
          message.value[32] !== 3 || message.value[33] !== 1 ||
          message.value[34] !== 1 || message.value[35] !== 7 ||
          message.value[36] !== 180 || message.value[37] !== 96 ||
          message.value[38] !== 40 || message.value[39] !== 4 ||
          message.value[40] !== 25 || message.value[41] !== 50 ||
          message.value[42] !== 0.6 || message.value[43] !== 0.8 ||
          pitchSequenceSteps[0].semitones.parentElement.querySelector("output").value !== "-5 semitones" ||
          pitchSequenceSteps[0].mode.parentElement.querySelector("output").value !== "Glitch" ||
          pitchSequenceDivision.parentElement.querySelector("output").value !== "Eighth") {
        throw new Error("Pitch Sequence controls must send all 16 shifts/modes and global settings in WASM parameter order and display their values.");
      }
    } finally {
      workletNode = previousPitchSequenceWorklet;
      pitchSequenceControls.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = pitchSequenceControlSnapshot[index];
        else control.value = pitchSequenceControlSnapshot[index];
      });
      updatePitchSequenceOptions();
    }
    if (DEFAULT_PARAMETERS.pitchSequence.join(",") !== pitchSequenceDefaults.join(",")) {
      throw new Error("Pitch Sequence self-test must restore project-authored defaults.");
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
    const duckingDelayTempoDefaults = [
      duckingDelayTimeMode.value,
      duckingDelayTempo.value,
      duckingDelayTapDivision.value,
      duckingDelayLowCut.value,
      duckingDelayHighCut.value,
      duckingDelayModRate.value,
      duckingDelayModDepth.value,
      duckingDelayModMode.value,
    ];
    duckingDelayTimeMode.value = "1";
    duckingDelayTimeMode.dispatchEvent(new Event("change", { bubbles: true }));
    duckingDelayTempo.value = "90";
    duckingDelayTempo.dispatchEvent(new Event("input", { bubbles: true }));
    duckingDelayTapDivision.value = "7";
    duckingDelayTapDivision.dispatchEvent(new Event("change", { bubbles: true }));
    duckingDelayLowCut.value = "150";
    duckingDelayLowCut.dispatchEvent(new Event("input", { bubbles: true }));
    duckingDelayHighCut.value = "7000";
    duckingDelayHighCut.dispatchEvent(new Event("input", { bubbles: true }));
    duckingDelayModRate.value = "1.25";
    duckingDelayModRate.dispatchEvent(new Event("input", { bubbles: true }));
    duckingDelayModDepth.value = "6.5";
    duckingDelayModDepth.dispatchEvent(new Event("input", { bubbles: true }));
    duckingDelayModMode.value = "3";
    duckingDelayModMode.dispatchEvent(new Event("change", { bubbles: true }));
    if (DEFAULT_PARAMETERS.duckingDelay[7] !== 1 ||
        DEFAULT_PARAMETERS.duckingDelay[8] !== 90 ||
        DEFAULT_PARAMETERS.duckingDelay[9] !== 7 ||
        DEFAULT_PARAMETERS.duckingDelay[10] !== 150 ||
        DEFAULT_PARAMETERS.duckingDelay[11] !== 7000 ||
        DEFAULT_PARAMETERS.duckingDelay[12] !== 1.25 ||
        DEFAULT_PARAMETERS.duckingDelay[13] !== 6.5 ||
        DEFAULT_PARAMETERS.duckingDelay[14] !== 3 ||
        !duckingDelayTime.disabled ||
        duckingDelayTempoValue.value !== "90 BPM" ||
        duckingDelayLowCutValue.value !== "150 Hz" ||
        duckingDelayHighCutValue.value !== "7000 Hz" ||
        duckingDelayModRateValue.value !== "1.25 Hz" ||
        duckingDelayModDepthValue.value !== "6.5 ms") {
      throw new Error("Ducking Delay tempo, repeat-cut, and modulation controls must reach browser parameters and display their values.");
    }
    [duckingDelayTimeMode.value, duckingDelayTempo.value,
      duckingDelayTapDivision.value, duckingDelayLowCut.value,
      duckingDelayHighCut.value, duckingDelayModRate.value,
      duckingDelayModDepth.value, duckingDelayModMode.value] = duckingDelayTempoDefaults;
    updateDuckingDelay();
    const defaultDuckSpillover = duckingDelaySpillover.checked;
    duckingDelaySpillover.checked = true;
    duckingDelaySpillover.dispatchEvent(new Event("change", { bubbles: true }));
    if (DEFAULT_PARAMETERS.duckingDelay[6] !== 1) {
      throw new Error("Ducking Delay Spillover must reach the browser effect parameters.");
    }
    duckingDelaySpillover.checked = defaultDuckSpillover;
    duckingDelaySpillover.dispatchEvent(new Event("change", { bubbles: true }));
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
    const defaultFilteredDelayMode = filteredDelayTimeMode.value;
    const defaultFilteredDelayDivision = filteredDelayDivision.value;
    filteredDelayTimeMode.value = "1";
    filteredDelayDivision.value = "12";
    filteredDelayTimeMode.dispatchEvent(new Event("change", { bubbles: true }));
    if (DEFAULT_PARAMETERS.filteredDelay[5] !== 1 ||
        DEFAULT_PARAMETERS.filteredDelay[7] !== 12 ||
        !filteredDelayTime.disabled || filteredDelayTempoBpm.disabled ||
        !filteredDelayTimeValue.value.startsWith("1/32")) {
      throw new Error("Filtered Delay tempo controls must update the processor and mode display.");
    }
    filteredDelayTimeMode.value = defaultFilteredDelayMode;
    filteredDelayDivision.value = defaultFilteredDelayDivision;
    filteredDelayTimeMode.dispatchEvent(new Event("change", { bubbles: true }));
    const defaultSweepRate = sweepDelayRate.value;
    sweepDelayRate.value = "1.25";
    sweepDelayRate.dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.sweepDelay[6] !== 1.25 ||
        sweepDelayRateValue.value !== "1.25 Hz") {
      throw new Error("Sweep Delay controls must update processor parameters and display values.");
    }
    sweepDelayRate.value = defaultSweepRate;
    sweepDelayRate.dispatchEvent(new Event("input", { bubbles: true }));
    const defaultSweepFilterType = sweepDelayFilterType.value;
    sweepDelayFilterType.value = "2";
    sweepDelayFilterType.dispatchEvent(new Event("change", { bubbles: true }));
    if (DEFAULT_PARAMETERS.sweepDelay[9] !== 2) {
      throw new Error("Sweep Delay filter type must update the processor parameters.");
    }
    sweepDelayFilterType.value = defaultSweepFilterType;
    sweepDelayFilterType.dispatchEvent(new Event("change", { bubbles: true }));
    const defaultSweepRateMode = sweepDelayRateMode.value;
    const defaultSweepTempo = sweepDelayTempoBpm.value;
    const defaultSweepDivision = sweepDelayDivision.value;
    sweepDelayRateMode.value = "1";
    sweepDelayTempoBpm.value = "100";
    sweepDelayDivision.value = "2";
    sweepDelayRateMode.dispatchEvent(new Event("change", { bubbles: true }));
    if (DEFAULT_PARAMETERS.sweepDelay[10] !== 1 ||
        DEFAULT_PARAMETERS.sweepDelay[11] !== 100 ||
        DEFAULT_PARAMETERS.sweepDelay[12] !== 2 ||
        !sweepDelayRate.disabled || sweepDelayTempoBpm.disabled ||
        sweepDelayDivision.disabled ||
        sweepDelayRateValue.value !== "Half · 100 BPM") {
      throw new Error("Sweep Delay tempo controls must update the processor and mode display.");
    }
    sweepDelayRateMode.value = defaultSweepRateMode;
    sweepDelayTempoBpm.value = defaultSweepTempo;
    sweepDelayDivision.value = defaultSweepDivision;
    sweepDelayRateMode.dispatchEvent(new Event("change", { bubbles: true }));
    effectCategoryFilter.value = "vocal";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const vocalIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
            if (vocalIds.join(",") !== "69" || effectCount.value !== `1 of ${effectRows.length}` ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error(`Vocal FX picker mismatch: IDs=${vocalIds.join(",")}; count=${effectCount.value}; chainStatePreserved=${JSON.stringify(enabledEffects()) === JSON.stringify(initialEffectState)}.`);
    }
    effectCategoryFilter.value = "modulation";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const modulationIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
    const expectedModulationIds = effectRows.filter((row) => row.dataset.category === "modulation")
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
    if (modulationIds.join(",") !== expectedModulationIds.join(",")) {
      throw new Error(`Modulation filter returned the wrong IDs (visible ${modulationIds.join(",")}; expected ${expectedModulationIds.join(",")}).`);
    }
    if (effectCount.value !== `${expectedModulationIds.length} of ${effectRows.length}`) {
      throw new Error(`Modulation filter count mismatch: ${effectCount.value}; expected ${expectedModulationIds.length} of ${effectRows.length}.`);
    }
    if (JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("Modulation category filtering changed processor chain state.");
    }
    effectCategoryFilter.value = "dynamics";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const dynamicsIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
               if (dynamicsIds.join(",") !== "4,5,6,23,24,32,45,56,95" ||
                      effectCount.value !== `9 of ${effectRows.length}` ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Dynamics filter must include Auto Swell and preserve the current chain state.");
    }
    effectCategoryFilter.value = "drive";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const driveIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
         if (driveIds.join(",") !== "9,10,11,12,13,14,46,70,80,82,85,88,89" ||
                      effectCount.value !== `13 of ${effectRows.length}` ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Drive & fuzz filter must include the distortion, boost, fuzz, crusher, and three-stage processors while preserving the current chain state.");
    }
    const originalMercatorParameters = [...DEFAULT_PARAMETERS.mercatorDistortion];
    mercatorDistortionControls[0].value = "0.62";
    mercatorDistortionControls[0].dispatchEvent(new Event("input", { bubbles: true }));
    mercatorDistortionControls[5].checked = true;
    mercatorDistortionControls[5].dispatchEvent(new Event("change", { bubbles: true }));
    if (DEFAULT_PARAMETERS.mercatorDistortion[0] !== 0.62 ||
        DEFAULT_PARAMETERS.mercatorDistortion[5] !== 1 ||
        mercatorDistortionControls[0].parentElement.querySelector("output").value !== "0.62" ||
        mercatorDistortionControls[5].parentElement.querySelector("output").value !== "On") {
      throw new Error("Three-Stage Distortion controls must update their displayed and chain parameters.");
    }
    originalMercatorParameters.forEach((value, index) => {
      const control = mercatorDistortionControls[index];
      if (control.type === "checkbox") control.checked = Boolean(value);
      else control.value = String(value);
    });
    updateMercatorDistortion();
     const originalOctobotParameters = [...DEFAULT_PARAMETERS.octobot];
     const octobotHint = document.querySelector(
       '.gain-controls[aria-label="Octobot project-authored controls"] .hint',
     ).textContent;
     if (octobotControls.length !== 3 ||
         octobotControls.some((control) => control.min !== "0" || control.max !== "1") ||
         octobotHint.includes("Fender settings") ||
         !octobotHint.includes("no numeric ranges") ||
         !octobotHint.includes("not Fender sound or settings parity") ||
         effectRows[82]?.dataset.category !== "drive") {
       throw new Error("Octobot controls, Drive & fuzz categorization, or vendor/project qualification is missing.");
     }
     octobotControls[1].value = "0.42";
     octobotControls[1].dispatchEvent(new Event("input", { bubbles: true }));
     if (DEFAULT_PARAMETERS.octobot[1] !== 0.42 ||
         octobotControls[1].parentElement.querySelector("output").value !== "42%") {
       throw new Error("Octobot controls must update their displayed and chain parameters.");
     }
     originalOctobotParameters.forEach((value, index) => {
       octobotControls[index].value = String(value);
     });
     updateOctobot();
     const originalTapeEchoParameters = [...DEFAULT_PARAMETERS.tapeEcho];
     const tapeEchoHint = document.querySelector(
       '.effect-parameter-group[aria-label="Tape Echo project-authored controls"] .hint',
     ).textContent;
      if (tapeEchoControls.length !== 16 ||
         tapeEchoControls[4].min !== "0" || tapeEchoControls[4].max !== "0.95" ||
         tapeEchoControls[10].type !== "checkbox" ||
         effectRows[84]?.dataset.category !== "delay" ||
         !tapeEchoHint.includes("no numeric ranges") ||
         !tapeEchoHint.includes("not Fender or Echoplex parity")) {
        throw new Error("Tape Echo control mapping, Delay categorization, or manufacturer/project qualification is missing.");
     }
     tapeEchoControls[0].value = "1";
     tapeEchoControls[3].value = "7";
     tapeEchoControls[4].value = "0.81";
     tapeEchoControls[4].dispatchEvent(new Event("input", { bubbles: true }));
     if (DEFAULT_PARAMETERS.tapeEcho[0] !== 1 ||
         DEFAULT_PARAMETERS.tapeEcho[3] !== 7 ||
         DEFAULT_PARAMETERS.tapeEcho[4] !== 0.81 ||
         tapeEchoControls[4].parentElement.querySelector("output").value !== "81%") {
       throw new Error("Tape Echo controls must update their display and parameter tuple.");
     }
     originalTapeEchoParameters.forEach((value, index) => {
       const control = tapeEchoControls[index];
       if (control.type === "checkbox") control.checked = Boolean(value);
       else control.value = String(value);
     });
     updateTapeEcho();
    const originalManualTzfParameters = [...DEFAULT_PARAMETERS.manualThruZeroFlanger];
    const manualTzfHint = document.querySelector(
      '.gain-controls[aria-label="Manual Through-Zero Flanger project-authored controls"] .hint',
    ).textContent;
    if (manualThruZeroFlangerControls.length !== 8 ||
        manualThruZeroFlangerControls[0].min !== "0.34" ||
        Math.abs(Number(manualThruZeroFlangerControls[0].max) -
          (Number(manualThruZeroFlangerControls[1].value) - 0.01)) > 0.0001 ||
        manualThruZeroFlangerControls[1].max !== "20" ||
        Math.abs(Number(manualThruZeroFlangerControls[1].min) -
          (Number(manualThruZeroFlangerControls[0].value) + 0.01)) > 0.0001 ||
        manualThruZeroFlangerControls[2].max !== "10" ||
        manualThruZeroFlangerControls[4].min !== "-100" ||
        manualThruZeroFlangerControls[4].max !== "100" ||
        manualTzfHint.includes("Fractal DSP") ||
        !manualTzfHint.includes("Manual mode only") ||
        !manualTzfHint.includes("384 frames at 48 kHz")) {
      throw new Error("Manual Through-Zero Flanger controls or source/latency qualifications are missing or incorrect.");
    }
    manualThruZeroFlangerControls[2].value = "6.25";
    manualThruZeroFlangerControls[2].dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.manualThruZeroFlanger[2] !== 6.25 ||
        manualThruZeroFlangerControls[2].parentElement.querySelector("output").value !== "6.25 ms") {
      throw new Error("Manual Through-Zero Flanger controls must update their displayed and chain parameters.");
    }
    originalManualTzfParameters.forEach((value, index) => {
      manualThruZeroFlangerControls[index].value = String(value);
    });
    updateManualThruZeroFlanger();
     effectCategoryFilter.value = "amp";
     effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
     const ampIds = effectRows.filter((row) => !row.hidden)
       .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
                   if (ampIds.join(",") !== "73" || effectCount.value !== `1 of ${effectRows.length}` ||
         JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
       throw new Error("The Amp / preamp category must return the Porta Preamp without changing chain state.");
     }
    effectCategoryFilter.value = "filter";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const filterIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
      if (filterIds.join(",") !== "2,3,7,8,36,37,43,87,96" ||
                        effectCount.value !== `9 of ${effectRows.length}` ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Filters category must include the wah and filter processors and preserve the current chain state.");
    }
    effectCategoryFilter.value = "eq";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const eqIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
    if (eqIds.join(",") !== "20,25,26,27,59,103" || effectCount.value !== `6 of ${effectRows.length}` ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Equalizer category must include both parametric EQs and the graphic/shelf EQs.");
    }
    effectCategoryFilter.value = "resonator";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const resonatorIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
                   if (resonatorIds.join(",") !== "44,49" || effectCount.value !== `2 of ${effectRows.length}` ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Resonator category must include both resonator processors and preserve the current chain state.");
    }
    effectCategoryFilter.value = "pitch";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const pitchIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
                   if (pitchIds.join(",") !== "22,50,51,74,99,101" || effectCount.value !== `6 of ${effectRows.length}` ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Pitch category must list the polyphonic shifter, harmonizer, pitch sequencers, and Chromatic Pitch Delay.");
    }
    effectCategoryFilter.value = "synth";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const synthIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
                   if (synthIds.join(",") !== "52,53,78,108" || effectCount.value !== `4 of ${effectRows.length}` ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Synth category must list the tracked guitar synths, free-running note sources, and three-voice tracked synth without changing chain state.");
    }
    effectCategoryFilter.value = "loop";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const loopIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
                    if (loopIds.join(",") !== "60,65,98" || effectCount.value !== `3 of ${effectRows.length}` ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Loop & performance category must list all three loopers and preserve the current chain state.");
    }
    effectCategoryFilter.value = "stereo";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const stereoIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
                   if (stereoIds.join(",") !== "61" || effectCount.value !== `1 of ${effectRows.length}` ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Stereo & image category must list the Enhancer and preserve chain state.");
    }
    const modernEnhancerEnabled = Array.from({ length: EFFECT_NAMES.length }, () => false);
    modernEnhancerEnabled[61] = true;
    if (processorChannelCount(1, modernEnhancerEnabled, { enhancer: [0] }) !== 2 ||
        processorChannelCount(1, modernEnhancerEnabled, { enhancer: [1] }) !== 1 ||
        processorChannelCount(1, Array.from({ length: EFFECT_NAMES.length }, () => false),
          { enhancer: [0] }) !== 1 ||
        processorChannelCount(2, modernEnhancerEnabled, { enhancer: [0] }) !== 2) {
      throw new Error("Modern Enhancer mono routing must expand only an enabled mono chain to stereo.");
    }
    effectCategoryFilter.value = "crossover";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const crossoverIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
        if (crossoverIds.join(",") !== "86" || effectCount.value !== `1 of ${effectRows.length}` ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Crossover category must list the XVR-inspired stage and preserve chain state.");
    }
    effectCategoryFilter.value = "filter";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const ashevilleFilterIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
     if (ashevilleFilterIds.join(",") !== "2,3,7,8,36,37,43,87,96" ||
             effectCount.value !== `9 of ${effectRows.length}` ||
        effectRows[87]?.dataset.category !== "filter" ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("Filters & wah must include Asheville Pattrn at ID 87 and preserve chain state.");
    }
    const originalStereoCrossover = { ...DEFAULT_PARAMETERS.stereoCrossover };
    stereoCrossoverControls.frequencyHz.value = "710";
    stereoCrossoverControls.frequencyHz.dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.stereoCrossover.frequencyHz !== 710 ||
        stereoCrossoverControls.frequencyHz.parentElement.querySelector("output").value !== "710 Hz") {
      throw new Error("Crossover Frequency did not update its displayed and named chain parameter.");
    }
    for (const [key, value] of Object.entries(originalStereoCrossover)) {
      stereoCrossoverControls[key].value = String(value);
    }
    updateStereoCrossover();
    const originalEnhancerMode = enhancerControls[0].value;
    enhancerControls[0].value = "2";
    enhancerControls[0].dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.enhancer[0] !== 2 ||
        enhancerControls[0].parentElement.querySelector("output").value !== "Stereoizer") {
      throw new Error("Enhancer mode control did not update its displayed and chain parameters.");
    }
    enhancerControls[0].value = originalEnhancerMode;
    enhancerControls[0].dispatchEvent(new Event("input", { bubbles: true }));
    const originalResonantDelay = [...DEFAULT_PARAMETERS.resonantDelay];
    resonantDelayControls[2].value = "1200";
    resonantDelayControls[2].dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.resonantDelay[2] !== 1200 ||
        resonantDelayControls[2].parentElement.querySelector("output").value !== "1200 Hz") {
      throw new Error("Resonant Delay frequency control did not update its displayed and chain parameters.");
    }
    originalResonantDelay.forEach((value, index) => {
      resonantDelayControls[index].value = String(value);
    });
    updateResonantDelay();
    effectCategoryFilter.value = "delay";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const delayIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
                if (delayIds.join(",") !== "21,38,39,40,41,42,54,55,62,64,66,68,72,75,76,84,91,92" || effectCount.value !== `18 of ${effectRows.length}` ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
      throw new Error("The Delay category must include Auto-Swell, Resonant, Digital, Grain, Pitch Feedback, Reverse, Ducking, Modulated, Filtered, and Sweep Delay and preserve the current chain state.");
    }
    effectCategoryFilter.value = "reverb";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    const reverbIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
                   if (reverbIds.join(",") !== "28,29,30,31,57,58,77,79" || effectCount.value !== `8 of ${effectRows.length}` ||
        JSON.stringify(enabledEffects()) !== JSON.stringify(initialEffectState)) {
       throw new Error("The Reverb filter must include Room, Hall, Plate, Spring, Shimmer, Convolution, Celestial, and Cloud Reverb without changing enabled state.");
    }
     const celestialDefaults = [...DEFAULT_PARAMETERS.celestialReverb];
     celestialReverbControls[0].value = "61";
     celestialReverbControls[0].dispatchEvent(new Event("input", { bubbles: true }));
     if (DEFAULT_PARAMETERS.celestialReverb[0] !== 0.61 ||
         celestialReverbControls[0].parentElement.querySelector("output").value !== "61%") {
       throw new Error("Celestial Reverb Mix control did not update its displayed and chain parameters.");
     }
     celestialDefaults.forEach((value, index) => {
       if (celestialReverbControls[index].type === "checkbox") {
         celestialReverbControls[index].checked = Boolean(value);
       } else {
         celestialReverbControls[index].value = String(index === 0 || index === 6
           ? value * 100 : value);
       }
     });
     updateCelestialReverb();
     const cloudDefaults = [...DEFAULT_PARAMETERS.cloudReverb];
     cloudReverbControls[0].value = "61";
     cloudReverbControls[0].dispatchEvent(new Event("input", { bubbles: true }));
     if (DEFAULT_PARAMETERS.cloudReverb[0] !== 0.61 ||
         cloudReverbControls[0].parentElement.querySelector("output").value !== "61%") {
       throw new Error("Cloud Reverb Mix control did not update its displayed and chain parameters.");
     }
     cloudDefaults.forEach((value, index) => {
       if (cloudReverbControls[index].type === "checkbox") {
         cloudReverbControls[index].checked = Boolean(value);
       } else {
         cloudReverbControls[index].value = String([0, 1, 3, 7].includes(index)
           ? value * 100 : value);
       }
     });
     updateCloudReverb();
    effectCategoryFilter.value = "all";
    effectCategoryFilter.dispatchEvent(new Event("change", { bubbles: true }));
    effectSearch.value = "guitar graphic";
    effectSearch.dispatchEvent(new Event("input", { bubbles: true }));
    const searchIds = effectRows.filter((row) => !row.hidden)
      .map((row) => Number(row.querySelector("[data-effect]").dataset.effect));
                 if (searchIds.join(",") !== "25" || effectCount.value !== `1 of ${effectRows.length}`) {
      throw new Error("Searching the effect browser returned the wrong processor.");
    }
    effectSearch.value = "";
    effectSearch.dispatchEvent(new Event("input", { bubbles: true }));
              if (effectCount.value !== `${effectRows.length} of ${effectRows.length}` || effectRows.some((row) => row.hidden)) {
      throw new Error("Clearing effect filters did not restore the complete chain.");
    }
    const defaultDiatonicHarmonizer = [...DEFAULT_PARAMETERS.diatonicHarmonizer];
    diatonicHarmonizerControls[1].value = "-3";
    diatonicHarmonizerControls[1].dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.diatonicHarmonizer[1] !== -3 ||
        diatonicHarmonizerControls[1].parentElement.querySelector("output").value !== "-3 degrees") {
      throw new Error("Diatonic Harmonizer controls did not update the selected scale interval.");
    }
    defaultDiatonicHarmonizer.forEach((value, index) => {
      diatonicHarmonizerControls[index].value = String(value);
    });
    updateDiatonicHarmonizer();
    const defaultPitchArpeggiator = [...DEFAULT_PARAMETERS.pitchArpeggiator];
    pitchArpeggiatorControls[0].checked = false;
    pitchArpeggiatorControls[1].value = "0.65";
    pitchArpeggiatorControls[5].value = "3";
    pitchArpeggiatorControls[14].value = "-1";
     pitchArpeggiatorControls[15].value = "7";
    pitchArpeggiatorControls[15].dispatchEvent(new Event("input", { bubbles: true }));
    pitchArpeggiatorControls[30].value = "-24";
    pitchArpeggiatorControls[30].dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.pitchArpeggiator[0] !== 0 ||
        DEFAULT_PARAMETERS.pitchArpeggiator[1] !== 0.65 ||
        DEFAULT_PARAMETERS.pitchArpeggiator[5] !== 3 ||
        DEFAULT_PARAMETERS.pitchArpeggiator[14] !== -1 ||
         DEFAULT_PARAMETERS.pitchArpeggiator[15] !== 7 ||
        DEFAULT_PARAMETERS.pitchArpeggiator[30] !== -24 ||
         pitchArpeggiatorControls[15].parentElement.querySelector("output").value !== "+7 degrees" ||
        pitchArpeggiatorControls[30].parentElement.querySelector("output").value !== "-24 dBFS" ||
        !pitchArpeggiatorControls[0].disabled) {
        throw new Error("Pitch Arpeggiator controls must update Run, supported shifts, and trigger threshold.");
    }
    defaultPitchArpeggiator.forEach((value, index) => {
      if (index === 0) pitchArpeggiatorControls[index].checked = Boolean(value);
      else pitchArpeggiatorControls[index].value = String(value);
    });
    updatePitchArpeggiator();
    const defaultMonoGuitarSynth = [...DEFAULT_PARAMETERS.monoGuitarSynth];
    monoGuitarSynthControls[0].value = "3";
    monoGuitarSynthControls[1].value = "0.4";
    monoGuitarSynthControls[3].value = "-36";
    monoGuitarSynthControls[6].value = "24";
    monoGuitarSynthControls[6].dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.monoGuitarSynth[0] !== 3 ||
        DEFAULT_PARAMETERS.monoGuitarSynth[1] !== 0.4 ||
        DEFAULT_PARAMETERS.monoGuitarSynth[3] !== -36 ||
        DEFAULT_PARAMETERS.monoGuitarSynth[6] !== 24 ||
        monoGuitarSynthControls[0].parentElement.querySelector("output").value !== "Square" ||
        monoGuitarSynthControls[6].parentElement.querySelector("output").value !== "24 ms") {
      throw new Error("Mono Guitar Synth controls must update waveform, mix, gate, portamento, and displayed values.");
    }
    defaultMonoGuitarSynth.forEach((value, index) => {
      monoGuitarSynthControls[index].value = String(value);
    });
    updateMonoGuitarSynth();
    const defaultNoteGenerator = [...DEFAULT_PARAMETERS.noteGenerator];
    noteGeneratorControls[0].value = "2";
    noteGeneratorControls[1].value = "3";
    noteGeneratorControls[2].value = "57";
    noteGeneratorControls[3].value = "-1";
    noteGeneratorControls[4].value = "0.7";
    noteGeneratorControls[13].value = "0.8";
    noteGeneratorControls[13].dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.noteGenerator[0] !== 2 ||
        DEFAULT_PARAMETERS.noteGenerator[1] !== 3 ||
        DEFAULT_PARAMETERS.noteGenerator[2] !== 57 ||
        DEFAULT_PARAMETERS.noteGenerator[3] !== -1 ||
        DEFAULT_PARAMETERS.noteGenerator[4] !== 0.7 ||
        DEFAULT_PARAMETERS.noteGenerator[13] !== 0.8 ||
        noteGeneratorControls[1].parentElement.querySelector("output").value !== "Square" ||
        noteGeneratorControls[2].parentElement.querySelector("output").value !== "MIDI 57" ||
        noteGeneratorControls[3].parentElement.querySelector("output").value !== "−1 octave" ||
        noteGeneratorControls[13].parentElement.querySelector("output").value !== "80%") {
      throw new Error("Note Generator controls must update voice count, oscillator pitch, waveform, levels, and displayed values.");
    }
    defaultNoteGenerator.forEach((value, index) => {
      noteGeneratorControls[index].value = String(value);
    });
    updateNoteGenerator();
    const defaultFourOscGenerator = [...DEFAULT_PARAMETERS.fourOscGenerator];
    fourOscGeneratorControls[0].value = "3";
    fourOscGeneratorControls[1].value = "57";
    fourOscGeneratorControls[2].value = "-1";
    fourOscGeneratorControls[3].value = "0.7";
    fourOscGeneratorControls[16].value = "0.8";
    fourOscGeneratorControls[16].dispatchEvent(new Event("input", { bubbles: true }));
    if (fourOscGeneratorControls.length !== 17 ||
        DEFAULT_PARAMETERS.fourOscGenerator[0] !== 3 ||
        DEFAULT_PARAMETERS.fourOscGenerator[1] !== 57 ||
        DEFAULT_PARAMETERS.fourOscGenerator[2] !== -1 ||
        DEFAULT_PARAMETERS.fourOscGenerator[3] !== 0.7 ||
        DEFAULT_PARAMETERS.fourOscGenerator[16] !== 0.8 ||
        fourOscGeneratorControls[0].parentElement.querySelector("output").value !== "Square" ||
        fourOscGeneratorControls[1].parentElement.querySelector("output").value !== "MIDI 57" ||
        fourOscGeneratorControls[2].parentElement.querySelector("output").value !== "−1 octave" ||
        fourOscGeneratorControls[16].parentElement.querySelector("output").value !== "80%") {
      throw new Error("4 OSC Generator controls must update waveform, pitch, octave, levels, and displayed values.");
    }
    defaultFourOscGenerator.forEach((value, index) => {
      fourOscGeneratorControls[index].value = String(value);
    });
    updateFourOscGenerator();
    const defaultGrainDelay = [...DEFAULT_PARAMETERS.grainDelay];
    [grainDelayControls[0].value, grainDelayControls[1].value,
      grainDelayControls[2].value, grainDelayControls[3].value,
      grainDelayControls[4].value, grainDelayControls[5].value,
      grainDelayControls[6].value] = ["400", "0.3", "0.5", "100", "3", "12", "20"];
    grainDelayControls[0].dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.grainDelay.join(",") !== "400,0.3,0.5,100,3,12,20" ||
        grainDelayControls[5].parentElement.querySelector("output").value !== "+12 semitones") {
      throw new Error("Grain Delay controls must update project parameters and display values.");
    }
    defaultGrainDelay.forEach((value, index) => {
      grainDelayControls[index].value = String(value);
    });
    updateGrainDelay();
    const defaultPitchFeedbackDelay = [...DEFAULT_PARAMETERS.pitchFeedbackDelay];
    [pitchFeedbackDelayControls[0].value, pitchFeedbackDelayControls[1].value,
      pitchFeedbackDelayControls[2].value, pitchFeedbackDelayControls[3].value] =
      ["500", "0.6", "0.45", "4.5"];
    pitchFeedbackDelayControls[0].dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.pitchFeedbackDelay.join(",") !== "500,0.6,0.45,4.5" ||
        pitchFeedbackDelayControls[3].parentElement.querySelector("output").value !== "+4.5 semitones") {
      throw new Error("Pitch Feedback Delay controls must update repeat timing, feedback, mix, pitch, and displays.");
    }
    defaultPitchFeedbackDelay.forEach((value, index) => {
      pitchFeedbackDelayControls[index].value = String(value);
    });
    updatePitchFeedbackDelay();
    const defaultFeedbackGenerator = [...DEFAULT_PARAMETERS.feedbackGenerator];
    feedbackGeneratorControls[0].value = "2";
    feedbackGeneratorControls[1].value = "0.65";
    feedbackGeneratorControls[2].value = "25";
    feedbackGeneratorControls[3].value = "750";
    feedbackGeneratorControls[4].value = "1";
    feedbackGeneratorControls[5].value = "0.8";
    feedbackGeneratorControls[6].value = "-36";
    feedbackGeneratorControls[6].dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.feedbackGenerator.join(",") !== "2,0.65,25,750,1,0.8,-36" ||
        feedbackGeneratorControls[0].parentElement.querySelector("output").value !== "Octave up" ||
        feedbackGeneratorControls[4].parentElement.querySelector("output").value !== "On" ||
        feedbackGeneratorControls[6].parentElement.querySelector("output").value !== "-36 dBFS") {
      throw new Error("Feedback Generator controls must update project parameters and displayed values.");
    }
    defaultFeedbackGenerator.forEach((value, index) => {
      feedbackGeneratorControls[index].value = String(value);
    });
    updateFeedbackGenerator();
    const defaultShimmerReverb = [...DEFAULT_PARAMETERS.shimmerReverb];
    [shimmerReverbControls[0].value, shimmerReverbControls[1].value,
      shimmerReverbControls[2].value, shimmerReverbControls[3].value,
      shimmerReverbControls[4].value, shimmerReverbControls[5].value,
      shimmerReverbControls[6].value, shimmerReverbControls[7].value,
      shimmerReverbControls[8].value] = ["4.5", "7200", "72", "40", "160",
        "7.5", "65", "38", "-3"];
    shimmerReverbControls[0].dispatchEvent(new Event("input", { bubbles: true }));
    if (DEFAULT_PARAMETERS.shimmerReverb.join(",") !==
          "4.5,7200,0.72,0.4,160,7.5,0.65,0.38,-3" ||
        shimmerReverbControls[5].parentElement.querySelector("output").value !== "+7.5 semitones" ||
        shimmerReverbControls[3].parentElement.querySelector("output").value !== "40%") {
      throw new Error("Shimmer Reverb controls must update project parameters and displays.");
    }
    defaultShimmerReverb.forEach((value, index) => {
      shimmerReverbControls[index].value = String(index === 2 || index === 3 ||
        index === 6 || index === 7 ? value * 100 : value);
    });
    updateShimmerReverb();
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
    const [inputBytes, referenceBytes, tunerBytes, convolutionIRBytes, cabinetIRBytes] = await Promise.all([
      inputResponse.arrayBuffer(),
      referenceResponse.arrayBuffer(),
      fetch("/__test_tuner.wav").then((response) => {
        if (!response.ok) throw new Error("Browser tuner fixture was not served.");
        return response.arrayBuffer();
      }),
      fetch("/__test_convolution_ir.wav").then((response) => {
        if (!response.ok) throw new Error("Browser convolution IR fixture was not served.");
        return response.arrayBuffer();
      }),
      fetch("/__test_cabinet_ir.wav").then((response) => {
        if (!response.ok) throw new Error("Browser Cabinet IR fixture was not served.");
        return response.arrayBuffer();
      }),
    ]);
    const reference = readFloatWave(referenceBytes);
    const captureBuffer = new SharedArrayBuffer(reference.samples.length * Float32Array.BYTES_PER_ELEMENT);
    const captureControl = new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 2);
    const captureView = new Int32Array(captureControl);
    const parityEnabled = Array.from({ length: EFFECT_NAMES.length }, () => true);
    parityEnabled[108] = false;
    // Poly Detune is covered by a separate focused browser smoke.
    parityEnabled[109] = false;
       // Project-authored Vibe, Multi-Delay, Porta Preamp, Pitch Sequence, Rhythm Tap, Plex Delay, Celestial/Cloud Reverb, and Fractal synth have separate browser playback smokes.
    parityEnabled[71] = false;
    parityEnabled[72] = false;
     parityEnabled[73] = false;
     parityEnabled[74] = false;
     parityEnabled[75] = false;
      parityEnabled[76] = false;
     parityEnabled[77] = false;
     parityEnabled[78] = false;
    parityEnabled[79] = false;
    // The project-authored Three-Stage Distortion has no offline CLI parity path.
    parityEnabled[80] = false;
    // Manual Through-Zero Flanger has a separate CLI/playback smoke.
    parityEnabled[81] = false;
    // Octobot has a separate browser playback smoke and no matching offline stage.
    parityEnabled[82] = false;
     // AM Ring Mod has a dedicated playback smoke; keep the shared offline parity chain fixed.
     parityEnabled[83] = false;
     // Tape Echo has a separate Play smoke; keep the shared offline comparison unchanged.
     parityEnabled[84] = false;
      // Octave Fuzz has its own local Play smoke and stays out of shared parity.
      parityEnabled[85] = false;
     // Asheville Pattrn has a separate isolated prerecorded-audio regression.
     parityEnabled[87] = false;
      // Rock Box Dist has its own local Play smoke and native unit regression.
      parityEnabled[88] = false;
       // Rock Box Boost also has a separate local Play smoke.
       parityEnabled[89] = false;
    // Stereo Panner is verified by its dedicated local stereo guitar smoke.
    parityEnabled[90] = false;
    // Poly Sustain and Step Filter Delay have no offline CLI reference path yet.
    parityEnabled[91] = false;
    parityEnabled[92] = false;
     // Volume/Pan has its own browser Play smoke; it is not in the offline CLI reference chain.
     parityEnabled[93] = false;
      // Stereo Filter has its own browser Play smoke; it is not in the offline CLI reference chain.
      parityEnabled[96] = false;
    // Chromatic Pitch Delay has its dedicated local guitar Play smoke; it is
    // not part of the offline CLI reference chain.
    parityEnabled[99] = false;
    // Cabinet IR has a focused browser/WASM Play smoke and no shared offline CLI stage.
    parityEnabled[100] = false;
    // Octave Divider is verified by its dedicated local guitar Play smoke and
    // focused CLI test, so keep it out of the shared offline parity chain.
    parityEnabled[101] = false;
    // Bias Trem has a native CLI reference and participates in full-chain parity.
    parityEnabled[105] = true;
    // Vibratone has a native CLI reference stage as well.
    parityEnabled[106] = true;
    // Uni-Verse Vibe has a focused browser Play test, but no offline CLI stage.
    parityEnabled[107] = false;
    const activeProcessorIndices = parityEnabled
        .map((enabled, index) => enabled && index > 0 && index !== 94 && index !== 95
        ? index : -1)
      .filter((index) => index >= 0);
    if (parityEnabled.length !== 110 || activeProcessorIndices.length !== 78 ||
            activeProcessorIndices[0] !== 1 || activeProcessorIndices.at(-1) !== 106) {
             throw new Error("The all-effect chain must contain 78 optional processors between input trim and output utilities.");
    }
    // The free-running oscillator has an independent phase clock; exercise it
    // with a frequency check below instead of phase-sensitive native parity.
    parityEnabled[53] = false;
     // Megatap's UI/WASM path is tested separately; the offline CLI reference
     // does not implement this project-authored effect.
     parityEnabled[68] = false;
     // The project-authored De-Esser also has no offline CLI reference stage.
     parityEnabled[69] = false;
    // The project-authored Dual-Engine Distortion has no offline CLI reference stage.
    parityEnabled[70] = false;
    const testOptions = {
      enabled: parityEnabled,
      inputMuted: false,
      globalBypass: false,
      panicMuted: false,
      parameters: {
        inputGainDb: 2,
        outputGainDb: -1,
        stereoCrossover: { ...DEFAULT_PARAMETERS.stereoCrossover },
        doubleTake: [3, 0.65, 0.55, 0.75, 0.45, 0],
        eq5Graphic: [2, -3, 6, -3, 2, 0],
        opticalTremolo: [1, 5.2, 0.65, 4, 0, 120, 1, 35, 58, 18, 10, 0, -36, 25, 0.75, -1, 0, 0],
        fractalBiasTrem: [3.7, 0.75, 1, 0, 120, 1, 0, -36, 0, 0.7, 1, 0, 0, 0],
        vibratone: [0, 0.8, 0, 6.5, 0.85, 0, 650, 0.5, 0],
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
        harmonicTremolo: [4.25, 0.8, 950, 1, 1, 96, 0.5, 0, 25],
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
         ringModulator: [0, 713, 1.75, 9000, 0.83],
        rhythmicSlicer: [1, 120, 8, 8, 0.4, 0.15, 0.5, 1, 0,
          1, 0.25, 1, 0.25, 1, 0.25, 1, 0.25,
          1, 0.25, 1, 0.25, 1, 0.25, 1, 0.25],
         stepFilter: [1, 120, 8, 8, 0, 120, 6000, 0.707, 0.65, 0.4, 3, 0.7, 0,
           0, 0.75, 0.2, 0.9, 0.35, 0.6, 0.1, 1,
           0.25, 0.8, 0.15, 0.95, 0.4, 0.7, 0.05, 0.85],
        notchFilter: [713, 6, 0.72],
        formantFilter: [1.5, 6, 0.72, -2, 1, 0, 2, 4, 65, 25],
        harmonicResonator: [220, 0.8, 0.7, 0.65],
        multibandCompressor: [500, 4000, 0,
          -30, 4, 0, 1, 30, 6, 0, 0,
          -24, 2, 6, 10, 100, 0, 1, 0,
          -18, 3, 3, 5, 250, -2, 1, 0],
         bitCrusher: [8, 4, 0.65],
        dimensionChorus: [0, 0, 0.5, 0],
        tripleDoubleChorus: [0, 0, 0.8, 7, 5, 8, 0.1, 0.2, 1],
        fourBandResonator: [1, 2, 1.1, 220, 330, 440, 660, 220,
          0.65, 0.55, 0.45, 0.35, 2, 2.5, 3, 3.5, 0.8, 0.7, 0.6, 0.5,
          -0.75, -0.25, 0.25, 0.75, 0, 1, 0, 1, 0, 0.9, -0.25, 0.75, 1.2, 0.65],
        diatonicHarmonizer: [0.7, 2, 7, 0, 8000, 120, 0.2, 0, 120],
        grainDelay: [350, 0.25, 0.35, 80, 2, 7, 35],
         pitchFeedbackDelay: [350, 0.35, 0.25, 7],
         feedbackGenerator: [1, 0.35, 15, 600, 0, 1, -48],
         shimmerReverb: [3, 6500, 0.85, 0.25, 80, 12, 1, 0.25, 0],
         convolutionReverb: [0.35, -3, 96.25, 8650, 1],
         fiveBandEq: [100, 1.5, 0.7, 1, 12, 350, -0.5, 1, 0, 12,
           1000, 1, 0.7, 0, 12, 3500, -0.5, 0.8, 0, 12,
           8000, 0.5, 0.7, 4, 24],
          reverseDelay: [350, 0.25, 0.35, 10, 0, 4, 120, 0, 0, 0, 0],
         reverseDelayRun: true,
  duckingDelay: [350, 0.25, 0.25, -30, 12, 250, 0, 0, 120, 4, 0, 0, 0.8, 0, 0],
        sweepDelay: [350, 0.25, 0.25, 250, 6000, 1.2, 0.5, 0, 90, 1, 0, 120, 4],
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
    testOptions.parameters.grainDelay.forEach((value, index) => {
      grainDelayControls[index].value = String(value);
    });
    updateGrainDelay();
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
    formantFilterMode.value = String(formantFilterParameters[4]);
    formantFilterStartVowel.value = String(formantFilterParameters[5]);
    formantFilterMiddleVowel.value = String(formantFilterParameters[6]);
    formantFilterEndVowel.value = String(formantFilterParameters[7]);
    formantFilterControl.value = String(formantFilterParameters[8]);
    formantFilterBalance.value = String(formantFilterParameters[9]);
    updateFormantFilter();
    if (DEFAULT_PARAMETERS.formantFilter[0] !== 1.5 ||
        DEFAULT_PARAMETERS.formantFilter[4] !== 1 ||
        formantFilterVowelValue.value !== "AAA → III → OOO" ||
        formantFilterControlValue.value !== "65%" ||
        DEFAULT_PARAMETERS.formantFilter[1] !== 6 ||
        DEFAULT_PARAMETERS.formantFilter[2] !== 0.72 ||
        DEFAULT_PARAMETERS.formantFilter[3] !== -2 ||
        DEFAULT_PARAMETERS.formantFilter[9] !== 25) {
      throw new Error("Formant Filter controls must update morph vowels, Control, Resonance, mix, level, and Balance.");
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
    duckingDelaySpillover.checked = Boolean(duckingDelayParameters[6] ?? 0);
    duckingDelayTimeMode.value = String(duckingDelayParameters[7] ?? 0);
    duckingDelayTempo.value = String(duckingDelayParameters[8] ?? 120);
    duckingDelayTapDivision.value = String(duckingDelayParameters[9] ?? 4);
    duckingDelayLowCut.value = String(duckingDelayParameters[10] ?? 0);
    duckingDelayHighCut.value = String(duckingDelayParameters[11] ?? 0);
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
    [shimmerReverbControls[0].value, shimmerReverbControls[1].value,
      shimmerReverbControls[2].value, shimmerReverbControls[3].value,
      shimmerReverbControls[4].value, shimmerReverbControls[5].value,
      shimmerReverbControls[6].value, shimmerReverbControls[7].value,
      shimmerReverbControls[8].value] = ["3.4", "7200", "72", "35", "120",
        "7", "65", "30", "-3"];
    updateShimmerReverb();
     convolutionReverbControls[0].value = "35";
     convolutionReverbControls[1].value = "-3";
     updateConvolutionReverb();
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
    delayCharacter.value = "bbd-mod";
    updateDelayCharacter();
    if (!DEFAULT_PARAMETERS.delayBbd || !DEFAULT_PARAMETERS.delayBbdMode ||
        DEFAULT_PARAMETERS.delayTape[0] !== 0 ||
        !delayTapeControls.hidden) {
      throw new Error("The BBD MOD mode did not update categorized delay state.");
    }
    delayCharacter.value = "digital";
    updateDelayCharacter();
    if (DEFAULT_PARAMETERS.delayBbd || DEFAULT_PARAMETERS.delayBbdMode ||
        DEFAULT_PARAMETERS.delayTape[0] !== 0) {
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
    updateDimensionChorus();
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
     await loadConvolutionIRFile(new File(
       [convolutionIRBytes], "browser-convolution-ir.wav", { type: "audio/wav" },
     ));
     await prepareProcessor(testOptions);
     await ensureConvolutionIRLoaded();
        if (new URLSearchParams(location.search).get("selftest") === "uni-verse-vibe") {
          testResult.textContent = "Checking Uni-Verse Vibe browser Play with the local guitar fixture…";
          const uniVerseVibeSmoke = await runUniVerseVibeBrowserSmokeTest(
            reference, captureView, captureBuffer,
          );
          const focusedReport = {
            result: "passed",
            browser: navigator.userAgent,
            sampleRate: audioContext.sampleRate,
            fixtureFrames: reference.frames,
            ...uniVerseVibeSmoke,
          };
          testResult.dataset.status = "passed";
          testResult.textContent = JSON.stringify(focusedReport);
          document.title = "PASS · TabletFX Uni-Verse Vibe browser test";
          return;
        }
        if (new URLSearchParams(location.search).get("selftest") === "four-osc-generator") {
          testResult.textContent = "Checking 4 OSC Generator browser Play with the local guitar fixture…";
          const fourOscSmoke = await runFourOscGeneratorBrowserSmokeTest(
            reference, captureView, captureBuffer,
          );
          const focusedReport = {
            result: "passed",
            browser: navigator.userAgent,
            sampleRate: audioContext.sampleRate,
            channels: reference.channels,
            frames: reference.frames,
            fourOscGenerator: fourOscSmoke,
          };
          testResult.dataset.status = "passed";
          testResult.textContent = JSON.stringify(focusedReport);
          document.title = "PASS · TabletFX 4 OSC Generator browser test";
          return;
        }
        if (new URLSearchParams(location.search).get("selftest") === "poly-detune") {
          testResult.textContent = "Checking Poly Detune browser Play with the local guitar fixture…";
          const polyDetuneSmoke = await runPolyDetuneBrowserSmokeTest(
            reference, captureView, captureBuffer,
          );
          const focusedReport = {
            result: "passed",
            browser: navigator.userAgent,
            sampleRate: audioContext.sampleRate,
            channels: reference.channels,
            frames: reference.frames,
            polyDetune: polyDetuneSmoke,
          };
          testResult.dataset.status = "passed";
          testResult.textContent = JSON.stringify(focusedReport);
          document.title = "PASS · TabletFX Poly Detune browser test";
          return;
        }
        if (new URLSearchParams(location.search).get("selftest") === "fractal-poly-pitch-range") {
          testResult.textContent = "Checking the FM9-family ±24 polyphonic pitch range with local guitar audio…";
          const pitchRangeSmoke = await runFractalPolyPitchRangeBrowserSmokeTest(
            reference, captureView, captureBuffer,
          );
          const focusedReport = {
            result: "passed",
            browser: navigator.userAgent,
            sampleRate: audioContext.sampleRate,
            channels: reference.channels,
            frames: reference.frames,
            polyPitchRange: pitchRangeSmoke,
          };
          testResult.dataset.status = "passed";
          testResult.textContent = JSON.stringify(focusedReport);
          document.title = "PASS · TabletFX poly-pitch range browser test";
          return;
        }
        if (new URLSearchParams(location.search).get("selftest") === "fender-pedal-shifter") {
          testResult.textContent = "Checking Pedal Shifter POSITION sweep with local guitar audio…";
          const pedalShifterSmoke = await runFenderPedalShifterBrowserSmokeTest(
            reference, captureView, captureBuffer,
          );
          const focusedReport = {
            result: "passed",
            browser: navigator.userAgent,
            sampleRate: audioContext.sampleRate,
            channels: reference.channels,
            frames: reference.frames,
            pedalShifter: pedalShifterSmoke,
          };
          testResult.dataset.status = "passed";
          testResult.textContent = JSON.stringify(focusedReport);
          document.title = "PASS · TabletFX Pedal Shifter browser test";
          return;
        }
        if (new URLSearchParams(location.search).get("selftest") === "fractal-diatonic-custom-scale") {
          testResult.textContent = "Checking the custom-scale harmonizer browser Play with local guitar audio…";
          const customScaleSmoke = await runFractalDiatonicCustomScaleBrowserSmokeTest(
            reference, captureView, captureBuffer,
          );
          const focusedReport = {
            result: "passed",
            browser: navigator.userAgent,
            sampleRate: audioContext.sampleRate,
            channels: reference.channels,
            frames: reference.frames,
            diatonicCustomScale: customScaleSmoke,
          };
          testResult.dataset.status = "passed";
          testResult.textContent = JSON.stringify(focusedReport);
          document.title = "PASS · TabletFX custom-scale browser test";
          return;
        }
        if (new URLSearchParams(location.search).get("selftest") === "fractal-bias-trem") {
          testResult.textContent = "Checking Bias Trem browser Play with the local guitar fixture…";
          const fractalBiasTremSmoke = await runFractalBiasTremBrowserSmokeTest(
            reference, captureView, captureBuffer,
          );
          const focusedReport = {
            result: "passed",
            browser: navigator.userAgent,
            sampleRate: audioContext.sampleRate,
            channels: reference.channels,
            frames: reference.frames,
            fractalBiasTrem: fractalBiasTremSmoke,
          };
          testResult.dataset.status = "passed";
          testResult.textContent = JSON.stringify(focusedReport);
          document.title = "PASS · TabletFX Bias Trem browser test";
          return;
        }
        if (new URLSearchParams(location.search).get("selftest") === "fender-vibratone") {
          testResult.textContent = "Checking Vibratone browser Play with the local guitar fixture…";
          const vibratoneSmoke = await runVibratoneBrowserSmokeTest(
            reference, captureView, captureBuffer,
          );
          const focusedReport = {
            result: "passed",
            browser: navigator.userAgent,
            sampleRate: audioContext.sampleRate,
            channels: reference.channels,
            frames: reference.frames,
            vibratone: vibratoneSmoke,
          };
          testResult.dataset.status = "passed";
          testResult.textContent = JSON.stringify(focusedReport);
          document.title = "PASS · TabletFX Vibratone browser test";
          return;
        }
        if (new URLSearchParams(location.search).get("selftest") === "step-tremolo") {
         testResult.textContent = "Checking Step Tremolo browser Play with the local guitar fixture…";
         const stepTremoloSmoke = await runStepTremoloBrowserSmokeTest(
           reference, captureView, captureBuffer,
         );
         const focusedReport = {
           result: "passed",
           browser: navigator.userAgent,
           sampleRate: audioContext.sampleRate,
           channels: reference.channels,
           frames: reference.frames,
           stepTremolo: stepTremoloSmoke,
         };
         testResult.dataset.status = "passed";
         testResult.textContent = JSON.stringify(focusedReport);
         document.title = "PASS · TabletFX Step Tremolo browser test";
         return;
       }
       if (new URLSearchParams(location.search).get("selftest") === "stereo-filter") {
        testResult.textContent = "Checking Stereo Filter browser Play with the local guitar fixture…";
        const stereoFilterSmoke = await runStereoFilterBrowserSmokeTest(
          reference, captureView, captureBuffer,
        );
        const focusedReport = {
          result: "passed",
          browser: navigator.userAgent,
          sampleRate: audioContext.sampleRate,
          channels: reference.channels,
          frames: reference.frames,
          stereoFilter: stereoFilterSmoke,
        };
        testResult.dataset.status = "passed";
        testResult.textContent = JSON.stringify(focusedReport);
        document.title = "PASS · TabletFX Stereo Filter browser test";
        return;
      }
      if (new URLSearchParams(location.search).get("selftest") === "volume-pan") {
       testResult.textContent = "Checking Volume/Pan browser Play with the local guitar fixture…";
       const volumePanSmoke = await runVolumePanBrowserSmokeTest(
         reference, captureView, captureBuffer,
       );
       const focusedReport = {
         result: "passed",
         browser: navigator.userAgent,
         sampleRate: audioContext.sampleRate,
         channels: reference.channels,
         frames: reference.frames,
         volumePan: volumePanSmoke,
       };
       testResult.dataset.status = "passed";
       testResult.textContent = JSON.stringify(focusedReport);
       document.title = "PASS · TabletFX Volume/Pan browser test";
       return;
     }
     if (convolutionIRUploadedVersion !== convolutionIRVersion) {
       throw new Error("The local convolution IR was not acknowledged by the WASM audio processor.");
     }
    testResult.textContent = "Playing the processed chain and capturing its output…";
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error(`Timed out starting browser playback at: ${testResult.textContent}`)),
        5000,
      )),
    ]);
    looperStatus.value = "Waiting for recording command";
    document.querySelector('[data-looper-command="0"]').click();
    await waitForLooperStatus("Recording · no loop");
    await waitForCapture(captureView, reference.samples.length);
    document.querySelector('[data-looper-command="3"]').click();
    await waitForLooperStatus("Stopped · ");
    if (looperStatus.value.includes("no loop")) {
      throw new Error("Browser Looper recording did not retain processed input audio.");
    }
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
    let actualSquares = 0;
    let referencePeak = 0;
    for (let i = 0; i < actual.length; i += 1) {
      const expected = reference.samples[i];
      const error = Math.abs(actual[i] - expected);
      if (error > maximumError) {
        maximumError = error;
        maximumErrorIndex = i;
      }
      errorSquares += error * error;
      referenceSquares += expected * expected;
      actualSquares += actual[i] * actual[i];
      referencePeak = Math.max(referencePeak, Math.abs(expected));
    }
    const rmsError = Math.sqrt(errorSquares / Math.max(1, actual.length));
    const referenceRms = Math.sqrt(referenceSquares / Math.max(1, actual.length));
    const actualRms = Math.sqrt(actualSquares / Math.max(1, actual.length));
    const maxAbsTolerance = Math.max(0.0002, referencePeak * 0.06);
    const rmsTolerance = Math.max(0.00002, referenceRms * 0.075);
    const minimumSignalRms = Math.max(1e-12, referenceRms * 0.01);
    if (maximumError > maxAbsTolerance || rmsError > rmsTolerance ||
        actualRms < minimumSignalRms) {
      const firstSamples = Array.from({ length: 8 }, (_, index) =>
        `${index}:${actual[index].toFixed(5)}/${reference.samples[index].toFixed(5)}`).join(", ");
      let expectedSquares = 0;
      for (let i = 0; i < actual.length; i += 1) {
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
      throw new Error(`Browser/native audio mismatch: max error ${maximumError} at sample ${maximumErrorIndex} (${actual[maximumErrorIndex].toFixed(5)}/${reference.samples[maximumErrorIndex].toFixed(5)}), RMS error ${rmsError}, signal RMS browser/native ${actualRms}/${Math.sqrt(expectedSquares / actual.length)}, minimum browser signal RMS ${minimumSignalRms}, best offset ${bestLag} samples (RMS ${Math.sqrt(bestLagError)}); first samples browser/native ${firstSamples}; decoded/context rates ${decodedAudio.sampleRate}/${audioContext.sampleRate} Hz; user agent ${navigator.userAgent}.`);
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
    for (let start = inputChannels[0].length - meters.frames; start >= 0; start -= 1) {
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
    const outputEnd = outputStart + lastBlockSamples;
    for (let i = outputStart; i < outputEnd; i += 1) {
      const magnitude = Math.abs(actual[i]);
      measuredOutputPeak = Math.max(measuredOutputPeak, magnitude);
      measuredOutputSquares += actual[i] * actual[i];
    }
    const measuredOutputRms = Math.sqrt(measuredOutputSquares / Math.max(1, lastBlockSamples));
    if (Math.abs(meters.output.peak - measuredOutputPeak) > 1e-6 ||
        Math.abs(meters.output.rms - measuredOutputRms) > 1e-6) {
      throw new Error("Output meter does not match the final processed audio block.");
    }
    testResult.textContent = "Checking Cabinet IR format, loading, memory, and playback on local guitar audio…";
    const cabinetIRSmoke = await runCabinetIrBrowserSmokeTest(
      reference, captureView, captureBuffer, cabinetIRBytes,
    );
    testResult.textContent = "Checking Octave Divider Pitch-category playback on local guitar audio…";
    const octaveDividerSmoke = await runOctaveDividerBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Double Take Modulation-category playback on local guitar audio…";
    const doubleTakeSmoke = await runDoubleTakeBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking EQ5 Graphic Equalizer-category playback on local guitar audio…";
    const eq5GraphicSmoke = await runEQ5GraphicBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Optical Tremolo Modulation-category playback on local guitar audio…";
    const opticalTremoloSmoke = await runOpticalTremoloBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Bias Trem Modulation-category playback on local guitar audio…";
    const fractalBiasTremSmoke = await runFractalBiasTremBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Fender-inspired Vibratone Modulation playback on local guitar audio…";
    const vibratoneSmoke = await runVibratoneBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Megatap Delay playback on local guitar audio…";
    const megatapDelaySmoke = await runMegatapDelayBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking De-Esser playback on local guitar audio…";
    const deEsserSmoke = await runDeEsserBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Dual-Engine Distortion playback on local guitar audio…";
    const dualEngineDistortionSmoke = await runDualEngineDistortionBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Vibe playback on local guitar audio…";
    const vibeSmoke = await runVibeBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Multi-Delay playback on local guitar audio…";
    const multiDelaySmoke = await runMultiDelayBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Porta Preamp playback on local guitar audio…";
    const portaPreampSmoke = await runPortaPreampBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Pitch Sequence playback on local guitar audio…";
    const pitchSequenceSmoke = await runPitchSequenceBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Rhythm Tap playback on local guitar audio…";
    const rhythmTapDelaySmoke = await runRhythmTapBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Plex Matrix Delay playback on local guitar audio…";
    const plexDelaySmoke = await runPlexDelayBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Celestial Reverb playback on local guitar audio…";
    const celestialReverbSmoke = await runCelestialReverbBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Cloud Reverb playback on local guitar audio…";
    const cloudReverbSmoke = await runCloudReverbBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Three-Stage Distortion playback on local guitar audio…";
    const mercatorDistortionSmoke = await runMercatorDistortionBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Manual Through-Zero Flanger playback on local guitar audio…";
    const manualThruZeroFlangerSmoke = await runManualThruZeroFlangerBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
      testResult.textContent = "Checking project-authored Octobot playback on local guitar audio…";
      const octobotSmoke = await runOctobotBrowserSmokeTest(
        reference, captureView, captureBuffer,
      );
    testResult.textContent = "Checking AM Ring Mod playback on local guitar audio…";
    const amRingModSmoke = await runAmRingModBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
     testResult.textContent = "Checking Tape Echo playback on local guitar audio…";
     const tapeEchoSmoke = await runTapeEchoBrowserSmokeTest(
       reference, captureView, captureBuffer,
     );
      testResult.textContent = "Checking Octave Fuzz playback on local guitar audio…";
      const octaveFuzzSmoke = await runOctaveFuzzBrowserSmokeTest(
        reference, captureView, captureBuffer,
      );
    testResult.textContent = "Checking Rock Box Dist playback on local guitar audio…";
    const rockBoxDistSmoke = await runRockBoxDistBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Rock Box Boost playback on local guitar audio…";
    const rockBoxBoostSmoke = await runRockBoxBoostBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Stereo Panner playback on local stereo guitar audio…";
    const stereoPannerSmoke = await runStereoPannerBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Stereo Crossover playback on local guitar audio…";
    const stereoCrossoverSmoke = await runStereoCrossoverBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Asheville Pattrn WASM playback on local guitar audio…";
    const ashevillePattrnSmoke = await runAshevillePattrnBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Sweep Delay WASM playback on local guitar audio…";
    const sweepDelaySmoke = await runSweepDelayBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Ring Modulator Classic and SSB modes on local guitar audio…";
    const ringModulatorSmoke = await runRingModulatorBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking tracked Synth playback on local guitar audio…";
    const fractalThreeVoiceSynthSmoke =
      await runFractalThreeVoiceSynthBrowserSmokeTest(
        reference, captureView, captureBuffer,
      );
    testResult.textContent = "Checking Shuffling Looper WASM transport and loop output…";
    const shufflingLooperSmoke = await runShufflingLooperBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Phrase Looper threshold-triggered recording…";
    const phraseLooperAutoRecordSmoke =
      await runPhraseLooperAutoRecordBrowserSmokeTest();
    testResult.textContent = "Checking Deluxe Looper record, playback, and clear on local guitar audio…";
    const deluxeLooperSmoke = await runDeluxeLooperBrowserSmokeTest();
    testResult.textContent = "Checking Step Filter Delay playback on local guitar audio…";
    const stepFilterDelaySmoke = await runStepFilterDelayBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Ducking Delay tempo sync, repeat cuts, and modulation on local guitar audio…";
    const duckingDelaySmoke = await runDuckingDelayBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Chromatic Pitch Delay playback on local guitar audio…";
    const chromaticPitchDelaySmoke =
      await runChromaticPitchDelayBrowserSmokeTest(
        reference, captureView, captureBuffer,
      );
    testResult.textContent = "Checking Volume/Pan playback on local guitar audio…";
    const volumePanSmoke = await runVolumePanBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking Step Tremolo playback on local guitar audio…";
    const stepTremoloSmoke = await runStepTremoloBrowserSmokeTest(
      reference, captureView, captureBuffer,
    );
    testResult.textContent = "Checking the free-running browser note generator…";
    stopSource(false);
    for (let index = 0; index < EFFECT_NAMES.length; index += 1) {
      workletNode.port.postMessage({
        type: "enable",
        index,
        value: index === 53,
      });
    }
    workletNode.port.postMessage({
      type: "note-generator",
      value: [1, 0, 69, 0, 0.8, 0, 60, 0, 0, 0, 60, 0, 0, 0.8],
    });
    workletNode.port.postMessage({ type: "reset" });
    workletNode.port.postMessage({ type: "capture-reset" });
    Atomics.store(captureView, 0, 0);
    Atomics.store(captureView, 1, 0);
    playbackOffset = 0;
    playButton.click();
    await Promise.race([
      playTask,
      new Promise((_, reject) => setTimeout(
        () => reject(new Error("Timed out starting note-generator playback.")), 5000,
      )),
    ]);
    await waitForCapture(captureView, reference.samples.length);
    stopSource(false);
    const generatedNote = new Float32Array(captureBuffer);
    let generatedSquares = 0;
    let positiveCrossings = 0;
    for (let frame = 0; frame < reference.frames; frame += 1) {
      const sample = generatedNote[frame * reference.channels];
      generatedSquares += sample * sample;
      if (frame > 0 &&
          generatedNote[(frame - 1) * reference.channels] <= 0 && sample > 0) {
        positiveCrossings += 1;
      }
    }
    const generatedRms = Math.sqrt(generatedSquares / Math.max(1, reference.frames));
    const generatedFrequency = positiveCrossings * audioContext.sampleRate /
      Math.max(1, reference.frames);
    if (generatedRms < 0.3 || Math.abs(generatedFrequency - 440) > 8) {
      throw new Error(`Free-running Note Generator did not produce a browser A4 tone: ${JSON.stringify({ generatedRms, generatedFrequency, positiveCrossings, sampleRate: audioContext.sampleRate })}`);
    }
    testResult.textContent = "Checking Modern Enhancer mono-to-stereo playback on local guitar audio…";
    const monoEnhancerSmoke = await runMonoModernEnhancerBrowserSmokeTest(
      decodedAudio, testOptions,
    );
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
      allEffectProcessorCount: activeProcessorIndices.length,
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
      convolutionIRLoaded: convolutionIRUploadedVersion === convolutionIRVersion,
      cabinetIR: cabinetIRSmoke,
      octaveDivider: octaveDividerSmoke,
      doubleTake: doubleTakeSmoke,
      eq5Graphic: eq5GraphicSmoke,
      opticalTremolo: opticalTremoloSmoke,
      fractalBiasTrem: fractalBiasTremSmoke,
      vibratone: vibratoneSmoke,
      playOutputCaptured: actualRms >= minimumSignalRms,
      megatapDelay: megatapDelaySmoke,
      deEsser: deEsserSmoke,
      dualEngineDistortion: dualEngineDistortionSmoke,
      vibe: vibeSmoke,
      multiDelay: multiDelaySmoke,
      portaPreamp: portaPreampSmoke,
      pitchSequence: pitchSequenceSmoke,
      rhythmTapDelay: rhythmTapDelaySmoke,
      plexDelay: plexDelaySmoke,
      celestialReverb: celestialReverbSmoke,
      cloudReverb: cloudReverbSmoke,
      threeStageDistortion: mercatorDistortionSmoke,
      manualThruZeroFlanger: manualThruZeroFlangerSmoke,
       octobot: octobotSmoke,
       amRingMod: amRingModSmoke,
         tapeEcho: tapeEchoSmoke,
         octaveFuzz: octaveFuzzSmoke,
      rockBoxBoost: rockBoxBoostSmoke,
      stereoPanner: stereoPannerSmoke,
      stereoCrossover: stereoCrossoverSmoke,
      ashevillePattrn: ashevillePattrnSmoke,
      sweepDelay: sweepDelaySmoke,
      ringModulator: ringModulatorSmoke,
      fractalThreeVoiceSynth: fractalThreeVoiceSynthSmoke,
      shufflingLooper: shufflingLooperSmoke,
      phraseLooperAutoRecord: phraseLooperAutoRecordSmoke,
      deluxeLooper: deluxeLooperSmoke,
      stepFilterDelay: stepFilterDelaySmoke,
      duckingDelay: duckingDelaySmoke,
      chromaticPitchDelay: chromaticPitchDelaySmoke,
      volumePan: volumePanSmoke,
      stepTremolo: stepTremoloSmoke,
      noteGenerator: { frequencyHz: generatedFrequency, rms: generatedRms },
      tuner: tunerReading,
      tunerDetectionMs,
      tunerUi: tunerReadingOutput.textContent,
      monoModernEnhancer: monoEnhancerSmoke,
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
