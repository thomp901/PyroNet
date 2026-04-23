PYTHON ?= python3
VENV := .venv
VENV_PYTHON := $(VENV)/bin/python
VENV_PIP := $(VENV)/bin/pip
RUFF := $(VENV)/bin/ruff
PRE_COMMIT := $(VENV)/bin/pre-commit
SQLITE := $(PYTHON) -m sqlite3
CONFIG ?= ./config.example.toml
DB_PATH ?= ./pyronet-gateway.sqlite3
LIVE_DECODE_ARGS ?=

.PHONY: venv install-dev lint format run live-decode test test-live-backhaul check pre-commit-install pre-commit-run db-path db-tables db-shell db-downlinks

venv:
	$(PYTHON) -m venv $(VENV)

install-dev: venv
	$(VENV_PIP) install -r requirements-dev.txt

lint:
	$(RUFF) check .

format:
	$(RUFF) format .

run:
	$(PYTHON) -m pyronet_gateway --config $(CONFIG)

live-decode:
	$(PYTHON) -m pyronet_gateway.live_decode --config $(CONFIG) $(LIVE_DECODE_ARGS)

test:
	$(PYTHON) -m unittest -v

test-live-backhaul:
	$(PYTHON) -m unittest -v tests.test_backhaul_live

check: lint test

pre-commit-install:
	$(PRE_COMMIT) install

pre-commit-run:
	$(PRE_COMMIT) run --all-files

db-path:
	@printf '%s\n' "$(DB_PATH)"

db-tables:
	@test -f "$(DB_PATH)" || { echo "DB not found: $(DB_PATH)"; exit 1; }
	$(SQLITE) "$(DB_PATH)" ".tables"

db-shell:
	@test -f "$(DB_PATH)" || { echo "DB not found: $(DB_PATH)"; exit 1; }
	$(SQLITE) "$(DB_PATH)"

db-downlinks:
	@test -f "$(DB_PATH)" || { echo "DB not found: $(DB_PATH)"; exit 1; }
	$(SQLITE) "$(DB_PATH)" "SELECT id, request_type, target_node_id, target_ipv6, status, error_category, error_detail, created_at, completed_at FROM downlink_attempts ORDER BY id DESC LIMIT 20;"
