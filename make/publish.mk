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
  celestial-reverb cloud-reverb mercator-dist fractal-three-voice-synth fractal-tzf octobot am-ring-mod tape-echo octave-fuzz fractal-xvr asheville-pattrn rock-box-dist rock-box-boost stereo-panner headrush-bbd-mode headrush-cverb-ten-second-ir poly-sustain step-filter-delay fractal-volume-pan deluxe-looper
PUBLISH_CHECKPOINTS += fender-pitch-sequence-audit
PUBLISH_CHECKPOINTS += fender-virtual-capo-mix
PUBLISH_CHECKPOINTS += fender-studio-delay-pan
PUBLISH_CHECKPOINTS += fender-volume-pedal-mute
PUBLISH_CHECKPOINTS += fm9-formant-control-lfo
PUBLISH_CHECKPOINTS += headrush-prime-drop-tune
PUBLISH_CHECKPOINTS += fender-stereo-flanger-phase
PUBLISH_CHECKPOINTS += fender-flanger-tap-division
PUBLISH_CHECKPOINTS += fender-phaser-90-tap-division
PUBLISH_CHECKPOINTS += fender-tremolo-lfo-clip
PUBLISH_CHECKPOINTS += fender-tremolo-duty-cycle
PUBLISH_CHECKPOINTS += fractal-source-correction
PUBLISH_CHECKPOINTS += fractal-chorus-voice-count
PUBLISH_CHECKPOINTS += fractal-chorus-delay-range

.PHONY: inspect-stadium-delay-trails
inspect-stadium-delay-trails:
	$(PYTHON) scripts/inspect_delay_trails.py
PUBLISH_CHECKPOINTS += stadium-delay-trails

.PHONY: test-stadium-delay-trails web-test-stadium-delay-trails
test-stadium-delay-trails:
	$(PYTHON) scripts/run_cmake.py build
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_delay_tests
	$(PYTHON) scripts/test_offline_cli.py --executable build/native/tabletfx_offline

web-test-stadium-delay-trails: test-stadium-delay-trails web-build
	$(PYTHON) scripts/web.py test-stadium-delay-trails

.PHONY: test-fender-tremolo-duty-cycle web-test-fender-tremolo-duty-cycle
test-fender-tremolo-duty-cycle:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_tremolo_tests
	$(PYTHON) scripts/test_offline_cli.py --executable build/native/tabletfx_offline

web-test-fender-tremolo-duty-cycle: test-fender-tremolo-duty-cycle web-build
	$(PYTHON) scripts/web.py test-fender-tremolo-duty-cycle

.PHONY: bench-fender-tremolo-duty-cycle
bench-fender-tremolo-duty-cycle:
	$(PYTHON) scripts/run_cmake.py bench-fender-tremolo-duty-cycle



PUBLISH_CHECKPOINTS += fractal-tzf-auto

.PHONY: test-fender-stereo-flanger-phase web-test-fender-stereo-flanger-phase
test-fender-stereo-flanger-phase:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_flanger_tests

web-test-fender-stereo-flanger-phase: test web-build
	$(PYTHON) scripts/web.py test-fender-stereo-flanger-phase

.PHONY: test-fender-flanger-tap-division web-test-fender-flanger-tap-division
test-fender-flanger-tap-division:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_flanger_tests
	$(PYTHON) scripts/test_offline_cli.py --executable build/native/tabletfx_offline

web-test-fender-flanger-tap-division: test web-build
	$(PYTHON) scripts/web.py test-fender-flanger-tap-division

.PHONY: test-fender-phaser-90-tap-division web-test-fender-phaser-90-tap-division
test-fender-phaser-90-tap-division:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_phaser_tests
	$(PYTHON) scripts/test_offline_cli.py --executable build/native/tabletfx_offline

web-test-fender-phaser-90-tap-division: test web-build
	$(PYTHON) scripts/web.py test-fender-phaser-90-tap-division

.PHONY: test-fender-tremolo-lfo-clip web-test-fender-tremolo-lfo-clip
test-fender-tremolo-lfo-clip:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_tremolo_tests
	$(PYTHON) scripts/test_offline_cli.py --executable build/native/tabletfx_offline

