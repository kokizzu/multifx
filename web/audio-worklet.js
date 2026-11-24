import createWasmModule from "../build/web/tabletfx_wasm.js";

const EFFECTS = [
  "input-trim", "volume-pedal", "high-pass", "low-pass", "noise-gate",
  "pedal-compressor", "compressor", "wah", "envelope-wah", "clean-boost",
  "transparent-overdrive",
  "mid-focused-overdrive", "high-gain-distortion", "round-fuzz", "soft-drive", "tremolo", "chorus", "flanger", "phaser", "vibrato", "three-band-eq", "delay", "poly-pitch-shifter",
  "optical-compressor", "fet-compressor", "guitar-graphic-eq", "bass-graphic-eq", "shelf-tilt-eq",
  "room-reverb", "hall-reverb", "plate-reverb", "spring-reverb", "auto-swell", "rotary-speaker", "ring-modulator", "rhythmic-slicer", "step-filter", "notch-filter", "reverse-delay", "ducking-delay", "modulated-delay", "filtered-delay", "sweep-delay", "formant-filter", "harmonic-resonator", "multiband-compressor", "bit-crusher", "dimension-chorus", "triple-double-chorus", "four-band-resonator", "diatonic-harmonizer", "pitch-arpeggiator", "mono-guitar-synth", "note-generator", "grain-delay", "pitch-feedback-delay", "feedback-generator", "shimmer-reverb", "convolution-reverb", "five-band-parametric-eq", "phrase-looper", "enhancer", "resonant-delay", "harmonic-tremolo", "auto-swell-delay", "shuffling-looper", "ten-tap-delay", "auto-pan", "megatap-delay", "de-esser", "dual-engine-distortion", "vibe", "multi-delay", "porta-preamp", "pitch-sequence", "rhythm-tap-delay", "plex-delay", "celestial-reverb", "fractal-three-voice-synth", "cloud-reverb", "three-stage-distortion", "manual-thru-zero-flanger", "octobot", "am-ring-mod", "tape-echo", "output-level", "peak-limiter",
];
EFFECTS.splice(EFFECTS.indexOf("output-level"), 0, "octave-fuzz");
EFFECTS.splice(EFFECTS.indexOf("output-level"), 0, "stereo-crossover");
EFFECTS.splice(EFFECTS.indexOf("output-level"), 0, "asheville-pattrn");
EFFECTS.splice(EFFECTS.indexOf("output-level"), 0, "rock-box-dist");
EFFECTS.splice(EFFECTS.indexOf("output-level"), 0, "rock-box-boost");
EFFECTS.splice(EFFECTS.indexOf("output-level"), 0, "stereo-panner");
EFFECTS.splice(EFFECTS.indexOf("output-level"), 0, "poly-sustain");
EFFECTS.splice(EFFECTS.indexOf("output-level"), 0, "step-filter-delay");
EFFECTS.splice(EFFECTS.indexOf("output-level"), 0, "volume-pan");
EFFECTS.push("stereo-filter");
EFFECTS.push("step-tremolo");
EFFECTS.push("deluxe-looper");
EFFECTS.push("chromatic-pitch-delay");
EFFECTS.push("cabinet-ir");
EFFECTS.push("octave-divider");
EFFECTS.push("double-take");
EFFECTS.push("eq5-graphic");
EFFECTS.push("optical-tremolo");
EFFECTS.push("fractal-bias-trem");
EFFECTS.push("vibratone");
const EFFECT_PIPELINE_EFFECT_COUNT = 96;
const FIRST_EXTRA_EFFECT_INDEX = 96;
const OUTPUT_LEVEL_INDEX = 94;
const PEAK_LIMITER_INDEX = 95;
const MAX_BLOCK_FRAMES = 8192;
const STATS_HEADER_WORDS = 2;
const STATS_RING_BLOCKS = 512;
const STATS_EFFECT_SLOTS = EFFECTS.length + 16;
const METER_INPUT_PEAK = EFFECTS.length + 2;
const METER_INPUT_RMS = EFFECTS.length + 3;
const METER_OUTPUT_PEAK = EFFECTS.length + 4;
const METER_OUTPUT_RMS = EFFECTS.length + 5;
const METER_FLAGS = EFFECTS.length + 6;
const METER_INPUT_INVALID = EFFECTS.length + 7;
const METER_OUTPUT_INVALID = EFFECTS.length + 8;
const TUNER_GENERATION = EFFECTS.length + 9;
const TUNER_NOTE = EFFECTS.length + 10;
const TUNER_VALID = EFFECTS.length + 11;
const TUNER_FREQUENCY = EFFECTS.length + 12;
const TUNER_CENTS = EFFECTS.length + 13;
const TUNER_CONFIDENCE = EFFECTS.length + 14;
const TUNER_TIME = EFFECTS.length + 15;

function configureMultiDelay(module, chain, parameters = {}) {
  const defaults = [
    { timeMs: 250, timeMode: 0, division: 4, feedback: 0, crossFeedback: 0, level: 1, pan: 0.1 },
    { timeMs: 375, timeMode: 0, division: 3, feedback: 0, crossFeedback: 0, level: 1, pan: 0.35 },
    { timeMs: 500, timeMode: 0, division: 2, feedback: 0, crossFeedback: 0, level: 1, pan: 0.65 },
    { timeMs: 750, timeMode: 0, division: 0, feedback: 0, crossFeedback: 0, level: 1, pan: 0.9 },
  ];
  const lines = Array.isArray(parameters.lines) ? parameters.lines : defaults;
  for (let index = 0; index < 4; index += 1) {
    const line = lines[index] || defaults[index];
    module._tf_chain_set_multi_delay_line(
      chain, index, Number(line.timeMs), Number(line.timeMode),
      Number(line.division), Number(line.feedback), Number(line.crossFeedback),
      Number(line.level), Number(line.pan),
    );
  }
  module._tf_chain_set_multi_delay_global(
    chain, Number(parameters.tempoBpm ?? 120), Number(parameters.lowCutHz ?? 20),
    Number(parameters.highCutHz ?? 20000), Number(parameters.inputGainDb ?? 0),
    Number(parameters.mix ?? 0.25), Number(parameters.levelDb ?? 0),
  );
  module._tf_chain_set_multi_delay_topology(
    chain, Number(parameters.topology ?? 0),
    Number(parameters.seriesFeedback ?? 0),
    Number(parameters.seriesFeedbackSend ?? 3),
    Number(parameters.seriesFeedbackReturn ?? 0),
    Number(parameters.masterTime ?? 1),
    Number(parameters.masterFeedback ?? 1),
    Number(parameters.motorSpeed ?? 1),
  );
  const defaultTimeLfo = {
    waveform: 0, phaseDegrees: 90, rateHz: 0.5,
    tempoSync: 0, division: 4, depthMs: 0,
  };
  const timeLfo = Array.isArray(parameters.timeLfo) ? parameters.timeLfo : [];
  for (let index = 0; index < 2; index += 1) {
    const lfo = timeLfo[index] || defaultTimeLfo;
    module._tf_chain_set_multi_delay_time_lfo(
      chain, index, Number(lfo.waveform ?? 0), Number(lfo.phaseDegrees ?? 90),
      Number(lfo.rateHz ?? 0.5), Number(lfo.tempoSync ?? 0),
      Number(lfo.division ?? 4), Number(lfo.depthMs ?? 0),
    );
  }
}

function configurePlexDelay(module, chain, parameters = {}) {
  const defaults = [
    [45, 0, 9, 1, -0.85], [70, 0, 8, 1, -0.6],
    [110, 0, 7, 1, -0.35], [150, 0, 6, 1, -0.1],
    [220, 0, 4, 1, 0.1], [300, 0, 3, 1, 0.35],
    [450, 0, 2, 1, 0.6], [600, 0, 0, 1, 0.85],
  ];
  const lines = Array.isArray(parameters.lines) && parameters.lines.length === 8
    ? parameters.lines : defaults.map(([timeMs, timeMode, division, level, pan]) =>
      ({ timeMs, timeMode, division, level, pan }));
  module._tf_chain_set_plex_delay_global(
    chain, parameters.activeLines ?? 8, parameters.tempoBpm ?? 120,
    parameters.masterTime ?? 1, parameters.masterLevel ?? 1,
    parameters.masterPan ?? 1, parameters.decaySeconds ?? 3,
    parameters.diffusion ?? 0.5, parameters.lowCutHz ?? 20,
    parameters.highCutHz ?? 20000, parameters.preDelayMs ?? 0,
    parameters.stackMode ?? 0, parameters.inputGainDb ?? 0,
    parameters.mix ?? 0.25, parameters.levelDb ?? 0,
    parameters.balance ?? 0, parameters.inputSelect ?? 0,
    parameters.inputDiffusionMix ?? 0,
    parameters.inputDiffusionTimeMs ?? 10,
    parameters.killDry ? 1 : 0,
  );
  module._tf_chain_set_plex_delay_advanced(
    chain, Number(parameters.wetFilterType ?? 0),
    Number(parameters.wetFilterFrequencyHz ?? 2000),
    Number(parameters.wetFilterQ ?? 0.707),
    Number(parameters.wetFilterGainDb ?? 0),
    Number(parameters.filterLfoEnabled ?? false),
    Number(parameters.filterLfoType ?? 0),
    Number(parameters.filterLfoRateHz ?? 0.5),
    Number(parameters.filterModFrequencyHz ?? 2000),
    Number(parameters.envelopeEnabled ?? false),
    Number(parameters.envelopeThresholdDb ?? -70),
    Number(parameters.envelopeAttackMs ?? 400),
    Number(parameters.envelopeReleaseMs ?? 15),
    Number(parameters.duckerAttenuationDb ?? 0),
    Number(parameters.duckerThresholdDb ?? -30),
    Number(parameters.duckerReleaseMs ?? 200),
    Number(parameters.stereoSpread ?? 1),
  );
  lines.forEach((line, index) => {
    module._tf_chain_set_plex_delay_line(
      chain, index, Number(line.timeMs), Number(line.timeMode),
      Number(line.division), Number(line.level), Number(line.pan),
    );
  });
}

