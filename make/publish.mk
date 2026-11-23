PUBLISH_CHECKPOINTS := \
	all dly08 effect-modules picker room-reverb hall-reverb plate-reverb spring-reverb \
	auto-swell rotary-speaker ring-mod rhythmic-slicer step-filter notch-filter \
	reverse-delay ducking-delay modulated-delay filtered-delay sweep-delay \
	formant-filter fm9-formant-morph harmonic-resonator multiband-compressor bit-crusher \
	dimension-chorus triple-double-chorus four-band-resonator diatonic-harmonizer \
	grain-delay pitch-arpeggiator pitch-feedback-delay feedback-generator \
	shimmer-reverb convolution-reverb mono-guitar-synth flagship-catalog \
 five-band-eq phrase-looper note-generator enhancer resonant-delay harmonic-tremolo \
  auto-swell-delay shuffling-looper ten-tap-delay auto-pan megatap-delay de-esser \
  dual-engine-distortion vibe multi-delay porta-preamp pitch-sequence rhythm-tap-delay plex-delay flagship-delays \
	celestial-reverb cloud-reverb mercator-dist fractal-three-voice-synth fractal-tzf octobot am-ring-mod tape-echo octave-fuzz fractal-xvr asheville-pattrn rock-box-dist rock-box-boost stereo-panner headrush-bbd-mode headrush-cverb-ten-second-ir poly-sustain

.PHONY: publish-plan publish-checkpoint
.PHONY: test-convolution-reverb
.PHONY: test-pitch-sequence test-poly-sustain

test-convolution-reverb:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_convolution_reverb_tests

test-pitch-sequence:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_pitch_sequence_tests

test-poly-sustain:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_poly_sustain_tests
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
.PHONY: publish-%-apply
publish-%-apply:
	python3 scripts/publish_checkpoint.py publish --checkpoint "$*"
inspect-effect-picker:
	python3 scripts/inspect_effect_picker.py





.PHONY: inspect-stereo-crossover-integration
