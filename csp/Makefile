SHELL := /bin/bash

ENV_FILE ?= .env.local

ifneq ($(wildcard $(ENV_FILE)),)
include $(ENV_FILE)
export
else ifneq ($(wildcard .env.example),)
include .env.example
export
endif

NPM ?= npm
NPX ?= npx
PSQL ?= psql
PG_ISREADY ?= pg_isready

.PHONY: help install frontend web backend api backhaul all dev db-check db-init \
	backhaul-registration backhaul-risk backhaul-connectivity

help:
	@printf "Available targets:\n"
	@printf "  make install    Install Node dependencies\n"
	@printf "  make frontend   Start the Vite frontend only\n"
	@printf "  make api        Start the Express API only\n"
	@printf "  make backhaul   Start the API process that also serves backhaul routes\n"
	@printf "  make all        Start the frontend and API together\n"
	@printf "  make db-check   Check connectivity to DATABASE_URL\n"
	@printf "  make db-init    Apply schema.sql to DATABASE_URL\n"
	@printf "  make backhaul-registration  Send a sample registration packet to the API\n"
	@printf "  make backhaul-risk          Send a sample critical-risk packet to the API\n"
	@printf "  make backhaul-connectivity  Send a stale packet to simulate connectivity loss\n"

install:
	$(NPM) install

frontend:
	$(NPM) run dev:web

web: frontend

backend: api

api:
	$(NPM) run dev:api

backhaul: api

all:
	$(NPM) run dev

dev: all

db-check:
	@if [ -z "$(DATABASE_URL)" ]; then \
		echo "DATABASE_URL is not set. Define it in $(ENV_FILE) or your shell."; \
		exit 1; \
	fi
	$(PG_ISREADY) -d "$(DATABASE_URL)"

db-init:
	@if [ -z "$(DATABASE_URL)" ]; then \
		echo "DATABASE_URL is not set. Define it in $(ENV_FILE) or your shell."; \
		exit 1; \
	fi
	$(PSQL) "$(DATABASE_URL)" -f schema.sql

backhaul-registration:
	$(NPX) tsx server/sendTestRegistrationPacket.ts

backhaul-risk:
	$(NPX) tsx server/sendTestRiskPacket.ts

backhaul-connectivity:
	$(NPX) tsx server/sendTestConnectivityLossPacket.ts