const wasmModulePromise = createWasmModule({ noInitialRun: true });

const hasHighResolutionTimer = Boolean(
  globalThis.performance && typeof globalThis.performance.now === "function",
);
const highResolutionNow = hasHighResolutionTimer
  ? globalThis.performance.now.bind(globalThis.performance)
  : Date.now;

function nowMs() {
  return highResolutionNow();
}

class TabletFxProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const config = options.processorOptions || {};
    this.channels = Math.max(1, Math.min(2, config.channels || 1));
    this.active = false;
    this.ready = false;
    this.disposed = false;
    this.chain = 0;
    this.module = null;
    this.inputIndex = 0;
    this.outputIndex = 0;
    this.stats = config.statsBuffer ? new Int32Array(config.statsBuffer) : null;
    this.statsFloats = config.statsBuffer ? new Float32Array(config.statsBuffer) : null;
    this.capture = config.captureBuffer ? new Float32Array(config.captureBuffer) : null;
    this.captureControl = config.captureControl ? new Int32Array(config.captureControl) : null;
    this.captureOffset = 0;
    this.captureComplete = false;
    this.tunerGeneration = 0;
    this.tunerReading = { valid: false, midiNote: -1, frequencyHz: 0, cents: 0, confidence: 0 };
    this.enabled = config.enabled || Array.from(
      { length: EFFECTS.length }, (_, index) => [0, 1, 6, OUTPUT_LEVEL_INDEX,
        PEAK_LIMITER_INDEX].includes(index),
    );
    this.initialMix = Number.isFinite(config.mix) ? config.mix : 1;

    this.port.onmessage = (event) => this.onMessage(event.data);
    wasmModulePromise.then((module) => {
      if (this.disposed) return;
      this.module = module;
      this.chain = module._tf_chain_create(sampleRate, this.channels, MAX_BLOCK_FRAMES);
      if (!this.chain) {
        this.port.postMessage({ type: "error", message: "Could not allocate the audio chain." });
        return;
      }
      const channelConversionTest = module._tf_channel_conversion_self_test();
      if (channelConversionTest !== 0) {
        this.port.postMessage({
          type: "error",
          message: `WASM channel conversion self-test failed (${channelConversionTest}).`,
        });
        return;
      }
      const parallelRoutingTest = module._tf_parallel_mixer_self_test();
      if (parallelRoutingTest !== 0) {
        this.port.postMessage({
          type: "error",
          message: `WASM parallel routing self-test failed (${parallelRoutingTest}).`,
        });
        return;
      }
      this.inputIndex = module._tf_chain_input_buffer(this.chain) >>> 2;
      this.outputIndex = module._tf_chain_output_buffer(this.chain) >>> 2;
      for (let i = 0; i < EFFECTS.length; i += 1) {
        module._tf_chain_set_enabled(this.chain, i, this.enabled[i] ? 1 : 0);
      }
      module._tf_chain_set_mix(this.chain, this.initialMix);
      const parameters = config.parameters || {};
      module._tf_chain_set_global_bypass(this.chain, config.globalBypass ? 1 : 0);
      module._tf_chain_set_input_mute(this.chain, config.inputMuted ? 1 : 0);
      module._tf_chain_set_panic_mute(this.chain, config.panicMuted ? 1 : 0);
      module._tf_chain_set_tuner_reference(this.chain, config.tunerReferenceHz ?? 440);
      module._tf_chain_set_tuner_muted(this.chain, config.tunerMuted ? 1 : 0);
      module._tf_chain_set_input_gain(this.chain, parameters.inputGainDb ?? 0);
      module._tf_chain_set_output_gain(this.chain, parameters.outputGainDb ?? 0);
      const compressor = parameters.compressor || [-18, 3, 6, 8, 100, 0];
      module._tf_chain_set_compressor(this.chain, ...compressor);
      const opticalCompressor = parameters.opticalCompressor || [-24, 3, 6, 10, 80, 600, 10, 0];
      module._tf_chain_set_optical_compressor(this.chain, ...opticalCompressor);
      const fetCompressor = parameters.fetCompressor || [-24, 8, 2, 0.1, 50, 0];
      module._tf_chain_set_fet_compressor(this.chain, ...fetCompressor);
      const guitarGraphicEQ = parameters.guitarGraphicEQ || [1, 0, 0, 0, 0, 0, 0, 0];
      module._tf_chain_set_guitar_graphic_eq(this.chain, ...guitarGraphicEQ);
      const bassGraphicEQ = parameters.bassGraphicEQ || [1, 0, 0, 0, 0, 0, 0, 0];
      module._tf_chain_set_bass_graphic_eq(this.chain, ...bassGraphicEQ);
      const shelfTiltEQ = parameters.shelfTiltEQ || [0, 1000, 0];
      module._tf_chain_set_shelf_tilt_eq(this.chain, ...shelfTiltEQ);
       const roomReverb = parameters.roomReverb || [0, 0.8, 10, 6000, 0.2, 0];
       module._tf_chain_set_room_reverb(this.chain, ...roomReverb);
        const hallReverb = parameters.hallReverb || [3.2, 32, 9000, 100, 0.25, 0];
        module._tf_chain_set_hall_reverb(this.chain, ...hallReverb);
        const plateReverb = parameters.plateReverb || [1.8, 0, 7500, 0.82, 0.5, 0.2, 0];
        module._tf_chain_set_plate_reverb(this.chain, ...plateReverb);
       const springReverb = parameters.springReverb || [2.4, 0, 8000, 0.35, 0.2, 0];
       module._tf_chain_set_spring_reverb(this.chain, ...springReverb);
       const autoSwell = parameters.autoSwell || [-36, 250, 250, 6, 6, 1];
       module._tf_chain_set_auto_swell(this.chain, ...autoSwell);
        const rotarySpeaker = parameters.rotarySpeaker || [6, 0, 120, 1, 0, 0.12, 0,
          0.75, 1, 0.55, 0.5, 0.72, 800, 450, 0, 0, 0.25, 1, 1, 1, 0, 0];
        module._tf_chain_set_rotary_speaker(this.chain, ...rotarySpeaker);
       const ringModulator = Array.from(
         parameters.ringModulator || [0, 440, 1, 20000, 1, 0, 0],
       );
       const ringModValues = ringModulator.length === 4
         ? [0, ...ringModulator, 0, 0]
         : ringModulator.length === 5
           ? [...ringModulator, 0, 0]
           : ringModulator.length === 6
             ? [...ringModulator, 0]
             : ringModulator;
       const [ringModType, ringModFrequencyHz, ringModMultiplier,
         ringModHighCutHz, ringModMix, ringModPitchTracking,
         ringModTrackingMode] = ringModValues;
       module._tf_chain_set_ring_modulator(
         this.chain, Math.max(0, Math.min(2, Math.trunc(ringModType))),
         ringModFrequencyHz, ringModMultiplier, ringModHighCutHz, ringModMix,
         Math.max(0, Math.min(1, Math.trunc(ringModPitchTracking))),
         Math.max(0, Math.min(1, Math.trunc(ringModTrackingMode))),
       );
        const rhythmicSlicer = parameters.rhythmicSlicer || [
          1, 120, 8, 8, 1, 0.15, 1, 1, 0,
          1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
        ];
        module._tf_chain_set_rhythmic_slicer(this.chain, ...rhythmicSlicer);
       const stepFilter = parameters.stepFilter || [
         1, 120, 8, 8, 0, 120, 6000, 0.707, 1, 0.15, 4, 1, 0,
         0, 0.72, 0.18, 0.88, 0.32, 0.62, 0.08, 0.96,
         0.24, 0.78, 0.12, 0.9, 0.38, 0.68, 0.04, 0.84,
       ];
       module._tf_chain_set_step_filter(this.chain, ...stepFilter);
       const ashevillePattrn = parameters.ashevillePattrn || [
         0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0.5, 0.75, 3, 0.5, 10,
       ];
       module._tf_chain_set_asheville_pattrn(this.chain, ...ashevillePattrn);
      module._tf_chain_set_drive(this.chain, parameters.drive || 3);
      const eq = parameters.eq || [120, 0, 0.707, 800, 0, 0.707, 5000, 0, 0.707];
      module._tf_chain_set_eq(this.chain, ...eq);
       const fiveBandEq = parameters.fiveBandEq || [
         100, 0, 0.707, 0, 12, 350, 0, 0.707, 0, 12,
         1000, 0, 0.707, 0, 12, 3500, 0, 0.707, 0, 12,
         8000, 0, 0.707, 0, 12, 0,
       ];
       module._tf_chain_set_five_band_eq(this.chain, ...fiveBandEq);
      const delay = parameters.delay || [350, 0.25, 0.25];
      module._tf_chain_set_delay(this.chain, ...delay);
      module._tf_chain_set_delay_bbd_style(this.chain, parameters.delayBbd ? 1 : 0);
      module._tf_chain_set_delay_bbd_mode(this.chain, parameters.delayBbdMode ? 1 : 0);
      const tape = parameters.delayTape || [0, 0.45, 0.35, 0.3, 0.08, 6];
      module._tf_chain_set_delay_tape_echo(this.chain, ...tape);
      const delayTempo = parameters.delayTempo || [0, 120, 4];
      if (delayTempo[0]) {
        module._tf_chain_set_delay_tempo(this.chain, delayTempo[1], delayTempo[2],
          delay[1], delay[2]);
      }
      if (parameters.delayDualEnabled) {
        const dual = parameters.delayDual || [350, 0.25, 0.25, 525, 0.25, 0.25];
        module._tf_chain_set_delay_dual(this.chain, ...dual);
      }
      const multiTap = parameters.delayMultiTap || [250, 0.65, -0.55, 500, 0.45, 0.55,
        750, 0.30, -0.25, 1000, 0.20, 0.25];
      for (let tap = 0; tap < 4; tap += 1) {
        const offset = tap * 3;
        module._tf_chain_set_delay_multitap_tap(this.chain, tap, ...multiTap.slice(offset, offset + 3));
      }
      if (parameters.delayMultiTapEnabled) {
        module._tf_chain_set_delay_multitap(this.chain, 1, delay[1], delay[2]);
      }
      if (parameters.delayPingPong) {
        module._tf_chain_set_delay_ping_pong(this.chain, 1);
      }
      const tapeHeads = parameters.delayTapeHeads || [110, 15];
      if (parameters.delayTapeHeadsEnabled) {
        module._tf_chain_set_delay_tape_heads(this.chain, 1, tapeHeads[0], tapeHeads[1]);
      }
      module._tf_chain_set_pitch(this.chain, parameters.pitchSemitones ?? 0);
      const filters = parameters.filters || [80, 12000];
      module._tf_chain_set_filters(this.chain, ...filters);
      const pedalCompressor = parameters.pedalCompressor || [0.45, 0.45, 0.25, 0];
      module._tf_chain_set_pedal_compressor(this.chain, ...pedalCompressor);
      const noiseGate = parameters.noiseGate || [-60, -72, 3, 2, 20, 80];
      module._tf_chain_set_noise_gate(this.chain, ...noiseGate);
      const volumePedal = parameters.volumePedal || [1, -96, 0, 1];
      module._tf_chain_set_volume_pedal(this.chain, ...volumePedal);
      this.configureVolumePan(parameters.volumePan ||
        [1, -96, 0, 0, 0, 0, -100, 100, 0, 0, 0]);
      this.configureStereoFilter(parameters.stereoFilter ||
        [1, 1000, 2, 0.707, 0, 0, 0, -100, 100, 0, 5, 0.5, 0.5, 0, 0, 0, 0, 0.5, 50, 4000, 0, 0, 1, 0, 120, 1, -36, 5, 80, 0, 0.25, 0, 0, 4]);
      const wah = parameters.wah || [0.5, 350, 2200, 4, 0];
      module._tf_chain_set_wah(this.chain, ...wah);
      const envelopeWah = parameters.envelopeWah || [0.5, 350, 2200, 4, 0, 5, 80, 5, 0];
      module._tf_chain_set_envelope_wah(this.chain, ...envelopeWah);
      module._tf_chain_set_clean_boost(this.chain, parameters.cleanBoostDb ?? 0);
      const overdrive = parameters.overdrive || [0.15, -3, 0.35];
      module._tf_chain_set_overdrive(this.chain, ...overdrive);
      const midOverdrive = parameters.midOverdrive || [0.18, 6, -4, 0.4];
      module._tf_chain_set_mid_overdrive(this.chain, ...midOverdrive);
      const highGainDistortion = parameters.highGainDistortion || [0.75, 100, 9000, 5200, -9, 1];
      module._tf_chain_set_high_gain_distortion(this.chain, ...highGainDistortion);
      const roundFuzz = parameters.roundFuzz || [0.55, -6, 0.85];
      module._tf_chain_set_round_fuzz(this.chain, ...roundFuzz);
      const tremolo = parameters.tremolo || [5, 0.5, 0, 0, 120, 1];
      module._tf_chain_set_tremolo(this.chain, ...tremolo);
      const harmonicTremolo = parameters.harmonicTremolo ||
        [3, 0.65, 700, 0, 0, 120, 1, 1, 10];
      module._tf_chain_set_harmonic_tremolo(this.chain, ...harmonicTremolo);
      const autoSwellDelay = parameters.autoSwellDelay ||
        [350, 0.25, 0.25, 0, 4, 120, 300, 0.15, 0.7, -36, 1, 0, 0];
      module._tf_chain_set_auto_swell_delay(this.chain, ...autoSwellDelay);
       const shufflingLooper = parameters.shufflingLooper ||
         [8, 8, 0.5, 0, 0, 0, 0, 0, 0.7, 0, 0, -12, 12];
       module._tf_chain_set_shuffling_looper_options(this.chain, ...shufflingLooper);
      const tenTapDelay = parameters.tenTapDelay ||
        [4, 220, 120, 0, 30, 0, 50, 100, 1, 100, 0, 20000, 0.25, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
      module._tf_chain_set_ten_tap_delay_options(this.chain, ...tenTapDelay.slice(0, 15));
      tenTapDelay.slice(15, 25).forEach((level, index) => {
        module._tf_chain_set_ten_tap_delay_level(this.chain, index, level);
      });
      const autoPan = parameters.autoPan || [0.5, 0.75, 1, 0, 120, 5];
      module._tf_chain_set_auto_pan(this.chain, ...autoPan);
      const stereoPanner = parameters.stereoPanner ||
        [0.5, 100, 0, 0, 120, 5, 50, 50, 180, 0, 0, 0, 0, 10];
      module._tf_chain_set_stereo_panner(this.chain, ...stereoPanner);
      const megatapDelay = parameters.megatapDelay ||
        [1000, 16, 1, 0, 2, 1, 0, 1, 100, 0, 0, 0, 16, 20, 20000,
          0, 10, 0.5, 0, -70, 400, 15, 0, 0.25, 0, 0, 0];
      module._tf_chain_set_megatap_delay(this.chain, ...megatapDelay);
      const deEsser = parameters.deEsser || [6500, 1.5, -30, 12, 2, 80];
      module._tf_chain_set_de_esser(this.chain, ...deEsser);
      const dualEngineDistortion = parameters.dualEngineDistortion ||
        [0.25, 0.7, 0.6, 0.5, 0.5, 0, 0, 0];
      module._tf_chain_set_dual_engine_distortion(
        this.chain, ...dualEngineDistortion);
      const vibe = parameters.vibe || [1.5, 0.7, 0.2, 2, 0.5, 1, 0, 4, 120];
      module._tf_chain_set_vibe(this.chain, ...vibe);
      configureMultiDelay(module, this.chain, parameters.multiDelay);
      const portaPreamp = parameters.portaPreamp || [6, 3, 0, 0, 0];
      module._tf_chain_set_porta_preamp(this.chain, ...portaPreamp);
      this.configurePitchSequence(parameters.pitchSequence || [
        0, 7, 12, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        4, 0, 0, 4, 250, 120, 15, 7, 40, 80, 1, 1,
      ]);
      this.configureRhythmTapDelay(parameters.rhythmTapDelay || [
        4, 120, 0, 25, 0, 50, 1, 100, 0, 20000, 0.25, 0, 0,
        220, 220, 220, 220, 220, 220, 220, 220, 220, 220,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      ]);
      configurePlexDelay(module, this.chain, parameters.plexDelay);
      this.configureCelestialReverb(parameters.celestialReverb ||
        [0.25, 3, 12, 7, 2, 9000, 0.5, 0]);
      this.configureCloudReverb(parameters.cloudReverb ||
        [0.25, 0.5, 7, 0.8, 9000, 80, 0.3, 0.15, 0]);
      this.configureMercatorDistortion(parameters.mercatorDistortion ||
        [0.45, 0.45, 0, 0, 0, 0, 0, 0]);
      this.configureManualThruZeroFlanger(parameters.manualThruZeroFlanger ||
        [0.34, 8, 5, 0.5, 0, 3, 0.5, 10]);
      this.configureOctobot(parameters.octobot || [0.65, 0.2, 0.25]);
      this.configureAmRingMod(parameters.amRingMod || [440, 0.5, 1, 0]);
      this.configureTapeEcho(parameters.tapeEcho ||
        [0, 350, 120, 4, 0.35, 0.25, 0.25, 0.35, 0, 0.15, 0,
          0, 350, 350, 4, 4]);
      this.configureOctaveFuzz(parameters.octaveFuzz || [0.55, 0.75, 1]);
      this.configureDoubleTake(parameters.doubleTake || [2, 0.25, 0.55, 0.85, 0.15, 0]);
      this.configureEQ5Graphic(parameters.eq5Graphic || [0, 0, 0, 0, 0, 0]);
      this.configureOpticalTremolo(parameters.opticalTremolo ||
        [0, 4.5, 0.5, 0, 0, 120, 1, 50, 50, 0, 0, 0, -36, 0, 1, 0, 0, 0]);
      this.configureFractalBiasTrem(parameters.fractalBiasTrem ||
        [4.5, 0.35, 0, 0, 120, 1, 0, -36, 0, 0.18, 1, 0, 0, 0]);
      this.configureVibratone(parameters.vibratone ||
        [0, 0.8, 0, 6.5, 0.85, 0, 650, 0.5, 0]);
      this.configureStereoCrossover(parameters.stereoCrossover || {
        frequencyHz: 500, frequencyMultiplier: "x1",
        lowLevelLeftDb: 0, lowLevelRightDb: 0,
        highLevelLeftDb: 0, highLevelRightDb: 0,
        lowPanLeft: -1, highPanLeft: -1,
        lowPanRight: 1, highPanRight: 1,
        levelDb: 0, balance: 0,
      });
      this.configureRockBoxDist(parameters.rockBoxDistortion ||
        [0.5, 0.5, 0, 10]);
      this.configureRockBoxBoost(parameters.rockBoxBoost || [0, 0, 10]);
      this.configurePolySustain(parameters.polySustain || [0, 222, 2000, 1, 0, 0]);
      const chorus = parameters.chorus || [0.8, 15, 7, 0.3];
      module._tf_chain_set_chorus(this.chain, ...chorus);
      const dimensionChorus = parameters.dimensionChorus || [0, 0, 0.5, 0];
      module._tf_chain_set_dimension_chorus(this.chain, ...dimensionChorus);
      const tripleDoubleChorus = parameters.tripleDoubleChorus ||
        [0, 0, 0.8, 7, 5, 8, 0.1, 0.2, 1];
      module._tf_chain_set_triple_double_chorus(this.chain, ...tripleDoubleChorus);
      this.configureFourBandResonator(parameters.fourBandResonator ||
        [0, 0, 1, 110, 164.81, 220, 329.63, 110,
          0.75, 0.75, 0.75, 0.75, 2, 2, 2, 2, 0.5, 0.5, 0.5, 0.5,
          -0.75, -0.25, 0.25, 0.75, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0.5]);
      const diatonicHarmonizer = parameters.diatonicHarmonizer ||
        [0.35, 2, 0, 0, 8000, 0, 0.2, 0, 120];
      module._tf_chain_set_diatonic_harmonizer(this.chain, ...diatonicHarmonizer);
      this.configurePitchArpeggiator(parameters.pitchArpeggiator ||
        [1, 1, 1, 0, 0, 4, 0, 7, 120, 250, 0, 0, 0, 0,
          0, 2, 4, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -90]);
      this.configureMonoGuitarSynth(parameters.monoGuitarSynth ||
        [2, 0.75, 0.6, -54, 8, 180, 8]);
      this.configureNoteGenerator(parameters.noteGenerator ||
        [1, 0, 60, 0, 0.5, 0, 60, 0, 0.5, 0, 60, 0, 0.5, 0.5]);
      this.configureFractalThreeVoiceSynth(parameters.fractalThreeVoiceSynth ||
        [3, 2, 440, 0, 0, 0.5, 0, 12000, 0.707, 5, 0.25,
          3, 2, 440, 12, 0, 0.5, -0.35, 12000, 0.707, 10, 0.2,
          3, 2, 440, -12, 0, 0.5, 0.35, 12000, 0.707, 20, 0.2, 0]);
      this.configureEnhancer(parameters.enhancer ||
        [0, 1, 0.25, 120, 12000, 7, 0, 0, 1, 0, 1]);
      this.configureResonantDelay(parameters.resonantDelay ||
        [350, 0.35, 800, 2, 0.25]);
      const grainDelay = parameters.grainDelay || [350, 0.25, 0.25, 80, 2, 0, 0];
      module._tf_chain_set_grain_delay(this.chain, ...grainDelay);
      this.configurePitchFeedbackDelay(parameters.pitchFeedbackDelay ||
        [350, 0.35, 0.25, 7]);
      this.configureChromaticPitchDelay(parameters.chromaticPitchDelay ||
        [0, 350, 0.25, 0.65, 0.25, 0, 120, 4, 0]);
      this.configureFeedbackGenerator(parameters.feedbackGenerator ||
        [1, 0.35, 15, 600, 0, 1, -48]);
      const convolutionReverb = parameters.convolutionReverb || [0.25, 0, 0, 0, 0];
      module._tf_chain_set_convolution_reverb(this.chain, ...convolutionReverb);
      const flanger = parameters.flanger || [0.25, 2, 1, 0.35, 0, 0.35];
      module._tf_chain_set_flanger(this.chain, ...flanger);
      const phaser = parameters.phaser || [0.5, 250, 1600, 0.2, 4, 0.5];
      module._tf_chain_set_phaser(this.chain, ...phaser);
      const notchFilter = parameters.notchFilter || [1000, 4, 1];
      module._tf_chain_set_notch_filter(this.chain, ...notchFilter);
      const reverseDelay = parameters.reverseDelay ||
        [350, 0.25, 0.35, 10, 0, 4, 120, 0, 0, 0, 0];
      module._tf_chain_set_reverse_delay(this.chain, ...reverseDelay);
      module._tf_chain_set_reverse_delay_run(this.chain,
        parameters.reverseDelayRun === false ? 0 : 1);
      const duckingDelay = parameters.duckingDelay ||
        [350, 0.25, 0.25, -30, 12, 250, 0, 0, 120, 4, 0, 0, 0.8, 0, 0];
      module._tf_chain_set_ducking_delay(this.chain, ...duckingDelay);
      const modulatedDelay = parameters.modulatedDelay || [350, 0.25, 0.25, 0.8, 2, 0, 180, -24];
      module._tf_chain_set_modulated_delay(this.chain, ...modulatedDelay);
      const filteredDelay = parameters.filteredDelay || [350, 0.25, 0.25, 120, 6000, 0, 120, 4];
      module._tf_chain_set_filtered_delay(this.chain, ...filteredDelay);
      const sweepDelay = parameters.sweepDelay || [350, 0.25, 0.25, 250, 6000, 1.2, 0.5, 0, 90, 1, 0, 120, 4];
      module._tf_chain_set_sweep_delay(this.chain, ...sweepDelay);
      const formantFilter = parameters.formantFilter || [0, 4, 1, 0];
      module._tf_chain_set_formant_filter(this.chain, ...formantFilter);
      const harmonicResonator = parameters.harmonicResonator || [110, 1.2, 0.55, 0.5];
      module._tf_chain_set_harmonic_resonator(this.chain, ...harmonicResonator);
      const multiband = parameters.multibandCompressor || [250, 2500, 1,
        -24, 2, 6, 10, 100, 0, 1, 0,
        -24, 2, 6, 10, 100, 0, 1, 0,
        -24, 2, 6, 10, 100, 0, 1, 0];
      this.configureMultiband(multiband);
      const bitCrusher = parameters.bitCrusher || [16, 1, 1];
      module._tf_chain_set_bit_crusher(this.chain, ...bitCrusher);
      const vibrato = parameters.vibrato || [5, 0.35, 1];
      module._tf_chain_set_vibrato(this.chain, ...vibrato);
      const limiter = parameters.limiter || [-1, 80];
      module._tf_chain_set_limiter(this.chain, ...limiter);
      const effectMemoryBytes = [];
      for (let i = 0; i < EFFECTS.length; i += 1) {
        effectMemoryBytes.push(module._tf_chain_effect_memory_bytes(this.chain, i));
      }
      this.port.postMessage({
        type: "ready",
        sampleRate,
        maxBlockFrames: MAX_BLOCK_FRAMES,
        memoryBytes: module._tf_chain_memory_bytes(this.chain),
        wasmMemoryBytes: module.HEAPF32.buffer.byteLength,
        effects: EFFECTS,
        channelConversionTest: true,
        parallelRoutingTest: true,
        effectMemoryBytes,
        tunerMemoryBytes: module._tf_chain_tuner_memory_bytes(this.chain),
        timerResolution: hasHighResolutionTimer ? "high-resolution" : "millisecond-fallback",
      });
      this.ready = true;
    }).catch((error) => {
      this.port.postMessage({ type: "error", message: String(error) });
    });
  }

  configureMultiband(values) {
    if (!this.chain || !Array.isArray(values) || values.length !== 27) return;
    this.module._tf_chain_set_multiband_crossover(
      this.chain, values[0], values[1], values[2]);
    for (let band = 0; band < 3; band += 1) {
      const offset = 3 + band * 8;
      this.module._tf_chain_set_multiband_band(
        this.chain, band,
        values[offset], values[offset + 1], values[offset + 2],
        values[offset + 3], values[offset + 4], values[offset + 5],
        values[offset + 6], values[offset + 7]);
    }
  }

  configureBitCrusher(values) {
    if (!this.chain || !Array.isArray(values) || values.length !== 3) return;
    this.module._tf_chain_set_bit_crusher(this.chain, ...values);
  }

  configureFourBandResonator(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 34) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_four_band_resonator(this.chain, pointer, values.length);
    this.module._free(pointer);
  }

  configureDiatonicHarmonizer(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 9) {
      return;
    }
    this.module._tf_chain_set_diatonic_harmonizer(this.chain, ...values);
  }

  configureGrainDelay(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 7) {
      return;
    }
    this.module._tf_chain_set_grain_delay(this.chain, ...values);
  }

  configurePitchFeedbackDelay(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 4) {
      return;
    }
    this.module._tf_chain_set_pitch_feedback_delay(this.chain, ...values);
  }

  configureChromaticPitchDelay(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 9) {
      return;
    }
    this.module._tf_chain_set_chromatic_pitch_delay(this.chain, ...values);
  }

  configureFeedbackGenerator(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 7) {
      return;
    }
    this.module._tf_chain_set_feedback_generator(this.chain, ...values);
  }

  configureShimmerReverb(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 9) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_shimmer_reverb(this.chain, pointer);
    this.module._free(pointer);
  }

  configureCelestialReverb(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 8) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_celestial_reverb(this.chain, pointer);
    this.module._free(pointer);
  }

  configureCloudReverb(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 9) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_cloud_reverb(this.chain, pointer);
    this.module._free(pointer);
  }

  configureMercatorDistortion(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 8) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_mercator_dist(this.chain, pointer);
    this.module._free(pointer);
  }

  configureManualThruZeroFlanger(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 8) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_manual_thru_zero_flanger(this.chain, pointer);
    this.module._free(pointer);
  }

  configureOctobot(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 3) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_octobot(this.chain, pointer);
    this.module._free(pointer);
  }

  configureAmRingMod(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 4) {
      return;
    }
    this.module._tf_chain_set_am_ring_mod(this.chain, ...values);
  }

  configureOctaveFuzz(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 3) {
      return;
    }
    this.module._tf_chain_set_octave_fuzz(
      this.chain, Number(values[0]), Number(values[1]), Number(values[2]) ? 1 : 0,
    );
  }

  configureDoubleTake(values) {
    this.module._tf_chain_set_double_take(this.chain, ...values);
  }

  configureEQ5Graphic(values) {
    this.module._tf_chain_set_eq5_graphic(this.chain, ...values);
  }

  configureOpticalTremolo(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 18) {
      return;
    }
    this.module._tf_chain_set_optical_tremolo(this.chain, ...values);
  }

  configureFractalBiasTrem(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 14) {
      return;
    }
    this.module._tf_chain_set_fractal_bias_trem(this.chain, ...values);
  }

  configureVibratone(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 9) {
      return;
    }
    this.module._tf_chain_set_vibratone(this.chain, ...values);
  }

  configureRockBoxDist(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 4) {
      return;
    }
    this.module._tf_chain_set_rock_box_dist(
      this.chain, Number(values[0]), Number(values[1]),
      Number(values[2]), Number(values[3]),
    );
  }

  configureRockBoxBoost(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 3) {
      return;
    }
    this.module._tf_chain_set_rock_box_boost(
      this.chain, Number(values[0]), Number(values[1]), Number(values[2]),
    );
  }

  configureTapeEcho(values) {
    if (!this.module || !this.chain || !Array.isArray(values)) {
      return;
    }
    const extendedValues = values.length === 11
      ? [...values, 0, values[1], values[1], values[3], values[3]]
      : values;
    if (extendedValues.length !== 16) return;
    this.module._tf_chain_set_tape_echo(this.chain, ...extendedValues);
  }

  configureStereoCrossover(parameters) {
    if (!this.module || !this.chain || !parameters || typeof parameters !== "object") {
      return;
    }
    const values = [
      Number(parameters.frequencyHz),
      parameters.frequencyMultiplier === "x10" ? 1 : 0,
      Number(parameters.lowLevelLeftDb),
      Number(parameters.lowLevelRightDb),
      Number(parameters.highLevelLeftDb),
      Number(parameters.highLevelRightDb),
      Number(parameters.lowPanLeft),
      Number(parameters.highPanLeft),
      Number(parameters.lowPanRight),
      Number(parameters.highPanRight),
      Number(parameters.levelDb),
      Number(parameters.balance),
    ];
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_stereo_crossover(this.chain, pointer);
    this.module._free(pointer);
  }

  loadConvolutionIR(message) {
    const requestId = Number(message.requestId);
    let pointer = 0;
    try {
      const frames = Number(message.frames);
      const channels = Number(message.channels);
      const sampleRate = Number(message.sampleRate);
      if (!Number.isInteger(frames) || frames < 1 || frames > 480000 ||
          ![1, 2].includes(channels) || !Number.isInteger(sampleRate) ||
          sampleRate < 8000 || sampleRate > 192000 ||
          !(message.left instanceof ArrayBuffer) ||
          (channels === 2 && !(message.right instanceof ArrayBuffer))) {
        throw new Error("The convolution IR message has an unsupported WAV shape or sample rate.");
      }
      const left = new Float32Array(message.left);
      const right = channels === 2 ? new Float32Array(message.right) : null;
      if (left.length !== frames || (right && right.length !== frames)) {
        throw new Error("The convolution IR channel lengths do not match its frame count.");
      }
      const sampleCount = frames * channels;
      pointer = this.module._malloc(sampleCount * Float32Array.BYTES_PER_ELEMENT);
      if (!pointer) throw new Error("The fixed WASM heap has no room for the selected convolution IR.");
      const heap = this.module.HEAPF32;
      const offset = pointer >>> 2;
      for (let frame = 0; frame < frames; frame += 1) {
        heap[offset + frame * channels] = left[frame];
        if (right) heap[offset + frame * channels + 1] = right[frame];
      }
      const loaded = this.module._tf_chain_load_convolution_reverb_ir(
        this.chain, pointer, frames, channels, sampleRate,
      );
      if (loaded !== 1) {
        throw new Error("The selected IR is invalid or exceeds the 480,000-frame target-rate limit.");
      }
      this.port.postMessage({
        type: "convolution-ir-loaded",
        requestId,
        version: Number(message.version),
        frames,
        channels,
        sampleRate,
      });
    } catch (error) {
      this.port.postMessage({
        type: "convolution-ir-error",
        requestId,
        version: Number(message.version),
        message: error instanceof Error ? error.message : String(error),
      });
    } finally {
      if (pointer) this.module._free(pointer);
    }
  }

  loadCabinetIR(message) {
    const requestId = Number(message.requestId);
    let pointer = 0;
    try {
      if (this.active) {
        throw new Error("Stop playback before replacing the Cabinet IR.");
      }
      const frames = Number(message.frames);
      const channels = Number(message.channels);
      const sampleRate = Number(message.sampleRate);
      const bitsPerSample = Number(message.bitsPerSample);
      if (frames !== 2048 || channels !== 1 || sampleRate !== 48000 ||
          bitsPerSample !== 24 || !(message.samples instanceof ArrayBuffer)) {
        throw new Error("Cabinet IR requires mono 24-bit PCM at 48 kHz with exactly 2,048 samples.");
      }
      const samples = new Float32Array(message.samples);
      if (samples.length !== frames) {
        throw new Error("Cabinet IR sample count does not match the 2,048-frame WAV shape.");
      }
      pointer = this.module._malloc(frames * Float32Array.BYTES_PER_ELEMENT);
      if (!pointer) throw new Error("The fixed WASM heap has no room for the Cabinet IR.");
      this.module.HEAPF32.set(samples, pointer >>> 2);
      const loaded = this.module._tf_chain_load_cabinet_ir(
        this.chain, pointer, frames, channels, sampleRate, bitsPerSample,
      );
      if (loaded !== 1) {
        throw new Error("The WASM processor rejected this Cabinet IR format.");
      }
      this.port.postMessage({
        type: "cabinet-ir-loaded",
        requestId,
        version: Number(message.version),
        frames,
        channels,
        sampleRate,
        bitsPerSample,
        latencyFrames: this.module._tf_chain_cabinet_ir_latency_frames(),
        memoryBytes: this.module._tf_chain_memory_bytes(this.chain),
        effectMemoryBytes: this.module._tf_chain_effect_memory_bytes(this.chain, 100),
        wasmMemoryBytes: this.module.HEAPF32.buffer.byteLength,
      });
    } catch (error) {
      this.port.postMessage({
        type: "cabinet-ir-error",
        requestId,
        version: Number(message.version),
        message: error instanceof Error ? error.message : String(error),
      });
    } finally {
      if (pointer) this.module._free(pointer);
    }
  }

  configurePitchArpeggiator(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 31) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_pitch_arpeggiator(this.chain, pointer, values.length);
    this.module._free(pointer);
  }

  configurePitchSequence(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 44) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_pitch_sequence(this.chain, pointer, values.length);
    this.module._free(pointer);
  }

  configurePolySustain(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 6) {
      return;
    }
    this.module._tf_chain_set_poly_sustain(
      this.chain, Number(values[0]), Number(values[1]), Number(values[2]),
      Number(values[3]), Number(values[4]), Number(values[5]) | 0,
    );
  }

  configureStepFilterDelay(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 33) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_step_filter_delay(this.chain, pointer, values.length);
    this.module._free(pointer);
  }

  configureVolumePan(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 11) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_volume_pan(this.chain, pointer, values.length);
    this.module._free(pointer);
  }

  configureStereoFilter(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 34) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_stereo_filter(this.chain, pointer, values.length);
    this.module._free(pointer);
  }

  configureStepTremolo(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 20) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_step_tremolo(this.chain, pointer, values.length);
    this.module._free(pointer);
  }

  configureRhythmTapDelay(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 33) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >> 2);
    this.module._tf_chain_set_rhythm_tap_delay(this.chain, pointer, values.length);
    this.module._free(pointer);
  }

  configureMonoGuitarSynth(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 7) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_mono_guitar_synth(this.chain, pointer, values.length);
    this.module._free(pointer);
  }

  configureNoteGenerator(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 14) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_note_generator(this.chain, pointer, values.length);
    this.module._free(pointer);
  }

  configureFractalThreeVoiceSynth(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 34) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_fractal_three_voice_synth(
      this.chain, pointer, values.length,
    );
    this.module._free(pointer);
  }

  configureEnhancer(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 11) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_enhancer(this.chain, pointer, values.length);
    this.module._free(pointer);
  }

  configureResonantDelay(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 5) {
      return;
    }
    const pointer = this.module._malloc(values.length * Float32Array.BYTES_PER_ELEMENT);
    if (!pointer) return;
    this.module.HEAPF32.set(values, pointer >>> 2);
    this.module._tf_chain_set_resonant_delay(this.chain, pointer, values.length);
    this.module._free(pointer);
  }

  onMessage(message) {
    if (!message || typeof message.type !== "string") return;
    if (message.type === "dispose") {
      this.active = false;
      this.disposed = true;
      if (this.module && this.chain &&
          typeof this.module._tf_chain_destroy === "function") {
        this.module._tf_chain_destroy(this.chain);
        this.chain = 0;
      }
      this.ready = false;
      this.port.postMessage({ type: "disposed" });
      return;
    }
    if (message.type === "active") {
      this.active = Boolean(message.value);
    } else if (message.type === "capture-reset") {
      this.captureOffset = 0;
      this.captureComplete = false;
    } else if (message.type === "enable" && this.ready) {
      const index = message.index | 0;
      if (index >= 0 && index < EFFECTS.length) {
        this.enabled[index] = Boolean(message.value);
        this.module._tf_chain_set_enabled(this.chain, index, this.enabled[index] ? 1 : 0);
      }
    } else if (message.type === "mix" && this.ready) {
      this.module._tf_chain_set_mix(this.chain, Number(message.value));
    } else if (message.type === "global-bypass" && this.ready) {
      this.module._tf_chain_set_global_bypass(this.chain, message.value ? 1 : 0);
    } else if (message.type === "input-mute" && this.ready) {
      this.module._tf_chain_set_input_mute(this.chain, message.value ? 1 : 0);
    } else if (message.type === "panic-mute" && this.ready) {
      this.module._tf_chain_set_panic_mute(this.chain, message.value ? 1 : 0);
    } else if (message.type === "clear-meter-clips" && this.ready) {
      this.module._tf_chain_clear_meter_clip(this.chain, 0);
      this.module._tf_chain_clear_meter_clip(this.chain, 1);
    } else if (message.type === "tuner-settings" && this.ready) {
      if (Number.isFinite(message.referenceHz)) {
        this.module._tf_chain_set_tuner_reference(this.chain, Number(message.referenceHz));
      }
      this.module._tf_chain_set_tuner_muted(this.chain, message.muted ? 1 : 0);
    } else if (message.type === "input-gain" && this.ready) {
      this.module._tf_chain_ramp_input_gain(this.chain, Number(message.value));
    } else if (message.type === "output-gain" && this.ready) {
      this.module._tf_chain_ramp_output_gain(this.chain, Number(message.value));
    } else if (message.type === "pitch-semitones" && this.ready) {
      this.module._tf_chain_set_pitch(this.chain, Number(message.value));
    } else if (message.type === "optical-compressor" && this.ready) {
      this.module._tf_chain_set_optical_compressor(this.chain, ...message.value);
    } else if (message.type === "fet-compressor" && this.ready) {
      this.module._tf_chain_set_fet_compressor(this.chain, ...message.value);
    } else if (message.type === "five-band-eq" && this.ready) {
      this.module._tf_chain_set_five_band_eq(this.chain, ...message.value);
    } else if (message.type === "looper-command" && this.ready) {
      const accepted = this.module._tf_chain_looper_command(
        this.chain, Number(message.command) | 0,
      ) !== 0;
      this.port.postMessage({
        type: "looper-status",
        accepted,
        state: this.module._tf_chain_looper_state(this.chain),
        frames: this.module._tf_chain_looper_frames(this.chain),
        sampleRate: globalThis.sampleRate || 48000,
      });
    } else if (message.type === "looper-options" && this.ready) {
      this.module._tf_chain_set_looper_options(
        this.chain, message.reverse ? 1 : 0, message.halfSpeed ? 1 : 0,
        Number(message.overdubLevel), message.autoRecord ? 1 : 0,
        Number(message.autoRecordThresholdDb),
      );
    } else if (message.type === "deluxe-looper-command" && this.ready) {
      const accepted = this.module._tf_chain_deluxe_looper_command(
        this.chain, Number(message.command) | 0,
      ) !== 0;
      this.port.postMessage({
        type: "deluxe-looper-status",
        accepted,
        state: this.module._tf_chain_deluxe_looper_state(this.chain),
        frames: this.module._tf_chain_deluxe_looper_frames(this.chain),
        sampleRate: globalThis.sampleRate || 48000,
      });
    } else if (message.type === "deluxe-looper-status-request" && this.ready) {
      this.port.postMessage({
        type: "deluxe-looper-status",
        state: this.module._tf_chain_deluxe_looper_state(this.chain),
        frames: this.module._tf_chain_deluxe_looper_frames(this.chain),
        sampleRate: globalThis.sampleRate || 48000,
      });
    } else if (message.type === "deluxe-looper-options" && this.ready) {
      this.module._tf_chain_set_deluxe_looper_options(
        this.chain, message.reverse ? 1 : 0, message.halfSpeed ? 1 : 0,
        Number(message.playbackLevel), Number(message.overdubDecayDb),
        Number(message.lowCutHz), Number(message.highCutHz),
      );
    } else if (message.type === "shuffling-looper-command" && this.ready) {
      const accepted = this.module._tf_chain_shuffling_looper_command(
        this.chain, Number(message.command) | 0,
      ) !== 0;
      this.port.postMessage({
        type: "shuffling-looper-status",
        accepted,
        state: this.module._tf_chain_shuffling_looper_state(this.chain),
        frames: this.module._tf_chain_shuffling_looper_frames(this.chain),
        steps: this.module._tf_chain_shuffling_looper_steps(this.chain),
        sampleRate: globalThis.sampleRate || 48000,
      });
    } else if (message.type === "shuffling-looper-status-request" && this.ready) {
      this.port.postMessage({
        type: "shuffling-looper-status",
        state: this.module._tf_chain_shuffling_looper_state(this.chain),
        frames: this.module._tf_chain_shuffling_looper_frames(this.chain),
        steps: this.module._tf_chain_shuffling_looper_steps(this.chain),
        sampleRate: globalThis.sampleRate || 48000,
      });
    } else if (message.type === "shuffling-looper-options" && this.ready) {
      this.module._tf_chain_set_shuffling_looper_options(this.chain, ...message.value);
    } else if (message.type === "ten-tap-delay-options" && this.ready) {
      this.module._tf_chain_set_ten_tap_delay_options(this.chain, ...message.value.slice(0, 15));
      message.value.slice(15, 25).forEach((level, index) => {
        this.module._tf_chain_set_ten_tap_delay_level(this.chain, index, level);
      });
    } else if (message.type === "auto-pan" && this.ready) {
      this.module._tf_chain_set_auto_pan(this.chain, ...message.value);
    } else if (message.type === "stereo-panner" && this.ready) {
      this.module._tf_chain_set_stereo_panner(this.chain, ...message.value);
    } else if (message.type === "megatap-delay" && this.ready) {
      this.module._tf_chain_set_megatap_delay(this.chain, ...message.value);
    } else if (message.type === "de-esser" && this.ready) {
      this.module._tf_chain_set_de_esser(this.chain, ...message.value);
    } else if (message.type === "dual-engine-distortion" && this.ready) {
      this.module._tf_chain_set_dual_engine_distortion(this.chain, ...message.value);
    } else if (message.type === "vibe" && this.ready) {
      this.module._tf_chain_set_vibe(this.chain, ...message.value);
    } else if (message.type === "multi-delay" && this.ready) {
      configureMultiDelay(this.module, this.chain, message.value);
    } else if (message.type === "porta-preamp" && this.ready) {
      this.module._tf_chain_set_porta_preamp(this.chain, ...message.value);
    } else if (message.type === "pitch-sequence" && this.ready) {
      this.configurePitchSequence(message.value);
    } else if (message.type === "poly-sustain" && this.ready) {
      this.configurePolySustain(message.value);
    } else if (message.type === "step-filter-delay" && this.ready) {
      this.configureStepFilterDelay(message.value);
    } else if (message.type === "volume-pan" && this.ready) {
      this.configureVolumePan(message.value);
    } else if (message.type === "stereo-filter" && this.ready) {
      this.configureStereoFilter(message.value);
    } else if (message.type === "step-tremolo" && this.ready) {
      this.configureStepTremolo(message.value);
    } else if (message.type === "rhythm-tap-delay" && this.ready) {
      this.configureRhythmTapDelay(message.value);
    } else if (message.type === "plex-delay" && this.ready) {
      configurePlexDelay(this.module, this.chain, message.value);
    } else if (message.type === "guitar-graphic-eq" && this.ready) {
      this.module._tf_chain_set_guitar_graphic_eq(this.chain, ...message.value);
    } else if (message.type === "bass-graphic-eq" && this.ready) {
      this.module._tf_chain_set_bass_graphic_eq(this.chain, ...message.value);
    } else if (message.type === "shelf-tilt-eq" && this.ready) {
      this.module._tf_chain_set_shelf_tilt_eq(this.chain, ...message.value);
    } else if (message.type === "room-reverb" && this.ready) {
      this.module._tf_chain_set_room_reverb(this.chain, ...message.value);
    } else if (message.type === "hall-reverb" && this.ready) {
      this.module._tf_chain_set_hall_reverb(this.chain, ...message.value);
    } else if (message.type === "plate-reverb" && this.ready) {
      this.module._tf_chain_set_plate_reverb(this.chain, ...message.value);
    } else if (message.type === "spring-reverb" && this.ready) {
      this.module._tf_chain_set_spring_reverb(this.chain, ...message.value);
    } else if (message.type === "auto-swell" && this.ready) {
      this.module._tf_chain_set_auto_swell(this.chain, ...message.value);
    } else if (message.type === "rotary-speaker" && this.ready) {
      this.module._tf_chain_set_rotary_speaker(this.chain, ...message.value);
    } else if (message.type === "ring-modulator" && this.ready) {
      const values = Array.from(message.value);
      const ringModValues = values.length === 4
        ? [0, ...values, 0, 0]
        : values.length === 5
          ? [...values, 0, 0]
          : values.length === 6
            ? [...values, 0]
            : values;
      const [type, frequencyHz, frequencyMultiplier, highCutHz, mix,
        pitchTracking, trackingMode] = ringModValues;
      this.module._tf_chain_set_ring_modulator(
        this.chain, Math.max(0, Math.min(2, Math.trunc(type))),
        frequencyHz, frequencyMultiplier, highCutHz, mix,
        Math.max(0, Math.min(1, Math.trunc(pitchTracking))),
        Math.max(0, Math.min(1, Math.trunc(trackingMode))),
      );
      } else if (message.type === "rhythmic-slicer" && this.ready) {
        this.module._tf_chain_set_rhythmic_slicer(this.chain, ...message.value);
      } else if (message.type === "step-filter" && this.ready) {
        this.module._tf_chain_set_step_filter(this.chain, ...message.value);
     } else if (message.type === "asheville-pattrn" && this.ready) {
       this.module._tf_chain_set_asheville_pattrn(this.chain, ...message.value);
    } else if (message.type === "notch-filter" && this.ready) {
      this.module._tf_chain_set_notch_filter(this.chain, ...message.value);
    } else if (message.type === "formant-filter" && this.ready) {
      this.module._tf_chain_set_formant_filter(this.chain, ...message.value);
    } else if (message.type === "harmonic-resonator" && this.ready) {
      this.module._tf_chain_set_harmonic_resonator(this.chain, ...message.value);
    } else if (message.type === "multiband-compressor" && this.ready) {
      this.configureMultiband(message.value);
    } else if (message.type === "bit-crusher" && this.ready) {
      this.configureBitCrusher(message.value);
    } else if (message.type === "reverse-delay" && this.ready) {
      this.module._tf_chain_set_reverse_delay(this.chain, ...message.value);
    } else if (message.type === "reverse-delay-run" && this.ready) {
      this.module._tf_chain_set_reverse_delay_run(this.chain, message.value ? 1 : 0);
    } else if (message.type === "reverse-delay-restart" && this.ready) {
      this.module._tf_chain_restart_reverse_delay(this.chain);
    } else if (message.type === "ducking-delay" && this.ready) {
      this.module._tf_chain_set_ducking_delay(this.chain, ...message.value);
    } else if (message.type === "modulated-delay" && this.ready) {
      this.module._tf_chain_set_modulated_delay(this.chain, ...message.value);
    } else if (message.type === "filtered-delay" && this.ready) {
      this.module._tf_chain_set_filtered_delay(this.chain, ...message.value);
    } else if (message.type === "sweep-delay" && this.ready) {
      this.module._tf_chain_set_sweep_delay(this.chain, ...message.value);
    } else if (message.type === "volume-pedal" && this.ready) {
      this.module._tf_chain_set_volume_pedal(this.chain, ...message.value);
    } else if (message.type === "noise-gate" && this.ready) {
      this.module._tf_chain_set_noise_gate(this.chain, ...message.value);
    } else if (message.type === "wah" && this.ready) {
      this.module._tf_chain_set_wah(this.chain, ...message.value);
    } else if (message.type === "envelope-wah" && this.ready) {
      this.module._tf_chain_set_envelope_wah(this.chain, ...message.value);
    } else if (message.type === "clean-boost" && this.ready) {
      this.module._tf_chain_set_clean_boost(this.chain, Number(message.value));
    } else if (message.type === "overdrive" && this.ready) {
      this.module._tf_chain_set_overdrive(this.chain, ...message.value);
    } else if (message.type === "mid-overdrive" && this.ready) {
      this.module._tf_chain_set_mid_overdrive(this.chain, ...message.value);
    } else if (message.type === "high-gain-distortion" && this.ready) {
      this.module._tf_chain_set_high_gain_distortion(this.chain, ...message.value);
    } else if (message.type === "round-fuzz" && this.ready) {
      this.module._tf_chain_set_round_fuzz(this.chain, ...message.value);
    } else if (message.type === "tremolo" && this.ready) {
      this.module._tf_chain_set_tremolo(this.chain, ...message.value);
    } else if (message.type === "harmonic-tremolo" && this.ready) {
      this.module._tf_chain_set_harmonic_tremolo(this.chain, ...message.value);
    } else if (message.type === "auto-swell-delay" && this.ready) {
      this.module._tf_chain_set_auto_swell_delay(this.chain, ...message.value);
    } else if (message.type === "chorus" && this.ready) {
      this.module._tf_chain_set_chorus(this.chain, ...message.value);
    } else if (message.type === "dimension-chorus" && this.ready) {
      this.module._tf_chain_set_dimension_chorus(this.chain, ...message.value);
    } else if (message.type === "triple-double-chorus" && this.ready) {
      this.module._tf_chain_set_triple_double_chorus(this.chain, ...message.value);
    } else if (message.type === "four-band-resonator" && this.ready) {
      this.configureFourBandResonator(message.value);
    } else if (message.type === "diatonic-harmonizer" && this.ready) {
      this.configureDiatonicHarmonizer(message.value);
    } else if (message.type === "pitch-arpeggiator" && this.ready) {
      this.configurePitchArpeggiator(message.value);
    } else if (message.type === "mono-guitar-synth" && this.ready) {
      this.configureMonoGuitarSynth(message.value);
    } else if (message.type === "note-generator" && this.ready) {
      this.configureNoteGenerator(message.value);
    } else if (message.type === "fractal-three-voice-synth" && this.ready) {
      this.configureFractalThreeVoiceSynth(message.value);
    } else if (message.type === "enhancer" && this.ready) {
      this.configureEnhancer(message.value);
    } else if (message.type === "resonant-delay" && this.ready) {
      this.configureResonantDelay(message.value);
    } else if (message.type === "grain-delay" && this.ready) {
      this.configureGrainDelay(message.value);
    } else if (message.type === "pitch-feedback-delay" && this.ready) {
      this.configurePitchFeedbackDelay(message.value);
    } else if (message.type === "chromatic-pitch-delay" && this.ready) {
      this.configureChromaticPitchDelay(message.value);
    } else if (message.type === "feedback-generator" && this.ready) {
      this.configureFeedbackGenerator(message.value);
    } else if (message.type === "shimmer-reverb" && this.ready) {
      this.configureShimmerReverb(message.value);
    } else if (message.type === "celestial-reverb" && this.ready) {
      this.configureCelestialReverb(message.value);
    } else if (message.type === "cloud-reverb" && this.ready) {
      this.configureCloudReverb(message.value);
    } else if (message.type === "mercator-distortion" && this.ready) {
      this.configureMercatorDistortion(message.value);
    } else if (message.type === "manual-thru-zero-flanger" && this.ready) {
      this.configureManualThruZeroFlanger(message.value);
    } else if (message.type === "octobot" && this.ready) {
      this.configureOctobot(message.value);
    } else if (message.type === "am-ring-mod" && this.ready) {
      this.configureAmRingMod(message.value);
    } else if (message.type === "tape-echo" && this.ready) {
      this.configureTapeEcho(message.value);
    } else if (message.type === "octave-fuzz" && this.ready) {
      this.configureOctaveFuzz(message.value);
    } else if (message.type === "rock-box-dist" && this.ready) {
      this.configureRockBoxDist(message.value);
    } else if (message.type === "rock-box-boost" && this.ready) {
      this.configureRockBoxBoost(message.value);
    } else if (message.type === "stereo-crossover" && this.ready) {
      this.configureStereoCrossover(message.value);
    } else if (message.type === "convolution-reverb" && this.ready) {
      this.module._tf_chain_set_convolution_reverb(this.chain, ...message.value);
    } else if (message.type === "convolution-ir" && this.ready) {
      this.loadConvolutionIR(message);
    } else if (message.type === "cabinet-ir" && this.ready) {
      this.loadCabinetIR(message);
    } else if (message.type === "octave-divider" && this.ready) {
      this.module._tf_chain_set_octave_divider(this.chain, ...message.value);
    } else if (message.type === "double-take" && this.ready) {
      this.configureDoubleTake(message.value);
    } else if (message.type === "eq5-graphic" && this.ready) {
      this.configureEQ5Graphic(message.value);
    } else if (message.type === "optical-tremolo" && this.ready) {
      this.configureOpticalTremolo(message.value);
    } else if (message.type === "fractal-bias-trem" && this.ready) {
      this.configureFractalBiasTrem(message.value);
    } else if (message.type === "vibratone" && this.ready) {
      this.configureVibratone(message.value);
    } else if (message.type === "flanger" && this.ready) {
      this.module._tf_chain_set_flanger(this.chain, ...message.value);
    } else if (message.type === "delay" && this.ready) {
      const delay = message.value;
      if (delay.tempoSync) {
        this.module._tf_chain_set_delay_tempo(this.chain, delay.bpm, delay.division,
          delay.feedback, delay.mix);
      } else {
        this.module._tf_chain_set_delay(this.chain, delay.delayMs,
          delay.feedback, delay.mix);
      }
    } else if (message.type === "delay-bbd" && this.ready) {
      this.module._tf_chain_set_delay_bbd_style(this.chain, message.value ? 1 : 0);
    } else if (message.type === "delay-character" && this.ready) {
      this.module._tf_chain_set_delay_bbd_style(this.chain, message.value.bbd ? 1 : 0);
      this.module._tf_chain_set_delay_bbd_mode(this.chain, message.value.bbdMode ? 1 : 0);
      this.module._tf_chain_set_delay_tape_echo(this.chain, ...message.value.tape);
    } else if (message.type === "delay-routing" && this.ready) {
      this.module._tf_chain_set_delay_ping_pong(this.chain, message.value ? 1 : 0);
    } else if (message.type === "delay-dual" && this.ready) {
      this.module._tf_chain_set_delay_dual(this.chain, ...message.value);
    } else if (message.type === "delay-multitap" && this.ready) {
      const { taps, feedback, mix } = message.value;
      for (let tap = 0; tap < 4; tap += 1) {
        const offset = tap * 3;
        this.module._tf_chain_set_delay_multitap_tap(
          this.chain, tap, taps[offset], taps[offset + 1], taps[offset + 2]);
      }
      this.module._tf_chain_set_delay_multitap(this.chain, 1, feedback, mix);
    } else if (message.type === "delay-tape-heads" && this.ready) {
      const { enabled, spacingMs, selectedHeads } = message.value;
      this.module._tf_chain_set_delay_tape_heads(
        this.chain, enabled ? 1 : 0, spacingMs, selectedHeads);
    } else if (message.type === "phaser" && this.ready) {
      this.module._tf_chain_set_phaser(this.chain, ...message.value);
    } else if (message.type === "vibrato" && this.ready) {
      this.module._tf_chain_set_vibrato(this.chain, ...message.value);
    } else if (message.type === "limiter" && this.ready) {
      this.module._tf_chain_set_limiter(this.chain, ...message.value);
    } else if (message.type === "reset" && this.ready) {
      this.module._tf_chain_reset(this.chain);
      this.tunerGeneration = 0;
      this.tunerReading = { valid: false, midiNote: -1, frequencyHz: 0, cents: 0, confidence: 0 };
    }
  }

  clearOutputs(outputs) {
    const channels = outputs[0];
    if (!channels) return;
    for (let channel = 0; channel < channels.length; channel += 1) {
      channels[channel].fill(0);
    }
  }

  process(inputs, outputs) {
    const outputChannels = outputs[0];
    if (!outputChannels || outputChannels.length === 0) return true;
    if (!this.ready || !this.active || outputChannels[0].length > MAX_BLOCK_FRAMES) {
      this.clearOutputs(outputs);
      if (this.ready && outputChannels[0].length > MAX_BLOCK_FRAMES && this.stats) {
        Atomics.store(this.stats, 1, 1);
      }
      return true;
    }

    const frames = outputChannels[0].length;
    const inputChannels = inputs[0];
    const heap = this.module.HEAPF32;
    const startBlockMs = nowMs();
    const samples = frames * this.channels;

    for (let frame = 0; frame < frames; frame += 1) {
      for (let channel = 0; channel < this.channels; channel += 1) {
        const source = inputChannels?.[channel] || inputChannels?.[0];
        heap[this.inputIndex + frame * this.channels + channel] = source ? source[frame] : 0;
      }
    }

    const began = this.module._tf_chain_begin(this.chain, frames);
    if (began !== frames) {
      this.clearOutputs(outputs);
      if (this.stats) Atomics.store(this.stats, 1, 1);
      return true;
    }

    const sequence = this.stats ? Atomics.load(this.stats, 0) : 0;
    const ringOffset = this.stats
      ? STATS_HEADER_WORDS + ((sequence % STATS_RING_BLOCKS) * STATS_EFFECT_SLOTS)
      : 0;
    const tunerStartMs = nowMs();
    this.module._tf_chain_process_tuner(this.chain);
    const tunerElapsedMs = nowMs() - tunerStartMs;
    if (this.stats) this.stats[ringOffset + TUNER_TIME] = Math.round(tunerElapsedMs * 1000000);
    for (let effect = 0; effect < EFFECT_PIPELINE_EFFECT_COUNT; effect += 1) {
      const startMs = nowMs();
      this.module._tf_chain_process_effect(this.chain, effect);
      const elapsedMs = nowMs() - startMs;
      if (this.stats) this.stats[ringOffset + effect] = Math.round(elapsedMs * 1000000);
    }
    this.module._tf_chain_finish(this.chain, frames);
    for (let effect = FIRST_EXTRA_EFFECT_INDEX; effect < EFFECTS.length; effect += 1) {
      const startMs = nowMs();
      this.module._tf_chain_process_effect(this.chain, effect);
      const elapsedMs = nowMs() - startMs;
      if (this.stats) this.stats[ringOffset + effect] = Math.round(elapsedMs * 1000000);
    }
    const outputGainStartMs = nowMs();
    this.module._tf_chain_process_output_gain(this.chain);
    const outputGainElapsedMs = nowMs() - outputGainStartMs;
    if (this.stats) this.stats[ringOffset + OUTPUT_LEVEL_INDEX] = Math.round(outputGainElapsedMs * 1000000);
    const limiterStartMs = nowMs();
    this.module._tf_chain_process_output_limiter(this.chain);
    const limiterElapsedMs = nowMs() - limiterStartMs;
    if (this.stats) this.stats[ringOffset + PEAK_LIMITER_INDEX] = Math.round(limiterElapsedMs * 1000000);

    const tunerGeneration = this.module._tf_chain_tuner_generation(this.chain) >>> 0;
    if (tunerGeneration !== this.tunerGeneration) {
      this.tunerGeneration = tunerGeneration;
      this.tunerReading = {
        valid: this.module._tf_chain_tuner_valid(this.chain) !== 0,
        midiNote: this.module._tf_chain_tuner_note(this.chain),
        frequencyHz: this.module._tf_chain_tuner_frequency(this.chain),
        cents: this.module._tf_chain_tuner_cents(this.chain),
        confidence: this.module._tf_chain_tuner_confidence(this.chain),
      };
    }

    for (let frame = 0; frame < frames; frame += 1) {
      for (let channel = 0; channel < outputChannels.length; channel += 1) {
        const sampleIndex = frame * this.channels + Math.min(channel, this.channels - 1);
        outputChannels[channel][frame] = heap[this.outputIndex + sampleIndex];
      }
    }

    const callbackMs = nowMs() - startBlockMs;
    if (this.stats) {
      this.stats[ringOffset + EFFECTS.length] = Math.round(callbackMs * 1000000);
      this.stats[ringOffset + EFFECTS.length + 1] = frames;
      this.statsFloats[ringOffset + METER_INPUT_PEAK] = this.module._tf_chain_meter_peak(this.chain, 0);
      this.statsFloats[ringOffset + METER_INPUT_RMS] = this.module._tf_chain_meter_rms(this.chain, 0);
      this.statsFloats[ringOffset + METER_OUTPUT_PEAK] = this.module._tf_chain_meter_peak(this.chain, 1);
      this.statsFloats[ringOffset + METER_OUTPUT_RMS] = this.module._tf_chain_meter_rms(this.chain, 1);
      const inputFlags = this.module._tf_chain_meter_flags(this.chain, 0);
      const outputFlags = this.module._tf_chain_meter_flags(this.chain, 1);
      this.stats[ringOffset + METER_FLAGS] = (inputFlags & 0x3) | ((outputFlags & 0x3) << 2);
      this.stats[ringOffset + METER_INPUT_INVALID] = this.module._tf_chain_meter_invalid_samples(this.chain, 0);
      this.stats[ringOffset + METER_OUTPUT_INVALID] = this.module._tf_chain_meter_invalid_samples(this.chain, 1);
      this.stats[ringOffset + TUNER_GENERATION] = this.tunerGeneration;
      this.stats[ringOffset + TUNER_NOTE] = this.tunerReading.midiNote;
      this.stats[ringOffset + TUNER_VALID] = this.tunerReading.valid ? 1 : 0;
      this.statsFloats[ringOffset + TUNER_FREQUENCY] = this.tunerReading.frequencyHz;
      this.statsFloats[ringOffset + TUNER_CENTS] = this.tunerReading.cents;
      this.statsFloats[ringOffset + TUNER_CONFIDENCE] = this.tunerReading.confidence;
      Atomics.store(this.stats, 0, sequence + 1);
    }

    if (this.capture && this.captureControl && !this.captureComplete) {
      for (let i = 0; i < samples && this.captureOffset < this.capture.length; i += 1) {
        this.capture[this.captureOffset] = heap[this.outputIndex + i];
        this.captureOffset += 1;
      }
      Atomics.store(this.captureControl, 0, this.captureOffset);
      if (this.captureOffset >= this.capture.length) {
        this.captureComplete = true;
        Atomics.store(this.captureControl, 1, 1);
      }
    }

    return true;
  }
}

registerProcessor("tabletfx-chain", TabletFxProcessor);
