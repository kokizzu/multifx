export async function runFractalPhaserGlobalMixBrowserSmokeTest(context) {
  const {
    reference,
    captureView,
    captureBuffer,
    processorReady,
    getWorkletNode,
    getAudioContext,
    enabledEffects,
    stopSource,
    waitForCapture,
    getPlayTask,
    playButton,
    setPlaybackOffset,
    defaults,
    effectNames,
    updatePhaser,
    controls,
  } = context;
  const previousEffects = enabledEffects();
  const previousControls = controls.all.map((control) =>
    control.type === "checkbox" ? control.checked : control.value);
  let barrierSequence = 0;
  const waitForBarrier = () => new Promise((resolve, reject) => {
    const port = getWorkletNode().port;
    const token = `fractal-phaser-global-mix-${++barrierSequence}`;
    const timeout = setTimeout(() => {
      port.removeEventListener("message", onMessage);
      reject(new Error("Timed out waiting for the Phaser Global Mix worklet barrier."));
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
    for (let index = 0; index < left.length; index += 1) {
      const difference = left[index] - right[index];
      squared += difference * difference;
    }
    return Math.sqrt(squared / Math.max(1, left.length));
  };

  try {
    await processorReady;
    const workletNode = getWorkletNode();
    const audioContext = getAudioContext();
    if (!workletNode || !audioContext || !reference?.samples?.length) {
      throw new Error("Phaser Global Mix smoke requires the ready local guitar browser chain.");
    }
    const row = document.querySelector('[data-effect="18"]')?.closest(".effect-row");
    if (row?.dataset.category !== "modulation" ||
        row.querySelector(".effect-category-tag")?.textContent.trim() !== "Modulation" ||
        !effectNames[18]?.toLowerCase().includes("phaser") ||
        controls.globalMixEnabled.checked || controls.globalMixOffset.value !== "0" ||
        controls.globalMixOffset.min !== "-50" || controls.globalMixOffset.max !== "50" ||
        defaults.length !== 12 || defaults[10] !== 0 || defaults[11] !== 0) {
      throw new Error("Phaser category, picker ID, or default-off Global Mix controls are invalid.");
    }
    await audioContext.resume();
    stopSource(false);
    const testEnabled = previousEffects.map((_, index) => index === 0 || index === 18);
    document.querySelectorAll("[data-effect]").forEach((checkbox, index) => {
      checkbox.checked = testEnabled[index];
      workletNode.port.postMessage({ type: "enable", index, value: testEnabled[index] });
    });
    controls.rate.value = "0.5";
    controls.tapDivision.value = "free";
    controls.tempoBpm.value = "120";
    controls.minimum.value = "250";
    controls.maximum.value = "1600";
    controls.feedback.value = "0.2";
    controls.stages.value = "4";
    controls.mix.value = "0.5";
    controls.stereo.checked = true;

    const captureAt = async (enabled, offsetPoints) => {
      controls.globalMixEnabled.checked = enabled;
      controls.globalMixOffset.value = String(offsetPoints);
      updatePhaser();
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
          () => reject(new Error("Timed out starting Phaser Global Mix playback.")), 5000,
        )),
      ]);
      await waitForCapture(captureView, reference.samples.length);
      stopSource(false);
      const output = Float32Array.from(new Float32Array(captureBuffer));
      if (output.some((sample) => !Number.isFinite(sample))) {
        throw new Error("Phaser Global Mix playback produced non-finite samples.");
      }
      return output;
    };

    const baseline = await captureAt(false, 0);
    const storedDisabled = await captureAt(false, 20);
    const raised = await captureAt(true, 20);
    const lowered = await captureAt(true, -20);
    const disabledDifference = rmsDifference(baseline, storedDisabled);
    const raisedDifference = rmsDifference(baseline, raised);
    const loweredDifference = rmsDifference(baseline, lowered);
    if (disabledDifference > 1.0e-7 || raisedDifference < 1.0e-6 ||
        loweredDifference < 1.0e-6) {
      throw new Error(
        `Phaser Global Mix failed: disabled=${disabledDifference}, +20pp=${raisedDifference}, -20pp=${loweredDifference}.`,
      );
    }
    return {
      category: "Modulation",
      effectIndex: 18,
      defaultEnabled: false,
      defaultOffsetPoints: 0,
      disabledOffsetRms: disabledDifference,
      enabledPlus20ppRms: raisedDifference,
      enabledMinus20ppRms: loweredDifference,
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
      updatePhaser();
      document.querySelectorAll("[data-effect]").forEach((checkbox, index) => {
        checkbox.checked = previousEffects[index];
        workletNode.port.postMessage({ type: "enable", index, value: previousEffects[index] });
      });
      workletNode.port.postMessage({ type: "reset" });
      workletNode.port.postMessage({ type: "capture-reset" });
    }
  }
}
