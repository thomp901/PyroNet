# PyroNet CSP Operations Console

This repository now contains a React + Vite frontend and a lightweight Node API layer for the PyroNet centralized software platform (CSP). The browser talks only to HTTP endpoints under `/api`; the server either reads the PostgreSQL schema in [schema.sql](/home/diego/PyroNet/csp/schema.sql) or falls back to an in-memory dataset for local development.

## Run it

1. Install dependencies:

```bash
npm install
```

2. Copy environment values if needed:

```bash
cp .env.example .env.local
```

3. Start the web app and API together:

```bash
npm run dev
```

4. Optional: run only the API server:

```bash
npm run start:api
```

5. Build for production:

```bash
npm run build
```

## Project structure

```text
server/             Express API with PostgreSQL queries and mock fallback
server/mocks/       Server-only in-memory dataset used for local fallback
src/api/            Typed frontend API clients and shared domain interfaces
src/app/            App entry and router setup
src/components/     Shared UI building blocks and layout
src/features/       Map rendering and feature-specific helpers
src/lib/            Runtime config, HTTP helper, formatters, async hooks
src/pages/          Routed operations pages
src/styles/         Global ops-console styling
schema.sql          PostgreSQL schema used by the API layer
```

## Available views

- Dashboard: fleet overview, mesh map, incident queue, and recent downlinks.
- Nodes: full node inventory plus per-node detail pages.
- Alerts: direct 0x03 critical alerts and derived offline incidents.
- History: 24-hour raw telemetry plus 7-day and 30-day aggregate views.
- Configuration: threshold revisions, NN table edits, and 0x04/0x05/0x06 triggers.
- Notifications: recipient preferences and email delivery tracking.

## Runtime configuration

- `VITE_API_BASE_URL=/api` keeps the frontend pointed at the local API proxy by default.
- `API_PORT=4000` controls the API server port.
- `DATABASE_URL=postgres://...` enables live PostgreSQL reads and writes.
- `NN_DEFAULT_RADIUS_METERS=250` and `NN_DEFAULT_MAX_NEIGHBORS=4` control automatic affected-radius NN generation.
- `NN_ON_REGISTRATION=false` disables automatic NN regeneration when nodes join or rejoin.

## Architecture notes

- The frontend never talks to PostgreSQL directly.
- The server exposes a stable resource layer for nodes, history, alerts, configuration, and notifications.
- The frontend API clients always use HTTP and surface real request failures to the page-level error states.
- The server-only mock backend and PostgreSQL-backed API return the same response shapes, so the UI can develop safely without coupling to raw SQL.
