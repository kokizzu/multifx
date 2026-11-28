.PHONY: record-euclidean-checkpoint

record-euclidean-checkpoint:
	python3 scripts/record_euclidean_checkpoint.py
.PHONY: research-verify
research-verify:
	$(PYTHON) scripts/validate_research_state.py
