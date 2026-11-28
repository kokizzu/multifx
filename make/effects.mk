.PHONY: test-euclidean-delay web-test-euclidean-delay bench-euclidean-delay

test-euclidean-delay:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_euclidean_delay_tests

web-test-euclidean-delay: test-euclidean-delay web-build
	$(PYTHON) scripts/web.py test-euclidean-delay

bench-euclidean-delay:
	$(PYTHON) scripts/run_cmake.py bench-euclidean-delay