web-test-fender-tremolo-lfo-clip: test web-build
	$(PYTHON) scripts/web.py test-fender-tremolo-lfo-clip

.PHONY: test-fractal-tzf-auto web-test-fractal-tzf-auto
test-fractal-tzf-auto:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_manual_thru_zero_flanger_tests
	$(PYTHON) scripts/test_offline_cli.py --executable build/native/tabletfx_offline

web-test-fractal-tzf-auto: test web-build
	$(PYTHON) scripts/web.py test-fractal-tzf-auto

.PHONY: publish-plan publish-checkpoint
PUBLISH_CHECKPOINTS += volume-pan
PUBLISH_CHECKPOINTS += stereo-filter
PUBLISH_CHECKPOINTS += step-tremolo
PUBLISH_CHECKPOINTS += fractal-ring-mod-tracking
PUBLISH_CHECKPOINTS += fractal-peq-types-solo
PUBLISH_CHECKPOINTS += fractal-filter-auto-wah
PUBLISH_CHECKPOINTS += fractal-filter-envelope
PUBLISH_CHECKPOINTS += fractal-filter-ladder
PUBLISH_CHECKPOINTS += chromatic-pitch-delay
PUBLISH_CHECKPOINTS += fractal-rotary-depth
PUBLISH_CHECKPOINTS += xr1-cabinet-ir
PUBLISH_CHECKPOINTS += xr1-global-eq
PUBLISH_CHECKPOINTS += fractal-octave-divider
PUBLISH_CHECKPOINTS += line6-double-take
PUBLISH_CHECKPOINTS += fender-eq5-graphic
PUBLISH_CHECKPOINTS += fractal-optical-tremolo
PUBLISH_CHECKPOINTS += fractal-bias-trem
PUBLISH_CHECKPOINTS += fender-vibratone fender-universe-vibe line6-four-osc-generator line6-poly-detune
PUBLISH_CHECKPOINTS += fractal-poly-pitch-range
PUBLISH_CHECKPOINTS += fractal-diatonic-custom-scale
PUBLISH_CHECKPOINTS += fender-pedal-shifter
PUBLISH_CHECKPOINTS += fender-pedal-detune
PUBLISH_CHECKPOINTS += fractal-touch-wah-mix
PUBLISH_CHECKPOINTS += flagship-control-routing
PUBLISH_CHECKPOINTS += stadium-mutant-filter
PUBLISH_CHECKPOINTS += fractal-chromatic-harmonizer
PUBLISH_CHECKPOINTS += fractal-diatonic-harmonizer
PUBLISH_CHECKPOINTS += feedback-generator-modes
PUBLISH_CHECKPOINTS += line6-stadium-3osc-research
PUBLISH_CHECKPOINTS += fractal-resonator-input-gain
PUBLISH_CHECKPOINTS += stadium-seven-band-peq
PUBLISH_CHECKPOINTS += fractal-chorus-tempo

.PHONY: test-fractal-chorus-tempo web-test-fractal-chorus-tempo bench-fractal-chorus-tempo
test-fractal-chorus-tempo:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_chorus_tests
	$(PYTHON) scripts/test_offline_cli.py --executable build/native/tabletfx_offline

web-test-fractal-chorus-tempo: test-fractal-chorus-tempo web-build
	$(PYTHON) scripts/web.py test-fractal-chorus-tempo

bench-fractal-chorus-tempo:
	$(PYTHON) scripts/run_cmake.py bench-fractal-chorus-tempo

.PHONY: publish-volume-pan-plan publish-volume-pan

.PHONY: inspect-effect-api
inspect-effect-api:
	python3 scripts/inspect_effect_api.py

.PHONY: inspect-web-effect-path
inspect-web-effect-path:
	python3 scripts/inspect_web_effect_path.py
.PHONY: test-convolution-reverb
.PHONY: test-fender-pedal-detune web-test-fender-pedal-detune
test-fender-pedal-detune:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_pedal_detune_tests

