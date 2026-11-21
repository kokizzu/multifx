PYTHON ?= python3

.PHONY: bootstrap-docs doctor repo-status test-docs test build configure bench audio-regression \
	audio-fixtures-plan audio-fixtures-fetch audio-fixtures-verify audio-fixtures-inspect \
	test-audio-fixtures windows-cross-build web-toolchain-bootstrap web-toolchain-verify \
	web-build web-browser-test web-test web-serve diff-check publish-plan publish-checkpoint \
	publish-picker-plan publish-picker publish-room-reverb-plan publish-room-reverb \
	publish-hall-reverb-plan publish-hall-reverb publish-plate-reverb-plan publish-plate-reverb \
	publish-spring-reverb-plan publish-spring-reverb \
	publish-auto-swell-plan publish-auto-swell \
	publish-rotary-speaker-plan publish-rotary-speaker \
	publish-ring-mod-plan publish-ring-mod \
	publish-rhythmic-slicer-plan publish-rhythmic-slicer \
	publish-step-filter-plan publish-step-filter \
	publish-notch-filter-plan publish-notch-filter \
	publish-reverse-delay-plan publish-reverse-delay \
	publish-ducking-delay-plan publish-ducking-delay \
	publish-modulated-delay-plan publish-modulated-delay \
	publish-filtered-delay-plan publish-filtered-delay \
	publish-sweep-delay-plan publish-sweep-delay

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

publish-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint "$(CHECKPOINT)"

publish-checkpoint:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint "$(CHECKPOINT)"

publish-picker-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint picker

publish-picker:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint picker

publish-room-reverb-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint room-reverb

publish-room-reverb:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint room-reverb

publish-hall-reverb-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint hall-reverb

publish-hall-reverb:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint hall-reverb

publish-plate-reverb-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint plate-reverb

publish-plate-reverb:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint plate-reverb

publish-spring-reverb-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint spring-reverb

publish-spring-reverb:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint spring-reverb

publish-auto-swell-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint auto-swell

publish-auto-swell:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint auto-swell

publish-rotary-speaker-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint rotary-speaker

publish-rotary-speaker:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint rotary-speaker

publish-ring-mod-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint ring-mod

publish-ring-mod:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint ring-mod

publish-rhythmic-slicer-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint rhythmic-slicer

publish-rhythmic-slicer:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint rhythmic-slicer

publish-step-filter-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint step-filter

publish-step-filter:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint step-filter

publish-notch-filter-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint notch-filter

publish-notch-filter:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint notch-filter

publish-reverse-delay-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint reverse-delay

publish-reverse-delay:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint reverse-delay

publish-ducking-delay-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint ducking-delay

publish-ducking-delay:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint ducking-delay

publish-modulated-delay-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint modulated-delay

publish-modulated-delay:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint modulated-delay

publish-filtered-delay-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint filtered-delay

publish-filtered-delay:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint filtered-delay

publish-sweep-delay-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan --checkpoint sweep-delay

publish-sweep-delay:
	$(PYTHON) scripts/publish_checkpoint.py publish --checkpoint sweep-delay
