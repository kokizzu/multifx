.PHONY: test-euclidean-delay web-test-euclidean-delay bench-euclidean-delay

test-euclidean-delay:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_euclidean_delay_tests

web-test-euclidean-delay: test-euclidean-delay web-build
	$(PYTHON) scripts/web.py test-euclidean-delay

bench-euclidean-delay:
	$(PYTHON) scripts/run_cmake.py bench-euclidean-delay

.PHONY: test-synth-texture-reverb web-test-synth-texture-reverb bench-synth-texture-reverb

test-synth-texture-reverb:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_synth_texture_reverb_tests

web-test-synth-texture-reverb: test-synth-texture-reverb web-build
	$(PYTHON) scripts/web.py test-synth-texture-reverb

bench-synth-texture-reverb:
	$(PYTHON) scripts/run_cmake.py bench-synth-texture-reverb
