import createWasmModule from "../build/web/tabletfx_wasm.js";

const EFFECTS = [
  "input-trim", "volume-pedal", "high-pass", "low-pass", "noise-gate",
  "pedal-compressor", "compressor", "wah", "envelope-wah", "clean-boost",
  "transparent-overdrive",
  "mid-focused-overdrive", "high-gain-distortion", "round-fuzz", "soft-drive", "tremolo", "chorus", "flanger", "phaser", "vibrato", "three-band-eq", "delay", "poly-pitch-shifter",
  "optical-compressor", "fet-compressor", "guitar-graphic-eq", "bass-graphic-eq", "shelf-tilt-eq",
    "room-reverb", "hall-reverb", "plate-reverb", "spring-reverb", "auto-swell", "rotary-speaker", "ring-modulator", "rhythmic-slicer", "step-filter", "notch-filter", "reverse-delay", "ducking-delay", "modulated-delay", "filtered-delay", "sweep-delay", "formant-filter", "harmonic-resonator", "multiband-compressor", "bit-crusher", "dimension-chorus", "triple-double-chorus", "four-band-resonator", "diatonic-harmonizer", "pitch-arpeggiator", "mono-guitar-synth", "grain-delay", "pitch-feedback-delay", "feedback-generator", "output-level", "peak-limiter",
];
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
      { length: EFFECTS.length }, (_, index) => [0, 1, 6, EFFECTS.length - 2,
        EFFECTS.length - 1].includes(index),
    );
    this.initialMix = Number.isFinite(config.mix) ? config.mix : 1;

    this.port.onmessage = (event) => this.onMessage(event.data);
    wasmModulePromise.then((module) => {
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
       const ringModulator = parameters.ringModulator || [440, 1, 20000, 1];
       module._tf_chain_set_ring_modulator(this.chain, ...ringModulator);
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
      module._tf_chain_set_drive(this.chain, parameters.drive || 3);
      const eq = parameters.eq || [120, 0, 0.707, 800, 0, 0.707, 5000, 0, 0.707];
      module._tf_chain_set_eq(this.chain, ...eq);
      const delay = parameters.delay || [350, 0.25, 0.25];
      module._tf_chain_set_delay(this.chain, ...delay);
      module._tf_chain_set_delay_bbd_style(this.chain, parameters.delayBbd ? 1 : 0);
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
      const grainDelay = parameters.grainDelay || [350, 0.25, 0.25, 80, 2, 0, 0];
      module._tf_chain_set_grain_delay(this.chain, ...grainDelay);
      this.configurePitchFeedbackDelay(parameters.pitchFeedbackDelay ||
        [350, 0.35, 0.25, 7]);
      this.configureFeedbackGenerator(parameters.feedbackGenerator ||
        [1, 0.35, 15, 600, 0, 1, -48]);
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
      const duckingDelay = parameters.duckingDelay || [-30, 12, 250];
      module._tf_chain_set_ducking_delay(this.chain,
        350, 0.25, 0.25, ...duckingDelay);
      const modulatedDelay = parameters.modulatedDelay || [350, 0.25, 0.25, 0.8, 2, 0, 180, -24];
      module._tf_chain_set_modulated_delay(this.chain, ...modulatedDelay);
      const filteredDelay = parameters.filteredDelay || [350, 0.25, 0.25, 120, 6000];
      module._tf_chain_set_filtered_delay(this.chain, ...filteredDelay);
      const sweepDelay = parameters.sweepDelay || [350, 0.25, 0.25, 250, 6000, 1.2, 0.5, 0, 90];
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

  configureFeedbackGenerator(values) {
    if (!this.module || !this.chain || !Array.isArray(values) || values.length !== 7) {
      return;
    }
    this.module._tf_chain_set_feedback_generator(this.chain, ...values);
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

  onMessage(message) {
    if (!message || typeof message.type !== "string") return;
    if (message.type === "active") {
      this.active = Boolean(message.value);
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
      this.module._tf_chain_set_ring_modulator(this.chain, ...message.value);
      } else if (message.type === "rhythmic-slicer" && this.ready) {
        this.module._tf_chain_set_rhythmic_slicer(this.chain, ...message.value);
      } else if (message.type === "step-filter" && this.ready) {
        this.module._tf_chain_set_step_filter(this.chain, ...message.value);
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
    } else if (message.type === "grain-delay" && this.ready) {
      this.configureGrainDelay(message.value);
    } else if (message.type === "pitch-feedback-delay" && this.ready) {
      this.configurePitchFeedbackDelay(message.value);
    } else if (message.type === "feedback-generator" && this.ready) {
      this.configureFeedbackGenerator(message.value);
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
    for (let effect = 0; effect < EFFECTS.length - 2; effect += 1) {
      const startMs = nowMs();
      this.module._tf_chain_process_effect(this.chain, effect);
      const elapsedMs = nowMs() - startMs;
      if (this.stats) this.stats[ringOffset + effect] = Math.round(elapsedMs * 1000000);
    }
    this.module._tf_chain_finish(this.chain, frames);
    const outputGainStartMs = nowMs();
    this.module._tf_chain_process_output_gain(this.chain);
    const outputGainElapsedMs = nowMs() - outputGainStartMs;
    if (this.stats) this.stats[ringOffset + EFFECTS.length - 2] = Math.round(outputGainElapsedMs * 1000000);
    const limiterStartMs = nowMs();
    this.module._tf_chain_process_output_limiter(this.chain);
    const limiterElapsedMs = nowMs() - limiterStartMs;
    if (this.stats) this.stats[ringOffset + EFFECTS.length - 1] = Math.round(limiterElapsedMs * 1000000);

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
