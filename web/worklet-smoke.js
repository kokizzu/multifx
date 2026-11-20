class TabletFxWorkletSmoke extends AudioWorkletProcessor {
  process(_inputs, outputs) {
    const channels = outputs[0] || [];
    for (let channel = 0; channel < channels.length; channel += 1) channels[channel].fill(0);
    return true;
  }
}

registerProcessor("tabletfx-worklet-smoke", TabletFxWorkletSmoke);
