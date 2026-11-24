export function decodePcmWave(audioContext, bytes) {
  const view = new DataView(bytes);
  if (view.byteLength < 44 || view.getUint32(0, false) !== 0x52494646 ||
      view.getUint32(8, false) !== 0x57415645) {
    return null;
  }

  let format = 0;
  let channels = 0;
  let sampleRate = 0;
  let bitsPerSample = 0;
  let dataOffset = -1;
  let dataLength = 0;
  for (let offset = 12; offset + 8 <= view.byteLength;) {
    const id = view.getUint32(offset, false);
    const length = view.getUint32(offset + 4, true);
    const body = offset + 8;
    if (body + length > view.byteLength) return null;
    if (id === 0x666d7420 && length >= 16) {
      format = view.getUint16(body, true);
      channels = view.getUint16(body + 2, true);
      sampleRate = view.getUint32(body + 4, true);
      bitsPerSample = view.getUint16(body + 14, true);
    } else if (id === 0x64617461) {
      dataOffset = body;
      dataLength = length;
    }
    offset = body + length + (length & 1);
  }

  const bytesPerSample = bitsPerSample / 8;
  if ((format !== 1 && format !== 3) || channels < 1 || channels > 2 ||
      sampleRate < 8000 || sampleRate > 192000 ||
      ![8, 16, 24, 32].includes(bitsPerSample) ||
      (format === 3 && bitsPerSample !== 32) || dataOffset < 0 ||
      dataLength % (channels * bytesPerSample) !== 0) {
    return null;
  }

  const frames = dataLength / (channels * bytesPerSample);
  const audio = audioContext.createBuffer(channels, frames, sampleRate);
  for (let channel = 0; channel < channels; channel += 1) {
    const output = audio.getChannelData(channel);
    for (let frame = 0; frame < frames; frame += 1) {
      const sampleOffset = dataOffset + (frame * channels + channel) * bytesPerSample;
      let sample;
      if (format === 3) {
        sample = view.getFloat32(sampleOffset, true);
      } else if (bitsPerSample === 8) {
        sample = (view.getUint8(sampleOffset) - 128) / 128;
      } else if (bitsPerSample === 16) {
        sample = view.getInt16(sampleOffset, true) / 32768;
      } else if (bitsPerSample === 24) {
        let integer = view.getUint8(sampleOffset) |
          (view.getUint8(sampleOffset + 1) << 8) |
          (view.getUint8(sampleOffset + 2) << 16);
        if (integer & 0x800000) integer |= 0xff000000;
        sample = integer / 8388608;
      } else {
        sample = view.getInt32(sampleOffset, true) / 2147483648;
      }
      output[frame] = Number.isFinite(sample) ? sample : 0;
    }
  }
  return audio;
}

export function decodeCabinetIrWave(bytes) {
  const view = new DataView(bytes);
  if (view.byteLength < 12 || view.getUint32(0, false) !== 0x52494646 ||
      view.getUint32(8, false) !== 0x57415645) {
    throw new Error("Cabinet IR must be a RIFF/WAVE PCM file.");
  }

  const riffEnd = view.getUint32(4, true) + 8;
  if (riffEnd < 12 || riffEnd > view.byteLength) {
    throw new Error("Cabinet IR WAV has a truncated or invalid RIFF length.");
  }

  let format = null;
  let dataOffset = -1;
  let dataLength = 0;
  for (let offset = 12; offset < riffEnd;) {
    if (offset + 8 > riffEnd) {
      throw new Error("Cabinet IR WAV contains a truncated RIFF chunk header.");
    }
    const id = view.getUint32(offset, false);
    const length = view.getUint32(offset + 4, true);
    const body = offset + 8;
    const end = body + length;
    if (end > riffEnd) {
      throw new Error("Cabinet IR WAV contains a truncated RIFF chunk.");
    }
    if (id === 0x666d7420) {
      if (format || length < 16) {
        throw new Error("Cabinet IR WAV must contain one valid PCM format chunk.");
      }
      format = {
        encoding: view.getUint16(body, true),
        channels: view.getUint16(body + 2, true),
        sampleRate: view.getUint32(body + 4, true),
        byteRate: view.getUint32(body + 8, true),
        blockAlign: view.getUint16(body + 12, true),
        bitsPerSample: view.getUint16(body + 14, true),
      };
    } else if (id === 0x64617461) {
      if (dataOffset >= 0) {
        throw new Error("Cabinet IR WAV must contain one audio data chunk.");
      }
      dataOffset = body;
      dataLength = length;
    }
    offset = end + (length & 1);
    if (offset > riffEnd) {
      throw new Error("Cabinet IR WAV has an invalid RIFF chunk padding byte.");
    }
  }

  if (!format || dataOffset < 0) {
    throw new Error("Cabinet IR WAV must contain PCM format and audio data chunks.");
  }
  if (format.encoding !== 1) {
    throw new Error("Cabinet IR WAV must use integer PCM encoding.");
  }
  if (format.channels !== 1) {
    throw new Error(`Cabinet IR must be mono; this file has ${format.channels} channels.`);
  }
  if (format.bitsPerSample !== 24) {
    throw new Error(`Cabinet IR must be 24-bit PCM; this file is ${format.bitsPerSample}-bit.`);
  }
  if (format.sampleRate !== 48000) {
    throw new Error(`Cabinet IR must be 48 kHz; this file is ${format.sampleRate} Hz.`);
  }
  if (format.blockAlign !== 3 || format.byteRate !== 144000) {
    throw new Error("Cabinet IR WAV has inconsistent 24-bit mono PCM alignment.");
  }
  if (dataLength % format.blockAlign !== 0) {
    throw new Error("Cabinet IR WAV data is not aligned to complete PCM frames.");
  }

  const frames = dataLength / format.blockAlign;
  if (frames !== 2048) {
    throw new Error(`Cabinet IR must contain exactly 2,048 samples; this file has ${frames}.`);
  }

  const samples = new Float32Array(frames);
  for (let frame = 0; frame < frames; frame += 1) {
    const sampleOffset = dataOffset + frame * 3;
    let value = view.getUint8(sampleOffset) +
      view.getUint8(sampleOffset + 1) * 256 +
      view.getUint8(sampleOffset + 2) * 65536;
    if (value >= 0x800000) value -= 0x1000000;
    samples[frame] = value / 8388608;
  }

  return {
    samples,
    frames,
    channels: format.channels,
    sampleRate: format.sampleRate,
    bitsPerSample: format.bitsPerSample,
  };
}
