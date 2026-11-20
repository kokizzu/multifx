PYTHON ?= python3

.PHONY: bootstrap-docs doctor repo-status test-docs test build configure bench audio-regression \
	audio-fixtures-plan audio-fixtures-fetch audio-fixtures-verify audio-fixtures-inspect \
	test-audio-fixtures windows-cross-build diff-check publish-plan publish-checkpoint

DATA_DIR ?= auto
FIXTURE_LINK ?= testdata/reference_audio

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

diff-check:
	$(PYTHON) scripts/check_repo_diff.py

publish-plan:
	$(PYTHON) scripts/publish_checkpoint.py plan

publish-checkpoint:
	$(PYTHON) scripts/publish_checkpoint.py publish
