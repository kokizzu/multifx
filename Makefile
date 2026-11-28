PYTHON ?= python3

.PHONY: bootstrap-docs doctor repo-status inspect-effect-draft test-docs test build configure bench audio-regression \
	test-pitch-ring-mod \
	audio-fixtures-plan audio-fixtures-fetch audio-fixtures-verify audio-fixtures-inspect \
	test-audio-fixtures windows-cross-build web-toolchain-bootstrap web-toolchain-verify \
web-build web-browser-test web-test web-serve diff-check bench-pitch-ring-mod

DATA_DIR ?= auto
FIXTURE_LINK ?= testdata/reference_audio
CHECKPOINT ?= all

bootstrap-docs:
	$(PYTHON) scripts/bootstrap_docs.py

doctor:
	$(PYTHON) scripts/doctor.py

repo-status:
	$(PYTHON) scripts/repo_status.py

test-docs:
	$(PYTHON) scripts/validate_docs.py

configure:
	$(PYTHON) scripts/run_cmake.py configure

build:
	$(PYTHON) scripts/run_cmake.py build

test:
	$(PYTHON) scripts/validate_docs.py
	$(PYTHON) scripts/test_audio_fixtures.py
	$(PYTHON) scripts/run_cmake.py test


bench:
	$(PYTHON) scripts/run_cmake.py bench

bench-pitch-ring-mod:
	$(PYTHON) scripts/run_cmake.py bench-pitch-ring-mod

test-pitch-ring-mod:
	$(PYTHON) scripts/run_cmake.py test --test-filter tabletfx_pitch_ring_mod_tests

audio-regression: build
	$(PYTHON) scripts/audio_regression.py --repo-root . --data-dir "$(DATA_DIR)" --link "$(FIXTURE_LINK)" --renderer build/native/tabletfx_offline

audio-fixtures-plan:
	$(PYTHON) scripts/audio_fixtures.py plan --repo-root . --data-dir "$(DATA_DIR)" --link "$(FIXTURE_LINK)"

audio-fixtures-fetch:
	$(PYTHON) scripts/audio_fixtures.py fetch --repo-root . --data-dir "$(DATA_DIR)" --link "$(FIXTURE_LINK)"

audio-fixtures-verify:
	$(PYTHON) scripts/audio_fixtures.py verify --repo-root . --data-dir "$(DATA_DIR)" --link "$(FIXTURE_LINK)"

audio-fixtures-inspect:
	$(PYTHON) scripts/audio_fixtures.py inspect --repo-root . --data-dir "$(DATA_DIR)" --link "$(FIXTURE_LINK)"

test-audio-fixtures:
	$(PYTHON) scripts/test_audio_fixtures.py

windows-cross-build:
	$(PYTHON) scripts/run_cmake.py windows-build

web-toolchain-bootstrap:
	$(PYTHON) scripts/web.py bootstrap-toolchain

web-toolchain-verify:
	$(PYTHON) scripts/web.py verify-toolchain

web-build:
	$(PYTHON) scripts/web.py build

web-test: test web-build
	$(PYTHON) scripts/web.py test

web-browser-test:
	$(PYTHON) scripts/web.py test

web-serve: web-build
	$(PYTHON) scripts/web.py serve

diff-check:
	$(PYTHON) scripts/check_repo_diff.py

include make/effects.mk
include make/research.mk
include make/publish.mk
