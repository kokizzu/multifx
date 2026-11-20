import createWasmModule from "../build/web/tabletfx_wasm.js";

const EFFECTS = [
  "input-trim", "volume-pedal", "high-pass", "low-pass", "noise-gate",
  "pedal-compressor", "compressor", "wah", "soft-drive", "tremolo",
  "three-band-eq", "delay", "output-level", "peak-limiter",
];
const MAX_BLOCK_FRAMES = 8192;
const STATS_HEADER_WORDS = 2;
const STATS_RING_BLOCKS = 512;
const STATS_EFFECT_SLOTS = EFFECTS.length + 2;

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
    this.capture = config.captureBuffer ? new Float32Array(config.captureBuffer) : null;
    this.captureControl = config.captureControl ? new Int32Array(config.captureControl) : null;
    this.captureOffset = 0;
    this.captureComplete = false;
    this.enabled = config.enabled || [true, true, false, false, false, false,
      true, false, false, false, false, false, true, true];
    this.initialMix = Number.isFinite(config.mix) ? config.mix : 1;

    this.port.onmessage = (event) => this.onMessage(event.data);
    wasmModulePromise.then((module) => {
      this.module = module;
      this.chain = module._tf_chain_create(sampleRate, this.channels, MAX_BLOCK_FRAMES);
      if (!this.chain) {
        this.port.postMessage({ type: "error", message: "Could not allocate the audio chain." });
        return;
      }
      this.inputIndex = module._tf_chain_input_buffer(this.chain) >>> 2;
      this.outputIndex = module._tf_chain_output_buffer(this.chain) >>> 2;
      for (let i = 0; i < EFFECTS.length; i += 1) {
        module._tf_chain_set_enabled(this.chain, i, this.enabled[i] ? 1 : 0);
      }
      module._tf_chain_set_mix(this.chain, this.initialMix);
      const parameters = config.parameters || {};
      module._tf_chain_set_input_gain(this.chain, parameters.inputGainDb ?? 0);
      module._tf_chain_set_output_gain(this.chain, parameters.outputGainDb ?? 0);
      const compressor = parameters.compressor || [-18, 3, 6, 8, 100, 0];
      module._tf_chain_set_compressor(this.chain, ...compressor);
      module._tf_chain_set_drive(this.chain, parameters.drive || 3);
      const eq = parameters.eq || [120, 0, 0.707, 800, 0, 0.707, 5000, 0, 0.707];
      module._tf_chain_set_eq(this.chain, ...eq);
      const delay = parameters.delay || [350, 0.25, 0.25];
      module._tf_chain_set_delay(this.chain, ...delay);
      const filters = parameters.filters || [80, 12000];
      module._tf_chain_set_filters(this.chain, ...filters);
      const pedalCompressor = parameters.pedalCompressor || [0.45, 0.45, 0.25, 0];
      module._tf_chain_set_pedal_compressor(this.chain, ...pedalCompressor);
      const volumePedal = parameters.volumePedal || [1, -96, 0, 1];
      module._tf_chain_set_volume_pedal(this.chain, ...volumePedal);
      const wah = parameters.wah || [0.5, 350, 2200, 4, 0];
      module._tf_chain_set_wah(this.chain, ...wah);
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
        effectMemoryBytes,
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
    } else if (message.type === "input-gain" && this.ready) {
      this.module._tf_chain_ramp_input_gain(this.chain, Number(message.value));
    } else if (message.type === "output-gain" && this.ready) {
      this.module._tf_chain_ramp_output_gain(this.chain, Number(message.value));
    } else if (message.type === "volume-pedal" && this.ready) {
      this.module._tf_chain_set_volume_pedal(this.chain, ...message.value);
    } else if (message.type === "wah" && this.ready) {
      this.module._tf_chain_set_wah(this.chain, ...message.value);
    } else if (message.type === "tremolo" && this.ready) {
      this.module._tf_chain_set_tremolo(this.chain, ...message.value);
    } else if (message.type === "limiter" && this.ready) {
      this.module._tf_chain_set_limiter(this.chain, ...message.value);
    } else if (message.type === "reset" && this.ready) {
      this.module._tf_chain_reset(this.chain);
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
