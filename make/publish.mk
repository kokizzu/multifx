PUBLISH_CHECKPOINTS := \
	all dly08 effect-modules picker room-reverb hall-reverb plate-reverb spring-reverb \
	auto-swell rotary-speaker ring-mod rhythmic-slicer step-filter notch-filter \
	reverse-delay ducking-delay modulated-delay filtered-delay sweep-delay \
	formant-filter harmonic-resonator multiband-compressor bit-crusher \
	dimension-chorus triple-double-chorus four-band-resonator diatonic-harmonizer \
	grain-delay pitch-arpeggiator pitch-feedback-delay feedback-generator \
	shimmer-reverb convolution-reverb mono-guitar-synth flagship-catalog \
 five-band-eq phrase-looper note-generator enhancer resonant-delay harmonic-tremolo \
  auto-swell-delay shuffling-looper ten-tap-delay auto-pan megatap-delay de-esser \
  dual-engine-distortion vibe multi-delay porta-preamp

.PHONY: publish-plan publish-checkpoint
.PHONY: $(foreach checkpoint,$(PUBLISH_CHECKPOINTS),publish-$(checkpoint)-plan publish-$(checkpoint))

publish-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint "$(CHECKPOINT)"

publish-checkpoint:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint "$(CHECKPOINT)"

define publish-checkpoint-rules
publish-$(1)-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint "$(1)"

publish-$(1):
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint "$(1)"
endef

$(foreach checkpoint,$(PUBLISH_CHECKPOINTS),$(eval $(call publish-checkpoint-rules,$(checkpoint))))