web-test-fender-pedal-detune: web-build
	$(PYTHON) scripts/web.py test-fender-pedal-detune
.PHONY: test-pitch-sequence test-poly-sustain
.PHONY: test-volume-pan test-stereo-filter web-test-volume-pan web-test-stereo-filter
.PHONY: test-volume-pedal web-test-volume-pedal
.PHONY: test-formant-filter web-test-formant-control-lfo

.PHONY: inspect-volume-pan-path
inspect-volume-pan-path:
	$(PYTHON) scripts/inspect_volume_pan_path.py



test-convolution-reverb:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_convolution_reverb_tests

test-pitch-sequence:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_pitch_sequence_tests

test-poly-sustain:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_poly_sustain_tests

test-volume-pan:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_volume_pan_tests

test-stereo-filter:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_stereo_filter_tests

web-test-volume-pan:
	$(PYTHON) scripts/web.py test-volume-pan

web-test-stereo-filter:
	$(PYTHON) scripts/web.py test-stereo-filter

.PHONY: test-fractal-resonator-input-gain web-test-fractal-resonator-input-gain
test-fractal-resonator-input-gain:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_four_band_resonator_tests
	$(PYTHON) scripts/test_offline_cli.py --executable build/native/tabletfx_offline

web-test-fractal-resonator-input-gain: web-build
	$(PYTHON) scripts/web.py test-fractal-resonator-input-gain

.PHONY: test-step-tremolo web-test-step-tremolo inspect-web-build-path inspect-web-smoke-path
.PHONY: test-optical-tremolo test-fractal-bias-trem web-test-fractal-bias-trem
.PHONY: test-vibe test-vibratone test-note-generator web-test-fender-vibratone web-test-fender-universe-vibe web-test-line6-four-osc-generator test-poly-detune web-test-line6-poly-detune web-test-fractal-poly-pitch-range web-test-fender-pedal-shifter web-test-fractal-diatonic-custom-scale test-fender-pedal-shifter
test-vibe:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_vibe_tests

test-step-tremolo:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_step_tremolo_tests

test-optical-tremolo:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_optical_tremolo_tests

test-fractal-bias-trem:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_fractal_bias_trem_tests

web-test-fractal-bias-trem: web-build
	$(PYTHON) scripts/web.py test-fractal-bias-trem

test-vibratone:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_vibratone_tests

test-note-generator:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_note_generator_tests

web-test-fender-vibratone: web-build
	$(PYTHON) scripts/web.py test-fender-vibratone

web-test-fender-universe-vibe: web-build
	$(PYTHON) scripts/web.py test-fender-universe-vibe

web-test-line6-four-osc-generator: web-build
	$(PYTHON) scripts/web.py test-line6-four-osc-generator

test-poly-detune:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_poly_detune_tests

web-test-line6-poly-detune: web-build
	$(PYTHON) scripts/web.py test-line6-poly-detune

web-test-fractal-poly-pitch-range: web-build
	$(PYTHON) scripts/web.py test-fractal-poly-pitch-range

test-fender-pedal-shifter:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_poly_pitch_shifter_tests

web-test-fender-pedal-shifter: web-build
	$(PYTHON) scripts/web.py test-fender-pedal-shifter

web-test-fractal-diatonic-custom-scale: web-build
	$(PYTHON) scripts/web.py test-fractal-diatonic-custom-scale

web-test-step-tremolo:
	$(PYTHON) scripts/web.py test-step-tremolo

inspect-web-build-path:
	$(PYTHON) scripts/web.py inspect-build-path

inspect-web-smoke-path:
	$(PYTHON) scripts/web.py inspect-smoke-path
PUBLISH_CHECKPOINTS += prime-reverb-tails
PUBLISH_CHECKPOINTS += fractal-synth-output-level
PUBLISH_CHECKPOINTS += fender-3d-chorus
PUBLISH_CHECKPOINTS += fractal-surround-delay
PUBLISH_CHECKPOINTS += tone-master-small-hall
.PHONY: $(foreach checkpoint,$(PUBLISH_CHECKPOINTS),publish-$(checkpoint)-plan stage-$(checkpoint) publish-$(checkpoint))

