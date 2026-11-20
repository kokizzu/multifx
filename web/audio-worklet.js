import createWasmModule from "../build/web/tabletfx_wasm.js";

const EFFECTS = [
  "input-trim", "volume-pedal", "high-pass", "low-pass", "noise-gate",
  "pedal-compressor", "compressor", "wah", "envelope-wah", "clean-boost",
  "soft-drive", "tremolo", "three-band-eq", "delay", "poly-pitch-shifter",
  "output-level", "peak-limiter",
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
    this.enabled = config.enabled || [true, true, false, false, false, false,
      true, false, false, false, false, false, false, false, false, true, true];
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
      module._tf_chain_set_drive(this.chain, parameters.drive || 3);
      const eq = parameters.eq || [120, 0, 0.707, 800, 0, 0.707, 5000, 0, 0.707];
      module._tf_chain_set_eq(this.chain, ...eq);
      const delay = parameters.delay || [350, 0.25, 0.25];
      module._tf_chain_set_delay(this.chain, ...delay);
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
      const tremolo = parameters.tremolo || [5, 0.5, 0, 0, 120, 1];
      module._tf_chain_set_tremolo(this.chain, ...tremolo);
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
    } else if (message.type === "tremolo" && this.ready) {
      this.module._tf_chain_set_tremolo(this.chain, ...message.value);
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
