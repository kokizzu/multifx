export async function runFractalMultitapGlobalMixBrowserSmokeTest(context) {
  const {
    reference,
    captureView,
    captureBuffer,
    processorReady,
    getWorkletNode,
    getDecodedAudio,
    getAudioContext,
    enabledEffects,
    stopSource,
    waitForCapture,
    getPlayTask,
    playButton,
    setPlaybackOffset,
    defaults,
    effectNames,
    updateOptions,
    controls,
  } = context;
  const previousEffectState = enabledEffects();
  const previousControls = controls.all.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  const previousDefaults = JSON.stringify(defaults);
  let barrierSequence = 0;
  const waitForBarrier = () => new Promise((resolve, reject) => {
    const port = getWorkletNode().port;
    const token = `fractal-multitap-global-mix-${++barrierSequence}`;
    const timeout = setTimeout(() => {
      port.removeEventListener("message", onMessage);
      reject(new Error("Timed out waiting for the Multitap Global Mix worklet barrier."));
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
  const rmsDifference = (left, right) => {
    let squared = 0;
    for (let i = 0; i < left.length; i += 1) {
      const difference = left[i] - right[i];
      squared += difference * difference;
    }
    return Math.sqrt(squared / Math.max(1, left.length));
  };
  try {
    await processorReady;
    const workletNode = getWorkletNode();
    const decodedAudio = getDecodedAudio();
    const audioContext = getAudioContext();
    if (!workletNode || !decodedAudio || !audioContext) {
      throw new Error("Multitap Global Mix test requires the ready browser chain and local guitar audio.");
    }
    const pickerRow = document.querySelector('[data-effect="72"]')?.closest(".effect-row");
    if (pickerRow?.dataset.category !== "delay" ||
        pickerRow.querySelector(".effect-category-tag")?.textContent.trim() !== "Delay" ||
        !effectNames[72]?.toLowerCase().includes("multi-delay") ||
        controls.globalMixEnabled.checked ||
        controls.globalMixOffset.value !== "0" ||
        controls.globalMixOffset.min !== "-50" ||
        controls.globalMixOffset.max !== "50" ||
        defaults.globalMixEnabled !== false || defaults.globalMixOffset !== 0) {
      throw new Error("Multitap Delay category, picker ID, or default-off Global Mix controls are invalid.");
    }
    await audioContext.resume();
    stopSource(false);
    const testEnabled = previousEffectState.map((_, index) => index === 72);
    document.querySelectorAll("[data-effect]").forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    const setControls = () => {
      const times = [45, 90, 135, 180];
      const pans = [0, 33, 67, 100];
      controls.lineControls.forEach((line, index) => {
        line.timeMs.value = String(times[index]);
        line.timeMode.value = "0";
        line.division.value = "4";
        line.feedback.value = "0";
        line.crossFeedback.value = "0";
        line.level.value = "100";
        line.pan.value = String(pans[index]);
      });
      controls.lfoControls.forEach((lfo) => {
        lfo.waveform.value = "0";
        lfo.phaseDegrees.value = "90";
        lfo.rateHz.value = "0.5";
        lfo.rateMode.value = "0";
        lfo.division.value = "4";
        lfo.depthMs.value = "0";
      });
      controls.topology.value = "0";
      controls.tempo.value = "120";
      controls.lowCut.value = "20";
      controls.highCut.value = "20000";
      controls.inputGain.value = "0";
      controls.mix.value = "25";
      controls.level.value = "0";
      controls.seriesFeedback.value = "0";
      controls.seriesSend.value = "3";
      controls.seriesReturn.value = "0";
      controls.diffusorMasterTime.value = "100";
      controls.diffusorMasterFeedback.value = "50";
      controls.motorSpeed.value = "1";
    };
    const captureAt = async (enabled, offsetPoints) => {
      setControls();
      controls.globalMixEnabled.checked = enabled;
      controls.globalMixOffset.value = String(offsetPoints);
      updateOptions();
      await waitForBarrier();
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
      await waitForBarrier();
      Atomics.store(captureView, 0, 0);
      Atomics.store(captureView, 1, 0);
      setPlaybackOffset(0);
      playButton.click();
      await Promise.race([
        getPlayTask(),
        new Promise((_, reject) => setTimeout(
          () => reject(new Error("Timed out starting Multitap Global Mix playback.")), 5000,
        )),
      ]);
      await waitForCapture(captureView, reference.samples.length);
      stopSource(false);
      const output = Float32Array.from(new Float32Array(captureBuffer));
      for (let i = 0; i < output.length; i += 1) {
        if (!Number.isFinite(output[i])) {
          throw new Error(`Multitap Global Mix output is not finite at sample ${i}.`);
        }
      }
      return output;
    };
    const baseline = await captureAt(false, 0);
    const storedDisabled = await captureAt(false, 20);
    const enabled = await captureAt(true, 20);
    const disabledRms = rmsDifference(baseline, storedDisabled);
    const enabledRms = rmsDifference(baseline, enabled);
    if (disabledRms > 1.0e-7 || enabledRms < 1.0e-5) {
      throw new Error(`Multitap Global Mix failed: disabled RMS ${disabledRms}, enabled RMS ${enabledRms}.`);
    }
    return {
      category: "Delay",
      effectIndex: 72,
      defaultEnabled: false,
      defaultOffsetPoints: 0,
      disabledOffsetRms: disabledRms,
      enabledPlus20ppRms: enabledRms,
      channels: reference.channels,
      frames: reference.frames,
    };
  } finally {
    const workletNode = getWorkletNode();
    stopSource(false);
    if (workletNode) {
      workletNode.port.postMessage({ type: "active", value: false });
      controls.all.forEach((control, index) => {
        if (control.type === "checkbox") control.checked = previousControls[index];
        else control.value = previousControls[index];
      });
      updateOptions();
      if (JSON.stringify(defaults) !== previousDefaults) {
        Object.assign(defaults, JSON.parse(previousDefaults));
      }
      document.querySelectorAll("[data-effect]").forEach((checkbox, index) => {
        checkbox.checked = previousEffectState[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffectState[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}