publish-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint "$(CHECKPOINT)"

publish-checkpoint:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint "$(CHECKPOINT)"

define publish-checkpoint-rules
publish-$(1)-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint "$(1)"

stage-$(1):
	$(PYTHON) scripts/publish_checkpoint.py stage --checkpoint "$(1)"

publish-$(1):
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint "$(1)"
endef

$(foreach checkpoint,$(PUBLISH_CHECKPOINTS),$(eval $(call publish-checkpoint-rules,$(checkpoint))))
.PHONY: publish-%-apply
publish-%-apply:
	python3 scripts/publish_checkpoint.py publish --checkpoint "$*"
.PHONY: unstage-%
unstage-%:
	$(PYTHON) scripts/publish_checkpoint.py unstage --checkpoint "$*"
inspect-effect-picker:
	python3 scripts/inspect_effect_picker.py





.PHONY: inspect-stereo-crossover-integration
.PHONY: inspect-publish-checkpoint
inspect-publish-checkpoint:
	python3 scripts/inspect_publish_checkpoint.py

.PHONY: test-mutant-filter web-test-mutant-filter
test-mutant-filter:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_mutant_filter_tests

web-test-mutant-filter: web-build
	$(PYTHON) scripts/web.py test-mutant-filter

.PHONY: test-fractal-chromatic-harmonizer web-test-fractal-chromatic-harmonizer
test-fractal-chromatic-harmonizer:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_fractal_chromatic_harmonizer_tests

web-test-fractal-chromatic-harmonizer: web-build
	$(PYTHON) scripts/web.py test-fractal-chromatic-harmonizer

.PHONY: test-fractal-diatonic-harmonizer web-test-fractal-diatonic-harmonizer bench-fractal-diatonic-harmonizer
test-fractal-diatonic-harmonizer:
	$(PYTHON) scripts/run_cmake.py test --verbose --test-filter "tabletfx_fractal_(diatonic|chromatic)_harmonizer_tests"

web-test-fractal-diatonic-harmonizer: web-build
	$(PYTHON) scripts/web.py test-fractal-diatonic-harmonizer

bench-fractal-diatonic-harmonizer:
	$(PYTHON) scripts/run_cmake.py bench-fractal-diatonic-harmonizer

.PHONY: sync-primary-main-plan sync-primary-main-apply
sync-primary-main-plan:
	$(PYTHON) scripts/sync_published_main.py plan

sync-primary-main-apply:
	$(PYTHON) scripts/sync_published_main.py apply

.PHONY: bench-feedback-generator
bench-feedback-generator:
	$(PYTHON) scripts/run_cmake.py bench-feedback-generator

.PHONY: bench-shuffling-looper
bench-shuffling-looper:
	$(PYTHON) scripts/run_cmake.py bench-shuffling-looper

.PHONY: web-test-stereo-panner
web-test-stereo-panner: test web-build
	$(PYTHON) scripts/web.py test-stereo-panner

test-volume-pedal:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_volume_pedal_tests

web-test-volume-pedal: test web-build
	$(PYTHON) scripts/web.py test-volume-pedal

test-formant-filter:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_formant_filter_tests

web-test-formant-control-lfo: test web-build
	$(PYTHON) scripts/web.py test-formant-control-lfo

.PHONY: web-test-headrush-drop-tune
web-test-headrush-drop-tune: test web-build
	$(PYTHON) scripts/web.py test-headrush-drop-tune

.PHONY: publish-fender-memory-delay-plan stage-fender-memory-delay publish-fender-memory-delay
publish-fender-memory-delay-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint fender-memory-delay

stage-fender-memory-delay:
	$(PYTHON) scripts/publish_checkpoint.py stage --checkpoint fender-memory-delay

publish-fender-memory-delay:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint fender-memory-delay
 .PHONY: inspect-stadium-delay-probe
inspect-stadium-delay-probe:
	python3 scripts/inspect_stadium_delay_probe.py
